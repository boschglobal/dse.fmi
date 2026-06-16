// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dse/clib/util/strings.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>

#define UNUSED(x) ((void)x)


static char* _get_fmu_env_value(FmuInstanceData* fmu, FmiGatewayParameter* e)
{
    if (strcmp(e->type, "String") == 0) {
        const char* value =
            hashmap_get(&fmu->variables.string.input, e->vref);  // NOLINT
        if (value) return strdup(value);
    } else if (strcmp(e->type, "Real") == 0) {
        double* value = hashmap_get(&fmu->variables.scalar.input, e->vref);
        if (value) {
            char* str_value = calloc(NUMERIC_ENVAR_LEN, sizeof(char));
            snprintf(str_value, NUMERIC_ENVAR_LEN, "%d", (int)*value);
            return str_value;
        }
    }
    return NULL;
}


static void _set_envar(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    for (FmiGatewayParameter* e = fmi_gw->settings.scripts.envar; e && e->name;
        e++) {
        const char* env_value = getenv(e->name);

        /* Set the ENV in order of priority: ENV, FMU, default. */
        if (env_value == NULL) {
            char* fmu_value = _get_fmu_env_value(fmu, e);
            if (fmu_value) {
                fmigateway_setenv(e->name, fmu_value);
                free(fmu_value);
            } else if (e->default_value != NULL) {
                fmigateway_setenv(e->name, e->default_value);
            }
        }
    }
}


static int _run_cmd(FmuInstanceData* fmu, const char* cmd_string)
{
    if (cmd_string == NULL) return 0;
    _set_envar(fmu);
    return fmigateway_run_cmd(fmu, cmd_string);
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
int fmigateway_session_start(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    char* startup = fmigateway_file_exists(fmu, "startup");
    if (startup == NULL && fmi_gw->settings.scripts.startup_cmd) {
        startup = strdup(fmi_gw->settings.scripts.startup_cmd);
    }
    int rc = _run_cmd(fmu, startup);
    free(startup);
    if (rc) {
        fmu_log(fmu, 0, "Debug", "Startup command failed: %d", rc);
        return rc;
    }

    /* Support future runtime types. */
    switch (fmi_gw->settings.runtime.type) {
    case FMIGATEWAY_RUNTIME_SIMER:
        fmigateway_run_simer(fmu);
        break;
    case FMIGATEWAY_RUNTIME_LEGACY:
    default:
        fmigateway_start_models(fmu);
        break;
    }

    return 0;
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
int fmigateway_session_end(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    /* Guard: if fmu_init was never called, model_gw_setup was never called
     * and model_gw_exit must not be called. But teardown must still be called.
     */
    if (fmi_gw->state < FMIGATEWAY_STATE_INITIALIZED) {
        fmigateway_teardown(fmu);
        return 0;
    }

    /* Support future runtime types. */
    switch (fmi_gw->settings.runtime.type) {
    case FMIGATEWAY_RUNTIME_SIMER:
        model_gw_exit(fmi_gw->model);
        fmigateway_stop_simer(fmu);
        break;
    case FMIGATEWAY_RUNTIME_LEGACY:
    default:
        FmiGatewaySession* session = fmi_gw->settings.session;
        if (session == NULL) {
            model_gw_exit(fmi_gw->model);
            return 0;
        }
        fmigateway_shutdown_models(fmu);
        break;
    }


    char* shutdown = fmigateway_file_exists(fmu, "shutdown");
    if (shutdown == NULL && fmi_gw->settings.scripts.shutdown_cmd) {
        shutdown = strdup(fmi_gw->settings.scripts.shutdown_cmd);
    }
    int rc = _run_cmd(fmu, shutdown);
    free(shutdown);
    if (rc) {
        fmu_log(fmu, 0, "Debug", "Shutdown command failed: %d", rc);
        return rc;
    }

    /* Teardown is used for cleaning up the parallelisation resources. */
    fmigateway_teardown(fmu);

    return 0;
}
