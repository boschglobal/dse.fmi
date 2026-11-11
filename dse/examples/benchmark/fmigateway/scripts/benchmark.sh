#!/usr/bin/env bash
# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0
#
# Generate a benchmark FMI Gateway FMU (legacy layout) and a Simer simulation
# with a counterpart target model (dse/examples/benchmark/fmigateway/model.c).
#
# Setup (build gateway and target model first):
#   $ make build
#
# Usage (from dse.fmi repo root):
#   dse/examples/benchmark/fmigateway/scripts/benchmark.sh <scalar_channels> <signals_per_channel> <binary_signals> [with_target=1] [csv_steps=0]
#
#   csv_steps > 0 generates an importer input CSV (all GW inputs) in OUT_DIR/csv/input.csv.
#
# Environment overrides:
#   OUT_DIR       Output directory (default: out/benchmark)
#   FMI_VERSION   2 or 3 (default: 2)
#   GW_LIB_DIR    Directory containing libfmi{2,3}gateway.so (default: dse/build/_out/fmigateway/lib)
#   TARGET_LIB    Prebuilt target model (default: dse/build/_out/examples/benchmark/fmigateway/lib/libbenchmarktarget.so)
#   STEP_SIZE     Gateway step size written to stack.yaml (default: 0.0005)
#   END_TIME      Gateway end time written to stack.yaml (default: 0.002)
#   LOG_LEVEL     Gateway log level written to stack.yaml (default: 6)
#   REDIS_URI     Redis transport URI (default: redis://localhost:6379)

# Re-exec under bash when invoked via `sh` (bash-only syntax is used below).
if [ -z "${BASH_VERSION:-}" ]; then exec bash "$0" "$@"; fi

set -euo pipefail

usage() {
    echo "Usage: $0 <scalar_channels> <signals_per_channel> <binary_signals> [with_target=1] [csv_steps=0]" >&2
    exit 1
}

is_uint() { [[ "$1" =~ ^[0-9]+$ ]]; }

[[ $# -ge 3 && $# -le 5 ]] || usage
SCALAR_CHANNELS=$1
SIGNALS_PER_CHANNEL=$2
BINARY_SIGNALS=$3
WITH_TARGET=${4:-1}
CSV_STEPS=${5:-0}
is_uint "${SCALAR_CHANNELS}" && is_uint "${SIGNALS_PER_CHANNEL}" && is_uint "${BINARY_SIGNALS}" || usage
is_uint "${CSV_STEPS}" || usage
[[ "${WITH_TARGET}" == 0 || "${WITH_TARGET}" == 1 ]] || usage
if (( SCALAR_CHANNELS > 0 && SIGNALS_PER_CHANNEL == 0 )); then
    echo "signals_per_channel must be > 0 when scalar_channels > 0" >&2
    exit 1
fi
if (( SCALAR_CHANNELS == 0 && BINARY_SIGNALS == 0 )); then
    echo "At least one scalar channel or binary signal is required" >&2
    exit 1
fi

REPO_DIR=$(pwd)
: "${OUT_DIR:=${REPO_DIR}/out/benchmark}"
: "${FMI_VERSION:=2}"
: "${GW_LIB_DIR:=${REPO_DIR}/dse/build/_out/fmigateway/lib}"
: "${TARGET_LIB:=${REPO_DIR}/dse/build/_out/examples/benchmark/fmigateway/lib/libbenchmarktarget.so}"
: "${STEP_SIZE:=0.0005}"
: "${END_TIME:=0.002}"
: "${LOG_LEVEL:=6}"
: "${REDIS_URI:=redis://localhost:6379}"

FMI_TOOL_DIR="${REPO_DIR}/extra/tools/fmi"
FMI_TOOL="${FMI_TOOL_DIR}/bin/fmi"
SG_DIR="${OUT_DIR}/signalgroups"
GEN_DIR="${OUT_DIR}/gen"
FMU_DIR="${OUT_DIR}/fmu"
SIMER_DIR="${OUT_DIR}/simer"
CSV_DIR="${OUT_DIR}/csv"
TARGET_NAME=benchmark_target
TARGET_DIR="${SIMER_DIR}/model/${TARGET_NAME}"

case "${FMI_VERSION}" in
    2) FMU_PLATFORM_DIR=linux64 ;;
    3) FMU_PLATFORM_DIR=x86_64-linux ;;
    *) echo "Unsupported FMI_VERSION: ${FMI_VERSION}" >&2; exit 1 ;;
