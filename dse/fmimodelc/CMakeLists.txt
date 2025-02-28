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
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi3/headers")
set(FMU_SOURCE_DIR "../fmu")
set(FMU_INCLUDE_DIR "../..")
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



# Targets
# =======

# FMI2 ModelC FMU
# ---------------
add_library(fmi2modelcfmu SHARED
${FMI_2_COMMON_FILES}
    runtime.c
    fmimodelc.c
)
target_include_directories(fmi2modelcfmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
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
        xml
        m
        $<$<BOOL:${WIN32}>:dl>
        $<$<BOOL:${WIN32}>:-Wl,-Bstatic>
        $<$<BOOL:${WIN32}>:pthread>
        $<$<BOOL:${WIN32}>:-Wl,-Bdynamic>
)
install(
TARGETS
    fmi2modelcfmu
LIBRARY DESTINATION
    ${MODULE_LC}/lib
)

# FMI3 ModelC FMU
# ---------------
add_library(fmi3modelcfmu SHARED
    ${FMI_3_COMMON_FILES}
    runtime.c
    fmimodelc.c
)
target_include_directories(fmi3modelcfmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${FMI3_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_directories(fmi3modelcfmu
    PRIVATE
        ${dse_modelc_lib_SOURCE_DIR}/lib
)
target_link_libraries(fmi3modelcfmu
    PRIVATE
        -Wl,--whole-archive
        model_runtime
        -Wl,--no-whole-archive
        yaml
        xml
        m
        $<$<BOOL:${WIN32}>:dl>
        $<$<BOOL:${WIN32}>:-Wl,-Bstatic>
        $<$<BOOL:${WIN32}>:pthread>
        $<$<BOOL:${WIN32}>:-Wl,-Bdynamic>
)
install(
    TARGETS
        fmi3modelcfmu
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
)



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
