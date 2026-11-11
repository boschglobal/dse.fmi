#!/usr/bin/env bash
# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0
#
# Generate a benchmark for the FMI MCL (fmimcl) with a minimal FMU
# (dse/examples/benchmark/fmimcl/fmu.c). Two directories are generated:
#
#   simer/    Simer simulation (Redis, SimBus and the optional target model
#             dse/examples/benchmark/fmimcl/model.c) described by
#             data/simulation.yaml.
#   fmimcl/   Standalone FMI MCL with its own stack.yaml (no SimBus), started
#             separately with the ModelC executable.
#
# Setup (build the FMI MCL, the FMU and the target model first):
#   $ make build
#
# Usage (from dse.fmi repo root):
#   benchmark.sh <inputs> <outputs> <fixed_params> <tunable_params> [with_target=1]
#
#   inputs          FMU input variables, set at every step (target -> FMU).
#   outputs         FMU output variables, change at every step (FMU -> target).
#   fixed_params    FMU parameters (variability fixed), set once at start.
#   tunable_params  FMU parameters (variability tunable), changed by the target
#                   at every step.
#   with_target     1 = add the target model to Simer, 0 = no target.
#
# All variables are Real. Fixed parameters are internal to the FMI MCL (not
# exchanged over SimBus), tunable parameters are exchanged over SimBus.
#
# Environment overrides:
#   SANDBOX_DIR   Build output (default: <module>/dse/build/_out)
#   OUT_DIR       Output directory (default: ./out/benchmark)
#   MCL_LIB       FMI MCL library (default: <sandbox>/fmimcl/lib/libfmimcl.so)
#   FMU_LIB       Benchmark FMU (default: <sandbox>/examples/benchmark/fmimcl/lib/libmclbenchmarkfmu.so)
#   TARGET_LIB    Target model (default: <sandbox>/examples/benchmark/fmimcl/lib/libmclbenchmarktarget.so)
#   TIMING_SHIM_LIB  If set, install this timing shim in place of the FMI MCL
#                 (see timing.sh); the FMI MCL is installed as real_<name>.
#   STEP_SIZE     Step size used in the printed commands (default: 0.0005)
#   END_TIME      End time used in the printed commands (default: 0.002)
#   LOG_LEVEL     SIMBUS_LOGLEVEL used in the Simer stack and printed commands (default: 6)
#   REDIS_URI     Redis transport URI (default: redis://localhost:6379)

# Re-exec under bash when invoked via `sh` (bash-only syntax is used below).
if [ -z "${BASH_VERSION:-}" ]; then exec bash "$0" "$@"; fi

set -euo pipefail

usage() {
    echo "Usage: $0 <inputs> <outputs> <fixed_params> <tunable_params> [with_target=1]" >&2
    exit 1
}

is_uint() { [[ "$1" =~ ^[0-9]+$ ]]; }

