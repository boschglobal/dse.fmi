# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

# set(CMAKE_VERBOSE_MAKEFILE ON)


#################################################
## Configure FMU here:
## ------------------
set(VERSION "$ENV{PACKAGE_VERSION}")
set(MODULE "Counter")
set(MODULE_LC "counter")
set(FMU_FILES
    counter.c
)
##
#################################################


project(Fmu VERSION ${VERSION})

include(FetchContent)

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ggdb -DLIBXML_STATIC")
list(APPEND C_CXX_WARNING_FLAGS
    -Wall
    -W
    -Wwrite-strings
    -Wno-missing-field-initializers
    -Wno-misleading-indentation
)
add_compile_options(${C_CXX_WARNING_FLAGS})


set(DSE_CLIB_SOURCE_FILES
    ${DSE_CLIB_SOURCE_DIR}/clib/util/strings.c
    ${DSE_CLIB_SOURCE_DIR}/clib/util/binary.c
    ${DSE_CLIB_SOURCE_DIR}/clib/collections/hashmap.c
)
set(FMI2_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi2/headers")
set(FMI3_INCLUDE_DIR "${DSE_CLIB_SOURCE_DIR}/clib/fmi/fmi3/headers")
set(FMU_SOURCE_DIR "../../../fmu")
set(FMU_INCLUDE_DIR "../../../..")
set(FMI_2_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi2fmu.c
    ${FMU_SOURCE_DIR}/fmi2variable.c
    ${FMU_SOURCE_DIR}/signal.c
)
set(FMI_3_COMMON_FILES
    ${FMU_SOURCE_DIR}/ascii85.c
    ${FMU_SOURCE_DIR}/fmi3fmu.c
    ${FMU_SOURCE_DIR}/fmi3variable.c
    ${FMU_SOURCE_DIR}/signal.c
)
file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/.keepme
    CONTENT "keepme"
)


# Targets
# =======

# FMI2 FMU
# --------
set(FMU_FMI2_PATH examples/fmu/${MODULE_LC}/fmi2)
add_library(fmu2${MODULE_LC} SHARED
    ${DSE_CLIB_SOURCE_FILES}
    ${FMI_2_COMMON_FILES}
    ${FMU_FILES}
)
target_include_directories(fmu2${MODULE_LC}
    PRIVATE
        ${FMI2_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_libraries(fmu2${MODULE_LC}
    PRIVATE
        network_codec
        flatcc_runtime
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


# FMI3 FMU
# --------
set(FMU_FMI3_PATH examples/fmu/${MODULE_LC}/fmi3)
add_library(fmu3${MODULE_LC} SHARED
    ${DSE_CLIB_SOURCE_FILES}
    ${FMI_3_COMMON_FILES}
    ${FMU_FILES}
)
target_include_directories(fmu3${MODULE_LC}
    PRIVATE
        ${FMI3_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
        ${DSE_NCODEC_INCLUDE_DIR}
        ${FMU_INCLUDE_DIR}
)
target_link_libraries(fmu3${MODULE_LC}
    PRIVATE
        network_codec
        flatcc_runtime
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
# -----------
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
