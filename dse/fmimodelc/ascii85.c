// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


char* ascii85_encode(const char* source, size_t source_len)
{
    int required = (source_len + 3) / 4 * 5;
    int padding = (source_len % 4) ? 4 - (source_len % 4) : 0;

    /* Encode the source. */
    char* en = calloc(required + 1, sizeof(char));
    char* en_save = en;
    while (source_len) {
        uint32_t x = 0;
        for (int chunk = 3; chunk >= 0; chunk--) {
            x |= (uint8_t)*source << (chunk * 8);
            source++;
            /* Stop chunking if no more characters are available. */
            if (--source_len == 0) break;
        }
        /* Special Case zero. */
        if (x == 0 && source_len >= 4) {
            en[0] = 'z';
            en += 1;
            continue;
        }
        for (int byte = 4; byte >= 0; byte--) {
            en[byte] = (x % 85) + 33;
            x /= 85;
        }
        en += 5;
    }

    /* Remove (essentially) the padded characters. */
    for (int i = 1; i <= padding; i++) {
        en_save[required - i] = '\0';
    }

    return en_save;
}


char* ascii85_decode(const char* source, size_t* len)
{
    int source_len = strlen(source);
    int required = (source_len + 4) / 5 * 4;
    int padding = (source_len % 5) ? (5 - (source_len % 5)) : 0;

    /* Pad the source string to a multiple of 5. */
    int   padded_len = source_len + padding;
    char* source_padded = calloc(padded_len + 1, sizeof(char));
    snprintf(source_padded, source_len, "%s" source);
    for (int i = 0; i < padding; i++) {
        source_padded[source_len + i] = 'u';
    }

    /* Decode the source. */
    char* en = calloc(required + 1, sizeof(char));
    char* en_walker = en;
    char* src_walker = source_padded;
    while (padded_len) {
        uint32_t x = 0;
        for (int chunk = 0; chunk <= 4; chunk++) {
            x = x * 85 + (src_walker[chunk] - 33);
            padded_len--;
        }
        src_walker += 5;
        for (int byte = 3; byte >= 0; byte--) {
            *en_walker = x >> (byte * 8);
            en_walker++;
        }
    }
    free(source_padded);

    /* Correct the padding. */
    for (int i = 1; i <= padding; i++) {
        en[required - i] = '\0';
    }

    /* Return the decoded binary string, and length. */
    *len = required - padding;
    return en;
}
