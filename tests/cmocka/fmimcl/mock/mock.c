// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdlib.h>
#include <string.h>
#include <dse/logger.h>
#include <dse/fmimcl/fmimcl.h>
#include <mock/mock.h>


void mock_create(FmuModel* m)
{
    /* Complete the MCL interface VTable. */
    m->mcl.vtable.load = mock_mcl_load;
    m->mcl.vtable.init = mock_mcl_init;
    m->mcl.vtable.step = mock_mcl_step;
    m->mcl.vtable.marshal_out = mock_mcl_marshal_out;
    m->mcl.vtable.marshal_in = mock_mcl_marshal_in;
    m->mcl.vtable.unload = mock_mcl_unload;

    /* Create adapter object. */
    m->adapter = calloc(1, sizeof(MockAdapterDesc));
}


int32_t mock_mcl_load(MclDesc* mcl)
{
    FmuModel*        m = (FmuModel*)mcl;
    MockAdapterDesc* a = m->adapter;
    if (a && a->expect_rc) return a->expect_rc + 1;
    return -1;
}

int32_t mock_mcl_init(MclDesc* mcl)
{
    FmuModel*        m = (FmuModel*)mcl;
    MockAdapterDesc* a = m->adapter;
    if (a && a->expect_rc) return a->expect_rc + 2;
    return -1;
}

int32_t mock_mcl_step(MclDesc* mcl, double* model_time, double end_time)
{
    FmuModel*        m = (FmuModel*)mcl;
    MockAdapterDesc* a = m->adapter;
    for (MarshalSignalMap* msm = m->mcl.msm; msm && msm->name; msm++) {
        log_trace("msm name: %s", msm->name);
        if (msm->is_binary) {
            for (size_t j = 0; j < msm->count; j++) {
                // Reverse the source (string).
                log_trace("  source: %s", msm->source.binary[j]);
                char*  _str = strdup(msm->source.binary[j]);
                size_t _len = strlen(_str);
                for (size_t i = 0; i < _len; i++) {
                    ((char**)msm->source.binary)[j][i] = _str[_len - i - 1];
                }
                free(_str);
            }
        } else {
            double* src_scalar = msm->source.scalar;
            for (size_t j = 0; j < msm->count; j++) {
                src_scalar[msm->source.index[j]] += 1;
            }
        }
    }

    a->expect_step += 1;
    *model_time += (end_time - *model_time);
    if (a && a->expect_rc) {
        return a->expect_rc + 3 + a->expect_step;
    }
    return -1;
}

int32_t mock_mcl_marshal_out(MclDesc* mcl)
{
    FmuModel*        m = (FmuModel*)mcl;
    MockAdapterDesc* a = m->adapter;
    if (a && a->expect_rc) return a->expect_rc + 3;
    return -1;
}

int32_t mock_mcl_marshal_in(MclDesc* mcl)
{
    FmuModel*        m = (FmuModel*)mcl;
    MockAdapterDesc* a = m->adapter;
    if (m->mcl.msm) return 0;
    if (a && a->expect_rc) return a->expect_rc + 5;
    return -1;
}

int32_t mock_mcl_unload(MclDesc* mcl)
{
    FmuModel*        m = (FmuModel*)mcl;
    MockAdapterDesc* a = m->adapter;

    // Determine the return value before destroying the adapter object.
    uint32_t rc = -1;
    if (a && a->expect_rc) rc = 437;

    // Continue ...
    if (m->adapter) free(m->adapter);
    return rc;
}
