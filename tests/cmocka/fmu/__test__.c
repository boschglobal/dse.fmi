// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>


uint8_t __log_level__; /* LOG_ERROR LOG_INFO LOG_DEBUG LOG_TRACE */


extern int run_ascii85_tests(void);
extern int run_fmu_default_signal_tests(void);
extern int run_fmu_variable_tests(void);


int main()
{
    int rc = 0;
    rc |= run_ascii85_tests();
    rc |= run_fmu_default_signal_tests();
    rc |= run_fmu_variable_tests();
    return rc;
}
