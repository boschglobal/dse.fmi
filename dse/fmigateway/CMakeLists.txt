# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODULE "FmiGateway")
set(MODULE_LC "fmigateway")
project(${MODULE}
    VERSION "$ENV{PACKAGE_VERSION}"
    DESCRIPTION "Dynamic Simulation Environment - FMI Gateway"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb -DLIBXML_STATIC")


# Gateway Files
# =============
set(FMIGATEWAY_FILES
    fmigateway.c
    index.c
    session.c
    parser.c
    signal.c
    $<$<BOOL:${WIN32}>:session_win32.c>
    $<$<BOOL:${UNIX}>:session_unix.c>
    ${DSE_CLIB_SOURCE_DIR}/clib/util/ascii85.c
)


# Target - FMI2 Gateway FMU
# =========================
set(FMI2_GW fmi2gateway)
add_library(${FMI2_GW} SHARED
    ${REPO_DIR}/dse/fmu/fmi2fmu.c
    ${FMIGATEWAY_FILES}
)
target_include_directories(${FMI2_GW}
    PRIVATE
        ${REPO_DIR}
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
)
target_link_directories(${FMI2_GW}
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_compile_definitions(${FMI2_GW}
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(${FMI2_GW}
    PRIVATE
        yaml
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${UNIX}>:-Wl,--whole-archive,--export-dynamic>
        $<$<BOOL:${UNIX}>:modelc_bundled>
        $<$<BOOL:${UNIX}>:-Wl,--no-whole-archive>
        $<$<BOOL:${UNIX}>:rt>
        $<$<BOOL:${WIN32}>:modelc>
        $<$<BOOL:${WIN32}>:dl>
)
install(
    TARGETS
        ${FMI2_GW}
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
    RUNTIME DESTINATION
        ${MODULE_LC}/lib
)


# Target - FMI3 Gateway FMU
# =========================
set(FMI3_GW fmi3gateway)
add_library(${FMI3_GW} SHARED
    ${REPO_DIR}/dse/fmu/fmi3fmu.c
    ${FMIGATEWAY_FILES}
)
target_include_directories(${FMI3_GW}
    PRIVATE
        ${REPO_DIR}
        ${FMI3_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
)
target_link_directories(${FMI3_GW}
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_compile_definitions(${FMI3_GW}
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(${FMI3_GW}
    PRIVATE
        yaml
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${UNIX}>:-Wl,--whole-archive,--export-dynamic>
        $<$<BOOL:${UNIX}>:modelc_bundled>
        $<$<BOOL:${UNIX}>:-Wl,--no-whole-archive>
        $<$<BOOL:${UNIX}>:rt>
        $<$<BOOL:${WIN32}>:modelc>
        $<$<BOOL:${WIN32}>:dl>
)
install(
    TARGETS
        ${FMI3_GW}
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
    RUNTIME DESTINATION
        ${MODULE_LC}/lib
)


# Package
# =======
set(CPACK_SYSTEM_NAME $ENV{PACKAGE_ARCH})
set(CPACK_PACKAGE_VENDOR "Robert Bosch GmbH")
set(CPACK_PACKAGE_DESCRIPTION "DSE FMI Gateway")
set(CPACK_OUTPUT_FILE_PREFIX _dist)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_CHECKSUM MD5)
set(CPACK_MONOLITHIC_INSTALL TRUE)
include(CPack)
