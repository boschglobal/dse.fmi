// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fmi2Functions.h>
#include <fmi2FunctionTypes.h>
#include <fmi2TypesPlatform.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/util/strings.h>
#include <dse/fmimodelc/fmimodelc.h>


#define UNUSED(x)    ((void)x)
#define VREF_KEY_LEN (10 + 1)
#define END_TIME     (3 * 24 * 60 * 60)


static void _log(const char* format, ...)
{
    printf("ModelCFmu: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

static void _log_binary_signal(SimbusVectorIndex* idx)
{
    if (idx == NULL || idx->sbv == NULL) return;
    uint32_t index = idx->vi;
    _log("      - name       : %s", idx->sbv->signal[index]);
    _log("        length     : %d", idx->sbv->length[index]);
    _log("        buffer len : %d", idx->sbv->buffer_size[index]);
    uint8_t* buffer = idx->sbv->binary[index];
    for (uint32_t j = 0; j + 16 < idx->sbv->length[index]; j += 16) {
        _log("          %02x %02x %02x %02x %02x %02x %02x %02x "
             "%02x %02x %02x %02x %02x %02x %02x %02x",
            buffer[j + 0], buffer[j + 1], buffer[j + 2], buffer[j + 3],
            buffer[j + 4], buffer[j + 5], buffer[j + 6], buffer[j + 7],
            buffer[j + 8], buffer[j + 9], buffer[j + 10], buffer[j + 11],
            buffer[j + 12], buffer[j + 13], buffer[j + 14], buffer[j + 15]);
    }
}


/* FMI2 FMU Instance Data */
typedef struct Fmu2InstanceData {
    /* FMI Instance Data. */
    struct {
        char*                       name;
        fmi2Type                    type;
        char*                       resource_location;
        char*                       guid;
        bool                        log_enabled;
        /* FMI Callbacks. */
        const fmi2CallbackFunctions callbacks;
        /* Storage for memory to be explicitly released. */
        char*                       save_resource_location;
    } instance;
    /* ModelC Model Runtime. */
    struct {
        RuntimeModelDesc* model;
        /* Variable indexes. */
        struct {
            HashMap input;
            HashMap output;
        } scalar;
        struct {
            HashMap  rx;
            HashMap  tx;
            HashMap  encode_func;
            HashMap  decode_func;
            HashList free_list; /*Lazy free list for allocated strings. */
        } binary;
    } runtime;
} Fmu2InstanceData;


/**
fmi2Instantiate
===============

Create an instance of this FMU, allocate/initialise a Fmu2InstanceData
object which should be used for subsequent calls to FMI methods (as parameter
`fmi2Component c`).

> Note: This implementation __does not__ use memory related callbacks provided
  by the Importer (e.g. `malloc()` or `free()`).

Returns
-------
fmi2Component (pointer)
: An Fmu2InstanceData object which represents this FMU instance.
*/
fmi2Component fmi2Instantiate(fmi2String instance_name, fmi2Type fmu_type,
    fmi2String fmu_guid, fmi2String fmu_resource_location,
    const fmi2CallbackFunctions* functions, fmi2Boolean visible,
    fmi2Boolean logging_on)
{
    UNUSED(visible);

    /* Create the FMU Model Instance Data. */
    _log("Create the FMU Model Instance Data");
    Fmu2InstanceData* fmu = calloc(1, sizeof(Fmu2InstanceData));
    fmu->instance.name = strdup(instance_name);
    fmu->instance.type = fmu_type;
    fmu->instance.resource_location = strdup(fmu_resource_location);
    fmu->instance.guid = strdup(fmu_guid);
    fmu->instance.log_enabled = logging_on;
    if (functions) {
        memcpy((void*)&fmu->instance.callbacks, functions,
            sizeof(fmi2CallbackFunctions));
    }

    /**
     *  Calculate the offset needed to trim/correct the resource location.
     *  The resource location may take the forms:
     *
     *      file:///tmp/MyFMU/resources
     *      file:/tmp/MyFMU/resources
     *      /tmp/MyFMU/resources
     */
    fmu->instance.save_resource_location = fmu->instance.resource_location;
    int resource_path_offset = 0;
    if (strstr(fmu_resource_location, FILE_URI_SCHEME)) {
        resource_path_offset = strlen(FILE_URI_SCHEME);
    } else if (strstr(fmu_resource_location, FILE_URI_SHORT_SCHEME)) {
        resource_path_offset = strlen(FILE_URI_SHORT_SCHEME);
    }
    fmu->instance.resource_location += resource_path_offset;
    _log("Resource location: %s", fmu->instance.resource_location);

    /* Allocate the RuntimeModelDesc object. */
    _log("Allocate the RuntimeModelDesc object");
    fmu->runtime.model = calloc(1, sizeof(RuntimeModelDesc));
    *fmu->runtime.model = (RuntimeModelDesc){};
    hashmap_init(&fmu->runtime.scalar.input);
    hashmap_init(&fmu->runtime.scalar.output);
    hashmap_init(&fmu->runtime.binary.rx);
    hashmap_init(&fmu->runtime.binary.tx);
    hashmap_init(&fmu->runtime.binary.encode_func);
    hashmap_init(&fmu->runtime.binary.decode_func);

    /* Lazy free list. */
    hashlist_init(&fmu->runtime.binary.free_list, 1024);

    /* Return the created instance object. */
    return (fmi2Component)fmu;
}


/**
fmi2ExitInitializationMode
==========================

Initialise the Model Runtime (of the ModelC library) and in the process
establish the simulation that this ModelC FMU is wrapping/operating.

This function will generate indexes to map between FMI Variables and ModelC
Signals; both scaler signals (double) and binary signals (string/binary).

Parameters
----------
c (fmi2Component*)
: An Fmu2InstanceData object representing an instance of this FMU.

Returns
-------
fmi2OK (fmi2Status)
: The simulation that this FMU represents is ready to be operated.
*/
fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    assert(c);
    Fmu2InstanceData* fmu = (Fmu2InstanceData*)c;
    RuntimeModelDesc* m = fmu->runtime.model;
    assert(m);

    /* Create the Model Runtime object. */
    _log("Create the Model Runtime object");
    *m = (RuntimeModelDesc){
         .runtime = {
             .runtime_model = fmu->instance.name,
             .model_name = fmu->instance.name,
             .sim_path = dse_path_cat(fmu->instance.resource_location, "sim"),
             .simulation_yaml = "data/simulation.yaml",
             .end_time = END_TIME,
             .log_level = 5,
         },
    };
    m->model.sim = calloc(1, sizeof(SimulationSpec));
    _log("Call model_runtime_create() ...");
    m = model_runtime_create(m);
    _log("Build indexes");
    fmimodelc_index_scalar_signals(
        m, &fmu->runtime.scalar.input, &fmu->runtime.scalar.output);
    fmimodelc_index_binary_signals(
        m, &fmu->runtime.binary.rx, &fmu->runtime.binary.tx);
    fmimodelc_index_text_encoding(
        m, &fmu->runtime.binary.encode_func, &fmu->runtime.binary.decode_func);

    return fmi2OK;
}


/**
fmi2GetReal
===========

Get values for the provided list of value references.

Parameters
----------
c (fmi2Component*)
: An Fmu2InstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to retrieve.

nvr (int)
: The number of value references to retrieve.

value (fmi2Real[])
: Storage for the retrieved values.

Returns
-------
fmi2OK (fmi2Status)
: The requested variables are retrieved (where available).
*/
fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Real value[])
{
    assert(c);
    Fmu2InstanceData* fmu = (Fmu2InstanceData*)c;

    for (size_t i = 0; i < nvr; i++) {
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        double* signal = hashmap_get(&fmu->runtime.scalar.output, vr_idx);
        if (signal == NULL) continue;

        /* Set the scalar signal value. */
        value[i] = *signal;
    }
    return fmi2OK;
}


