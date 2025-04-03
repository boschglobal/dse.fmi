// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/fmu/fmu.h>

typedef struct {
    double input;
    double factor;
    double offset;
    double output;
} VarTable;

FmuInstanceData* fmu_create(FmuInstanceData* fmu)
{
    VarTable* v = malloc(sizeof(VarTable));
    *v = (VarTable){
        .input = fmu_register_var(fmu, 1, true, offsetof(VarTable, input)),
        .factor = fmu_register_var(fmu, 2, true, offsetof(VarTable, factor)),
        .offset = fmu_register_var(fmu, 3, true, offsetof(VarTable, offset)),
        .output = fmu_register_var(fmu, 4, false, offsetof(VarTable, output)),
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

    /*
    Evaluate the Linear Function.

        y = mx + c

    where:
        x = input
        m = factor
        c = offset
        y = output
    */
    v->output = (v->input * v->factor) + v->offset;
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
