# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODEL_PATH "${MODULE_LC}/examples")
project(Input)


# Target - Input Model
# ====================
set(TARGET "input")
add_library(${TARGET} SHARED
    model.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/ascii85.c
)
target_include_directories(${TARGET}
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_directories(${TARGET}
    PRIVATE
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(${TARGET}
    PRIVATE
        $<$<BOOL:${WIN32}>:modelc>
)
install(
    TARGETS
        ${TARGET}
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
)
install(
    FILES
        data/model.yaml
        data/simulation.yaml
    DESTINATION
         ${MODEL_PATH}/data
)
