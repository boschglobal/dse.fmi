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

Fault conditions can be communicated to the caller by setting variable
`errno` to a non-zero value.

> Implemented by FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

Returns
-------
NULL
: The FMU was configured.

(FmuInstanceData*)
: Pointer to a new, or mutilated, version of the Fmu Descriptor object. The
  original Fmu Descriptor object will be released by the higher layer (i.e.
  don't call `free()`).

errno <> 0 (indirect)
: Indicates an error condition.
*/
FmuInstanceData* fmu_create(FmuInstanceData* fmu)
{
    return fmu;
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
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

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


/**
fmu_log
=======

Write a log message to the logger defined by the FMU.

Parameters
----------
fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

status (const int)
: The status of the message to be logged.

category (const char*)
: The category the message belongs to.

message (const char*)
: The message to be logged by the FMU.
*/
extern void fmu_log(FmuInstanceData* fmu, const int status,
    const char* category, const char* message, ...);
