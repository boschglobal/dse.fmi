<!--
Copyright 2024 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# DSE FMI Toolset

Containerised DSE FMI toolset.


## Usage

```bash
# Using a local build `dse-fmi` container.
$ export FMI_IMAGE=fmi
$ export FMI_TAG=test

# List the toolchains available:
$ task -l
task: Available tasks for this project:
* generate-fmimcl:          Generate an FMI MCL from an existing FMU.
* generate-fmimodelc:       Generate a FMI ModelC FMU from an existing (DSE/ModelC) Simer simulation.

# Construct the examples:
$ make build fmi

# Build an FMU from the staged example:
$ task generate-fmimodelc \
    SIM=extra/tools/fmi/build/stage/examples/fmimodelc/sim \
    FMU_NAME=fubar \
    VERSION=1.0.0
Running FMI Toolset command: gen-fmu
Options:
  libroot        : /usr/local
  log            : 4
  name           : fubar
  outdir         : out
  platform       : linux-amd64
  signalgroups   :
  sim            : extra/tools/fmi/build/stage/examples/fmimodelc/sim
  uuid           : 11111111-2222-3333-4444-555555555555
  version        : 1.0.0
Scanning simulation (extra/tools/fmi/build/stage/examples/fmimodelc/sim) ...
Build the FMU file layout (out/fubar) ...
Create FMU Model Description (out/fubar/modelDescription.xml) ...
Adding SignalGroup: scalar_vector (extra/tools/fmi/build/stage/examples/fmimodelc/sim/data/model.yaml)
Adding SignalGroup: network_vector (extra/tools/fmi/build/stage/examples/fmimodelc/sim/data/model.yaml)
Create FMU Package (out/fubar.fmu) ...

$ ls out out/fubar out/fubar/binaries out/fubar/resources
out:
fubar/  fubar.fmu*

out/fubar:
binaries/  modelDescription.xml*  resources/

out/fubar/binaries:
linux64/

out/fubar/resources:
licenses/  sim/

# Configure and FMI MCL from the example FMU:
$ task generate-fmimcl \
    FMU_DIR=dse/build/_out/fmimcl/examples/fmu \
    MCL_PATH=dse/build/_out/fmimcl/lib/libfmimcl.so
Running FMI Toolset command: gen-mcl
Options:
  fmu            : dse/build/_out/fmimcl/examples/fmu
  log            : 4
  mcl            : dse/build/_out/fmimcl/lib/libfmimcl.so
  outdir         : out/model
  platform       : linux-amd64
Reading FMU Desciption (dse/build/_out/fmimcl/examples/fmu/modelDescription.xml)
Creating Model YAML: fmi2fmu (out/model/model.yaml)
Running FMI Toolset command: gen-signalgroup
Options:
  input          : dse/build/_out/fmimcl/examples/fmu/modelDescription.xml
  log            : 4
  output         : out/model/signalgroup.yaml
Reading file: dse/build/_out/fmimcl/examples/fmu/modelDescription.xml
Appending file: out/model/signalgroup.yaml

$ ls  out/model
model.yaml*  signalgroup.yaml*


$ task generate-fmigateway \
    SIGNAL_GROUPS="extra/tools/fmi/test/testdata/fmigateway/SG1.yaml,extra/tools/fmi/test/testdata/fmigateway/SG2.yaml"
Running FMI Toolset command: gen-gateway
Options:
  fmiVersion     : 2
  libroot        : /usr/local
  log            : 4
  outdir         : out
  session        : 
  signalgroups   : extra/tools/fmi/test/testdata/fmigateway/SG1.yaml,extra/tools/fmi/test/testdata/fmigateway/SG2.yaml
  uuid           : 
  version        : 0.0.1
Adding SignalGroup Model_1 to out/fmu.yaml
Adding SignalGroup Model_2 to out/fmu.yaml
Creating Model YAML: gateway (out/model.yaml)

$ ls out/
fmu.yaml  modelDescription.xml  model.yaml
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

# Build the DSE FMI Toolset container (from repo root).
$ make tools
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
