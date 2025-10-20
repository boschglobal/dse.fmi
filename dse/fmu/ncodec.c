// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/strings.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/frame.h>
#include <dse/ncodec/interface/pdu.h>
#include <dse/fmu/fmu.h>


#define UNUSED(x)      ((void)x)


/*
Trace Interface.
*/
#define NCT_BUFFER_LEN 2000
#define NCT_ENVVAR_LEN 100
#define NCT_ID_LEN     100
#define NCT_KEY_LEN    20


typedef struct NCodecTraceData {
    const char*      model_inst_name;
    FmuInstanceData* fmu;
    char             identifier[NCT_ID_LEN];
    /* Filters. */
    bool             wildcard;
    HashMap          filter;
} NCodecTraceData;


static void __log(NCodecInstance* nc, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* Log to console. */
    fprintf(stdout, message);
    fprintf(stdout, "\n");
    fflush(stdout);

    /* Log to FMU interface. */
    if (nc == NULL || nc->private == NULL) return;
    NCodecTraceData* td = nc->private;
    fmu_log(td->fmu, FmiLogOk, "Debug", message);
}


static const char* __get_codec_config(NCodecInstance* nc, const char* name)
{
    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem ci = ncodec_stat((void*)nc, &i);
        if (ci.name && (strcmp(ci.name, name) == 0)) {
            return ci.value;
        }
    }
    return NULL;
}


static void trace_can_log(
    NCodecInstance* nc, NCodecMessage* m, const char* direction)
{
    NCodecTraceData*  td = nc->private;
    NCodecCanMessage* msg = m;
    static char       b[NCT_BUFFER_LEN];
    static char       identifier[NCT_ID_LEN];

    /* Setup bus identifier (on first call). */
    if (strlen(td->identifier) == 0) {
        snprintf(td->identifier, NCT_ID_LEN, "%s:%s:%s",
            __get_codec_config(nc, "bus_id"), __get_codec_config(nc, "node_id"),
            __get_codec_config(nc, "interface_id"));
    }

    /* Filter the message. */
    if (td->wildcard == false) {
        char key[NCT_KEY_LEN];
        snprintf(key, NCT_KEY_LEN, "%u", msg->frame_id);
        if (hashmap_get(&td->filter, key) == NULL) return;
    }

    /* Format and write the log. */
    memset(b, 0, NCT_BUFFER_LEN);
    if (strcmp(direction, "RX") == 0) {
        snprintf(identifier, NCT_ID_LEN, "%d:%d:%d", msg->sender.bus_id,
            msg->sender.node_id, msg->sender.interface_id);
    } else {
        strncpy(identifier, td->identifier, NCT_ID_LEN);
    }
    if (msg->len <= 16) {
        // Short form log.
        for (uint32_t i = 0; i < msg->len; i++) {
            if (i && (i % 8 == 0)) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", msg->buffer[i]);
        }
    } else {
        // Long form log.
        for (uint32_t i = 0; i < msg->len; i++) {
            if (strlen(b) > NCT_BUFFER_LEN) break;
            if (i % 32 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), "\n ");
            }
            if (i % 8 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", msg->buffer[i]);
        }
    }
    __log(nc, "(%s) [%s] %s %02x %d %lu :%s", td->model_inst_name, identifier,
        direction, msg->frame_id, msg->frame_type, msg->len, b);
}

static void trace_can_read(NCODEC* nc, NCodecMessage* m)
{
    trace_can_log((NCodecInstance*)nc, m, "RX");
}

static void trace_can_write(NCODEC* nc, NCodecMessage* m)
{
    trace_can_log((NCodecInstance*)nc, m, "TX");
}


