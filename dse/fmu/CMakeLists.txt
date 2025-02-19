# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "FMU")
set(MODULE_LC "fmu")

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


set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/clib/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/hashmap.c
)
set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi3/headers")
set(FMU_INCLUDE_DIR "../..")
set(FMI_2_COMMON_FILES
    ascii85.c
    fmi2fmu.c
    fmi2variable.c
    signal.c
)
set(FMI_3_COMMON_FILES
    ascii85.c
    fmi3fmu.c
    fmi3variable.c
    signal.c
)


# Targets
# =======

# Target - flatcc_runtime
# -----------------------
add_library(flatcc_runtime OBJECT
    ${FLATCC_SOURCE_DIR}/src/builder.c
    ${FLATCC_SOURCE_DIR}/src/emitter.c
    ${FLATCC_SOURCE_DIR}/src/refmap.c
)
target_include_directories(flatcc_runtime
    PRIVATE
        ${FLATCC_INCLUDE_DIR}
)


# Target - network_codec
# ----------------------
add_library(network_codec OBJECT
    ncodec.c
    ${DSE_NCODEC_SOURCE_DIR}/codec.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/codec.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/frame_can_fbs.c
    ${DSE_NCODEC_SOURCE_DIR}/libs/automotive-bus/pdu_fbs.c
)
target_include_directories(network_codec
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}/dse/ncodec/libs
        ${FMU_INCLUDE_DIR}
        ${FLATCC_INCLUDE_DIR}
        ${AUTOMOTIVE_BUS_SCHEMA_SOURCE_DIR}
        ../../..
)
target_link_libraries(network_codec
    PRIVATE
        flatcc_runtime
)


# Target - FMI2 FMU (for testing)
# -----------------
add_library(fmi2fmu SHARED
    ${DSE_CLIB_SOURCE_FILES}
    ${FMI_2_COMMON_FILES}
    fmu.c
)
target_include_directories(fmi2fmu
    PRIVATE
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_libraries(fmi2fmu
    PRIVATE
        network_codec
        flatcc_runtime
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
        m
)
set_target_properties(fmi2fmu
    PROPERTIES
        PREFIX ""
)
install(
    TARGETS
        fmi2fmu
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
)
