// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_FMIGATEWAY_FMIGATEWAY_H_
#define DSE_FMIGATEWAY_FMIGATEWAY_H_

#include <dse/clib/util/yaml.h>
#include <dse/modelc/gateway.h>
#include <dse/fmu/fmu.h>


/**
FMI ModelC Gateway
==================


Component Diagram
-----------------
<div hidden>

```
@startuml fmigateway-component

title FMI Gateway FMU

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](fmigateway-component.png)


Example
-------
*/

typedef struct WindowsModel {
    /* process Information */
    const char* exe;
    /* Model information. */
    const char* name;
    double      step_size;
    double      end_time;
    int         log_level;
    char*       yaml;
    double      current_step;
    double      timeout;
    /* Windows Information. */
    void*       w_process;
} WindowsModel;

typedef struct FmiGatewaySession {
    const char*   model_stack;
    YamlNode*     model_stack_file;
    WindowsModel* w_models;
    WindowsModel* simbus;
    WindowsModel* transport;
    struct {
        bool models;
        bool simbus;
        bool transport;
    } visibility;
    /* cmds. */
    const char* init_cmd;
    const char* shutdown_cmd;
    HashMap     envar;
    /* Additional information. */
    double      last_step;
} FmiGatewaySession;

typedef struct FmiGateway {
    ModelGatewayDesc* model;
    struct {
        YamlDocList*       doc_list;
        const char**       yaml_files;
        double             step_size;
        double             end_time;
        int                log_level;
        const char*        log_location;
        FmiGatewaySession* session;
    } settings;
    bool binary_signals_reset;
} FmiGateway;

/* index.c */
DLL_PRIVATE void fmigateway_index_scalar_signals(
    FmuInstanceData* fmu, ModelGatewayDesc* m, HashMap* input, HashMap* output);
DLL_PRIVATE void fmigateway_index_binary_signals(
    FmuInstanceData* fmu, ModelGatewayDesc* m, HashMap* rx, HashMap* tx);
DLL_PRIVATE void fmigateway_index_text_encoding(FmuInstanceData* fmu,
    ModelGatewayDesc* m, HashMap* encode_func, HashMap* decode_func);

/* parser.c */
DLL_PRIVATE void fmigateway_parse(FmuInstanceData* fmu);

/* session.c */
DLL_PRIVATE void fmigateway_session_configure(FmuInstanceData* fmu);
DLL_PRIVATE void fmigateway_session_end(FmuInstanceData* fmu);
DLL_PRIVATE int  fmigateway_setenv(const char* name, const char* value);

/* session_win32.c */
DLL_PRIVATE void fmigateway_session_windows_start(FmuInstanceData* fmu);
DLL_PRIVATE void fmigateway_session_windows_end(FmuInstanceData* fmu);


#endif  // DSE_FMIGATEWAY_FMIGATEWAY_H_
