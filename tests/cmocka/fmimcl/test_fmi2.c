// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>
#include <dse/fmimcl/adapter/fmi2mcl.h>

#include <fmimcl/mock/mock.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef struct Fmi2Mock {
    FmuModel          model;
    ModelInstanceSpec model_instance;
} Fmi2Mock;

int test_fmi2_setup(void** state)
{
    /* Construct the mock object .*/
    Fmi2Mock* mock = malloc(sizeof(Fmi2Mock));
    *mock = (Fmi2Mock) {
        .model = {
            .mcl = {
                .adapter = "fmi2",
                .version = "2.0.3",
            },
            .cosim = true,
            .guid = "",
            .resource_dir = "",
            .path = "../../../../dse/build/_out/fmimcl/examples/lib/fmi2fmu.so",
            .handle = "",
        },
        .model_instance = {
            .name = (char*)"mock_inst",
        },
    };
    mock->model.mcl.model.mi = &mock->model_instance;

    /* Return the mock. */
    *state = mock;
    return 0;
}


int test_fmi2_teardown(void** state)
{
    Fmi2Mock* mock = *state;
    if (mock) {
        free(mock);
    }
    return 0;
}


void test_fmi2__create(void** state)
{
    Fmi2Mock* mock = *state;
    FmuModel* fmu_model = &mock->model;

    assert_null(fmu_model->adapter);
    assert_null(fmu_model->mcl.vtable.load);
    assert_null(fmu_model->mcl.vtable.init);
    assert_null(fmu_model->mcl.vtable.step);
    assert_null(fmu_model->mcl.vtable.marshal_out);
    assert_null(fmu_model->mcl.vtable.marshal_in);
    assert_null(fmu_model->mcl.vtable.unload);

    fmi2mcl_create(fmu_model);

    assert_non_null(fmu_model->adapter);
    assert_non_null(fmu_model->mcl.vtable.load);
    assert_non_null(fmu_model->mcl.vtable.init);
    assert_non_null(fmu_model->mcl.vtable.step);
    assert_non_null(fmu_model->mcl.vtable.marshal_out);
    assert_non_null(fmu_model->mcl.vtable.marshal_in);
    assert_non_null(fmu_model->mcl.vtable.unload);

    free(fmu_model->adapter);
}

void test_fmi2__interface(void** state)
{
    Fmi2Mock* mock = *state;
    FmuModel* fmu_model = &mock->model;
    int       rc;

    fmi2mcl_create(fmu_model);

    rc = fmu_model->mcl.vtable.load((void*)fmu_model);
    assert_int_equal(rc, 0);

    Fmi2Adapter* adapter = fmu_model->adapter;
    assert_non_null(adapter->vtable.instantiate);
    assert_non_null(adapter->vtable.setup_experiment);
    assert_non_null(adapter->vtable.enter_initialization);
    assert_non_null(adapter->vtable.exit_initialization);
    assert_non_null(adapter->vtable.get_real);
    assert_non_null(adapter->vtable.get_integer);
    assert_non_null(adapter->vtable.get_boolean);
    assert_non_null(adapter->vtable.get_string);
    assert_non_null(adapter->vtable.set_real);
    assert_non_null(adapter->vtable.set_integer);
    assert_non_null(adapter->vtable.set_boolean);
    assert_non_null(adapter->vtable.set_string);
    assert_non_null(adapter->vtable.terminate);
    assert_non_null(adapter->vtable.free_instance);

    free(fmu_model->adapter);
}

void test_fmi2__lifecycle(void** state)
{
    Fmi2Mock* mock = *state;
    FmuModel* fmu_model = &mock->model;
    int       rc;

    fmi2mcl_create(fmu_model);

    rc = fmu_model->mcl.vtable.load((void*)fmu_model);
    assert_int_equal(rc, 0);

    rc = fmu_model->mcl.vtable.init((void*)fmu_model);
    assert_int_equal(rc, 0);

    rc = fmu_model->mcl.vtable.unload((void*)fmu_model);
    assert_int_equal(rc, 0);
}

typedef struct FMI2_TC {
    MarshalGroup mg[3];
    int          ref[2];
    union {
        double _double[2];
        int    _int[2];
    } check;
    union {
        double _double[2];
        int    _int[2];
    } init;
} FMI2_TC;

