# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(cmocka)

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


set(REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")


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
set(XML_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/libxml2")
set(XML_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/libxml2")
set(XML_INCLUDE_DIR "$ENV{EXTERNAL_BUILD_DIR}/libxml2/install/include/libxml2")
find_library(XML_LIB
    NAMES
        libxml2.a
    PATHS
        ${XML_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(xml STATIC IMPORTED GLOBAL)
add_dependencies(xml libxml2)
set_target_properties(xml
    PROPERTIES
        IMPORTED_LOCATION "${XML_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${XML_INCLUDE_DIR}"
)


# External Project - msgpackc
# ---------------------------
set(MSGPACKC_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/msgpackc")
set(MSGPACKC_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/msgpackc-build")
find_library(MSGPACKC_LIB
    NAMES
        libmsgpackc.a
    PATHS
        ${MSGPACKC_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(msgpackc STATIC IMPORTED GLOBAL)
set_target_properties(msgpackc
    PROPERTIES
        IMPORTED_LOCATION "${MSGPACKC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MSGPACKC_BINARY_DIR}"
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
set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/fmi/fmi2/headers")
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/fmi/fmi3/headers")


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
set(DSE_MODELC_SOURCE_INCLUDE_DIR "${dse_modelc_SOURCE_DIR}")


# External Project - DSE ModelC LIB
# ---------------------------------
FetchContent_Declare(dse_modelc_lib
    URL                 $ENV{DSE_MODELC_LIB_URL}
    HTTP_USERNAME       $ENV{DSE_MODELC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_MODELC_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib"
)
FetchContent_MakeAvailable(dse_modelc_lib)
set(DSE_MODELC_INCLUDE_DIR "${dse_modelc_lib_SOURCE_DIR}/include")


# External Project - DSE Network Codec
# ------------------------------------
FetchContent_Declare(dse_ncodec
    URL                 $ENV{DSE_NCODEC_URL}
    HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
    SOURCE_SUBDIR       dse/ncodec
)
FetchContent_MakeAvailable(dse_ncodec)


# Common Targets
# ==============
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


# Targets
# =======
add_subdirectory(fmu)
add_subdirectory(fmimcl)
add_subdirectory(fmimodelc)
add_subdirectory(fmigateway)
