// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fmi3Functions.h>
#include <fmi3FunctionTypes.h>
#include <fmi3PlatformTypes.h>
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
void default_log(fmi3InstanceEnvironment instanceEnvironment, fmi3Status status,
    fmi3String category, fmi3String message)
{
    UNUSED(instanceEnvironment);

    static const char* statusString[] = { "OK", "Warning", "Discard", "Error",
        "Fatal" };

    printf("[%s:%s] %s\n", category, statusString[status], message);
    fflush(stdout);
}


void fmu_log(FmuInstanceData* fmu, const int status, const char* category,
    const char* message, ...)
{
    if (fmu->instance.log_enabled == fmi3False) return;

    va_list args;
    va_start(args, message);
    char format[1024];
    vsnprintf(format, sizeof(format), message, args);
    va_end(args);

    ((void (*)())fmu->instance.logger)(
        fmu->instance.environment, status, category, format);
}


static void _log_binary_signal(
    FmuInstanceData* fmu, FmuSignalVectorIndex* idx, const char* op)
{
    if (idx == NULL || idx->sv->binary == NULL) return;
    uint32_t index = idx->vi;

    fmu_log(fmu, fmi3OK, "Debug",
        "\n      - name       : %s (%s)"
        "\n        length     : %d"
        "\n        buffer len : %d",
        idx->sv->signal[index], op, idx->sv->length[index],
        idx->sv->buffer_size[index]);

    uint8_t* buffer = idx->sv->binary[index];
    for (uint32_t j = 0; j + 16 < idx->sv->length[index]; j += 16) {
        fmu_log(fmu, fmi3OK, "Debug",
            "%02x %02x %02x %02x %02x %02x %02x %02x "
            "%02x %02x %02x %02x %02x %02x %02x %02x",
            buffer[j + 0], buffer[j + 1], buffer[j + 2], buffer[j + 3],
            buffer[j + 4], buffer[j + 5], buffer[j + 6], buffer[j + 7],
            buffer[j + 8], buffer[j + 9], buffer[j + 10], buffer[j + 11],
            buffer[j + 12], buffer[j + 13], buffer[j + 14], buffer[j + 15]);
    }
}

/* Inquire version numbers and setting logging status */

const char* fmi3GetVersion()
{
    return fmi3Version;
}

fmi3Status fmi3SetDebugLogging(fmi3Instance instance, fmi3Boolean loggingOn,
    size_t nCategories, const fmi3String categories[])
{
    assert(instance);
    UNUSED(loggingOn);
    UNUSED(nCategories);
    UNUSED(categories);

    return fmi3OK;
}

/* Creation and destruction of FMU instances and setting debug status */

fmi3Instance fmi3InstantiateModelExchange(fmi3String instanceName,
    fmi3String instantiationToken, fmi3String resourcePath, fmi3Boolean visible,
    fmi3Boolean loggingOn, fmi3InstanceEnvironment instanceEnvironment,
    fmi3LogMessageCallback logMessage)
{
    UNUSED(instanceName);
    UNUSED(instantiationToken);
    UNUSED(resourcePath);
    UNUSED(visible);
    UNUSED(loggingOn);
    UNUSED(instanceEnvironment);
    UNUSED(logMessage);

    return NULL;
}

