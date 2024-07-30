// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <time.h>

#define UNUSED(x) ((void)x)


int clock_gettime(clockid_t clk_id, struct timespec* tp)
{
    UNUSED(clk_id);
    UNUSED(tp);
    return 0;
}
