// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>


#define UNUSED(x)      ((void)x)
#define ARRAY_SIZE(x)  (sizeof(x) / sizeof(x[0]))
#define VARNAME_MAXLEN 250


char* _get_measurement_file_name(ModelDesc* model)
{
    char* value = model_expand_vars(model, "${MEASUREMENT_FILE:-}");
    if (strlen(value)) {
        return value;
    } else {
        free(value);
        return NULL;
    }
}


static void __trace_sv(SignalVector* sv_save)
{
    for (SignalVector* sv = sv_save; sv && sv->name; sv++) {
        log_trace("SV Trace: name=%s (binary=%d)", sv->name, sv->is_binary);
        for (uint32_t i = 0; i < sv->count; i++) {
            if (sv->is_binary) {
                log_trace("  signal[%d] %s (len=%d,blen=%d,reset=%d)", i,
                    sv->signal[i], sv->length[i], sv->buffer_size[i],
                    sv->reset_called[i]);
            } else {
                log_trace(
                    "  signal[%d] %s: %f", i, sv->signal[i], sv->scalar[i]);
            }
        }
    }
}


ModelDesc* model_create(ModelDesc* model)
{
    int32_t  rc;
    MclDesc* m = mcl_create((void*)model);

    rc = mcl_load(m);
    if (rc != 0) log_fatal("Could not load MCL (%d)", rc);

    rc = mcl_init(m);
    if (rc != 0) log_fatal("Could not initiate MCL (%d)", rc);

    /* Initialise measurement. */
    FmuModel* fmu = (FmuModel*)m;
    fmu->measurement.file_name = _get_measurement_file_name(model);
    log_notice("Measurement File: %s", fmu->measurement.file_name);
    if (fmu->measurement.file_name) {
        errno = 0;
        fmu->measurement.file = fopen(fmu->measurement.file_name, "wb");
        if (fmu->measurement.file == NULL) {
            log_fatal("Failed to open measurement file: %s",
                fmu->measurement.file_name);
        }
        /* Configure the measurement interface. */
        size_t count = 0;
        for (size_t i = 0; i < m->source.count; i++) {
            if (m->source.kind[i] != MARSHAL_KIND_PRIMITIVE) {
                // Stop counting once signals of 'primitive' kind end. These
                // should be first in the list of signals (followed by signals
                // with 'binary' kind).
                break;
            }
            count++;
        }
        fmu->measurement.cg = calloc(1, sizeof(MdfChannelGroup));
        fmu->measurement.cg[0] = (MdfChannelGroup){
            .name = model->mi->name,
            .signal = m->source.signal,
            .scalar = m->source.scalar,
            .count = count,
        };
        fmu->measurement.mdf =
            mdf_create(fmu->measurement.file, fmu->measurement.cg, 1);
        mdf_start_blocks(&fmu->measurement.mdf);
    }

    /* Marshal FMU values after initialization from the simbus so that
       that they are available (to the simulation) in the first step. */
    rc = mcl_marshal_in(m);
    __trace_sv(model->sv);
    if (rc != 0) {
        log_error("Could not marshal initial fmu values (%d)", rc);
    }

    /* Return the extended object (FmuModel). */
    return (ModelDesc*)m;
}


int model_step(ModelDesc* model, double* model_time, double stop_time)
{
    int32_t   rc;
    MclDesc*  m = (MclDesc*)model;
    FmuModel* fmu = (FmuModel*)m;

    /* Call the measurement interface. */
    if (fmu->measurement.file) {
        mdf_write_records(&fmu->measurement.mdf, *model_time);
    }

    /* Step the FMU. */
    __trace_sv(model->sv);
    rc = mcl_marshal_out(m);
    if (rc != 0) return rc;

    rc = mcl_step(m, stop_time);
    if (rc != 0) return rc;

    rc = mcl_marshal_in(m);
    __trace_sv(model->sv);
    if (rc != 0) return rc;

    /* Advance the model time. */
    *model_time = stop_time;

    return 0;
}


void model_destroy(ModelDesc* model)
{
    int32_t  rc;
    MclDesc* m = (MclDesc*)model;

    /* Finalise measurement. */
    FmuModel* fmu = (FmuModel*)m;
    if (fmu->measurement.file) {
        fclose(fmu->measurement.file);
        fmu->measurement.file = NULL;
    }
    free(fmu->measurement.file_name);
    free(fmu->measurement.cg);

    /* Unload the MCL. */
    rc = mcl_unload((void*)m);
    if (rc != 0) log_fatal("Could not unload MCL (%d)", rc);

    mcl_destroy((void*)m);
}
