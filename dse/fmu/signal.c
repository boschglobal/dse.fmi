// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <dlfcn.h>
#include <libxml/xpath.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/fmu/fmu.h>


#define UNUSED(x)         ((void)x)
#define FMI2_SCALAR_XPATH "/fmiModelDescription/ModelVariables/ScalarVariable"


/**
fmu_signals_reset
=================

This method will reset any binary variables which where used by an FMU in the
previous step. Typically this will mean that indexes into the buffers of
binary variables are set to 0, hovever the buffers themselves are
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

This method will reomve any buffers used to provide storage for FMU variables.
If those buffers were allocated (e.g by an implementation
of `fmu_signals_setup()`) then those buffers should be freed in this method.

> Integrators may provide their own implementation of this method.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
extern void fmu_signals_remove(FmuInstanceData* fmu);


static bool __is_scalar_var(xmlNodePtr child)
{
    if (xmlStrcmp(child->name, (xmlChar*)"Real") == 0 ||
        xmlStrcmp(child->name, (xmlChar*)"Integer") == 0 ||
        xmlStrcmp(child->name, (xmlChar*)"Boolean") == 0 ||
        xmlStrcmp(child->name, (xmlChar*)"Float64") == 0) {
        // TODO add FMI3 types.
        return true;
    } else {
        return false;
    }
}


static bool __is_binary_var(xmlNodePtr child)
{
    if (xmlStrcmp(child->name, (xmlChar*)"String") == 0 ||
        xmlStrcmp(child->name, (xmlChar*)"Binary") == 0) {
        return true;
    } else {
        return false;
    }
}


static size_t __count_variables(xmlDoc* doc, bool is_binary)
{
    size_t           count = 0;
    xmlXPathContext* ctx = xmlXPathNewContext(doc);
    xmlXPathObject*  obj =
        xmlXPathEvalExpression((xmlChar*)FMI2_SCALAR_XPATH, ctx);
    if (obj == NULL) goto cleanup;

    for (int i = 0; i < obj->nodesetval->nodeNr; i++) {
        xmlNodePtr scalarVariable = obj->nodesetval->nodeTab[i];
        for (xmlNodePtr child = scalarVariable->children; child;
             child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;
            if (is_binary) {
                if (__is_binary_var(child)) {
                    count += 1;
                }
            } else {
                if (__is_scalar_var(child)) {
                    count += 1;
                }
            }
        }
    }

cleanup:
    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
    return count;
}


static FmuSignalVector* __allocate_sv(xmlDoc* doc, bool is_binary)
{
    size_t count = __count_variables(doc, is_binary);
    if (count == 0) return NULL;

    FmuSignalVector* sv = calloc(1, sizeof(FmuSignalVector));
    sv->count = count;
    sv->signal = calloc(count, sizeof(char*));
    if (is_binary) {
        sv->binary = calloc(count, sizeof(void*));
        sv->length = calloc(count, sizeof(uint32_t));
        sv->buffer_size = calloc(count, sizeof(uint32_t));
    } else {
        sv->scalar = calloc(count, sizeof(double));
    }
    return sv;
}


static void __index_variables(
    xmlDoc* doc, FmuInstanceData* fmu, FmuSignalVector* sv, bool is_binary)
{
    xmlXPathContext* ctx = xmlXPathNewContext(doc);
    xmlXPathObject*  obj =
        xmlXPathEvalExpression((xmlChar*)FMI2_SCALAR_XPATH, ctx);
    if (obj == NULL) goto cleanup;

    /* Scan all variables. */
    uint32_t sv_idx = 0;
    for (int i = 0; i < obj->nodesetval->nodeNr; i++) {
        /* ScalarVariable. */
        xmlNodePtr scalarVariable = obj->nodesetval->nodeTab[i];
        xmlChar*   name = xmlGetProp(scalarVariable, (xmlChar*)"name");
        xmlChar*   vr = xmlGetProp(scalarVariable, (xmlChar*)"valueReference");
        xmlChar* causality = xmlGetProp(scalarVariable, (xmlChar*)"causality");

        /* Search for a supported variable type. */
        for (xmlNodePtr child = scalarVariable->children; child;
             child = child->next) {
            if (child->type != XML_ELEMENT_NODE) continue;
            if (is_binary) {
                if (!__is_binary_var(child)) goto next;
            } else {
                if (!__is_scalar_var(child)) goto next;
            }

            /* Index this variable. */
            assert(sv_idx < sv->count);
            sv->signal[sv_idx] = strdup((char*)name);
            if (is_binary) {
                FmuSignalVectorIndex* idx =
                    calloc(1, sizeof(FmuSignalVectorIndex));
                idx->sv = sv;
                idx->vi = sv_idx;
                if (xmlStrcmp(causality, (xmlChar*)"output") == 0) {
                    hashmap_set_alt(
                        &(fmu->variables.binary.tx), (char*)vr, idx);
                } else if (xmlStrcmp(causality, (xmlChar*)"input") == 0) {
                    hashmap_set_alt(
                        &(fmu->variables.binary.rx), (char*)vr, idx);
                }
            } else {
                if (xmlStrcmp(causality, (xmlChar*)"output") == 0) {
                    hashmap_set(&(fmu->variables.scalar.output), (char*)vr,
                        &(sv->scalar[sv_idx]));
                } else if (xmlStrcmp(causality, (xmlChar*)"input") == 0) {
                    hashmap_set(&(fmu->variables.scalar.input), (char*)vr,
                        &(sv->scalar[sv_idx]));
                }
            }
            /* Only increment the index if a variable was indexed. */
            sv_idx += 1;
        }
    next:
        /* Cleanup. */
        xmlFree(name);
        xmlFree(vr);
        xmlFree(causality);
    }