static void trace_pdu_log(
    NCodecInstance* nc, NCodecMessage* m, const char* direction)
{
    NCodecTraceData* td = nc->private;
    NCodecPdu*       pdu = m;
    static char      b[NCT_BUFFER_LEN];
    static char      identifier[NCT_ID_LEN];

    /* Setup bus identifier (on first call). */
    if (strlen(td->identifier) == 0) {
        snprintf(td->identifier, NCT_ID_LEN, "%s:%s",
            __get_codec_config(nc, "swc_id"), __get_codec_config(nc, "ecu_id"));
    }

    /* Filter the message. */
    if (td->wildcard == false) {
        char key[NCT_KEY_LEN];
        snprintf(key, NCT_KEY_LEN, "%u", pdu->id);
        if (hashmap_get(&td->filter, key) == NULL) return;
    }

    /* Format and write the log. */
    memset(b, 0, NCT_BUFFER_LEN);
    if (strcmp(direction, "RX") == 0) {
        snprintf(identifier, NCT_ID_LEN, "%d:%d", pdu->swc_id, pdu->ecu_id);
    } else {
        strncpy(identifier, td->identifier, NCT_ID_LEN);
    }
    if (pdu->payload_len <= 16) {
        // Short form log.
        for (uint32_t i = 0; i < pdu->payload_len; i++) {
            if (i && (i % 8 == 0)) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", pdu->payload[i]);
        }
    } else {
        // Long form log.
        for (uint32_t i = 0; i < pdu->payload_len; i++) {
            if (strlen(b) > NCT_BUFFER_LEN) break;
            if (i % 32 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), "\n ");
            }
            if (i % 8 == 0) {
                snprintf(b + strlen(b), sizeof(b) - strlen(b), " ");
            }
            snprintf(
                b + strlen(b), sizeof(b) - strlen(b), " %02x", pdu->payload[i]);
        }
    }
    __log(nc, "(%s) [%s] %s %02x %lu :%s", td->model_inst_name, identifier,
        direction, pdu->id, pdu->payload_len, b);

    // Transport
    switch (pdu->transport_type) {
    case NCodecPduTransportTypeCan: {
        // CAN
        __log(nc,
            "    CAN:    frame_format=%d  frame_type=%d  interface_id=%d  "
            "network_id=%d",
            pdu->transport.can_message.frame_format,
            pdu->transport.can_message.frame_type,
            pdu->transport.can_message.interface_id,
            pdu->transport.can_message.network_id);
    } break;
    case NCodecPduTransportTypeIp: {
        // ETH
        __log(nc, "    ETH:    src_mac=%016lx  dst_mac=%016lx",
            pdu->transport.ip_message.eth_src_mac,
            pdu->transport.ip_message.eth_dst_mac);
        __log(nc,
            "    ETH:    ethertype=%04x  tci_pcp=%02x  tci_dei=%02x  "
            "tci_vid=%04x",
            pdu->transport.ip_message.eth_ethertype,
            pdu->transport.ip_message.eth_tci_pcp,
            pdu->transport.ip_message.eth_tci_dei,
            pdu->transport.ip_message.eth_tci_vid);

        // IP
        switch (pdu->transport.ip_message.ip_addr_type) {
        case NCodecPduIpAddrIPv4: {
            __log(nc, "    IP:    src_addr=%08x  dst_addr=%08x",
                pdu->transport.ip_message.ip_addr.ip_v4.src_addr,
                pdu->transport.ip_message.ip_addr.ip_v4.dst_addr);
        } break;
        case NCodecPduIpAddrIPv6: {
            __log(nc,
                "    IP:     src_addr=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[0],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[1],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[2],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[3],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[4],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[5],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[6],
                pdu->transport.ip_message.ip_addr.ip_v6.src_addr[7]);
            __log(nc,
                "    IP:     dst_addr=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[0],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[1],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[2],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[3],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[4],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[5],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[6],
                pdu->transport.ip_message.ip_addr.ip_v6.dst_addr[7]);
        } break;
        default:
            break;
        }
        __log(nc, "    IP:     src_port=%04x  dst_port=%04x  proto=%d",
            pdu->transport.ip_message.ip_src_port,
            pdu->transport.ip_message.ip_dst_port,
            pdu->transport.ip_message.ip_protocol);

        // Socket Adapter
        switch (pdu->transport.ip_message.so_ad_type) {
        case NCodecPduSoAdDoIP: {
            __log(nc, "    DOIP:   protocol_version=%d  payload_type=%d",
                pdu->transport.ip_message.so_ad.do_ip.protocol_version,
                pdu->transport.ip_message.so_ad.do_ip.payload_type);
        } break;
        case NCodecPduSoAdSomeIP: {
            __log(nc, "    SOMEIP: protocol_version=%d  interface_version=%d",
                pdu->transport.ip_message.so_ad.some_ip.protocol_version,
                pdu->transport.ip_message.so_ad.some_ip.interface_version);
            __log(nc, "    SOMEIP: request_id=%d  return_code=%d",
                pdu->transport.ip_message.so_ad.some_ip.request_id,
                pdu->transport.ip_message.so_ad.some_ip.return_code);
            __log(nc, "    SOMEIP: message_type=%d  message_id=%d  length=%d",
                pdu->transport.ip_message.so_ad.some_ip.message_type,
                pdu->transport.ip_message.so_ad.some_ip.message_id,
                pdu->transport.ip_message.so_ad.some_ip.length);
        } break;
        default:
            break;
        }
    } break;
    default:
        break;
    }
}

