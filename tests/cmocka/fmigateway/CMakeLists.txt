# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmigateway)


add_library(fmigateway_runtime OBJECT
    ${DSE_CLIB_SOURCE_DIR}/util/ascii85.c
    ${REPO_DIR}/dse/fmigateway/fmigateway.c
    ${REPO_DIR}/dse/fmigateway/index.c
    ${REPO_DIR}/dse/fmigateway/session.c
    ${REPO_DIR}/dse/fmigateway/session_unix.c
    ${REPO_DIR}/dse/fmigateway/parser.c
    ${REPO_DIR}/dse/fmigateway/signal.c
    ${REPO_DIR}/dse/fmu/fmi2fmu.c
)
target_include_directories(fmigateway_runtime
    PUBLIC
        ${REPO_DIR}
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_compile_definitions(fmigateway_runtime
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(fmigateway_runtime
    PRIVATE
        cmocka
        xml
)

add_library(modelcgateway_runtime OBJECT
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
target_include_directories(modelcgateway_runtime
    PUBLIC
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${MSGPACKC_SOURCE_DIR}/include
        ${YAML_SOURCE_DIR}/include
)
target_compile_definitions(modelcgateway_runtime
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
        MODELC_VERSION="${DSE_MODELC_VERSION}"
)
target_link_libraries(modelcgateway_runtime
    PRIVATE
        cmocka
        yaml
        dl
)


# Target - test_fmigateway
# ========================
add_executable(test_fmigateway
    __test__.c
    test_parser.c
    test_fmi2.c
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
        fmigateway_runtime
        modelcgateway_runtime
        clib_runtime
    PRIVATE
        ab-codec
        cmocka
)
install(
    TARGETS
        test_fmigateway
)
install(
    FILES
    DESTINATION
        data/fmigateway
)