void test_fmi2__api(void** state)
{
    Fmi2Mock* mock = *state;
    FmuModel* fmu_model = &mock->model;
    int       rc;

    FMI2_TC tc[] = {
        {
            .mg = {
                {
                    .name = (char*)"double_tx", // input
                    .kind = MARSHAL_KIND_PRIMITIVE,
                    .dir = MARSHAL_DIRECTION_TXONLY,
                    .type = MARSHAL_TYPE_DOUBLE,
                    .count = 1,
                    .target = {
                        .ref = calloc(1, sizeof(uint32_t)),
                        ._double = calloc(1, sizeof(double))
                    },
                    .source = {
                        .offset = 0,
                    },
                },
                {
                    .name = (char*)"double_rx", // output
                    .kind = MARSHAL_KIND_PRIMITIVE,
                    .dir = MARSHAL_DIRECTION_RXONLY,
                    .type = MARSHAL_TYPE_DOUBLE,
                    .count = 1,
                    .target = {
                        .ref = calloc(1, sizeof(uint32_t)),
                        ._double = calloc(1, sizeof(double))
                    },
                    .source = {
                        .offset = 1,
                    },
                },
                {NULL}
            },
            .ref = {0, 1},
            .init._double = {1.0, 0.0},
            .check._double = {1.0, 2.0}
        },
        {
            .mg = {
                {
                    .name = (char*)"integer_tx", // input
                    .kind = MARSHAL_KIND_PRIMITIVE,
                    .dir = MARSHAL_DIRECTION_TXONLY,
                    .type = MARSHAL_TYPE_INT32,
                    .count = 1,
                    .target = {
                        .ref = calloc(1, sizeof(uint32_t)),
                        ._int32 = calloc(1, sizeof(uint32_t))
                    },
                    .source.offset = 0,
                },
                {
                    .name = (char*)"integer_rx", // output
                    .kind = MARSHAL_KIND_PRIMITIVE,
                    .dir = MARSHAL_DIRECTION_RXONLY,
                    .type = MARSHAL_TYPE_INT32,
                    .count = 1,
                    .target = {
                        .ref = calloc(1, sizeof(uint32_t)),
                        ._int32 = calloc(1, sizeof(uint32_t))
                    },
                    .source.offset = 1,
                },
                {NULL}
            },
            .ref = {2, 3},
            .init._int = {1, 0},
            .check._int = {1, 2}
        },
        {
            .mg = {
                {
                    .name = (char*)"boolean_tx", // input
                    .kind = MARSHAL_KIND_PRIMITIVE,
                    .dir = MARSHAL_DIRECTION_TXONLY,
                    .type = MARSHAL_TYPE_BOOL,
                    .count = 1,
                    .target = {
                        .ref = calloc(1, sizeof(uint32_t)),
                        ._int32 = calloc(1, sizeof(uint32_t))
                    },
                    .source.offset = 0,
                },
                {
                    .name = (char*)"boolean_rx", // output
                    .kind = MARSHAL_KIND_PRIMITIVE,
                    .dir = MARSHAL_DIRECTION_RXONLY,
                    .type = MARSHAL_TYPE_BOOL,
                    .count = 1,
                    .target = {
                        .ref = calloc(1, sizeof(uint32_t)),
                        ._int32 = calloc(1, sizeof(uint32_t))
                    },
                    .source.offset = 1,
                },
                {NULL}
            },
            .ref = {6, 7},
            .init._int = {1, 0},
            .check._int = {1, 1}
        }
    };

    // Check the test cases.
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        log_trace("Testcase: %d", i);
        log_trace("  name: [0]%s [1]%s", tc[i].mg[0].name, tc[i].mg[1].name);
        tc[i].mg[0].target.ref[0] = tc[i].ref[0];
        tc[i].mg[1].target.ref[0] = tc[i].ref[1];
        double* ptr_d = calloc(2, sizeof(double));

        switch (tc[i].mg[0].type) {
        case MARSHAL_TYPE_UINT64:
        case MARSHAL_TYPE_INT64:
        case MARSHAL_TYPE_DOUBLE:
        case MARSHAL_TYPE_BYTE8: {
            tc[i].mg[0].source.scalar = ptr_d;
            tc[i].mg[1].source.scalar = ptr_d;
            ptr_d[0] = tc[i].init._double[0];
            ptr_d[1] = tc[i].init._double[1];
            break;
        }
        case MARSHAL_TYPE_UINT32:
        case MARSHAL_TYPE_INT32:
        case MARSHAL_TYPE_FLOAT:
        case MARSHAL_TYPE_BYTE4:
        case MARSHAL_TYPE_BOOL: {
            tc[i].mg[0].source.scalar = ptr_d;
            tc[i].mg[1].source.scalar = ptr_d;
            ptr_d[0] = (double)tc[i].init._int[0];
            ptr_d[1] = (double)tc[i].init._int[1];
            break;
        }
        case MARSHAL_TYPE_UINT16:
        case MARSHAL_TYPE_INT16:
        case MARSHAL_TYPE_BYTE2:
            break;
        case MARSHAL_TYPE_UINT8:
        case MARSHAL_TYPE_INT8:
        case MARSHAL_TYPE_BYTE1:
            break;
        default:
            break;
        };

        fmu_model->data.mg_table = (MarshalGroup*)&(tc[i].mg);
        fmi2mcl_create(fmu_model);

        rc = fmu_model->mcl.vtable.load((void*)fmu_model);
        assert_int_equal(rc, 0);

        rc = fmu_model->mcl.vtable.init((void*)fmu_model);
        assert_int_equal(rc, 0);

        rc = fmu_model->mcl.vtable.marshal_out((void*)fmu_model);
        assert_int_equal(rc, 0);

        double model_time = 0.0;
        rc = fmu_model->mcl.vtable.step((void*)fmu_model, &model_time, 1.0);
        assert_int_equal(rc, 0);
        assert_double_equal(model_time, 1.0, 0.0);

        rc = fmu_model->mcl.vtable.marshal_in((void*)fmu_model);
        assert_int_equal(rc, 0);

        switch (tc[i].mg[0].type) {
        case MARSHAL_TYPE_UINT64:
        case MARSHAL_TYPE_INT64:
        case MARSHAL_TYPE_DOUBLE:
        case MARSHAL_TYPE_BYTE8: {
            log_trace("    mg[0]: target=%f check=%f",
                tc[i].mg[0].target._double[0], tc[i].check._double[0]);
            log_trace("    mg[1]: target=%f check=%f",
                tc[i].mg[1].target._double[0], tc[i].check._double[1]);
            assert_double_equal(
                tc[i].mg[0].target._double[0], tc[i].check._double[0], 0.0);
            assert_double_equal(
                tc[i].mg[1].target._double[0], tc[i].check._double[1], 0.0);
            break;
        }
        case MARSHAL_TYPE_UINT32:
        case MARSHAL_TYPE_INT32:
        case MARSHAL_TYPE_FLOAT:
        case MARSHAL_TYPE_BYTE4:
        case MARSHAL_TYPE_BOOL: {
            assert_int_equal(tc[i].mg[0].target._int32[0], tc[i].check._int[0]);
            assert_int_equal(tc[i].mg[1].target._int32[0], tc[i].check._int[1]);
            break;
        }
        case MARSHAL_TYPE_UINT16:
        case MARSHAL_TYPE_INT16:
        case MARSHAL_TYPE_BYTE2:
            break;
        case MARSHAL_TYPE_UINT8:
        case MARSHAL_TYPE_INT8:
        case MARSHAL_TYPE_BYTE1:
            break;
        default:
            break;
        };

        rc = fmu_model->mcl.vtable.unload((void*)fmu_model);
        assert_int_equal(rc, 0);

        /* Cleanup. */
        free(tc[i].mg[0].target.ref);
        free(tc[i].mg[1].target.ref);
        free(ptr_d);
        /* free union. */
        free(tc[i].mg[0].target._double);
        free(tc[i].mg[1].target._double);
    }
}