static void trace_pdu_read(NCODEC* nc, NCodecMessage* m)
{
    trace_pdu_log((NCodecInstance*)nc, m, "RX");
}

static void trace_pdu_write(NCODEC* nc, NCodecMessage* m)
{
    trace_pdu_log((NCodecInstance*)nc, m, "TX");
}


static void trace_configure(NCodecInstance* nc, FmuInstanceData* fmu)
{
    bool        type_can = false;
    bool        type_pdu = false;
    const char* codec_type = __get_codec_config(nc, "type");
    if (codec_type == NULL) return;
    if (strcmp(codec_type, "frame") == 0) type_can = true;
    if (strcmp(codec_type, "pdu") == 0) type_pdu = true;

    /* Query the environment variable. */
    char env_name[NCT_ENVVAR_LEN];
    if (type_can) {
        snprintf(env_name, NCT_ENVVAR_LEN, "NCODEC_TRACE_%s_%s",
            __get_codec_config(nc, "bus"), __get_codec_config(nc, "bus_id"));
    } else if (type_pdu) {
        snprintf(env_name, NCT_ENVVAR_LEN, "NCODEC_TRACE_PDU_%s",
            __get_codec_config(nc, "swc_id"));
    } else {
        return;
    }
    for (int i = 0; env_name[i]; i++)
        env_name[i] = toupper(env_name[i]);
    char* filter = getenv(env_name);
    if (filter == NULL) return;

    /* Setup the trace data. */
    NCodecTraceData* td = calloc(1, sizeof(NCodecTraceData));

    td->model_inst_name = fmu->instance.name;
    td->fmu = fmu;
    hashmap_init(&td->filter);

    if (strcmp(filter, "*") == 0) {
        td->wildcard = true;
        __log(nc, "    <wildcard> (all frames)");
    } else {
        char* _filter = strdup(filter);
        char* _saveptr = NULL;
        char* _idptr = strtok_r(_filter, ",", &_saveptr);
        while (_idptr) {
            int64_t _id = strtol(_idptr, NULL, 0);
            if (_id > 0) {
                char key[NCT_KEY_LEN];
                snprintf(key, NCT_KEY_LEN, "%u", (uint32_t)_id);
                hashmap_set_long(&td->filter, key, true);
                __log(nc, "    %02x", (unsigned int)_id);
            }
            _idptr = strtok_r(NULL, ",", &_saveptr);
        }
        free(_filter);
    }

    /* Install the trace. */
    if (type_can) {
        nc->trace.write = trace_can_write;
        nc->trace.read = trace_can_read;
        nc->private = td;
    } else if (type_pdu) {
        nc->trace.write = trace_pdu_write;
        nc->trace.read = trace_pdu_read;
        nc->private = td;
    }
}


static void trace_destroy(NCodecInstance* nc)
{
    if (nc->private) {
        NCodecTraceData* td = nc->private;
        hashmap_destroy(&td->filter);
        free(td);
        nc->private = NULL;
    }
}


/*
Stream Interface for binary signals (supports NCodec).
*/
typedef struct __BinarySignalStream {
    NCodecStreamVTable s;
    /**/
    FmuSignalVector*   sv;
    uint32_t           idx;
    uint32_t           pos;
} __BinarySignalStream;

static void fmu_sv_stream_destroy(void* stream)
{
    if (stream) free(stream);
}

static size_t stream_read(
    NCODEC* nc, uint8_t** data, size_t* len, int32_t pos_op)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc == NULL || _nc->stream == NULL) return -ENOSTR;
    if (data == NULL || len == NULL) return -EINVAL;

    __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
    uint32_t              s_len = _s->sv->length[_s->idx];
    uint8_t*              s_buffer = _s->sv->binary[_s->idx];

    /* Check if any buffer. */
    if (s_buffer == NULL) {
        *data = NULL;
        *len = 0;
        return 0;
    }
    /* Check EOF. */
    if (_s->pos >= s_len) {
        *data = NULL;
        *len = 0;
        return 0;
    }
    /* Return buffer, from current pos. */
    *data = &s_buffer[_s->pos];
    *len = s_len - _s->pos;
    /* Advance the position indicator. */
    if (pos_op == NCODEC_POS_UPDATE) _s->pos = s_len;

    return *len;
}

