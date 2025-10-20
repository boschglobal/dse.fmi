# Copyright 2025 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODULE "Network")
set(MODULE_LC "network")
project(${MODULE}
    VERSION ${VERSION}
    DESCRIPTION "Dynamic Simulation Environment - FMU Network Example"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb -DLIBXML_STATIC")
file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/.keepme
    CONTENT "keepme"
)
set(FMU_FILES
    network.c
)


# Targets - FMI2 FMU
# ==================
set(FMU_FMI2_PATH examples/fmu/${MODULE_LC}/fmi2)
add_library(fmu2${MODULE_LC} SHARED
    ${FMU_FILES}
)
target_include_directories(fmu2${MODULE_LC}
    PRIVATE
        ${REPO_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_libraries(fmu2${MODULE_LC}
    PRIVATE
        fmi2-common
        ab-codec
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
        m
)
set_target_properties(fmu2${MODULE_LC}
    PROPERTIES
        PREFIX ""
)
install(
    TARGETS
        fmu2${MODULE_LC}
    LIBRARY DESTINATION
        ${FMU_FMI2_PATH}/binaries/linux64
)
install(
    FILES
        fmi2/modelDescription.xml
    DESTINATION
        ${FMU_FMI2_PATH}
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/.keepme
    DESTINATION
        ${FMU_FMI2_PATH}/resources
)


# Targets - FMI3 FMU
# ==================
set(FMU_FMI3_PATH examples/fmu/${MODULE_LC}/fmi3)
add_library(fmu3${MODULE_LC} SHARED
    ${FMU_FILES}
)
target_include_directories(fmu3${MODULE_LC}
    PRIVATE
        ${REPO_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
)
target_link_libraries(fmu3${MODULE_LC}
    PRIVATE
        fmi2-common
        ab-codec
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${WIN32}>:dl>
        m
)
set_target_properties(fmu3${MODULE_LC}
    PROPERTIES
        PREFIX ""
)
install(
    TARGETS
        fmu3${MODULE_LC}
    LIBRARY DESTINATION
        ${FMU_FMI3_PATH}/binaries/x86_64-linux
)
install(
    FILES
        fmi3/modelDescription.xml
    DESTINATION
        ${FMU_FMI3_PATH}
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/.keepme
    DESTINATION
        ${FMU_FMI3_PATH}/resources
)


# Package FMU
# ===========
install(CODE "
    execute_process(
        COMMAND sh -c \"
            FILE=${CMAKE_CURRENT_BINARY_DIR}/fmu2${MODULE_LC}.zip
            if [ ! -f $FILE ]; then
                zip -r $FILE ./
            fi
            exit 0
        \"
        WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${FMU_FMI2_PATH}
    )"
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/fmu2${MODULE_LC}.zip
    DESTINATION
        ${FMU_FMI2_PATH}
    RENAME
        ${MODULE_LC}.fmu
)

install(CODE "
    execute_process(
        COMMAND sh -c \"
            FILE=${CMAKE_CURRENT_BINARY_DIR}/fmu3${MODULE_LC}.zip
            if [ ! -f $FILE ]; then
                zip -r $FILE ./
            fi
        \"
        WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${FMU_FMI3_PATH}
    )"
)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/fmu3${MODULE_LC}.zip
    DESTINATION
        ${FMU_FMI3_PATH}
    RENAME
        ${MODULE_LC}.fmu
)
