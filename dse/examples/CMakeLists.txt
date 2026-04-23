# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

set(OS_KEYS
    "Windows-4"
    "Windows-8"
    "Linux-4"
    "Linux-8"
)
set(FMI2_BIN_PATHS
    "binaries/win32"
    "binaries/win64"
    "binaries/linux32"
    "binaries/linux64"
)
set(FMI3_BIN_PATHS
    "binaries/x86-windows"
    "binaries/x86_64-windows"
    "binaries/x86-linux"
    "binaries/x86_64-linux"
)
list(FIND OS_KEYS "${CMAKE_SYSTEM_NAME}-${CMAKE_SIZEOF_VOID_P}" OS_IDX)

add_subdirectory(fmu/counter)
add_subdirectory(fmu/linear)
if (UNIX)
add_subdirectory(fmu/network)
add_subdirectory(gateway)
add_subdirectory(direct)
endif()
add_subdirectory(fmimodelc)