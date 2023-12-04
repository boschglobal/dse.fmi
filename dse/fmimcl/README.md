<!--
Copyright 2023 Robert Bosch GmbH

SPDX-License-Identifier: Apache-2.0
-->

# FMIMCL

## Design

### Schema

#### Model

Fixed configuration of the Model. Paths to DLLs and Signal Vector alias.

#### Signal Group

Generated per FMU. List of signals per Signal Vector alias.

#### Model Instance

Specification of the Model and Signal Group (i.e. alias vector linking).

### Questions

* Where is the FMU specific details (version / path)
  * Signal Group : same place as variables.
  * Model Instance : far away from actual "thing".
* Is the Model.yaml the best place?
  * Network has model.yaml and network.yaml ...
  * SMCL has:
    * smcl.yaml - model / mcl (static)
    * model.yaml - specific S-Fun (each model) (i.e. adapter selection)
    * signal_group.yaml - signals and structs of the S-Fun
    * stack.yaml - links to smcl.yaml and model.yaml (i.e. strategy selection)

Possible solution for YAML:

* model.yaml
  * Model - static MCL config
* fmu.yaml
  * Model - fmu specific config
  * SignalGroup - variables (annotations)
* simulation
  * Stack - fmu instance config
    * selector for model.yaml and fmu.yaml
    * selection of strategy
