# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODULE "FmiModelc")
set(MODULE_LC "fmimodelc")
project(${MODULE}
    VERSION ${VERSION}
    DESCRIPTION "Dynamic Simulation Environment - FMI ModelC Example"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb -DLIBXML_STATIC")


# Target - Simulation (single Model)
# ===================
add_library(fmimodelctarget SHARED
    model.c
)
target_include_directories(fmimodelctarget
    PRIVATE
        ${REPO_DIR}
        ${dse_modelc_lib_SOURCE_DIR}/include
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


# Install - FMI2 ModelC FMU
# =========================
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


# Install - FMI3 ModelC FMU
# =========================
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
