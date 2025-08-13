# Copyright 2024 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.21)

if (UNIX)
add_subdirectory(fmi2fmu)
add_subdirectory(input)
endif()