/**
fmi2GetString
=============

Get values for the provided list of value references.

Parameters
----------
c (fmi2Component*)
: An Fmu2InstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to retrieve.

nvr (int)
: The number of value references to retrieve.

value (fmi2String[])
: Storage for the retrieved values.

Returns
-------
fmi2OK (fmi2Status)
: The requested variables are retrieved (where available).
*/
fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2String value[])
{
    assert(c);
    Fmu2InstanceData* fmu = (Fmu2InstanceData*)c;

    /* Free items on the lazy free list. */
    hashmap_clear(&fmu->runtime.binary.free_list.hash_map);

    for (size_t i = 0; i < nvr; i++) {
        /* Initial value condition is a NULL string. */
        value[i] = NULL;

        /* Lookup the binary signal, by VRef. */
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        SimbusVectorIndex* idx = hashmap_get(&fmu->runtime.binary.tx, vr_idx);
        if (idx == NULL) continue;
        if (idx->sbv == NULL) continue;
        uint8_t* data = idx->sbv->binary[idx->vi];
        uint32_t data_len = idx->sbv->length[idx->vi];
        if (data == NULL || data_len == 0) continue;

        /* Write the requested string, encode if configured. */
        _log_binary_signal(idx);
        EncodeFunc ef = hashmap_get(&fmu->runtime.binary.encode_func, vr_idx);
        if (ef) {
            value[i] = ef((char*)data, data_len);
        } else {
            value[i] = strdup((char*)data);
        }
        /* Save reference for later free. */
        char key[HASHLIST_KEY_LEN];
        snprintf(key, HASHLIST_KEY_LEN, "%i",
            hashlist_length(&fmu->runtime.binary.free_list));
        hashmap_set_alt(
            &fmu->runtime.binary.free_list.hash_map, key, (void*)value[i]);
    }
    return fmi2OK;
}


