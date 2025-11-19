// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fmi2Functions.h>
#include <fmi2FunctionTypes.h>
#include <fmi2TypesPlatform.h>
#include <dse/testing.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/util/strings.h>
#include <dse/fmu/fmu.h>


#define UNUSED(x)    ((void)x)
#define VREF_KEY_LEN (10 + 1)


/**
default_log
===========

Default logging function in case the FMU caller does not provide any logger.
*/
void default_log(fmi2ComponentEnvironment componentEnvironment,
    fmi2String instanceName, fmi2Status status, fmi2String category,
    fmi2String message, ...)
{
    UNUSED(componentEnvironment);
    UNUSED(instanceName);

    static const char* statusString[] = { "OK", "Warning", "Discard", "Error",
        "Fatal", "Pending" };

    printf("[%s:%s] %s\n", category, statusString[status], message);
    fflush(stdout);
}


void fmu_log(FmuInstanceData* fmu, const int status, const char* category,
    const char* message, ...)
{
    if (fmu->instance.log_enabled == fmi2False) return;

    va_list args;
    va_start(args, message);
    char format[1024];
    vsnprintf(format, sizeof(format), message, args);
    va_end(args);

    ((void (*)())fmu->instance.logger)(fmu->instance.environment,
        fmu->instance.name, status, category, format);
}


static void _log_binary_signal(
    FmuInstanceData* fmu, FmuSignalVectorIndex* idx, const char* op)
{
    if (idx == NULL || idx->sv->binary == NULL) return;
    uint32_t index = idx->vi;

    fmu_log(fmu, fmi2OK, "Debug",
        "\n      - name       : %s (%s)"
        "\n        length     : %d"
        "\n        buffer len : %d",
        idx->sv->signal[index], op, idx->sv->length[index],
        idx->sv->buffer_size[index]);

    uint8_t* buffer = idx->sv->binary[index];
    for (uint32_t j = 0; j + 16 < idx->sv->length[index]; j += 16) {
        fmu_log(fmu, fmi2OK, "Debug",
            "%02x %02x %02x %02x %02x %02x %02x %02x "
            "%02x %02x %02x %02x %02x %02x %02x %02x",
            buffer[j + 0], buffer[j + 1], buffer[j + 2], buffer[j + 3],
            buffer[j + 4], buffer[j + 5], buffer[j + 6], buffer[j + 7],
            buffer[j + 8], buffer[j + 9], buffer[j + 10], buffer[j + 11],
            buffer[j + 12], buffer[j + 13], buffer[j + 14], buffer[j + 15]);
    }
}


