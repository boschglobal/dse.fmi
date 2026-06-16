// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x) ((void)x)


int test_fmigateway__fmi3_xml_setup(void** state)
{
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));

    FmiGateway* fmi_gw = calloc(1, sizeof(FmiGateway));
    fmu->data = (void*)fmi_gw;

    fmu->instance.resource_location =
        strdup("../../../../tests/cmocka/fmigateway/fmi3/data/fmi3/resources");
    fmu->instance.version = 3;

    hashmap_init(&fmu->variables.string.input);
    hashmap_init(&fmu->variables.scalar.input);

    *state = fmu;
    return 0;
}

int test_fmigateway__fmi3_xml_default_setup(void** state)
{
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));

    FmiGateway* fmi_gw = calloc(1, sizeof(FmiGateway));
    fmu->data = (void*)fmi_gw;

    fmu->instance.resource_location = strdup(
        "../../../../tests/cmocka/fmigateway/fmi3/data/fmi3_default/resources");
    fmu->instance.version = 3;

    hashmap_init(&fmu->variables.string.input);
    hashmap_init(&fmu->variables.scalar.input);

    *state = fmu;
    return 0;
}

int test_fmigateway__fmi3_xml_teardown(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    for (size_t i = 0; i < vector_len(&fmi_gw->settings.runtime.cmds); i++) {
        free(*(char**)vector_at(&fmi_gw->settings.runtime.cmds, i, NULL));
    }
    vector_reset(&fmi_gw->settings.runtime.cmds);

    free((char*)fmi_gw->settings.runtime.log_location);

    for (FmiGatewayParameter* e = fmi_gw->settings.scripts.envar; e && e->name;
        e++) {
        free(e->vref);
        free((char*)e->name);
        free(e->default_value);
    }
    free(fmi_gw->settings.scripts.envar);

    hashmap_destroy(&fmu->variables.string.input);
    hashmap_destroy(&fmu->variables.scalar.input);

    free(fmu->instance.resource_location);
    free((char*)fmi_gw->settings.model_name);
    free(fmi_gw);
    free(fmu);
    return 0;
}


void test_fmigateway__fmi3_xml_default_experiment(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    fmigateway_parse_xml(fmu);

    assert_double_equal(fmi_gw->settings.end_time, 3600.0, 0.0);
    assert_double_equal(fmi_gw->settings.step_size, 0.0005, 0.0);
}


void test_fmigateway__fmi3_xml_runtime(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    fmigateway_parse_xml(fmu);

    assert_int_equal(fmi_gw->settings.runtime.type, FMIGATEWAY_RUNTIME_SIMER);
    assert_int_equal(vector_len(&fmi_gw->settings.runtime.cmds), 2);
    assert_string_equal(
        *(char**)vector_at(&fmi_gw->settings.runtime.cmds, 0, NULL), "cmd0");
    assert_string_equal(
        *(char**)vector_at(&fmi_gw->settings.runtime.cmds, 1, NULL), "cmd1");
    assert_string_equal(fmi_gw->settings.runtime.log_location, "./logs");
    assert_int_equal(fmi_gw->settings.log_level, 4);
}


void test_fmigateway__fmi3_xml_simer_parameters(void** state)
{
    FmuInstanceData* fmu = *state;

    fmigateway_parse_xml(fmu);

    /* valueReference="0" -> Simer_Command (String) */
    assert_string_equal(
        (char*)hashmap_get(&fmu->variables.string.input, "0"), "");
    /* valueReference="1" -> Simer_Command_Selector (Float64) */
    assert_double_equal(
        *(double*)hashmap_get(&fmu->variables.scalar.input, "1"), 0.0, 0.0);
}


void test_fmigateway__fmi3_xml_runtime_default(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    /* No Annotations block: _parse_runtime returns early.
       - type stays LEGACY (0)
       - cmds vector is empty
       - log_location stays NULL */
    fmigateway_parse_xml(fmu);

    assert_int_equal(fmi_gw->settings.runtime.type, FMIGATEWAY_RUNTIME_LEGACY);
    assert_int_equal(vector_len(&fmi_gw->settings.runtime.cmds), 0);
    assert_non_null(fmi_gw->settings.runtime.log_location);
    assert_string_equal(
        fmi_gw->settings.runtime.log_location, fmu->instance.resource_location);
}


void test_fmigateway__fmi3_xml_simer_parameters_default(void** state)
{
    FmuInstanceData* fmu = *state;

    /* No simer.parameter variables -> keys absent from both inputs. */
    fmigateway_parse_xml(fmu);

    assert_null(hashmap_get(&fmu->variables.string.input, "0"));
    assert_null(hashmap_get(&fmu->variables.scalar.input, "1"));
}


void test_fmigateway__fmi3_xml_script_envar(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    fmigateway_parse_xml(fmu);

    /* envar0 (String, vref=2), envar1 (Float64, vref=3), envar2 (Float64,
     * vref=4) */
    FmiGatewayParameter* envars = fmi_gw->settings.scripts.envar;
    assert_non_null(envars);

    /* All three should be present (order: String then Float64). */
    assert_string_equal(envars[0].name, "envar0");
    assert_string_equal(envars[0].type, "String");
    assert_string_equal(envars[0].default_value, "");

    assert_string_equal(envars[1].name, "envar1");
    assert_string_equal(envars[1].type, "Real");
    assert_string_equal(envars[1].default_value, "0.000000");

    assert_string_equal(envars[2].name, "envar2");
    assert_string_equal(envars[2].type, "Real");
    assert_string_equal(envars[2].default_value, "1.000000");

    /* Sentinel at end. */
    assert_null(envars[3].name);

    /* String envar registered in string input hashmap. */
    assert_string_equal(
        (char*)hashmap_get(&fmu->variables.string.input, "2"), "");  // NOLINT
    /* Float64 envars registered in scalar input hashmap. */
    assert_double_equal(
        *(double*)hashmap_get(&fmu->variables.scalar.input, "3"), 0.0, 0.0);
    assert_double_equal(
        *(double*)hashmap_get(&fmu->variables.scalar.input, "4"), 1.0, 0.0);
}


int run_fmigateway__fmi3_xml_parsing_tests(void)
{
    void* s = test_fmigateway__fmi3_xml_setup;
    void* t = test_fmigateway__fmi3_xml_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_xml_default_experiment, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_xml_runtime, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_xml_simer_parameters, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_xml_runtime_default,
            test_fmigateway__fmi3_xml_default_setup, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_xml_simer_parameters_default,
            test_fmigateway__fmi3_xml_default_setup, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi3_xml_script_envar, s, t),
    };

    return cmocka_run_group_tests_name("FMI3_XML_PARSING", tests, NULL, NULL);
}
