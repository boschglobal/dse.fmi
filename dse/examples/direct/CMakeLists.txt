# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODULE "DirectIndex")
set(MODULE_LC "direct")
set(EXAMPLE_DIR examples/${MODULE_LC})
project(${MODULE}
    VERSION ${VERSION}
    DESCRIPTION "Dynamic Simulation Environment - FMI ModelC Direct Index Example"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb -DLIBXML_STATIC")


# Target - Simer Layout
# =====================
add_library(${MODULE_LC} SHARED
    model.c
)
target_include_directories(${MODULE_LC}
    PRIVATE
        ${REPO_DIR}
        ${dse_modelc_lib_SOURCE_DIR}/include
)
target_link_directories(${MODULE_LC}
    PRIVATE
        ${dse_modelc_lib_SOURCE_DIR}/lib
)
target_link_libraries(${MODULE_LC}
    PRIVATE
        modelc
        yaml
)
install(
    TARGETS
        ${MODULE_LC}
    LIBRARY DESTINATION
        ${EXAMPLE_DIR}/sim/model/${MODULE_LC}/lib
    RUNTIME DESTINATION
        ${EXAMPLE_DIR}/sim/model/${MODULE_LC}/lib
)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/lib/libmodelc.so
    DESTINATION
        ${EXAMPLE_DIR}/sim/model/${MODULE_LC}/lib
)
install(
    FILES
        model.yaml
        signalgroup.yaml
        in_values.csv
    DESTINATION
        ${EXAMPLE_DIR}/sim/model/${MODULE_LC}/data
)
install(
    FILES
        simulation.yaml
    DESTINATION
        ${EXAMPLE_DIR}/sim/data
)
