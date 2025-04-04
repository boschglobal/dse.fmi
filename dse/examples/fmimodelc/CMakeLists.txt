# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "FmiModelc")
set(MODULE_LC "fmimodelc")

project(fmimodelc VERSION ${VERSION})

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
)
set(FMI_3_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi3fmu.c
)
set(FMIMODELC_SOURCE_DIR "../../fmimodelc")
set(FMIMODELC_FILES
    ${FMIMODELC_SOURCE_DIR}/fmimodelc.c
    ${FMIMODELC_SOURCE_DIR}/runtime.c
    ${FMIMODELC_SOURCE_DIR}/signal.c
)

# Targets
# =======

# Simulation (single Model)
# ----------
add_library(fmimodelctarget SHARED
    model.c
)
target_include_directories(fmimodelctarget
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_directories(fmimodelctarget
    PRIVATE
        ${dse_modelc_lib_SOURCE_DIR}/lib
)
target_link_libraries(fmimodelctarget
    PRIVATE
        modelc
        yaml
)


# FMI2 ModelC FMU
# ---------------
if(UNIX)
install(
    TARGETS
        fmi2modelcfmu
    LIBRARY DESTINATION
        examples/fmimodelc/fmi2/binaries/linux64
    RUNTIME DESTINATION
        examples/fmimodelc/fmi2/binaries/linux64
)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/lib/libmodelc.so
    DESTINATION
        examples/fmimodelc/fmi2/binaries/linux64
)
elseif(WIN32)
install(
    TARGETS
        fmi2modelcfmu
    LIBRARY DESTINATION
        examples/fmimodelc/fmi2/binaries/win64
    RUNTIME DESTINATION
        examples/fmimodelc/fmi2/binaries/win64
)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/bin/libmodelc.dll
    DESTINATION
        examples/fmimodelc/fmi2/binaries/win64
)
endif()

install(
    FILES
        fmi2/modelDescription.xml
    DESTINATION
        examples/fmimodelc/fmi2
)
install(
    FILES
        data/model.yaml
        data/simulation.yaml
    DESTINATION
        examples/fmimodelc/fmi2/resources/sim/data
)
install(
    TARGETS
        fmimodelctarget
    LIBRARY DESTINATION
        examples/fmimodelc/fmi2/resources/sim/lib
    RUNTIME DESTINATION
        examples/fmimodelc/fmi2/resources/sim/lib
)

# FMI3 ModelC FMU
# ---------------
if(UNIX)
install(
    TARGETS
        fmi3modelcfmu
    LIBRARY DESTINATION
        examples/fmimodelc/fmi3/binaries/x86_64-linux
    RUNTIME DESTINATION
        examples/fmimodelc/fmi3/binaries/x86_64-linux
)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/lib/libmodelc.so
    DESTINATION
        examples/fmimodelc/fmi3/binaries/x86_64-linux
)
elseif(WIN32)
install(
    TARGETS
        fmi2modelcfmu
    LIBRARY DESTINATION
        examples/fmimodelc/fmi3/binaries/x86_64-windows
    RUNTIME DESTINATION
        examples/fmimodelc/fmi3/binaries/x86_64-windows
)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/bin/libmodelc.dll
    DESTINATION
        examples/fmimodelc/fmi3/binaries/x86_64-windows
)
endif()

install(
    FILES
        fmi3/modelDescription.xml
    DESTINATION
        examples/fmimodelc/fmi3
)
install(
    FILES
        data/model.yaml
        data/simulation.yaml
    DESTINATION
        examples/fmimodelc/fmi3/resources/sim/data
)
install(
    TARGETS
        fmimodelctarget
    LIBRARY DESTINATION
        examples/fmimodelc/fmi3/resources/sim/lib
    RUNTIME DESTINATION
        examples/fmimodelc/fmi3/resources/sim/lib
)
