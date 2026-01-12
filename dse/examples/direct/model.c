// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/modelc/runtime.h>


#define UNUSED(x)           ((void)x)
#define ARRAY_SIZE(x)       (sizeof(x) / sizeof(x[0]))
#define MODEL_PARAM__FACTOR "FACTOR"
#define MODEL_PARAM__OFFSET "OFFSET"


typedef struct fx {
    const char* in;
    const char* out;
    struct {
        ModelSignalIndex in;
        ModelSignalIndex out;
    } index;
} fx;

typedef struct {
    ModelDesc model;
    /* F(x) parameters and matrix. */
    double    factor;
    double    offset;
    fx        matrix[10];
} FxModelDesc;


static inline void _index(FxModelDesc* m, fx* item)
{
    item->index.in = signal_index((void*)m, "in_vector", item->in);
    if (item->index.in.scalar == NULL) {
        log_fatal("Signal not found:  %s (in_vector)", item->in);
    }
    item->index.out = signal_index((void*)m, "out_vector", item->out);
    if (item->index.out.scalar == NULL) {
        log_fatal("Signal not found:  %s (out_vector)", item->out);
    }
}

static inline double _envar(
    ModelDesc* m, const char* name, double default_value)
{
    char   buf[50];
    double val = default_value;

    snprintf(buf, sizeof(buf), "%s__%s", m->mi->name, name);
    for (size_t i = 0; i < strlen(buf); i++)
        buf[i] = toupper(buf[i]);
    if (getenv(buf)) {
        val = atof(getenv(buf));
    } else {
        snprintf(buf, sizeof(buf), "%s", name);
        for (size_t i = 0; i < strlen(buf); i++)
            buf[i] = toupper(buf[i]);
        if (getenv(buf)) {
            val = atof(getenv(buf));
        }
    }

    return val;
}

ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    FxModelDesc* m = calloc(1, sizeof(FxModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Static config for F(x). */
    fx matrix[10] = {
        { .in = "in_a", .out = "out_a" },
        { .in = "in_b", .out = "out_b" },
        { .in = "in_c", .out = "out_c" },
        { .in = "in_d", .out = "out_d" },
        { .in = "in_e", .out = "out_e" },
        { .in = "in_f", .out = "out_f" },
        { .in = "in_g", .out = "out_g" },
        { .in = "in_h", .out = "out_h" },
        { .in = "in_i", .out = "out_i" },
        { .in = "in_j", .out = "out_j" },
    };
    memcpy(m->matrix, matrix, sizeof(m->matrix));

    /* Index the signals. */
    for (size_t i = 0; i < ARRAY_SIZE(m->matrix); i++) {
        _index(m, &m->matrix[i]);
    }

    /* Model parameters. */
    m->factor = _envar(model, MODEL_PARAM__FACTOR, 1);
    m->offset = _envar(model, MODEL_PARAM__OFFSET, 0);

    /* Return the extended object. */
    return (ModelDesc*)m;
}

int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    FxModelDesc* m = (FxModelDesc*)model;

    /* Apply the Model Function. */
    for (size_t i = 0; i < ARRAY_SIZE(m->matrix); i++) {
        fx* _ = &m->matrix[i];
        *_->index.out.scalar = (*_->index.in.scalar * m->factor) + m->offset;
        log_debug("[%d] %f <- %f * %f + %f", *_->index.out.scalar,
            *_->index.in.scalar, m->factor, m->offset);
    }

    /* Indicate progression of time for the entire step.*/
    *model_time = stop_time;
    return 0;
}
