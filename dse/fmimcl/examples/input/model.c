// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


extern char* dse_ascii85_encode(const char* source, size_t source_len);
extern char* dse_ascii85_decode(const char* source, size_t* len);


typedef struct {
    SignalVector* sv;
    uint32_t      index;
    uint8_t*      buffer;
    uint32_t      buffer_size;
} BinarySignalDesc;

typedef struct {
    ModelDesc model;
    /* Signal Pointers. */
    struct {
        double* real_3_rx;
        double* integer_3_rx;
        double* real_A_rx;
    } signals;
    /* Binary Signal Indexes. */
    struct {
        BinarySignalDesc string_tx;
        BinarySignalDesc string_rx;
        BinarySignalDesc string_ascii85_tx;
        BinarySignalDesc string_ascii85_rx;
    } binary;
} ExtendedModelDesc;


static inline double* _index_scalar(
    ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.scalar == NULL) log_fatal("Signal not found (%s:%s)", v, s);
    return idx.scalar;
}


static inline BinarySignalDesc _index_binary(
    ExtendedModelDesc* m, const char* v, const char* s)
{
    ModelSignalIndex idx = signal_index((ModelDesc*)m, v, s);
    if (idx.binary == NULL) log_fatal("Signal not found (%s:%s)", v, s);

    BinarySignalDesc ret = {
        .sv = &(m->model.sv[idx.vector]),
        .index = idx.signal,
        .buffer = calloc(10, sizeof(char)),
        .buffer_size = 10,
    };
    return ret;
}


ModelDesc* model_create(ModelDesc* model)
{
    /* Extend the ModelDesc object (using a shallow copy). */
    ExtendedModelDesc* m = calloc(1, sizeof(ExtendedModelDesc));
    memcpy(m, model, sizeof(ModelDesc));

    /* Index the signals that are used by this model. */
    m->signals.real_3_rx = _index_scalar(m, "input_channel", "real_3_rx");
    m->signals.integer_3_rx = _index_scalar(m, "input_channel", "integer_3_rx");
    m->signals.real_A_rx = _index_scalar(m, "input_channel", "real_A_rx");

    m->binary.string_tx = _index_binary(m, "binary_channel", "string_tx");
    m->binary.string_rx = _index_binary(m, "binary_channel", "string_rx");
    m->binary.string_ascii85_tx =
        _index_binary(m, "binary_channel", "string_ascii85_tx");
    m->binary.string_ascii85_rx =
        _index_binary(m, "binary_channel", "string_ascii85_rx");


    /* Set initial values. */
    *(m->signals.real_3_rx) = 1;
    *(m->signals.integer_3_rx) = 2;
    *(m->signals.real_A_rx) = 3;

    /* Return the extended object. */
    return (ModelDesc*)m;
}


static inline void _write_message(
    BinarySignalDesc* b, const char* prefix, int v, bool encoded)
{
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%s %d", prefix, v);
    if (encoded) {
        char* msg = dse_ascii85_encode(buffer, strlen(buffer));
        signal_append(b->sv, b->index, (uint8_t*)msg, strlen(msg) + 1);
        free(msg);
    } else {
        signal_append(b->sv, b->index, (uint8_t*)buffer, strlen(buffer) + 1);
    }
}


static inline void _log_message(BinarySignalDesc* b, bool encoded)
{
    uint8_t* buffer;
    size_t   len;

    signal_read(b->sv, b->index, &buffer, &len);
    if (len) {
        if (encoded) {
            char* msg = dse_ascii85_decode((char*)buffer, &len);
            log_info("String (%s) : %s", b->sv->signal[b->index], msg);
            free(msg);
        } else {
            log_info("String (%s) : %s", b->sv->signal[b->index], buffer);
        }
    }
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    /* Print incomming strings. */
    _log_message(&(m->binary.string_tx), false);
    _log_message(&(m->binary.string_rx), false);
    _log_message(&(m->binary.string_ascii85_tx), true);
    _log_message(&(m->binary.string_ascii85_rx), true);

    /* Increment signals. */
    *(m->signals.real_3_rx) = *(m->signals.real_3_rx) + 1;
    *(m->signals.integer_3_rx) = *(m->signals.integer_3_rx) + 2;
    *(m->signals.real_A_rx) = *(m->signals.real_A_rx) + 3;

    /* Reset binary signals. */
    signal_reset(m->binary.string_tx.sv, m->binary.string_tx.index);
    signal_reset(m->binary.string_rx.sv, m->binary.string_rx.index);
    signal_reset(
        m->binary.string_ascii85_tx.sv, m->binary.string_ascii85_tx.index);
    signal_reset(
        m->binary.string_ascii85_rx.sv, m->binary.string_ascii85_rx.index);

    /* Generate strings. */
    _write_message(
        &(m->binary.string_tx), "foo", (int)*(m->signals.real_3_rx), false);
    _write_message(&(m->binary.string_ascii85_tx), "bar",
        (int)*(m->signals.real_A_rx), true);

    /* Complete the step. */
    *model_time = stop_time;
    return 0;
}