/**
fmi2SetReal
===========

Set values for the provided list of value references and values.

Parameters
----------
c (fmi2Component*)
: An Fmu2InstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to set.

nvr (int)
: The number of value references to set.

value (fmi2Real[])
: Storage for the values to be set.

Returns
-------
fmi2OK (fmi2Status)
: The requested variables have been set (where available).
*/
fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Real value[])
{
    assert(c);
    Fmu2InstanceData* fmu = (Fmu2InstanceData*)c;

    for (size_t i = 0; i < nvr; i++) {
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        double* signal = hashmap_get(&fmu->runtime.scalar.input, vr_idx);
        if (signal == NULL) continue;

        /* Set the scalar signal value. */
        *signal = value[i];
    }
    return fmi2OK;
}


/**
fmi2SetString
=============

Set values for the provided list of value references and values. String/Binary
variables are always appended to the ModelC Binary Signal.

> Note: If several variables are indexed against the same ModelC Binary Signal,
  for instance in a Bus Topology, then each variable will be appended to that
  ModelC Binary Signal.

Parameters
----------
c (fmi2Component*)
: An Fmu2InstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to set.

nvr (int)
: The number of value references to set.

value (fmi2String[])
: Storage for the values to be set.

Returns
-------
fmi2OK (fmi2Status)
: The requested variables have been set (where available).
*/
fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2String value[])
{
    assert(c);
    Fmu2InstanceData* fmu = (Fmu2InstanceData*)c;
    RuntimeModelDesc* m = fmu->runtime.model;
    assert(m);

    /* Make sure that all binary signals were reset at some point. */
    fmimodelc_reset_binary_signals(m);

    for (size_t i = 0; i < nvr; i++) {
        /* String to process? */
        if (value[i] == NULL) continue;

        /* Lookup the binary signal, by VRef. */
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        SimbusVectorIndex* idx = hashmap_get(&fmu->runtime.binary.rx, vr_idx);
        if (idx == NULL) continue;
        if (idx->sbv == NULL) continue;

        /* Get the input binary string, decode if configured. */
        char*      data = (char*)value[i];
        size_t     data_len = strlen(data);
        DecodeFunc df = hashmap_get(&fmu->runtime.binary.decode_func, vr_idx);
        if (df) {
            data = df((char*)data, &data_len);
        }

        /* Append the binary string to the Binary Signal. */
        dse_buffer_append(&idx->sbv->binary[idx->vi],
            &idx->sbv->length[idx->vi], &idx->sbv->buffer_size[idx->vi],
            (void*)data, data_len);
        _log_binary_signal(idx);

        /* Release the decode string/memory. Caller owns value[]. */
        if (data != value[i]) free(data);
    }
    return fmi2OK;
}