/**
fmi2Instantiate
===============

Create an instance of this FMU, allocate/initialise a FmuInstanceData
object which should be used for subsequent calls to FMI methods (as parameter
`fmi2Component c`).

> Note: This implementation __does not__ use memory related callbacks provided
  by the Importer (e.g. `malloc()` or `free()`).

Returns
-------
fmi2Component (pointer)
: An FmuInstanceData object which represents this FMU instance.
*/
fmi2Component fmi2Instantiate(fmi2String instance_name, fmi2Type fmu_type,
    fmi2String fmu_guid, fmi2String fmu_resource_location,
    const fmi2CallbackFunctions* functions, fmi2Boolean visible,
    fmi2Boolean logging_on)
{
    UNUSED(visible);

    /* Create the FMU Model Instance Data. */
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));
    fmu->instance.name = strdup(instance_name);
    fmu->instance.type = fmu_type;
    fmu->instance.resource_location = strdup(fmu_resource_location);
    fmu->instance.guid = strdup(fmu_guid);
    fmu->instance.log_enabled = logging_on;
    fmu->instance.version = 2;

    if (functions) {
        fmu->instance.environment = functions->componentEnvironment;
        if (functions->logger) {
            fmu->instance.logger = functions->logger;
            /* Callbacks are provided, but no logger. */
        } else {
            fmu->instance.logger = default_log;
        }
        /* No callback function at all provided. */
    } else {
        fmu->instance.logger = default_log;
    }
    fmu_log(fmu, fmi2OK, "Debug", "FMU Model instantiated");

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

    fmu_log(fmu, fmi2OK, "Debug", "Resource location: %s",
        fmu->instance.resource_location);

    fmu_log(fmu, fmi2OK, "Debug", "Build indexes...");
    hashmap_init(&fmu->variables.scalar.input);
    hashmap_init(&fmu->variables.scalar.output);
    hashmap_init(&fmu->variables.string.input);
    hashmap_init(&fmu->variables.string.output);
    hashmap_init(&fmu->variables.binary.rx);
    hashmap_init(&fmu->variables.binary.tx);
    hashmap_init(&fmu->variables.binary.encode_func);
    hashmap_init(&fmu->variables.binary.decode_func);

    /* Setup signal indexing. */
    fmu_load_signal_handlers(fmu);
    if (fmu->variables.vtable.setup) fmu->variables.vtable.setup(fmu);

    /* Lazy free list. */
    hashlist_init(&fmu->variables.binary.free_list, 1024);

    /* Create the FMU. */
    errno = 0;
    FmuInstanceData* extended_fmu_inst = fmu_create(fmu);
    if (errno) {
        fmu_log(fmu, fmi2Error, "Error",
            "The FMU was not created correctly! (errro = %d)", errno);
    }

    if (extended_fmu_inst && extended_fmu_inst != fmu) {
        /* The fmu returned an extended (new) FmuInstanceData object. */
        free(fmu);
        fmu = extended_fmu_inst;
    }
    if (fmu->var_table.table == NULL) {
        fmu_log(fmu, fmi2OK, "Debug", "FMU Var Table is not configured");
    }

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
: An FmuInstanceData object representing an instance of this FMU.

Returns
-------
fmi2OK (fmi2Status)
: The simulation that this FMU represents is ready to be operated.
*/
fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    assert(c);
    FmuInstanceData* fmu = (FmuInstanceData*)c;

    int32_t rc = fmu_init(fmu);

    return (rc == 0 ? fmi2OK : fmi2Error);
}


