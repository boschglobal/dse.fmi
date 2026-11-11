// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/modelc/model.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/frame.h>


/* Gateway counterpart: drives all gateway output scalars and binary signals. */


typedef struct {
    ModelDesc model;
    struct {
        double** signal;
        size_t   count;
    } scalar;
    struct {
        NCODEC** nc;
        size_t   count;
    } binary;
    uint32_t counter;
} BenchmarkTargetDesc;


static void _index_signals(BenchmarkTargetDesc* m, bool assign)
{
    m->scalar.count = 0;
    m->binary.count = 0;
    for (SignalVector* sv = m->model.sv; sv && sv->name; sv++) {
        for (uint32_t i = 0; i < sv->count; i++) {
            if (sv->is_binary) {
                NCODEC* nc = signal_codec(sv, i);
                if (nc == NULL) {
                    log_fatal("NCodec not available (%s:%s)", sv->name,
                        sv->signal[i]);
                }
                if (assign) m->binary.nc[m->binary.count] = nc;
                m->binary.count++;
            } else {
                const char* causality =
                    signal_annotation(sv, i, "fmi_variable_causality", NULL);
                if (causality == NULL || strcmp(causality, "output") != 0) {
                    continue;
                }
                if (assign) m->scalar.signal[m->scalar.count] = &sv->scalar[i];
                m->scalar.count++;
            }
        }
    }
}


ModelDesc* model_create(ModelDesc* model)
{
    BenchmarkTargetDesc* m = calloc(1, sizeof(BenchmarkTargetDesc));
    memcpy(m, model, sizeof(ModelDesc));

    _index_signals(m, false);
    m->scalar.signal = calloc(m->scalar.count + 1, sizeof(double*));
    m->binary.nc = calloc(m->binary.count + 1, sizeof(NCODEC*));
    _index_signals(m, true);

    log_notice("Benchmark Target: scalar outputs=%zu, binary signals=%zu",
        m->scalar.count, m->binary.count);

    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    BenchmarkTargetDesc* m = (BenchmarkTargetDesc*)model;
    m->counter++;

    for (size_t i = 0; i < m->scalar.count; i++) {
        *(m->scalar.signal[i]) = m->counter + i;
    }

    for (size_t i = 0; i < m->binary.count; i++) {
        NCODEC* nc = m->binary.nc[i];

        /* Drain RX so frames from the gateway do not accumulate. */
        NCodecCanMessage rx_msg = {};
        while (ncodec_read(nc, &rx_msg) >= 0) {
        }

        char payload[32];
        snprintf(payload, sizeof(payload), "%u", m->counter);
        NCodecCanMessage tx_msg = {
            .frame_id = 1000 + (uint32_t)i,
            .frame_type = CAN_EXTENDED_FRAME,
            .buffer = (uint8_t*)payload,
            .len = strlen(payload) + 1,
        };
        ncodec_truncate(nc);
        ncodec_write(nc, &tx_msg);
        ncodec_flush(nc);
    }

    *model_time = stop_time;
    return 0;
}


void model_destroy(ModelDesc* model)
{
    BenchmarkTargetDesc* m = (BenchmarkTargetDesc*)model;
    free(m->scalar.signal);
    free(m->binary.nc);
}
