// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <dse/testing.h>
#include <fmi3Functions.h>
#include <dse/clib/util/strings.h>
#include <dse/logger.h>
#include <dse/modelc/controller/model_private.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


extern uint8_t __log_level__;


typedef struct fmi3_setup {
    fmi3String              instance_name;
    fmi3String              instantiation_token;
    fmi3String              resource_location;
    fmi3Boolean             visible;
    fmi3Boolean             logging_on;
    fmi3InstanceEnvironment instance_env;
    Controller*             controller;
    Endpoint*               endpoint;
} fmi3_setup;

void stub_setup_objects(Controller* c, Endpoint* e);
void stub_release_objects(Controller* c, Endpoint* e);


static void _fmi3_unit_test_logger(fmi3InstanceEnvironment instanceEnvironment,
    fmi3Status status, fmi3String category, fmi3String message)
{
    UNUSED(instanceEnvironment);
    UNUSED(status);

    if ((strcmp(category, "Debug") == 0) && (__log_level__ > LOG_DEBUG)) {
        return;
    }
    if ((strcmp(category, "Info") == 0) && (__log_level__ > LOG_INFO)) {
        return;
    }
    if ((strcmp(category, "Notice") == 0) && (__log_level__ > LOG_NOTICE)) {
        return;
    }

    printf("[%s] %s\n", category, message);
    fflush(stdout);
}


static int _fmi3_simer_setup(void** state)
{
    fmi3_setup* setup = calloc(1, sizeof(fmi3_setup));
    setup->logging_on = fmi3True;
    setup->instantiation_token = "{880ba27a-938d-4e61-9817-2ae18767232a}";
    setup->instance_name = "test_inst";
    setup->visible = fmi3True;
    setup->resource_location =
        "../../../../tests/cmocka/fmigateway/fmi3/data/fmi3/resources";
    setup->instance_env = NULL;

    setup->controller = calloc(1, sizeof(Controller));
    setup->controller->adapter = calloc(1, sizeof(Adapter));
    setup->endpoint = calloc(1, sizeof(Endpoint));
    stub_setup_objects(setup->controller, setup->endpoint);

    *state = setup;
    return 0;
}


static int _fmi3_legacy_setup(void** state)
{
    fmi3_setup* setup = calloc(1, sizeof(fmi3_setup));
    setup->logging_on = fmi3True;
    setup->instantiation_token = "{880ba27a-938d-4e61-9817-2ae18767232a}";
    setup->instance_name = "test_inst";
    setup->visible = fmi3True;
    setup->resource_location =
        "../../../../tests/cmocka/fmigateway/fmi3/data/fmi3_legacy/resources";
    setup->instance_env = NULL;

    setup->controller = calloc(1, sizeof(Controller));
    setup->controller->adapter = calloc(1, sizeof(Adapter));
    setup->endpoint = calloc(1, sizeof(Endpoint));
    stub_setup_objects(setup->controller, setup->endpoint);

    *state = setup;
    return 0;
}


static int _fmi3_teardown(void** state)
{
    fmi3_setup* setup = *state;

    if (setup->controller) {
        if (setup->controller->adapter) free(setup->controller->adapter);
        free(setup->controller);
    }
    if (setup->endpoint) free(setup->endpoint);

    stub_release_objects(setup->controller, setup->endpoint);
    free(setup);
    return 0;
}


void test_fmigateway__fmi3_instantiate(void** state)
{
    fmi3_setup* setup = *state;

    FmuInstanceData* inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->instantiation_token, setup->resource_location, setup->visible,
        setup->logging_on, fmi3False, fmi3False, NULL, 0, setup->instance_env,
        _fmi3_unit_test_logger, NULL);

    assert_non_null(inst);

    /* Check instance data. */
    assert_non_null(inst->instance.guid);
    assert_string_equal(inst->instance.guid, setup->instantiation_token);
    assert_non_null(inst->instance.name);
    assert_string_equal(inst->instance.name, setup->instance_name);
    assert_non_null(inst->instance.resource_location);
    assert_string_equal(
        inst->instance.resource_location, setup->resource_location);
    assert_int_equal(inst->instance.version, 3);

    /* Check gateway data. */
    assert_non_null(inst->data);

    fmi3FreeInstance(inst);
}


