// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#define _GNU_SOURCE
#include <dlfcn.h>
#include <inttypes.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* Times model_step() of the FMI MCL without modifying it (see
 * scripts/timing.sh).
 *
 * This library is installed in place of the FMI MCL (same file name), the real
 * FMI MCL is installed next to it with the prefix `real_`. ModelC calls the
 * model_*() functions of this library, which call the real ones and time
 * model_step(): marshal out, FMU step and marshal in (not the SimBus exchange
 * which is done by ModelC).
 *
 * Only the FMI MCL is measured. Per step, the time spent inside the FMU (which
 * the benchmark FMU publishes as `benchmark_fmu_time`) and the calibrated cost
 * of the clock reads are subtracted. Only the vDSO clock CLOCK_MONOTONIC is
 * read, so there are no syscalls in or next to the measured region.
 *
 * At model_destroy() one line of `key=value` pairs is appended to the file
 * named by the environment variable MCL_TIMING_FILE.
 */

typedef struct ModelDesc ModelDesc;

/* As in fmu.c. */
typedef struct {
    uint64_t ns;
    uint64_t calls;
} BenchmarkFmuTime;

static void* real;
static ModelDesc* (*real_create)(ModelDesc*);
static int (*real_step)(ModelDesc*, double*, double);
static void (*real_destroy)(ModelDesc*);

static const volatile BenchmarkFmuTime  no_fmu_time;
static const volatile BenchmarkFmuTime* fmu_time = &no_fmu_time;
static int64_t  clock_ns; /* Duration of one clock read. */
static int64_t* mcl_ns;   /* FMI MCL time per step. */
static int64_t* fmu_ns;   /* FMU time per step. */
static size_t   count;
static size_t   capacity;


static inline int64_t _now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + (int64_t)ts.tv_nsec;
}

static int _cmp_int64(const void* a, const void* b)
{
    int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
    return (x > y) - (x < y);
}

/* An interval measured with two clock reads is its content plus one read. */
static int64_t _calibrate(void)
{
    enum { N = 10001 };
    static int64_t d[N];
    for (size_t i = 0; i < N; i++) {
        int64_t t0 = _now_ns();
        d[i] = _now_ns() - t0;
    }
    qsort(d, N, sizeof(int64_t), _cmp_int64);
    return d[N / 2];
}

/* The FMU is loaded RTLD_LOCAL by the FMI MCL, search all loaded objects. */
static void _find_fmu_time(void)
{
    struct link_map* lm;
    if (dlinfo(real, RTLD_DI_LINKMAP, &lm) != 0) return;
    while (lm->l_prev)
        lm = lm->l_prev;
    for (; lm; lm = lm->l_next) {
        if (lm->l_name == NULL || lm->l_name[0] == '\0') continue;
        void* h = dlopen(lm->l_name, RTLD_LAZY | RTLD_NOLOAD);
        if (h == NULL) continue;
        void* sym = dlsym(h, "benchmark_fmu_time");
        dlclose(h);
        if (sym) {
            fmu_time = sym;
            return;
        }
    }
    fprintf(stderr, "mcl_timing_shim: benchmark_fmu_time not found, the FMU "
                    "time is included in the FMI MCL time\n");
}

static int _load_real(void)
{
    if (real) return 0;

    Dl_info info;
    if (dladdr((void*)_load_real, &info) == 0) return -1;
    const char* slash = strrchr(info.dli_fname, '/');
    size_t      dir_len = slash ? (size_t)(slash - info.dli_fname + 1) : 0;
    const char* base = info.dli_fname + dir_len;

    size_t path_len = dir_len + strlen("real_") + strlen(base) + 1;
    char* path = malloc(path_len);
    if (path == NULL) return -1;
    memcpy(path, info.dli_fname, dir_len);
    snprintf(path + dir_len, path_len - dir_len, "real_%s", base);
    real = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (real == NULL) fprintf(stderr, "mcl_timing_shim: %s\n", dlerror());
    free(path);
    if (real == NULL) return -1;

    real_create = (ModelDesc * (*)(ModelDesc*)) dlsym(real, "model_create");
    real_step = (int (*)(ModelDesc*, double*, double))dlsym(real, "model_step");
    real_destroy = (void (*)(ModelDesc*))dlsym(real, "model_destroy");
    return (real_create && real_step && real_destroy) ? 0 : -1;
}

static void _report(void)
{
    const char* file = getenv("MCL_TIMING_FILE");
    if (file == NULL || file[0] == '\0' || count == 0) return;

    double mcl_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        mcl_sum += (double)mcl_ns[i];
    }
    qsort(mcl_ns, count, sizeof(int64_t), _cmp_int64);
    qsort(fmu_ns, count, sizeof(int64_t), _cmp_int64);
    FILE* fp = fopen(file, "a");
    if (fp == NULL) return;
    fprintf(fp,
        "steps=%zu mcl_wall_us_per_step=%.3f mcl_wall_us_median=%.3f "
        "mcl_wall_us_p95=%.3f fmu_wall_us_median=%.3f clock_ns=%" PRId64 " "
        "fmu_excluded=%d\n",
        count, mcl_sum * 1e-3 / (double)count, (double)mcl_ns[count / 2] * 1e-3,
        (double)mcl_ns[count * 95 / 100] * 1e-3,
        (double)fmu_ns[count / 2] * 1e-3, clock_ns,
        fmu_time != &no_fmu_time);
    fclose(fp);
}


__attribute__((visibility("default"))) ModelDesc* model_create(ModelDesc* m)
{
    if (_load_real() != 0) return NULL;
    ModelDesc* model = real_create(m);
    _find_fmu_time();
    clock_ns = _calibrate();
    return model;
}

__attribute__((visibility("default"))) int model_step(
    ModelDesc* m, double* model_time, double stop_time)
{
    uint64_t fmu0 = fmu_time->ns;
    uint64_t calls0 = fmu_time->calls;
    int64_t  t0 = _now_ns();
    int      rc = real_step(m, model_time, stop_time);
    int64_t  t1 = _now_ns();
    int64_t  fmu = (int64_t)(fmu_time->ns - fmu0);
    int64_t  calls = (int64_t)(fmu_time->calls - calls0);

    if (count == capacity) {
        size_t   new_capacity = capacity ? capacity * 2 : 1024;
        int64_t* p = realloc(mcl_ns, new_capacity * sizeof(int64_t));
        if (p) mcl_ns = p;
        int64_t* f = realloc(fmu_ns, new_capacity * sizeof(int64_t));
        if (f) fmu_ns = f;
        if (p == NULL || f == NULL) return rc;
        capacity = new_capacity;
    }
    /* Each FMU call adds two clock reads, its measured time one of them. */
    mcl_ns[count] = (t1 - t0) - fmu - (calls + 1) * clock_ns;
    fmu_ns[count] = fmu - calls * clock_ns;
    count++;
    return rc;
}

__attribute__((visibility("default"))) void model_destroy(ModelDesc* m)
{
    real_destroy(m);
    _report();
    free(mcl_ns);
    free(fmu_ns);
    mcl_ns = fmu_ns = NULL;
    count = capacity = 0;
}
