// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/fmu/fmu.h>
#include <dse/fmimodelc/fmimodelc.h>


#define UNUSED(x) ((void)x)


/**
fmu_signals_reset
=================

> Required by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmu_signals_reset(FmuInstanceData* fmu)
{
    assert(fmu);
    RuntimeModelDesc* m = fmu->data;
    assert(m);

    if (fmu->variables.signals_reset) return;

    simbus_vector_binary_reset(m->model.sim);

    fmu->variables.signals_reset = true;
}


/**
fmu_signals_setup
=================

Placeholder to signal the FMU to not use the default signal allocation.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmu_signals_setup(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


/**
fmu_signals_remove
==================

This method frees the allocated binary signal indexes.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmu_signals_remove(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}


/**
fmu_load_signal_handlers
========================

This method assigns the signal handler function to a vtable.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmu_load_signal_handlers(FmuInstanceData* fmu)
{
    fmu->variables.vtable.reset = fmu_signals_reset;
    fmu->variables.vtable.setup = fmu_signals_setup;
    fmu->variables.vtable.remove = fmu_signals_remove;
}
