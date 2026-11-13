# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0135 NEW)

project(External_Projects__dse_fmimcl)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_C_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O3")
add_compile_options(-Wall -W -Wwrite-strings -Wno-missing-field-initializers)



# Load URLs
# =========
include(oss_repos.cmake)
if(DEFINED ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()



# External Projects
# =================
set_property(DIRECTORY PROPERTY EP_STEP_TARGETS download)
include(FetchContent)
include(ExternalProject)


# DSE C Lib
# ---------
FetchContent_Declare(dse_clib
    URL                 $ENV{DSE_CLIB_URL}
    HTTP_USERNAME       $ENV{DSE_CLIB_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_CLIB_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/..")

# DSE ModelC
# ----------
FetchContent_Declare(dse_modelc
    URL                 $ENV{DSE_MODELC_URL}
    HTTP_USERNAME       $ENV{DSE_MODELC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_MODELC_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc"
)
FetchContent_MakeAvailable(dse_modelc)
set(DSE_MODELC_SOURCE_DIR ${dse_modelc_SOURCE_DIR}/dse/modelc)
set(DSE_MODELC_INCLUDE_DIR "${DSE_MODELC_SOURCE_DIR}/../..")

# DSE ModelC LIB
# --------------
FetchContent_Declare(dse_modelc_lib
    URL                 $ENV{DSE_MODELC_LIB_URL}
    HTTP_USERNAME       $ENV{DSE_MODELC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_MODELC_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib"
)
FetchContent_MakeAvailable(dse_modelc_lib)

# DSE Network Codec
# -----------------
include(FetchContent)
FetchContent_Declare(dse_ncodec
    URL                 $ENV{DSE_NCODEC_URL}
    HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
    HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.ncodec"
)
FetchContent_MakeAvailable(dse_ncodec)
set(DSE_NCODEC_SOURCE_DIR ${dse_ncodec_SOURCE_DIR}/dse/ncodec)
set(DSE_NCODEC_INCLUDE_DIR "${DSE_NCODEC_SOURCE_DIR}/../..")

# # DSE Network Codec (unit test only)
# # -----------------
# ExternalProject_Add(dse_ncodec
#     URL                 $ENV{DSE_NCODEC_URL}
#     HTTP_USERNAME       $ENV{DSE_NCODEC_URL_USER}
#     HTTP_PASSWORD       $ENV{DSE_NCODEC_URL_TOKEN}
#     SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec"
#     SOURCE_SUBDIR       "dse/ncodec/libs/automotive-bus"
#     BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse_ncodec"
#     CMAKE_ARGS
#         -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
#     INSTALL_COMMAND     ""
# )

# OSS Sources
# ===========
include(xml2.cmake)
include(dlfcnwin32.cmake)


# OSS Target
# ==========
add_custom_target(oss)
# src: a directory, or an ExternalProject target (its downloaded source is copied).
function(oss_copy src dst)
    if(TARGET ${src})
        add_dependencies(oss ${src}-download)
        ExternalProject_Get_Property(${src} SOURCE_DIR)
        set(src ${SOURCE_DIR})
    endif()
    add_custom_command(TARGET oss PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${src}"
            "../../../dse/__oss__/${dst}"
    )
endfunction()
oss_copy(${dse_clib_SOURCE_DIR}                 boschglobal/dse.clib)
oss_copy(${dse_modelc_SOURCE_DIR}               boschglobal/dse.modelc)
oss_copy(${dse_ncodec_SOURCE_DIR}               boschglobal/dse.ncodec)
oss_copy(dse_external_dl_build                  dlfcn-win32/dlfcn-win32)
oss_copy(dse_external_xml2_build                GNOME/libxml2)