/**
fmi2DoStep
==========

Set values for the provided list of value references and values. String/Binary
variables are always appended to the ModelC Binary Signal.

> Note: If several variables are indexed against the same ModelC Binary Signal,
  for instance in a Bus Topology, then each variable will be appended to that
  ModelC Binary Signal.

Parameters
----------
c (fmi2Component*)
: An Fmu2InstanceData object representing an instance of this FMU.

currentCommunicationPoint (fmi2Real)
: The model time (for the start of this step).

communicationStepSize (fmi2Real)
: The model step size.

noSetFMUStatePriorToCurrentPoint (fmi2Boolean)
: Not used.

Returns
-------
fmi2OK (fmi2Status)
: The step completed.

fmi2Error (fmi2Status)
: An error occurred when stepping the ModelC Simulation.
*/
fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint,
    fmi2Real    communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint)
{
    assert(c);
    UNUSED(noSetFMUStatePriorToCurrentPoint);
    Fmu2InstanceData* fmu = (Fmu2InstanceData*)c;
    RuntimeModelDesc* m = fmu->runtime.model;
    assert(m);

    /* Make sure that all binary signals were reset at some point. */
    fmimodelc_reset_binary_signals(m);

    /* Step the model. */
    double model_time = currentCommunicationPoint;
    _log("Call model_runtime_step() ...");
    int rc = model_runtime_step(
        m, &model_time, currentCommunicationPoint + communicationStepSize);

    /* Reset the binary signal reset mechanism. */
    m->runtime.binary_signals_reset = false;

    /* return final status. */
    return (rc == 0 ? fmi2OK : fmi2Error);
}


/**
fmi2FreeInstance
================

Free memory and resources related to the provided FMU instance.

Parameters
----------
c (fmi2Component*)
: An Fmu2InstanceData object representing an instance of this FMU.
*/
void fmi2FreeInstance(fmi2Component c)
{
    assert(c);
    Fmu2InstanceData* fmu = (Fmu2InstanceData*)c;
    RuntimeModelDesc* m = fmu->runtime.model;
    assert(m);

    _log("Call model_runtime_destroy() ...");
    free(m->runtime.sim_path);
    model_runtime_destroy(m);
    free(m->model.sim);

    _log("Destroy the index");
    hashmap_destroy(&fmu->runtime.scalar.input);
    hashmap_destroy(&fmu->runtime.scalar.output);
    hashmap_destroy(&fmu->runtime.binary.rx);
    hashmap_destroy(&fmu->runtime.binary.tx);
    hashmap_destroy(&fmu->runtime.binary.encode_func);
    hashmap_destroy(&fmu->runtime.binary.decode_func);
    hashlist_destroy(&fmu->runtime.binary.free_list);
    free(m);

    _log("Release FMI instance resources");
    free(fmu->instance.name);
    free(fmu->instance.guid);
    free(fmu->instance.save_resource_location);
    free(c);
}


/*
Unused parts of FMI interface
=============================

These functions are required to satisfy FMI packaging restrictions (i.e. these
functions need to exist in an FMU for some reason ...).
*/

