# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

if (UNIX)
add_subdirectory(fmu/counter)
add_subdirectory(fmu/linear)
add_subdirectory(fmu/network)
add_subdirectory(gateway)
add_subdirectory(direct)
endif()
add_subdirectory(fmimodelc)