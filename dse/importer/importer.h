// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_IMPORTER_IMPORTER_H_
#define DSE_IMPORTER_IMPORTER_H_


#ifndef DLL_PUBLIC
#define DLL_PUBLIC __attribute__((visibility("default")))
#endif
#ifndef DLL_PRIVATE
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif


/**
Importer
========

Component Diagram
-----------------
<div hidden>

```
@startuml importer-component

title FMI Importer

center footer Dynamic Simulation Environment

@enduml
```

</div>

*/


typedef struct modelDescription {
    char* modelName;
    char* fmiVersion;
    char* guid;
    struct {
        unsigned int* vr_rx_real;
        unsigned int* vr_tx_real;
        double*       val_rx_real;
        double*       val_tx_real;
        size_t        rx_count;
        size_t        tx_count;
    } real;
    struct {
        unsigned int* vr_rx_binary;
        unsigned int* vr_tx_binary;
        char**        val_rx_binary;
        char**        val_tx_binary;
        size_t*       val_size_rx_binary;
        size_t*       val_size_tx_binary;
        size_t        rx_count;
        size_t        tx_count;
    } binary;
} modelDescription;


/* xml.c */
DLL_PRIVATE modelDescription* parse_model_desc(char* docname, uint8_t version);


#endif  // DSE_IMPORTER_IMPORTER_H_
