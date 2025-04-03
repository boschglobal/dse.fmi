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


set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi3/headers")
set(FMU_SOURCE_DIR "../fmu")
set(FMU_INCLUDE_DIR "../..")
set(FMI_2_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi2fmu.c
)
set(FMI_3_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi3fmu.c
)

set(FMIMODELC_FILES
    fmimodelc.c
    runtime.c
    signal.c
)


# Targets
# =======

# FMI2 ModelC FMU
# ---------------
add_library(fmi2modelcfmu SHARED
    ${FMI_2_COMMON_FILES}
    ${FMIMODELC_FILES}
)
target_include_directories(fmi2modelcfmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
set_target_properties(fmi2modelcfmu PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
target_link_directories(fmi2modelcfmu
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(fmi2modelcfmu
    PRIVATE
        modelc
        xml
        yaml
        m
        $<$<BOOL:${WIN32}>:bcrypt>
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
    ${FMIMODELC_FILES}
)
target_include_directories(fmi3modelcfmu
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${FMI3_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
set_target_properties(fmi3modelcfmu PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
target_link_directories(fmi3modelcfmu
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(fmi3modelcfmu
    PRIVATE
        modelc
        yaml
        xml
        m
        $<$<BOOL:${WIN32}>:bcrypt>
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

if (UNIX)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/lib/libmodelc.so
    DESTINATION
        ${MODULE_LC}/lib
)
endif()


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
