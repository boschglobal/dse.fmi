// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimodelc/fmimodelc.h>


#define UNUSED(x)          ((void)x)
#define ARRAY_SIZE(x)      (sizeof(x) / sizeof(x[0]))

#define EXAMPLE_PATH       "../../../../dse/build/_out/examples/fmimodelc/fmi2"

#define EXAMPLE_MODEL_PATH EXAMPLE_PATH "/resources/sim"


int test_index_setup(void** state)
{
    RuntimeModelDesc* m = calloc(1, sizeof(RuntimeModelDesc));
    *m = (RuntimeModelDesc){
        .runtime = {
             .runtime_model = "network_fmu",
             .sim_path = strdup(EXAMPLE_MODEL_PATH),
             .simulation_yaml = "data/simulation.yaml",
             .end_time = 3600,
             .log_level = 5,
         },
    };
    m->model.sim = calloc(1, sizeof(SimulationSpec));
    *state = m;
    return 0;
}


int test_index_teardown(void** state)
{
    RuntimeModelDesc* m = *state;
    if (m) {
        free(m->model.sim);
        free(m->runtime.sim_path);
        free(m);
    }
    return 0;
}


void test_index__scalar(void** state)
{
    RuntimeModelDesc* m = *state;
    FmuInstanceData   fmu = {
          .data = m,
    };
    hashmap_init(&fmu.variables.scalar.input);
    hashmap_init(&fmu.variables.scalar.output);
    m = model_runtime_create(m);

    /* Locate the SimBus scalar SV. */
    SimbusVectorIndex index =
        simbus_vector_lookup(m->model.sim, "scalar", "counter");
    assert_non_null(index.sbv);

    /* Index the scalar signals. */
    fmimodelc_index_scalar_signals(&fmu);
    assert_int_equal(fmu.variables.scalar.input.used_nodes, 0);
    assert_int_equal(fmu.variables.scalar.output.used_nodes, 1);
    double* sig_counter = NULL;
    sig_counter = hashmap_get(&fmu.variables.scalar.output, "1");
    assert_non_null(sig_counter);
    assert_string_equal("counter", index.sbv->signal[index.vi]);
    assert_ptr_equal(sig_counter, &index.sbv->scalar[index.vi]);

    /* Cleanup. */
    model_runtime_destroy(m);
    hashmap_destroy(&fmu.variables.scalar.input);
    hashmap_destroy(&fmu.variables.scalar.output);
}


void test_index__binary(void** state)
{
    RuntimeModelDesc* m = *state;
    FmuInstanceData   fmu = {
          .data = m,
    };
    hashmap_init(&fmu.variables.binary.rx);
    hashmap_init(&fmu.variables.binary.tx);
    m = model_runtime_create(m);

    /* Locate the SimBus network SV. */
    SimbusVectorIndex index = {};
    index = simbus_vector_lookup(m->model.sim, "network", "can");
    assert_non_null(index.sbv);

    /* Index the network signals. */
    fmimodelc_index_binary_signals(&fmu);
    assert_int_equal(fmu.variables.binary.rx.used_nodes, 4);
    assert_int_equal(fmu.variables.binary.tx.used_nodes, 4);

    /* Check the RX index. */
    const char* rx_vref[] = {
        "2",
        "4",
        "6",
        "8",
    };
    for (size_t i = 0; i < ARRAY_SIZE(rx_vref); i++) {
        // Each index should have a ModelSignalIndex with the same content.
        SimbusVectorIndex* var = NULL;
        var = hashmap_get(&fmu.variables.binary.rx, rx_vref[i]);
        assert_non_null(var);
        // TODO: Should this check happen, currently memory is only calloc'ed.
        // assert_memory_equal(var, &index, sizeof(SimbusVectorIndex));
    }

    /* Check the TX index. */
    const char* tx_vref[] = {
        "3",
        "5",
        "7",
        "9",
    };
    for (size_t i = 0; i < ARRAY_SIZE(tx_vref); i++) {
        // Each index should have a ModelSignalIndex with the same content.
        SimbusVectorIndex* var = NULL;
        var = hashmap_get(&fmu.variables.binary.tx, tx_vref[i]);
        assert_non_null(var);
        // TODO: Should this check happen, currently memory is only calloc'ed.
        // assert_memory_equal(var, &index, sizeof(SimbusVectorIndex));
    }

    /* Cleanup. */
    model_runtime_destroy(m);
    hashmap_destroy(&fmu.variables.binary.rx);
    hashmap_destroy(&fmu.variables.binary.tx);
}


int run_index_tests(void)
{
    void* s = test_index_setup;
    void* t = test_index_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_index__scalar, s, t),
        cmocka_unit_test_setup_teardown(test_index__binary, s, t),
    };

    return cmocka_run_group_tests_name("INDEX", tests, NULL, NULL);
}