/**
fmi2GetReal
===========

Get values for the provided list of value references.

Parameters
----------
c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.

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
    FmuInstanceData* fmu = (FmuInstanceData*)c;

    for (size_t i = 0; i < nvr; i++) {
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        double* signal = NULL;
        signal = hashmap_get(&fmu->variables.scalar.output, vr_idx);
        /* Get operations can also be used on input variables. */
        if (signal == NULL) {
            signal = hashmap_get(&fmu->variables.scalar.input, vr_idx);
            /* Signal was not found on either output or input signals. */
            if (signal == NULL) continue;
        }

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
: An FmuInstanceData object representing an instance of this FMU.

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
    FmuInstanceData* fmu = (FmuInstanceData*)c;

    /* Free items on the lazy free list. */
    hashmap_clear(&fmu->variables.binary.free_list.hash_map);

    for (size_t i = 0; i < nvr; i++) {
        /* Initial value condition is a NULL string. */
        value[i] = NULL;

        /* Lookup the binary signal, by VRef. */
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        FmuSignalVectorIndex* idx =
            hashmap_get(&fmu->variables.binary.tx, vr_idx);
        if (idx == NULL) continue;

        uint8_t* data = idx->sv->binary[idx->vi];
        uint32_t data_len = idx->sv->length[idx->vi];
        if (data == NULL || data_len == 0) continue;

        /* Write the requested string, encode if configured. */
        _log_binary_signal(fmu, idx, "GetString");
        EncodeFunc ef = hashmap_get(&fmu->variables.binary.encode_func, vr_idx);
        if (ef) {
            value[i] = ef((char*)data, data_len);
        } else {
            value[i] = strdup((char*)data);
        }

        /* Save reference for later free. */
        char key[HASHLIST_KEY_LEN];
        snprintf(key, HASHLIST_KEY_LEN, "%i",
            hashlist_length(&fmu->variables.binary.free_list));
        hashmap_set_alt(
            &fmu->variables.binary.free_list.hash_map, key, (void*)value[i]);
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
: An FmuInstanceData object representing an instance of this FMU.

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
    FmuInstanceData* fmu = (FmuInstanceData*)c;

    for (size_t i = 0; i < nvr; i++) {
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        double* signal = hashmap_get(&fmu->variables.scalar.input, vr_idx);
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
: An FmuInstanceData object representing an instance of this FMU.

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
    FmuInstanceData* fmu = (FmuInstanceData*)c;

    /* Make sure that all binary signals were reset at some point. */
    if (fmu->variables.vtable.reset) fmu->variables.vtable.reset(fmu);

    for (size_t i = 0; i < nvr; i++) {
        /* String to process? */
        if (value[i] == NULL) continue;

        /* Lookup the binary signal, by VRef. */
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", vr[i]);
        FmuSignalVectorIndex* idx =
            hashmap_get(&fmu->variables.binary.rx, vr_idx);
        if (idx == NULL) {
            hashmap_set_string(
                &fmu->variables.string.input, vr_idx, (char*)value[i]);
            continue;
        };

        /* Get the input binary string, decode if configured. */
        char*      data = (char*)value[i];
        size_t     data_len = strlen(data);
        DecodeFunc df = hashmap_get(&fmu->variables.binary.decode_func, vr_idx);
        if (df) {
            data = df((char*)data, &data_len);
        }

        /* Append the binary string to the Binary Signal. */
        dse_buffer_append(&idx->sv->binary[idx->vi], &idx->sv->length[idx->vi],
            &idx->sv->buffer_size[idx->vi], (void*)data, data_len);
        _log_binary_signal(fmu, idx, "SetString");

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
: An FmuInstanceData object representing an instance of this FMU.

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
    FmuInstanceData* fmu = (FmuInstanceData*)c;
    assert(fmu);

    /* Make sure that all binary signals were reset at some point. */
    if (fmu->variables.vtable.reset) fmu->variables.vtable.reset(fmu);
    /* Marshal Signal Vectors to the VarTable. */
    for (FmuVarTableMarshalItem* mi = fmu->var_table.marshal_list;
        mi && mi->variable; mi++) {
        *mi->variable = *mi->signal;
    }

    /* Step the model. */
    int32_t rc =
        fmu_step(fmu, currentCommunicationPoint, communicationStepSize);

    /* Marshal the VarTable to the Signal Vectors. */
    for (FmuVarTableMarshalItem* mi = fmu->var_table.marshal_list;
        mi && mi->variable; mi++) {
        *mi->signal = *mi->variable;
    }
    /* Reset the binary signal reset mechanism. */
    fmu->variables.signals_reset = false;

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
: An FmuInstanceData object representing an instance of this FMU.
*/
void fmi2FreeInstance(fmi2Component c)
{
    assert(c);
    FmuInstanceData* fmu = (FmuInstanceData*)c;

    if (fmu_destroy(fmu) < fmi2OK) {
        fmu_log(fmu, fmi2Error, "Error", "Could not release model");
    }

    if (fmu->variables.vtable.remove) fmu->variables.vtable.remove(fmu);

    fmu_log(fmu, fmi2OK, "Debug", "Release var table");
    free(fmu->var_table.table);
    free(fmu->var_table.marshal_list);
    if (fmu->var_table.var_list.hash_map.hash_function) {
        hashlist_destroy(&fmu->var_table.var_list);
    }

    fmu_log(fmu, fmi2OK, "Debug", "Destroy the index");
    hashmap_destroy(&fmu->variables.scalar.input);
    hashmap_destroy(&fmu->variables.scalar.output);
    hashmap_destroy(
        &fmu->variables.string.input);  // NOLINT (build/include_what_you_use)
    hashmap_destroy(
        &fmu->variables.string.output);  // NOLINT (build/include_what_you_use)
    hashmap_destroy(&fmu->variables.binary.rx);
    hashmap_destroy(&fmu->variables.binary.tx);
    hashmap_destroy(&fmu->variables.binary.encode_func);
    hashmap_destroy(&fmu->variables.binary.decode_func);
    hashlist_destroy(&fmu->variables.binary.free_list);

    fmu_log(fmu, fmi2OK, "Debug", "Release FMI instance resources");
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