esac
MODEL_IDENTIFIER="libfmi${FMI_VERSION}gateway"
GW_LIB="${GW_LIB_DIR}/${MODEL_IDENTIFIER}.so"
[[ -f "${GW_LIB}" ]] || { echo "Gateway library not found: ${GW_LIB} (run 'make build')" >&2; exit 1; }
if (( WITH_TARGET )); then
    [[ -f "${TARGET_LIB}" ]] || { echo "Target model not found: ${TARGET_LIB} (run 'make build')" >&2; exit 1; }
fi

rm -rf "${SG_DIR}" "${GEN_DIR}" "${FMU_DIR}" "${SIMER_DIR}" "${CSV_DIR}"
mkdir -p "${SG_DIR}" "${GEN_DIR}" "${FMU_DIR}/resources" "${FMU_DIR}/binaries/${FMU_PLATFORM_DIR}" \
    "${SIMER_DIR}/data"


# 1. Generate Signal Groups (one file per Signal Group).
# =====================================================
SG_FILES=()
CHANNELS=()

for (( c = 1; c <= SCALAR_CHANNELS; c++ )); do
    channel="scalar_${c}"
    sg_file="${SG_DIR}/${channel}.yaml"
    {
        cat <<EOF
---
kind: SignalGroup
metadata:
  name: ${channel}
  labels:
    channel: ${channel}
  annotations:
    vector_type: scalar
spec:
  signals:
EOF
        for (( s = 1; s <= SIGNALS_PER_CHANNEL; s++ )); do
            # Alternate input/output so both directions are exercised.
            if (( s % 2 )); then causality=input; else causality=output; fi
            cat <<EOF
    - signal: ${channel}_sig_${s}
      annotations:
        fmi_variable_causality: ${causality}
EOF
        done
    } > "${sg_file}"
    SG_FILES+=("${sg_file}")
    CHANNELS+=("${channel}")
done

# Binary Signal Group last: the generator assigns vrefs in file order.
if (( BINARY_SIGNALS > 0 )); then
    channel="binary"
    sg_file="${SG_DIR}/${channel}.yaml"
    {
        cat <<EOF
---
kind: SignalGroup
metadata:
  name: ${channel}
  labels:
    channel: ${channel}
    type: binary
  annotations:
    vector_type: binary
    vector_name: network_vector
spec:
  signals:
EOF
        # Unique bus_id per signal, XML variable names are network_<bus_id>_<n>_<rx|tx>.
        for (( b = 1; b <= BINARY_SIGNALS; b++ )); do
            cat <<EOF
    - signal: can_${b}
      annotations:
        mime_type: 'application/x-automotive-bus; interface=stream; type=frame; bus=can; schema=fbs; bus_id=${b}; node_id=11; interface_id=0'
        dse.standards.fmi-ls-binary-to-text.encoding: ascii85
        dse.standards.fmi-ls-bus-topology.bus_id: ${b}
EOF
        done
    } > "${sg_file}"
    SG_FILES+=("${sg_file}")
    CHANNELS+=("${channel}")
fi
echo "Generated ${#SG_FILES[@]} Signal Group file(s) in ${SG_DIR}"


# 2. Generate Gateway files (fmu.yaml, model.yaml, modelDescription.xml).
# ======================================================================
echo "Building FMI toolset (${FMI_TOOL}) ..."
(cd "${FMI_TOOL_DIR}" && GOPRIVATE=github.com/boschglobal go build -o bin/fmi ./cmd/fmi)

SG_LIST=$(IFS=,; echo "${SG_FILES[*]}")
"${FMI_TOOL}" gen-gatewayfmu \
    -signalgroups "${SG_LIST}" \
    -outdir "${GEN_DIR}" \
    -fmiVersion "${FMI_VERSION}" \
    -name benchmark \
    -modelidentifier "${MODEL_IDENTIFIER}"


# 3. Generate stack.yaml.
# ======================
STACK_FILE="${GEN_DIR}/stack.yaml"
{
    cat <<EOF
---
kind: Stack
metadata:
  name: gateway
spec:
  connection:
    transport:
      redis:
        uri: ${REDIS_URI}
        timeout: 60
  models:
    - name: gateway
      uid: 42
      model:
        name: gateway
      annotations:
        step_size: ${STEP_SIZE}
        end_time: ${END_TIME}
        log_level: ${LOG_LEVEL}
      channels:
EOF
    for channel in "${CHANNELS[@]}"; do
        cat <<EOF
        - name: ${channel}
          alias: ${channel}
          selectors:
            channel: ${channel}
EOF
    done
} > "${STACK_FILE}"


