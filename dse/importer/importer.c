// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <fmi2FunctionTypes.h>
#include <fmi3FunctionTypes.h>
#include <dse/importer/importer.h>
#include <dse/fmu/fmu.h>


/**
Importer for FMU with Model Runtime
====================================

This Importer is able to load and operate an FMU with a simple Co-Simulation.

Support for both FMI 2 and FMI 3 Co-Simulation.

*/


/* Define types for the FMI 2 inteface methods being used. */
typedef void* (*fmi2Instantiate)();
typedef int32_t (*fmi2ExitInitializationMode)();
typedef int32_t (*fmi2GetReal)();
typedef int32_t (*fmi2GetString)();
typedef int32_t (*fmi2SetReal)();
typedef int32_t (*fmi2SetString)();
typedef int32_t (*fmi2DoStep)();
typedef void (*fmi2FreeInstance)();


/* Define types for the FMI 3 inteface methods being used. */
typedef void* (*fmi3InstantiateCoSimulation)();
typedef int32_t (*fmi3ExitInitializationMode)();
typedef int32_t (*fmi3GetFloat64)();
typedef int32_t (*fmi3GetBinary)();
typedef int32_t (*fmi3SetFloat64)();
typedef int32_t (*fmi3SetBinary)();
typedef int32_t (*fmi3DoStep)();
typedef void (*fmi3FreeInstance)();


#define UNUSED(x)      ((void)x)
#define ARRAY_SIZE(x)  (sizeof(x) / sizeof(x[0]))
#define MODEL_XML_FILE "modelDescription.xml"


#define OPT_LIST       "hs:X:P:"
static struct option long_options[] = {
    { "help", no_argument, NULL, 'h' },
    { "step_size", optional_argument, NULL, 's' },
    { "steps", optional_argument, NULL, 'X' },
    { "platform", optional_argument, NULL, 'P' },
};

static inline void print_usage()
{
    printf("usage: fmuImporter [options] [<fmu_path>]\n\n");
    printf("      [<fmu_path>] (defaults to working directory)\n");
    printf("      [-h, --help]\n");
    printf("      [-s, --step_size]\n");
    printf("      [-X, --steps]\n");
    printf("      [-P, --platform] (defaults to linux-amd64)\n");
}