fmi3Instance fmi3InstantiateCoSimulation(fmi3String instanceName,
    fmi3String instantiationToken, fmi3String resourcePath, fmi3Boolean visible,
    fmi3Boolean loggingOn, fmi3Boolean eventModeUsed,
    fmi3Boolean                    earlyReturnAllowed,
    const fmi3ValueReference       requiredIntermediateVariables[],
    size_t                         nRequiredIntermediateVariables,
    fmi3InstanceEnvironment        instanceEnvironment,
    fmi3LogMessageCallback         logMessage,
    fmi3IntermediateUpdateCallback intermediateUpdate)
{
    UNUSED(visible);
    UNUSED(eventModeUsed);
    UNUSED(earlyReturnAllowed);
    UNUSED(requiredIntermediateVariables);
    UNUSED(nRequiredIntermediateVariables);
    UNUSED(intermediateUpdate);

    /* Create the FMU Model Instance Data. */
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));
    fmu->instance.name = strdup(instanceName);
    fmu->instance.resource_location = strdup(resourcePath);
    fmu->instance.guid = strdup(instantiationToken);
    fmu->instance.log_enabled = loggingOn;
    fmu->instance.version = 3;
    fmu->instance.environment = instanceEnvironment;

    if (logMessage) {
        fmu->instance.logger = logMessage;
    } else {
        fmu->instance.logger = default_log;
    }
    fmu_log(fmu, fmi3OK, "Debug", "FMU Model instantiated");

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
    if (strstr(resourcePath, FILE_URI_SCHEME)) {
        resource_path_offset = strlen(FILE_URI_SCHEME);
    } else if (strstr(resourcePath, FILE_URI_SHORT_SCHEME)) {
        resource_path_offset = strlen(FILE_URI_SHORT_SCHEME);
    }
    fmu->instance.resource_location += resource_path_offset;

    fmu_log(fmu, fmi3OK, "Debug", "Resource location: %s",
        fmu->instance.resource_location);

    fmu_log(fmu, fmi3OK, "Debug", "Build indexes...");
    hashmap_init(&fmu->variables.scalar.input);
    hashmap_init(&fmu->variables.scalar.output);
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
        fmu_log(fmu, fmi3Error, "Error",
            "The FMU was not created correctly! (errro = %d)", errno);
    }

    if (extended_fmu_inst && extended_fmu_inst != fmu) {
        free(fmu);
        fmu = extended_fmu_inst;
    }
    if (fmu->var_table.table == NULL) {
        fmu_log(fmu, fmi3OK, "Debug", "FMU Var Table is not configured");
    }

    /* Return the created instance object. */
    return (fmi3Instance)fmu;
}

fmi3Instance fmi3InstantiateScheduledExecution(fmi3String instanceName,
    fmi3String instantiationToken, fmi3String resourcePath, fmi3Boolean visible,
    fmi3Boolean loggingOn, fmi3InstanceEnvironment instanceEnvironment,
    fmi3LogMessageCallback logMessage, fmi3ClockUpdateCallback clockUpdate,
    fmi3LockPreemptionCallback   lockPreemption,
    fmi3UnlockPreemptionCallback unlockPreemption)
{
    UNUSED(instanceName);
    UNUSED(instantiationToken);
    UNUSED(resourcePath);
    UNUSED(visible);
    UNUSED(loggingOn);
    UNUSED(instanceEnvironment);
    UNUSED(logMessage);
    UNUSED(clockUpdate);
    UNUSED(lockPreemption);
    UNUSED(unlockPreemption);

    return NULL;
}

void fmi3FreeInstance(fmi3Instance instance)
{
    assert(instance);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;

    if (fmu_destroy(fmu) < fmi3OK) {
        fmu_log(fmu, fmi3Error, "Error",
            "Error while releasing the allocated specialised model.");
    }

    if (fmu->variables.vtable.remove) fmu->variables.vtable.remove(fmu);

    fmu_log(fmu, fmi3OK, "Debug", "Release var table");
    free(fmu->var_table.table);
    free(fmu->var_table.marshal_list);
    if (fmu->var_table.var_list.hash_map.hash_function) {
        hashlist_destroy(&fmu->var_table.var_list);
    }

    fmu_log(fmu, fmi3OK, "Debug", "Destroy the index");
    hashmap_destroy(&fmu->variables.scalar.input);
    hashmap_destroy(&fmu->variables.scalar.output);
    hashmap_destroy(&fmu->variables.binary.rx);
    hashmap_destroy(&fmu->variables.binary.tx);
    hashmap_destroy(&fmu->variables.binary.encode_func);
    hashmap_destroy(&fmu->variables.binary.decode_func);
    hashlist_destroy(&fmu->variables.binary.free_list);

    fmu_log(fmu, fmi3OK, "Debug", "Release FMI instance resources");
    free(fmu->instance.name);
    free(fmu->instance.guid);
    free(fmu->instance.save_resource_location);
    free(instance);
}

/* Enter and exit initialization mode, enter event mode, terminate and reset */

fmi3Status fmi3EnterInitializationMode(fmi3Instance instance,
    fmi3Boolean toleranceDefined, fmi3Float64 tolerance, fmi3Float64 startTime,
    fmi3Boolean stopTimeDefined, fmi3Float64 stopTime)
{
    assert(instance);
    UNUSED(toleranceDefined);
    UNUSED(tolerance);
    UNUSED(startTime);
    UNUSED(stopTimeDefined);
    UNUSED(stopTime);

    return fmi3OK;
}

