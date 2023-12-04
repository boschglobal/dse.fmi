// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/fmimcl/fmimcl.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


ModelDesc* model_create(ModelDesc* model)
{
    int32_t  rc;
    MclDesc* m = mcl_create((void*)model);

    rc = mcl_load(m);
    if (rc != 0) log_fatal("Could not load MCL (%d)", rc);

    rc = mcl_init(m);
    if (rc != 0) log_fatal("Could not initiate MCL (%d)", rc);

    /* Return the extended object. */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    int32_t  rc;
    MclDesc* m = (MclDesc*)model;

    rc = mcl_marshal_out(m);
    if (rc != 0) return rc;

    rc = mcl_step(m, stop_time);
    if (rc != 0) return rc;

    rc = mcl_marshal_in(m);
    if (rc != 0) return rc;

    /* Advance the model time. */
    *model_time = stop_time;

    return 0;
}


void model_destroy(ModelDesc* model)
{
    int32_t  rc;
    MclDesc* m = (MclDesc*)model;

    rc = mcl_unload((void*)m);
    if (rc != 0) log_fatal("Could not unload MCL (%d)", rc);

    mcl_destroy((void*)m);
}
