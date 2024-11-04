// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

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

    adapter->fmi2_inst = adapter->vtable.instantiate(m->name, m->cosim, m->guid,
        m->resource_dir, &(adapter->callbacks), fmi2False, fmi2False);

    if (adapter->fmi2_inst == NULL) {
        log_error("FMI2 Instance could not be created.");
        return EINVAL;
    }

    rc = adapter->vtable.enter_initialization(adapter->fmi2_inst);
    if (rc > 0) {
        log_error("FMI2 enter initialization did not return OK (%d).", rc);
        return rc;
    }

    rc = adapter->vtable.exit_initialization(adapter->fmi2_inst);
    if (rc > 0) {
        log_error("FMI2 exit initialization did not return OK (%d).", rc);
        return rc;
    }

    return 0;
}


static int32_t fmi2mcl_step(FmuModel* m, double* model_time, double end_time)
{
    Fmi2Adapter* a = m->adapter;
    if (a->vtable.do_step(a->fmi2_inst, *model_time, (end_time - *model_time),
            fmi2True) > 0) {
        return EBADMSG;
    };
    *model_time = end_time;
    return 0;
}


static int32_t fmi2mcl_marshal_in(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;
    for (MarshalGroup* mg = m->data.mg_table; mg && mg->name; mg++) {
        if (mg->dir != MARSHAL_DIRECTION_RXONLY) continue;

        switch (mg->type) {
        case MARSHAL_TYPE_DOUBLE: {
            if (a->vtable.get_real(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._double) > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_INT32: {
            if (a->vtable.get_integer(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._int32) > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_BOOL: {
            if (a->vtable.get_boolean(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._int32) > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_STRING: {
            if (a->vtable.get_string(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._string) > 0) {
                return EBADMSG;
            };
            break;
        }

        default:
            break;
        }

        marshal_group_in(mg);
    }

    return 0;
}


static int32_t fmi2mcl_marshal_out(FmuModel* m)
{
    Fmi2Adapter* a = m->adapter;
    for (MarshalGroup* mg = m->data.mg_table; mg && mg->name; mg++) {
        if (mg->dir != MARSHAL_DIRECTION_TXONLY) continue;

        marshal_group_out(mg);

        switch (mg->type) {
        case MARSHAL_TYPE_DOUBLE: {
            if (a->vtable.set_real(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._double) > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_INT32: {
            if (a->vtable.set_integer(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._int32) > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_BOOL: {
            if (a->vtable.set_boolean(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._int32) > 0) {
                return EBADMSG;
            };
            break;
        }
        case MARSHAL_TYPE_STRING: {
            if (a->vtable.set_string(a->fmi2_inst, mg->target.ref, mg->count,
                    mg->target._string) > 0) {
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
