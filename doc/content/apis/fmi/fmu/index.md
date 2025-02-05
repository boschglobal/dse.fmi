---
title: FMU API Reference
linkTitle: FMU
---
## FMU API


The FMU API provides a simplified FMU inteface with an abstracted varaible
interface (indexing and storage). The FMU Interface includes the methods:
* Implemented by FMU developer:
    * `[fmu_create()]({{< ref "#fmu_create" >}})`
    * `[fmu_init()]({{< ref "#fmu_init" >}})`
    * `[fmu_step()]({{< ref "#fmu_step" >}})`
    * `[fmu_destroy()]({{< ref "#fmu_destroy" >}})`
* Additional provided functions:
    * `[fmu_log()]({{< ref "#fmu_log" >}})` - logging function
* Supporting Variable Table mechanism:
    * `[fmu_register_var()]({{< ref "#fmu_register_var" >}})`
    * `[fmu_register_var_table()]({{< ref "#fmu_register_var_table" >}})`
    * `[fmu_var_table()]({{< ref "#fmu_var_table" >}})`


An additional FMU Signal Interface is available for more complex integrations:
* `[fmu_signals_reset()]({{< ref "#fmu_signals_reset" >}})`
* `[fmu_signals_setup()]({{< ref "#fmu_signals_setup" >}})`
* `[fmu_signals_remove()]({{< ref "#fmu_signals_remove" >}})`


FMUs imlemented using this simplified FMU API can be built for both FMI 2
and FMI 3 standards by linking to the relevant implementations:
* `fmi2fmu.c` for and FMI 2 FMU
* `fmi3fmu.c` for and FMI 3 FMU


