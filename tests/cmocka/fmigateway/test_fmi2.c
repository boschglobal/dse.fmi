// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <dse/testing.h>
#include <fmi2Functions.h>
#include <fmi2FunctionTypes.h>
#include <fmi2TypesPlatform.h>
#include <dse/clib/util/strings.h>
#include <dse/logger.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/ncodec/codec.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


extern uint8_t __log_level__;


typedef struct fmi2_setup {
    fmi2String             instance_name;
    fmi2Type               fmu_type;
    fmi2String             fmu_guid;
    fmi2String             fmu_resource_location;
    fmi2CallbackFunctions* functions;
    fmi2Boolean            visible;
    fmi2Boolean            logging_on;
    Controller*            controller;
    Endpoint*              endpoint;
} fmi2_setup;

void stub_setup_objects(Controller* c, Endpoint* e);
void stub_release_objects(Controller* c, Endpoint* e);


void _fmi2_unit_test_logger(fmi2ComponentEnvironment componentEnvironment,
    fmi2String instanceName, fmi2Status status, fmi2String category,
    fmi2String message, ...)
{
    UNUSED(componentEnvironment);
    UNUSED(instanceName);

    static const char* statusString[] = { "OK", "Warning", "Discard", "Error",
        "Fatal", "Pending" };

    if ((strcmp(category, "Debug") == 0) && (__log_level__ > LOG_DEBUG)) {
        return;
    }
    if ((strcmp(category, "Simbus") == 0) && (__log_level__ > LOG_SIMBUS)) {
        return;
    }
    if ((strcmp(category, "Info") == 0) && (__log_level__ > LOG_INFO)) {
        return;
    }
    if ((strcmp(category, "Notice") == 0) && (__log_level__ > LOG_NOTICE)) {
        return;
    }
    if ((strcmp(category, "Quiet") == 0) && (__log_level__ > LOG_QUIET)) {
        return;
    }

    printf("[%s:%s] %s\n", category, statusString[status], message);
    fflush(stdout);
}


int test_fmigateway__fmi2_setup(void** state)
{
    fmi2_setup* setup = calloc(1, sizeof(fmi2_setup));
    /* commit with log level in model.yaml set to 6. */
    setup->logging_on = fmi2True;
    setup->fmu_guid = "{1-22-333-4444-55555-666666-7777777}";
    setup->fmu_type = fmi2CoSimulation;
    setup->instance_name = "test_inst";
    setup->visible = fmi2True;
    setup->fmu_resource_location = "../../../../tests/cmocka/fmigateway/data";
    setup->functions = calloc(1, sizeof(fmi2CallbackFunctions));
    setup->functions->logger = _fmi2_unit_test_logger;

    setup->controller = calloc(1, sizeof(Controller));
    setup->controller->adapter = calloc(1, sizeof(Adapter));
    setup->endpoint = calloc(1, sizeof(Endpoint));
    stub_setup_objects(setup->controller, setup->endpoint);

    *state = setup;

    return 0;
}


int test_fmigateway__fmi2_teardown(void** state)
{
    fmi2_setup* setup = *state;
    free(setup->functions);

    if (setup && setup->controller) {
        if (setup->controller->adapter) free(setup->controller->adapter);
        free(setup->controller);
    }
    if (setup && setup->endpoint) free(setup->endpoint);

    stub_release_objects(setup->controller, setup->endpoint);
    free(setup);

    return 0;
}

void test_fmigateway__fmi2_instantiate(void** state)
{
    fmi2_setup* setup = *state;

    FmuInstanceData* inst = fmi2Instantiate(setup->instance_name,
        setup->fmu_type, setup->fmu_guid, setup->fmu_resource_location,
        setup->functions, setup->visible, setup->logging_on);

    /* Check instance data. */
    assert_non_null(inst->instance.guid);
    assert_string_equal(inst->instance.guid, setup->fmu_guid);
    assert_non_null(inst->instance.name);
    assert_string_equal(inst->instance.name, setup->instance_name);
    assert_non_null(inst->instance.resource_location);
    assert_string_equal(
        inst->instance.resource_location, setup->fmu_resource_location);
    assert_non_null(inst->instance.type);
    assert_int_equal(inst->instance.type, setup->fmu_type);

    /* Check gateway data. */
    assert_non_null(inst->data);

    fmi2FreeInstance(inst);
}

