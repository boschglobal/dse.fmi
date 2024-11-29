# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmu)

include(ExternalProject)


# FMU Code
# ===============
set(DSE_FMU_SOURCE_DIR ../../../dse/fmu)
set(DSE_FMU_SOURCE_FILES
    ${DSE_FMU_SOURCE_DIR}/ascii85.c
    ${DSE_FMU_SOURCE_DIR}/signal.c
)
set(DSE_FMU_INCLUDE_DIR "${DSE_FMU_SOURCE_DIR}/../..")

set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/collections/hashmap.c
)

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


# Targets
# =======

# Target - FMI ModelC
# -------------------
add_executable(test_fmu
    __test__.c
    test_ascii85.c
    test_signals.c
    ${DSE_FMU_SOURCE_FILES}
    ${DSE_CLIB_SOURCE_FILES}
)
target_include_directories(test_fmu
    PRIVATE
        ${DSE_FMU_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ./
)
target_compile_definitions(test_fmu
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_directories(test_fmu
    PRIVATE
        ${MODELC_BINARY_DIR}/lib
)
target_link_libraries(test_fmu
    PUBLIC
    PRIVATE
        xml
        cmocka
        dl
        m
)
install(TARGETS test_fmu)
install(
    FILES
        data/modelDescription.xml
    DESTINATION
        data/test_fmu
)
install(
    FILES
        data/modelDescription.xml
    DESTINATION
        data/test_fmu/resources
)
