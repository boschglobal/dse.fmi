<!--
Copyright 2026 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# FMI MCL Benchmark Example

This benchmark profiles the FMI MCL (`fmimcl`) while it operates a minimal FMI 2
Co-Simulation FMU. The FMI MCL runs in a standalone ModelC process (under the
profiler), SimBus and a target model run in a Simer container.

## Operation

Run from the `dse.fmi` repository root. Build the FMI MCL, the benchmark FMU
and the target model first:

```bash
$ make build

# Run the benchmark and collect a Callgrind profile (the default).
$ sh dse/examples/benchmark/fmimcl/scripts/profile.sh

# Re-open the newest profile in a selected view without rerunning the benchmark.
$ sh dse/examples/benchmark/fmimcl/scripts/profile.sh view inclusive
$ sh dse/examples/benchmark/fmimcl/scripts/profile.sh view tree
$ sh dse/examples/benchmark/fmimcl/scripts/profile.sh view code
```

The run uses 200 inputs, 200 outputs, 10 fixed parameters, 0 tunable parameters,
10,000 steps, and a step size of 0.0005 by default. Override these settings with
`INPUTS`, `OUTPUTS`, `FIXED_PARAMS`, `TUNABLE_PARAMS`, `STEPS`, and `STEP_SIZE`;
for example:

```bash
$ INPUTS=200 OUTPUTS=200 FIXED_PARAMS=15000 STEPS=10000 \
        sh dse/examples/benchmark/fmimcl/scripts/profile.sh
```

`USE_TARGET=0` disables the target model. `PROFILE` selects `callgrind`
(default), `cachegrind`, or `gdb`. Profile output is written as
`callgrind.out.<pid>` or `cachegrind.out.<pid>` in `out/profiles` (override with
`PROFILE_DIR`); the `view` command reads the newest profile. `profile.sh --help`
lists all settings. Valgrind and Docker must be available
to run the benchmark.

The benchmark layout can also be generated without running it:

```bash
sh dse/examples/benchmark/fmimcl/scripts/benchmark.sh <inputs> <outputs> <fixed_params> <tunable_params> [with_target=1]
```

## Measuring Real Time

Callgrind counts instructions, not time. To measure the real time used by the
FMI MCL (e.g. before and after a code change), use `timing.sh`. It runs the
benchmark several times without a profiler and compares the result with a
saved baseline:

```bash
# On the unchanged code: save a baseline.
$ sh dse/examples/benchmark/fmimcl/scripts/timing.sh baseline

# After the change (rebuild with `make build` first): measure and compare.
$ sh dse/examples/benchmark/fmimcl/scripts/timing.sh

# List the results of all previous runs.
$ sh dse/examples/benchmark/fmimcl/scripts/timing.sh history
```

The FMI MCL is not modified. `timing.sh` installs a small timing shim
(`mcl_timing_shim.c`, built as `libmclbenchmarkshim.so`) in place of the FMI MCL
library, which forwards to the real FMI MCL and times each `model_step()`
(marshal out, FMU step, marshal in). Only the FMI MCL is measured: the
benchmark FMU accumulates the time spent in its `fmi2SetReal()`,
`fmi2GetReal()` and `fmi2DoStep()` (exported as `benchmark_fmu_time`), and the
shim subtracts it, together with the cost of the clock reads (calibrated at
start, reported as `clock_ns`), from every step. Only the vDSO clock
`CLOCK_MONOTONIC` is read, so there are no syscalls in the measured region. It
reports the FMI MCL wall time per step (mean, median, p95) and, for reference,
the FMU time (`fmu_wall_us_median`). Waiting for SimBus and the target model is
not included, so the result does not depend on Redis or the target.
`profile.sh` does not use the shim. Results are the median over `RUNS`
(default 5) after `WARMUP` (default 1) discarded runs. A change is only
reported as `SLOWER` or `FASTER` if it exceeds both `THRESHOLD` (default 5%)
and the run-to-run noise (max-min of the runs). The exit code is 1 if
`mcl_wall_us_median` is slower than the baseline.

Baselines are stored in `out/timing` (override with `TIMING_DIR`) per benchmark
settings (`INPUTS`, `OUTPUTS`, `STEPS`, ...) and are only meaningful on the
machine on which they were measured; a warning is printed if the machine
differs. Keep the machine idle while measuring. `timing.sh --help` lists all
settings.

## Benchmark Model

All FMU variables are Real, with value references assigned in the order inputs,
outputs, fixed parameters, tunable parameters. The FMI MCL reads the variable
configuration from the generated Signal Group (no `modelDescription.xml`).

| Variable       | FMU behaviour                             | Target model                     |
| -------------- | ----------------------------------------- | -------------------------------- |
| Input          | Stored by `fmi2SetReal()`.                | Writes a changing value (step).  |
| Output         | Reads back as `steps + vr` (changes).     | Reads the value every step.      |
| Fixed param    | Set once from its start value (internal). | Not exchanged over SimBus.       |
| Tunable param  | Stored by `fmi2SetReal()`.                | Writes a changing value (step).  |

The FMI MCL skips setting a fixed parameter when its value equals the start
value, so fixed parameters add no `fmi2SetReal()` calls unless overridden (e.g.
with `PARAMETER_FILE`).

## Profiling Scope

Callgrind profiles the standalone ModelC process running the FMI MCL and the FMU
library. The target model runs separately inside Simer and is not profiled. Use
the `tree` view to inspect call paths; inclusive costs overlap between callers
and callees, so do not add them together.

## Interpreting the Profile

- `inclusive` attributes a function's cost to that function and all functions
  it calls. Use it to find expensive functions and call subtrees. Costs overlap
  between callers and callees, so do not add inclusive values together.
- `tree` shows the caller hierarchy and costs along call paths. Use it to trace
  where an expensive function is reached and which callers contribute to it.
- `code` annotates source lines with their measured costs. Use it to locate
  expensive lines inside a function.

Callgrind costs are event counts (normally instruction counts), not elapsed
time. Compare profiles only when benchmark settings and environment are alike.