void test_fmigateway__fmi2_ExitInitializationMode(void** state)
{
    fmi2_setup* setup = *state;

    FmuInstanceData* inst = fmi2Instantiate(setup->instance_name,
        setup->fmu_type, setup->fmu_guid, setup->fmu_resource_location,
        setup->functions, setup->visible, setup->logging_on);

    fmi2ExitInitializationMode(inst);
    FmiGateway* fmi_gw = inst->data;

    assert_string_equal(fmi_gw->model->mi->name, "gateway");
    assert_double_equal(fmi_gw->settings.step_size, 0.0005, 0.0);
    assert_double_equal(fmi_gw->settings.end_time, 0.002, 0.0);

    assert_int_equal(inst->variables.scalar.input.used_nodes, 2);
    assert_int_equal(inst->variables.scalar.output.used_nodes, 2);
    for (SignalVector* sv = fmi_gw->model->sv; sv && sv->name; sv++) {
        if (sv->is_binary) continue;

        /* Check if SignalVector <-> Hashmap is set up correctly. */
        assert_memory_equal(
            (hashmap_get(&inst->variables.scalar.input, "1001")),
            &sv->scalar[0], sizeof(double));
        assert_memory_equal(
            (hashmap_get(&inst->variables.scalar.input, "1004")),
            &sv->scalar[2], sizeof(double));

        assert_memory_equal(
            (hashmap_get(&inst->variables.scalar.output, "1002")),
            &sv->scalar[1], sizeof(double));
        assert_memory_equal(
            (hashmap_get(&inst->variables.scalar.output, "1005")),
            &sv->scalar[3], sizeof(double));
    }

    assert_int_equal(inst->variables.binary.rx.used_nodes, 4);
    assert_int_equal(inst->variables.binary.tx.used_nodes, 4);

    fmi2FreeInstance(inst);
}

void test_fmigateway__fmi2_DOUBLE(void** state)
{
    fmi2_setup* setup = *state;

    FmuInstanceData* inst = fmi2Instantiate(setup->instance_name,
        setup->fmu_type, setup->fmu_guid, setup->fmu_resource_location,
        setup->functions, setup->visible, setup->logging_on);

    fmi2ExitInitializationMode(inst);
    FmiGateway* fmi_gw = inst->data;

    const unsigned int VR_DBL_IN[] = { 1001, 1004 };
    const unsigned int VR_DBL_OUT[] = { 1002, 1005 };
    double             VALUE[] = { 1.0, 2.0 };
    fmi2SetReal(inst, VR_DBL_IN, 2, VALUE);

    for (SignalVector* sv = fmi_gw->model->sv; sv && sv->name; sv++) {
        if (sv->is_binary) continue;
        assert_double_equal(VALUE[0], sv->scalar[0], 0.0);
        assert_double_equal(VALUE[1], sv->scalar[2], 0.0);

        sv->scalar[1] = 3.0;
        sv->scalar[3] = 4.0;
    }

    fmi2DoStep(inst, 0.0, 0.0005, 0);

    fmi2GetReal(inst, VR_DBL_OUT, 2, VALUE);
    for (SignalVector* sv = fmi_gw->model->sv; sv && sv->name; sv++) {
        if (sv->is_binary) continue;
        assert_double_equal(VALUE[0], sv->scalar[1], 0.0);
        assert_double_equal(VALUE[1], sv->scalar[3], 0.0);
    }
    fmi2FreeInstance(inst);
}

void test_fmigateway__fmi2_BINARY(void** state)
{
    fmi2_setup* setup = *state;

    FmuInstanceData* inst = fmi2Instantiate(setup->instance_name,
        setup->fmu_type, setup->fmu_guid, setup->fmu_resource_location,
        setup->functions, setup->visible, setup->logging_on);

    fmi2ExitInitializationMode(inst);
    FmiGateway* fmi_gw = inst->data;

    const unsigned int VR_BIN_IN[] = { 2, 4, 6, 8 };
    const unsigned int VR_BIN_OUT[] = { 3, 5, 7, 9 };
    const char*        VALUE[] = { "6:jpZ0`", "6:jpZ1&", "6:jpZ1B", "6:jpZ1]" };

    fmi2SetString(inst, VR_BIN_IN, 4, VALUE);

    for (SignalVector* sv = fmi_gw->model->sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;
        int len = strlen("BIN_1BIN_2BIN_3BIN_4");
        assert_memory_equal("BIN_1BIN_2BIN_3BIN_4", sv->binary[0], len);
        assert_int_equal(sv->length[0], len);
        assert_int_equal(sv->buffer_size[0], len);
    }

    fmi2DoStep(inst, 0.0, 0.0005, 0);

    for (SignalVector* sv = fmi_gw->model->sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;
        sv->length[0] = 0;

        dse_buffer_append(&sv->binary[0], &sv->length[0], &sv->buffer_size[0],
            (void*)"REMOTE_1", strlen("REMOTE_1"));
    }

    fmi2GetString(inst, VR_BIN_OUT, 4, VALUE);
    for (SignalVector* sv = fmi_gw->model->sv; sv && sv->name; sv++) {
        if (sv->is_binary == false) continue;
        assert_memory_equal(";FO;U<(1.K", VALUE[0], 8);
        assert_memory_equal(";FO;U<(1.K", VALUE[1], 8);
        assert_memory_equal(";FO;U<(1.K", VALUE[2], 8);
        assert_memory_equal(";FO;U<(1.K", VALUE[3], 8);
    }

    fmi2FreeInstance(inst);
}


int run_fmigateway__fmi2_tests(void)
{
    void* s = test_fmigateway__fmi2_setup;
    void* t = test_fmigateway__fmi2_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_instantiate, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_ExitInitializationMode, s, t),
        cmocka_unit_test_setup_teardown(test_fmigateway__fmi2_DOUBLE, s, t),
        cmocka_unit_test_setup_teardown(test_fmigateway__fmi2_BINARY, s, t),
    };

    return cmocka_run_group_tests_name("engine", tests, NULL, NULL);
}
