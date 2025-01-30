// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <dse/fmigateway/fmigateway.h>


int fmigateway_setenv(const char* name, const char* value)
{
    return setenv(name, value, true);
}
