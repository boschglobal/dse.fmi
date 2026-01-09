<!--
Copyright 2024 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Dynamic Simulation Environment - FMI Library

[![CI](https://github.com/boschglobal/dse.fmi/actions/workflows/ci.yaml/badge.svg)](https://github.com/boschglobal/dse.fmi/actions/workflows/ci.yaml)
[![Super Linter](https://github.com/boschglobal/dse.fmi/actions/workflows/super-linter.yml/badge.svg)](https://github.com/boschglobal/dse.fmi/actions/workflows/super-linter.yml)
![GitHub](https://img.shields.io/github/license/boschglobal/dse.fmi)


## Introduction

FMI Libraries of the Dynamic Simulation Environment (DSE) Core Platform provide various solutions for working with FMUs and FMI based simulation environments. Included are:

* [FMI MCL (Model Compatibility Library)](#fmi-mcl) - for loading FMUs into a DSE simulation.
* [ModelC FMU](#modelc-fmu) - for packaging a DSE simulation as an FMU.
* [Gateway FMU](#gateway-fmu) - for bridging between a remote simulation and a DSE simulation.
* [FMU API](#fmu-api) - a minimal API for implementing FMUs with support for Binary Variables and Virtual Networks.

The DSE FMI libraries operate in Co-simulation environments and support both scalar and binary variables.
Virtual networks (e.g. CAN) are implemented using [Network Codecs](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec) and supported via FMI Binary variables, or in the case of FMI 2, by using encoded [FMI String variables](https://github.com/boschglobal/dse.standards/tree/main/modelica/fmi-ls-binary-to-text).


### [FMI MCL]

<img src="doc/static/ki_fmimcl.png" alt="fmimcl.png" align="right" width="220" />

* Multi platform (Linux, Windows) and multi architecture (x64, x86, i386) simulation environment.
* Native FMU support for Co-simulation.
* Virtual Networks (CAN etc.) using [Network Codecs](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec) and Binary Streams. Includes support for FMI 2 (via String variables).

<br/>

### [ModelC FMU]

<img src="doc/static/ki_fmimodelc.png" alt="ki_fmimodelc.png" align="right" width="200" />

* FMU with embedded Model Runtime and SimBus from the [ModelC](https://github.com/boschglobal/dse.modelc/blob/main/dse/modelc/runtime.h) project.
* Packages [DSE Simer](https://boschglobal.github.io/dse.doc/docs/user/simer/) simulation as an FMU.
* FMU interface supports Codec based Binary Streams (e.g. CAN Virtual Bus). Includes support for FMI 2 (via String variables).

<br/>

### [Gateway FMU]

<img src="doc/static/ki_fmigateway.png" alt="ki_fmigateway.png" align="right" width="300" />

* Bridge simulaiton environments using a Gateway FMU.
* Gateway FMU interface supports Codec based Binary Streams, including FMI 2 support.
* Manages the Co-simulation time-domain between the bridged simulation environments.
* Simple lifecycle which can be customised to support automation of simulation environments (e.g. session management).

<br/>

### [FMU API]

* Minimal API for implementing Co-simulation FMUs with methods:
  * `fmu_create()`
  * `fmu_init()`
  * `fmu_step()`
  * `fmu_destroy()`
* Automatic FMI Variable indexing and storage, including FMI 2 support for Binary Variables
* Build targets for FMI 2 and FMI 3.
* Virtual Networks (CAN etc.) using [Network Codecs](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec) and Binary Streams. Includes support for FMI 2 (via String variables).


### Project Structure

```text
└── dse
    └── examples            <-- Examples
    └── fmigateway          <-- Gateway FMU source code
    └── fmimcl              <-- FMI MCL source code
    └── fmimodelc           <-- ModelC FMU source code
    └── fmu                 <-- FMI API source code
    └── importer            <-- Simple FMU Importer
└── extra                   <-- Build infrastructure
    └── tools/fmi           <-- Containerised tools
└── licenses                <-- Third Party Licenses
└── tests                   <-- Unit and E2E tests
└── Taskfile.yml            <-- Workflow automation via Task
```


## Usage

### ModelC FMU with DSE Script and Simulation Builder

> Info: This usage example is derived from [E2E Test Direct Index](tests/testscript/e2e/direct_index.txtar).


__simulation.dse__ (DSE Script)
```hs
simulation
channel in
channel out

uses
dse.fmi file:///repo

model direct dse.fmi.example.direct
    channel in in_vector
    channel out out_vector
    envar OFFSET 100
    envar FACTOR 4

workflow generate-modelcfmu
    var FMU_NAME direct
    var FMI_VERSION 2
    var INDEX direct
```


__Simulation Builder and Operation__

```bash
# Setup environment.
$ export BUILDER_IMAGE=ghcr.io/boschglobal/dse-builder:latest
$ export FMI_IMAGE=ghcr.io/boschglobal/dse-fmi:latest
$ export TASK_X_REMOTE_TASKFILES=1

# Set command aliases.
function dse-builder() {
    ( if test -f "$1"; then cd $(dirname "$1"); fi && docker run -it --user $(id -u):$(id -g) --rm -e AR_USER -e AR_TOKEN -e GHE_USER -e GHE_TOKEN -e GHE_PAT -v $(pwd):/workdir $BUILDER_IMAGE "$@"; )
}

# Install Task (from DSE fork).
$ go install github.com/boschglobal/task/v3/cmd/task@latest

# Build the simulation.
$ dse-builder simulation.dse
$ task -y -v

# Operate the simulation with the Importer.
$ dse/build/_out/importer/fmuImporter --verbose out/fmu/direct
```


### Examples

* [ModelC FMU with Direct Indexing](dse/examples/direct/README.md)



## Build

```bash
# Clone the repo.
$ git clone https://github.com/boschglobal/dse.fmi.git
$ cd dse.fmi

# Optionally set builder images.
$ export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:latest

# Build.
$ make
$ make package

# Build containerised tools (more details in the extra/tools/fmi directory).
$ make build fmi tools

# Optionally use local container images.
$ export FMI_IMAGE=fmi:test
$ export SIMER_IMAGE=simer:test

# Run tests.
$ make test

# Generate documentation.
$ make generate

# Remove (clean) temporary build artifacts.
$ make clean
$ make cleanall
```


### Toolchains

The FMI Library is built using containerised toolchains. Those are
available from the DSE C Library and can be built as follows:

```bash
$ git clone https://github.com/boschglobal/dse.clib.git
$ cd dse.clib
$ make docker
```

Alternatively, the latest Docker Images are available on ghcr.io and can be
used as follows:

```bash
$ export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:latest
```


## Additional Resources

The FMI Library is implemented using the following related repositories:

* [DSE Model C Library](https://github.com/boschglobal/dse.modelc)
* [DSE C Library](https://github.com/boschglobal/dse.clib)
* [DSE Network Codec](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec)
* [DSE Standards Extensions](https://github.com/boschglobal/dse.standards)
  * [Binary Codec specification for FMI 2/3](https://github.com/boschglobal/dse.standards/tree/main/modelica/fmi-ls-binary-codec)
  * [String encoding for binary data, for FMI 2/3](https://github.com/boschglobal/dse.standards/tree/main/modelica/fmi-ls-binary-to-text)
  * [Bus Topologies and Virtual Bus/Networks, for FMI 2/3](https://github.com/boschglobal/dse.standards/tree/main/modelica/fmi-ls-bus-topology)


## Contribute

Please refer to the [CONTRIBUTING.md](./CONTRIBUTING.md) file.


## License

Dynamic Simulation Environment FMI Library is open-sourced under the Apache-2.0 license.
See the [LICENSE](LICENSE) and [NOTICE](./NOTICE) files for details.


### Third Party Licenses

[Third Party Licenses](licenses/)
