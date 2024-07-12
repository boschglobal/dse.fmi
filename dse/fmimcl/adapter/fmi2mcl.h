// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_FMIMCL_ADAPTER_FMI2MCL_H_
#define DSE_FMIMCL_ADAPTER_FMI2MCL_H_

#include <dse/fmimcl/fmimcl.h>
#include <dse/clib/fmi/fmi2/headers/fmi2FunctionTypes.h>

typedef void* (*fmi2Instantiate)();
typedef int32_t (*fmi2SetupExperiment)();
typedef int32_t (*fmi2EnterInitializationMode)();
typedef int32_t (*fmi2ExitInitializationMode)();
typedef int32_t (*fmi2GetReal)();
typedef int32_t (*fmi2GetInteger)();
typedef int32_t (*fmi2GetBoolean)();
typedef int32_t (*fmi2GetString)();
typedef int32_t (*fmi2SetReal)();
typedef int32_t (*fmi2SetInteger)();
typedef int32_t (*fmi2SetBoolean)();
typedef int32_t (*fmi2SetString)();
typedef int32_t (*fmi2DoStep)();
typedef int32_t (*fmi2Terminate)();
typedef void (*fmi2FreeInstance)();

typedef struct Fmi2VTable {
    fmi2Instantiate             instantiate;
    fmi2SetupExperiment         setup_experiment;
    fmi2EnterInitializationMode enter_initialization;
    fmi2ExitInitializationMode  exit_initialization;
    fmi2GetReal                 get_real;
    fmi2GetInteger              get_integer;
    fmi2GetBoolean              get_boolean;
    fmi2GetString               get_string;
    fmi2SetReal                 set_real;
    fmi2SetInteger              set_integer;
    fmi2SetBoolean              set_boolean;
    fmi2SetString               set_string;
    fmi2DoStep                  do_step;
    fmi2Terminate               terminate;
    fmi2FreeInstance            free_instance;
} Fmi2VTable;

typedef struct Fmi2Adapter {
    void*                 fmi2_inst;
    Fmi2VTable            vtable;
    fmi2CallbackFunctions callbacks;
} Fmi2Adapter;


/* fmi2mcl.c*/
DLL_PRIVATE void fmi2mcl_create(FmuModel* m);

#endif  // DSE_FMIMCL_ADAPTER_FMI2MCL_H_
