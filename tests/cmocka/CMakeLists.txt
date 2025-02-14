# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_modelc)

include(FetchContent)
include(ExternalProject)
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


# External Project - xml
# ----------------------
ExternalProject_Add(libxml2
    URL https://github.com/GNOME/libxml2/archive/refs/tags/v2.13.5.zip
    CMAKE_ARGS
        -D CMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libxml2
        -D CMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -D BUILD_SHARED_LIBS=OFF
        -D LIBXML2_WITH_HTML=OFF
        -D LIBXML2_WITH_HTTP=OFF
        -D LIBXML2_WITH_ICONV=OFF
        -D LIBXML2_WITH_LZMA=OFF
        -D LIBXML2_WITH_PYTHON=OFF
        -D LIBXML2_WITH_SCHEMATRON=OFF
        -D LIBXML2_WITH_TESTS=OFF
        -D LIBXML2_WITH_THREAD_ALLOC=OFF
        -D LIBXML2_WITH_THREADS=OFF
        -D LIBXML2_WITH_TREE=OFF
        -D LIBXML2_WITH_VALID=OFF
        -D LIBXML2_WITH_WRITER=OFF
        -D LIBXML2_WITH_ZLIB=OFF
    INSTALL_DIR "${CMAKE_BINARY_DIR}/libxml2"
    UPDATE_COMMAND ""
)
ExternalProject_Get_property(libxml2 SOURCE_DIR BINARY_DIR INSTALL_DIR)
set(XML_SOURCE_DIR "${SOURCE_DIR}")
set(XML_BINARY_DIR "${BINARY_DIR}")
set(XML_INSTALL_DIR "${INSTALL_DIR}")

add_library(xml STATIC IMPORTED GLOBAL)
add_dependencies(xml libxml2)
file(MAKE_DIRECTORY "${XML_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${XML_INSTALL_DIR}/include/libxml2")
set_target_properties(xml
    PROPERTIES
        IMPORTED_LOCATION "${XML_BINARY_DIR}/libxml2.a"
        INTERFACE_INCLUDE_DIRECTORIES "${XML_INSTALL_DIR}/include/libxml2"
)


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse/clib)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/../..")

add_library(clib_runtime OBJECT
    ${DSE_CLIB_SOURCE_DIR}/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/util/yaml.c
    ${DSE_CLIB_SOURCE_DIR}/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/collections/set.c
    ${DSE_CLIB_SOURCE_DIR}/data/marshal.c
)
target_include_directories(clib_runtime
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
        ${YAML_SOURCE_DIR}/include
)

set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/util/yaml.c
    ${DSE_CLIB_SOURCE_DIR}/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/collections/set.c
    ${DSE_CLIB_SOURCE_DIR}/data/marshal.c
)
set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/fmi/fmi2/headers")


# External Project - DSE ModelC
# -----------------------------
FetchContent_Declare(dse_modelc
    URL                 $ENV{DSE_MODELC_URL}
    HTTP_USERNAME       $ENV{DSE_MODELC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_MODELC_URL_TOKEN}
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


# External Project - DSE Network Codec
# ------------------------------------
FetchContent_Declare(dse_network_codec
    URL                 $ENV{DSE_NCODEC_URL}
    HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
)
FetchContent_MakeAvailable(dse_network_codec)
set(DSE_NCODEC_SOURCE_DIR ${dse_network_codec_SOURCE_DIR}/dse/ncodec)
set(DSE_NCODEC_INCLUDE_DIR "${DSE_NCODEC_SOURCE_DIR}/../..")


# Automotive Bus Schema
# ---------------------
FetchContent_Declare(automotive_bus_schema
    URL                 $ENV{AUTOMOTIVE_BUS_SCHEMA_URL}
    HTTP_USERNAME       $ENV{AUTOMOTIVE_BUS_SCHEMA_URL_USER}
    HTTP_PASSWORD       $ENV{AUTOMOTIVE_BUS_SCHEMA_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/automotive.bus.schema"
)
FetchContent_MakeAvailable(automotive_bus_schema)
set(AUTOMOTIVE_BUS_SCHEMA_SOURCE_DIR ${automotive_bus_schema_SOURCE_DIR}/flatbuffers/c)
set(FLATCC_SOURCE_DIR ${AUTOMOTIVE_BUS_SCHEMA_SOURCE_DIR}/automotive_bus_schema/flatcc)
set(FLATCC_INCLUDE_DIR ${FLATCC_SOURCE_DIR}/include)



# FMIMCL Code
# ===========
set(DSE_FMIMCL_SOURCE_DIR ../../dse/fmimcl)
set(DSE_FMIMCL_SOURCE_FILES
    ${DSE_FMIMCL_SOURCE_DIR}/engine.c
    ${DSE_FMIMCL_SOURCE_DIR}/fmimcl.c
    ${DSE_FMIMCL_SOURCE_DIR}/parser.c
    ${DSE_FMIMCL_SOURCE_DIR}/adapter/fmi2mcl.c
    ${DSE_FMIMCL_SOURCE_DIR}/../fmimodelc/ascii85.c
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
add_subdirectory(fmu)
add_subdirectory(fmigateway)
