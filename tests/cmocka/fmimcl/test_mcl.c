// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>
#include <fmimcl/mock/mock.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


typedef struct FmimclMock {
    FmuModel          model;
    ModelInstanceSpec model_instance;
    ModelDesc         model_desc;
    SimulationSpec    simulation_spec;
} FmimclMock;


int test_mcl_setup(void** state)
{
    /* Load yaml files. */
    const char* yaml_files[] = {
        "data/mcl_mock.yaml",
        "data/mcl.yaml",
        NULL,
    };
    YamlDocList* doc_list = NULL;
    for (const char** _ = yaml_files; *_ != NULL; _++) {
        doc_list = dse_yaml_load_file(*_, doc_list);
    }
    /* Construct the mock object .*/
    FmimclMock* mock = malloc(sizeof(FmimclMock));
    *mock = (FmimclMock) {
        .model = {
            .mcl = {
                .adapter = "mock",
                .version = "1.0.0",
            },
        },
        .model_instance = {
            .name = (char*)"mock_inst",
            .yaml_doc_list = doc_list,
            .model_definition.doc = dse_yaml_load_single_doc("data/mcl.yaml")
        },
        .model_desc = {
            .sv = calloc(3, sizeof(SignalVector))
        },
        .simulation_spec = {
            .step_size = 0.0001
        }
    };
    mock->model.mcl.model.mi = &mock->model_instance;
    mock->model_desc.mi = &mock->model_instance;
    mock->model.mcl.model.sim = &mock->simulation_spec;
    mock->model_desc.sim = &mock->simulation_spec;
    for (size_t i = 0; i < 2; i++) {
        mock->model_desc.sv[i].scalar = calloc(3, sizeof(double));
        mock->model_desc.sv[i].signal = calloc(3, sizeof(char*));
    }


    /* Return the mock. */
    *state = mock;
    return 0;
}


int test_mcl_teardown(void** state)
{
    FmimclMock* mock = *state;
    if (mock) {
        dse_yaml_destroy_doc_list(mock->model_instance.yaml_doc_list);
        dse_yaml_destroy_node(mock->model_instance.model_definition.doc);
        for (SignalVector* sv = mock->model_desc.sv; sv && sv->scalar; sv++) {
            free(sv->scalar);
            free(sv->signal);
        }
        free(mock->model_desc.sv);
        free(mock);
    }
    return 0;
}


/* Maybe relloc that to test_fmimcl? */
void test_mcl__create_adapter(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    assert_null(fmu_model->adapter);
    assert_null(fmu_model->mcl.vtable.load);
    assert_null(fmu_model->mcl.vtable.init);
    assert_null(fmu_model->mcl.vtable.step);
    assert_null(fmu_model->mcl.vtable.marshal_out);
    assert_null(fmu_model->mcl.vtable.marshal_in);
    assert_null(fmu_model->mcl.vtable.unload);

    // Call create to get an initialised (and extended) FmuModel object.
    int rc = fmimcl_adapter_create(fmu_model);
    assert_int_equal(rc, 0);
    assert_non_null(fmu_model->adapter);
    assert_ptr_equal(fmu_model->mcl.vtable.load, mock_mcl_load);
    assert_ptr_equal(fmu_model->mcl.vtable.init, mock_mcl_init);
    assert_ptr_equal(fmu_model->mcl.vtable.step, mock_mcl_step);
    assert_ptr_equal(fmu_model->mcl.vtable.marshal_out, mock_mcl_marshal_out);
    assert_ptr_equal(fmu_model->mcl.vtable.marshal_in, mock_mcl_marshal_in);
    assert_ptr_equal(fmu_model->mcl.vtable.unload, mock_mcl_unload);

    // Release the adapter object directly.
    free(fmu_model->adapter);
}


