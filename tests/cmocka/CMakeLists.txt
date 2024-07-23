# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_modelc)

include(FetchContent)
include(GNUInstallDirs)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/_out)

set(CMAKE_ENABLE_EXPORTS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb")
list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})


# External Project - yaml
# -----------------------
set(YAML_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/yaml")
set(YAML_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/yaml")
find_library(YAML_LIB
    NAMES
        libyaml.a
    PATHS
        ${YAML_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(yaml STATIC IMPORTED GLOBAL)
set_target_properties(yaml
    PROPERTIES
        IMPORTED_LOCATION "${YAML_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${YAML_BINARY_DIR}"
)


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 ${ExternalProject__CLIB__URL}
    HTTP_USERNAME       ${ExternalProject__CLIB__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__CLIB__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse/clib)
set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/util/yaml.c
    ${DSE_CLIB_SOURCE_DIR}/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/collections/set.c
    ${DSE_CLIB_SOURCE_DIR}/data/marshal.c
)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/../..")


# External Project - DSE ModelC
# -----------------------------
FetchContent_Declare(dse_modelc
    URL                 ${ExternalProject__MODELC__URL}
    HTTP_USERNAME       ${ExternalProject__MODELC__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__MODELC__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc"
)
FetchContent_MakeAvailable(dse_modelc)
set(DSE_MODELC_SOURCE_DIR ${dse_modelc_SOURCE_DIR}/dse/modelc)
set(DSE_MODELC_SOURCE_FILES
    ${DSE_MODELC_SOURCE_DIR}/model/mcl.c
    ${DSE_MODELC_SOURCE_DIR}/model/schema.c
)
set(DSE_MODELC_INCLUDE_DIR "${DSE_MODELC_SOURCE_DIR}/../..")


# External Project - DSE ModelC Library (for linking to mstep)
# -------------------------------------
set(MODELC_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib")
find_library(MODELC_LIB
    NAMES
        libmodelc_bundled.a
    PATHS
        ${MODELC_BINARY_DIR}/lib
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(modelc STATIC IMPORTED GLOBAL)
set_target_properties(modelc
    PROPERTIES
        IMPORTED_LOCATION "${MODELC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MODELC_BINARY_DIR}"
)
set(DSE_MODELC_LIB_INCLUDE_DIR "${MODELC_BINARY_DIR}/include")
set(DSE_MODELC_LIB_MOCK_SOURCE_DIR "${MODELC_BINARY_DIR}/mocks")
set(DSE_MODELC_LIB_MOCK_SOURCE_FILES
    ${DSE_MODELC_LIB_MOCK_SOURCE_DIR}/simmock.c
    ${DSE_CLIB_SOURCE_DIR}/data/marshal.c
)



# FMIMCL Code
# ===========
set(DSE_FMIMCL_SOURCE_DIR ../../dse/fmimcl)
set(DSE_FMIMCL_SOURCE_FILES
    ${DSE_FMIMCL_SOURCE_DIR}/engine.c
    ${DSE_FMIMCL_SOURCE_DIR}/fmimcl.c
    ${DSE_FMIMCL_SOURCE_DIR}/parser.c
    ${DSE_FMIMCL_SOURCE_DIR}/adapter/fmi2mcl.c
)
set(DSE_FMIMCL_INCLUDE_DIR "${DSE_FMIMCL_SOURCE_DIR}/../..")



# Targets
# =======

# Target - Stub
# -------------
# add_subdirectory(${DSE_FMIMCL_SOURCE_DIR}/examples/stub build)


# Target - FMI MCL
# ----------------
add_executable(test_fmimcl
    fmimcl/__test__.c
    fmimcl/test_parser.c
    fmimcl/test_mcl.c
    fmimcl/test_engine.c
    fmimcl/test_fmi2.c
    fmimcl/mock/mock.c
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
    PUBLIC
    PRIVATE
        cmocka
        yaml
        dl
        m
)
install(TARGETS test_fmimcl)
install(
    FILES
        fmimcl/data/simulation.yaml
        fmimcl/data/fmu.yaml
        fmimcl/data/parser_sort.yaml
        fmimcl/data/mcl_mock.yaml
        fmimcl/data/mcl.yaml
    DESTINATION
        data
)

# Target - MSTEP
# --------------
add_executable(test_mstep
    fmimcl/mstep/__test__.c
    fmimcl/mstep/test_mstep.c
    ${DSE_MODELC_LIB_MOCK_SOURCE_FILES}
)
target_include_directories(test_mstep
    PRIVATE
        ${DSE_FMIMCL_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_LIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ./
)
target_compile_definitions(test_mstep
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(test_mstep
    PUBLIC
        -Wl,-Bstatic modelc -Wl,-Bdynamic ${CMAKE_DL_LIBS}
    PRIVATE
        cmocka
        dl
        rt
        m
)
install(TARGETS test_mstep)
install(
    FILES
        fmimcl/mstep/fmu_mstep.yaml
    DESTINATION
        data
)



add_subdirectory(fmimodelc)
