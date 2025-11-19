// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <libxml/xpath.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/ncodec/codec.h>
#include <dse/fmu/fmu.h>


#define UNUSED(x) ((void)x)


extern size_t fmu_variable_count(xmlDoc* doc, bool is_binary);
extern void   fmu_variable_index(
      xmlDoc* doc, FmuInstanceData* fmu, FmuSignalVector* sv, bool is_binary);
extern void fmu_sv_stream_destroy(void* stream);


/**
fmu_signals_reset
=================

This method will reset any binary variables which where used by an FMU in the
previous step. Typically this will mean that indexes into the buffers of
binary variables are set to 0, however the buffers themselves are
not released (i.e. free() is not called).

> Integrators may provide their own implementation of this method.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

*/
extern void fmu_signals_reset(FmuInstanceData* fmu);

/**
fmu_signals_setup
=================

This method will setup the buffers which provide storage for FMU variables.
Depending on the implementation buffers may be mapped to existing buffers
in the implementation, or allocated specifically. When allocating buffers the
method `fmu_signals_setup()` should also be implemented to release those buffers
when the `FmuInstanceData()` is freed.

> Integrators may provide their own implementation of this method.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
extern void fmu_signals_setup(FmuInstanceData* fmu);

/**
fmu_signals_remove
==================

This method will remove any buffers used to provide storage for FMU variables.
If those buffers were allocated (e.g by an implementation
of `fmu_signals_setup()`) then those buffers should be freed in this method.

> Integrators may provide their own implementation of this method.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
extern void fmu_signals_remove(FmuInstanceData* fmu);


/**
fmu_register_var
================

Register a variable with the FMU Variable Table mechanism.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
vref (uint32_t)
: Variable reference of the variable being registered.
input (bool)
: Set `true` for input, and `false` for output variable causality.
offset (size_t)
: Offset of the variable (type double) in the FMU provided variable table.

Returns
-------
start_value (double)
: The configured FMU Variable start value, or 0.
*/
double fmu_register_var(
    FmuInstanceData* fmu, uint32_t vref, bool input, size_t offset)
{
    double* signal = NULL;
    char    key[HASHLIST_KEY_LEN];

    /* Lookup the signal. */
    snprintf(key, HASHLIST_KEY_LEN, "%i", vref);
    if (input) {
        signal = hashmap_get(&fmu->variables.scalar.input, key);
    } else {
        signal = hashmap_get(&fmu->variables.scalar.output, key);
    }
    if (signal == NULL) return 0;

    /* Create the marshal list. */
    FmuVarTableMarshalItem* mi = malloc(sizeof(FmuVarTableMarshalItem));
    *mi = (FmuVarTableMarshalItem){
        .variable = (void*)offset,  // Corrected in fmu_register_var_table.
        .signal = signal,
    };
    if (fmu->var_table.var_list.hash_map.hash_function == NULL) {
        hashlist_init(&fmu->var_table.var_list, 128);
    }
    hashlist_append(&fmu->var_table.var_list, mi);
    return 0;
}


/**
fmu_lookup_ncodec
=================

Lookup and existing NCODEC object which represents a binary (or string)
variable of the FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
vref (uint32_t)
: Variable reference of the variable with an associated NCODEC object.
input (bool)
: Set `true` for input, and `false` for output variable causality.

Returns
-------
void* (NCODEC pointer)
: A valid NCODEC object for the underlying variable.
*/
void* fmu_lookup_ncodec(FmuInstanceData* fmu, uint32_t vref, bool input)
{
    FmuSignalVectorIndex* idx = NULL;
    char                  key[HASHLIST_KEY_LEN];

    /* Lookup the signal. */
    snprintf(key, HASHLIST_KEY_LEN, "%i", vref);
    if (input) {
        idx = hashmap_get(&fmu->variables.binary.rx, key);
    } else {
        idx = hashmap_get(&fmu->variables.binary.tx, key);
    }
    if (idx == NULL) return NULL;

    /* Return the NCODEC object pointer. */
    return idx->sv->ncodec[idx->vi];
}


/**
fmu_register_var_table
======================

Register the Variable Table. The previously registered variables, via calls to
`fmu_register_var`, are configured and the FMU Variable Table mechanism
is enabled.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
table (void*)
: Pointer to the Variable Table being registered.
*/
void fmu_register_var_table(FmuInstanceData* fmu, void* table)
{
    fmu->var_table.table = table;
    fmu->var_table.marshal_list = hashlist_ntl(
        &fmu->var_table.var_list, sizeof(FmuVarTableMarshalItem), true);
    for (FmuVarTableMarshalItem* mi = fmu->var_table.marshal_list;
        mi && mi->signal; mi++) {
        /* Correct the variable pointer offset, to vt base. */
        mi->variable = (double*)(table + (size_t)mi->variable);
    }
}