Binary variables are supported for FMI 3 and FMI 2 standards.
In FMUs built to the FMI 2 standard, binary variables are implemented via
FMI String Variables and an associated encoding.
See [Dynamic Simulation Environment - FMI Layered Standard Binary Codec
Selection](https://github.com/boschglobal/dse.standards/tree/main/modelica/fmi-ls-binary-codec)
for details.


### Component Diagram

<div hidden>

```
@startuml fmu-component

skinparam nodesep 55
skinparam ranksep 40
skinparam roundcorner 10
skinparam componentTextAlignment center

title FMU Component Diagram

component "Importer" as importer
interface "FMI I/F" as fmi
package "FMU Library" {
        component "FMI FMU\n(fmiXfmu.c)" as fmiXfmu
        component "**FMU**\n(fmu.c)" as fmu
        interface "FmuVTable" as fmuVt
        component "Index\n(signal.c)" as index
        interface "FmuSignalVTable" as signalVt
        component "Encoder\n(ascii85.c)" as encoder
        component "Network\n(ncodec.c)" as network
        component "Stream\n(stream.c)" as stream
        interface "NCodecVTable" as ncodec
        note as N_fmu
  Contains FMU functional
  implementation/models.
end note

fmu .up. N_fmu
}

importer -down-( fmi
fmi --down- fmiXfmu
fmiXfmu -left-( fmuVt
fmuVt -left- fmu
fmiXfmu -down-( signalVt
signalVt -down- index
stream .up.> index
encoder .up.> index
network .right.> stream
fmu -down-( ncodec
ncodec --- network



center footer Dynamic Simulation Environment

@enduml
```

</div>

![fmu-component](fmu-component.png)


### Example


The following example demonstrates an FMU which implements an incrementing
counter.

{{< readfile file="../examples/fmu/fmu.c" code="true" lang="c" >}}




## Typedefs

### FmuInstanceData

```c
typedef struct FmuInstanceData {
    struct {
        char* name;
        int type;
        int version;
        char* resource_location;
        char* guid;
        bool log_enabled;
        void* logger;
        char* save_resource_location;
    } instance;
    struct {
        struct {
            int input;
            int output;
        } scalar;
        struct {
            int input;
            int output;
        } string;
        struct {
            int rx;
            int tx;
            int encode_func;
            int decode_func;
            int free_list;
        } binary;
        FmuSignalVTable vtable;
        bool signals_reset;
    } variables;
    void* data;
    struct {
        void* table;
        int var_list;
        FmuVarTableMarshalItem* marshal_list;
    } var_table;
}
```

### FmuSignalVTable

```c
typedef struct FmuSignalVTable {
    FmuSignalsResetFunc reset;
    FmuSignalsSetupFunc setup;
    FmuSignalsRemoveFunc remove;
}
```

### FmuSignalVector

```c
typedef struct FmuSignalVector {
    int index;
    uint32_t count;
    char** signal;
    uint32_t* uid;
    double* scalar;
    void** binary;
    uint32_t* length;
    uint32_t* buffer_size;
}
```

### FmuSignalVectorIndex

```c
typedef struct FmuSignalVectorIndex {
    FmuSignalVector* sv;
    uint32_t vi;
}
```

### FmuVTable

```c
typedef struct FmuVTable {
    FmuCreateFunc create;
    FmuInitFunc init;
    FmuStepFunc step;
    FmuDestroyFunc destroy;
}
```

### FmuVarTableMarshalItem

```c
typedef struct FmuVarTableMarshalItem {
    double* variable;
    double* signal;
}
```

## Functions

### fmu_create

This method creates a FMU specific instance which will be used to operate the
FMU. It is called in the `Instantiate()` method of the FMI standard.

> Implemented by FMU.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

#### Returns

0 (int32_t)
: The FMU was created correctly.



### fmu_destroy

Releases memory and system resources allocated by FMU.
It is called in the `FreeInstance()` Method of the FMU.

> Implemented by FMU.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

#### Returns

0 (int32_t)
: The FMU data was released correctly.



### fmu_init

This method initializes all FMU relevant data that is represented by the FMU.
It is called in the `ExitInitializationMode()` Method of the FMU.

> Implemented by FMU.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

#### Returns

0 (int32_t)
: The FMU was created correctly.



### fmu_log

Write a log message to the logger defined by the FMU.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

status (const int)
: The status of the message to be logged.

category (const char*)
: The category the message belongs to.

message (const char*)
: The message to be logged by the FMU.



### fmu_register_var

Register a variable with the FMU Variable Table mechanism.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
vref (uint32_t)
: Variable reference of the variable being registered.
input (bool)
: Set `true` for input, and `false` for output variable causality.
offset (size_t)
: Offse of the variable (type double) in the FMU provided variable table.

#### Returns

start_value (double)
: The configured FMU Variable start value, or 0.



### fmu_register_var_table

Register the Variable Table. The previouly registered variables, via calls to
`fmu_register_var`, are configured and the FMU Variable Table mechanism
is enabled.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
table (void*)
: Pointer to the Variable Table being registered.



### fmu_signals_remove

This method will reomve any buffers used to provide storage for FMU variables.
If those buffers were allocated (e.g by an implementation
of `fmu_signals_setup()`) then those buffers should be freed in this method.

> Integrators may provide their own implementation of this method.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.



### fmu_signals_reset

This method will reset any binary variables which where used by an FMU in the
previous step. Typically this will mean that indexes into the buffers of
binary variables are set to 0, hovever the buffers themselves are
not released (i.e. free() is not called).

> Integrators may provide their own implementation of this method.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.




### fmu_signals_setup

This method will setup the buffers which provide storage for FMU variables.
Depending on the implementation buffers may be mapped to existing buffers
in the implementation, or allocated specifically. When allocating buffers the
method `fmu_signals_setup()` should also be implemented to release those buffers
when the `FmuInstanceData()` is freed.

> Integrators may provide their own implementation of this method.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.



### fmu_step

This method initializes all FMU relevant data that is represented by the FMU.
It is called in the `DoStep()` Method of the FMU.

> Implemented by FMU.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.
communication_point (double)
: The current model time of the FMU in seconds.
step_size (double)
: The step size of the FMU in seconds.

#### Returns

0 (int32_t)
: The FMU step was performed correctly.



### fmu_var_table

Return a reference to the previously registered Variable Table.

#### Parameters

fmu (FmuInstanceData*)
: The FMU Descriptor object representing an instance of the FMU Model.

#### Returns

table (void*)
: Pointer to the Variable Table.



