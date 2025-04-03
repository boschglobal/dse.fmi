// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/fmu/fmu.h>

typedef struct {
    double counter;
} VarTable;

FmuInstanceData* fmu_create(FmuInstanceData* fmu)
{
    VarTable* v = malloc(sizeof(VarTable));
    *v = (VarTable){
        .counter = fmu_register_var(fmu, 1, false, offsetof(VarTable, counter)),
    };
    fmu_register_var_table(fmu, v);
    return fmu;
}

int fmu_init(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}

int fmu_step(FmuInstanceData* fmu, double CommunicationPoint, double stepSize)
{
    UNUSED(CommunicationPoint);
    UNUSED(stepSize);
    VarTable* v = fmu_var_table(fmu);

    /* Increment the counter. */
    v->counter += 1;
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