[[ $# -ge 4 && $# -le 5 ]] || usage
INPUTS=$1
OUTPUTS=$2
FIXED_PARAMS=$3
TUNABLE_PARAMS=$4
WITH_TARGET=${5:-1}
is_uint "${INPUTS}" && is_uint "${OUTPUTS}" && is_uint "${FIXED_PARAMS}" &&
    is_uint "${TUNABLE_PARAMS}" || usage
[[ "${WITH_TARGET}" == 0 || "${WITH_TARGET}" == 1 ]] || usage
if (( INPUTS + OUTPUTS + FIXED_PARAMS + TUNABLE_PARAMS == 0 )); then
    echo "At least one variable is required" >&2
    exit 1
fi
if (( WITH_TARGET && INPUTS + OUTPUTS + TUNABLE_PARAMS == 0 )); then
    echo "The target model requires inputs, outputs or tunable_params" >&2
    exit 1
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODULE_DIR=$(cd "${SCRIPT_DIR}/../../../../.." && pwd)
: "${SANDBOX_DIR:=${MODULE_DIR}/dse/build/_out}"
: "${OUT_DIR:=$(pwd)/out/benchmark}"
: "${MCL_LIB:=${SANDBOX_DIR}/fmimcl/lib/libfmimcl.so}"
: "${FMU_LIB:=${SANDBOX_DIR}/examples/benchmark/fmimcl/lib/libmclbenchmarkfmu.so}"
: "${TARGET_LIB:=${SANDBOX_DIR}/examples/benchmark/fmimcl/lib/libmclbenchmarktarget.so}"
: "${STEP_SIZE:=0.0005}"
: "${END_TIME:=0.002}"
: "${LOG_LEVEL:=6}"
: "${REDIS_URI:=redis://localhost:6379}"

STACK_NAME=benchmark_stack
FMU_MODEL=BenchmarkFmu
MCL_NAME=fmimcl_inst
MCL_UID=42
MCL_DIR="${OUT_DIR}/fmimcl"
FMU_DIR="${MCL_DIR}/fmu"
FMU_BINARY="binaries/linux64/$(basename "${FMU_LIB}")"
SIMER_DIR="${OUT_DIR}/simer"
SIMER_DATA_DIR="${SIMER_DIR}/data"
TARGET_NAME=benchmark_target
TARGET_UID=22
TARGET_DIR="${SIMER_DIR}/model/${TARGET_NAME}"

required_files=("${MCL_LIB}" "${FMU_LIB}")
if (( WITH_TARGET )); then required_files+=("${TARGET_LIB}"); fi
if [[ -n "${TIMING_SHIM_LIB:-}" ]]; then required_files+=("${TIMING_SHIM_LIB}"); fi
for f in "${required_files[@]}"; do
    [[ -f "${f}" ]] || { echo "File not found: ${f} (run 'make build')" >&2; exit 1; }
done

rm -rf "${SIMER_DIR}" "${MCL_DIR}"
mkdir -p "${SIMER_DATA_DIR}" "${MCL_DIR}/lib" "${FMU_DIR}/resources" \
    "${FMU_DIR}/binaries/linux64"


# 1. Generate Signal Groups.
# =========================
# FMU variables get consecutive value references in the order: inputs, outputs,
# fixed parameters, tunable parameters.
FMU_SG_FILE="${MCL_DIR}/signalgroup.yaml"
TARGET_SG_FILE="${OUT_DIR}/signalgroup_target.yaml"

# Emit FMU signals: <name_prefix> <count> <causality> <variability> <extra annotations>
vref=0
gen_fmu_signals() {
    local prefix=$1 count=$2 causality=$3 variability=$4 extra=$5 n
    for (( n = 1; n <= count; n++ )); do
        cat <<EOF
    - signal: ${prefix}_${n}
      annotations:
        fmi_variable_vref: ${vref}
        fmi_variable_name: ${prefix}_${n}
        fmi_variable_type: Real
        fmi_variable_causality: ${causality}
        fmi_variable_variability: ${variability}
EOF
        [[ -z "${extra}" ]] || printf '%s\n' "${extra//\{n\}/${n}}"
        vref=$((vref + 1))
    done
}

{
    cat <<EOF
---
kind: SignalGroup
metadata:
  name: ${FMU_MODEL}
  labels:
    channel: signal_vector
    model: ${FMU_MODEL}
spec:
  signals:
EOF
    gen_fmu_signals input "${INPUTS}" input continuous ""
    gen_fmu_signals output "${OUTPUTS}" output continuous ""
    # Fixed parameters are set once from their start value, no SimBus exchange.
    gen_fmu_signals fixed "${FIXED_PARAMS}" parameter fixed \
        "        fmi_variable_start_value: '{n}.0'
        internal: true"
    gen_fmu_signals tunable "${TUNABLE_PARAMS}" parameter tunable \
        "        fmi_variable_start_value: '{n}.0'"
} > "${FMU_SG_FILE}"

# Target Signal Group: only the signals which are exchanged over SimBus.
gen_target_signals() {
    local prefix=$1 count=$2 causality=$3 n
    for (( n = 1; n <= count; n++ )); do
        cat <<EOF
    - signal: ${prefix}_${n}
      annotations:
        fmi_variable_causality: ${causality}
EOF
    done
}

{
    cat <<EOF
---
kind: SignalGroup
metadata:
  name: BenchmarkTarget
  labels:
    channel: target_vector
spec:
  signals:
EOF
    gen_target_signals input "${INPUTS}" input
    gen_target_signals output "${OUTPUTS}" output
    gen_target_signals tunable "${TUNABLE_PARAMS}" parameter
} > "${TARGET_SG_FILE}"
echo "Generated Signal Groups: inputs=${INPUTS}, outputs=${OUTPUTS}, fixed=${FIXED_PARAMS}, tunable=${TUNABLE_PARAMS}"


# 2. Assemble the standalone FMI MCL.
# ==================================
if [[ -n "${TIMING_SHIM_LIB:-}" ]]; then
    cp "${TIMING_SHIM_LIB}" "${MCL_DIR}/lib/$(basename "${MCL_LIB}")"
    cp "${MCL_LIB}" "${MCL_DIR}/lib/real_$(basename "${MCL_LIB}")"
else
    cp "${MCL_LIB}" "${MCL_DIR}/lib/"
fi
cp "${FMU_LIB}" "${FMU_DIR}/${FMU_BINARY}"

cat > "${MCL_DIR}/model.yaml" <<EOF
---
kind: Model
metadata:
  name: ${FMU_MODEL}
  annotations:
    mcl_adapter: 'fmi'
    mcl_version: '2.0'
    fmi_model_cosim: true
    fmi_model_version: '1.0'
    fmi_stepsize: '${STEP_SIZE}'
    fmi_guid: '{11111111-2222-3333-4444-555555555555}'
    fmi_resource_dir: 'fmu/resources'
spec:
  runtime:
    dynlib:
      - os: linux
        arch: amd64
        path: lib/$(basename "${MCL_LIB}")
    mcl:
      - os: linux
        arch: amd64
        path: fmu/${FMU_BINARY}
  channels:
    - alias: signal_channel
      selectors:
        channel: signal_vector
        model: ${FMU_MODEL}
EOF

# Stack without SimBus, models are started individually by ModelC.
cat > "${MCL_DIR}/stack.yaml" <<EOF
---
kind: Stack
metadata:
  name: ${STACK_NAME}
spec:
  connection:
    transport:
      redis:
        uri: ${REDIS_URI}
        timeout: 60
  models:
    - name: ${MCL_NAME}
      uid: ${MCL_UID}
      model:
        name: ${FMU_MODEL}
      channels:
        - name: signal
          alias: signal_channel
          selectors:
            channel: signal_vector
            model: ${FMU_MODEL}
EOF


# 3. Assemble the Simer simulation (SimBus + optional target model).
# =================================================================
# SimBus expects the FMI MCL (started outside of Simer) and the optional target.
EXPECTED_MODEL_COUNT=$((1 + WITH_TARGET))

if (( WITH_TARGET )); then
    mkdir -p "${TARGET_DIR}/data" "${TARGET_DIR}/lib"
    cp "${TARGET_LIB}" "${TARGET_DIR}/lib/"
    cp "${TARGET_SG_FILE}" "${TARGET_DIR}/data/signalgroup.yaml"
    cat > "${TARGET_DIR}/data/model.yaml" <<EOF
---
kind: Model
metadata:
  name: BenchmarkTarget
spec:
  runtime:
    dynlib:
      - os: linux
        arch: amd64
        path: model/${TARGET_NAME}/lib/$(basename "${TARGET_LIB}")
  channels:
    - alias: target_channel
      selectors:
        channel: target_vector
EOF
fi

{
    cat <<EOF
---
kind: Stack
metadata:
  name: ${STACK_NAME}
spec:
  runtime:
    env:
      SIMBUS_LOGLEVEL: ${LOG_LEVEL}
  connection:
    transport:
      redis:
        uri: ${REDIS_URI}
        timeout: 60
  models:
    - name: simbus
      model:
        name: simbus
      channels:
        - name: signal
          expectedModelCount: ${EXPECTED_MODEL_COUNT}
EOF
    if (( WITH_TARGET )); then
        cat <<EOF
    - name: ${TARGET_NAME}
      uid: ${TARGET_UID}
      model:
        name: BenchmarkTarget
      channels:
        - name: signal
          alias: target_channel
          selectors:
            channel: target_vector
      runtime:
        files:
          - model/${TARGET_NAME}/data/signalgroup.yaml
EOF
    fi
    cat <<EOF
---
kind: Model
metadata:
  name: simbus
EOF
} > "${SIMER_DATA_DIR}/simulation.yaml"
rm -f "${TARGET_SG_FILE}"

echo "Layout created: ${OUT_DIR} (target model: ${WITH_TARGET})"
find "${SIMER_DIR}" "${MCL_DIR}" -type f | sort
echo
echo "Run Simer (Redis, SimBus, target):"
echo "  docker run --rm -i --net=host -v ${SIMER_DIR}:/sim ghcr.io/boschglobal/dse-simer:latest \\"
echo "      -stack ${STACK_NAME} -stepsize ${STEP_SIZE} -endtime ${END_TIME}"
echo
echo "Run FMI MCL (separate shell):"
echo "  cd ${MCL_DIR} && SIMBUS_LOGLEVEL=${LOG_LEVEL} modelc --name ${MCL_NAME} \\"
echo "      --stepsize ${STEP_SIZE} --endtime ${END_TIME} \\"
echo "      stack.yaml model.yaml signalgroup.yaml"
