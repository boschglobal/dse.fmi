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

The configuration and operation of the gateway is done in the stack.yaml that is to be loaded
by the gateway. The stack follows the following [schema](https://github.com/boschglobal/dse.schemas/blob/main/doc/content/schemas/yaml/Stack.md) with some additional, gateway specific annotations/ additions.

### Stack Metadata

|Name|Type|Required|Description|
|---|---|---|---|
|metadata|object|true|none|
|» name|string|true|This field is required to be named "gateway"|
|» annotation|object|false|none|
|»» start_redis|boolean|true|Set to false with no redis instance should be started on windows, can be controlled via environment variable|
|»» create_logfiles|boolean|false|Set to true to create Logfiles of the started models, can be controlled via environment variable|
|»» show_redis|boolean|false|Set to true if the redis process should be shown as terminal window, can be controlled via environment variable|
|»» show_simbus|boolean|false|Set to true if the simbus process should be shown as terminal window, can be controlled via environment variable|
|»» show_models|boolean|false|Set to true if the model processes should be shown as terminal windows, can be controlled via environment variable|
|»» model_stack|string|false|Name of the Stack.yaml containing the model descriptions. Required for session handling|

```text
---
kind: Stack
metadata:
  name: gateway
  annotations:
    redis_show: true
    simbus_show: true
    models_show: true
    model_stack: stack_models_parser.yaml
```

### Runtime

|Name|Type|Required|Description|
|---|---|---|---|
|spec|object|true|none|
|» runtime|object|false|none|
|»» env|object|false|none|
|»»» REDIS_EXE_PATH|string|false|Path to the redis executable. If set, redis is handled by the gateway|
|»»» SIMBUS_EXE_PATH|string|false|Path to the simbus executable. Required if simbus is handled by the gateway|
|»»» MODELC_EXE_PATH|string|false|Path to the modelc executable. Required if models are handled by the gateway|

```text
---
kind: Stack
metadata:
  name: gateway
spec:
  runtime:
    env:
      REDIS_EXE_PATH: redis.exe
      SIMBUS_EXE_PATH: simbus.exe
      MODELC_EXE_PATH: modelc.exe
```

### Simbus

If the simbus should be handled by the gateway, the simbus description must be part
of the stack.yaml loaded by the gateway. For the handling additional information must be
set as follows:

|Name|Type|Required|Description|
|---|---|---|---|
|spec|object|true|none|
|» models|object|true|none|
|»» annotations|object|false|none|
|»»» cli|object|false|none|
|»»»» step_size|float64|false|Simbus step size. Default 0.0005|
|»»»» end_time|float64|false|Simbus end size. Default 10min (36000), do not set if gateway end time is variable|
|»»»» log_level|int|false|Simbus log level. Default 6 (lower equals to more information)|
|»»»» timeout|float64|false|Simbus timeout. Default 60|
|»» runtime|object|true|none|
|»»» files|object|true|This part contains a list of yaml files that are to be loaded by the simbus|

```text
---
kind: Stack
metadata:
  name: gateway
spec:
  models:
    - name: simbus
      model:
        name: simbus
      annotations:
        cli:
          step_size: 0.005
          end_time: 0.02
          log_level: 4
          timeout: 600.0
      runtime:
        files:
          - stack.yaml
      channels:
        - name: E2M_M2E
          expectedModelCount: 2
        - name: com_phys
          expectedModelCount: 2
```

### Gateway

The gateway description requires additional information in the stack.yaml loaded by the gateway as follows:

|Name|Type|Required|Description|
|---|---|---|---|
|spec|object|true|none|
|» models|object|true|none|
|»» annotations|object|false|none|
|»»» step_size|float64|false|Gateway step size. Default 0.0005|
|»»» end_time|float64|false|Gateway end size. Default 10min (36000), do not set if gateway end time is variable|
|»»» log_level|int|false|Gateway log level. Default 6 (lower equals to more information)|
|»»» log_location|string|false|Path to where the log files of the models have to be saved|
|»»» cmd_envvars|object|false|This part contains a list of environment variales that should be part of the fmi interface|
|»» runtime|object|false|none|
|»»» env|object|false|none|
|»»»» GATEWAY_INIT_CMD|string|false|path to the script/ cmd that is to be run before the simulation starts (in relation to the resource directory of the fmu)|
|»»»» GATEWAY_SHUTDOWN_CMD|string|false|path to the script/ cmd that is to be run after the simulation ends (in relation to the resource directory of the fmu)|

```text
---
kind: Stack
metadata:
  name: gateway
spec:
  models:
    - name: gateway
      uid: 40004
      model:
        name: Gateway
      annotations:
        step_size: 0.0050
        end_time: 0.0200
        log_level: 4
        log_location: "./here"
        cmd_envvars:
          - envar0
          - envar1
          - envar2
      runtime:
        env:
          # Gateway scripts.
          GATEWAY_INIT_CMD: "init_cmd"
          GATEWAY_SHUTDOWN_CMD: "shutdown_cmd"
      channels:
        - name: E2M_M2E
          alias: E2M_M2E
          selectors:
            channel: E2M_M2E
        - name: com_phys
          alias: com_phys
          selectors:
            channel: com_phys
```

### Session handling of models

Models that should be handled (started & stopped) by the gateway are defined in an additional stack.yaml file
that is referenced as described in this [section](#stack-metadata). Additional information required is as follows:

|Name|Type|Required|Description|
|---|---|---|---|
|spec|object|true|none|
|» runtime|object|false|none|
|»» env|object|false|none|
|»»» cli|object|false|none|
|» models|object|true|none|
|»» annotations|object|false|none|
|»»» timeout|float64|false|Timeout definition for all models|
|»»»» step_size|float64|false|Model step size. Default 0.0005|
|»»»» end_time|float64|false|Model end size. Default 10min (36000), do not set if gateway end time is variable|
|»»»» log_level|int|false|Model log level. Default 6 (lower equals to more information)|
|»»»» timeout|float64|false|Model timeout. Default 60|
|»»»» exe|string|false|Model executable. This can be set in case the executable differs from the [runtime modelc exe](#runtime)|
|»» runtime|object|true|none|
|»»» files|object|true|This part contains a list of yaml files that are to be loaded by the model|

```text
---
kind: Stack
metadata:
  name: stack_models
spec:
  connection:
    transport:
      redispubsub:
        uri: redis://localhost:6379
        timeout: 60
  runtime:
    env:
      timeout: 600
  models:
    - name: Model_1
      uid: 42
      model:
        name: Model_1
      annotations:
        cli:
          step_size: 0.0005
          end_time: 20.0
          log_level: 1
          timeout: 600.0
          exe: "different.exe"
      runtime:
        files:
          - stack.yaml
          - model.yaml
          - signalgroup.yaml
      channels:
        - name: E2M_M2E
          alias: signal_channel
          selectors:
            model: Model_1
            channel: signal_vector
```

### Examples

Complete examples of stack.yaml and stack_models.yaml for the gateway and its
session handling.

<details>
<summary>Complete Example Stack.yaml</summary>

```text
---
kind: Stack
metadata:
  name: gateway
  annotations:
    redis_show: true
    simbus_show: true
    models_show: true
    model_stack: stack_models.yaml
spec:
  connection:
    transport:
      redispubsub:
        uri: redis://localhost:6379
        timeout: 60
  runtime:
    env:
      REDIS_EXE_PATH: redis.exe
      SIMBUS_EXE_PATH: simbus.exe
      MODELC_EXE_PATH: modelc.exe
  models:
    - name: simbus
      model:
        name: simbus
      annotations:
        cli:
          step_size: 0.005
          end_time: 0.02
          log_level: 4
          timeout: 600.0
      runtime:
        env:
          SIMBUS_LOGLEVEL: 6
        files:
          - stack.yaml
      channels:
        - name: E2M_M2E
          expectedModelCount: 2
        - name: com_phys
          expectedModelCount: 2
    - name: gateway
      uid: 40004
      model:
        name: Gateway
      annotations:
        step_size: 0.0050
        end_time: 0.0200
        log_level: 4
        log_location: "./here"
        cmd_envvars:
          - envar0
          - envar1
          - envar2
      runtime:
        env:
          # Gateway scripts.
          GATEWAY_INIT_CMD: "init_cmd"
          GATEWAY_SHUTDOWN_CMD: "shutdown_cmd"
      channels:
        - name: E2M_M2E
          alias: E2M_M2E
          selectors:
            channel: E2M_M2E
        - name: com_phys
          alias: com_phys
          selectors:
            channel: com_phys
```

</details>

<details>
<summary>Complete Example Stack_models.yaml</summary>

```text
---
kind: Stack
metadata:
  name: stack_models
spec:
  connection:
    transport:
      redispubsub:
        uri: redis://localhost:6379
        timeout: 60
  models:
    - name: Model_1
      uid: 42
      model:
        name: Model_1
      annotations:
        cli:
          step_size: 0.0005
          end_time: 20.0
          log_level: 1
          timeout: 600.0
          exe: "different.exe"
      runtime:
        files:
          - stack.yaml
          - model.yaml
          - signalgroup.yaml
      channels:
        - name: E2M_M2E
          alias: signal_channel
          selectors:
            model: Model_1
            channel: signal_vector

    - name: Model_2
      uid: 45
      model:
        name: Model_2
      runtime:
        files:
          - stack.yaml
          - model.yaml
          - signalgroup.yaml
      channels:
        - name: com_phys
          alias: signal_channel
          selectors:
            model: Model_2
            channel: signal_vector
```

</details>

### Toolchains

The required files for the FMI gateway can be generated using a toolchain included
in the FMI GO tools. This operation with generate the required modelDescription.xml,
model.yaml and fmu.yaml.

|Name|Required|Description|
|---|---|---|
|VERSION|false|Version of the Gateway FMU|
|FMI_VERSION|false|FMI Version of the Gateway FMU (Default 2)|
|STACK|false|stack.yaml file to be loaded by the gateway|
|SIGNAL_GROUPS|true|List of yaml files to generate the required files for the Gateway FMU|
|OUT_DIR|false|Path to the generated files|
|UUID|false|UUID of the Gateway FMU|

```bash
$ task generate-fmigateway \
    VERSION=0.01 \
    FMI_VERSION=2 \
    STACK=stack.yaml \
    SIGNAL_GROUPS="extra/tools/fmi/test/testdata/fmigateway/SG1.yaml,extra/tools/fmi/test/testdata/fmigateway/SG2.yaml" \
    OUT_DIR=./out \
    UUID=2f9f2b62-0718-4e66-8f40-6735e03d4c08
```
