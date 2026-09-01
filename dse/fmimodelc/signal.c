// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/fmu/fmu.h>
#include <dse/fmimodelc/fmimodelc.h>


#define UNUSED(x) ((void)x)


static int _reset_binary_signal(void* value, void* data)
{
    UNUSED(data);

    SimbusVectorIndex* idx = value;
    if (idx && idx->sbv && idx->vi < idx->sbv->count) {
        idx->sbv->length[idx->vi] = 0;
    }
    return 0;
}


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

    if (fmu->variables.signals_reset) return;

    // Calling reset on the SimBus vector causes networking issues as
    // data is not exchanged between all nodes. A different method of
    // connecting to the SimBus vector is required (NetBus) to decouple
    // from the SimBus vector (which has an inverse lifecycle to the
    // Model representation of a Binary Vector).
    //
    // simbus_vector_binary_reset(m->model.sim);

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
