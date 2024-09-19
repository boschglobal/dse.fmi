// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <fmi2Functions.h>
#include <fmi2FunctionTypes.h>
#include <fmi2TypesPlatform.h>
#include <dse/clib/collections/hashmap.h>


#define UNUSED(x) ((void)x)

typedef struct Fmu2InstanceData {
    /* FMI Instance Data. */
    const char*                  instance_name;
    fmi2Type                     interface_type;
    const char*                  resource_location;
    const char*                  guid;
    bool                         log_enabled;
    HashMap                      var;
    /* FMI Callbacks. */
    const fmi2CallbackFunctions* callbacks;
} Fmu2InstanceData;


/* required */
fmi2Component fmi2Instantiate(fmi2String instance_name, fmi2Type fmu_type,
    fmi2String fmu_guid, fmi2String fmu_resource_location,
    const fmi2CallbackFunctions* functions, fmi2Boolean visible,
    fmi2Boolean logging_on)
{
    UNUSED(visible);
    assert(functions);
    assert(functions->allocateMemory);
    assert(functions->freeMemory);

    /* Create the FMU Model Instance Data. */
    Fmu2InstanceData* fmu_inst =
        functions->allocateMemory(1, sizeof(Fmu2InstanceData));
    fmu_inst->instance_name = instance_name;
    fmu_inst->interface_type = fmu_type;
    fmu_inst->resource_location = fmu_resource_location;
    fmu_inst->guid = fmu_guid;
    fmu_inst->log_enabled = logging_on;
    fmu_inst->callbacks = functions;

    hashmap_init(&fmu_inst->var);
    hashmap_set_double(&fmu_inst->var, "0", 0.0);
    hashmap_set_double(&fmu_inst->var, "1", 0.0);
    hashmap_set_long(&fmu_inst->var, "2", 0);
    hashmap_set_long(&fmu_inst->var, "3", 0);
    hashmap_set_long(&fmu_inst->var, "4", 0);
    hashmap_set_double(&fmu_inst->var, "5", 0.0);
    /* Boolean. */
    hashmap_set_long(&fmu_inst->var, "6", 0);
    hashmap_set_long(&fmu_inst->var, "7", 0);
    /* Shared VRs. */
    hashmap_set_double(&fmu_inst->var, "8", 0);
    hashmap_set_double(&fmu_inst->var, "9", 0);
    hashmap_set_double(&fmu_inst->var, "10", 0);
    hashmap_set_double(&fmu_inst->var, "11", 0);
    /* Local variable. */
    hashmap_set_double(&fmu_inst->var, "12", 0);

    return (fmi2Component)fmu_inst;
}

/* required */
fmi2Status fmi2SetupExperiment(fmi2Component c, fmi2Boolean toleranceDefined,
    fmi2Real tolerance, fmi2Real startTime, fmi2Boolean stopTimeDefined,
    fmi2Real stopTime)
{
    UNUSED(toleranceDefined);
    UNUSED(tolerance);
    UNUSED(startTime);
    UNUSED(stopTimeDefined);
    UNUSED(stopTime);

    Fmu2InstanceData* fmu_inst = c;
    if (fmu_inst == NULL) return fmi2Fatal;

    return fmi2OK;
}

/* required */
fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
    assert(c);
    /* FMI Master at this point may call fmi2SetX() to adjust any
     * variables before the Model is started (in ExitInitialization).
     */
    Fmu2InstanceData* fmu_inst = c;
    if (fmu_inst == NULL) return fmi2Fatal;
    return fmi2OK;
}

/* required */
fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    Fmu2InstanceData* fmu_inst = c;
    if (fmu_inst == NULL) return fmi2Fatal;
    return fmi2OK;
}


/**
 *  FMI 2 Variable GET Interface.
 *
 *  Mapping of FMI Types to storage types:
 *
 *      fmi2Real -> STORAGE_DOUBLE
 *      fmi2Integer -> STORAGE_INT
 *      fmi2Boolean -> STORAGE_INT
 *      fmi2String -> STORAGE_STRING
 */
fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Real value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        // value[i] = fmu_inst->variables->Real[vr[i]];
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        double* val = hashmap_get(&fmu_inst->var, key);
        value[i] = *val;
    }
    return fmi2OK;
}

fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Integer value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        int* val = hashmap_get(&fmu_inst->var, key);
        value[i] = *val;
    }
    return fmi2OK;
}

fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Boolean value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        int* val = hashmap_get(&fmu_inst->var, key);
        value[i] = *val;
    }
    return fmi2OK;
}

fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2String value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        char** val = hashmap_get(&fmu_inst->var, key);
        value[i] = *val;
    }
    return fmi2OK;
}

/**
 *  FMI 2 Variable SET Interface.
 *
 *  Mapping of FMI Types to storage types:
 *
 *      fmi2Real -> STORAGE_DOUBLE
 *      fmi2Integer -> STORAGE_INT
 *      fmi2Boolean -> STORAGE_INT
 *      fmi2String -> STORAGE_STRING
 */
fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Real value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        hashmap_set_double(&fmu_inst->var, key, value[i]);
    }
    return fmi2OK;
}

fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Integer value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        hashmap_set_long(&fmu_inst->var, key, value[i]);
    }
    return fmi2OK;
}

fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Boolean value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        hashmap_set_long(&fmu_inst->var, key, value[i]);
    }
    return fmi2OK;
}

fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2String value[])
{
    Fmu2InstanceData* fmu_inst = c;
    for (size_t i = 0; i < nvr; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%d", vr[i]);
        hashmap_set_string(&fmu_inst->var, key, (char*)value[i]);
    }
    return fmi2OK;
}


/* COSIM Interface. */
fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint,
    fmi2Real    communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint)
{
    UNUSED(noSetFMUStatePriorToCurrentPoint);
    UNUSED(currentCommunicationPoint);
    UNUSED(communicationStepSize);
    Fmu2InstanceData* fmu_inst = c;

    double* vr_0 = hashmap_get(&fmu_inst->var, "0");
    double* vr_1 = hashmap_get(&fmu_inst->var, "1");
    *vr_1 = *vr_0 + *vr_1 + 1;

    int* vr_2 = hashmap_get(&fmu_inst->var, "2");
    int* vr_3 = hashmap_get(&fmu_inst->var, "3");
    *vr_3 = *vr_2 + *vr_3 + 1;

    int* vr_6 = hashmap_get(&fmu_inst->var, "6");
    int* vr_7 = hashmap_get(&fmu_inst->var, "7");
    if (*vr_6 == true) {
        *vr_7 = true;
    } else {
        *vr_7 = false;
    }

    double* vr_8 = hashmap_get(&fmu_inst->var, "8");
    double* vr_9 = hashmap_get(&fmu_inst->var, "9");
    double* vr_10 = hashmap_get(&fmu_inst->var, "10");
    double* vr_11 = hashmap_get(&fmu_inst->var, "11");
    *vr_10 = *vr_8 + 10.0;
    *vr_11 = *vr_9 + 100.0;

    /* Keep track on how many times the fmu ran. */
    int* vr_4 = hashmap_get(&fmu_inst->var, "4");
    *vr_4 += 1;

    /* Local vars too. */
    double* vr_12 = hashmap_get(&fmu_inst->var, "12");
    *vr_12 = 12000 + *vr_4;  // Offset step count.

    return fmi2OK;
}

/* Lifecycle interface. */
fmi2Status fmi2Terminate(fmi2Component c)
{
    UNUSED(c);
    return fmi2OK;
}

void fmi2FreeInstance(fmi2Component c)
{
    Fmu2InstanceData* fmu_inst = c;
    hashmap_destroy(&fmu_inst->var);
    fmu_inst->callbacks->freeMemory(fmu_inst);
}
