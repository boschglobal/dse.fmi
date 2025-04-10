// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/model.h>
#include <dse/fmu/fmu.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x) ((void)x)


/**
fmu_signals_reset
=================

Resets the binary signals of the gateway to a length of 0, if the signals have
not been reseted yet.

> Required by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
*/
void fmu_signals_reset(FmuInstanceData* fmu)
{
    assert(fmu);
    FmiGateway* fmi_gw = fmu->data;
    assert(fmi_gw);

    if (fmu->variables.signals_reset) return;


    for (SignalVector* sv = fmi_gw->model->sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;
        for (size_t i = 0; i < sv->count; i++) {
            signal_reset(sv, i);
        }
    }

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


static inline int _free_fmu_idx(void* map_item, void* additional_data)
{
    UNUSED(additional_data);
    FmuSignalVectorIndex* fmu_idx = map_item;
    if (fmu_idx) {
        free(fmu_idx->sv);
    }
    return 0;
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
    assert(fmu);
    fmu_log(fmu, 0, "Debug", "Removing additional signal data...");
    hashmap_iterator(&fmu->variables.binary.rx, _free_fmu_idx, false, NULL);
    hashmap_iterator(&fmu->variables.binary.tx, _free_fmu_idx, false, NULL);
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
