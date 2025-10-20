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

#define UNUSED(x) ((void)x)


/**
FMU API
=======

The FMU API provides a simplified FMU inteface with an abstracted variable
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


FMUs implemented using this simplified FMU API can be built for both FMI 2
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

typedef enum {
    FmiLogOk,
    FmiLogWarning,
    FmiLogDiscard,
    FmiLogError,
    FmiLogFatal,
    FmiLogPending
} FmiLogStatus;


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

    /* Scalar Signals. */
    double* scalar;

    /* Binary Signals. */
    void**    binary;
    uint32_t* length;
    uint32_t* buffer_size;

    /* Network Codec Objects (related to binary signals).*/
    char** mime_type;
    void** ncodec;
} FmuSignalVector;


typedef struct FmuSignalVectorIndex {
    FmuSignalVector* sv;
    uint32_t         vi;
} FmuSignalVectorIndex;


/* FMU NCodec Interface. */
#define FMU_NCODEC_OPEN_FUNC_NAME  "fmu_ncodec_open"
#define FMU_NCODEC_CLOSE_FUNC_NAME "fmu_ncodec_close"
typedef void* (*FmuNcodecOpenFunc)(
    FmuInstanceData* fmu, const char* mime_type, FmuSignalVectorIndex* idx);
typedef void (*FmuNcodecCloseFunc)(FmuInstanceData* fmu, void* ncodec);


typedef struct FmuVarTableMarshalItem {
    double* variable;  // Pointer to FMU allocated storage.
    double* signal;    // Pointer to FmuSignalVector storage (i.e. scalar).
} FmuVarTableMarshalItem;


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
        void* environment;
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
            HashMap input;
            HashMap output;
        } string;  // NOLINT(build/include_what_you_use)
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

    /* FMU Variable Table, used for indirect variable access. */
    struct {
        void*    table;
        HashList var_list;

        /* NLT for var/signal mirroring. */
        FmuVarTableMarshalItem* marshal_list;
    } var_table;
} FmuInstanceData;


/* ascii85.c */
DLL_PRIVATE char* dse_ascii85_encode(const char* source, size_t len);
DLL_PRIVATE char* dse_ascii85_decode(const char* source, size_t* len);

/* signal.c (default implementations for generic FMU) */
DLL_PUBLIC void    fmu_load_signal_handlers(FmuInstanceData* fmu);
DLL_PRIVATE double fmu_register_var(
    FmuInstanceData* fmu, uint32_t vref, bool input, size_t offset);
DLL_PRIVATE void* fmu_lookup_ncodec(
    FmuInstanceData* fmu, uint32_t vref, bool input);
DLL_PRIVATE void  fmu_register_var_table(FmuInstanceData* fmu, void* table);
DLL_PRIVATE void* fmu_var_table(FmuInstanceData* fmu);

/* FMU Interface (example implementation in fmu.c)  */
DLL_PRIVATE FmuInstanceData* fmu_create(FmuInstanceData* fmu);
DLL_PRIVATE int32_t          fmu_init(FmuInstanceData* fmu);
DLL_PRIVATE int32_t          fmu_step(
             FmuInstanceData* fmu, double communication_point, double step_size);
DLL_PRIVATE int32_t fmu_destroy(FmuInstanceData* fmu);
DLL_PRIVATE void    fmu_log(FmuInstanceData* fmu, const int status,
       const char* category, const char* message, ...);

/* FMU Signal Interface (optional)  */
DLL_PUBLIC void fmu_signals_reset(FmuInstanceData* fmu);
DLL_PUBLIC void fmu_signals_setup(FmuInstanceData* fmu);
DLL_PUBLIC void fmu_signals_remove(FmuInstanceData* fmu);

/* FMU NCodec Interface (optional) */
DLL_PUBLIC void* fmu_ncodec_open(
    FmuInstanceData* fmu, const char* mime_type, FmuSignalVectorIndex* idx);
DLL_PUBLIC void fmu_ncodec_close(FmuInstanceData* fmu, void* ncodec);


#endif  // DSE_FMU_FMU_H_