const char* fmi2GetTypesPlatform(void)
{
    return fmi2TypesPlatform;
}

const char* fmi2GetVersion(void)
{
    return fmi2Version;
}

fmi2Status fmi2SetDebugLogging(fmi2Component c, fmi2Boolean loggingOn,
    size_t nCategories, const fmi2String categories[])
{
    assert(c);
    UNUSED(loggingOn);
    UNUSED(nCategories);
    UNUSED(categories);
    return fmi2OK;
}

fmi2Status fmi2SetupExperiment(fmi2Component c, fmi2Boolean toleranceDefined,
    fmi2Real tolerance, fmi2Real startTime, fmi2Boolean stopTimeDefined,
    fmi2Real stopTime)
{
    assert(c);
    UNUSED(toleranceDefined);
    UNUSED(tolerance);
    UNUSED(startTime);
    UNUSED(stopTimeDefined);
    UNUSED(stopTime);
    return fmi2OK;
}

fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
    assert(c);
    return fmi2OK;
}


fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Integer value[])
{
    assert(c);
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Boolean value[])
{
    assert(c);
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Integer value[])
{
    assert(c);
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Boolean value[])
{
    assert(c);
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2GetStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Status* value)
{
    assert(c);
    UNUSED(s);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2GetRealStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Real* value)
{
    assert(c);
    UNUSED(s);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2GetIntegerStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Integer* value)
{
    assert(c);
    UNUSED(s);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2GetBooleanStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value)
{
    assert(c);
    UNUSED(s);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2GetStringStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2String* value)
{
    assert(c);
    UNUSED(s);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2SetRealInputDerivatives(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[],
    const fmi2Real value[])
{
    assert(c);
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(order);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2GetRealOutputDerivatives(fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr, const fmi2Integer order[],
    fmi2Real value[])
{
    assert(c);
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(order);
    UNUSED(value);
    return fmi2OK;
}

fmi2Status fmi2CancelStep(fmi2Component c)
{
    assert(c);
    return fmi2OK;
}

fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
    assert(c);
    UNUSED(FMUstate);
    return fmi2OK;
}

fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate FMUstate)
{
    assert(c);
    UNUSED(FMUstate);
    return fmi2OK;
}

fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* FMUstate)
{
    assert(c);
    UNUSED(FMUstate);
    return fmi2OK;
}

fmi2Status fmi2SerializedFMUstateSize(
    fmi2Component c, fmi2FMUstate FMUstate, size_t* size)
{
    assert(c);
    UNUSED(FMUstate);
    UNUSED(size);
    return fmi2OK;
}

fmi2Status fmi2SerializeFMUstate(fmi2Component c, fmi2FMUstate FMUstate,
    fmi2Byte serializedState[], size_t size)
{
    assert(c);
    UNUSED(FMUstate);
    UNUSED(serializedState);
    UNUSED(size);
    return fmi2OK;
}

fmi2Status fmi2DeSerializeFMUstate(fmi2Component c,
    const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate)
{
    assert(c);
    UNUSED(serializedState);
    UNUSED(size);
    UNUSED(FMUstate);
    return fmi2OK;
}

fmi2Status fmi2GetDirectionalDerivative(fmi2Component c,
    const fmi2ValueReference vUnknown_ref[], size_t nUnknown,
    const fmi2ValueReference vKnown_ref[], size_t nKnown,
    const fmi2Real dvKnown[], fmi2Real dvUnknown[])
{
    assert(c);
    UNUSED(vUnknown_ref);
    UNUSED(nUnknown);
    UNUSED(vKnown_ref);
    UNUSED(nKnown);
    UNUSED(dvKnown);
    UNUSED(dvUnknown);
    return fmi2OK;
}

fmi2Status fmi2Reset(fmi2Component c)
{
    assert(c);
    return fmi2OK;
}

fmi2Status fmi2Terminate(fmi2Component c)
{
    assert(c);
    return fmi2OK;
}
