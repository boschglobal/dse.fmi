// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_IMPORTER_IMPORTER_H_
#define DSE_IMPORTER_IMPORTER_H_

#include <dse/ncodec/codec.h>

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


typedef struct BinaryData {
    char* start;
    char* mimetype;
    char* type;
} BinaryData;


typedef struct modelDescription {
    char* name;
    char* version;
    char* guid;

    /* Calculated properties. */
    char* fmu_lib_path;

    /* Storage. */
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
        BinaryData**  rx_binary_info;
        BinaryData**  tx_binary_info;
    } binary;
} modelDescription;


/* xml.c */
DLL_PRIVATE modelDescription* parse_model_desc(
    const char* docname, const char* platform);

/* importer NCodec Interface */
DLL_PUBLIC void importer_codec_write(uint32_t frame_id, uint8_t frame_type,
    const uint8_t* message_buffer, size_t message_len, uint8_t** out_buffer,
    size_t* out_length, const char* mime_type);
DLL_PUBLIC void importer_ncodec_read(
    const char* mime_type, uint8_t* data, size_t len);
DLL_PUBLIC void importer_ncodec_stat(const char* mime_type, char** value);

#endif  // DSE_IMPORTER_IMPORTER_H_
