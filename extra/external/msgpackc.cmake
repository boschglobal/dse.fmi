# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)
if(TARGET dse_external_msgpackc)
    return()
endif()

include(ExternalProject)
include(${CMAKE_CURRENT_LIST_DIR}/dse_external.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()

set(DSE_EXTERNAL_MSGPACKC_PREFIX "${CMAKE_BINARY_DIR}/_deps/dse_external_msgpackc")
set(DSE_EXTERNAL_MSGPACKC_INSTALL_DIR "${DSE_EXTERNAL_MSGPACKC_PREFIX}/install")
set(DSE_EXTERNAL_MSGPACKC_LIB "${DSE_EXTERNAL_MSGPACKC_INSTALL_DIR}/lib/libmsgpackc.a")
set(DSE_EXTERNAL_MSGPACKC_INCLUDE_DIR "${DSE_EXTERNAL_MSGPACKC_INSTALL_DIR}/include")

dse_external_download(msgpackc "${ExternalProject__MSGPACK__URL}"
    "${ExternalProject__MSGPACK__USERNAME}" "${ExternalProject__MSGPACK__PASSWORD}" DSE_EXTERNAL_MSGPACKC_ARCHIVE)
ExternalProject_Add(dse_external_msgpackc_build
    URL                 "${DSE_EXTERNAL_MSGPACKC_ARCHIVE}"
    URL_HASH            ${DSE_EXTERNAL_MSGPACKC_ARCHIVE_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX              "${DSE_EXTERNAL_MSGPACKC_PREFIX}"
    INSTALL_DIR         "${DSE_EXTERNAL_MSGPACKC_INSTALL_DIR}"
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
        -DMSGPACK_ENABLE_SHARED=OFF
        -DMSGPACK_BUILD_EXAMPLES=OFF
        -DMSGPACK_BUILD_TESTS=OFF
        -DMSGPACK_GEN_COVERAGE=OFF
    BUILD_BYPRODUCTS    "${DSE_EXTERNAL_MSGPACKC_LIB}"
)

# Imported targets require their include directories to exist at generate time.
file(MAKE_DIRECTORY "${DSE_EXTERNAL_MSGPACKC_INCLUDE_DIR}")
add_library(dse_external_msgpackc STATIC IMPORTED GLOBAL)
set_target_properties(dse_external_msgpackc
    PROPERTIES
        IMPORTED_LOCATION "${DSE_EXTERNAL_MSGPACKC_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DSE_EXTERNAL_MSGPACKC_INCLUDE_DIR}"
)
add_dependencies(dse_external_msgpackc dse_external_msgpackc_build)
