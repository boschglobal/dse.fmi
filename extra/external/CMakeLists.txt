# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

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
if($ENV{REPO_MIRRORS})
    include($ENV{REPO_MIRRORS} OPTIONAL)
endif()



# External Projects
# =================
set_property(DIRECTORY PROPERTY EP_STEP_TARGETS download)
include(FetchContent)
include(ExternalProject)


# External Project - DSE C Lib
# ----------------------------
FetchContent_Declare(dse_clib
    URL                 ${ExternalProject__CLIB__URL}
    HTTP_USERNAME       ${ExternalProject__CLIB__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__CLIB__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.clib"
)
FetchContent_MakeAvailable(dse_clib)
set(DSE_CLIB_SOURCE_DIR ${dse_clib_SOURCE_DIR}/dse)
set(DSE_CLIB_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/..")

# External Project - DSE ModelC
# -----------------------------
FetchContent_Declare(dse_modelc
    URL                 ${ExternalProject__MODELC__URL}
    HTTP_USERNAME       ${ExternalProject__MODELC__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__MODELC__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc"
)
FetchContent_MakeAvailable(dse_modelc)
set(DSE_MODELC_SOURCE_DIR ${dse_modelc_SOURCE_DIR}/dse/modelc)
set(DSE_MODELC_INCLUDE_DIR "${DSE_MODELC_SOURCE_DIR}/../..")

# External Project - DSE ModelC LIB
# ---------------------------------
FetchContent_Declare(dse_modelc_lib
    URL                 ${ExternalProject__MODELC_LIB__URL}
    HTTP_USERNAME       ${ExternalProject__MODELC_LIB__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__MODELC_LIB__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib"
)
FetchContent_MakeAvailable(dse_modelc_lib)

# External Project - yaml (unit test only)
# -----------------------
ExternalProject_Add(yaml
    URL                 ${ExternalProject__YAML__URL}
    HTTP_USERNAME       ${ExternalProject__YAML__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__YAML__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/yaml"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/yaml"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DBUILD_SHARED_LIBS=OFF
    INSTALL_COMMAND     ""
)

# dlfcn-win32
# -----------
if(NOT WIN32)
    set(DLFCNWIN32_BUILD_COMMAND "")
endif()
ExternalProject_Add(dlfcnwin32
    URL                 ${ExternalProject__DLFCNWIN32__URL}
    HTTP_USERNAME       ${ExternalProject__DLFCNWIN32__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__DLFCNWIN32__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32"
    BINARY_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32-build"
    CMAKE_ARGS
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
    BUILD_COMMAND       ${DLFCNWIN32_BUILD_COMMAND}
    INSTALL_COMMAND     ""
)


# OSS Target
# ==========
add_custom_target(oss)
function(oss_copy src dst)
    add_custom_command(TARGET oss PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${src}"
            "../../../dse/__oss__/${dst}"
    )
endfunction()
add_dependencies(oss
    dlfcnwin32-download
)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/dse.clib      boschglobal/dse.clib)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/dse.modelc    boschglobal/dse.modelc)
oss_copy($ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32    dlfcn-win32/dlfcn-win32)
