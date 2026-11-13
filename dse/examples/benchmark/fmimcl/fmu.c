// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fmi2Functions.h>


#define UNUSED(x) ((void)x)


/* Time spent in the FMU functions called by the FMI MCL in model_step(), read
 * by the timing shim (mcl_timing_shim.c) to exclude it from the MCL time.
 */
#if defined(__linux__)
#include <time.h>

typedef struct {
    uint64_t ns;
    uint64_t calls;
} BenchmarkFmuTime;

__attribute__((
    visibility("default"))) volatile BenchmarkFmuTime benchmark_fmu_time;

static inline uint64_t _now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

#define TIMED_RETURN(expr)                                                     \
    do {                                                                       \
        uint64_t   t0_ = _now_ns();                                            \
        fmi2Status s_ = (expr);                                                \
        benchmark_fmu_time.ns += _now_ns() - t0_;                              \
        benchmark_fmu_time.calls++;                                            \
        return s_;                                                             \
    } while (0)
#else
#define TIMED_RETURN(expr) return (expr)
#endif


/* Minimal FMI 2 Co-Simulation FMU for benchmarking the FMI MCL.
 *
 * Real variables which are set by the master (inputs, parameters) are stored.
 * Any other Real variable (outputs) reads back as `steps + vr`, which changes
 * with every fmi2DoStep(). Variables of other types are not supported.
 */

typedef struct {
    double*        value;
    unsigned char* is_set;
    size_t         capacity;
    double         steps;
} BenchmarkFmu;


static int _reserve(BenchmarkFmu* f, size_t count)
{
    if (count <= f->capacity) return 0;

    size_t         capacity = count * 2;
    double*        value = realloc(f->value, capacity * sizeof(double));
    unsigned char* is_set = realloc(f->is_set, capacity);
    if (value) f->value = value;
    if (is_set) f->is_set = is_set;
    if (value == NULL || is_set == NULL) return -1;

    memset(
        &f->value[f->capacity], 0, (capacity - f->capacity) * sizeof(double));
    memset(&f->is_set[f->capacity], 0, capacity - f->capacity);
    f->capacity = capacity;
    return 0;
}


fmi2Component fmi2Instantiate(fmi2String instance_name, fmi2Type fmu_type,
    fmi2String fmu_guid, fmi2String fmu_resource_location,
    const fmi2CallbackFunctions* functions, fmi2Boolean visible,
    fmi2Boolean logging_on)
{
    UNUSED(instance_name);
    UNUSED(fmu_type);
    UNUSED(fmu_guid);
    UNUSED(fmu_resource_location);
    UNUSED(functions);
    UNUSED(visible);
    UNUSED(logging_on);

    return calloc(1, sizeof(BenchmarkFmu));
}

fmi2Status fmi2SetupExperiment(fmi2Component c, fmi2Boolean toleranceDefined,
    fmi2Real tolerance, fmi2Real startTime, fmi2Boolean stopTimeDefined,
    fmi2Real stopTime)
{
    UNUSED(toleranceDefined);
    UNUSED(tolerance);
    UNUSED(startTime);
    UNUSED(stopTimeDefined);
    UNUSED(stopTime);

    return c ? fmi2OK : fmi2Fatal;
}

fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
    return c ? fmi2OK : fmi2Fatal;
}

fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    return c ? fmi2OK : fmi2Fatal;
}

static fmi2Status _do_step(fmi2Component c)
{
    BenchmarkFmu* f = c;
    if (f == NULL) return fmi2Fatal;
    f->steps += 1.0;
    return fmi2OK;
}

fmi2Status fmi2DoStep(fmi2Component c, fmi2Real currentCommunicationPoint,
    fmi2Real    communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint)
{
    UNUSED(currentCommunicationPoint);
    UNUSED(communicationStepSize);
    UNUSED(noSetFMUStatePriorToCurrentPoint);

    TIMED_RETURN(_do_step(c));
}

fmi2Status fmi2Terminate(fmi2Component c)
{
    return c ? fmi2OK : fmi2Fatal;
}

void fmi2FreeInstance(fmi2Component c)
{
    BenchmarkFmu* f = c;
    if (f == NULL) return;
    free(f->value);
    free(f->is_set);
    free(f);
}


static fmi2Status _get_real(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Real value[])
{
    BenchmarkFmu* f = c;
    if (f == NULL) return fmi2Fatal;
    for (size_t i = 0; i < nvr; i++) {
        if (vr[i] < f->capacity && f->is_set[vr[i]]) {
            value[i] = f->value[vr[i]];
        } else {
            value[i] = f->steps + vr[i];
        }
    }
    return fmi2OK;
}

fmi2Status fmi2GetReal(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Real value[])
{
    TIMED_RETURN(_get_real(c, vr, nvr, value));
}

fmi2Status fmi2GetInteger(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Integer value[])
{
    UNUSED(vr);
    for (size_t i = 0; i < nvr; i++) {
        value[i] = 0;
    }
    return c ? fmi2OK : fmi2Fatal;
}

fmi2Status fmi2GetBoolean(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2Boolean value[])
{
    UNUSED(vr);
    for (size_t i = 0; i < nvr; i++) {
        value[i] = 0;
    }
    return c ? fmi2OK : fmi2Fatal;
}

fmi2Status fmi2GetString(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, fmi2String value[])
{
    UNUSED(vr);
    for (size_t i = 0; i < nvr; i++) {
        value[i] = NULL;
    }
    return c ? fmi2OK : fmi2Fatal;
}


static fmi2Status _set_real(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Real value[])
{
    BenchmarkFmu* f = c;
    if (f == NULL) return fmi2Fatal;
    for (size_t i = 0; i < nvr; i++) {
        if (_reserve(f, (size_t)vr[i] + 1) != 0) return fmi2Fatal;
        f->value[vr[i]] = value[i];
        f->is_set[vr[i]] = 1;
    }
    return fmi2OK;
}

fmi2Status fmi2SetReal(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Real value[])
{
    TIMED_RETURN(_set_real(c, vr, nvr, value));
}

fmi2Status fmi2SetInteger(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Integer value[])
{
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(value);
    return c ? fmi2OK : fmi2Fatal;
}

fmi2Status fmi2SetBoolean(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2Boolean value[])
{
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(value);
    return c ? fmi2OK : fmi2Fatal;
}

fmi2Status fmi2SetString(fmi2Component c, const fmi2ValueReference vr[],
    size_t nvr, const fmi2String value[])
{
    UNUSED(vr);
    UNUSED(nvr);
    UNUSED(value);
    return c ? fmi2OK : fmi2Fatal;
}
