# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(EXAMPLE_PATH "examples/gateway")
project(examplefmigateway)


# Target - Simulation w. Single Model
# ===================================
set(GW_TARGET gatewaytarget)
add_library(${GW_TARGET} SHARED
    model.c
)
target_include_directories(${GW_TARGET}
    PRIVATE
        ${REPO_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_directories(${GW_TARGET}
    PRIVATE
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(${GW_TARGET}
    PRIVATE
        $<$<BOOL:${WIN32}>:modelc>
)
install(TARGETS ${GW_TARGET}
    LIBRARY DESTINATION
        ${EXAMPLE_PATH}/model
)


# Example - FMI2 Gateway FMU
# ==========================
set(FMI2_GW fmi2gateway)
install(
    TARGETS
        ${FMI2_GW}
    LIBRARY DESTINATION
        ${EXAMPLE_PATH}/fmi2/binaries/linux64
)
install(
    FILES
        fmi2/modelDescription.xml
    DESTINATION
        ${EXAMPLE_PATH}/fmi2
)
install(
    FILES
        data/fmu.yaml
        data/model.yaml
        data/stack.yaml
        data/stack_target.yaml
        data/signalgroup_target.yaml
    DESTINATION
        ${EXAMPLE_PATH}/fmi2/resources
)


# Example - FMI3 Gateway FMU
# ==========================
set(FMI3_GW fmi3gateway)
install(
    TARGETS
        ${FMI3_GW}
    LIBRARY DESTINATION
        ${EXAMPLE_PATH}/fmi3/binaries/x86_64-linux
)
install(
    FILES
        fmi3/modelDescription.xml
    DESTINATION
        ${EXAMPLE_PATH}/fmi3
)
install(
    FILES
        data/fmu.yaml
        data/model.yaml
        data/stack.yaml
        data/stack_target.yaml
        data/signalgroup_target.yaml
    DESTINATION
        ${EXAMPLE_PATH}/fmi3/resources
)
