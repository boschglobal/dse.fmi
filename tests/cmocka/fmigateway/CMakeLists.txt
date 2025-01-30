# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmigateway)



# FMI ModelC Code
# ===============
set(DSE_FMIGATEWAY_SOURCE_DIR ../../../dse/fmigateway)
set(DSE_FMI2FMU_SOURCE_DIR ../../../dse/fmu)
set(DSE_FMIGATEWAY_SOURCE_FILES
    ${DSE_FMI2FMU_SOURCE_DIR}/fmi2fmu.c
    ${DSE_FMI2FMU_SOURCE_DIR}/ascii85.c
    ${DSE_FMI2FMU_SOURCE_DIR}/signal.c
    ${DSE_FMIGATEWAY_SOURCE_DIR}/fmigateway.c
    ${DSE_FMIGATEWAY_SOURCE_DIR}/index.c
    ${DSE_FMIGATEWAY_SOURCE_DIR}/session.c
    ${DSE_FMIGATEWAY_SOURCE_DIR}/session_unix.c
    ${DSE_FMIGATEWAY_SOURCE_DIR}/parser.c
)
set(DSE_FMIGATEWAY_INCLUDE_DIR "${DSE_FMIGATEWAY_SOURCE_DIR}/../..")

set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/fmi/fmi2/headers")

# External Project - DSE Network Codec
# ------------------------------------
set(DSE_NCODEC_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec")
set(DSE_NCODEC_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec")
find_library(DSE_NCODEC_LIB
    NAMES
        libautomotive-bus-codec.a
    PATHS
        ${DSE_NCODEC_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(dse_ncodec STATIC IMPORTED GLOBAL)
set_target_properties(dse_ncodec
    PROPERTIES
        IMPORTED_LOCATION "${DSE_NCODEC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_NCODEC_SOURCE_DIR}"
)
set(DSE_NCODEC_SOURCE_FILES
    ${DSE_NCODEC_SOURCE_DIR}/dse/ncodec/codec.c
)
set(DSE_NCODEC_INCLUDE_DIR "${DSE_NCODEC_SOURCE_DIR}")
set(DSE_MODELC_SOURCE_FILES
    ${DSE_MODELC_SOURCE_DIR}/model/gateway.c
    ${DSE_MODELC_SOURCE_DIR}/model/model.c
    ${DSE_MODELC_SOURCE_DIR}/model/ncodec.c
    ${DSE_MODELC_SOURCE_DIR}/model/schema.c
    ${DSE_MODELC_SOURCE_DIR}/model/signal.c
    ${DSE_MODELC_SOURCE_DIR}/model/trace.c

    ${DSE_MODELC_SOURCE_DIR}/controller/controller_stub.c
    ${DSE_MODELC_SOURCE_DIR}/controller/model_function.c
    ${DSE_MODELC_SOURCE_DIR}/controller/loader.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_debug.c
    ${DSE_MODELC_SOURCE_DIR}/controller/modelc_args.c
    ${DSE_MODELC_SOURCE_DIR}/controller/step.c
    ${DSE_MODELC_SOURCE_DIR}/controller/model_runtime.c

    ${DSE_MODELC_SOURCE_DIR}/adapter/adapter_loopb.c
    ${DSE_MODELC_SOURCE_DIR}/adapter/index.c
)

# Targets
# =======

# Target - FMI Gateway
# -------------------
add_executable(test_fmigateway
    __test__.c
    test_parser.c
    test_fmi2.c
    ${DSE_NCODEC_SOURCE_FILES}
    ${DSE_FMIGATEWAY_SOURCE_FILES}
    ${DSE_MODELC_SOURCE_FILES}
    ${DSE_CLIB_SOURCE_FILES}
)
target_include_directories(test_fmigateway
    PRIVATE
        ${DSE_NCODEC_INCLUDE_DIR}
        ${DSE_FMIGATEWAY_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
        ./
)
target_compile_definitions(test_fmigateway
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${DSE_MODELC_VERSION}"
)
target_link_libraries(test_fmigateway
    PUBLIC
    PRIVATE
        xml
        dse_ncodec
        yaml
        cmocka
        dl
        m
)
install(TARGETS test_fmigateway)
install(
    FILES
    DESTINATION
        data/fmigateway
)

