# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)
if(TARGET dse_external_xml2)
    return()
endif()

include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/dse_external.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()

set(DSE_EXTERNAL_XML2_PREFIX "${CMAKE_BINARY_DIR}/_deps/dse_external_xml2")
set(DSE_EXTERNAL_XML2_INSTALL_DIR "${DSE_EXTERNAL_XML2_PREFIX}/install")
set(DSE_EXTERNAL_XML2_LIB "${DSE_EXTERNAL_XML2_INSTALL_DIR}/lib/libxml2.a")
set(DSE_EXTERNAL_XML2_INCLUDE_DIR "${DSE_EXTERNAL_XML2_INSTALL_DIR}/include/libxml2")

dse_external_download(xml2 "${ExternalProject__XML2__URL}"
    "${ExternalProject__XML2__USERNAME}" "${ExternalProject__XML2__PASSWORD}" DSE_EXTERNAL_XML2_ARCHIVE)
ExternalProject_Add(dse_external_xml2_build
    URL                 "${DSE_EXTERNAL_XML2_ARCHIVE}"
    URL_HASH            ${DSE_EXTERNAL_XML2_ARCHIVE_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX              "${DSE_EXTERNAL_XML2_PREFIX}"
    INSTALL_DIR         "${DSE_EXTERNAL_XML2_INSTALL_DIR}"
    EXCLUDE_FROM_ALL    TRUE
    CMAKE_ARGS
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -Wno-dev
        -Wno-deprecated
        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR=lib
        -DBUILD_SHARED_LIBS=OFF
        -DLIBXML2_WITH_HTML=OFF
        -DLIBXML2_WITH_HTTP=OFF
        -DLIBXML2_WITH_ICONV=OFF
        -DLIBXML2_WITH_LZMA=OFF
        -DLIBXML2_WITH_PYTHON=OFF
        -DLIBXML2_WITH_SCHEMATRON=OFF
        -DLIBXML2_WITH_TESTS=OFF
        -DLIBXML2_WITH_THREAD_ALLOC=OFF
        -DLIBXML2_WITH_THREADS=OFF
        -DLIBXML2_WITH_TREE=OFF
        -DLIBXML2_WITH_VALID=OFF
        -DLIBXML2_WITH_WRITER=OFF
        -DLIBXML2_WITH_ZLIB=OFF
    BUILD_BYPRODUCTS    "${DSE_EXTERNAL_XML2_LIB}"
)

# Imported targets require their include directories to exist at generate time.
file(MAKE_DIRECTORY "${DSE_EXTERNAL_XML2_INCLUDE_DIR}")
add_library(dse_external_xml2 STATIC IMPORTED GLOBAL)
set_target_properties(dse_external_xml2
    PROPERTIES
        IMPORTED_LOCATION "${DSE_EXTERNAL_XML2_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_EXTERNAL_XML2_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS LIBXML_STATIC
)
add_dependencies(dse_external_xml2 dse_external_xml2_build)
