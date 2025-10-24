// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdio.h>
#include <dse/clib/collections/vector.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/frame.h>
#include <dse/ncodec/stream/stream.h>
#include <dse/logger.h>

#define UNUSED(x) ((void)x)

bool signal_bus_enabled = false;


typedef struct NetworkSignal {
    const char* name;
    const char* mime_type;
    NCODEC*     nc;
} NetworkSignal;

int NetworkSignalCompar(const void* left, const void* right)
{
    const NetworkSignal* l = left;
    const NetworkSignal* r = right;
    return strcmp(l->name, r->name);
}

static Vector ns_v = { 0 }; /* NetworkSignal */


static NetworkSignal* _get_network_signal(
    const char* name, const char* mime_type)
{
    if (ns_v.capacity == 0) {
        ns_v = vector_make(sizeof(NetworkSignal), 0, NetworkSignalCompar);
    }
    NetworkSignal* ns =
        vector_find(&ns_v, &(NetworkSignal){ .name = name }, 0, NULL);
    if (ns == NULL) {
        if (mime_type == NULL) return NULL;
        NCodecInstance* nc = ncodec_create(mime_type);
        if (nc) nc->stream = ncodec_buffer_stream_create(1024);
        vector_push(&ns_v,
            &(NetworkSignal){ .name = name, .mime_type = mime_type, .nc = nc });
        vector_sort(&ns_v);
    }
    return vector_find(&ns_v, &(NetworkSignal){ .name = name }, 0, NULL);
}

static void _network_signal_close(void* item, void* data)
{
    UNUSED(data);
    NetworkSignal* ns = item;
    if (ns->nc) {
        ncodec_close(ns->nc);
        ns->nc = NULL;
    }
}


char* network_mime_type_value(const char* mime_type, const char* key)
{
    NCODEC* nc = ncodec_create(mime_type);
    char*   value = NULL;

    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem nci = ncodec_stat(nc, &i);
        if (nci.name && strcmp(nci.name, key) == 0) {
            value = strdup(nci.value);
            break;
        }
    }

    ncodec_close(nc);
    return value;
}


void network_inject_frame(const char* name, const char* mime_type, int32_t id,
    uint8_t* data, size_t len)
{
    /* Locate the network signal. */
    NetworkSignal* ns = _get_network_signal(name, mime_type);
    if (ns == NULL || ns->nc == NULL) return;

    /* Append a frame to the NC. */
    NCodecInstance* nc = ns->nc;
    ncodec_seek(nc, 0, NCODEC_SEEK_END);
    ncodec_write(nc, &(struct NCodecCanMessage){ .frame_id = id,
                         .frame_type = CAN_FD_BASE_FRAME,
                         .buffer = data,
                         .len = len });
    ncodec_flush(nc);
}


void network_push(
    const char* name, const char* mime_type, uint8_t* buffer, size_t len)
{
    if (buffer == NULL || len == 0) return;

    /* Locate the network signal. */
    NetworkSignal* ns = _get_network_signal(name, mime_type);
    if (ns == NULL || ns->nc == NULL) return;

    /* Append buffer to the underlying NCodec stream. */
    if (signal_bus_enabled) {
        /* Only perform Tx -> Rx if signal bus enabled. */
        NCodecInstance* nc = ns->nc;
        ncodec_seek(nc, 0, NCODEC_SEEK_END);
        nc->stream->write(nc, buffer, len);
    } else {
        char* type = network_mime_type_value((char*)mime_type, "type");
        if (strcmp(type, "frame") == 0) {
            NCodecInstance* nc = ncodec_create(mime_type);
            if (nc == NULL) return;
            nc->stream = ncodec_buffer_stream_create(len);
            nc->stream->write(nc, buffer, len);
            ncodec_seek(nc, 0, NCODEC_SEEK_SET);
            NCodecCanMessage msg = {};
            while (ncodec_read(nc, &msg) >= 0) {
                printf("Importer: network message (RX): %s\n", msg.buffer);
            }
            ncodec_close(nc);
        }
        free(type);
    }
}


void network_pull(
    const char* name, const char* mime_type, uint8_t** buffer, size_t* len)
{
    if (buffer == NULL || len == NULL) return;
    *buffer = NULL;
    *len = 0;

    /* Locate the network signal. */
    NetworkSignal* ns = _get_network_signal(name, mime_type);
    if (ns == NULL || ns->nc == NULL) return;

    /* Copy and return the underlying NCodec stream. */
    NCodecInstance* nc = ns->nc;
    ncodec_seek(nc, 0, NCODEC_SEEK_SET);
    uint8_t* stream;
    nc->stream->read(nc, &stream, len, NCODEC_POS_NC);
    if (*len) {
        *buffer = malloc(*len); /* Caller to free. */
        memcpy(*buffer, stream, *len);
    }
}


void network_truncate(void)
{
    for (size_t i = 0; i < ns_v.length; i++) {
        NetworkSignal* ns = vector_at(&ns_v, i, NULL);
        if (ns && ns->nc) {
            ncodec_truncate(ns->nc);
        }
    }
}


void network_close(void)
{
    vector_clear(&ns_v, _network_signal_close, NULL);
}