cleanup:
    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctx);
}


static void fmu_default_signals_reset(FmuInstanceData* fmu)
{
    assert(fmu);

    if (fmu->variables.signals_reset == false) {
        for (FmuSignalVector* sv = fmu->data; sv && sv->signal; sv++) {
            if (sv->binary == NULL) continue;
            for (uint32_t i = 0; i < sv->count; i++) {
                sv->length[i] = 0;
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
        if (_sv->scalar) {
            __index_variables(doc, fmu, _sv, false);
            continue;
        }
        __index_variables(doc, fmu, _sv, true);
    }
    fmu->data = sv;
    xmlFreeDoc(doc);
}


static void fmu_default_signals_remove(FmuInstanceData* fmu)
{
    if (fmu->data == NULL) return;
    for (FmuSignalVector* sv = fmu->data; sv && sv->signal; sv++) {
        for (uint32_t i = 0; i < sv->count; i++) {
            if (sv->signal[i]) free(sv->signal[i]);
        }
        if (sv->signal) free(sv->signal);
        if (sv->scalar) free(sv->scalar);
        if (sv->binary) free(sv->binary);
        if (sv->length) free(sv->length);
        if (sv->buffer_size) free(sv->buffer_size);
    }
    if (fmu->data) free(fmu->data);
}


void fmu_load_signal_handlers(FmuInstanceData* fmu)
{
    /* Set default handlers. */
    fmu->variables.vtable.reset = fmu_default_signals_reset;
    fmu->variables.vtable.setup = fmu_default_signals_setup;
    fmu->variables.vtable.remove = fmu_default_signals_remove;

    /* Get a handle to _this_ executable/libary (self reference). */
    void* handle = dlopen(NULL, RTLD_LAZY);
    assert(handle);
    void* func = NULL;

    /* Load custom handlers if they exist. */
    func = dlsym(handle, FMU_SIGNALS_RESET_FUNC_NAME);
    if (func) fmu->variables.vtable.reset = func;
    func = dlsym(handle, FMU_SIGNALS_SETUP_FUNC_NAME);
    if (func) {
        /* Always load setup and remove as a pair. Remove can be NULL. */
        fmu->variables.vtable.setup = func;
        fmu->variables.vtable.remove =
            dlsym(handle, FMU_SIGNALS_REMOVE_FUNC_NAME);
    }
}
