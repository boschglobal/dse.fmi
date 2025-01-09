---
title: "FMI Gateway FMU"
linkTitle: "Gateway"
weight: 200
tags:
- FMI
- Gateway
- Model
github_repo: "https://github.com/boschglobal/dse.fmi"
github_subdir: "doc"
---

## Synposis

FMI Gateway FMU


## Simulation Setup

### Structure

#### Example Simulation

```text

```

## Setup

> DOC: Provide description of Gateway Taskfile workflows.


## Operation

### Session

A session handling can be defined for the gatway using the following yaml file in the toolchain.
It is then automatically placed as part of the annoations in the model.yaml.

|Name|Type|Required|Description|
|---|---|---|---|
|session|object|true|none|
|» initialization|object|false|Script/ File that is run in the initialization phase of the FMU|
|»» path|string|true|Path to the file|
|»» file|string|true|File to be executed|
|» shutdown|object|false|Script/ File that is run in the shutdown phase of the FMU|
|»» path|string|true|Path to the file|
|»» file|string|true|File to be executed|
|» windows|object|false|List of ModelC models to be started/ stopped on windows|
|»» model|string|true|Name of the model|
|»» annotation|object|true||
|»»» stepsize|float|true|Step size of this model|
|»»» endtime|float|false|End time for this model. If not set, model runs until gateway shutdown|
|»»» loglevel|int|true|Log level between 0 and 6 (0 for most logs)|
|»»» timeout|false|true|Model timeout|
|»»» path|string|true|Path to the model executable|
|»»» file|string|true|Model file|
|»»» yaml|string|true|List of yaml files, separated by ";"|
|»»» show|boolean|true|Terminal toggle for on/ off|

```text
---
annotations:
  session:
    initialization:
      path: 'foo'
      file: 'bar'
    shutdown:
      path: 'foo_shutdown'
      file: 'bar_shutdown'
    windows:
      - model: 'Model_1'
        annotations:
          stepsize: 0.1
          endtime: 0.2
          loglevel: 6
          timeout: 60.0
          path: 'foo'
          file: 'bar'
          yaml: 'foo;bar'
          show: true
      - model: 'Model_2'
        annotations:
          stepsize: 0.15
          endtime: 0.25
          loglevel: 5
          timeout: 61.0
          path: 'foo_2'
          file: 'bar_2'
          yaml: 'foo_2;bar_2'
          show: false
```

### Toolchains

The required files for the FMI gateway can be generated using a toolchain included
in the FMI GO tools. This operation with generate the required modelDescription.xml,
model.yaml and fmu.yaml.

|Name|Required|Description|
|---|---|---|
|VERSION|false|Version of the Gateway FMU|
|FMI_VERSION|false|FMI Version of the Gateway FMU (Default 2)|
|SESSION|false|Yaml file containing the session handling|
|SIGNAL_GROUPS|true|List of yaml files to generate the required files for the Gateway FMU|
|OUT_DIR|false|Path to the generated files|
|UUID|false|UUID of the Gateway FMU|

```bash
$ task generate-fmigateway \
    VERSION=0.01 \
    FMI_VERSION=2 \
    SESSION=session.yaml \
    SIGNAL_GROUPS="extra/tools/fmi/test/testdata/fmigateway/SG1.yaml,extra/tools/fmi/test/testdata/fmigateway/SG2.yaml" \
    OUT_DIR=./out \
    UUID=2f9f2b62-0718-4e66-8f40-6735e03d4c08
```
