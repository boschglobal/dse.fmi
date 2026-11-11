# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(EXAMPLE_PATH "examples/benchmark/fmigateway")
project(examplebenchmark)


# Benchmark Target - Gateway counterpart model
# ============================================
set(BM_TARGET benchmarktarget)
add_library(${BM_TARGET} SHARED
    model.c
)
target_include_directories(${BM_TARGET}
    PRIVATE
        ${REPO_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_directories(${BM_TARGET}
    PRIVATE
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(${BM_TARGET}
    PRIVATE
        $<$<BOOL:${WIN32}>:modelc>
)
install(TARGETS ${BM_TARGET}
    LIBRARY DESTINATION
        ${EXAMPLE_PATH}/lib
)
