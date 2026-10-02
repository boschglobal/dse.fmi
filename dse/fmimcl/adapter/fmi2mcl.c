// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>
#include <dse/fmimcl/adapter/fmi2mcl.h>
#include <dse/logger.h>


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define UNUSED(x)     ((void)x)

static int32_t fmi2mcl_marshal_out(FmuModel* m);


/**
FMI2 Model Compatibility Library
================================

*/

static void fmu2_logger_callback(fmi2ComponentEnvironment componentEnvironment,
    fmi2String instanceName, fmi2Status status, fmi2String category,
    fmi2String message, ...)
{
    UNUSED(componentEnvironment);
    UNUSED(instanceName);
    UNUSED(status);
    if (LOG_LEVEL > LOG_DEBUG) return;

    static char buffer[2048];
    va_list     ap;
    va_start(ap, message);
    vsnprintf(buffer, sizeof(buffer), message, ap);
    va_end(ap);

    log_debug("FMU LOG:%s:%s", category, buffer);
}


static void fmu2_step_finished_callback(
    fmi2ComponentEnvironment componentEnvironment, fmi2Status status)
{
    UNUSED(componentEnvironment);
    UNUSED(status);
}


/* Track error/fatal states (FMI 2.0 section 2.1.3); warnings are not errors. */
static int32_t _check(FmuModel* m, int32_t status, const char* func)
{
    if (status == fmi2OK || status == fmi2Warning) return 0;

    errno = 0; /* Otherwise log_error() prints an unrelated stale errno. */
    log_error("%s returned status %d", func, status);
    if (status == fmi2Fatal) {
        m->runtime.state = FMU_STATE_FATAL;
    } else if (status != fmi2Discard) {
        m->runtime.state = FMU_STATE_ERROR;
    }
    return EBADMSG;
}


static void _trace_values(const char* op, MarshalGroup* mg, size_t i, size_t n)
{
    if (LOG_LEVEL > LOG_TRACE) return;

    log_trace(
        "  %s (name: %s, count: %zu, type: %d)", op, mg->name, n, mg->type);
    for (size_t k = i; k < i + n; k++) {
        uint32_t vr = mg->target.ref[k];
        switch (mg->type) {
        case MARSHAL_TYPE_DOUBLE:
            log_trace("    vr[%u]=%f", vr, mg->target._double[k]);
            break;
        case MARSHAL_TYPE_INT32:
        case MARSHAL_TYPE_BOOL:
            log_trace("    vr[%u]=%d", vr, mg->target._int32[k]);
            break;
        case MARSHAL_TYPE_STRING:
            log_trace("    vr[%u]=%s", vr, mg->target._string[k]);
            break;
        default:
            break;
        }
    }
}


static const char* const fmi2_func_names[] = {
    "fmi2Instantiate",
    "fmi2SetupExperiment",
    "fmi2EnterInitializationMode",
    "fmi2ExitInitializationMode",
    "fmi2GetReal",
    "fmi2GetInteger",
    "fmi2GetBoolean",
    "fmi2GetString",
    "fmi2SetReal",
    "fmi2SetInteger",
    "fmi2SetBoolean",
    "fmi2SetString",
    "fmi2DoStep",
    "fmi2Terminate",
    "fmi2FreeInstance",
};


/* can be perhaps reused for other adapter */
static inline int _get_func(void* handle, const char* name, void** func)
{
    *func = dlsym(handle, name);
    if (*func == NULL) {
        log_error("Could not load fmi2 function: %s (%s)", name, dlerror());
        return EINVAL;
    }
    return 0;
}


static int32_t fmi2mcl_load(FmuModel* m)
{
    int          rc = 0;
    Fmi2Adapter* a = m->adapter;

    log_debug("Load fmu from path: %s", m->model_path);
    a->dl_handle = dlopen(m->model_path, RTLD_NOW | RTLD_LOCAL);
    if (a->dl_handle == NULL) {
        log_error("%s", dlerror());
        return -1;
    }

    size_t len = ARRAY_SIZE(fmi2_func_names);
    if (len != sizeof(Fmi2VTable) / sizeof(void*)) return EINVAL;

    void** vt = (void**)&a->vtable;
    for (size_t i = 0; i < len; i++) {
        rc |= _get_func(a->dl_handle, fmi2_func_names[i], &vt[i]);
    }
    if (rc != 0) {
        log_error("Not all fmi2 functions loaded!");
        return rc;
    }

    a->callbacks.allocateMemory = calloc;
    a->callbacks.freeMemory = free;
    a->callbacks.logger = fmu2_logger_callback;
    a->callbacks.stepFinished = fmu2_step_finished_callback;

    return 0;
}


