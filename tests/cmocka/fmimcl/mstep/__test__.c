// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>


extern int run_mstep_tests(void);


int main()
{
    int rc = 0;
    rc |= run_mstep_tests();
    return rc;
}
