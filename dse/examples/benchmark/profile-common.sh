#!/usr/bin/env bash

# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0
#
# Helpers shared by the benchmark profile.sh scripts (source it, do not run).
#
# Environment overrides:
#   PROFILE       callgrind (default), cachegrind or gdb
#   PROFILE_DIR   Where profiles are written (default: <cwd>/out/profiles)
#   SIMER_IMAGE   Simer Docker image
#   REDIS_PORT    Port polled to detect that Simer is ready (default: 6379)

: "${ENTRY_DIR:=$(pwd)}"
: "${PROFILE:=callgrind}"
: "${PROFILE_DIR:=${ENTRY_DIR}/out/profiles}"
: "${SIMER_IMAGE:=ghcr.io/boschglobal/dse-simer:2.3.39}"
: "${REDIS_PORT:=6379}"


# Profiling
# =========

profile_check() {
    case "${PROFILE}" in
        callgrind|cachegrind|gdb) ;;
        *)
            echo "Unknown PROFILE: ${PROFILE} (callgrind|cachegrind|gdb)" >&2
            exit 1
            ;;
    esac
}

# Print the header comment (from line 5 to the first blank line) of a script.
profile_help() {
    sed -n '5,/^$/p' "$1" | sed 's/^# \{0,1\}//'
}

# Run a command under the selected profiler.
profile_run() {
    if [[ "${PROFILE}" == gdb ]]; then
        gdb --args "$@"
        return
    fi

    mkdir -p "${PROFILE_DIR}"
    local shim_dir rc=0
    shim_dir=$(mktemp -d)
    # Valgrind forgets the line info of dlclose'd libraries (the models) before
    # the profile is written, which shows up as "???"; so make dlclose a no-op.
    if printf 'int dlclose(void *h) { (void)h; return 0; }\n' |
        cc -shared -fPIC -x c -o "${shim_dir}/libnodlclose.so" -; then
        LD_PRELOAD="${shim_dir}/libnodlclose.so" \
            valgrind --tool="${PROFILE}" \
            "--${PROFILE}-out-file=${PROFILE_DIR}/${PROFILE}.out.%p" \
            "$@" || rc=$?
    else
        rc=1
    fi
    rm -rf "${shim_dir}"
    return "${rc}"
}

# Print the newest profile file.
profile_latest() {
    local file
    file=$(ls -1t "${PROFILE_DIR}/${PROFILE}".out.* 2>/dev/null |
        head -n 1 || true)
    if [[ -z "${file}" ]]; then
        echo "No ${PROFILE} output found in ${PROFILE_DIR}" >&2
        return 1
    fi
    printf '%s\n' "${file}"
}

# View the newest profile.
#   profile_view <inclusive|tree|code> <sed_script> <source_dir>...
# sed_script maps source paths recorded in the profile (container paths) to
# local paths; it may be empty. source_dirs are searched for the sources.
profile_view() {
    local mode=$1 path_map=$2
    shift 2

    if [[ "${PROFILE}" == gdb ]]; then
        echo "GDB does not produce a profile to view." >&2
        return 1
    fi

    local file dir include_args=()
    file=$(profile_latest)
    for dir in "$@"; do
        include_args+=(--include="${dir%/}/")
    done

    if [[ "${PROFILE}" == cachegrind ]]; then
        cg_annotate --threshold=1 "${include_args[@]}" "${file}"
        return
    fi

    if [[ -n "${path_map}" ]]; then
        sed -i "${path_map}" "${file}"
    fi
    case "${mode}" in
        inclusive)
            # Drop libc/loader start-up rows and rows repeating the previous
            # count (the same cost reported again for the same function).
            callgrind_annotate --inclusive=yes --auto=no \
                "${include_args[@]}" "${file}" |
                grep -vE 'ld-linux|libc\.so|libc_start|:main' |
                awk '($1 != last_count || $1 == "") {
                    print
                    last_count = $1
                }'
            ;;
        tree)
            callgrind_annotate --tree=caller --auto=no \
                "${include_args[@]}" "${file}"
            ;;
        code)
            # Annotate functions until 90% of the total cost is covered.
            callgrind_annotate --threshold=90 \
                "${include_args[@]}" "${file}"
            ;;
        *)
            echo "Unknown view mode: ${mode} (inclusive|tree|code)" >&2
            return 2
            ;;
    esac
}


# Simer (Redis, SimBus and optional target model in Docker)
# =========================================================

# Start Simer in the background: simer_start <sim_dir> [simer args...]
simer_start() {
    local sim_dir=$1
    shift
    simer_stop
    docker run --name simer -i --rm -v "${sim_dir}:/sim" \
        --env SIMBUS_LOGLEVEL=6 \
        --net=host \
        "${SIMER_IMAGE}" "$@" &
}

# Wait until Redis accepts connections, then give SimBus time to subscribe.
simer_wait() {
    local i
    for (( i = 0; i < 60; i++ )); do
        if (exec 3<>"/dev/tcp/localhost/${REDIS_PORT}") 2>/dev/null; then
            sleep 2
            return 0
        fi
        sleep 1
    done
    echo "Simer did not start (Redis port ${REDIS_PORT})" >&2
    return 1
}

simer_stop() {
    docker rm -f simer >/dev/null 2>&1 || true
}
