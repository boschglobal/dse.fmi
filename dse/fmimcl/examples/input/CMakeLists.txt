# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(Input)
set(MODEL_PATH "examples")


# Targets
# =======

add_library(input SHARED
    model.c
)
target_include_directories(input
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_directories(input
    PRIVATE
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(input
    PRIVATE
        $<$<BOOL:${WIN32}>:modelc>
)
install(
    TARGETS
        input
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
