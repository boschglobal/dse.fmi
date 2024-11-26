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


# External Project - xml
# ----------------------
ExternalProject_Add(libxml2
    URL https://github.com/GNOME/libxml2/archive/refs/tags/v2.13.5.zip
    CMAKE_ARGS
        -D CMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/libxml2
        -D CMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -D BUILD_SHARED_LIBS=OFF
        -D LIBXML2_WITH_HTML=OFF
        -D LIBXML2_WITH_HTTP=OFF
        -D LIBXML2_WITH_ICONV=OFF
        -D LIBXML2_WITH_LZMA=OFF
        -D LIBXML2_WITH_PYTHON=OFF
        -D LIBXML2_WITH_SCHEMATRON=OFF
        -D LIBXML2_WITH_TESTS=OFF
        -D LIBXML2_WITH_THREAD_ALLOC=OFF
        -D LIBXML2_WITH_THREADS=OFF
        -D LIBXML2_WITH_TREE=OFF
        -D LIBXML2_WITH_VALID=OFF
        -D LIBXML2_WITH_WRITER=OFF
        -D LIBXML2_WITH_ZLIB=OFF
    INSTALL_DIR "${CMAKE_BINARY_DIR}/libxml2"
    UPDATE_COMMAND ""
)
ExternalProject_Get_property(libxml2 SOURCE_DIR BINARY_DIR INSTALL_DIR)
set(XML_SOURCE_DIR "${SOURCE_DIR}")
set(XML_BINARY_DIR "${BINARY_DIR}")
set(XML_INSTALL_DIR "${INSTALL_DIR}")

add_library(xml STATIC IMPORTED GLOBAL)
add_dependencies(xml libxml2)
file(MAKE_DIRECTORY "${XML_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${XML_INSTALL_DIR}/include/libxml2")
set_target_properties(xml
    PROPERTIES
        IMPORTED_LOCATION "${XML_BINARY_DIR}/libxml2.a"
        INTERFACE_INCLUDE_DIRECTORIES "${XML_INSTALL_DIR}/include/libxml2"
)

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
