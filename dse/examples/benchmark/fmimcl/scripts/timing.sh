#!/usr/bin/env bash
# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0
#
# Measure the real time used by the FMI MCL (fmimcl) and compare it with a saved
# baseline (e.g. before and after a code change). The FMI MCL is not modified.
#
# Runs the benchmark (like profile.sh, but without a profiler) several times,
# with a timing shim (mcl_timing_shim.c) installed in place of the FMI MCL. The
# shim times each model_step() of the FMI MCL (marshal out, FMU step, marshal
# in) and subtracts the time spent inside the FMU and the cost of the clock
# reads: wall time of the FMI MCL per step (mean, median, p95). Waiting for
# SimBus and the target model is not included. Runs are summarised by their
# median; a change is only reported when it exceeds both THRESHOLD and the
# measured run-to-run noise.
#
# Usage (from dse.fmi repo root):
#   timing.sh                  Measure and compare with the baseline (if any)
#   timing.sh baseline         Measure and save the result as the baseline
#   timing.sh history          Print the results of all previous runs
#
# The exit code is 1 if mcl_wall_us_median is slower than the baseline.
#
# Environment overrides (see profile.sh for the benchmark settings):
#   STEPS, STEP_SIZE, INPUTS, OUTPUTS, FIXED_PARAMS, TUNABLE_PARAMS, USE_TARGET
#   RUNS             Measured runs (default: 5)
#   WARMUP           Discarded runs before measuring (default: 1)
#   THRESHOLD        Minimum change in percent to report (default: 5)
#   LABEL            Label stored with the result (default: git describe)
#   TIMING_DIR       Baselines and history (default: out/timing)
#   SANDBOX_DIR      Build output (default: <module>/dse/build/_out)
#   TIMING_SHIM_LIB  Timing shim (default: <sandbox>/examples/benchmark/fmimcl/lib/libmclbenchmarkshim.so)
#   EXTERNAL_BUILD_DIR, MODELC_EXE   As profile.sh

# Re-exec under bash when invoked via `sh` (bash-only syntax is used below).
if [ -z "${BASH_VERSION:-}" ]; then exec bash "$0" "$@"; fi

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODULE_DIR=$(cd "${SCRIPT_DIR}/../../../../.." && pwd)
ENTRY_DIR=$(pwd)
source "${SCRIPT_DIR}/../../profile-common.sh"

: "${STEPS:=10000}"
: "${STEP_SIZE:=0.0005}"
: "${INPUTS:=200}"
: "${OUTPUTS:=200}"
: "${FIXED_PARAMS:=10}"
: "${TUNABLE_PARAMS:=0}"
: "${USE_TARGET:=1}"
: "${RUNS:=5}"
: "${WARMUP:=1}"
: "${THRESHOLD:=5}"
: "${LABEL:=$(git -C "${MODULE_DIR}" describe --always --dirty 2>/dev/null || echo unknown)}"
: "${TIMING_DIR:=${ENTRY_DIR}/out/timing}"
: "${SANDBOX_DIR:=${MODULE_DIR}/dse/build/_out}"
: "${TIMING_SHIM_LIB:=${SANDBOX_DIR}/examples/benchmark/fmimcl/lib/libmclbenchmarkshim.so}"
: "${EXTERNAL_BUILD_DIR:=/tmp/dse.fmi}"
: "${MODELC_EXE:=${EXTERNAL_BUILD_DIR}/dse.modelc.lib/bin/modelc}"
export TIMING_SHIM_LIB SANDBOX_DIR

OUT_DIR="${ENTRY_DIR}/out/benchmark"
END_TIME=$(awk -v s="${STEPS}" -v t="${STEP_SIZE}" \
    'BEGIN { printf "%.9g", s * t }')

# Results are only comparable for identical settings.
KEY="mcl2-i${INPUTS}-o${OUTPUTS}-f${FIXED_PARAMS}-t${TUNABLE_PARAMS}-s${STEPS}-d${STEP_SIZE}-m${USE_TARGET}"
BASELINE_FILE="${TIMING_DIR}/baseline.${KEY}.txt"
LAST_FILE="${TIMING_DIR}/last.${KEY}.txt"
HISTORY_FILE="${TIMING_DIR}/history.tsv"
HOST_INFO="host=$(hostname) cpu=$(sed -n 's/^model name[^:]*: *//p' /proc/cpuinfo 2>/dev/null | head -n 1 | tr ' ' '_')"

cleanup() {
    simer_stop
    rm -rf "${OUT_DIR}"
    rmdir "${ENTRY_DIR}/out" 2>/dev/null || true
}

# One benchmark run; the timing shim appends its result to the given file.
run_once() {
    local timing_file=$1
    simer_start "${OUT_DIR}/simer" \
        -stack benchmark_stack -stepsize "${STEP_SIZE}" -endtime "${END_TIME}"
    simer_wait
    (
        cd "${OUT_DIR}/fmimcl"
        export SIMBUS_LOGLEVEL=6 MCL_TIMING_FILE="${timing_file}"
        "${MODELC_EXE}" --name fmimcl_inst \
            --stepsize "${STEP_SIZE}" --endtime "${END_TIME}" \
            stack.yaml model.yaml signalgroup.yaml
    )
    simer_stop
    wait 2>/dev/null || true
}

# Print "median min max" of a metric over the runs in a result file.
stats() {
    grep -v '^#' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | sort -g |
        awk '{ v[NR] = $1 } END {
            m = (NR % 2) ? v[(NR + 1) / 2] : (v[NR / 2] + v[NR / 2 + 1]) / 2
            printf "%.3f %.3f %.3f\n", m, v[1], v[NR]
        }'
}

