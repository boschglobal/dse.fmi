// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_FMIMCL_FMIMCL_H_
#define DSE_FMIMCL_FMIMCL_H_

#include <stdint.h>
#include <dse/platform.h>
#include <dse/clib/data/marshal.h>
#include <dse/clib/mdf/mdf.h>
#include <dse/modelc/mcl.h>
#include <dse/modelc/model.h>


#ifndef DLL_PUBLIC
#define DLL_PUBLIC __attribute__((visibility("default")))
#endif
#ifndef DLL_PRIVATE
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif


/**
FMI Model Compatibility Library
===============================

The FMI Model Compatibility Library provides an interfaces for loading and
operating FMUs.


Sequence Diagram
----------------
<div hidden>

```
@startuml fmimcl-sequence

title FMI MCL - Sequence

actor       User
participant ModelC
participant MCL
participant MARSHAL
participant FMIMCL
participant ENGINE
participant ADAPTER
participant COMPONENT

User -> ModelC : model_create()
activate ModelC
ModelC -> MCL : MCL_create()
activate MCL

MCL -> FMIMCL : fmimcl_parse()
activate FMIMCL
FMIMCL -> FMIMCL : parse yaml
FMIMCL -> MCL
deactivate FMIMCL

MCL -> FMIMCL : fmimcl_adapter_create()
activate FMIMCL

FMIMCL -> ADAPTER : adapter_create()
activate ADAPTER
ADAPTER -> ADAPTER : set adapter functions
ADAPTER -> FMIMCL
deactivate ADAPTER

FMIMCL -> MCL
deactivate FMIMCL

MCL-> ENGINE: fmimcl_allocate_scalar_source()
activate ENGINE
ENGINE-> MCL
deactivate ENGINE

MCL-> ENGINE: fmimcl_generate_marshal_table()
activate ENGINE
ENGINE-> MCL
deactivate ENGINE

MCL -> ModelC
deactivate MCL

ModelC -> MCL : MCL_load()
activate MCL
group Generate MarshalSignalMap list
|||
MCL -> MCL++
loop for each SignalVector
MCL -> MARSHAL : marshal_generate_signalmap()
activate MARSHAL
MARSHAL -> MCL
deactivate MARSHAL
end
return NTL
|||
end
MCL -> ADAPTER : adapter_load()
activate ADAPTER
ADAPTER -> COMPONENT : load_functions()
activate COMPONENT
COMPONENT -> ADAPTER
deactivate COMPONENT
ADAPTER -> MCL
deactivate ADAPTER
MCL -> ModelC
deactivate MCL

ModelC -> MCL : mcl_init()
activate MCL
MCL -> ADAPTER : adapter_init()
activate ADAPTER
ADAPTER -> COMPONENT : do_init()
activate COMPONENT
COMPONENT -> ADAPTER
deactivate COMPONENT
ADAPTER -> MCL
deactivate ADAPTER
MCL -> ModelC
deactivate MCL
ModelC -> User
deactivate ModelC

User -> ModelC : model_step()
activate ModelC
ModelC -> MCL : mcl_marshal_out()
activate MCL
MCL -> MARSHAL : marshal_signalmap_out()
activate MARSHAL
MARSHAL -> MCL
deactivate MARSHAL
MCL -> ADAPTER : adapter_marshal_out()
activate ADAPTER
ADAPTER -> MARSHAL : marshal_group_out()
activate MARSHAL
MARSHAL -> ADAPTER
deactivate MARSHAL
ADAPTER -> COMPONENT : set_variables()
activate COMPONENT
COMPONENT -> ADAPTER
deactivate COMPONENT
ADAPTER -> MCL
deactivate ADAPTER
MCL -> ModelC
deactivate MCL

ModelC -> MCL : mcl_step()
activate MCL
MCL -> ADAPTER : adapter_step()
activate ADAPTER
ADAPTER -> COMPONENT : do_step()
activate COMPONENT
COMPONENT -> ADAPTER
deactivate COMPONENT
ADAPTER -> MCL
deactivate ADAPTER
MCL -> ModelC
deactivate MCL

ModelC -> MCL : mcl_marshal_in()
activate MCL
MCL -> ADAPTER : adapter_marshal_in()
activate ADAPTER
ADAPTER -> COMPONENT : get_variables()
activate COMPONENT
COMPONENT -> ADAPTER
deactivate COMPONENT
ADAPTER -> MARSHAL : marshal_group_in()
activate MARSHAL
MARSHAL -> ADAPTER
deactivate MARSHAL
ADAPTER -> MCL
deactivate ADAPTER
MCL -> MARSHAL : marshal_signalmap_in()
activate MARSHAL
MARSHAL -> MCL
deactivate MARSHAL
MCL -> ModelC
deactivate MCL
ModelC -> User
deactivate ModelC

User -> ModelC : model_destroy()
activate ModelC
ModelC -> MCL : mcl_unload()
activate MCL
MCL -> ADAPTER : adapter_unload()
activate ADAPTER
ADAPTER -> MCL
deactivate ADAPTER
MCL -> ModelC
deactivate MCL

ModelC -> MCL : MCL_destroy()
activate MCL
MCL -> FMIMCL: fmimcl_destroy()
activate FMIMCL
FMIMCL-> MCL
deactivate FMIMCL
MCL -> ModelC
deactivate MCL
ModelC -> User
deactivate ModelC

center footer Dynamic Simulation Environment

@enduml
```

</div>

![](fmimcl-sequence.png)

*/


typedef struct FmuData {
    /* Signal storage (i.e. "source"). */
    size_t       count;
    const char** name;
    union {
        double* scalar;
        void**  binary;
    };
    uint32_t*     binary_len;
    MarshalKind*  kind;
    /* Marshalling tables. */
    MarshalGroup* mg_table; /* NULL terminated list. */
} FmuData;


typedef struct FmuSignal {
    const char* name;
    /* Variable. */
    uint32_t    variable_vref;
    const char* variable_name;
    MarshalKind variable_kind;
    MarshalDir  variable_dir;
    MarshalType variable_type;
    /* Annotations. */
    const char* variable_annotation_encoding;
} FmuSignal;


typedef struct FmuModel {
    MclDesc     mcl;
    /* Extensions to base MclDesc type. */
    const char* name;
    const char* version;
    bool        cosim;
    const char* guid;
    const char* resource_dir;
    const char* path;
    const char* handle;
    /* Signals (representing FMU Variables). */
    FmuSignal*  signals; /* NULL terminated list. */
    /* Internal data objects (YamlNode). */
    void*       m_doc;
    /* Adapter/Instance data. */
    void*       adapter;
    /* Data marshalling support. */
    FmuData     data;
    /* Measurement file. */
    struct {
        char*            file_name;
        void*            file;
        MdfChannelGroup* cg;
        MdfDesc          mdf;
    } measurement;
} FmuModel;


/* fmimcl.c */
DLL_PRIVATE int32_t fmimcl_adapter_create(FmuModel* fmu_model);
DLL_PRIVATE void    fmimcl_destroy(FmuModel* fmu_model);

/* parser.c */
DLL_PRIVATE void fmimcl_parse(FmuModel* fmu_model);
DLL_PRIVATE void fmimcl_load(FmuModel* fmu_model);

/* engine.c */
DLL_PRIVATE void fmimcl_allocate_source(FmuModel* m);
DLL_PRIVATE void fmimcl_generate_marshal_table(FmuModel* m);
DLL_PRIVATE void fmimcl_load_encoder_funcs(FmuModel* m);


#endif  // DSE_FMIMCL_FMIMCL_H_
