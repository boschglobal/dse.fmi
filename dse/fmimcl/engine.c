// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <dse/testing.h>
#include <dse/platform.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/fmimcl/fmimcl.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define STR_BUFFER    512

static size_t _count_signals(FmuModel* m)
{
    size_t count = 0;
    for (FmuSignal* s = m->signals; s && s->name; s++)
        count++;
    return count;
}


/**
fmimcl_allocate_source
======================

For each Signal parsed from the Signalgroup, this function creates an
intermediate signal object for mapping between SignalVector and FMU Variable.

Parameters
----------
fmu_model (FmuModel*)
: FMU Model descriptor object.
*/
void fmimcl_allocate_source(FmuModel* m)
{
    size_t count = _count_signals(m);
    m->data.count = count;
    m->data.name = calloc(count, sizeof(char*));
    m->data.scalar = calloc(count, sizeof(double));  // Also binary (union).
    m->data.binary_len = calloc(count, sizeof(uint32_t));
    m->data.kind = calloc(count, sizeof(MarshalKind));
    for (size_t i = 0; i < count; i++) {
        m->data.name[i] = m->signals[i].name;
        m->data.kind[i] = m->signals[i].variable_kind;
    }

    /* Set references in the MCL. */
    m->mcl.source.count = m->data.count;
    m->mcl.source.signal = m->data.name;
    m->mcl.source.scalar = m->data.scalar;
    m->mcl.source.binary_len = m->data.binary_len;
    m->mcl.source.kind = m->data.kind;
}


static MarshalGroup* _create_mg(MarshalKind kind, MarshalDir dir,
    MarshalType type, size_t count, size_t offset, FmuModel* m,
    HashList* ref_list)
{
    static char name[STR_BUFFER];
    snprintf(name, STR_BUFFER, "mg-%d-%d-%d", kind, dir, type);

    /* Target `ref` */
    assert(count == hashlist_length(ref_list));
    uint32_t* ref = calloc(count, marshal_type_size(type));
    for (size_t i = 0; i < count; i++) {
        ref[i] = *(uint32_t*)hashlist_at(ref_list, i);
    }
    hashmap_clear(&ref_list->hash_map);

    /* Target `_binary_lan` */
    uint32_t* binary_len = NULL;
    if (kind == MARSHAL_KIND_BINARY) {
        binary_len = calloc(count, sizeof(uint32_t));
    }

    /* Functions */
    MarshalStringEncode* string_encode = NULL;
    MarshalStringDecode* string_decode = NULL;
    if (kind == MARSHAL_KIND_BINARY) {
        string_encode = calloc(count, sizeof(MarshalStringEncode));
        string_decode = calloc(count, sizeof(MarshalStringDecode));
    }

    /* Construct the MarshalGroup. */
    MarshalGroup* mg = malloc(sizeof(MarshalGroup));
    *mg = (MarshalGroup){
        .name = strdup(name),
        .kind = kind,
        .dir = dir,
        .type = type,
        .count = count,
        .target.ref = ref,
        .target.ptr = calloc(count, marshal_type_size(type)),
        .target._binary_len = binary_len,
        .source.offset = offset,
        .source.scalar = m->data.scalar,
        .source.binary_len = m->data.binary_len,
        .functions.string_encode = string_encode,
        .functions.string_decode = string_decode,
    };
    return mg;
}


/**
fmimcl_generate_marshal_table
=============================

The FMU Signals are sorted according to the marshal groups. A source
vector is already allocated of N signals. This function will create
a marshal table, which is a mapping from the vector to a sequential list
of signal blocks, each representing a marshal group.

Parameters
----------
fmu_model (FmuModel*)
: FMU Model descriptor object.
*/
void fmimcl_generate_marshal_table(FmuModel* m)
{
    HashList mg_list;
    hashlist_init(&mg_list, 100);
    size_t   offset = 0;
    size_t   count = 0;
    HashList ref_list;
    hashlist_init(&ref_list, 100);

    /* Loop each signal, when the marshal group changes (kind, dir, type) then
    emit a MarshalGroup. */
    MarshalKind kind = MARSHAL_KIND_NONE;
    MarshalDir  dir = MARSHAL_DIRECTION_NONE;
    MarshalType type = MARSHAL_TYPE_NONE;
    for (FmuSignal* s = m->signals; s && s->name; s++) {
        /* MarshalGroup changed? */
        if (kind != s->variable_kind || dir != s->variable_dir ||
            type != s->variable_type) {
            /* Emit the previous MarshalGroup (that was being accumulated). */
            if (count) {
                hashlist_append(&mg_list,
                    _create_mg(kind, dir, type, count, offset, m, &ref_list));
            }
            /* Reset the conditions, based on the current signal. */
            offset = offset + count;
            kind = s->variable_kind;
            dir = s->variable_dir;
            type = s->variable_type;
            count = 1;
            hashlist_append(&ref_list, &s->variable_vref);
        } else {
            count++;
            hashlist_append(&ref_list, &s->variable_vref);
        }
    }
    if (count) {
        /* Emit the last MarshalGroup. */
        hashlist_append(
            &mg_list, _create_mg(kind, dir, type, count, offset, m, &ref_list));
    }
    /* Convert to a NTL. */
    m->data.mg_table = hashlist_ntl(&mg_list, sizeof(MarshalGroup), true);

    /* Cleanup. */
    hashlist_destroy(&ref_list);
}


extern char* dse_ascii85_encode(const char* source, size_t len);
extern char* dse_ascii85_decode(const char* source, size_t* len);

/**
fmimcl_load_encoder_funcs
=========================

Parse the MarshalGroup NTL and for each Kind which supports an encoder
function attempt to load the configured encoder functions to:

*    .functions.string_encode
*    .functions.string_decode

Parameters
----------
fmu_model (FmuModel*)
: FMU Model descriptor object.
*/
void fmimcl_load_encoder_funcs(FmuModel* m)
{
    for (MarshalGroup* mg = m->data.mg_table; mg && mg->name; mg++) {
        switch (mg->kind) {
        case MARSHAL_KIND_BINARY:
            if (mg->functions.string_encode == NULL) continue;
            if (mg->functions.string_decode == NULL) continue;
            for (size_t i = 0; i < mg->count; i++) {
                FmuSignal* s = &m->signals[mg->source.offset + i];
                if (s->variable_annotation_encoding) {
                    if (strcmp("ascii85", s->variable_annotation_encoding) ==
                        0) {
                        mg->functions.string_encode[i] = dse_ascii85_encode;
                        mg->functions.string_decode[i] = dse_ascii85_decode;
                    }
                }
            }
            break;
        default:
            break;
        }
    }
}