static void _log(const char* format, ...)
{
    printf("Importer: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}


static void _write_ncodec_msg(char** val_tx_binary, char** val_rx_binary,
    char* mimetype, uint32_t frame_id, char* response)
{
    size_t data_len = strlen(*val_tx_binary);
    char*  ncodec_tx = ascii85_decode(*val_tx_binary, &data_len);
    importer_ncodec_read(mimetype, (uint8_t*)ncodec_tx, data_len);

    uint8_t* ncodec_rx = NULL;
    size_t   ncodec_rx_len = 0;
    importer_codec_write(frame_id, 2, (uint8_t*)response, strlen(response),
        &ncodec_rx, &ncodec_rx_len, mimetype);

    *val_rx_binary = ascii85_encode((char*)ncodec_rx, ncodec_rx_len);

    /* Cleanup. */
    free(ncodec_tx);
    free(ncodec_rx);
    free(*val_tx_binary);
    *val_tx_binary = NULL;
}


static int _run_fmu2_cosim(
    modelDescription* desc, void* handle, double step_size, unsigned int steps)
{
    /* Setup the FMU
     * ============= */
    fmi2Component* fmu = NULL;

    /* fmi2Instantiate */
    char* dlerror_str;
    dlerror();
    fmi2Instantiate instantiate = dlsym(handle, "fmi2Instantiate");
    dlerror_str = dlerror();
    if (dlerror_str || instantiate == NULL) {
        _log("ERROR: could not load fmi2Instantiate() from FMU: %s",
            dlerror_str);
        return EINVAL;
    }
    if (instantiate == NULL) {
        return EINVAL;
    }
    fmu = instantiate(
        "fmu", fmi2CoSimulation, "guid", "resources", NULL, true, true);
    if (fmu == NULL) return EINVAL;

    /* fmi2ExitInitializationMode */
    fmi2ExitInitializationMode exit_init_mode =
        dlsym(handle, "fmi2ExitInitializationMode");
    if (exit_init_mode == NULL) return EINVAL;
    exit_init_mode(fmu);


    /* Step the FMU
     * ============ */
    double model_time = 0.0;
    _log("Scalar Variables: Input %lu, Output %lu", desc->real.rx_count,
        desc->real.tx_count);
    _log("Binary Variables: Input %lu, Output %lu", desc->binary.rx_count,
        desc->binary.tx_count);


    /* Load Get and Set functions. */
    fmi2GetReal get_real = dlsym(handle, "fmi2GetReal");
    if (get_real == NULL) return EINVAL;
    fmi2GetString get_string = dlsym(handle, "fmi2GetString");
    if (get_string == NULL) return EINVAL;
    fmi2SetReal set_real = dlsym(handle, "fmi2SetReal");
    if (set_real == NULL) return EINVAL;
    fmi2SetString set_string = dlsym(handle, "fmi2SetString");
    if (set_string == NULL) return EINVAL;

    /* fmi2DoStep */
    fmi2DoStep do_step = dlsym(handle, "fmi2DoStep");
    if (do_step == NULL) return EINVAL;

    for (unsigned int j = 0; j < steps; j++) {
        /* Loopback the binary data. */
        for (size_t i = 0; i < desc->binary.tx_count; i++) {
            if (desc->binary.val_tx_binary[i]) {
                if (desc->binary.tx_binary_info) {
                    if (strcmp(desc->binary.tx_binary_info[i]->type, "frame") ==
                        0) {
                        int32_t frame_id = (i + (j * 10));
                        char    resp[128];
                        snprintf(
                            resp, sizeof(resp), "Hello from Importer (%ld)", i);
                        _write_ncodec_msg(&(desc->binary.val_tx_binary[i]),
                            &(desc->binary.val_rx_binary[i]),
                            desc->binary.rx_binary_info[i]->mimetype, frame_id,
                            resp);
                        continue;
                    }
                }

                desc->binary.val_rx_binary[i] = desc->binary.val_tx_binary[i];
                desc->binary.val_tx_binary[i] = NULL;
            }
        }

        set_string(fmu, desc->binary.vr_rx_binary, desc->binary.rx_count,
            desc->binary.val_rx_binary);
        for (size_t i = 0; i < desc->binary.rx_count; i++) {
            /* Release the string (FMU should have duplicated). */
            if (desc->binary.val_rx_binary[i]) {
                free(desc->binary.val_rx_binary[i]);
                desc->binary.val_rx_binary[i] = NULL;
            }
        }
        set_real(fmu, desc->real.vr_rx_real, desc->real.rx_count,
            desc->real.val_rx_real);

        _log("Calling fmi2DoStep(): model_time=%f, step_size=%f", model_time,
            step_size);
        int rc = do_step(fmu, model_time, step_size);
        if (rc != 0) {
            _log("step() returned error code: %d", rc);
        }

        /* Read from FMU. */
        get_real(fmu, desc->real.vr_tx_real, desc->real.tx_count,
            desc->real.val_tx_real);
        get_string(fmu, desc->binary.vr_tx_binary, desc->binary.tx_count,
            desc->binary.val_tx_binary);
        for (size_t i = 0; i < desc->binary.tx_count; i++) {
            /* Duplicate received strings (in-case FMU releases). */
            if (desc->binary.val_tx_binary[i]) {
                desc->binary.val_tx_binary[i] =
                    strdup(desc->binary.val_tx_binary[i]);
            }
        }

        /* Increment model time. */
        model_time += step_size;
    }
    _log("Scalar Variables:");
    for (size_t i = 0; i < desc->real.tx_count; i++) {
        _log("  [%d] %lf", desc->real.vr_tx_real[i], desc->real.val_tx_real[i]);
    }
    _log("String Variables:");
    for (size_t i = 0; i < desc->binary.tx_count; i++) {
        _log("  [%d] %s", desc->binary.vr_tx_binary[i],
            desc->binary.val_tx_binary[i]);
    }


    /* Terminate/Free the FMU
     * ====================== */
    fmi2FreeInstance free_instance = dlsym(handle, "fmi2FreeInstance");
    if (free_instance == NULL) return EINVAL;
    free_instance(fmu);

    return 0;
}


static int _run_fmu3_cosim(
    modelDescription* desc, void* handle, double step_size, unsigned int steps)
{
    /* Setup the FMU
     * ============= */
    void* fmu = NULL;

    /* fmi3Instantiate */
    fmi3InstantiateCoSimulation instantiate =
        dlsym(handle, "fmi3InstantiateCoSimulation");
    if (instantiate == NULL) return EINVAL;
    fmu = instantiate("fmu", "guid", "resources", false, true, false, false,
        NULL, 0, NULL, NULL, NULL);
    if (fmu == NULL) return EINVAL;

    /* fmi3ExitInitializationMode */
    fmi3ExitInitializationMode exit_init_mode =
        dlsym(handle, "fmi3ExitInitializationMode");
    if (exit_init_mode == NULL) return EINVAL;
    exit_init_mode(fmu);


    /* Step the FMU
     * ============ */
    double model_time = 0.0;
    _log("Scalar Variables: Input %lu, Output %lu", desc->real.rx_count,
        desc->real.tx_count);
    _log("Binary Variables: Input %lu, Output %lu", desc->binary.rx_count,
        desc->binary.tx_count);


    /* Load Get and Set functions. */
    fmi3GetFloat64 get_float64 = dlsym(handle, "fmi3GetFloat64");
    if (get_float64 == NULL) return EINVAL;
    fmi3GetBinary get_binary = dlsym(handle, "fmi3GetBinary");
    if (get_binary == NULL) return EINVAL;
    fmi3SetFloat64 set_float64 = dlsym(handle, "fmi3SetFloat64");
    if (set_float64 == NULL) return EINVAL;
    fmi3SetBinary set_binary = dlsym(handle, "fmi3SetBinary");
    if (set_binary == NULL) return EINVAL;

    /* fmi3DoStep */
    fmi3DoStep do_step = dlsym(handle, "fmi3DoStep");
    if (do_step == NULL) return EINVAL;

    for (unsigned int j = 0; j < steps; j++) {
        /* Loopback the binary data. */
        for (size_t i = 0; i < desc->binary.tx_count; i++) {
            if (desc->binary.val_tx_binary[i]) {
                if (desc->binary.tx_binary_info) {
                    if (strcmp(desc->binary.tx_binary_info[i]->type, "frame") ==
                        0) {
                        int32_t frame_id = (i + (j * 10));
                        char    resp[128];
                        snprintf(
                            resp, sizeof(resp), "Hello from Importer (%ld)", i);
                        _write_ncodec_msg(&(desc->binary.val_tx_binary[i]),
                            &(desc->binary.val_rx_binary[i]),
                            desc->binary.rx_binary_info[i]->mimetype, frame_id,
                            resp);
                        continue;
                    }
                }
                desc->binary.val_rx_binary[i] = desc->binary.val_tx_binary[i];
                desc->binary.val_tx_binary[i] = NULL;
            }
        }

        set_binary(fmu, desc->binary.vr_rx_binary, desc->binary.rx_count,
            desc->binary.val_size_rx_binary, desc->binary.val_rx_binary,
            desc->binary.rx_count);
        for (size_t i = 0; i < desc->binary.rx_count; i++) {
            /* Release the string (FMU should have duplicated). */
            if (desc->binary.val_rx_binary[i]) {
                free(desc->binary.val_rx_binary[i]);
                desc->binary.val_rx_binary[i] = NULL;
            }
        }
        set_float64(fmu, desc->real.vr_rx_real, desc->real.rx_count,
            desc->real.val_rx_real, desc->real.rx_count);

        _log("Calling fmi3DoStep(): model_time=%f, step_size=%f", model_time,
            step_size);
        int rc = do_step(fmu, model_time, step_size);
        if (rc != 0) {
            _log("step() returned error code: %d", rc);
        }

        /* Read from FMU. */
        get_float64(fmu, desc->real.vr_tx_real, desc->real.tx_count,
            desc->real.val_tx_real, desc->real.tx_count);
        get_binary(fmu, desc->binary.vr_tx_binary, desc->binary.tx_count,
            desc->binary.val_size_tx_binary, desc->binary.val_tx_binary,
            desc->binary.tx_count);
        for (size_t i = 0; i < desc->binary.tx_count; i++) {
            /* Duplicate received strings (in-case FMU releases). */
            if (desc->binary.val_tx_binary[i]) {
                desc->binary.val_tx_binary[i] =
                    strdup(desc->binary.val_tx_binary[i]);
            }
        }

        /* Increment model time. */
        model_time += step_size;
    }
    _log("Scalar Variables:");
    for (size_t i = 0; i < desc->real.tx_count; i++) {
        _log("  [%d] %lf", desc->real.vr_tx_real[i], desc->real.val_tx_real[i]);
    }
    _log("String Variables:");
    for (size_t i = 0; i < desc->binary.tx_count; i++) {
        _log("  [%d] %s", desc->binary.vr_tx_binary[i],
            desc->binary.val_tx_binary[i]);
    }


    /* Terminate/Free the FMU
     * ====================== */
    fmi3FreeInstance free_instance = dlsym(handle, "fmi3FreeInstance");
    if (free_instance == NULL) return EINVAL;
    free_instance(fmu);

    return 0;
}


static inline void _parse_arguments(int argc, char** argv, double* step_size,
    unsigned int* steps, const char** fmu_path, const char** platform)
{
    extern int   optind, optopt;
    extern char* optarg;
    int          c;
    /* Parse the named options. */
    optind = 1;
    while ((c = getopt_long(argc, argv, OPT_LIST, long_options, NULL)) != -1) {
        switch (c) {
        case 'h': {
            print_usage();
            exit(0);
        }
        default:
            break;
        }
    }

    optind = 1;
    while ((c = getopt_long(argc, argv, OPT_LIST, long_options, NULL)) != -1) {
        switch (c) {
        case 'h':
            break;
        case 's':
            *step_size = atof(optarg);
            break;
        case 'X':
            *steps = atoi(optarg);
            break;
        case 'P':
            *platform = optarg;
            break;
        default:
            exit(1);
        }
    }

    // get the fmu Path
    if ((optind + 1) <= argc) {
        *fmu_path = argv[optind];
    }
}

int main(int argc, char** argv)
{
    double       step_size = 0.0005;
    unsigned int steps = 10;
    const char*  fmu_path = NULL;
    const char*  platform = "linux-amd64";
    static char  _cwd[PATH_MAX];


    /* Parse arguments
     * =============== */
    _parse_arguments(argc, argv, &step_size, &steps, &fmu_path, &platform);
    getcwd(_cwd, PATH_MAX);
    if (fmu_path == NULL) {
        fmu_path = _cwd;
    }
    errno = 0;
    if (chdir(fmu_path)) {
        _log("ERROR: Could not change to FMU path: %s", fmu_path);
        return errno || EINVAL;
    }
    getcwd(_cwd, PATH_MAX);
    _log("FMU Dir: %s", _cwd);
    _log("Step Size: %f", step_size);
    _log("Steps: %u", steps);
    _log("Platform: %s", platform);
    _log("Loading FMU Definition: %s", MODEL_XML_FILE);
    modelDescription* desc = parse_model_desc(MODEL_XML_FILE, platform);
    if (desc == NULL) {
        _log("ERROR: Could not parse the model correctly!");
        return EINVAL;
    }
    _log("FMU Version: %d", atoi(desc->version));

    /* Load the FMU
     * ============ */
    _log("Loading FMU: %s", desc->fmu_lib_path);
    dlerror();
    void* handle = dlopen(desc->fmu_lib_path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        _log("ERROR: dlopen call failed: %s", dlerror());
        _log("Model library not loaded!");
        return ENOSYS;
    }

    /* Run a CoSimulation
     * ================== */
    int rc = 0;
    switch (atoi(desc->version)) {
    case 2:
        rc = _run_fmu2_cosim(desc, handle, step_size, steps);
        break;
    case 3:
        rc = _run_fmu3_cosim(desc, handle, step_size, steps);
        break;
    default:
        _log("Unsupported FMI version (%s)!", desc->version);
        return EINVAL;
    }
    _log("Simulation return value: %d", rc);

    /* Release allocated resources
     * =========================== */
    free(desc->version);
    free(desc->fmu_lib_path);
    for (size_t i = 0; i < desc->binary.tx_count; i++) {
        if (desc->binary.val_tx_binary[i]) {
            free(desc->binary.val_tx_binary[i]);
            desc->binary.val_tx_binary[i] = NULL;
        }
        if (desc->binary.tx_binary_info[i]) {
            free(desc->binary.tx_binary_info[i]->mimetype);
            free(desc->binary.tx_binary_info[i]->start);
            free(desc->binary.tx_binary_info[i]->type);
            free(desc->binary.tx_binary_info[i]);
        }
    }
    for (size_t i = 0; i < desc->binary.rx_count; i++) {
        if (desc->binary.val_rx_binary[i]) {
            free(desc->binary.val_rx_binary[i]);
            desc->binary.val_rx_binary[i] = NULL;
        }
        if (desc->binary.rx_binary_info[i]) {
            free(desc->binary.rx_binary_info[i]->mimetype);
            free(desc->binary.rx_binary_info[i]->start);
            free(desc->binary.rx_binary_info[i]->type);
            free(desc->binary.rx_binary_info[i]);
        }
    }
    free(desc->binary.rx_binary_info);
    free(desc->binary.tx_binary_info);

    free(desc->binary.vr_tx_binary);
    free(desc->binary.val_tx_binary);
    free(desc->binary.val_size_tx_binary);
    free(desc->binary.vr_rx_binary);
    free(desc->binary.val_rx_binary);
    free(desc->binary.val_size_rx_binary);
    free(desc->real.vr_tx_real);
    free(desc->real.val_tx_real);
    free(desc->real.vr_rx_real);
    free(desc->real.val_rx_real);
    free(desc);

    dlclose(handle);

    /* Return result. */
    return rc;
}
