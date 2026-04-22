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


static char* _get_fmu_env_value(FmuInstanceData* fmu, FmiGatewayEnvvar* e)
{
    if (strcmp(e->type, "string") == 0) {
        const char* value = hashmap_get(&fmu->variables.string.input,  // NOLINT
            e->vref);
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


static void _set_envar(FmuInstanceData* fmu)
{
    FmiGateway* fmi_gw = fmu->data;

    for (FmiGatewayEnvvar* e = fmi_gw->settings.session->envar; e && e->name;
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

    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session == NULL) return 0;

    if (session->init_cmd) {
        if (strlen(session->init_cmd) > 0) {
            int rc = _run_cmd(fmu, session->init_cmd);
            if (rc) {
                fmu_log(fmu, 0, "Debug", "Init cmd failed: %d", rc);
                return rc;
            }
        }
    }

    fmigateway_start_models(fmu);

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

    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session == NULL) {
        model_gw_exit(fmi_gw->model);
        return 0;
    }

    fmigateway_shutdown_models(fmu);

    if (session->shutdown_cmd) {
        if (strlen(session->shutdown_cmd) > 0) {
            int rc = _run_cmd(fmu, session->shutdown_cmd);
            if (rc) return rc;
        }
    }

    return 0;
}
