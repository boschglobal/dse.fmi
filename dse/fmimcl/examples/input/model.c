// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


typedef struct {
    ModelDesc model;
    /* Signal Pointers. */
    struct {
        double* real_3_rx;
        double* integer_3_rx;
        double* real_A_rx;
    } signals;
} ExtendedModelDesc;


static inline double* _index(ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = m->model.index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    return idx.scalar;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Index the signals that are used by this model. */
    m->signals.real_3_rx = _index(m, "input_channel", "real_3_rx");
    m->signals.integer_3_rx = _index(m, "input_channel", "integer_3_rx");
    m->signals.real_A_rx = _index(m, "input_channel", "real_A_rx");

    /* Set initial values. */
    *(m->signals.real_3_rx) = 1;
    *(m->signals.integer_3_rx) = 2;
    *(m->signals.real_A_rx) = 3;

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;
    *(m->signals.real_3_rx) = *(m->signals.real_3_rx) + 1;
    *(m->signals.integer_3_rx) = *(m->signals.integer_3_rx) + 2;
    *(m->signals.real_A_rx) = *(m->signals.real_A_rx) + 3;

    *model_time = stop_time;
    return 0;
}
