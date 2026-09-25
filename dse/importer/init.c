// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <dse/importer/importer.h>


extern void _log(const char* format, ...);


void init_list_parse(InitList* l, const char* arg)
{
    char* list = strdup(arg);
    char* saveptr = NULL;
    for (char* item = strtok_r(list, ",", &saveptr); item;
        item = strtok_r(NULL, ",", &saveptr)) {
        char* eq = strchr(item, '=');
        if (eq == NULL) {
            _log("ERROR: bad init item (expected vr=value): %s", item);
            exit(1);
        }
        *eq = '\0';
        l->vr = realloc(l->vr, (l->count + 1) * sizeof(unsigned int));
        l->val = realloc(l->val, (l->count + 1) * sizeof(char*));
        l->vr[l->count] = (unsigned int)strtoul(item, NULL, 10);
        l->val[l->count] = strdup(eq + 1);
        l->count++;
    }
    free(list);
}

void init_list_free(InitList* l)
{
    for (size_t i = 0; i < l->count; i++) {
        free(l->val[i]);
    }
    free(l->val);
    free(l->vr);
}

double* init_real_values(InitList* l, modelDescription* desc)
{
    double* val = calloc(l->count, sizeof(double));
    for (size_t i = 0; i < l->count; i++) {
        val[i] = strtod(l->val[i], NULL);
        /* Prevent the step loop from reverting the value to its start. */
        for (size_t j = 0; j < desc->real.rx_count; j++) {
            if (desc->real.vr_rx_real[j] == l->vr[i]) {
                desc->real.val_rx_real[j] = val[i];
            }
        }
    }
    return val;
}

size_t init_readback_alloc(modelDescription* desc, InitList* init_real,
    unsigned int** vr, double** val)
{
    size_t count = desc->real.rx_count;
    *vr = calloc(count + init_real->count, sizeof(unsigned int));
    *val = calloc(count + init_real->count, sizeof(double));
    memcpy(*vr, desc->real.vr_rx_real, count * sizeof(unsigned int));
    for (size_t i = 0; i < init_real->count; i++) {
        bool found = false;
        for (size_t j = 0; j < desc->real.rx_count; j++) {
            if (desc->real.vr_rx_real[j] == init_real->vr[i]) found = true;
        }
        if (!found) (*vr)[count++] = init_real->vr[i];
    }
    /* Unknown VRs are not written by the FMU. */
    for (size_t i = 0; i < count; i++) {
        (*val)[i] = -1.0;
    }
    return count;
}

void init_readback_log(unsigned int* vr, double* val, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        _log("Init [%u] %f", vr[i], val[i]);
    }
    free(vr);
    free(val);
}
