# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(VERSION "$ENV{PACKAGE_VERSION}")

project(FmiMcl
    VERSION ${VERSION}
    DESCRIPTION "DSE FMI Model Compatibility Library"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)

include(GNUInstallDirs)
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/_out)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS_DEBUG "-g -ggdb")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O3")
list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})
add_compile_definitions(DLL_BUILD)
set(CMAKE_SHARED_LIBRARY_PREFIX "")



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


# External Project - DSE ModelC LIB
# ---------------------------------
FetchContent_Declare(dse_modelc_lib
    URL                 ${ExternalProject__MODELC_LIB__URL}
    HTTP_USERNAME       ${ExternalProject__MODELC_LIB__USERNAME}
    HTTP_PASSWORD       ${ExternalProject__MODELC_LIB__PASSWORD}
    SOURCE_DIR          "$ENV{EXTERNAL_BUILD_DIR}/dse.modelc.lib"
)
FetchContent_MakeAvailable(dse_modelc_lib)
set(DSE_MODELC_INCLUDE_DIR "${dse_modelc_lib_SOURCE_DIR}/include")


# External Project - dlfcn-win32
# ------------------------------
if(WIN32)
set(DLFCNWIN32_SOURCE_DIR "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32/src")
set(DLFCNWIN32_BINARY_DIR "$ENV{EXTERNAL_BUILD_DIR}/dlfcnwin32-build/lib")
find_library(DLFCNWIN32_LIB
    NAMES
        libdl.a
    PATHS
        ${DLFCNWIN32_BINARY_DIR}
    REQUIRED
    NO_DEFAULT_PATH
)
add_library(dl STATIC IMPORTED GLOBAL)
set_target_properties(dl
    PROPERTIES
        IMPORTED_LOCATION "${DLFCNWIN32_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DLFCNWIN32_SOURCE_DIR}"
)
endif()



# Targets
# =======

# FMI MCL
# -------
add_library(fmimcl SHARED
    engine.c
    fmimcl.c
    model.c
    parser.c
    adapter/fmi2mcl.c
)
target_include_directories(fmimcl
    PRIVATE
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ../..
)
target_compile_definitions(fmimcl
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_directories(fmimcl
    PRIVATE
        $<$<BOOL:${WIN32}>:${dse_modelc_lib_SOURCE_DIR}/bin>
)
target_link_libraries(fmimcl
    PRIVATE
        $<$<BOOL:${WIN32}>:modelc>
        $<$<BOOL:${WIN32}>:dl>
)
install(TARGETS fmimcl)
install(
    DIRECTORY
        ../../licenses
    DESTINATION
        licenses
)
install(
    DIRECTORY
        ${CMAKE_BINARY_DIR}/compile_commands.json
    DESTINATION
        doc
)


# Sub Projects
# ============

add_subdirectory(examples)



# Package
# =======
set(CPACK_SYSTEM_NAME $ENV{PACKAGE_ARCH})
set(CPACK_PACKAGE_VENDOR "Robert Bosch GmbH")
set(CPACK_PACKAGE_DESCRIPTION "DSE FMI Model Compatibility Library")
set(CPACK_OUTPUT_FILE_PREFIX _dist)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_CHECKSUM MD5)
set(CPACK_MONOLITHIC_INSTALL TRUE)
include(CPack)
