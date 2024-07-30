# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(Model)
set(EX_PATH "${MODULE_LC}/examples/network_fmu")
set(FMU_PATH "${EX_PATH}/fmu")
set(SIM_PATH "${FMU_PATH}/resources/sim")



# Targets
# =======

# FMU Model Runtime
# -----------------
install(
    FILES
        modelDescription.xml
    DESTINATION
        ${FMU_PATH}
)
install(TARGETS
        fmi2modelcfmu
    LIBRARY DESTINATION
        ${FMU_PATH}/lib/$ENV{PACKAGE_ARCH}
)


# Simulation (single Model)
# ----------
add_library(target SHARED
    sim/model.c
)
target_include_directories(target
    PRIVATE
        ${DSE_MODELC_INCLUDE_DIR}
        ../../../..
)
target_link_libraries(target
    PUBLIC
        $<$<BOOL:${WIN32}>:fmi2modelcfmu>
)
install(TARGETS target
    LIBRARY DESTINATION
        ${SIM_PATH}/lib
    RUNTIME DESTINATION
        ${SIM_PATH}/lib
)
install(
    FILES
        sim/model.yaml
        sim/simulation.yaml
    DESTINATION
        ${SIM_PATH}/data
)


# Importer
# --------
add_executable(fmi2importer
    fmi2importer.c
)
set_target_properties(fmi2importer
    PROPERTIES
        OUTPUT_NAME fmi2importer
)
target_include_directories(fmi2importer
    PRIVATE
        ${FMI2_INCLUDE_DIR}
)
target_link_libraries(fmi2importer
    PRIVATE
        dl
        m
        $<$<BOOL:${UNIX}>:mcheck>
)
install(TARGETS fmi2importer RUNTIME
    DESTINATION
        ${EX_PATH}/bin
)