measure() {
    [[ -x "${MODELC_EXE}" ]] || { echo "ModelC executable not found: ${MODELC_EXE}" >&2; exit 1; }
    mkdir -p "${TIMING_DIR}"
    trap cleanup EXIT
    OUT_DIR="${OUT_DIR}" STEP_SIZE="${STEP_SIZE}" END_TIME="${END_TIME}" \
        "${SCRIPT_DIR}/benchmark.sh" \
        "${INPUTS}" "${OUTPUTS}" "${FIXED_PARAMS}" "${TUNABLE_PARAMS}" \
        "${USE_TARGET}"

    echo "# label=${LABEL} ${HOST_INFO} date=$(date -Iseconds)" > "${LAST_FILE}"
    local i
    for (( i = 1; i <= WARMUP + RUNS; i++ )); do
        if (( i <= WARMUP )); then
            echo "Warm-up run ${i}/${WARMUP}"
            run_once /dev/null
        else
            echo "Run $((i - WARMUP))/${RUNS}"
            run_once "${LAST_FILE}"
            if [[ $(grep -vc '^#' "${LAST_FILE}") -ne $((i - WARMUP)) ]]; then
                echo "No timing reported by the timing shim" >&2
                exit 1
            fi
        fi
    done
    cleanup
    trap - EXIT
}

# compare <baseline_file> <current_file>; returns 1 if a gated metric is slower.
compare() {
    local rc=0 metric bm bmin bmax cm cmin cmax line
    echo "baseline: $(head -n 1 "$1")"
    echo "current:  $(head -n 1 "$2")"
    if [[ "$(head -n 1 "$1" | sed 's/ date=.*//; s/label=[^ ]* //')" != \
          "$(head -n 1 "$2" | sed 's/ date=.*//; s/label=[^ ]* //')" ]]; then
        echo "WARNING: baseline was measured on a different machine"
    fi
    printf '\n%-22s %12s %12s %9s %7s  %s\n' \
        metric baseline current change noise verdict
    for metric in mcl_wall_us_median mcl_wall_us_p95 mcl_wall_us_per_step \
        fmu_wall_us_median; do
        read -r bm bmin bmax <<< "$(stats "$1" "${metric}")"
        read -r cm cmin cmax <<< "$(stats "$2" "${metric}")"
        line=$(awk -v m="${metric}" -v bm="${bm}" -v bn="${bmin}" -v bx="${bmax}" \
            -v cm="${cm}" -v cn="${cmin}" -v cx="${cmax}" -v thr="${THRESHOLD}" '
            BEGIN {
                if (bm <= 0 || cm <= 0) {
                    printf "%-22s %12.3f %12.3f\n", m, bm, cm
                    exit
                }
                d = (cm - bm) / bm * 100
                n1 = (bx - bn) / bm * 100; n2 = (cx - cn) / cm * 100
                n = (n1 > n2) ? n1 : n2
                lim = (n > thr) ? n : thr
                v = (d > lim) ? "SLOWER" : (d < -lim) ? "FASTER" : "ok"
                printf "%-22s %12.3f %12.3f %+8.1f%% %6.1f%%  %s\n", m, bm, cm, d, n, v
            }')
        echo "${line}"
        if [[ "${metric}" == mcl_wall_us_median && "${line}" == *SLOWER ]]; then
            rc=1
        fi
    done
    echo
    echo "Values are microseconds, medians over the runs. A change is only reported"
    echo "when it exceeds ${THRESHOLD}% and the run-to-run noise (max-min of the runs)."
    return "${rc}"
}

record_history() {
    local m
    m=$(for metric in mcl_wall_us_median mcl_wall_us_p95 mcl_wall_us_per_step; do
        printf '%s\t' "$(stats "${LAST_FILE}" "${metric}" | cut -d' ' -f1)"
    done)
    printf '%s\t%s\t%s\t%s\t%s%s\n' "$(date -Iseconds)" "${LABEL}" "${KEY}" \
        "${HOST_INFO%% *}" "${m}" "${RUNS}" >> "${HISTORY_FILE}"
}

history() {
    [[ -f "${HISTORY_FILE}" ]] || { echo "No history in ${TIMING_DIR}" >&2; return 1; }
    { printf 'date\tlabel\tsettings\thost\tmcl_wall_us_median\tmcl_wall_us_p95\tmcl_wall_us/step\truns\n'
      cat "${HISTORY_FILE}"; } | column -t -s "$(printf '\t')"
}

case "${1:-run}" in
    help|-h|--help) profile_help "${BASH_SOURCE[0]}" ;;
    history) history ;;
    baseline)
        measure
        cp "${LAST_FILE}" "${BASELINE_FILE}"
        record_history
        echo "Saved baseline: ${BASELINE_FILE}"
        printf 'mcl_wall_us_median: %s (median, min, max)\n' \
            "$(stats "${BASELINE_FILE}" mcl_wall_us_median)"
        ;;
    run)
        measure
        record_history
        if [[ -f "${BASELINE_FILE}" ]]; then
            compare "${BASELINE_FILE}" "${LAST_FILE}"
        else
            echo "No baseline for these settings (run '$0 baseline' first)."
            printf 'mcl_wall_us_median: %s (median, min, max)\n' \
                "$(stats "${LAST_FILE}" mcl_wall_us_median)"
        fi
        ;;
    *) echo "Unknown command: $1 (see --help)" >&2; exit 1 ;;
esac
