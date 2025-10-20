# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmimcl)


add_library(modelc_runtime OBJECT
    ${DSE_MODELC_SOURCE_DIR}/model/mcl.c
    ${DSE_MODELC_SOURCE_DIR}/model/schema.c
)
target_include_directories(modelc_runtime
    PUBLIC
        ${DSE_MODELC_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
)
target_link_libraries(modelc_runtime
    PRIVATE
        xml
        m
)

add_library(modelc_mock OBJECT
    ${dse_modelc_SOURCE_DIR}/dse/mocks/simmock.c
    ${dse_clib_SOURCE_DIR}/dse/clib/data/marshal.c
)
target_include_directories(modelc_mock
    PUBLIC
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
)
target_compile_definitions(modelc_mock
    PUBLIC
        CMOCKA_TESTING
)
target_link_libraries(modelc_mock
    PRIVATE
        cmocka
)

add_library(fmimcl_runtime OBJECT
    ${DSE_CLIB_SOURCE_DIR}/util/ascii85.c
    ${REPO_DIR}/dse/fmimcl/engine.c
    ${REPO_DIR}/dse/fmimcl/fmimcl.c
    ${REPO_DIR}/dse/fmimcl/parser.c
    ${REPO_DIR}/dse/fmimcl/adapter/fmi2mcl.c
)
target_include_directories(fmimcl_runtime
    PUBLIC
        ${REPO_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
)
target_compile_definitions(fmimcl_runtime
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(fmimcl_runtime
    PRIVATE
        cmocka
)


# Target - test_fmimcl
# ====================
add_executable(test_fmimcl
    __test__.c
    test_parser.c
    test_mcl.c
    test_engine.c
    test_fmi2.c
    mock/mock.c
)
target_include_directories(test_fmimcl
    PRIVATE
        ./
)
target_compile_definitions(test_fmimcl
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(test_fmimcl
    PUBLIC
        fmimcl_runtime
        modelc_runtime
        clib_runtime
    PRIVATE
        yaml
        cmocka
        dl
        m
)
install(
    TARGETS
        test_fmimcl
)
install(
    FILES
        data/simulation.yaml
        data/fmu.yaml
        data/parser_sort.yaml
        data/mcl_mock.yaml
        data/mcl.yaml
    DESTINATION
        data
)


# Target - test_mstep
# ===================
add_executable(test_mstep
    mstep/__test__.c
    mstep/test_mstep.c
)
target_include_directories(test_mstep
    PRIVATE
#        ./
)
target_compile_definitions(test_mstep
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_directories(test_mstep
    PRIVATE
        ${dse_modelc_lib_SOURCE_DIR}/lib
)
target_link_libraries(test_mstep
    PUBLIC
        modelc_mock
        -Wl,-Bstatic modelc_bundled -Wl,-Bdynamic ${CMAKE_DL_LIBS}
    PRIVATE
        cmocka
        dl
        rt
        m
)
install(
    TARGETS
        test_mstep
)
install(
    FILES
        mstep/fmu_mstep.yaml
    DESTINATION
        data
)