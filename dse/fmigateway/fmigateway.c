// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/clib/util/strings.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x) ((void)x)


/**
fmu_create
==========

This method allocates the necessary gateway models. The location of the required
yaml files is set and allocated.

Fault conditions can be communicated to the caller by setting variable
`errno` to a non-zero value.

> Required by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

Returns
-------
NULL
: The FMU was configured.

(FmuInstanceData*)
: Pointer to a new, or mutilated, version of the Fmu Descriptor object. The
  original Fmu Descriptor object will be released by the higher layer (i.e.
  don't call `free()`).

errno <> 0 (indirect)
: Indicates an error condition.

*/
FmuInstanceData* fmu_create(FmuInstanceData* fmu)
{
    assert(fmu);
    /* Allocate the ModelGatewayDesc object. */
    FmiGateway* fmi_gw = calloc(1, sizeof(FmiGateway));
    *fmi_gw = (FmiGateway){
        .model = calloc(1, sizeof(ModelGatewayDesc)),
        .settings.yaml_files = calloc(4, sizeof(char*)),
    };

    /* Allocate a NTL for the required files. */
    fmi_gw->settings.yaml_files[0] =
        dse_path_cat(fmu->instance.resource_location, "model.yaml");
    fmi_gw->settings.yaml_files[1] =
        dse_path_cat(fmu->instance.resource_location, "fmu.yaml");
    fmi_gw->settings.yaml_files[2] =
        dse_path_cat(fmu->instance.resource_location, "stack.yaml");

    fmu->data = (void*)fmi_gw;

    /* Parse the yaml files. */
    fmigateway_parse(fmu);

    return fmu;
}


/**
fmu_init
========

In this method the required yaml files are parsed and the session is configured,
if required. The gateway is set up and connected to the simbus. After a
sucessfull connection has been established, the fmu variables are indexed to
their corresponding simbus signals.

> Required by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

Returns
-------
0 (int32_t)
: The FMU was created correctly.
*/
int32_t fmu_init(FmuInstanceData* fmu)
{
    assert(fmu);
    FmiGateway* fmi_gw = fmu->data;
    assert(fmi_gw);
    ModelGatewayDesc* gw = fmi_gw->model;
    assert(gw);
    int rc;

    rc = fmigateway_session_configure(fmu);
    if (rc) return rc;

    /* Setup the Model Gateway object. */
    fmu_log(fmu, 0, "Debug", "Setting up the Simbus connection...");
    rc = model_gw_setup(gw, "gateway", fmi_gw->settings.yaml_files,
        fmi_gw->settings.log_level, fmi_gw->settings.step_size,
        fmi_gw->settings.end_time);
    if (rc) return rc;

    fmu_log(fmu, 0, "Debug", "Connected to the Simbus...");

    fmigateway_index_scalar_signals(
        fmu, gw, &fmu->variables.scalar.input, &fmu->variables.scalar.output);
    fmigateway_index_binary_signals(
        fmu, gw, &fmu->variables.binary.rx, &fmu->variables.binary.tx);
    fmigateway_index_text_encoding(fmu, gw, &fmu->variables.binary.encode_func,
        &fmu->variables.binary.decode_func);

    return 0;
}


/**
fmu_step
========

This method executes one step of the gateway model and signals are exchanged
with the other simulation participants.

> Required by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
communication_point (double)
: The current model time of the FMU in seconds.
step_size (double)
: The step size of the FMU in seconds.

Returns
-------
0 (int32_t)
: The FMU step was performed correctly.

*/
int32_t fmu_step(
    FmuInstanceData* fmu, double communication_point, double step_size)
{
    assert(fmu);
    FmiGateway* fmi_gw = fmu->data;
    assert(fmi_gw);
    ModelGatewayDesc* gw = fmi_gw->model;
    assert(gw);

    /* Step the model. */
    int rc = model_gw_sync(gw, communication_point);
    if (rc == E_GATEWAYBEHIND) {
        return 0;
    }

    /* Save current step for shutdown process. */
    if (fmi_gw->settings.session) {
        fmi_gw->settings.session->last_step = communication_point;
        fmi_gw->settings.step_size = step_size;
    }

    return 0;
}


/**
fmu_destroy
===========

Releases memory and system resources allocated by gateway.

> Required by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

Returns
-------
0 (int32_t)
: The FMU data was released correctly.
*/
int32_t fmu_destroy(FmuInstanceData* fmu)
{
    assert(fmu);
    FmiGateway* fmi_gw = fmu->data;
    assert(fmi_gw);
    ModelGatewayDesc* gw = fmi_gw->model;
    assert(gw);

    fmigateway_session_end(fmu);

    /* Disconnect from the simbus. */
    model_gw_exit(gw);

    /* Cleanup */
    dse_yaml_destroy_doc_list(fmi_gw->settings.doc_list);
    for (size_t i = 0; fmi_gw->settings.yaml_files[i]; i++) {
        free((char*)fmi_gw->settings.yaml_files[i]);
    }

    FmiGatewaySession* session = fmi_gw->settings.session;
    if (session) {
        /* Cleanup Simbus model. */
        if (session->simbus) {
            free(session->simbus->name);
            free(session->simbus->yaml);
            free(session->simbus->envar);
        }
        free(session->simbus);
        /* Cleanup transport model. */
        if (session->transport) free(session->transport->name);
        free(session->transport);
        /* Cleanup model stack files. */
        dse_yaml_destroy_doc_list(session->model_stack_files);
        /* Cleanup ModelC models. */
        for (WindowsModel* model = session->w_models; model && model->name;
            model++) {
            free(model->envar);
            free(model->yaml);
            free(model->name);
        }
        free(session->w_models);

        for (FmiGatewayEnvvar* e = session->envar; e && e->name; e++) {
            free(e->vref);
            free(e->default_value);
        }
        free(session->envar);
        free(session);
    }
    free(fmi_gw->settings.yaml_files);
    free(gw);
    free(fmi_gw);

    return 0;
}
