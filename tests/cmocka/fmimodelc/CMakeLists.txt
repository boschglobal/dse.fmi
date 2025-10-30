# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmimodelc)


add_library(fmimodelc_runtime OBJECT
    ${REPO_DIR}/dse/fmimodelc/runtime.c
    $<$<BOOL:${WIN32}>:${REPO_DIR}/dse/fmimodelc/env_win32.c>
    $<$<BOOL:${UNIX}>:${REPO_DIR}/dse/fmimodelc/env_unix.c>
    ${DSE_CLIB_SOURCE_DIR}/util/ascii85.c
)
target_include_directories(fmimodelc_runtime
    PUBLIC
        ${REPO_DIR}
        ${DSE_MODELC_SOURCE_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
)
target_compile_definitions(fmimodelc_runtime
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_libraries(fmimodelc_runtime
    PRIVATE
        cmocka
)


# Target - test_fmimodelc
# =======================
add_executable(test_fmimodelc
    __test__.c
    test_index.c
)
set_target_properties(test_fmimodelc PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)
target_compile_definitions(test_fmimodelc
    PUBLIC
        CMOCKA_TESTING
    PRIVATE
        PLATFORM_OS="${CDEF_PLATFORM_OS}"
        PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
)
target_link_directories(test_fmimodelc
    PRIVATE
        ${dse_modelc_lib_SOURCE_DIR}/lib
)
target_link_libraries(test_fmimodelc
    PUBLIC
        fmimodelc_runtime
    PRIVATE
        modelc
        yaml
        cmocka
        dl
        m
)
install(TARGETS test_fmimodelc)
install(
    FILES
        ${dse_modelc_lib_SOURCE_DIR}/lib/libmodelc.so
    DESTINATION
        bin
)

