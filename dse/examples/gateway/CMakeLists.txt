# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "Gateway")
set(MODULE_LC "gateway")

project(fmigateway VERSION ${VERSION})

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
set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi3/headers")


# External Project - DSE ModelC LIB
# ---------------------------------
FetchContent_Declare(dse_modelc_lib
    URL https://github.com/boschglobal/dse.modelc/releases/download/v$ENV{DSE_MODELC_VERSION}/ModelC-$ENV{DSE_MODELC_VERSION}-$ENV{PACKAGE_ARCH}.zip
)
FetchContent_MakeAvailable(dse_modelc_lib)
set(DSE_MODELC_INCLUDE_DIR "${dse_modelc_lib_SOURCE_DIR}/include")


set(FMU_SOURCE_DIR "../../fmu")
set(FMU_INCLUDE_DIR "../../..")
set(FMI_2_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi2fmu.c
    ${FMU_SOURCE_DIR}/fmi2variable.c
    ${FMU_SOURCE_DIR}/signal.c
)
set(FMI_3_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi3fmu.c
    ${FMU_SOURCE_DIR}/fmi3variable.c
    ${FMU_SOURCE_DIR}/signal.c
)
set(FMIGATEWAY_FILES "../../fmigateway")
set(FMIGATEWAY_FILES
    ${FMIGATEWAY_FILES}/fmigateway.c
    ${FMIGATEWAY_FILES}/index.c
    ${FMIGATEWAY_FILES}/session.c
    ${FMIGATEWAY_FILES}/parser.c
    $<$<BOOL:${WIN32}>:${FMIGATEWAY_FILES}/session_win32.c>
    $<$<BOOL:${UNIX}>:${FMIGATEWAY_FILES}/session_unix.c>
)

# Targets
# =======

# Simulation (single Model)
# ----------
add_library(gatewaytarget SHARED
    model.c
)
target_include_directories(gatewaytarget
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_directories(gatewaytarget
    PRIVATE
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(gatewaytarget
    PRIVATE
        $<$<BOOL:${WIN32}>:modelc>
)
install(TARGETS gatewaytarget
    LIBRARY DESTINATION
        examples/gateway/model
)

# FMI2 Gateway FMU
# ---------------
add_library(gatewayfmi2fmu SHARED
    ${FMI_2_COMMON_FILES}
    ${FMIGATEWAY_FILES}
)
target_include_directories(gatewayfmi2fmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_directories(gatewayfmi2fmu
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_compile_definitions(gatewayfmi2fmu
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(gatewayfmi2fmu
    PRIVATE
        yaml
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${UNIX}>:-Wl,--whole-archive,--export-dynamic>
        $<$<BOOL:${UNIX}>:modelc_bundled>
        $<$<BOOL:${UNIX}>:-Wl,--no-whole-archive>
        $<$<BOOL:${UNIX}>:rt>
        $<$<BOOL:${WIN32}>:modelc>
        $<$<BOOL:${WIN32}>:dl>

)
set_target_properties(gatewayfmi2fmu
    PROPERTIES
        OUTPUT_NAME
            fmi2gateway
        PREFIX ""
)
install(
    TARGETS
        gatewayfmi2fmu
    LIBRARY DESTINATION
        examples/gateway/fmi2/binaries/linux64
)
install(
    FILES
        fmi2/modelDescription.xml
    DESTINATION
        examples/gateway/fmi2
)
install(
    FILES
        data/fmu.yaml
        data/model.yaml
        data/stack.yaml
    DESTINATION
        examples/gateway/fmi2/resources
)

# FMI3 Gateway FMU
# ---------------
add_library(gatewayfmi3fmu SHARED
    ${FMI_3_COMMON_FILES}
    ${FMIGATEWAY_FILES}
)
target_include_directories(gatewayfmi3fmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${FMI3_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_directories(gatewayfmi3fmu
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_compile_definitions(gatewayfmi3fmu
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(gatewayfmi3fmu
    PRIVATE
        yaml
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${UNIX}>:-Wl,--whole-archive,--export-dynamic>
        $<$<BOOL:${UNIX}>:modelc_bundled>
        $<$<BOOL:${UNIX}>:-Wl,--no-whole-archive>
        $<$<BOOL:${UNIX}>:rt>
        $<$<BOOL:${WIN32}>:modelc>
        $<$<BOOL:${WIN32}>:dl>

)
set_target_properties(gatewayfmi3fmu
    PROPERTIES
        OUTPUT_NAME
            fmi3gateway
        PREFIX ""
)
install(
    TARGETS
        gatewayfmi3fmu
    LIBRARY DESTINATION
        examples/gateway/fmi3/binaries/x86_64-linux
)
install(
    FILES
        fmi3/modelDescription.xml
    DESTINATION
        examples/gateway/fmi3
)
install(
    FILES
        data/fmu.yaml
        data/model.yaml
        data/stack.yaml
    DESTINATION
        examples/gateway/fmi3/resources
)