fmi3Status fmi3ExitInitializationMode(fmi3Instance instance)
{
    assert(instance);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;

    int32_t rc = fmu_init(fmu);

    return (rc == 0 ? fmi3OK : fmi3Error);
}

fmi3Status fmi3EnterEventMode(fmi3Instance instance,
    fmi3EventQualifier stepEvent, fmi3EventQualifier stateEvent,
    const fmi3Int32 rootsFound[], size_t nEventIndicators,
    fmi3EventQualifier timeEvent)
{
    assert(instance);
    UNUSED(stepEvent);
    UNUSED(stateEvent);
    UNUSED(rootsFound);
    UNUSED(nEventIndicators);
    UNUSED(timeEvent);

    return fmi3OK;
}

fmi3Status fmi3Terminate(fmi3Instance instance)
{
    assert(instance);

    return fmi3OK;
}

fmi3Status fmi3Reset(fmi3Instance instance)
{
    assert(instance);

    return fmi3OK;
}

/* Getting and setting variable values */

fmi3Status fmi3GetFloat32(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Float32 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetFloat64(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Float64 values[], size_t nValues)
{
    UNUSED(nValues);

    assert(instance);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;

    for (size_t i = 0; i < nValueReferences; i++) {
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", valueReferences[i]);
        double* signal = hashmap_get(&fmu->variables.scalar.output, vr_idx);
        /* Get operations can also be used on input variables. */
        if (signal == NULL) {
            signal = hashmap_get(&fmu->variables.scalar.input, vr_idx);
            /* Signal was not found on either output or input signals. */
            if (signal == NULL) continue;
        }

        /* Set the scalar signal value. */
        values[i] = *signal;
    }

    return fmi3OK;
}

fmi3Status fmi3GetInt8(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Int8 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetUInt8(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3UInt8 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetInt16(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Int16 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetUInt16(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3UInt16 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetInt32(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Int32 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetUInt32(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3UInt32 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetInt64(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Int64 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetUInt64(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3UInt64 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetBoolean(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Boolean values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetString(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3String values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3GetBinary(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    size_t valueSizes[], fmi3Binary values[], size_t nValues)
{
    UNUSED(nValues);
    assert(instance);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;

    /* Free items on the lazy free list. */
    hashmap_clear(&fmu->variables.binary.free_list.hash_map);

    for (size_t i = 0; i < nValueReferences; i++) {
        /* Initial value condition is a NULL string. */
        values[i] = NULL;

        /* Lookup the binary signal, by VRef. */
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", valueReferences[i]);
        FmuSignalVectorIndex* idx =
            hashmap_get(&fmu->variables.binary.tx, vr_idx);
        if (idx == NULL) continue;

        fmi3Binary data = idx->sv->binary[idx->vi];
        uint32_t   data_len = idx->sv->length[idx->vi];
        if (data == NULL || data_len == 0) continue;

        /* Write the requested string, encode if configured. */
        _log_binary_signal(fmu, idx, "GetBinary");
        EncodeFunc ef = hashmap_get(&fmu->variables.binary.encode_func, vr_idx);
        if (ef) {
            values[i] = (fmi3Binary)ef((char*)data, data_len);
        } else {
            values[i] = malloc(data_len);
            memcpy((void*)values[i], data, data_len);
        }
        valueSizes[i] = data_len;

        /* Save reference for later free. */
        char key[HASHLIST_KEY_LEN];
        snprintf(key, HASHLIST_KEY_LEN, "%i",
            hashlist_length(&fmu->variables.binary.free_list));
        hashmap_set_alt(
            &fmu->variables.binary.free_list.hash_map, key, (void*)values[i]);
    }

    return fmi3OK;
}

fmi3Status fmi3GetClock(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Clock values[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);

    return fmi3OK;
}

fmi3Status fmi3SetFloat32(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Float32 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetFloat64(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Float64 values[], size_t nValues)
{
    UNUSED(nValues);

    assert(instance);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;

    for (size_t i = 0; i < nValueReferences; i++) {
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", valueReferences[i]);
        double* signal = hashmap_get(&fmu->variables.scalar.input, vr_idx);
        if (signal == NULL) continue;

        /* Set the scalar signal value. */
        *signal = values[i];
    }

    return fmi3OK;
}
fmi3Status fmi3SetInt8(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Int8 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}
fmi3Status fmi3SetUInt8(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3UInt8 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetInt16(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Int16 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetUInt16(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3UInt16 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetInt32(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Int32 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetUInt32(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3UInt32 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetInt64(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Int64 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetUInt64(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3UInt64 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}
fmi3Status fmi3SetBoolean(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Boolean values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetString(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3String values[], size_t nValues)
{
    assert(instance);
    UNUSED(nValues);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;

    for (size_t i = 0; i < nValueReferences; i++) {
        /* String to process? */
        if (values[i] == NULL) continue;

        /* Lookup the binary signal, by VRef. */
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", valueReferences[i]);
        hashmap_set_string(&fmu->variables.string.input,  // NOLINT
            vr_idx, (char*)values[i]);
    }

    return fmi3OK;
}

fmi3Status fmi3SetBinary(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const size_t valueSizes[], const fmi3Binary values[], size_t nValues)
{
    assert(instance);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;

    /* Make sure that all binary signals were reset at some point. */
    if (fmu->variables.vtable.reset) fmu->variables.vtable.reset(fmu);

    for (size_t i = 0; i < nValueReferences; i++) {
        /* String to process? */
        if (values[i] == NULL) continue;

        /* Lookup the binary signal, by VRef. */
        static char vr_idx[VREF_KEY_LEN];
        snprintf(vr_idx, VREF_KEY_LEN, "%i", valueReferences[i]);
        FmuSignalVectorIndex* idx =
            hashmap_get(&fmu->variables.binary.rx, vr_idx);
        if (idx == NULL) continue;

        /* Get the input binary string, decode if configured. */
        fmi3Binary data = values[i];
        DecodeFunc df = hashmap_get(&fmu->variables.binary.decode_func, vr_idx);
        if (df) {
            data = (fmi3Binary)df((char*)data, (size_t*)&valueSizes[i]);
        }

        /* Append the binary string to the Binary Signal. */
        dse_buffer_append(&idx->sv->binary[idx->vi], &idx->sv->length[idx->vi],
            &idx->sv->buffer_size[idx->vi], (void*)data, valueSizes[i]);

        /* Release the decode string/memory. Caller owns value[]. */
        if (data != values[i]) free((uint8_t*)data);
    }
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3SetClock(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Clock values[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(values);

    return fmi3OK;
}

/* Getting Variable Dependency Information */

fmi3Status fmi3GetNumberOfVariableDependencies(fmi3Instance instance,
    fmi3ValueReference valueReference, size_t* nDependencies)
{
    assert(instance);
    UNUSED(valueReference);
    UNUSED(nDependencies);

    return fmi3OK;
}

fmi3Status fmi3GetVariableDependencies(fmi3Instance instance,
    fmi3ValueReference dependent, size_t elementIndicesOfDependent[],
    fmi3ValueReference independents[], size_t elementIndicesOfIndependents[],
    fmi3DependencyKind dependencyKinds[], size_t nDependencies)
{
    assert(instance);
    UNUSED(dependent);
    UNUSED(elementIndicesOfDependent);
    UNUSED(independents);
    UNUSED(elementIndicesOfIndependents);
    UNUSED(dependencyKinds);
    UNUSED(nDependencies);

    return fmi3OK;
}

/* Getting and setting the internal FMU state */

fmi3Status fmi3GetFMUState(fmi3Instance instance, fmi3FMUState* FMUState)
{
    assert(instance);
    UNUSED(FMUState);

    return fmi3OK;
}

fmi3Status fmi3SetFMUState(fmi3Instance instance, fmi3FMUState FMUState)
{
    assert(instance);
    UNUSED(FMUState);

    return fmi3OK;
}

fmi3Status fmi3FreeFMUState(fmi3Instance instance, fmi3FMUState* FMUState)
{
    assert(instance);
    UNUSED(FMUState);

    return fmi3OK;
}

fmi3Status fmi3SerializedFMUStateSize(
    fmi3Instance instance, fmi3FMUState FMUState, size_t* size)
{
    assert(instance);
    UNUSED(FMUState);
    UNUSED(size);

    return fmi3OK;
}

fmi3Status fmi3SerializeFMUState(fmi3Instance instance, fmi3FMUState FMUState,
    fmi3Byte serializedState[], size_t size)
{
    assert(instance);
    UNUSED(FMUState);
    UNUSED(serializedState);
    UNUSED(size);

    return fmi3OK;
}

fmi3Status fmi3DeserializeFMUState(fmi3Instance instance,
    const fmi3Byte serializedState[], size_t size, fmi3FMUState* FMUState)
{
    assert(instance);
    UNUSED(serializedState);
    UNUSED(size);
    UNUSED(FMUState);

    return fmi3OK;
}

/* Getting partial derivatives */

fmi3Status fmi3GetDirectionalDerivative(fmi3Instance instance,
    const fmi3ValueReference unknowns[], size_t nUnknowns,
    const fmi3ValueReference knowns[], size_t nKnowns, const fmi3Float64 seed[],
    size_t nSeed, fmi3Float64 sensitivity[], size_t nSensitivity)
{
    assert(instance);
    UNUSED(unknowns);
    UNUSED(nUnknowns);
    UNUSED(knowns);
    UNUSED(nKnowns);
    UNUSED(seed);
    UNUSED(nSeed);
    UNUSED(sensitivity);
    UNUSED(nSensitivity);

    return fmi3OK;
}

fmi3Status fmi3GetAdjointDerivative(fmi3Instance instance,
    const fmi3ValueReference unknowns[], size_t nUnknowns,
    const fmi3ValueReference knowns[], size_t nKnowns, const fmi3Float64 seed[],
    size_t nSeed, fmi3Float64 sensitivity[], size_t nSensitivity)
{
    assert(instance);
    UNUSED(unknowns);
    UNUSED(nUnknowns);
    UNUSED(knowns);
    UNUSED(nKnowns);
    UNUSED(seed);
    UNUSED(nSeed);
    UNUSED(sensitivity);
    UNUSED(nSensitivity);

    return fmi3OK;
}

/* Entering and exiting the Configuration or Reconfiguration Mode */

fmi3Status fmi3EnterConfigurationMode(fmi3Instance instance)
{
    assert(instance);

    return fmi3OK;
}

fmi3Status fmi3ExitConfigurationMode(fmi3Instance instance)
{
    assert(instance);

    return fmi3OK;
}

fmi3Status fmi3GetIntervalDecimal(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Float64 intervals[], fmi3IntervalQualifier qualifiers[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(intervals);
    UNUSED(qualifiers);

    return fmi3OK;
}

fmi3Status fmi3GetIntervalFraction(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3UInt64 counters[], fmi3UInt64 resolutions[],
    fmi3IntervalQualifier qualifiers[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(counters);
    UNUSED(resolutions);
    UNUSED(qualifiers);

    return fmi3OK;
}

fmi3Status fmi3GetShiftDecimal(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3Float64 shifts[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(shifts);

    return fmi3OK;
}

fmi3Status fmi3GetShiftFraction(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    fmi3UInt64 counters[], fmi3UInt64 resolutions[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(counters);
    UNUSED(resolutions);

    return fmi3OK;
}

fmi3Status fmi3SetIntervalDecimal(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Float64 intervals[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(intervals);

    return fmi3OK;
}

fmi3Status fmi3SetIntervalFraction(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3UInt64 counters[], const fmi3UInt64 resolutions[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(counters);
    UNUSED(resolutions);

    return fmi3OK;
}

fmi3Status fmi3SetShiftDecimal(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Float64 shifts[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(shifts);

    return fmi3OK;
}

fmi3Status fmi3SetShiftFraction(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3UInt64 counters[], const fmi3UInt64 resolutions[])
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(counters);
    UNUSED(resolutions);

    return fmi3OK;
}

fmi3Status fmi3EvaluateDiscreteStates(fmi3Instance instance)
{
    assert(instance);

    return fmi3OK;
}

fmi3Status fmi3UpdateDiscreteStates(fmi3Instance instance,
    fmi3Boolean* discreteStatesNeedUpdate, fmi3Boolean* terminateSimulation,
    fmi3Boolean* nominalsOfContinuousStatesChanged,
    fmi3Boolean* valuesOfContinuousStatesChanged,
    fmi3Boolean* nextEventTimeDefined, fmi3Float64* nextEventTime)
{
    assert(instance);
    UNUSED(discreteStatesNeedUpdate);
    UNUSED(terminateSimulation);
    UNUSED(nominalsOfContinuousStatesChanged);
    UNUSED(valuesOfContinuousStatesChanged);
    UNUSED(nextEventTimeDefined);
    UNUSED(nextEventTime);

    return fmi3OK;
}

fmi3Status fmi3EnterContinuousTimeMode(fmi3Instance instance)
{
    assert(instance);

    return fmi3OK;
}

fmi3Status fmi3CompletedIntegratorStep(fmi3Instance instance,
    fmi3Boolean noSetFMUStatePriorToCurrentPoint, fmi3Boolean* enterEventMode,
    fmi3Boolean* terminateSimulation)
{
    assert(instance);
    UNUSED(noSetFMUStatePriorToCurrentPoint);
    UNUSED(enterEventMode);
    UNUSED(terminateSimulation);

    return fmi3OK;
}

/* Providing independent variables and re-initialization of caching */

fmi3Status fmi3SetTime(fmi3Instance instance, fmi3Float64 time)
{
    assert(instance);
    UNUSED(time);

    return fmi3OK;
}

fmi3Status fmi3SetContinuousStates(fmi3Instance instance,
    const fmi3Float64 continuousStates[], size_t nContinuousStates)
{
    assert(instance);
    UNUSED(continuousStates);
    UNUSED(nContinuousStates);

    return fmi3OK;
}

/* Evaluation of the model equations */

fmi3Status fmi3GetContinuousStateDerivatives(
    fmi3Instance instance, fmi3Float64 derivatives[], size_t nContinuousStates)
{
    assert(instance);
    UNUSED(derivatives);
    UNUSED(nContinuousStates);

    return fmi3OK;
}

fmi3Status fmi3GetEventIndicators(fmi3Instance instance,
    fmi3Float64 eventIndicators[], size_t nEventIndicators)
{
    assert(instance);
    UNUSED(eventIndicators);
    UNUSED(nEventIndicators);

    return fmi3OK;
}

fmi3Status fmi3GetContinuousStates(fmi3Instance instance,
    fmi3Float64 continuousStates[], size_t nContinuousStates)
{
    assert(instance);
    UNUSED(continuousStates);
    UNUSED(nContinuousStates);

    return fmi3OK;
}

fmi3Status fmi3GetNominalsOfContinuousStates(
    fmi3Instance instance, fmi3Float64 nominals[], size_t nContinuousStates)
{
    assert(instance);
    UNUSED(nominals);
    UNUSED(nContinuousStates);

    return fmi3OK;
}

fmi3Status fmi3GetNumberOfEventIndicators(
    fmi3Instance instance, size_t* nEventIndicators)
{
    assert(instance);
    UNUSED(nEventIndicators);

    return fmi3OK;
}

fmi3Status fmi3GetNumberOfContinuousStates(
    fmi3Instance instance, size_t* nContinuousStates)
{
    assert(instance);
    UNUSED(nContinuousStates);

    return fmi3OK;
}

/* Simulating the FMU */

fmi3Status fmi3EnterStepMode(fmi3Instance instance)
{
    assert(instance);

    return fmi3OK;
}

fmi3Status fmi3GetOutputDerivatives(fmi3Instance instance,
    const fmi3ValueReference valueReferences[], size_t nValueReferences,
    const fmi3Int32 orders[], fmi3Float64 values[], size_t nValues)
{
    assert(instance);
    UNUSED(valueReferences);
    UNUSED(nValueReferences);
    UNUSED(orders);
    UNUSED(values);
    UNUSED(nValues);

    return fmi3OK;
}

fmi3Status fmi3DoStep(fmi3Instance instance,
    fmi3Float64 currentCommunicationPoint, fmi3Float64 communicationStepSize,
    fmi3Boolean  noSetFMUStatePriorToCurrentPoint,
    fmi3Boolean* eventHandlingNeeded, fmi3Boolean* terminateSimulation,
    fmi3Boolean* earlyReturn, fmi3Float64* lastSuccessfulTime)
{
    UNUSED(noSetFMUStatePriorToCurrentPoint);
    UNUSED(eventHandlingNeeded);
    UNUSED(terminateSimulation);
    UNUSED(earlyReturn);
    UNUSED(lastSuccessfulTime);

    assert(instance);
    FmuInstanceData* fmu = (FmuInstanceData*)instance;
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
    return (rc == 0 ? fmi3OK : fmi3Error);
}

fmi3Status fmi3ActivateModelPartition(fmi3Instance instance,
    fmi3ValueReference clockReference, fmi3Float64 activationTime)
{
    assert(instance);
    UNUSED(clockReference);
    UNUSED(activationTime);

    return fmi3OK;
}
