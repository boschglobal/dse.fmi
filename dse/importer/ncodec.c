// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <dse/ncodec/codec.h>


#define UNUSED(x)  ((void)x)
#define BUFFER_LEN 1024


/* Declare an extension to the NCodecStreamVTable type. */
typedef struct __stream {
    NCodecStreamVTable s;
    uint8_t            buffer[BUFFER_LEN];
    size_t             len;
    size_t             pos;
} __stream;


size_t stream_read(NCODEC* nc, uint8_t** data, size_t* len, int32_t pos_op)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc == NULL || _nc->stream == NULL) return -ENOSTR;
    if (data == NULL || len == NULL) return -EINVAL;

    __stream* _s = (__stream*)_nc->stream;

    /* Check EOF. */
    if (_s->pos >= _s->len) {
        *data = NULL;
        *len = 0;
        return 0;
    }
    /* Return buffer, from current pos. */
    *data = &_s->buffer[_s->pos];
    *len = _s->len - _s->pos;
    /* Advance the position indicator. */
    if (pos_op == NCODEC_POS_UPDATE) _s->pos = _s->len;

    return *len;
}

size_t stream_write(NCODEC* nc, uint8_t* data, size_t len)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc == NULL || _nc->stream == NULL) return -ENOSTR;

    __stream* _s = (__stream*)_nc->stream;

    if ((_s->pos + len) > BUFFER_LEN) return -EMSGSIZE;
    memcpy(&_s->buffer[_s->pos], data, len);
    _s->pos += len;
    if (_s->pos > _s->len) _s->len = _s->pos;

    return len;
}

int64_t stream_seek(NCODEC* nc, size_t pos, int32_t op)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __stream* _s = (__stream*)_nc->stream;
        if (op == NCODEC_SEEK_SET) {
            if (pos > _s->len) {
                _s->pos = _s->len;
            } else {
                _s->pos = pos;
            }
        } else if (op == NCODEC_SEEK_CUR) {
            pos = _s->pos + pos;
            if (pos > _s->len) {
                _s->pos = _s->len;
            } else {
                _s->pos = pos;
            }
        } else if (op == NCODEC_SEEK_END) {
            _s->pos = _s->len;
        } else if (op == NCODEC_SEEK_RESET) {
            _s->pos = _s->len = 0;
        } else {
            return -EINVAL;
        }
        return _s->pos;
    }
    return -ENOSTR;
}

int64_t stream_tell(NCODEC* nc)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __stream* _s = (__stream*)_nc->stream;
        return _s->pos;
    }
    return -ENOSTR;
}

int32_t stream_eof(NCODEC* nc)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __stream* _s = (__stream*)_nc->stream;
        if (_s->pos < _s->len) return 0;
    }
    return 1;
}

int32_t stream_close(NCODEC* nc)
{
    UNUSED(nc);

    return 0;
}


/* Define a single stream instance. */
__stream mem_stream = {
    .s =
        (struct NCodecStreamVTable){
            .read = stream_read,
            .write = stream_write,
            .seek = stream_seek,
            .tell = stream_tell,
            .eof = stream_eof,
            .close = stream_close,
        },
    .len = 0,
    .pos = 0,
};

void trace_read(NCODEC* nc, NCodecMessage* m)
{
    UNUSED(nc);
    NCodecCanMessage* msg = m;
    printf("TRACE RX: %02d (length=%lu) (sender=%d)\n", msg->frame_id, msg->len,
        msg->sender.node_id);
}

void trace_write(NCODEC* nc, NCodecMessage* m)
{
    UNUSED(nc);
    NCodecCanMessage* msg = m;
    printf("TRACE TX: %02d (length=%lu) (sender=%d)\n", msg->frame_id, msg->len,
        msg->sender.node_id);
}

NCODEC* ncodec_open(const char* mime_type, NCodecStreamVTable* stream)
{
    UNUSED(stream);

    NCODEC* nc = ncodec_create(mime_type);
    if (nc) {
        NCodecInstance* _nc = (NCodecInstance*)nc;
        _nc->stream = (NCodecStreamVTable*)&mem_stream;
        // _nc->trace.read = trace_read;
        // _nc->trace.write = trace_write;
    }
    return nc;
}

// Modified function with configurable arguments
void importer_codec_write(uint32_t frame_id, uint8_t frame_type,
    const uint8_t* message_buffer, size_t message_len, uint8_t** out_buffer,
    size_t* out_length, const char* mime_type)
{
    NCODEC* nc = (void*)ncodec_open(mime_type, (void*)&mem_stream);

    // Write and flush the message with provided arguments
    ncodec_seek(nc, 0, NCODEC_SEEK_RESET);
    ncodec_write(nc, &(struct NCodecCanMessage){ .frame_id = frame_id,
                              .frame_type = frame_type,
                              .buffer = (uint8_t*)message_buffer,
                              .len = message_len });

    ncodec_flush(nc);

    // Seek to start, modify node_id
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    uint8_t* buffer;
    size_t   buffer_len;
    stream_read(nc, &buffer, &buffer_len, NCODEC_POS_NC);

    // Copy the buffer
    uint8_t* copy = malloc(buffer_len);
    if (!copy) {
        *out_buffer = NULL;
        *out_length = 0;
        return;
    }
    memcpy(copy, buffer, buffer_len);

    *out_buffer = copy;        // Return buffer via pointer
    *out_length = buffer_len;  // Return length via pointer
    ncodec_close(nc);
}

void importer_ncodec_read(const char* mime_type, uint8_t* data, size_t len)
{
    NCODEC* nc = (void*)ncodec_open(mime_type, (void*)&mem_stream);
    ncodec_truncate(nc);
    stream_write(nc, data, len);

    uint8_t* buffer;
    size_t   buffer_len;
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    stream_read(nc, &buffer, &buffer_len, NCODEC_POS_NC);

    while (1) {
        NCodecCanMessage msg = {};
        if (ncodec_read(nc, &msg) < 0) {
            break;
        }
        printf("Importer received binary msg: %s\n", msg.buffer);
    }
    ncodec_truncate(nc);
    ncodec_close(nc);
}

void importer_ncodec_stat(const char* mime_type, char** value)
{
    NCODEC* nc = (void*)ncodec_open(mime_type, (void*)&mem_stream);

    for (int i = 0; i < 100; i++) {  // limit to 100 cases;
        NCodecConfigItem nci = ncodec_stat(nc, &i);
        if (strcmp(nci.name, "type") == 0) {
            *value = strdup(nci.value);
            break;
        }
    }

    ncodec_close(nc);
}
