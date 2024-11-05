# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(Fmi2FMU)
set(MODEL_PATH "${MODULE_LC}/examples")

set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")


# Targets
# =======

add_library(fmi2fmu SHARED
    fmi2fmu.c
    ../../../fmimodelc/ascii85.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/set.c
)
target_include_directories(fmi2fmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
)
install(
    TARGETS
        fmi2fmu
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
)

install(
    TARGETS
        fmi2fmu
    LIBRARY DESTINATION
        ${MODEL_PATH}/fmu/binaries/linux64
)
install(
    FILES
        modelDescription.xml
    DESTINATION
         ${MODEL_PATH}/fmu
)
