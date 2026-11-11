#!/usr/bin/env bash
# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0
#
# Profile the FMI importer (fmuImporter) running a generated gateway FMU.
#
# Generates the benchmark (benchmark.sh), starts Simer (Redis, SimBus and the
# optional target model) in Docker and runs the importer under the selected
# profiler. Simer itself is not profiled.
#
# Usage (from dse.fmi repo root):
#   profile.sh                 Run the benchmark and profile (default)
#   profile.sh view [mode]     View the newest profile (inclusive|tree|code)
#
# Environment overrides:
#   PROFILE              callgrind (default), cachegrind or gdb
#   STEPS                Simulation steps (default: 10000)
#   STEP_SIZE            Step size (default: 0.0005)
#   SCALAR_CHANNELS      Scalar channels, see benchmark.sh (default: 2)
#   SIGNALS_PER_CHANNEL  Signals per scalar channel (default: 10)
#   BINARY_SIGNALS       Binary signals (default: 0)
#   USE_TARGET           1 = Simer runs the target model (default: 1)
#   VIEW_MODE            View after a run: inclusive (default), tree or code
#   IMPORTER_EXE         Importer, relative to the repo root
#                        (default: dse/build/_out/importer/fmuImporter)
#   BENCH_FMU_DIR        Generated FMU (default: out/benchmark/fmu)
#   CSV_FILE             Importer input CSV (default: out/benchmark/csv/input.csv)
#   PROFILE_DIR          Profile output directory (default: out/profiles)

# Re-exec under bash when invoked via `sh` (bash-only syntax is used below).
if [ -z "${BASH_VERSION:-}" ]; then exec bash "$0" "$@"; fi

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENTRY_DIR=$(pwd)
source "${SCRIPT_DIR}/../../profile-common.sh"

: "${STEPS:=10000}"
: "${STEP_SIZE:=0.0005}"
: "${SCALAR_CHANNELS:=2}"
: "${SIGNALS_PER_CHANNEL:=10}"
: "${BINARY_SIGNALS:=0}"
: "${USE_TARGET:=1}"
: "${VIEW_MODE:=inclusive}"
: "${IMPORTER_EXE:=dse/build/_out/importer/fmuImporter}"

OUT_DIR="${ENTRY_DIR}/out/benchmark"
: "${BENCH_FMU_DIR:=${OUT_DIR}/fmu}"
: "${CSV_FILE:=${OUT_DIR}/csv/input.csv}"

# Generate the layout: ${OUT_DIR}/simer (Simer), fmu and the importer CSV.
generate_benchmark() {
    OUT_DIR="${OUT_DIR}" \
        "${SCRIPT_DIR}/benchmark.sh" \
        "${SCALAR_CHANNELS}" "${SIGNALS_PER_CHANNEL}" "${BINARY_SIGNALS}" \
        "${USE_TARGET}" "${STEPS}"
}

run_importer() {
    local cmd=("${ENTRY_DIR}/${IMPORTER_EXE}" -X "${STEPS}" -s "${STEP_SIZE}")
    if [[ -f "${CSV_FILE}" ]]; then
        cmd+=(--csv="${CSV_FILE}")
    fi
    profile_run "${cmd[@]}" "${BENCH_FMU_DIR}"
}

cleanup() {
    simer_stop
    rm -rf "${OUT_DIR}"
    rmdir "${ENTRY_DIR}/out" 2>/dev/null || true
}

view() {
    # Binaries were built in containers; map their source paths to checkouts.
    profile_view "$1" "s|/tmp/repo/dse/modelc|${ENTRY_DIR}/dse/modelc|g" \
        "${ENTRY_DIR}"
}

run() {
    trap cleanup EXIT
    generate_benchmark
    simer_start "${OUT_DIR}/simer" -stepsize "${STEP_SIZE}" -endtime 9999
    simer_wait
    run_importer
    cleanup
    view "${VIEW_MODE}"
}

profile_check
case "${1:-run}" in
    help|-h|--help) profile_help "${BASH_SOURCE[0]}" ;;
    view) view "${2:-${VIEW_MODE}}" ;;
    run) run ;;
    *) echo "Unknown command: $1 (see --help)" >&2; exit 1 ;;
esac
