// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dse/fmimcl/fmimcl.h>
#include <dse/fmimcl/adapter/fmi2mcl.h>
#include <dse/logger.h>


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define UNUSED(x)     ((void)x)

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


const char* fmi2_func_names[] = {
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
    if (func == NULL) return EINVAL;

    /* try to find the function in the shared lib */
    *func = dlsym(handle, name);
    char* dl_error = dlerror();
    if (dl_error != NULL) {
        *func = NULL;
        log_error("Could not load fmi2 function: %s (%s)", name, dl_error);
        return EINVAL;
    }
    return 0;
}


static int32_t fmi2mcl_load(FmuModel* m)
{
    char*        dlerror_str;
    void*        handle;
    int          rc = 0;
    Fmi2Adapter* a = m->adapter;

    log_debug("Load fmu from path: %s", m->path);
    dlerror();
    handle = dlopen(m->path, RTLD_NOW | RTLD_LOCAL);
    dlerror_str = dlerror();
    if (dlerror_str) {
        log_error(dlerror_str);
        return -1;
    }

    size_t len = ARRAY_SIZE(fmi2_func_names);
    if (len != sizeof(Fmi2VTable) / sizeof(void*)) return EINVAL;

    void** vt = (void**)&a->vtable;
    for (size_t i = 0; i < len; i++) {
        rc |= _get_func(handle, fmi2_func_names[i], &vt[i]);
    }
    if (rc != 0) {
        log_error("Not all fmi2 functions loaded!");
    }

    a->callbacks.allocateMemory = calloc;
    a->callbacks.freeMemory = free;
    a->callbacks.logger = fmu2_logger_callback;
    a->callbacks.stepFinished = fmu2_step_finished_callback;

    return 0;
}


static int32_t fmi2mcl_init(FmuModel* m)
{
    Fmi2Adapter* adapter = m->adapter;
    int          rc = 0;

    errno = 0;
    adapter->fmi2_inst = adapter->vtable.instantiate(m->name, m->cosim, m->guid,
        m->resource_dir, &(adapter->callbacks), fmi2False, fmi2True);
    if (errno) {
        log_debug("FMU set errno (%d): %s", errno, strerror(errno));
        errno = 0;
    }
    if (adapter->fmi2_inst == NULL) {
        log_error("FMI2 Instance could not be created.");
        return EINVAL;
    }

    errno = 0;
    rc = adapter->vtable.enter_initialization(adapter->fmi2_inst);
    if (errno) {
        log_debug("FMU set errno (%d): %s", errno, strerror(errno));
        errno = 0;
    }
    if (rc > 0) {
        log_error("FMI2 enter initialization did not return OK (%d).", rc);
        return rc;
    }

    errno = 0;
    rc = adapter->vtable.exit_initialization(adapter->fmi2_inst);
    if (errno) {
        log_debug("FMU set errno (%d): %s", errno, strerror(errno));
        errno = 0;
    }
    if (rc > 0) {
        log_error("FMI2 exit initialization did not return OK (%d).", rc);
        return rc;
    }

    return 0;
}


static int32_t fmi2mcl_step(FmuModel* m, double* model_time, double end_time)
{
    log_trace("Step: model_time: %f, end_time: %f", *model_time, end_time);

    Fmi2Adapter* a = m->adapter;
    int          rc = 0;

    errno = 0;
    rc = a->vtable.do_step(
        a->fmi2_inst, *model_time, (end_time - *model_time), fmi2True);
    if (errno) {
        log_debug("FMU set errno (%d): %s", errno, strerror(errno));
        errno = 0;
    }
    if (rc > 0) {
        return EBADMSG;
    };
    *model_time = end_time;
    return 0;
}


static int32_t fmi2mcl_marshal_in(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;
    int          rc = 0;

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

        log_trace(
            "  (name: %s, count: %d, type: %d)", mg->name, mg->count, mg->type);

        switch (mg->type) {
        case MARSHAL_TYPE_DOUBLE: {
            errno = 0;
            rc = a->vtable.get_real(
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._double);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  get_real[%d]: vr[%d]=%f", i, mg->target.ref[i],
                    mg->target._double[i]);
            }
            break;
        }
        case MARSHAL_TYPE_INT32: {
            errno = 0;
            rc = a->vtable.get_integer(
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._int32);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  get_integer[%d]: vr[%d]=%d", i, mg->target.ref[i],
                    mg->target._int32[i]);
            }
            break;
        }
        case MARSHAL_TYPE_BOOL: {
            errno = 0;
            rc = a->vtable.get_boolean(
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._int32);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  get_boolean[%d]: vr[%d]=%d", i, mg->target.ref[i],
                    mg->target._int32[i]);
            }

            break;
        }
        case MARSHAL_TYPE_STRING: {
            errno = 0;
            rc = a->vtable.get_string(
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._string);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  get_string[%d]: vr[%d]=%s", i, mg->target.ref[i],
                    mg->target._string[i]);
            }

            break;
        }
        default:
            break;
        }
    }

    marshal_group_in(m->data.mg_table);

    return 0;
}


int32_t fmi2mcl_marshal_out(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;
    int          rc = 0;

    marshal_group_out(m->data.mg_table);

    log_trace("Marshal OUT (target -> FMU):");
    for (MarshalGroup* mg = m->data.mg_table; mg && mg->name; mg++) {
        switch (mg->dir) {
        case MARSHAL_DIRECTION_TXRX:
        case MARSHAL_DIRECTION_TXONLY:
        case MARSHAL_DIRECTION_PARAMETER:
            break;
        default:
            continue;
        }

        log_trace(
            "  (name: %s, count: %d, type: %d)", mg->name, mg->count, mg->type);

        switch (mg->type) {
        case MARSHAL_TYPE_DOUBLE: {
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  set_real[%d]: vr[%d]=%f", i, mg->target.ref[i],
                    mg->target._double[i]);
            }
            errno = 0;
            rc = a->vtable.set_real(
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._double);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_INT32: {
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  set_integer[%d]: vr[%d]=%d", i, mg->target.ref[i],
                    mg->target._int32[i]);
            }
            errno = 0;
            rc = a->vtable.set_integer(
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._int32);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_BOOL: {
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  set_boolean[%d]: vr[%d]=%d", i, mg->target.ref[i],
                    mg->target._int32[i]);
            }
            errno = 0;
            rc = a->vtable.set_boolean(
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._int32);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_STRING: {
            for (uint32_t i = 0; i < mg->count; i++) {
                log_trace("  set_string[%d]: vr[%d]=%s", i, mg->target.ref[i],
                    mg->target._string[i]);
            }
            errno = 0;
            rc = a->vtable.set_string(
                // a->fmi2_inst, mg->target.ref, mg->count, "foobar");
                a->fmi2_inst, mg->target.ref, mg->count, mg->target._string);
            if (errno) {
                log_debug("FMU set errno (%d): %s", errno, strerror(errno));
                errno = 0;
            }
            if (rc > 0) {
                return EBADMSG;
            };
            break;
        }
        default:
            break;
        }
    }

    return 0;
}


static int32_t fmi2mcl_unload(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;

    a->vtable.free_instance(a->fmi2_inst);

    if (m->adapter) free(m->adapter);

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
