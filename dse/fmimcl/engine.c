// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
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
fmimcl_allocate_scalar_source
=============================

For each Signal parsed from the Signalgroup, this function creates an
intermediate signal object for mapping between SignalVector and FMU Variable.

Parameters
----------
fmu_model (FmuModel*)
: FMU Model descriptor object.
*/
void fmimcl_allocate_scalar_source(FmuModel* m)
{
    size_t count = _count_signals(m);
    m->data.scalar = calloc(count, sizeof(double));
    m->data.count = count;
    m->data.name = calloc(count, sizeof(char*));
    for (size_t i = 0; i < count; i++) {
        m->data.name[i] = m->signals[i].name;
    }
    m->mcl.source.scalar = &(m->data.scalar);
    m->mcl.source.count = &m->data.count;
    m->mcl.source.signal = m->data.name;
}


static MarshalGroup* _create_mg(MarshalKind kind, MarshalDir dir,
    MarshalType type, size_t count, size_t offset, FmuModel* m,
    HashList* ref_list)
{
    static char name[STR_BUFFER];
    snprintf(name, STR_BUFFER, "mg-%d-%d-%d", kind, dir, type);

    assert(count == hashlist_length(ref_list));
    uint32_t* ref = calloc(count, marshal_type_size(type));
    for (size_t i = 0; i < count; i++) {
        ref[i] = *(uint32_t*)hashlist_at(ref_list, i);
    }
    hashmap_clear(&ref_list->hash_map);

    MarshalGroup* mg = malloc(sizeof(MarshalGroup));
    *mg = (MarshalGroup){
        .name = strdup(name),
        .kind = kind,
        .dir = dir,
        .type = type,
        .count = count,
        .target.ref = ref,
        .target.ptr = calloc(count, marshal_type_size(type)),
        .source.offset = offset,
        .source.scalar = m->data.scalar,
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
    count = hashlist_length(&mg_list);
    m->data.mg_table = calloc(count + 1, sizeof(MarshalGroup));
    for (uint32_t i = 0; i < count; i++) {
        memcpy(&m->data.mg_table[i], hashlist_at(&mg_list, i),
            sizeof(MarshalGroup));
        free(hashlist_at(&mg_list, i));
    }
    hashlist_destroy(&mg_list);

    /* Cleanup. */
    hashlist_destroy(&ref_list);
}
