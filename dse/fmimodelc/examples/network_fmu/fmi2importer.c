// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <fmi2FunctionTypes.h>


/* Define types for the FMI inteface methods being used. */
typedef void* (*fmi2Instantiate)();
typedef int32_t (*fmi2ExitInitializationMode)();
typedef int32_t (*fmi2GetReal)();
typedef int32_t (*fmi2GetString)();
typedef int32_t (*fmi2SetReal)();
typedef int32_t (*fmi2SetString)();
typedef int32_t (*fmi2DoStep)();
typedef void (*fmi2FreeInstance)();


/**
Importer for FMU2 with Model Runtime
====================================

This Importer is able to load and execute an FMU that includes the Model
Runtime (from ModelC) and with which it (the FMU) can run ModelC models.

This importer has no additional linked libraries, the Model Runtime is expected
to provide all necessary objects/symbols.

> Note: Specifically coded for the example `network_fmu`.
*/


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define STEP_SIZE     0.0005
#define END_TIME      600
#define STEPS         10


static void _log(const char* format, ...)
{
    printf("Importer: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}


int main(int argc, char** argv)
{
    if (argc < 2 || argc > 2) {
        _log("Usage: %s <fmu_lib_path>", argv[0]);
        _log("Run from the FMU base/root directory");
        return EINVAL;
    }
    const char* fmu_lib_path = argv[1];
    static char _cwd[PATH_MAX];
    getcwd(_cwd, PATH_MAX);
    _log("Cwd: %s", _cwd);

    /* Load the FMU
     * ============ */
    _log("Loading FMU: %s ...", fmu_lib_path);
    dlerror();
    void* handle = dlopen(fmu_lib_path, RTLD_NOW | RTLD_GLOBAL);
    if (handle == NULL) {
        _log("ERROR: dlopen call failed: %s", dlerror());
        _log("Model library not loaded!");
        return ENOSYS;
    }


    /* Setup the FMU
     * ============= */
    fmi2Component* fmu = NULL;

    /* fmi2Instantiate */
    fmi2Instantiate instantiate = dlsym(handle, "fmi2Instantiate");
    if (instantiate == NULL) return EINVAL;
    fmu = instantiate(
        "network_fmu", fmi2CoSimulation, "guid", "resources", NULL, true, false);
    if (fmu == NULL) return EINVAL;

    /* fmi2ExitInitializationMode */
    fmi2ExitInitializationMode exit_init_mode =
        dlsym(handle, "fmi2ExitInitializationMode");
    if (exit_init_mode == NULL) return EINVAL;
    exit_init_mode(fmu);


    /* Step the FMU
     * ============ */

    /* Load Get and Set functions. */
    fmi2GetReal get_real = dlsym(handle, "fmi2GetReal");
    if (get_real == NULL) return EINVAL;
    fmi2GetString get_string = dlsym(handle, "fmi2GetString");
    if (get_string == NULL) return EINVAL;
    fmi2SetReal set_real = dlsym(handle, "fmi2SetReal");
    if (set_real == NULL) return EINVAL;
    fmi2SetString set_string = dlsym(handle, "fmi2SetString");
    if (set_string == NULL) return EINVAL;

    /* fmi2DoStep */
    fmi2DoStep do_step = dlsym(handle, "fmi2DoStep");
    if (do_step == NULL) return EINVAL;

    double             model_time = 0.0;
    fmi2ValueReference vr_real[] = { 1 };
    fmi2Real           val_real[] = { 0.0 };
    assert(ARRAY_SIZE(vr_real) == ARRAY_SIZE(val_real));
    fmi2ValueReference vr_rx_string[] = { 2, 4, 6, 8 };
    fmi2ValueReference vr_tx_string[] = { 3, 5, 7, 9 };
    char*              val_rx_string[] = { NULL, NULL, NULL, NULL };
    char*              val_tx_string[] = { NULL, NULL, NULL, NULL };
    assert(ARRAY_SIZE(vr_rx_string) == ARRAY_SIZE(val_rx_string));
    assert(ARRAY_SIZE(vr_tx_string) == ARRAY_SIZE(val_tx_string));
    assert(ARRAY_SIZE(vr_rx_string) == ARRAY_SIZE(vr_tx_string));
    for (int i = 0; i < STEPS; i++) {
        /* Loopback the binary data. */
        for (size_t i = 0; i < ARRAY_SIZE(vr_tx_string); i++) {
            if (val_tx_string[i]) {
                val_rx_string[i] = val_tx_string[i];
                val_tx_string[i] = NULL;
            }
        }
        set_string(fmu, vr_rx_string, ARRAY_SIZE(vr_rx_string), val_rx_string);
        for (size_t i = 0; i < ARRAY_SIZE(vr_rx_string); i++) {
            /* Release the string (FMU should have duplicated). */
            if (val_rx_string[i]) {
                free(val_rx_string[i]);
                val_rx_string[i] = NULL;
            }
        }

        _log("Calling fmi2DoStep(): model_time=%f, step_size=%f", model_time,
            STEP_SIZE);
        int rc = do_step(fmu, model_time, STEP_SIZE);
        if (rc != 0) {
            _log("step() returned error code: %d", rc);
        }

        /* Read from FMU. */
        get_real(fmu, vr_real, ARRAY_SIZE(vr_real), val_real);
        get_string(fmu, vr_tx_string, ARRAY_SIZE(vr_tx_string), val_tx_string);
        for (size_t i = 0; i < ARRAY_SIZE(vr_tx_string); i++) {
            /* Duplicate received strings (in-case FMU releases). */
            if (val_tx_string[i]) {
                val_tx_string[i] = strdup(val_tx_string[i]);
            }
        }

        /* Increment model time. */
        model_time += STEP_SIZE;
    }
    _log("Scalar Variables:");
    for (size_t i = 0; i < ARRAY_SIZE(vr_real); i++) {
        _log("  [%d] %g", vr_real[i], val_real[i]);
    }
    _log("String Variables:");
    for (size_t i = 0; i < ARRAY_SIZE(vr_tx_string); i++) {
        _log("  [%d] %s", vr_tx_string[i], val_tx_string[i]);
    }


    /* Terminate/Free the FMU
     * ====================== */
    fmi2FreeInstance free_instance = dlsym(handle, "fmi2FreeInstance");
    if (free_instance == NULL) return EINVAL;
    free_instance(fmu);

    /* Release any remaining memory. */
    for (size_t i = 0; i < ARRAY_SIZE(val_tx_string); i++) {
        if (val_tx_string[i]) {
            free(val_tx_string[i]);
            val_tx_string[i] = NULL;
        }
    }
}
