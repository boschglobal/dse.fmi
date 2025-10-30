// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/clib/util/strings.h>
#include <dse/fmu/fmu.h>
#include <dse/fmimodelc/fmimodelc.h>


#define UNUSED(x) ((void)x)
#define END_TIME  (3 * 24 * 60 * 60)


/**
fmu_create
==========

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
    RuntimeModelDesc* m = calloc(1, sizeof(RuntimeModelDesc));

    /* Create the Model Runtime object. */
    fmu_log(fmu, 0, "Debug", "Create the Model Runtime object");
    *m = (RuntimeModelDesc){
        .runtime = {
            /* Logging/Information parameters (Importer only). */
            .runtime_model = fmu->instance.name,
            .model_name = fmu->instance.name,
            /* Operational parameters. */
            .sim_path = dse_path_cat(fmu->instance.resource_location, "sim"),
            .simulation_yaml = "data/simulation.yaml",
            .end_time = END_TIME,
            .log_level = 5, // Adjust log level via env:SIMBUS_LOGLEVEL.
            /* VTable callbacks (Importer provided). */
            .vtable = {
                .set_env = fmimodelc_set_model_env,
            },
        },
    };
    m->model.sim = calloc(1, sizeof(SimulationSpec));
    fmu_log(fmu, 0, "Debug", "Call model_runtime_create() ...");
    m = model_runtime_create(m);

    fmu->data = (void*)m;

    return NULL;
}


/**
fmu_init
========

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
    RuntimeModelDesc* m = fmu->data;
    assert(m);

    fmu_log(fmu, 0, "Debug", "Build indexes");
    fmimodelc_index_scalar_signals(
        m, &fmu->variables.scalar.input, &fmu->variables.scalar.output);
    fmimodelc_index_binary_signals(
        m, &fmu->variables.binary.rx, &fmu->variables.binary.tx);
    fmimodelc_index_text_encoding(m, &fmu->variables.binary.encode_func,
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
    RuntimeModelDesc* m = fmu->data;
    assert(m);

    /* Step the model. */
    double model_time = communication_point;
    fmu_log(fmu, 0, "Debug", "Call model_runtime_step() ...");
    int rc =
        model_runtime_step(m, &model_time, communication_point + step_size);

    return (rc == 0 ? 0 : 1);
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
    RuntimeModelDesc* m = fmu->data;
    assert(m);

    fmu_log(fmu, 0, "Debug", "Call model_runtime_destroy() ...");
    free(m->runtime.sim_path);
    model_runtime_destroy(m);
    free(m->model.sim);
    free(m);

    return 0;
}