# 4. Assemble the FMU layout.
# ==========================
cp "${GEN_DIR}/modelDescription.xml" "${FMU_DIR}/"
cp "${GEN_DIR}/fmu.yaml" "${GEN_DIR}/model.yaml" "${STACK_FILE}" "${FMU_DIR}/resources/"
cp "${GW_LIB}" "${FMU_DIR}/binaries/${FMU_PLATFORM_DIR}/"

echo "FMU layout created: ${FMU_DIR}"
find "${FMU_DIR}" -type f | sort


# 4a. Generate importer input CSV (format: dse/importer/csv.c).
# ============================================================
if (( CSV_STEPS > 0 )); then
    mkdir -p "${CSV_DIR}"
    python3 - "${FMU_DIR}/modelDescription.xml" "${CSV_DIR}/input.csv" \
        "${CSV_STEPS}" "${STEP_SIZE}" <<'EOF'
import sys
import xml.etree.ElementTree as ET

xml_file, csv_file, steps, step_size = sys.argv[1:5]
steps, step_size = int(steps), float(step_size)

root = ET.parse(xml_file).getroot()
vrs = []
if root.get("fmiVersion", "").startswith("2"):
    for sv in root.iter("ScalarVariable"):
        if sv.get("causality") == "input" and sv.find("Real") is not None:
            vrs.append(sv.get("valueReference"))
else:
    for v in root.iter("Float64"):
        if v.get("causality") == "input":
            vrs.append(v.get("valueReference"))

with open(csv_file, "w") as f:
    f.write(",".join(["time"] + vrs) + "\n")
    for step in range(steps):
        values = [f"{step + i * 0.001:g}" for i in range(len(vrs))]
        line = ",".join([f"{step * step_size:.6f}"] + values)
        if len(line) >= 4095:
            sys.exit(f"CSV line exceeds CSV_LINE_MAXLEN (4096): {len(line)}")
        f.write(line + "\n")
print(f"Generated {csv_file}: {len(vrs)} inputs, {steps} steps")
EOF
fi


# 5. Assemble the Simer simulation (simbus + optional target model).
# =================================================================
if (( WITH_TARGET )); then
    EXPECTED_MODEL_COUNT=2
    mkdir -p "${TARGET_DIR}/data" "${TARGET_DIR}/lib"
    cp "${TARGET_LIB}" "${TARGET_DIR}/lib/"
    TARGET_FILES=()
    for sg_file in "${SG_FILES[@]}"; do
        tgt_file="${TARGET_DIR}/data/$(basename "${sg_file}")"
        # Target needs its own node_id, otherwise NCodec filters the gateway frames.
        sed 's/node_id=11;/node_id=1;/' "${sg_file}" > "${tgt_file}"
        TARGET_FILES+=("model/${TARGET_NAME}/data/$(basename "${sg_file}")")
    done
    {
        cat <<EOF
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
EOF
        for channel in "${CHANNELS[@]}"; do
            cat <<EOF
    - alias: ${channel}
      selectors:
        channel: ${channel}
EOF
        done
    } > "${TARGET_DIR}/data/model.yaml"
else
    EXPECTED_MODEL_COUNT=1
fi

{
    cat <<EOF
---
kind: Stack
metadata:
  name: benchmark_stack
spec:
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
EOF
    for channel in "${CHANNELS[@]}"; do
        cat <<EOF
        - name: ${channel}
          expectedModelCount: ${EXPECTED_MODEL_COUNT}
EOF
    done
    if (( WITH_TARGET )); then
        cat <<EOF
    - name: ${TARGET_NAME}
      uid: 43
      model:
        name: BenchmarkTarget
      runtime:
        files:
EOF
        for f in "${TARGET_FILES[@]}"; do
            echo "          - ${f}"
        done
        echo "      channels:"
        for channel in "${CHANNELS[@]}"; do
            cat <<EOF
        - name: ${channel}
          alias: ${channel}
          selectors:
            channel: ${channel}
EOF
        done
    fi
    cat <<EOF
---
kind: Model
metadata:
  name: simbus
EOF
} > "${SIMER_DIR}/data/simulation.yaml"

echo "Simer layout created: ${SIMER_DIR} (target model: ${WITH_TARGET})"
find "${SIMER_DIR}" -type f | sort