static size_t stream_write(NCODEC* nc, uint8_t* data, size_t len)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc == NULL || _nc->stream == NULL) return -ENOSTR;

    __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
    uint32_t              s_len = _s->sv->length[_s->idx];

    /* Write from current pos (i.e. truncate). */
    if (_s->pos > s_len) _s->pos = s_len;
    _s->sv->length[_s->idx] = _s->pos;
    dse_buffer_append(&_s->sv->binary[_s->idx], &_s->sv->length[_s->idx],
        &_s->sv->buffer_size[_s->idx], data, len);
    _s->pos += len;

    return len;
}

static int64_t stream_seek(NCODEC* nc, size_t pos, int32_t op)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
        uint32_t              s_len = _s->sv->length[_s->idx];

        if (op == NCODEC_SEEK_SET) {
            if (pos > s_len) {
                _s->pos = s_len;
            } else {
                _s->pos = pos;
            }
        } else if (op == NCODEC_SEEK_CUR) {
            pos = _s->pos + pos;
            if (pos > s_len) {
                _s->pos = s_len;
            } else {
                _s->pos = pos;
            }
        } else if (op == NCODEC_SEEK_END) {
            _s->pos = s_len;
        } else if (op == NCODEC_SEEK_RESET) {
            /* Reset before stream_write has truncate effect. */
            _s->pos = 0;
            _s->sv->length[_s->idx] = 0;
        } else {
            return -EINVAL;
        }
        return _s->pos;
    }
    return -ENOSTR;
}

static int64_t stream_tell(NCODEC* nc)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
        return _s->pos;
    }
    return -ENOSTR;
}

static int32_t stream_eof(NCODEC* nc)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        __BinarySignalStream* _s = (__BinarySignalStream*)_nc->stream;
        uint32_t              s_len = _s->sv->length[_s->idx];

        if (_s->pos < s_len) return 0;
    }
    return 1;
}

static int32_t stream_close(NCODEC* nc)
{
    NCodecInstance* _nc = (NCodecInstance*)nc;
    if (_nc && _nc->stream) {
        fmu_sv_stream_destroy(_nc->stream);
        _nc->stream = NULL;
    }
    return 0;
}

static void* fmu_sv_stream_create(FmuSignalVector* sv, uint32_t idx)
{
    __BinarySignalStream* stream = calloc(1, sizeof(__BinarySignalStream));
    stream->s = (struct NCodecStreamVTable){
        .read = stream_read,
        .write = stream_write,
        .seek = stream_seek,
        .tell = stream_tell,
        .eof = stream_eof,
        .close = stream_close,
    };
    stream->sv = sv;
    stream->idx = idx;
    stream->pos = 0;

    return stream;
}


/*
NCodec Interface.
*/

NCODEC* ncodec_open(const char* mime_type, NCodecStreamVTable* stream)
{
    if (mime_type == NULL) {
        errno = EINVAL;
        return NULL;
    }
    NCODEC* nc = ncodec_create(mime_type);
    if (nc == NULL || stream == NULL) {
        errno = EINVAL;
        return NULL;
    }
    NCodecInstance* _nc = (NCodecInstance*)nc;
    _nc->stream = stream;
    return nc;
}


/**
fmu_ncodec_open
===============

Open and configure an NCODEC object.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
mime_type (const char*)
: MIME Type string defining the NCODEC object.
idx (FmuSignalVectorIndex*)
: Index object referencing the Signal Vector (i.e. variable) which the NCODEC
object will reference.

Returns
-------
NCODEC (pointer)
: Pointer to the configured NCODEC object.
*/
void* fmu_ncodec_open(
    FmuInstanceData* fmu, const char* mime_type, FmuSignalVectorIndex* idx)
{
    assert(fmu);
    assert(idx);
    void*   stream = fmu_sv_stream_create(idx->sv, idx->vi);
    NCODEC* nc = ncodec_open(mime_type, stream);
    if (nc) {
        trace_configure((NCodecInstance*)nc, fmu);
    } else {
        fmu_sv_stream_destroy(stream);
    }
    return nc;
}


/**
fmu_ncodec_close
================

Close an NCODEC object.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
ncodec (void*)
: NCODEC object.
*/
void fmu_ncodec_close(FmuInstanceData* fmu, void* ncodec)
{
    assert(fmu);
    NCodecInstance* nc = ncodec;
    if (nc == NULL) return;

    trace_destroy(nc);
    // fmu_sv_stream_destroy(nc->stream);
    ncodec_close((NCODEC*)nc);
}
