# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODEL_PATH "${MODULE_LC}/examples")
project(Fmi2FMU)


# Target - FMU for testing FMI MCL
# ================================
set(TARGET "mclfmi2fmu")
add_library(${TARGET} SHARED
    fmi2fmu.c
    ${CLIB_SOURCE_FILES}
)
target_include_directories(${TARGET}
    PRIVATE
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
)
install(
    TARGETS
        ${TARGET}
    LIBRARY DESTINATION
        ${MODEL_PATH}/lib
)
install(
    TARGETS
        ${TARGET}
    LIBRARY DESTINATION
        ${MODEL_PATH}/fmu/binaries/linux64
)
install(
    FILES
        modelDescription.xml
    DESTINATION
         ${MODEL_PATH}/fmu
)
