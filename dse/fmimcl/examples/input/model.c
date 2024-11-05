// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <dse/modelc/model.h>
#include <dse/logger.h>


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


static inline int _format_message(
    BinarySignalDesc* b, const char* prefix, int v)
{
    return snprintf((char*)b->buffer, b->buffer_size, "%s %d", prefix, v);
}

int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    ExtendedModelDesc* m = (ExtendedModelDesc*)model;

    /* Print incomming strings. */
    uint8_t* buffer;
    size_t   len;
    signal_read(
        m->binary.string_tx.sv, m->binary.string_tx.index, &buffer, &len);
    if (len)
        log_info("String (%s) : %s",
            m->binary.string_tx.sv->signal[m->binary.string_tx.index], buffer);
    signal_read(
        m->binary.string_rx.sv, m->binary.string_rx.index, &buffer, &len);
    if (len)
        log_info("String (%s) : %s",
            m->binary.string_rx.sv->signal[m->binary.string_rx.index], buffer);

    /* Increment signals. */
    *(m->signals.real_3_rx) = *(m->signals.real_3_rx) + 1;
    *(m->signals.integer_3_rx) = *(m->signals.integer_3_rx) + 2;
    *(m->signals.real_A_rx) = *(m->signals.real_A_rx) + 3;

    /* Generate strings. */
    len = _format_message(
        &(m->binary.string_tx), "foo", (int)*(m->signals.real_3_rx));
    if (len >= (m->binary.string_tx.buffer_size - 1)) {
        m->binary.string_tx.buffer =
            realloc(m->binary.string_tx.buffer, len + 1);
        m->binary.string_tx.buffer_size = len + 1;
        _format_message(
            &(m->binary.string_tx), "foo", (int)*(m->signals.real_3_rx));
    }
    signal_reset(m->binary.string_tx.sv, m->binary.string_tx.index);
    signal_append(m->binary.string_tx.sv, m->binary.string_tx.index,
        m->binary.string_tx.buffer,
        strlen((char*)m->binary.string_tx.buffer) + 1);


    signal_reset(m->binary.string_rx.sv, m->binary.string_rx.index);
    signal_reset(
        m->binary.string_ascii85_tx.sv, m->binary.string_ascii85_tx.index);
    signal_reset(
        m->binary.string_ascii85_rx.sv, m->binary.string_ascii85_rx.index);

    *model_time = stop_time;
    return 0;
}
