#!/usr/bin/env bash
# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0
#
# Profile the ModelC process running the FMI MCL (fmimcl) and a benchmark FMU.
#
# Generates the benchmark (benchmark.sh), starts Simer (Redis, SimBus and the
# optional target model) in Docker and runs the ModelC executable for the FMI
# MCL under the selected profiler. Simer itself is not profiled.
#
# Usage (from dse.fmi repo root):
#   profile.sh                 Run the benchmark and profile (default)
#   profile.sh view [mode]     View the newest profile (inclusive|tree|code)
#
# Environment overrides:
#   PROFILE          callgrind (default), cachegrind or gdb
#   STEPS            Simulation steps (default: 10000)
#   STEP_SIZE        Step size (default: 0.0005)
#   INPUTS           FMU inputs, see benchmark.sh (default: 200)
#   OUTPUTS          FMU outputs, see benchmark.sh (default: 200)
#   FIXED_PARAMS     FMU fixed parameters (default: 10)
#   TUNABLE_PARAMS   FMU tunable parameters (default: 0)
#   USE_TARGET       1 = Simer runs the target model (default: 1)
#   VIEW_MODE        View after a run: inclusive (default), tree or code
#   EXTERNAL_BUILD_DIR  FMI external build dir (default: /tmp/dse.fmi)
#   MODELC_EXE       ModelC executable (default: <external>/dse.modelc.lib/bin/modelc)
#   MODELC_SRC_DIR   ModelC sources for annotation (default: <external>/dse.modelc)
#   CLIB_SRC_DIR     C library sources for annotation (default: ../dse.clib)
#   PROFILE_DIR      Profile output directory (default: out/profiles)

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
: "${VIEW_MODE:=inclusive}"
: "${EXTERNAL_BUILD_DIR:=/tmp/dse.fmi}"
: "${MODELC_EXE:=${EXTERNAL_BUILD_DIR}/dse.modelc.lib/bin/modelc}"
: "${MODELC_SRC_DIR:=${EXTERNAL_BUILD_DIR}/dse.modelc}"
: "${CLIB_SRC_DIR:=$(cd "${MODULE_DIR}/../dse.clib" && pwd)}"

OUT_DIR="${ENTRY_DIR}/out/benchmark"
END_TIME=$(awk -v s="${STEPS}" -v t="${STEP_SIZE}" \
    'BEGIN { printf "%.9g", s * t }')

# Generate the layout: ${OUT_DIR}/simer (Simer) and ${OUT_DIR}/fmimcl (MCL).
generate_benchmark() {
    OUT_DIR="${OUT_DIR}" STEP_SIZE="${STEP_SIZE}" END_TIME="${END_TIME}" \
        "${SCRIPT_DIR}/benchmark.sh" \
        "${INPUTS}" "${OUTPUTS}" "${FIXED_PARAMS}" "${TUNABLE_PARAMS}" \
        "${USE_TARGET}"
}

# ModelC resolves the MCL files relative to the current directory.
run_modelc() {
    if [[ ! -x "${MODELC_EXE}" ]]; then
        echo "ModelC executable not found: ${MODELC_EXE}" >&2
        return 1
    fi
    (
        cd "${OUT_DIR}/fmimcl"
        export SIMBUS_LOGLEVEL=6
        profile_run "${MODELC_EXE}" --name fmimcl_inst \
            --stepsize "${STEP_SIZE}" --endtime "${END_TIME}" \
            stack.yaml model.yaml signalgroup.yaml
    )
}

cleanup() {
    simer_stop
    rm -rf "${OUT_DIR}"
    rmdir "${ENTRY_DIR}/out" 2>/dev/null || true
}

view() {
    # Binaries were built in containers; map their source paths to checkouts.
    profile_view "$1" \
        "s|/tmp/repo|${MODULE_DIR}|g; s|/__w/dse.modelc/dse.modelc|${MODELC_SRC_DIR}|g; s|${MODELC_SRC_DIR}/dse/modelc/build/_deps/dse_clib-src|${CLIB_SRC_DIR}|g" \
        "${MODULE_DIR}" "${MODELC_SRC_DIR}" "${CLIB_SRC_DIR}"
}

run() {
    trap cleanup EXIT
    generate_benchmark
    simer_start "${OUT_DIR}/simer" \
        -stack benchmark_stack -stepsize "${STEP_SIZE}" -endtime "${END_TIME}"
    simer_wait
    run_modelc
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
