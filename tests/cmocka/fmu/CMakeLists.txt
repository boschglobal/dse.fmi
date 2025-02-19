# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmu)


# FMU Code
# ========
set(DSE_FMU_SOURCE_DIR ../../../dse/fmu)
set(DSE_FMU_SOURCE_FILES
    ${DSE_FMU_SOURCE_DIR}/ascii85.c
    ${DSE_FMU_SOURCE_DIR}/fmu.c
    ${DSE_FMU_SOURCE_DIR}/fmi2fmu.c
    ${DSE_FMU_SOURCE_DIR}/fmi2variable.c
    ${DSE_FMU_SOURCE_DIR}/signal.c
)
set(DSE_FMU_INCLUDE_DIR "${DSE_FMU_SOURCE_DIR}/../..")


# NCodec Objects
# ==============
add_library(flatcc_runtime OBJECT
    ${FLATCC_SOURCE_DIR}/src/builder.c
    ${FLATCC_SOURCE_DIR}/src/emitter.c
    ${FLATCC_SOURCE_DIR}/src/refmap.c
)
target_include_directories(flatcc_runtime
    PRIVATE
        ${FLATCC_INCLUDE_DIR}
)
add_library(network_codec OBJECT
    ${DSE_FMU_SOURCE_DIR}/ncodec.c
    ${DSE_NCODEC_SOURCE_DIR}/codec.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/codec.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/frame_can_fbs.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/pdu_fbs.c
)
target_include_directories(network_codec
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}/dse/ncodec/libs
        ${FMU_INCLUDE_DIR}
        ${FLATCC_INCLUDE_DIR}
        ${AUTOMOTIVE_BUS_SCHEMA_SOURCE_DIR}
        ../../..
)
target_link_libraries(network_codec
    PRIVATE
        flatcc_runtime
)


# Targets
# =======

# Target - FMU
# ------------
add_executable(test_fmu
    __test__.c
    test_ascii85.c
    test_signals.c
    test_variables.c
    ${DSE_FMU_SOURCE_FILES}
)
target_include_directories(test_fmu
    PRIVATE
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_FMU_INCLUDE_DIR}
)
target_compile_definitions(test_fmu
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_directories(test_fmu
    PRIVATE
        ${MODELC_BINARY_DIR}/lib
)
target_link_libraries(test_fmu
    PUBLIC
    PRIVATE
        clib_runtime
        flatcc_runtime
        network_codec
        yaml
        xml
        cmocka
        dl
        m
)
install(TARGETS test_fmu)
install(
    FILES
        data/modelDescription.xml
    DESTINATION
        data/test_fmu
)
install(
    FILES
        data/modelDescription.xml
    DESTINATION
        data/test_fmu/resources
)