void test_mcl__create_no_adapter(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    const char* tc[2][2] = {
        [0][0] = "missing",
        [0][1] = "1.0.0",
        [1][0] = "mock",
        [1][1] = "42.24",
    };
    uint32_t tc_count = 0;

    for (uint32_t i = 0; i < ARRAY_SIZE(tc); i++) {
        // Modify the mock.
        mock->model.mcl.adapter = tc[i][0];
        mock->model.mcl.version = tc[i][1];

        // Call create to get an initialised (and extended) FmuModel object.
        int rc = fmimcl_adapter_create(fmu_model);
        assert_int_equal(rc, -EINVAL);

        assert_null(fmu_model->adapter);
        assert_null(fmu_model->mcl.vtable.load);
        assert_null(fmu_model->mcl.vtable.init);
        assert_null(fmu_model->mcl.vtable.step);
        assert_null(fmu_model->mcl.vtable.marshal_out);
        assert_null(fmu_model->mcl.vtable.marshal_in);
        assert_null(fmu_model->mcl.vtable.unload);

        tc_count++;
    }

    assert_int_equal(tc_count, ARRAY_SIZE(tc));
}


typedef struct API_TC {
    double step_size;
    double model_time_correction;
    double model_time;
    double end_time;
    int    step_result[10];
    size_t simulation_steps;
} API_TC;

void test_mcl__api(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;
    int32_t     rc;

    API_TC tc[] = {
        {
            .model_time_correction = 0.0,
            .step_result = {44},
            .model_time = 0.0,
            .end_time = 0.1,
            .simulation_steps = 1
        },
        {
            .model_time_correction = 0.0,
            .step_result = {53},
            .step_size = 0.01,
            .model_time = 0.0,
            .end_time = 0.1,
            .simulation_steps = 1
        },
        {
            .model_time_correction = 0.0,
            .step_result = {0, 44},
            .step_size = 0.2,
            .model_time = 0.0,
            .end_time = 0.1,
            .simulation_steps = 2
        },
    };

    // Check the test cases.
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        fmimcl_adapter_create(fmu_model);
        assert_non_null(fmu_model->adapter);
        MockAdapterDesc* adapter = fmu_model->adapter;
        adapter->expect_rc = 40;
        fmu_model->mcl.step_size = tc[i].step_size;
        fmu_model->mcl.model_time_correction = tc[i].model_time_correction;
        fmu_model->mcl.model_time = tc[i].model_time;

        // Indirectly call the mock adapter, via MCL.
        rc = mcl_load((void*)fmu_model);
        assert_int_equal(rc, 41);

        rc = mcl_init((void*)fmu_model);
        assert_int_equal(rc, 42);

        rc = mcl_marshal_out((void*)fmu_model);
        assert_int_equal(rc, 43);

        for (size_t j = 0; j < tc[i].simulation_steps; j++) {
            rc = mcl_step((void*)fmu_model, tc[i].end_time);
            assert_int_equal(rc, tc[i].step_result[j]);
            tc[i].end_time += tc[i].end_time;
        }
        rc = mcl_marshal_in((void*)fmu_model);
        assert_int_equal(rc, 45);

        rc = mcl_unload((void*)fmu_model);
        assert_int_equal(rc, 437);
    }
}


void test_mcl__api_partial(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Modify the mock.
    mock->model.mcl.adapter = "missing";
    mock->model.mcl.model_time = 0.0;

    // Call create to get an initialised (and extended) FmuModel object.
    int rc = fmimcl_adapter_create(fmu_model);
    assert_int_equal(rc, -EINVAL);

    // Check the object is not initialised.
    assert_null(fmu_model->adapter);
    assert_null(fmu_model->mcl.vtable.load);
    assert_null(fmu_model->mcl.vtable.init);
    assert_null(fmu_model->mcl.vtable.step);
    assert_null(fmu_model->mcl.vtable.marshal_out);
    assert_null(fmu_model->mcl.vtable.marshal_in);
    assert_null(fmu_model->mcl.vtable.unload);

    // Call the MCL to check safe operation.
    assert_int_equal(-EINVAL, mcl_load((void*)fmu_model));
    assert_int_equal(-EINVAL, mcl_init((void*)fmu_model));
    assert_int_equal(-EINVAL, mcl_step((void*)fmu_model, 0.1));
    assert_int_equal(-EINVAL, mcl_marshal_out((void*)fmu_model));
    assert_int_equal(-EINVAL, mcl_marshal_in((void*)fmu_model));
    assert_int_equal(-EINVAL, mcl_unload((void*)fmu_model));
}


