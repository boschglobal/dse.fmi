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

FMI Model Compatibility Layer
: Load FMUs into a Dynamic Simulation Environment (i.e. using ModelC/SimBus).


The FMI Library is implemented with support of the [DSE Model C Library](https://github.com/boschglobal/dse.modelc)
and [DSE C Library](https://github.com/boschglobal/dse.clib).


### Project Structure

```text
L- dse
  L- dse/fmimcl  FMI related Library source code.
L- extra         Build infrastructure.
  L- tools       Containerised tools.
L- licenses      Third Party Licenses.
L- tests         Unit and E2E tests.
```


## Usage

### FMI Model Compatibility Layer

<!-- Usage example showing all steps, see Network for example. -->


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
