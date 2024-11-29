# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "Counter")
set(MODULE_LC "counter")

project(Fmu VERSION ${VERSION})

include(FetchContent)

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb -DLIBXML_STATIC")
list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL https://github.com/boschglobal/dse.clib/archive/refs/tags/v$ENV{DSE_CLIB_VERSION}.zip
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/..")
set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/clib/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/hashmap.c
)
set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi3/headers")


set(FMU_SOURCE_DIR "../../../fmu")
set(FMU_INCLUDE_DIR "../../../..")
set(FMI_2_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi2fmu.c
    ${FMU_SOURCE_DIR}/signal.c
)
set(FMI_3_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi3fmu.c
    ${FMU_SOURCE_DIR}/signal.c
)


# Targets
# =======

# FMI2 FMU
# --------
add_library(fmu2counter SHARED
    ${DSE_CLIB_SOURCE_FILES}
    ${FMI_2_COMMON_FILES}
    fmu.c
)
target_include_directories(fmu2counter
    PRIVATE
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_libraries(fmu2counter
    PRIVATE
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
)
install(
    TARGETS
        fmu2counter
    LIBRARY DESTINATION
        examples/fmu/counter
)

# FMI3 FMU
# --------
add_library(fmu3counter SHARED
    ${DSE_CLIB_SOURCE_FILES}
    ${FMI_3_COMMON_FILES}
    fmu.c
)
target_include_directories(fmu3counter
    PRIVATE
        ${FMI3_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_libraries(fmu3counter
    PRIVATE
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
)
install(
    TARGETS
        fmu3counter
    LIBRARY DESTINATION
        examples/fmu/counter
)
