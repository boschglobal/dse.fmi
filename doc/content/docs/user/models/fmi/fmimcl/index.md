---
title: "FMI Model Compatibility Library"
linkTitle: "MCL"
weight: 400
tags:
- FMI
- MCL
- Model
github_repo: "https://github.com/boschglobal/dse.fmi"
github_subdir: "doc"
---

## Synposis

FMI Model Compatibility Library


## Simulation Setup

### Structure

#### Example Simulation

```text

```

## Setup

> DOC: Provide description of FMI MCL Taskfile workflows.


## Operation

### Measurement

The FMI MCL can be configured to produce an MDF measurement file for scalar
signals of the FMUs interface, including FMI Variables with 'local' causality.

> Note: the path of the measurement file is relative to the simulation path (i.e. /sim/...).


```bash
# Using Simer; enable measurement on the FMU represented by 'fmu_inst'.
$ simer path/to/simulation \
    -env fmu_inst:MEASUREMENT_FILE=/sim/fmu_measurement.mf4 \
    -stepsize 0.0005 -endtime 0.005
$ ls path/to/simulation/fmu_measurement.mf4
path/to/simulation/fmu_measurement.mf4
```
