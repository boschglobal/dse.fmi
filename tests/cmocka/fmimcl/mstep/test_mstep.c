// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/mocks/simmock.h>
#include <dse/modelc/mcl.h>

#define real_3_tx    0
#define real_1_rx    1
#define integer_3_tx 2
#define integer_2_rx 3
#define integer_1_rx 4
#define bool_1_tx    6
#define bool_2_rx    7
#define real_A_tx    8
#define real_B_rx    10
#define real_C_rx    11
#define real_D_rx    12


int test_setup(void** state)
{
    const char* inst_names[] = {
        "fmu_inst",
    };
    char* argv[] = {
        (char*)"test_fmimcl",
        (char*)"--name=fmu_inst",
        (char*)"--logger=5",  // 1=debug, 5=QUIET (commit with 5!)
        (char*)"data/fmu_mstep.yaml",
    };
    SimMock* mock = simmock_alloc(inst_names, ARRAY_SIZE(inst_names));
    simmock_configure(mock, argv, ARRAY_SIZE(argv), ARRAY_SIZE(inst_names));
    simmock_load(mock);
    simmock_load_model_check(mock->model, true, true, true);
    simmock_setup(mock, "signal", NULL);

    /* Return the mock. */
    *state = mock;
    return 0;
}


static int test_teardown(void** state)
{
    SimMock* mock = *state;

    simmock_exit(mock, true);
    simmock_free(mock);

    return 0;
}

typedef struct TC_MSTEP {
    double init_value[10];
    int    vr_input[10];
    size_t steps;
    double sim_stepsize;
    double expected_value[10];
    int    vr_output[10];
} TC_MSTEP;

void test_mstep(void** state)
{
    SimMock*   mock = *state;
    ModelMock* model = &mock->model[0];
    assert_non_null(model);

    /* Check the mock object was constructed. */
    assert_non_null(model->sv_signal);
    assert_string_equal(model->sv_signal->name, "signal");
    assert_int_equal(model->sv_signal->count, 13);
    assert_non_null(model->sv_signal->scalar);

    TC_MSTEP tc[] = {
        { // FMU runs 1 time
            .init_value = { 0.0, 0.0, 1.0, 1 },
            .vr_input = { real_3_tx, integer_3_tx, real_A_tx, bool_1_tx },
            .expected_value = { 1.0, 1.0, 11.0, 101.0, 101.0, 1 },
            .vr_output = { real_1_rx, integer_2_rx, real_B_rx, real_C_rx,
                real_D_rx, bool_2_rx },
            .sim_stepsize = 0.0001,
            .steps = 1 },
        { // FMU runs additional 1 time
            .init_value = { 0.0, 0.0 },
            .vr_input = { real_3_tx, integer_3_tx },
            .expected_value = { 2.0, 2.0 },
            .vr_output = { real_1_rx, integer_2_rx },
            .sim_stepsize = 0.00001,
            .steps = 10 },
        { // FMU runs additional 10 times
            .init_value = { 0.0, 0.0 },
            .vr_input = { real_3_tx, integer_3_tx },
            .expected_value = { 12.0, 12.0 },
            .vr_output = { real_1_rx, integer_2_rx },
            .sim_stepsize = 0.001,
            .steps = 1 },
    };

    // Check the test cases.
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        for (size_t x = 0; x < ARRAY_SIZE(tc[i].init_value); x++) {
            if (tc[i].init_value[x] == 0.0) continue;
            mock->sv_signal->scalar[tc[i].vr_input[x]] = tc[i].init_value[x];
        }

        mock->step_size = tc[i].sim_stepsize;

        for (size_t j = 1; j <= tc[i].steps; j++) {
            assert_int_equal(simmock_step(mock, true), 0);
        }

        for (size_t y = 0; y < ARRAY_SIZE(tc[i].expected_value); y++) {
            SignalCheck checks[] = {
                { .index = tc[i].vr_output[y],
                    .value = tc[i].expected_value[y] },
            };
            simmock_signal_check(
                mock, "fmu_inst", checks, ARRAY_SIZE(checks), NULL);
        }
    }
}


int run_mstep_tests(void)
{
    void* s = test_setup;
    void* t = test_teardown;

    /* The linker will remove mcl functions as they are not directly called so
    this dummy call makes those functions available. */
    mcl_load(NULL);


    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_mstep, s, t),
    };

    return cmocka_run_group_tests_name("MSTEP", tests, NULL, NULL);
}
