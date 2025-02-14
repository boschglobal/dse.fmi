# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(VERSION "$ENV{PACKAGE_VERSION}")

project(Fmi
    VERSION ${VERSION}
    DESCRIPTION "Dynamic Simulation Environment - FMI Library"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)

include(GNUInstallDirs)
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/_out)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O3")
list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})
add_compile_definitions(DLL_BUILD)


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/..")


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


# External Project - dlfcn-win32
# ------------------------------
if(WIN32)
set(DLFCNWIN32_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32/src")
set(DLFCNWIN32_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32-build/lib")
find_library(DLFCNWIN32_LIB
    NAMES
        libdl.a
    PATHS
        ${DLFCNWIN32_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(dl STATIC IMPORTED GLOBAL)
set_target_properties(dl
    PROPERTIES
        IMPORTED_LOCATION "${DLFCNWIN32_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DLFCNWIN32_SOURCE_DIR}"
)
endif()



# Sub Modules
# ===========
add_subdirectory(fmimcl)
add_subdirectory(fmimodelc)
add_subdirectory(fmigateway)
add_subdirectory(fmu)
add_subdirectory(importer)
add_subdirectory(examples)



# Common Targets
# ==============
install(
    DIRECTORY
        ../licenses/
    DESTINATION
        licenses
)
install(
    FILES
        ${CMAKE_BINARY_DIR}/compile_commands.json
    DESTINATION
        doc
)
