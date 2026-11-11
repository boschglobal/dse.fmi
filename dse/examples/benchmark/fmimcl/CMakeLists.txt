# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(EXAMPLE_PATH "examples/benchmark/fmimcl")
project(examplebenchmarkfmimcl)


# Benchmark FMU - minimal FMI 2 Co-Simulation FMU
# ===============================================
set(BM_FMU mclbenchmarkfmu)
add_library(${BM_FMU} SHARED
    fmu.c
)
target_include_directories(${BM_FMU}
    PRIVATE
        ${FMI2_INCLUDE_DIR}
)
install(TARGETS ${BM_FMU}
    LIBRARY DESTINATION
        ${EXAMPLE_PATH}/lib
)


# Benchmark Target - FMI MCL counterpart model
# ============================================
set(BM_TARGET mclbenchmarktarget)
add_library(${BM_TARGET} SHARED
    model.c
)
target_include_directories(${BM_TARGET}
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
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


# Timing Shim - measures model_step() of the FMI MCL (scripts/timing.sh)
# =====================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BM_SHIM mclbenchmarkshim)
    add_library(${BM_SHIM} SHARED
        mcl_timing_shim.c
    )
    target_link_libraries(${BM_SHIM}
        PRIVATE
            ${CMAKE_DL_LIBS}
    )
    install(TARGETS ${BM_SHIM}
        LIBRARY DESTINATION
            ${EXAMPLE_PATH}/lib
    )
endif()
