// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>


/* FMI MCL counterpart: drives the FMU inputs and tunable parameters, and reads
 * the FMU outputs. */


typedef struct {
    ModelDesc model;
    struct {
        double** signal;
        size_t   count;
    } tx;
    struct {
        double** signal;
        size_t   count;
    } rx;
    uint32_t counter;
    double   checksum;
} BenchmarkTargetDesc;


static void _index_signals(BenchmarkTargetDesc* m, int assign)
{
    m->tx.count = 0;
    m->rx.count = 0;
    for (SignalVector* sv = m->model.sv; sv && sv->name; sv++) {
        for (uint32_t i = 0; i < sv->count; i++) {
            if (sv->is_binary) continue;

            const char* causality =
                signal_annotation(sv, i, "fmi_variable_causality", NULL);
            if (causality == NULL) continue;
            if (strcmp(causality, "input") == 0 ||
                strcmp(causality, "parameter") == 0) {
                if (assign) m->tx.signal[m->tx.count] = &sv->scalar[i];
                m->tx.count++;
            } else if (strcmp(causality, "output") == 0) {
                if (assign) m->rx.signal[m->rx.count] = &sv->scalar[i];
                m->rx.count++;
            }
        }
    }
}


ModelDesc* model_create(ModelDesc* model)
{
    BenchmarkTargetDesc* m = calloc(1, sizeof(BenchmarkTargetDesc));
    memcpy(m, model, sizeof(ModelDesc));

    _index_signals(m, 0);
    m->tx.signal = calloc(m->tx.count + 1, sizeof(double*));
    m->rx.signal = calloc(m->rx.count + 1, sizeof(double*));
    _index_signals(m, 1);

    log_notice("Benchmark Target: tx signals=%zu, rx signals=%zu", m->tx.count,
        m->rx.count);

    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    BenchmarkTargetDesc* m = (BenchmarkTargetDesc*)model;
    m->counter++;

    for (size_t i = 0; i < m->tx.count; i++) {
        *(m->tx.signal[i]) = m->counter + i;
    }
    for (size_t i = 0; i < m->rx.count; i++) {
        m->checksum += *(m->rx.signal[i]);
    }

    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    BenchmarkTargetDesc* m = (BenchmarkTargetDesc*)model;
    log_notice("Benchmark Target: steps=%u, rx checksum=%.0f", m->counter,
        m->checksum);
    free(m->tx.signal);
    free(m->rx.signal);
}
