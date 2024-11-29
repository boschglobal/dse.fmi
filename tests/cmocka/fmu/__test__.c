// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>


extern int run_ascii85_tests(void);
extern int run_fmu_default_signal_tests(void);


int main()
{
    int rc = 0;
    rc |= run_ascii85_tests();
    rc |= run_fmu_default_signal_tests();
    return rc;
}