static int32_t _get_values(FmuModel* m, MarshalGroup* mg)
{
    Fmi2Adapter* a = m->adapter;
    uint32_t*    vr = mg->target.ref;
    size_t       n = mg->count;
    const char*  func;
    int32_t      status;

    switch (mg->type) {
    case MARSHAL_TYPE_DOUBLE:
        func = "fmi2GetReal";
        status = a->vtable.get_real(a->fmi2_inst, vr, n, mg->target._double);
        break;
    case MARSHAL_TYPE_INT32:
        func = "fmi2GetInteger";
        status = a->vtable.get_integer(a->fmi2_inst, vr, n, mg->target._int32);
        break;
    case MARSHAL_TYPE_BOOL:
        func = "fmi2GetBoolean";
        status = a->vtable.get_boolean(a->fmi2_inst, vr, n, mg->target._int32);
        break;
    case MARSHAL_TYPE_STRING:
        func = "fmi2GetString";
        status = a->vtable.get_string(a->fmi2_inst, vr, n, mg->target._string);
        break;
    default:
        return 0;
    }
    int32_t rc = _check(m, status, func);
    if (rc == 0) _trace_values("get", mg, 0, n);
    return rc;
}


static int32_t _set_values(FmuModel* m, MarshalGroup* mg, size_t i, size_t n)
{
    Fmi2Adapter* a = m->adapter;
    uint32_t*    vr = &mg->target.ref[i];
    const char*  func;
    int32_t      status;

    _trace_values("set", mg, i, n);
    switch (mg->type) {
    case MARSHAL_TYPE_DOUBLE:
        func = "fmi2SetReal";
        status =
            a->vtable.set_real(a->fmi2_inst, vr, n, &mg->target._double[i]);
        break;
    case MARSHAL_TYPE_INT32:
        func = "fmi2SetInteger";
        status =
            a->vtable.set_integer(a->fmi2_inst, vr, n, &mg->target._int32[i]);
        break;
    case MARSHAL_TYPE_BOOL:
        func = "fmi2SetBoolean";
        status =
            a->vtable.set_boolean(a->fmi2_inst, vr, n, &mg->target._int32[i]);
        break;
    case MARSHAL_TYPE_STRING:
        func = "fmi2SetString";
        status =
            a->vtable.set_string(a->fmi2_inst, vr, n, &mg->target._string[i]);
        break;
    default:
        return 0;
    }
    return _check(m, status, func);
}


/* The FMU already holds its start values (FMI 2.0 section 2.2.7). */
static bool _is_start_value(MarshalGroup* mg, size_t k, FmuSignal* s)
{
    const char* v = s->variable_start_value;
    if (v == NULL) return false;

    size_t idx = mg->source.offset + k;
    switch (mg->type) {
    case MARSHAL_TYPE_DOUBLE:
    case MARSHAL_TYPE_INT32:
        return mg->source.scalar[idx] == strtod(v, NULL);
    case MARSHAL_TYPE_BOOL:
        return mg->source.scalar[idx] ==
               ((strcmp(v, "true") == 0 || strtod(v, NULL) != 0) ? 1 : 0);
    case MARSHAL_TYPE_STRING: {
        const char* src = mg->source.binary[idx];
        return src && strcmp(src, v) == 0;
    }
    default:
        return false;
    }
}


/* Compares against target, which holds the value last passed to the FMU. */
static bool _is_changed(MarshalGroup* mg, size_t k)
{
    size_t idx = mg->source.offset + k;
    switch (mg->type) {
    case MARSHAL_TYPE_DOUBLE:
        return mg->target._double[k] != mg->source.scalar[idx];
    case MARSHAL_TYPE_INT32:
    case MARSHAL_TYPE_BOOL:
        return mg->target._int32[k] != (int32_t)mg->source.scalar[idx];
    case MARSHAL_TYPE_STRING: {
        const char* src = mg->source.binary[idx];
        const char* tgt = mg->target._string[k];
        if (src == NULL || tgt == NULL) return src != tgt;
        return strcmp(src, tgt) != 0;
    }
    default:
        return true;
    }
}


