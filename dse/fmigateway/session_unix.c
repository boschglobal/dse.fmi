// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <dse/fmigateway/fmigateway.h>


int fmigateway_setenv(const char* name, const char* value)
{
    if (value == NULL) return unsetenv(name);

    return setenv(name, value, true);
}

void fmigateway_session_windows_start(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


void fmigateway_session_windows_end(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}