typedef struct MCLC_TC {
    struct {
        const char* name;
        const char* signal[10];
        size_t      count;
        double      scalar[10];
        bool        is_binary;
    } sv[10];
    struct {
        int         count;
        const char* name;
        double      result[10];
    } expect_msm[10];
    size_t steps;
    double sim_stepsize;
} MCLC_TC;

/* Test create here or later in mstep? */
void test_mcl__marshalling(void** state)
{
    FmimclMock* mock = *state;
    ModelDesc   model_desc = mock->model_desc;
    int32_t     rc = 0;

    MCLC_TC tc[] = {
        {
            .sv = {
                {
                    .name = "double_sv",
                    .signal = {"real_1_tx", "real_3_rx", "real_2_rx"},
                    .scalar = {1.0, 2.0, 3.0},
                    .count = 3,
                }
            },
            .expect_msm = {
                {
                    .count = 3,
                    .name = "double_sv",
                    .result = {2.0, 3.0, 4.0}
                }
            },
            .steps = 1,
            .sim_stepsize = 0.0001
        },
        {
            // Test case: FMU stepsize is 10x smaller than Simulation
            .sv = {
                {
                    .name = "double_sv",
                    .signal = {"real_1_tx", "real_3_rx", "real_2_rx"},
                    .scalar = {1.0, 2.0, 3.0},
                    .count = 3,
                }
            },
            .expect_msm = {
                {
                    .count = 3,
                    .name = "double_sv",
                    .result = {11.0, 12.0, 13.0}
                }
            },
            .steps = 1,
            .sim_stepsize = 0.001  // model stepsize is 0.0001
        },
        {
            // Test case: FMU stepsize is 10x bigger than Simulation
            .sv = {
                {
                    .name = "double_sv",
                    .signal = {"real_1_tx", "real_3_rx", "real_2_rx"},
                    .scalar = {1.0, 2.0, 3.0},
                    .count = 3,
                }
            },
            .expect_msm = {
                {
                    .count = 3,
                    .name = "double_sv",
                    .result = {2.0, 3.0, 4.0}
                }
            },
            .steps = 10,
            .sim_stepsize = 0.00001  // model stepsize is 0.0001
        },
        {
            .sv = {
                {
                    .name = "double_sv",
                    .signal = {"real_1_tx", "real_3_rx", "real_2_rx"},
                    .scalar = {1.0, 2.0, 3.0},
                    .count = 3,
                }
            },
            .expect_msm = {
                {
                    .count = 3,
                    .name = "double_sv",
                    .result = {11.0, 12.0, 13.0}
                }
            },
            .steps = 10,
            .sim_stepsize = 0.0001
        },
        {
            .sv = {
                {
                    .name = "double_sv",
                    .signal = {"real_1_tx", "real_3_rx", "real_2_rx"},
                    .scalar = {1.0, 2.0, 3.0},
                    .count = 3,
                },
                {
                    .name = "integer_sv",
                    .signal = {"integer_1_tx", "integer_3_rx", "integer_2_tx"},
                    .scalar = {4.0, 5.0, 6.0},
                    .count = 3,
                }
            },
            .expect_msm = {
                {
                    .count = 3,
                    .name = "double_sv",
                    .result = {2.0, 3.0, 4.0}
                },
                {
                    .count = 3,
                    .name = "integer_sv",
                    .result = {7.0, 5.0, 6.0}
                }
            },
            .steps = 1,
            .sim_stepsize = 0.0001
        },
    };

    // Check the test cases.
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        size_t count = 0;
        for (SignalVector* sv = model_desc.sv; sv && sv->scalar; sv++) {
            sv->name = tc[i].sv[count].name;
            sv->is_binary = false;
            sv->count = tc[i].sv[count].count;
            for (size_t j = 0; j < sv->count; j++) {
                sv->signal[j] = tc[i].sv[count].signal[j];
                sv->scalar[j] = tc[i].sv[count].scalar[j];
            }
            count++;
        }
        model_desc.sim->step_size = tc[i].sim_stepsize;

        FmuModel*        fm = (FmuModel*)mcl_create(&model_desc);
        MockAdapterDesc* adapter = fm->adapter;
        adapter->expect_rc = 40;

        rc = mcl_load((void*)fm);
        assert_int_equal(rc, 41);

        size_t msm_count = 0;
        for (MarshalSignalMap* msm = fm->mcl.msm; msm && msm->name; msm++) {
            assert_int_equal(msm->count, tc[i].expect_msm[msm_count].count);
            assert_string_equal(msm->name, tc[i].expect_msm[msm_count].name);
            assert_non_null(msm->signal.scalar);
            assert_ptr_equal(
                *(msm->signal.scalar), model_desc.sv[msm_count].scalar);
            assert_non_null(msm->source.scalar);
            assert_ptr_equal(*(msm->source.scalar), *(fm->mcl.source.scalar));

            double* src_scalar = *(msm->source.scalar);
            for (size_t j = 0; j > msm->count; j++) {
                assert_double_equal(src_scalar[msm->source.index[j]], 0.0, 0.0);
            }

            msm_count++;
        }

        rc = mcl_init((void*)fm);
        assert_int_equal(rc, 42);
        rc = mcl_marshal_out((void*)fm);
        assert_int_equal(rc, 43);

        for (MarshalSignalMap* msm = fm->mcl.msm; msm && msm->name; msm++) {
            double* src_scalar = *(msm->source.scalar);
            double* sig_scalar = *(msm->signal.scalar);
            for (size_t j = 0; j < msm->count; j++) {
                assert_double_equal(src_scalar[msm->source.index[j]],
                    sig_scalar[msm->signal.index[j]], 0.0);
            }
        }

        for (size_t j = 1; j <= tc[i].steps; j++) {
            rc = mcl_step((void*)fm, tc[i].sim_stepsize * j);
        }

        msm_count = 0;
        for (MarshalSignalMap* msm = fm->mcl.msm; msm && msm->name; msm++) {
            double* src_scalar = *(msm->source.scalar);
            for (size_t j = 0; j < msm->count; j++) {
                assert_double_equal(src_scalar[msm->source.index[j]],
                    tc[i].expect_msm[msm_count].result[j], 0.0);
            }
            msm_count++;
        }

        rc = mcl_marshal_in((void*)fm);
        assert_int_equal(rc, 0);

        for (MarshalSignalMap* msm = fm->mcl.msm; msm && msm->name; msm++) {
            double* src_scalar = *(msm->source.scalar);
            double* sig_scalar = *(msm->signal.scalar);
            for (size_t j = 0; j < msm->count; j++) {
                assert_double_equal(src_scalar[msm->source.index[j]],
                    sig_scalar[msm->signal.index[j]], 0.0);
            }
        }

        rc = mcl_unload((void*)fm);
        assert_int_equal(rc, 437);

        mcl_destroy((void*)fm);
        free(fm);
    }
}


int run_mcl_tests(void)
{
    void* s = test_mcl_setup;
    void* t = test_mcl_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_mcl__marshalling, s, t),
        cmocka_unit_test_setup_teardown(test_mcl__create_adapter, s, t),
        cmocka_unit_test_setup_teardown(test_mcl__create_no_adapter, s, t),
        cmocka_unit_test_setup_teardown(test_mcl__api, s, t),
        cmocka_unit_test_setup_teardown(test_mcl__api_partial, s, t),
    };

    return cmocka_run_group_tests_name("MCL", tests, NULL, NULL);
}