static bool _is_group_changed(MarshalGroup* mg)
{
    /* DOUBLE source and target are contiguous and unconverted: one memcmp. */
    if (mg->type == MARSHAL_TYPE_DOUBLE) {
        return memcmp(mg->target._double, &mg->source.scalar[mg->source.offset],
                   mg->count * sizeof(double)) != 0;
    }
    for (size_t i = 0; i < mg->count; i++) {
        if (_is_changed(mg, i)) return true;
    }
    return false;
}


/* FMI 2.0.5 section 4.2.4: parameters are settable in instantiated, tunable
   parameters also in stepComplete (here only when changed). */
static bool _is_settable(FmuModel* m, MarshalGroup* mg, size_t k)
{
    Fmi2Adapter* a = m->adapter;
    size_t       idx = mg->source.offset + k;
    FmuSignal*   s = &m->signals[idx];

    switch (m->runtime.state) {
    case FMU_STATE_INSTANTIATED:
        return !_is_start_value(mg, k, s);
    case FMU_STATE_RUN:
        return a->changed[idx] &&
               s->variable_variability == MARSHAL_VARIABILITY_TUNABLE;
    default:
        return false;
    }
}


static int32_t fmi2mcl_init(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;
    int32_t      rc;

    /* Parameter change flags, covering every group's source range. */
    size_t n = 0, n_mg = 0;
    for (MarshalGroup* mg = m->data.mg_table; mg && mg->name; mg++, n_mg++) {
        if (mg->source.offset + mg->count > n) {
            n = mg->source.offset + mg->count;
        }
    }
    a->changed = calloc(n ? n : 1, sizeof(bool));
    a->mg_changed = calloc(n_mg ? n_mg : 1, sizeof(bool));

    a->fmi2_inst = a->vtable.instantiate(m->name, m->cosim, m->guid,
        m->resource_dir, &(a->callbacks), fmi2False,
        (LOG_LEVEL <= LOG_DEBUG) ? fmi2True : fmi2False);
    if (a->fmi2_inst == NULL) {
        log_error("FMI2 Instance could not be created.");
        return EINVAL;
    }
    m->runtime.state = FMU_STATE_INSTANTIATED;

    rc = _check(m,
        a->vtable.setup_experiment(
            a->fmi2_inst, fmi2False, 0.0, m->mcl.model_time, fmi2False, 0.0),
        "fmi2SetupExperiment");
    if (rc) return rc;

    rc = fmi2mcl_marshal_out(m);
    if (rc) return rc;

    rc = _check(m, a->vtable.enter_initialization(a->fmi2_inst),
        "fmi2EnterInitializationMode");
    if (rc) return rc;
    m->runtime.state = FMU_STATE_INIT;

    rc = fmi2mcl_marshal_out(m);
    if (rc) return rc;

    rc = _check(m, a->vtable.exit_initialization(a->fmi2_inst),
        "fmi2ExitInitializationMode");
    if (rc) return rc;
    m->runtime.state = FMU_STATE_RUN;

    return 0;
}


static int32_t fmi2mcl_step(FmuModel* m, double* model_time, double end_time)
{
    log_trace("Step: model_time: %f, end_time: %f", *model_time, end_time);

    Fmi2Adapter* a = m->adapter;
    if (m->runtime.state != FMU_STATE_RUN) return EBADMSG;

    int32_t status = a->vtable.do_step(
        a->fmi2_inst, *model_time, (end_time - *model_time), fmi2True);
    int32_t rc = _check(m, status, "fmi2DoStep");
    if (status == fmi2Discard) m->runtime.state = FMU_STATE_STEP_FAILED;
    if (rc) return rc;

    *model_time = end_time;
    return 0;
}


