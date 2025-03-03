// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <limits.h>
#include <dse/clib/util/strings.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>

#define UNUSED(x) ((void)x)


static inline char* _get_fmu_env_value(
    FmuInstanceData* fmu, FmiGatewayEnvvar* e)
{
    if (strcmp(e->type, "string") == 0) {
        const char* value = hashmap_get(&fmu->variables.string.input, e->vref);
        if (value) return strdup(value);
    } else if (strcmp(e->type, "real") == 0) {
        double* d = hashmap_get(&fmu->variables.scalar.input, e->vref);
        if (d) {
            char* value = calloc(NUMERIC_ENVAR_LEN, sizeof(char));
            snprintf(value, NUMERIC_ENVAR_LEN, "%d", (int)*d);
            return value;
        }
    }
    return NULL;
}

static inline void _set_envar(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    for (FmiGatewayEnvvar* e = fmi_gw->settings.session->envar; e && e->name;
         e++) {
        const char* env_value = getenv(e->name);
        char* fmu_value = _get_fmu_env_value(fmu, e);

        /* Set the ENV in order of priority: FMU, ENV, default. */
        if (fmu_value) {
            fmigateway_setenv(e->name, fmu_value);
            free(fmu_value);
        } else if (env_value != NULL) {
            // NOP
        } else if (e->default_value != NULL) {
            fmigateway_setenv(e->name, e->default_value);
        }
    }
}

static inline void _run_cmd(FmuInstanceData* fmu, const char* cmd_string)
{
    _set_envar(fmu);

    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "cd %s && %s", fmu->instance.resource_location,
        cmd_string);
    fmu_log(fmu, 0, "Debug", "Run cmd: %s", cmd);

    if (system(cmd)) {
        fmu_log(fmu, 4, "Error", "Could not execute the script %s correctly.",
            cmd_string);
    };
}

/**
fmigateway_session_configure
============================

If session parameters were parsed from the model description, this method
configures and starts the additional models, or executes the given command.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmigateway_session_configure(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session == NULL) return;

    if (session->init_cmd) {
        _run_cmd(fmu, session->init_cmd);
    }

    if (session->w_models && strcmp(PLATFORM_OS, "windows") == 0) {
        fmigateway_session_windows_start(fmu);
    }
}


/**
fmigateway_session_end
======================

If session parameters were parsed from the model description, this method
shuts down the additional models, or executes the given command.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmigateway_session_end(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session == NULL) return;

    if (session->w_models && strcmp(PLATFORM_OS, "windows") == 0) {
        fmigateway_session_windows_end(fmu);
    }

    if (session->shutdown_cmd) {
        _run_cmd(fmu, session->shutdown_cmd);
    }
}


__attribute__((weak)) void fmigateway_session_windows_start(
    FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


__attribute__((weak)) void fmigateway_session_windows_end(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}
