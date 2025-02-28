# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "FmiMcl")
set(MODULE_LC "fmimcl")

project(${MODULE}
    VERSION ${VERSION}
    DESCRIPTION "DSE FMI Model Compatibility Library"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)


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
    ../fmu/ascii85.c
    ${DSE_CLIB_SOURCE_DIR}/clib/mdf/mdf.c
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
install(
    TARGETS fmimcl
    LIBRARY DESTINATION
        ${MODULE_LC}/lib
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
