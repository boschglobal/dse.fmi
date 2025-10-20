# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

project(test_fmu)


# FMU Objects
# -----------
set(DSE_FMU_SOURCE_DIR ${REPO_DIR}/dse/fmu)
add_library(fmi2_runtime OBJECT
    ${DSE_CLIB_SOURCE_DIR}/util/ascii85.c
    ${DSE_FMU_SOURCE_DIR}/fmi2fmu.c
    ${DSE_FMU_SOURCE_DIR}/fmi2variable.c
    ${DSE_FMU_SOURCE_DIR}/ncodec.c
    ${DSE_FMU_SOURCE_DIR}/signal.c
)
target_include_directories(fmi2_runtime
    PUBLIC
        ${REPO_DIR}
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_libraries(fmi2_runtime
    PUBLIC
        ab-codec
    PRIVATE
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
        m
)
add_library(fmi3_runtime OBJECT
    ${DSE_CLIB_SOURCE_DIR}/util/ascii85.c
    ${DSE_FMU_SOURCE_DIR}/fmi3fmu.c
    ${DSE_FMU_SOURCE_DIR}/fmi3variable.c
    ${DSE_FMU_SOURCE_DIR}/ncodec.c
    ${DSE_FMU_SOURCE_DIR}/signal.c
)
target_include_directories(fmi3_runtime
    PUBLIC
        ${REPO_DIR}
        ${FMI3_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_libraries(fmi3_runtime
    PUBLIC
        ab-codec
    PRIVATE
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
        m
)


# Target - test_fmiXfmu
# =====================
function(fmiXfmu_executable FMI_VERSION)
    set(EXE_NAME test_fmi${FMI_VERSION}fmu)
    set(FMU_SOURCE_FILES ${DSE_FMU${FMI_VERSION}_SOURCE_FILES})
    add_executable(${EXE_NAME}
        ${EXE_NAME}.c
        mock_interface.c
    )
    target_include_directories(${EXE_NAME}
        PRIVATE
            ./
    )
    target_link_libraries(${EXE_NAME}
        PUBLIC
            fmi${FMI_VERSION}_runtime
            clib_runtime
        PRIVATE
            ab-codec
            yaml
            xml
            cmocka
            dl
            m
    )
    target_compile_definitions(${EXE_NAME}
        PUBLIC
            CMOCKA_TESTING
        PRIVATE
            PLATFORM_OS="${CDEF_PLATFORM_OS}"
            PLATFORM_ARCH="${CDEF_PLATFORM_ARCH}"
    )
    target_link_options(${EXE_NAME}
        PRIVATE
            -Wl,-wrap=fmu_load_signal_handlers
    )
    target_link_directories(${EXE_NAME}
        PRIVATE
            ${MODELC_BINARY_DIR}/lib
    )
    install(TARGETS ${EXE_NAME})
endfunction()
fmiXfmu_executable(2)
fmiXfmu_executable(3)


# Target - test_fmu
# =================
add_executable(test_fmu
    __test__.c
    test_ascii85.c
    test_signals.c
    test_variables.c
    ${DSE_FMU_SOURCE_DIR}/fmu.c
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
        fmi2_runtime
        clib_runtime
    PRIVATE
        ab-codec
        yaml
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
