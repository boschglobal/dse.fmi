// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/fmimcl/fmimcl.h>

#ifndef TESTS_CMOCKA_FMIMCL_MOCK_MOCK_H_
#define TESTS_CMOCKA_FMIMCL_MOCK_MOCK_H_


typedef int32_t (*MockLoad)(void* instance);
typedef int32_t (*MockUnload)(void* instance);

typedef struct MockAdapterVTable {
    MockLoad   load;
    MockUnload unload;
} MockAdapterVTable;

typedef struct MockAdapterDesc {
    int32_t           expect_rc;
    int32_t           expect_step;
    MockAdapterVTable vtable;
    void*             mock_instance_data;
} MockAdapterDesc;


DLL_PRIVATE int32_t mock_mcl_load(MclDesc* m);
DLL_PRIVATE int32_t mock_mcl_init(MclDesc* m);
DLL_PRIVATE int32_t mock_mcl_step(
    MclDesc* m, double* model_time, double end_time);
DLL_PRIVATE int32_t mock_mcl_marshal_out(MclDesc* m);
DLL_PRIVATE int32_t mock_mcl_marshal_in(MclDesc* m);
DLL_PRIVATE int32_t mock_mcl_unload(MclDesc* m);

#endif  // TESTS_CMOCKA_FMIMCL_MOCK_MOCK_H_
