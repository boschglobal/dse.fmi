// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/frame.h>
#include <dse/ncodec/stream/stream.h>


#define UNUSED(x)  ((void)x)
#define BUFFER_LEN 1024


void* mem_stream = NULL;


void trace_read(NCODEC* nc, NCodecMessage* m)
{
    UNUSED(nc);
    NCodecCanMessage* msg = m;
    printf("TRACE RX: %02d (length=%zu) (sender=%d)\n", msg->frame_id, msg->len,
        msg->sender.node_id);
}

void trace_write(NCODEC* nc, NCodecMessage* m)
{
    UNUSED(nc);
    NCodecCanMessage* msg = m;
    printf("TRACE TX: %02d (length=%zu) (sender=%d)\n", msg->frame_id, msg->len,
        msg->sender.node_id);
}

NCODEC* ncodec_open(const char* mime_type, NCodecStreamVTable* stream)
{
    UNUSED(stream);

    if (mem_stream == NULL) {
        mem_stream = ncodec_buffer_stream_create(0);
    }

    NCODEC* nc = ncodec_create(mime_type);
    if (nc) {
        NCodecInstance* _nc = (NCodecInstance*)nc;
        _nc->stream = (NCodecStreamVTable*)mem_stream;
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
    NCODEC*         nc = (void*)ncodec_open(mime_type, mem_stream);
    NCodecInstance* _nc = (NCodecInstance*)nc;

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
    _nc->stream->read(nc, &buffer, &buffer_len, NCODEC_POS_NC);

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
    NCODEC*         nc = (void*)ncodec_open(mime_type, mem_stream);
    NCodecInstance* _nc = (NCodecInstance*)nc;
    ncodec_truncate(nc);
    _nc->stream->write(nc, data, len);

    uint8_t* buffer;
    size_t   buffer_len;
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    _nc->stream->read(nc, &buffer, &buffer_len, NCODEC_POS_NC);

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
    NCODEC* nc = (void*)ncodec_open(mime_type, mem_stream);

    for (int i = 0; i < 100; i++) {  // limit to 100 cases;
        NCodecConfigItem nci = ncodec_stat(nc, &i);
        if (strcmp(nci.name, "type") == 0) {
            *value = strdup(nci.value);
            break;
        }
    }

    ncodec_close(nc);
}
