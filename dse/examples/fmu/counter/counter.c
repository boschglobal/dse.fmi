// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/clib/util/strings.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/fmu/fmu.h>

#define UNUSED(x)    ((void)x)
#define VR_COUNTER  "1"

int fmu_create(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}

int fmu_init(FmuInstanceData* fmu)
{
    hashmap_set_double(&(fmu->variables.scalar.output), VR_COUNTER, 0.0);
    return 0;
}

int fmu_step(
    FmuInstanceData* fmu, double CommunicationPoint, double stepSize)
{
    UNUSED(CommunicationPoint);
    UNUSED(stepSize);

    /* Increment the counter. */
    double* counter = hashmap_get(&fmu->variables.scalar.output, VR_COUNTER);
    if (counter) *counter += 1;
    return 0;
}

int fmu_destroy(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}

void fmu_reset_binary_signals(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}
