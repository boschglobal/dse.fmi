# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODULE "FmiModelC")
set(MODULE_LC "fmimodelc")
project(${MODULE}
    VERSION "$ENV{PACKAGE_VERSION}"
    DESCRIPTION "Dynamic Simulation Environment - FMI ModelC FMU"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb -DLIBXML_STATIC")


# FMI ModelC Files
# ================
set(FMIMODELC_FILES
    fmimodelc.c
    runtime.c
    signal.c
    $<$<BOOL:${WIN32}>:env_win32.c>
    $<$<BOOL:${UNIX}>:env_unix.c>
    ${DSE_CLIB_SOURCE_DIR}/clib/util/ascii85.c
)


# Target - FMI2 ModelC FMU
# ========================
set(FMI2_MODELC fmi2modelcfmu)
add_library(${FMI2_MODELC} SHARED
    ${REPO_DIR}/dse/fmu/fmi2fmu.c
    ${FMIMODELC_FILES}
)
target_include_directories(${FMI2_MODELC}
    PRIVATE
        ${REPO_DIR}
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
)
set_target_properties(${FMI2_MODELC} PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
target_link_directories(${FMI2_MODELC}
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(${FMI2_MODELC}
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
        ${FMI2_MODELC}
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
    RUNTIME DESTINATION
        ${MODULE_LC}/lib
)


# Target - FMI3 ModelC FMU
# ========================
set(FMI3_MODELC fmi3modelcfmu)
add_library(${FMI3_MODELC} SHARED
    ${REPO_DIR}/dse/fmu/fmi3fmu.c
    ${FMIMODELC_FILES}
)
target_include_directories(${FMI3_MODELC}
    PRIVATE
        ${REPO_DIR}
        ${FMI3_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
)
set_target_properties(${FMI3_MODELC} PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
target_link_directories(${FMI3_MODELC}
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(${FMI3_MODELC}
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
        ${FMI3_MODELC}
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
    RUNTIME DESTINATION
        ${MODULE_LC}/lib
)


# Install the ModelC library
# ==========================
if (UNIX)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/lib/libmodelc.so
    DESTINATION
        ${MODULE_LC}/lib
)
elseif(WIN32)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/bin/libmodelc.dll
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
