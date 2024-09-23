<!--
Copyright 2024 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# FmiMCL Toolset

Containerised FmiMCL toolset.


## Usage

```bash
```

## Integration Testing

### Building Artifacts

```bash
# Clear the Fmi Tool build folder.
$ rm -rvf extra/tools/fmi/build/stage

# Build x32 targets (optional).
$ make cleanall
$ PACKAGE_ARCH=linux-x86 make build fmi
$ PACKAGE_ARCH=linux-i386 make build fmi

# Build x64 targets.
$ make cleanall
$ make build fmi

# Build the FmiMCL Toolset container (from repo root).
$ make tools

# Generate an FMU:
$ cd extra/tools/fmi
$ bin/fmi gen-fmu \
    -sim build/stage/examples/fmimodelc/sim \
    -name TestFmu \
    -outdir out \
    -libroot build/stage
```


## Development

### Go Module Update (schema updates)

```bash
$ export GOPRIVATE=github.com/boschglobal
$ go clean -modcache
$ go mod tidy
$ go get -x github.com/boschglobal/dse.schemas/code/go/dse@v1.2.6
```

> Note: Release Tags for modules in DSE Schemas are according to the schema `code/go/dse/v1.2.6`.


### Container Debug

```bash
# Start the container.
$ docker run --entrypoint /bin/bash -it --rm -v .:/sim simer:test
```


### Go Module Vendor

```bash
# Vendor the project.
$ go mod tidy
$ go mod vendor
```
