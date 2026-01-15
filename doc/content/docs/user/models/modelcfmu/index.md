---
title: "Model - ModelC FMU"
linkTitle: "ModelC FMU"
tags:
- FMI
- ModelC
- ModelC FMU
- Workflow
- Model
github_repo: "https://github.com/boschglobal/dse.fmi"
github_subdir: "doc"
---

## Synposis

Generate an FMU which includes the ModelC Runtime and a complete simulation.


## Workflow

### Structure

```text
examples/direct
    └── sim
    │   ├── data
    │   │    └── simulation.yml
    │   │    └── direct_index.yml           <-- Workflow generated direct index (SignalGroup).
    │   └── model
    │       └── direct
    │           └── lib/libdirect.so
    │           └── data/model.yml
    └── fmu
        └── direct/                         <-- Packaged FMU layout (can be directly operated).
        └── direct.fmu                      <-- FMU archive file.
```

## Transformations

generate-fmimodelc
    gen-modelcfmu
    gen-modelcfmu-annotations


GenFmiModelcCommand (gen-modelcfmu)
    signalgroups
        Signal Groups to export as FMU variables
        default is all


GenFmuAnnotationCommand (gen-modelcfmu-annotations)
    fmi_variable_vref
    fmi_variable_type
    fmi_variable_name


GenModelCFmuAnnotationCommand
    signalgroups **** needs to smart select all ****

    Stack
        model_runtime__model_inst ... list of mi, set automatically
        model_runtime__yaml_files ... all yaml files

    SignalGroup
        must contain metadata:name
        signal must contain direction
            fmi_variable_vref
            fmi_variable_type
            fmi_variable_name



### Annotations

### Direct Index

### Patcher

### Generate FMU

## Simulation (DSE)

{{< readfile file="script.md" code="true" lang="hs" >}}


## Workflow

### Example

```bash
task generate-modelcfmu FMU_NAME=foo
```

### Task CLI

{{< readfile file="task.md" code="true" lang="bash" >}}
