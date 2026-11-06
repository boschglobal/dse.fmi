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


# External Projects
# =================
set(DSE_FMI_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../extra/external)
include(${DSE_FMI_EXTERNAL_DIR}/yaml.cmake)
include(${DSE_FMI_EXTERNAL_DIR}/xml2.cmake)
include(${DSE_FMI_EXTERNAL_DIR}/lua.cmake)
include(${DSE_FMI_EXTERNAL_DIR}/dlfcnwin32.cmake)


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
    SOURCE_SUBDIR       dse
)
FetchContent_MakeAvailable(dse_ncodec)


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
        dse_external_xml2
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dse_external_dl>
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
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib/libmodelc.so>
    DESTINATION
        lib
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
