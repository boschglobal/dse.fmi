// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <dse/testing.h>
#include <dse/fmu/fmu.h>
#include <dse/clib/util/strings.h>
#include <mock_interface.h>


FmuInstanceData* captured_fmu_instance;

static void _test_fmu_setup(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    function_called();
}

static void _test_fmu_reset(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    function_called();
}

static void _test_fmu_remove(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    function_called();
}

void __wrap_fmu_load_signal_handlers(FmuInstanceData* fmu)
{
    function_called();
    fmu->variables.vtable.reset = _test_fmu_reset;
    fmu->variables.vtable.setup = _test_fmu_setup;
    fmu->variables.vtable.remove = _test_fmu_remove;
}

FmuInstanceData* fmu_create(FmuInstanceData* fmu)
{
    captured_fmu_instance = fmu;
    function_called();
    int scenario = mock_type(int);
    switch (scenario) {
    case RETURN_NULL:
        return NULL;
    case RETURN_NEW_INSTANCE: {
        FmuInstanceData* new_instance = calloc(1, sizeof(FmuInstanceData));
        memcpy(new_instance, fmu, sizeof(FmuInstanceData));
        return new_instance;
    }
    case RETURN_THE_SAME_INSTANCE:
        return fmu;
    case SET_ERRNO: {
        errno = EACCES;
        return NULL;
    }
    }
    return NULL;
}

int32_t fmu_init(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}

int32_t fmu_step(
    FmuInstanceData* fmu, double communication_point, double step_size)
{
    UNUSED(fmu);
    UNUSED(communication_point);
    UNUSED(step_size);
    return 0;
}

int32_t fmu_destroy(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    function_called();
    return (int32_t)mock();
}
