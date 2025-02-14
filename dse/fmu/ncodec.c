// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <dse/testing.h>
#include <dse/ncodec/codec.h>


/* NCodec Interface. */

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
