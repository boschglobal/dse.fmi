// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0


#include <stdlib.h>
#include <stdbool.h>


int fmimodelc_setenv(const char* name, const char* value)
{
    if (value == NULL) return unsetenv(name);
    return setenv(name, value, true);
}
