# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLIBXML_STATIC")
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
FetchContent_Declare(dse_ncodec
    URL                 $ENV{DSE_NCODEC_URL}
    HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
    SOURCE_SUBDIR       dse/ncodec
)
FetchContent_MakeAvailable(dse_ncodec)


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


# Common Targets
# ==============
set(REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi3/headers")
set(CLIB_SOURCE_FILES # TODO: redistribute CLib files?
    ${DSE_CLIB_SOURCE_DIR}/clib/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/ascii85.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/hashmap.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/set.c
)

add_library(fmi2-common OBJECT
    fmu/fmi2fmu.c
    fmu/fmi2variable.c
    fmu/ncodec.c
    fmu/signal.c
    ${CLIB_SOURCE_FILES}
)
target_include_directories(fmi2-common
    PRIVATE
        ${REPO_DIR}
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_libraries(fmi2-common
    PUBLIC
        ab-codec
    PRIVATE
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
        m
)

add_library(fmi3-common OBJECT
    fmu/fmi3fmu.c
    ${CLIB_SOURCE_FILES}
)
target_include_directories(fmi3-common
    PRIVATE
        ${REPO_DIR}
        ${FMI3_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_libraries(fmi3-common
    PUBLIC
    PRIVATE
)


# Sub Modules
# ===========
add_subdirectory(fmu)
add_subdirectory(fmimcl)
add_subdirectory(fmigateway)
add_subdirectory(fmimodelc)
add_subdirectory(importer)
add_subdirectory(examples)


# ModelC DLL
# ==========
install(
    FILES
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin/libmodelc.dll>
    DESTINATION
        bin
)


# Administrative Targets
# ======================
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