/**
fmu_var_table
=============

Return a reference to the previously registered Variable Table.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

Returns
-------
table (void*)
: Pointer to the Variable Table.
*/
void* fmu_var_table(FmuInstanceData* fmu)
{
    return fmu->var_table.table;
}


static FmuSignalVector* __allocate_sv(xmlDoc* doc, bool is_binary)
{
    size_t count = fmu_variable_count(doc, is_binary);
    if (count == 0) return NULL;

    FmuSignalVector* sv = calloc(1, sizeof(FmuSignalVector));
    sv->count = count;
    sv->signal = calloc(count, sizeof(char*));
    if (is_binary) {
        sv->binary = calloc(count, sizeof(void*));
        sv->length = calloc(count, sizeof(uint32_t));
        sv->buffer_size = calloc(count, sizeof(uint32_t));
        sv->mime_type = calloc(count, sizeof(char*));
        sv->ncodec = calloc(count, sizeof(void*));
    } else {
        sv->scalar = calloc(count, sizeof(double));
    }
    return sv;
}


static void fmu_default_signals_reset(FmuInstanceData* fmu)
{
    assert(fmu);

    if (fmu->variables.signals_reset == false) {
        for (FmuSignalVector* sv = fmu->data; sv && sv->signal; sv++) {
            if (sv->binary == NULL) continue;
            for (uint32_t i = 0; i < sv->count; i++) {
                if (sv->ncodec[i]) {
                    ncodec_truncate(sv->ncodec[i]);
                } else {
                    sv->length[i] = 0;
                }
            }
        }
        fmu->variables.signals_reset = true;
    }
}


static void fmu_default_signals_setup(FmuInstanceData* fmu)
{
    char     xml_path[PATH_MAX];
    HashList sv_list;
    hashlist_init(&sv_list, 10);

    snprintf(xml_path, PATH_MAX, "%s/../modelDescription.xml",
        fmu->instance.resource_location);
    xmlInitParser();
    xmlDocPtr doc = xmlParseFile(xml_path);
    if (doc == NULL) {
        fprintf(stderr, "Document not parsed successfully.\n");
        return;
    }

    /* Setup scalar variables. */
    FmuSignalVector* scalar_sv = __allocate_sv(doc, false);
    if (scalar_sv) hashlist_append(&sv_list, scalar_sv);

    /* Setup binary variables. */
    FmuSignalVector* binary_sv = __allocate_sv(doc, true);
    if (binary_sv) hashlist_append(&sv_list, binary_sv);

    /* Complete and store the signal vectors. */
    FmuSignalVector* sv = hashlist_ntl(&sv_list, sizeof(FmuSignalVector), true);
    for (FmuSignalVector* _sv = sv; _sv && _sv->signal; _sv++) {
        fmu_variable_index(doc, fmu, _sv, _sv->scalar ? false : true);
    }

    fmu->data = sv;
    xmlFreeDoc(doc);
}


static void fmu_default_signals_remove(FmuInstanceData* fmu)
{
    if (fmu->data == NULL) return;
    for (FmuSignalVector* sv = fmu->data; sv && sv->signal; sv++) {
        if (sv->signal) {
            for (uint32_t i = 0; i < sv->count; i++) {
                free(sv->signal[i]);
            }
            free(sv->signal);
        }
        if (sv->ncodec) {
            for (uint32_t i = 0; i < sv->count; i++) {
                NCodecInstance* nc = sv->ncodec[i];
                if (nc) {
                    fmu_ncodec_close(fmu, nc);
                }
                sv->ncodec[i] = NULL;
            }
            free(sv->ncodec);
        }
        if (sv->mime_type) {
            for (uint32_t i = 0; i < sv->count; i++) {
                free(sv->mime_type[i]);
            }
            free(sv->mime_type);
        }
        free(sv->scalar);
        if (sv->binary) {
            for (uint32_t i = 0; i < sv->count; i++) {
                free(sv->binary[i]);
                sv->binary[i] = NULL;
            }
            free(sv->binary);
            sv->binary = NULL;
        }
        free(sv->length);
        free(sv->buffer_size);
    }
    free(fmu->data);
}


/**
fmu_load_signal_handlers
========================

This method assigns the signal handler function to a vtable.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmu_load_signal_handlers(FmuInstanceData* fmu)
{
    /* Set default handlers. */
    fmu->variables.vtable.reset = fmu_default_signals_reset;
    fmu->variables.vtable.setup = fmu_default_signals_setup;
    fmu->variables.vtable.remove = fmu_default_signals_remove;
}
