// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_FMU_FMU_H_
#define DSE_FMU_FMU_H_

#include <stdbool.h>
#include <stdint.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/collections/hashlist.h>


#ifndef DLL_PUBLIC
#define DLL_PUBLIC __attribute__((visibility("default")))
#endif
#ifndef DLL_PRIVATE
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif


/**
FMU API
=======

The FMU API provides a simplified FMU inteface with an abstracted varaible
interface (indexing and storage). The FMU Interface includes the methods:
* `[fmu_create()]({{< ref "#fmu_create" >}})`
* `[fmu_init()]({{< ref "#fmu_init" >}})`
* `[fmu_step()]({{< ref "#fmu_step" >}})`
* `[fmu_destroy()]({{< ref "#fmu_destroy" >}})`

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


Component Diagram
-----------------
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


Example
-------

The following example demonstrates an FMU which implements an incrementing
counter.

{{< readfile file="../examples/fmu/fmu.c" code="true" lang="c" >}}

*/

typedef struct FmuInstanceData FmuInstanceData;

/* Encode/Decode Interface. */
typedef char* (*EncodeFunc)(const char* source, size_t len);
typedef char* (*DecodeFunc)(const char* source, size_t* len);

/* FMU Interface. */
typedef int32_t (*FmuCreateFunc)(FmuInstanceData* fmu);
typedef int32_t (*FmuInitFunc)(FmuInstanceData* fmu);
typedef int32_t (*FmuStepFunc)(
    FmuInstanceData* fmu, double communication_point, double step_size);
typedef int32_t (*FmuDestroyFunc)(FmuInstanceData* fmu);

typedef struct FmuVTable {
    FmuCreateFunc  create;
    FmuInitFunc    init;
    FmuStepFunc    step;
    FmuDestroyFunc destroy;
} FmuVTable;


/* FMU Signal Interface. */
#define FMU_SIGNALS_RESET_FUNC_NAME  "fmu_signals_reset"
#define FMU_SIGNALS_SETUP_FUNC_NAME  "fmu_signals_setup"
#define FMU_SIGNALS_REMOVE_FUNC_NAME "fmu_signals_remove"
typedef void (*FmuSignalsResetFunc)(FmuInstanceData* fmu);
typedef void (*FmuSignalsSetupFunc)(FmuInstanceData* fmu);
typedef void (*FmuSignalsRemoveFunc)(FmuInstanceData* fmu);

typedef struct FmuSignalVTable {
    FmuSignalsResetFunc  reset;
    FmuSignalsSetupFunc  setup;
    FmuSignalsRemoveFunc remove;
} FmuSignalVTable;


typedef struct FmuSignalVector {
    HashMap   index;  // map{signal:uint32_t} -> index to vectors
    uint32_t  count;
    char**    signal;
    uint32_t* uid;
    double*   scalar;
    void**    binary;
    uint32_t* length;
    uint32_t* buffer_size;
} FmuSignalVector;


typedef struct FmuSignalVectorIndex {
    FmuSignalVector* sv;
    uint32_t         vi;
} FmuSignalVectorIndex;


typedef struct FmuInstanceData {
    /* FMI Instance Data. */
    struct {
        char* name;
        int   type;
        int   version;
        char* resource_location;
        char* guid;
        bool  log_enabled;
        void* logger;

        /* Storage for memory to be explicitly released. */
        char* save_resource_location;
    } instance;
    /* FMU, Signal Variables. */
    struct {
        /* Variable indexes. */
        struct {
            HashMap input;
            HashMap output;
        } scalar;
        struct {
            HashMap  rx;
            HashMap  tx;
            HashMap  encode_func;
            HashMap  decode_func;
            /* Lazy free list for allocated strings. */
            HashList free_list;
        } binary;
        /* Variable storage, via Signal Vectors. */
        FmuSignalVTable vtable;
        /* Indicate if (binary) signals have been reset. */
        bool            signals_reset;
    } variables;

    /* FMU Instance Data (additional). */
    void* data;
} FmuInstanceData;


/* ascii85.c */
DLL_PRIVATE char* ascii85_encode(const char* source, size_t len);
DLL_PRIVATE char* ascii85_decode(const char* source, size_t* len);

/* signal.c (default implementations for generic FMU) */
DLL_PRIVATE void fmu_load_signal_handlers(FmuInstanceData* fmu);

/* FMU Interface (example implementation in fmu.c)  */
DLL_PRIVATE int32_t fmu_create(FmuInstanceData* fmu);
DLL_PRIVATE int32_t fmu_init(FmuInstanceData* fmu);
DLL_PRIVATE int32_t fmu_step(
    FmuInstanceData* fmu, double communication_point, double step_size);
DLL_PRIVATE int32_t fmu_destroy(FmuInstanceData* fmu);

/* FMU Signal Interface (optional)  */
DLL_PUBLIC void fmu_signals_reset(FmuInstanceData* fmu);
DLL_PUBLIC void fmu_signals_setup(FmuInstanceData* fmu);
DLL_PUBLIC void fmu_signals_remove(FmuInstanceData* fmu);


#endif  // DSE_FMU_FMU_H_
