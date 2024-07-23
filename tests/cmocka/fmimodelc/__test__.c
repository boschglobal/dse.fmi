// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>


extern uint8_t __log_level__; /* LOG_ERROR LOG_INFO LOG_DEBUG LOG_TRACE */


extern int run_index_tests(void);
extern int run_ascii85_tests(void);


int main()
{
    __log_level__ = LOG_QUIET;  // LOG_DEBUG;//LOG_QUIET;

    int rc = 0;
    rc |= run_index_tests();
    rc |= run_ascii85_tests();
    return rc;
}
