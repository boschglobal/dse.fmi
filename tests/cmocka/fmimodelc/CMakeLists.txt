# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)

project(test_fmimodelc)



# FMI ModelC Code
# ===============
set(DSE_FMIMODELC_SOURCE_DIR ../../../dse/fmimodelc)
set(DSE_FMIMODELC_SOURCE_FILES
    ${DSE_FMIMODELC_SOURCE_DIR}/runtime.c
    ${DSE_FMIMODELC_SOURCE_DIR}/ascii85.c
)
set(DSE_FMIMODELC_INCLUDE_DIR "${DSE_FMIMODELC_SOURCE_DIR}/../..")


# Targets
# =======

# Target - FMI ModelC
# -------------------
add_executable(test_fmimodelc
    __test__.c
    test_index.c
    test_ascii85.c
    ${DSE_FMIMODELC_SOURCE_FILES}
)
target_include_directories(test_fmimodelc
    PRIVATE
        ${DSE_FMIMODELC_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ./
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
        ${MODELC_BINARY_DIR}/lib
)
target_link_libraries(test_fmimodelc
    PUBLIC
    PRIVATE
        -Wl,--whole-archive
        model_runtime
        -Wl,--no-whole-archive
        yaml
        cmocka
        dl
        m
)
install(TARGETS test_fmimodelc)
install(
    FILES
    DESTINATION
        data
)