void test_fmigateway__fmi3_runtime_simer(void** state)
{
    fmi3_setup* setup = *state;

    FmuInstanceData* inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->instantiation_token, setup->resource_location, setup->visible,
        setup->logging_on, fmi3False, fmi3False, NULL, 0, setup->instance_env,
        _fmi3_unit_test_logger, NULL);

    assert_non_null(inst);
    FmiGateway* fmi_gw = inst->data;

    /* modelDescription.xml parsed: simer runtime, two cmds, explicit log
     * location. */
    assert_int_equal(fmi_gw->settings.runtime.type, FMIGATEWAY_RUNTIME_SIMER);
    assert_int_equal(vector_len(&fmi_gw->settings.runtime.cmds), 2);
    assert_string_equal(
        *(char**)vector_at(&fmi_gw->settings.runtime.cmds, 0, NULL), "cmd0");
    assert_string_equal(
        *(char**)vector_at(&fmi_gw->settings.runtime.cmds, 1, NULL), "cmd1");
    assert_string_equal(fmi_gw->settings.runtime.log_location, "./logs");
    assert_string_equal(fmi_gw->settings.model_name, "fmugw");
    assert_double_equal(fmi_gw->settings.end_time, 3600.0, 0.0);
    assert_double_equal(fmi_gw->settings.step_size, 0.0005, 0.0);
    /* loglevel annotation "4" -> atoi -> LOG_NOTICE. */
    assert_int_equal(fmi_gw->settings.log_level, LOG_NOTICE);

    /* yaml_files: simer layout uses sim/model/<name>/data/ prefix. */
    assert_non_null(strstr(
        fmi_gw->settings.yaml_files[0], "sim/model/fmugw/data/model.yaml"));
    assert_non_null(strstr(
        fmi_gw->settings.yaml_files[1], "sim/model/fmugw/data/fmu.yaml"));
    assert_non_null(
        strstr(fmi_gw->settings.yaml_files[2], "sim/data/simulation.yaml"));

    /* Simer runtime parameters registered in input hashmaps during XML parse.
     */
    assert_non_null(hashmap_get(&inst->variables.string.input, "0"));
    assert_non_null(hashmap_get(&inst->variables.scalar.input, "1"));

    /* Script environment variables populated from script.parameter annotations.
     */
    FmiGatewayParameter* envar = fmi_gw->settings.scripts.envar;
    assert_non_null(envar);
    assert_string_equal(envar[0].name, "envar0");
    assert_string_equal(envar[0].type, "String");
    assert_string_equal(envar[0].vref, "2");
    assert_string_equal(envar[1].name, "envar1");
    assert_string_equal(envar[1].type, "Real");
    assert_string_equal(envar[1].vref, "3");
    assert_string_equal(envar[2].name, "envar2");
    assert_string_equal(envar[2].type, "Real");
    assert_string_equal(envar[2].vref, "4");
    assert_null(envar[3].name); /* NTL terminator */

    fmi3FreeInstance(inst);
}


void test_fmigateway__fmi3_runtime_legacy(void** state)
{
    fmi3_setup* setup = *state;

    FmuInstanceData* inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->instantiation_token, setup->resource_location, setup->visible,
        setup->logging_on, fmi3False, fmi3False, NULL, 0, setup->instance_env,
        _fmi3_unit_test_logger, NULL);

    assert_non_null(inst);
    FmiGateway* fmi_gw = inst->data;

    /* modelDescription.xml parsed: no runtime annotation -> legacy, no cmds. */
    assert_int_equal(fmi_gw->settings.runtime.type, FMIGATEWAY_RUNTIME_LEGACY);
    assert_int_equal(vector_len(&fmi_gw->settings.runtime.cmds), 0);
    assert_string_equal(
        fmi_gw->settings.runtime.log_location, setup->resource_location);
    assert_string_equal(fmi_gw->settings.model_name, "gateway");
    assert_double_equal(fmi_gw->settings.end_time, 3600.0, 0.0);
    assert_double_equal(fmi_gw->settings.step_size, 0.0005, 0.0);
    /* No loglevel annotation: stays at LOG_ERROR default. */
    assert_int_equal(fmi_gw->settings.log_level, LOG_ERROR);

    /* yaml_files: legacy layout uses resource_location root (no sim/ prefix).
     */
    assert_null(strstr(fmi_gw->settings.yaml_files[0], "sim/"));
    assert_non_null(strstr(fmi_gw->settings.yaml_files[0], "model.yaml"));
    assert_non_null(strstr(fmi_gw->settings.yaml_files[1], "fmu.yaml"));
    assert_non_null(strstr(fmi_gw->settings.yaml_files[2], "stack.yaml"));

    /* No simer runtime parameters: simer.parameter variables absent. */
    assert_null(hashmap_get(&inst->variables.string.input, "0"));  // NOLINT
    assert_null(hashmap_get(&inst->variables.scalar.input, "1"));

    /* Script environment variables from script.parameter annotations. */
    FmiGatewayParameter* envar = fmi_gw->settings.scripts.envar;
    assert_non_null(envar);
    assert_string_equal(envar[0].name, "envar0");
    assert_string_equal(envar[0].type, "String");
    assert_string_equal(envar[0].vref, "2");
    assert_string_equal(envar[1].name, "envar1");
    assert_string_equal(envar[1].type, "Real");
    assert_string_equal(envar[1].vref, "3");
    assert_null(
        envar[2].name); /* NTL terminator: only 2 envars in legacy XML */

    fmi3FreeInstance(inst);
}


int run_fmigateway__fmi3_tests(void)
{
    void* ss = _fmi3_simer_setup;
    void* ls = _fmi3_legacy_setup;
    void* t = _fmi3_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_instantiate, ss, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_runtime_simer, ss, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_runtime_legacy, ls, t),
    };

    return cmocka_run_group_tests_name("fmi3_engine", tests, NULL, NULL);
}
