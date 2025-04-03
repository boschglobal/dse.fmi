// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef TESTS_CMOCKA_FMU_MOCK_INTERFACE_H_
#define TESTS_CMOCKA_FMU_MOCK_INTERFACE_H_

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

void __wrap_fmu_load_signal_handlers(FmuInstanceData* fmu);


#endif  // TESTS_CMOCKA_FMU_MOCK_INTERFACE_H_
