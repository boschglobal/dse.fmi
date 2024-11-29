# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "Importer")
set(MODULE_LC "importer")

project(${MODULE}
    VERSION ${VERSION}
    DESCRIPTION "FMU Importer for fmi2 and fmi3"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)

include(ExternalProject)

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

set(DSE_CLIB_SOURCE_FILES
    ${dse_clib_SOURCE_DIR}/dse/clib/collections/hashmap.c
)


# Targets
# =======

# Importer
# --------

add_executable(importer
    ${DSE_CLIB_SOURCE_FILES}
    importer.c
    xml.c
)
set_target_properties(importer
    PROPERTIES
        OUTPUT_NAME fmuImporter
)
target_include_directories(importer
    PRIVATE
        ${FMI2_INCLUDE_DIR}
        ${FMI3_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ../..
)
target_link_libraries(importer
    PRIVATE
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${UNIX}>:mcheck>
        dl
        m
)
install(TARGETS importer RUNTIME
    DESTINATION
        ${MODULE_LC}
)


# Package
# =======
set(CPACK_SYSTEM_NAME $ENV{PACKAGE_ARCH})
set(CPACK_PACKAGE_VENDOR "Robert Bosch GmbH")
set(CPACK_PACKAGE_DESCRIPTION "DSE FMI Importer")
set(CPACK_OUTPUT_FILE_PREFIX _dist)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_CHECKSUM MD5)
set(CPACK_MONOLITHIC_INSTALL TRUE)
include(CPack)
