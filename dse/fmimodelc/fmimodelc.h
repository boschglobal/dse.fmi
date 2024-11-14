// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_FMIMODELC_FMIMODELC_H_
#define DSE_FMIMODELC_FMIMODELC_H_

#include <dse/platform.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/modelc/adapter/simbus/simbus.h>
#include <dse/modelc/runtime.h>


#ifndef DLL_PUBLIC
#define DLL_PUBLIC __attribute__((visibility("default")))
#endif
#ifndef DLL_PRIVATE
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif


/**
FMI ModelC FMU
==============

The FMI ModelC FMU is and FMU which is capable of loading and running a
DSE Simulation (e.g. a ModelC Simulation Stack). All capabilites of the ModelC
Runtime are supported, including the exchange of binary signals (e.g. CAN) and
realisation of bus topologies (e.g. multi-node CAN Networks).


Component Diagram
-----------------
<div hidden>

```
@startuml fmimodelc-component

title FMI ModelC FMU

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](fmimodelc-component.png)

*/


/* Encode/Decode Interface. */
typedef char* (*EncodeFunc)(const char* source, size_t len);
typedef char* (*DecodeFunc)(const char* source, size_t* len);


/* runtime.c */
DLL_PRIVATE void fmimodelc_index_scalar_signals(
    RuntimeModelDesc* m, HashMap* input, HashMap* output);
DLL_PRIVATE void fmimodelc_index_binary_signals(
    RuntimeModelDesc* m, HashMap* rx, HashMap* tx);
DLL_PRIVATE void fmimodelc_index_text_encoding(
    RuntimeModelDesc* m, HashMap* encode_func, HashMap* decode_func);
DLL_PRIVATE void fmimodelc_reset_binary_signals(RuntimeModelDesc* m);


/* ascii85.c */
DLL_PRIVATE char* ascii85_encode(const char* source, size_t len);
DLL_PRIVATE char* ascii85_decode(const char* source, size_t* len);


#endif  // DSE_FMIMODELC_FMIMODELC_H_
