// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <windows.h>


int fmimodelc_setenv(const char* name, const char* value)
{
    return SetEnvironmentVariable(name, value);
}
