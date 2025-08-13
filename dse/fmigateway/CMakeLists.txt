# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "FmiGateway")
set(MODULE_LC "fmigateway")

project(${MODULE}
    VERSION ${VERSION}
    DESCRIPTION "DSE FMI ModelC Gateway"
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

set(FMIGATEWAY_FILES
    fmigateway.c
    index.c
    session.c
    parser.c
    signal.c
    $<$<BOOL:${WIN32}>:session_win32.c>
    $<$<BOOL:${UNIX}>:session_unix.c>
)

# Targets
# =======

# FMI2 Gateway FMU
# ---------------
add_library(fmi2gateway SHARED
    ${FMI_2_COMMON_FILES}
    ${FMIGATEWAY_FILES}
)
target_include_directories(fmi2gateway
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${FMI2_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_directories(fmi2gateway
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_compile_definitions(fmi2gateway
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(fmi2gateway
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
        fmi2gateway
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
    RUNTIME DESTINATION
        ${MODULE_LC}/lib
)

# FMI3 Gateway FMU
# ---------------
add_library(fmi3gateway SHARED
    ${FMI_3_COMMON_FILES}
    ${FMIGATEWAY_FILES}
)
target_include_directories(fmi3gateway
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${FMI3_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_directories(fmi3gateway
    PRIVATE
        $<$<BOOL:${UNIX}>:${dse_modelc_lib_SOURCE_DIR}/lib>
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_compile_definitions(fmi3gateway
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(fmi3gateway
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
        fmi3gateway
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
