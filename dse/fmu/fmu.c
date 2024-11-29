// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/fmu/fmu.h>


#define UNUSED(x) ((void)x)


/**
fmu_create
==========

This method creates a FMU specific instance which will be used to operate the
FMU. It is called in the `Instantiate()` method of the FMI standard.

> Implemented by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

Returns
-------
0 (int32_t)
: The FMU was created correctly.
*/
int32_t fmu_create(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}


/**
fmu_init
========

This method initializes all FMU relevant data that is represented by the FMU.
It is called in the `ExitInitializationMode()` Method of the FMU.

> Implemented by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

Returns
-------
0 (int32_t)
: The FMU was created correctly.
*/
int32_t fmu_init(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}


/**
fmu_step
========

This method initializes all FMU relevant data that is represented by the FMU.
It is called in the `DoStep()` Method of the FMU.

> Implemented by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
communication_point (double)
: The current model time of the FMU in seconds.
step_size (double)
: The step size of the FMU in seconds.

Returns
-------
0 (int32_t)
: The FMU step was performed correctly.
*/
int32_t fmu_step(
    FmuInstanceData* fmu, double communication_point, double step_size)
{
    UNUSED(fmu);
    UNUSED(communication_point);
    UNUSED(step_size);
    return 0;
}


/**
fmu_destroy
===========

Releases memory and system resources allocated by FMU.
It is called in the `FreeInstance()` Method of the FMU.

> Implemented by FMU.

Parameters
----------
model (ModelDesc*)
: Model descriptor object.

Returns
-------
0 (int32_t)
: The FMU data was released correctly.
*/
int32_t fmu_destroy(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}
