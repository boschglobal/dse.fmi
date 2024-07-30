<!--
Copyright 2024 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# Dynamic Simulation Environment - FMI Library

[![CI](https://github.com/boschglobal/dse.fmi/actions/workflows/ci.yaml/badge.svg)](https://github.com/boschglobal/dse.fmi/actions/workflows/ci.yaml)
[![Super Linter](https://github.com/boschglobal/dse.fmi/actions/workflows/super-linter.yml/badge.svg)](https://github.com/boschglobal/dse.fmi/actions/workflows/super-linter.yml)
![GitHub](https://img.shields.io/github/license/boschglobal/dse.fmi)


## Introduction

FMI Libraries of the Dynamic Simulation Environment (DSE) Core Platform.

<!-- Overview diagram, PlantUML generated, see Network for example. -->

<dl>
  <dt><b>FMI Model Compatibility Layer</b></dt>
  <dd>Load FMUs into a Dynamic Simulation Environment (i.e. using ModelC/SimBus).</dd>

  <dt><b>FMI ModelC FMU</b></dt>
  <dd>FMU capable of running a DSE Simulation (i.e. ModelC based Simulation Stack).</dd>
</dl>

The FMI Library is implemented with support of:
* [DSE Model C Library](https://github.com/boschglobal/dse.modelc)
* [DSE C Library](https://github.com/boschglobal/dse.clib)
* [DSE Network Codec](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec)


### Project Structure

```text
L- dse
  L- dse/fmimcl     FMI MCL source code.
  L- dse/fmimodelc  FMI ModelC FMU source code.
L- extra            Build infrastructure.
  L- tools          Containerised tools.
L- licenses         Third Party Licenses.
L- tests            Unit and E2E tests.
```


## Usage

### FMI Model Compatibility Layer

<!-- Usage example showing all steps, see Network for example. -->


### FMI ModelC FMU

<!-- Usage example showing all steps, see Network for example. -->


#### Example: Network FMU with CAN Network Topology

The FMI ModelC FMU includes an example Network FMU which demonstrates how a
CAN Network Topology can be realised using FMI2 String variables and a wrapped
ModelC Simulation Stack with models which implement a [Network Codec](https://github.com/boschglobal/dse.standards/tree/main/dse/ncodec).


__Network FMU Layout:__

```text
L- network_fmu             The example Network FMU.
  L- bin/fmi2importer      Importer (simple) which can run the Network FMU.
L- fmu                     FMU Package.
  L- modelDescription.xml  Model description for the FMU.
  L- lib/linux-amd64
    L- fmi2modelcfmu.so    FMU shared library.
  L- resources/sim         ModelC Simulation Stack.
    L- data
      L- simulation.yaml   Simulation Stack specification.
      L- model.yaml        Model specification.
    L- lib
      L- target.so         The ModelC model shared library.
```


__Network FMU Operation:__

```bash
# Build the DSE FMI and examples.
$ git clone https://github.com/boschglobal/dse.fmi.git
$ cd dse.fmi
$ make

# Change to the FMU directory and run the Importer/FMU.
$ cd dse/build/_out/fmimodelc/examples/network_fmu/fmu
$ ../bin/fmi2importer lib/linux-amd64/libfmi2modelcfmu.so
Importer: Loading FMU: lib/linux-amd64/libfmi2modelcfmu.so ...
ModelCFmu: Create the FMU Model Instance Data
ModelCFmu: Resource location: resource
ModelCFmu: Allocate the RuntimeModelDesc object
ModelCFmu: Create the Model Runtime object
ModelCFmu: Call model_runtime_create() ...
Runtime: Version: 2.0.24
Runtime: Platform: linux-amd64
Runtime: Simulation Path: resources/sim
Runtime: Simulation YAML: resources/sim/data/simulation.yaml
Runtime: Model: network_fmu
Load YAML File: resources/sim/data/simulation.yaml
Load YAML File: resources/sim/data/model.yaml
...
```


## Build

> Note : see the following section on configuring toolchains.

```bash
# Get the repo.
$ git clone https://github.com/boschglobal/dse.fmi.git
$ cd dse.fmi

# Optionally set builder images.
$ export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:main

# Build.
$ make

# Run tests.
$ make test

# Build containerised tools.
$ make tools

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
$ export GCC_BUILDER_IMAGE=ghcr.io/boschglobal/dse-gcc-builder:main
```


## Contribute

Please refer to the [CONTRIBUTING.md](./CONTRIBUTING.md) file.


## License

Dynamic Simulation Environment FMI Library is open-sourced under the Apache-2.0 license.
See the [LICENSE](LICENSE) and [NOTICE](./NOTICE) files for details.


### Third Party Licenses

[Third Party Licenses](licenses/)
