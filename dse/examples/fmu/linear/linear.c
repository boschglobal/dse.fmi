// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/clib/util/strings.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/fmu/fmu.h>

#define UNUSED(x) ((void)x)
#define VR_INPUT  "1"
#define VR_FACTOR "2"
#define VR_OFFSET "3"
#define VR_OUTPUT "4"

typedef struct {
    double* input;
    double* factor;
    double* offset;
    double* output;
} LinearModel;

int fmu_create(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}

int fmu_init(FmuInstanceData* fmu)
{
    if (fmu->data) return 1;

    LinearModel* lm = calloc(1, sizeof(LinearModel));
    lm->input = hashmap_get(&fmu->variables.scalar.input, VR_INPUT);
    lm->factor = hashmap_get(&fmu->variables.scalar.input, VR_FACTOR);
    lm->offset = hashmap_get(&fmu->variables.scalar.input, VR_OFFSET);
    lm->output = hashmap_get(&fmu->variables.scalar.output, VR_OUTPUT);
    if (lm->input && lm->factor && lm->offset && lm->output) {
        fmu->data = lm;
        return 0;
    } else {
        free(lm);
        return 2;
    }
}

int fmu_step(FmuInstanceData* fmu, double CommunicationPoint, double stepSize)
{
    UNUSED(CommunicationPoint);
    UNUSED(stepSize);
    if (fmu->data == NULL) return 3;
    LinearModel* lm = fmu->data;

    /*
    Evaluate the Linear Function.

        y = mx + c

    where:
        x = input
        m = factor
        c = offset
        y = output
    */
    *lm->output = (*lm->input * *lm->factor) + *lm->offset;
    return 0;
}

int fmu_destroy(FmuInstanceData* fmu)
{
    free(fmu->data);
    return 0;
}

void fmu_reset_binary_signals(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}
