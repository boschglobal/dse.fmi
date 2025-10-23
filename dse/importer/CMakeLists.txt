# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

set(MODULE "Importer")
set(MODULE_LC "importer")
project(${MODULE}
    VERSION "$ENV{PACKAGE_VERSION}"
    DESCRIPTION "FMU Importer for fmi2 and fmi3"
    HOMEPAGE_URL "$ENV{PROJECT_URL}"
)


# Target - Importer
# =================
add_executable(${MODULE_LC}
    importer.c
    signal_bus.c
    xml.c
    ${CLIB_SOURCE_FILES}
)
set_target_properties(${MODULE_LC}
    PROPERTIES
        OUTPUT_NAME fmuImporter
)
target_include_directories(${MODULE_LC}
    PRIVATE
        ${REPO_DIR}
        ${FMI2_INCLUDE_DIR}
        ${FMI3_INCLUDE_DIR}
        ${DSE_MODELC_INCLUDE_DIR}
        ${DSE_CLIB_INCLUDE_DIR}
)
target_link_libraries(${MODULE_LC}
    PRIVATE
        ab-codec
        xml
        $<$<BOOL:${WIN32}>:bcrypt>
        $<$<BOOL:${UNIX}>:mcheck>
        dl
        m
)
install(
    TARGETS
        ${MODULE_LC}
    RUNTIME DESTINATION
        ${MODULE_LC}
)


# Package
# =======
set(CPACK_SYSTEM_NAME $ENV{PACKAGE_ARCH})
set(CPACK_PACKAGE_VENDOR "Robert Bosch GmbH")
set(CPACK_PACKAGE_DESCRIPTION "DSE FMI Importer")
set(CPACK_OUTPUT_FILE_PREFIX _dist)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_CHECKSUM MD5)
set(CPACK_MONOLITHIC_INSTALL TRUE)
include(CPack)
