---
title: FMI ModelC FMU API Reference
linkTitle: ModelC FMU
---
## FMI ModelC FMU


The FMI ModelC FMU is and FMU which is capable of loading and running a
DSE Simulation (e.g. a ModelC Simulation Stack). All capabilites of the ModelC
Runtime are supported, including the exchange of binary signals (e.g. CAN) and
realisation of bus topologies (e.g. multi-node CAN Networks).


### Component Diagram

<div hidden>

```
@startuml fmimodelc-component

title FMI ModelC FMU

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](fmimodelc-component.png)




## default_log


Default logging function in case the FMU caller does not provide any logger.



## fmi2ExitInitializationMode


Initialise the Model Runtime (of the ModelC library) and in the process
establish the simulation that this ModelC FMU is wrapping/operating.

This function will generate indexes to map between FMI Variables and ModelC
Signals; both scaler signals (double) and binary signals (string/binary).

### Parameters

c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.

### Returns

fmi2OK (fmi2Status)
: The simulation that this FMU represents is ready to be operated.



## fmi2GetReal


Get values for the provided list of value references.

### Parameters

c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to retrieve.

nvr (int)
: The number of value references to retrieve.

value (fmi2Real[])
: Storage for the retrieved values.

### Returns

fmi2OK (fmi2Status)
: The requested variables are retrieved (where available).



## fmi2GetString


Get values for the provided list of value references.

### Parameters

c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to retrieve.

nvr (int)
: The number of value references to retrieve.

value (fmi2String[])
: Storage for the retrieved values.

### Returns

fmi2OK (fmi2Status)
: The requested variables are retrieved (where available).



## fmi2SetReal


Set values for the provided list of value references and values.

### Parameters

c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to set.

nvr (int)
: The number of value references to set.

value (fmi2Real[])
: Storage for the values to be set.

### Returns

fmi2OK (fmi2Status)
: The requested variables have been set (where available).



## fmi2SetString


Set values for the provided list of value references and values. String/Binary
variables are always appended to the ModelC Binary Signal.

> Note: If several variables are indexed against the same ModelC Binary Signal,
  for instance in a Bus Topology, then each variable will be appended to that
  ModelC Binary Signal.

### Parameters

c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.

vr (fmi2ValueReference[])
: List of value references to set.

nvr (int)
: The number of value references to set.

value (fmi2String[])
: Storage for the values to be set.

### Returns

fmi2OK (fmi2Status)
: The requested variables have been set (where available).



## fmu_signals_reset


> Required by FMU.

### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.



## fmi2Instantiate


Create an instance of this FMU, allocate/initialise a FmuInstanceData
object which should be used for subsequent calls to FMI methods (as parameter
`fmi2Component c`).

> Note: This implementation __does not__ use memory related callbacks provided
  by the Importer (e.g. `malloc()` or `free()`).

### Returns

fmi2Component (pointer)
: An FmuInstanceData object which represents this FMU instance.



## fmi2DoStep


Set values for the provided list of value references and values. String/Binary
variables are always appended to the ModelC Binary Signal.

> Note: If several variables are indexed against the same ModelC Binary Signal,
  for instance in a Bus Topology, then each variable will be appended to that
  ModelC Binary Signal.

### Parameters

c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.

currentCommunicationPoint (fmi2Real)
: The model time (for the start of this step).

communicationStepSize (fmi2Real)
: The model step size.

noSetFMUStatePriorToCurrentPoint (fmi2Boolean)
: Not used.

### Returns

fmi2OK (fmi2Status)
: The step completed.

fmi2Error (fmi2Status)
: An error occurred when stepping the ModelC Simulation.



## fmi2FreeInstance


Free memory and resources related to the provided FMU instance.

### Parameters

c (fmi2Component*)
: An FmuInstanceData object representing an instance of this FMU.



## fmu_signals_setup


Placeholder to signal the FMU to not use the default signal allocation.

### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.



## fmu_signals_remove


This method frees the allocated binary signal indexes.

### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.



## fmu_load_signal_handlers


This method assigns the signal handler function to a vtable.

### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.



## Typedefs

## Functions

