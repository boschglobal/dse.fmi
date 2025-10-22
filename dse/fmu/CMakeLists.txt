# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODULE "FMU")
set(MODULE_LC "fmu")
project(${MODULE}
    VERSION "$ENV{PACKAGE_VERSION}"
    DESCRIPTION "Dynamic Simulation Environment - FMU Library"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)


# Target - FMI2 FMU (for testing, no package)
# =================
set(FMU2_TARGET fmi2fmu)
add_library(${FMU2_TARGET} SHARED
    fmu.c
)
target_include_directories(${FMU2_TARGET}
    PRIVATE
        ${REPO_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
)
target_link_libraries(${FMU2_TARGET}
    PUBLIC
        ab-codec
    PRIVATE
        fmi2-common
)
set_target_properties(${FMU2_TARGET}
    PROPERTIES
        PREFIX ""
)
install(
    TARGETS
        ${FMU2_TARGET}
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
    RUNTIME DESTINATION
        ${MODULE_LC}/lib
)
