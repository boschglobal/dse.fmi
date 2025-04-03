# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmimcl)


set(DSE_MODELC_SOURCE_FILES
    ${DSE_MODELC_SOURCE_DIR}/model/mcl.c
    ${DSE_MODELC_SOURCE_DIR}/model/schema.c
)

set(DSE_MODELC_LIB_MOCK_SOURCE_DIR "${dse_modelc_lib_SOURCE_DIR}/mocks")
set(DSE_MODELC_LIB_MOCK_SOURCE_FILES
    ${DSE_MODELC_LIB_MOCK_SOURCE_DIR}/simmock.c
    ${DSE_CLIB_SOURCE_DIR}/data/marshal.c
)


# FMIMCL Code
# ===========
set(DSE_FMIMCL_SOURCE_DIR ../../../dse/fmimcl)
set(DSE_FMIMCL_SOURCE_FILES
    ${DSE_FMIMCL_SOURCE_DIR}/engine.c
    ${DSE_FMIMCL_SOURCE_DIR}/fmimcl.c
    ${DSE_FMIMCL_SOURCE_DIR}/parser.c
    ${DSE_FMIMCL_SOURCE_DIR}/adapter/fmi2mcl.c
    ${DSE_FMIMCL_SOURCE_DIR}/../fmu/ascii85.c
)
set(DSE_FMIMCL_INCLUDE_DIR "${DSE_FMIMCL_SOURCE_DIR}/../..")


# Target - FMI MCL
# ----------------
add_executable(test_fmimcl
    __test__.c
    test_parser.c
    test_mcl.c
    test_engine.c
    test_fmi2.c
    mock/mock.c
    ${DSE_FMIMCL_SOURCE_FILES}
    ${DSE_MODELC_SOURCE_FILES}
    ${DSE_CLIB_SOURCE_FILES}
)
target_include_directories(test_fmimcl
    PRIVATE
        ${DSE_FMIMCL_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
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
    PRIVATE
        cmocka
        yaml
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

# Target - MSTEP
# --------------
add_executable(test_mstep
    mstep/__test__.c
    mstep/test_mstep.c
    ${DSE_MODELC_LIB_MOCK_SOURCE_FILES}
)
target_include_directories(test_mstep
    PRIVATE
        ${DSE_FMIMCL_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ./
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