static int32_t fmi2mcl_marshal_in(FmuModel* m)
{
    if (m->runtime.state != FMU_STATE_RUN) return 0;

    log_trace("Marshal IN (FMU -> target):");
    for (MarshalGroup* mg = m->data.mg_table; mg && mg->name; mg++) {
        switch (mg->dir) {
        case MARSHAL_DIRECTION_TXRX:
        case MARSHAL_DIRECTION_RXONLY:
        case MARSHAL_DIRECTION_LOCAL:
            break;
        default:
            continue;
        }
        int32_t rc = _get_values(m, mg);
        if (rc) return rc;
    }

    marshal_group_in(NULL, m->data.mg_table);

    return 0;
}


static int32_t fmi2mcl_marshal_out(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;
    FmuState     state = m->runtime.state;
    int32_t      rc = 0;
    size_t       g;

    /* Detect changes before marshal_group_out() overwrites the target. */
    if (m->signals && state == FMU_STATE_RUN) {
        g = 0;
        for (MarshalGroup* mg = m->data.mg_table; mg && mg->name; mg++, g++) {
            a->mg_changed[g] =
                mg->dir == MARSHAL_DIRECTION_PARAMETER && _is_group_changed(mg);
            if (!a->mg_changed[g]) continue;
            /* Per-element flags only for groups that changed (rare). */
            for (size_t i = 0; i < mg->count; i++) {
                a->changed[mg->source.offset + i] = _is_changed(mg, i);
            }
        }
    }

    marshal_group_out(NULL, m->data.mg_table);
    if (m->signals == NULL) return 0;

    log_trace("Marshal OUT (target -> FMU):");
    g = 0;
    for (MarshalGroup* mg = m->data.mg_table; rc == 0 && mg && mg->name;
        mg++, g++) {
        switch (mg->dir) {
        case MARSHAL_DIRECTION_TXONLY:
            /* Section 4.2.4: inputs are settable in initializationMode and
               stepComplete; always set as they usually change every step. */
            if (state == FMU_STATE_INIT || state == FMU_STATE_RUN) {
                rc = _set_values(m, mg, 0, mg->count);
            }
            break;
        case MARSHAL_DIRECTION_PARAMETER: {
            if (state == FMU_STATE_RUN && !a->mg_changed[g]) break;
            /* Batch each contiguous run of settable parameters in one call. */
            size_t start = 0;
            for (size_t i = 0; rc == 0 && i <= mg->count; i++) {
                if (i < mg->count && _is_settable(m, mg, i)) continue;
                if (i > start) rc = _set_values(m, mg, start, i - start);
                start = i + 1;
            }
            break;
        }
        default:
            break;
        }
    }

    return rc;
}


static int32_t fmi2mcl_unload(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;

    /* fmi2Terminate is allowed in stepComplete and stepFailed only. */
    if (m->runtime.state == FMU_STATE_RUN ||
        m->runtime.state == FMU_STATE_STEP_FAILED) {
        _check(m, a->vtable.terminate(a->fmi2_inst), "fmi2Terminate");
    }
    /* No FMI calls are allowed after fmi2Fatal. */
    if (a->fmi2_inst && m->runtime.state != FMU_STATE_FATAL) {
        a->vtable.free_instance(a->fmi2_inst);
    }
    m->runtime.state = FMU_STATE_TERMINATED;

    if (a->dl_handle) dlclose(a->dl_handle);
    free(a->changed);
    free(a->mg_changed);
    free(m->adapter);
    m->adapter = NULL;

    return 0;
}


/**
fmi2mcl_create
===========

This functions sets the specific adapter functions in the Vtable of the MCL.

Parameters
----------
fmu_model (FmuModel*)
: Fmu Model descriptor object.

*/
void fmi2mcl_create(FmuModel* m)
{
    m->mcl.vtable = (struct MclVTable){
        .load = (MclLoad)fmi2mcl_load,
        .init = (MclInit)fmi2mcl_init,
        .step = (MclStep)fmi2mcl_step,
        .marshal_out = (MclMarshalOut)fmi2mcl_marshal_out,
        .marshal_in = (MclMarshalIn)fmi2mcl_marshal_in,
        .unload = (MclUnload)fmi2mcl_unload,
    };

    m->adapter = calloc(1, sizeof(Fmi2Adapter));
}