int run_fmi2_tests(void)
{
    void* s = test_fmi2_setup;
    void* t = test_fmi2_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_fmi2__create, s, t),
        cmocka_unit_test_setup_teardown(test_fmi2__interface, s, t),
        cmocka_unit_test_setup_teardown(test_fmi2__lifecycle, s, t),
        cmocka_unit_test_setup_teardown(test_fmi2__api, s, t),
    };

    return cmocka_run_group_tests_name("fmi2", tests, NULL, NULL);
}


// create

// run through mcl api, consider supported use cases.


// efficient array interface of FMI.

// typedef fmi2Status fmi2GetRealTYPE(fmi2Component c,
//     const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]);


// SignalVector <--> FmuModel.double[count] <-marshal-> FMU

// FmuModel->signals --> sorted list ON variable_type AND variable_dir -> groups


// double[0] group[0][0] -> TX, double
// double[1] group[0][1]
// double[2] group[0][2]

// double[3] group[1][0] -> TX, int
// double[4] group[1][1]
// double[5] group[1][2]
// ...


// typedef struct fmi_value_group {
//     MarshalType
//     MarshalDir

//     size_t offset; // offset to signal_vector on FMU Model.
//     size_t count; // 3
//     uint32_t* value_ref;
//     union {
//         double* value_double;  // calloc(3, sizeof(double))
//         uint32_t* value_uint32;
//     }
// } ;
