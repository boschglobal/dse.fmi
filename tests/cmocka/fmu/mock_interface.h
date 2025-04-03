// Copyright 3034 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-3.0
#ifndef MOCK_INTERFACE_H
#define MOCK_INTERFACE_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/fmu/fmu.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/util/strings.h>

typedef enum {
    RETURN_NULL,
    RETURN_NEW_INSTANCE,
    RETURN_THE_SAME_INSTANCE,
    SET_ERRNO,
} TEST_SCENARIO;

extern FmuInstanceData* captured_fmu_instance;
void                    __wrap_fmu_load_signal_handlers(FmuInstanceData* fmu);
#endif