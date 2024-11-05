// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>
#include <dse/fmimcl/adapter/fmi2mcl.h>


#define UNUSED(x) ((void)x)


#if defined(CMOCKA_TESTING)
extern void mock_create(FmuModel* m);
#endif


/**
fmimcl_adapter_create
=====================

This method creates an adapter object based on the configuration in the FMU
Model object.

Parameters
----------
fmu_model (FmuModel*)
: FMU Model descriptor object.

Returns
-------
0 (int32_t)
: The related adapter was loaded by the fmimcl.

-EINVAL (-22)
: No matching adapter found.
*/
int32_t fmimcl_adapter_create(FmuModel* fmu_model)
{
#if defined(CMOCKA_TESTING)
    if (strcmp(fmu_model->mcl.adapter, "mock") == 0) {
        if (strcmp(fmu_model->mcl.version, "1.0.0") == 0) {
            mock_create(fmu_model);
            return 0;
        }
    }
#endif
    if (strcmp(fmu_model->mcl.adapter, "fmi") == 0) {
        if (strncmp(fmu_model->mcl.version, "2.0", strlen("2.0")) == 0) {
            fmi2mcl_create(fmu_model);
            return 0;
        }
    }

    return -EINVAL;
}


/**
fmimcl_destroy
==============

Releases memory and system resources allocated by
FMI Model Compatibility Library.

Parameters
----------
fmu_model (FmuModel*)
: FMU Model descriptor object.
*/
void fmimcl_destroy(FmuModel* fmu_model)
{
    if (fmu_model == NULL) return;

    marshal_group_destroy(fmu_model->data.mg_table);
    if (fmu_model->signals) free(fmu_model->signals);
    if (fmu_model->data.name) free(fmu_model->data.name);
    if (fmu_model->data.scalar) free(fmu_model->data.scalar);  // also binary
    if (fmu_model->data.binary_len) free(fmu_model->data.binary_len);
    if (fmu_model->data.kind) free(fmu_model->data.kind);
}


/**
mcl_create
==========

Create an instance of the MCL which will then be used to operate the Model that
the MCL represents.

Parameters
----------
model (ModelDesc*)
: Model descriptor object.

Returns
-------
MclDesc (pointer)
: Object representing the MCL Model, an extended ModelDesc type (derived from
parameter `model`).

NULL
: The MCL Model could not be created. Inspect `errno` for more details.

Error Conditions
----------------

Available by inspection of `errno`.
*/
MclDesc* mcl_create(ModelDesc* model)
{
    FmuModel* m = calloc(1, sizeof(FmuModel));
    memcpy(m, model, sizeof(ModelDesc));

    dse_yaml_get_string(
        m->mcl.model.mi->model_definition.doc, "metadata/name", &m->name);

    fmimcl_parse(m);

    int32_t rc = fmimcl_adapter_create(m);
    if (rc != 0) {
        log_error("No matching FMI adapter was found!");
        return NULL;
    }

    fmimcl_allocate_source(m);
    fmimcl_generate_marshal_table(m);

    return (MclDesc*)m;
}


/**
mcl_destroy
===========

Releases memory and system resources allocated by `mcl_create()`.

Parameters
----------
model (ModelDesc*)
: Model descriptor object.
*/
void mcl_destroy(MclDesc* model)
{
    FmuModel* m = (FmuModel*)model;
    fmimcl_destroy(m);
}
