# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "FmiModelC")
set(MODULE_LC "fmimodelc")

project(${MODULE}
    VERSION ${VERSION}
    DESCRIPTION "DSE FMI ModelC FMU"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)

set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")



# Targets
# =======

# FMI2 ModelC FMU
# ---------------
add_library(fmi2modelcfmu SHARED
    ascii85.c
    fmi2fmu.c
    runtime.c
    signal_api_backport.c  # Backport of ModelC 2.1.0 api.
    $<$<BOOL:${WIN32}>:clock_gettime.c>
)
target_include_directories(fmi2modelcfmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
        ../..
)
target_link_directories(fmi2modelcfmu
    PRIVATE
        ${dse_modelc_lib_SOURCE_DIR}/lib
)
target_link_libraries(fmi2modelcfmu
    PRIVATE
        -Wl,--whole-archive
        model_runtime
        -Wl,--no-whole-archive
        yaml
        $<$<BOOL:${WIN32}>:dl>
)
install(
    TARGETS
        fmi2modelcfmu
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
)



# Sub Projects
# ============

add_subdirectory(examples)



# Package
# =======
set(CPACK_SYSTEM_NAME $ENV{PACKAGE_ARCH})
set(CPACK_PACKAGE_VENDOR "Robert Bosch GmbH")
set(CPACK_PACKAGE_DESCRIPTION "DSE FMI ModelC FMU")
set(CPACK_OUTPUT_FILE_PREFIX _dist)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_CHECKSUM MD5)
set(CPACK_MONOLITHIC_INSTALL TRUE)
include(CPack)
