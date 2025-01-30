// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <dse/clib/util/strings.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>

#define UNUSED(x) ((void)x)


static inline void _set_envar(FmuInstanceData* fmu, HashMap* envars)
{
    char** names = hashmap_keys(envars);

    for (size_t i = 0; i < envars->used_nodes; i++) {
        const char* vref = hashmap_get(envars, names[i]);
        const char* value = hashmap_get(&fmu->variables.string.input, vref);
        fmu_log(fmu, 0, "Notice", "Set envar %s to %s", names[i], value);
        fmigateway_setenv(names[i], value);
    }
}

static inline void _run_cmd(
    FmuInstanceData* fmu, HashMap* envars, const char* cmd_string)
{
    _set_envar(fmu, envars);

    char* cmd = dse_path_cat(fmu->instance.resource_location, cmd_string);
    system(cmd);
    free(cmd);
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
        _run_cmd(fmu, &session->envar, session->init_cmd);
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

    if (session->shutdown_cmd) {
        _run_cmd(fmu, &session->envar, session->shutdown_cmd);
    }

    if (session->w_models && strcmp(PLATFORM_OS, "windows") == 0) {
        fmigateway_session_windows_end(fmu);
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
