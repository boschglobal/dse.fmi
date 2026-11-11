<!--
Copyright 2026 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# FMI Benchmark Example

This benchmark profiles the FMI importer while it runs a generated gateway FMU.

## Operation

Run from the `dse.fmi` repository root. Build the importer and benchmark
libraries first:

```bash
$ make build tools

# Run the benchmark and collect a Callgrind profile (the default).
$ sh dse/examples/benchmark/fmigateway/scripts/profile.sh

# Re-open the newest profile in a selected view without rerunning the benchmark.
$ sh dse/examples/benchmark/fmigateway/scripts/profile.sh view inclusive
$ sh dse/examples/benchmark/fmigateway/scripts/profile.sh view tree
$ sh dse/examples/benchmark/fmigateway/scripts/profile.sh view code
```

The run uses 2 scalar channels, 10 signals per channel, 10,000 steps, and a
step size of 0.0005 by default. It generates CSV input at
`out/benchmark/csv/input.csv`. Override these settings with `SCALAR_CHANNELS`,
`SIGNALS_PER_CHANNEL`, `BINARY_SIGNALS`, `STEPS`, and `STEP_SIZE`; for example:

```bash
$ SCALAR_CHANNELS=5 SIGNALS_PER_CHANNEL=200 STEPS=5000 \
        sh dse/examples/benchmark/fmigateway/scripts/profile.sh
```

Callgrind output is written to `out/profiles/callgrind.out.<pid>` (override
with `PROFILE_DIR`). The `view` command reads the newest such file. `PROFILE`
selects `callgrind` (default), `cachegrind`, or `gdb`; `profile.sh --help` lists
all settings. Valgrind and Docker must be available to run the benchmark.

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
