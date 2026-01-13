// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dse/importer/importer.h>


extern void _log(const char* format, ...);


CsvDesc* csv_open(const char* path)
{
    if (path == NULL) return NULL;

    CsvDesc* c = malloc(sizeof(CsvDesc));
    *c = (CsvDesc){
        .line = calloc(CSV_LINE_MAXLEN, sizeof(char)),
        .index = vector_make(sizeof(double*), 0, NULL),
    };
    c->file = fopen(path, "r");
    if (c->file == NULL) {
        _log("ERROR: Could not open CSV file: %s", path);
        perror("Error opening file");
        exit(1);
    }

    return c;
}

void csv_index(CsvDesc* c, unsigned int* rx_vr, double* rx_real, size_t count)
{
    if (c == NULL) return;

    // Build an index based on the first line of the CSV file.
    // The Index is a mapping based on VR.
    fgets(c->line, CSV_LINE_MAXLEN, c->file);
    char* _saveptr = NULL;
    strtok_r(c->line, CSV_DELIMITER, &_saveptr);
    char*  vr;
    size_t index = 0;
    while ((vr = strtok_r(NULL, CSV_DELIMITER, &_saveptr))) {
        if (index >= count) break;

        unsigned int _vr = atoi(vr);
        double*      signal_val = NULL;
        for (size_t i = 0; i < count; i++) {
            if (rx_vr[i] == _vr) {
                signal_val = &rx_real[i];
                break;
            }
        }
        if (signal_val == NULL) {
            _log("ERROR: VR not found: %s", vr);
            exit(1);
        }
        vector_push(&c->index, &signal_val);
        index++;
    }

    // Preload the CSV Desc object with the first sample.
    csv_read_line(c);
}

bool csv_read_line(CsvDesc* c)
{
    if (c == NULL) return false;

    c->timestamp = -1;  // Set a safe time (i.e. no valid value set).
    while (c->timestamp < 0) {
        // Read a line.
        char* line = fgets(c->line, CSV_LINE_MAXLEN, c->file);
        if (line == NULL) return false;
        if (strlen(line) == 0) continue;

        // Get the timestamp.
        errno = 0;
        double ts = strtod(c->line, NULL);
        if (errno) {
            _log("Bad line, timestamp conversion failed");
            _log("%s", c->line);
            continue;
        }
        if (ts >= 0) c->timestamp = ts;
    }

    return true;
}

void csv_close(CsvDesc* c)
{
    if (c == NULL) return;

    if (c->line) free(c->line);
    c->line = NULL;
    if (c->file) fclose(c->file);
    c->file = NULL;
    vector_reset(&c->index);

    free(c);
}
