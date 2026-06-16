// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <string.h>
#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x) ((void)x)


int test_fmigateway__fmi2_xml_setup(void** state)
{
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));

    FmiGateway* fmi_gw = calloc(1, sizeof(FmiGateway));
    fmu->data = (void*)fmi_gw;

    fmu->instance.resource_location =
        strdup("../../../../tests/cmocka/fmigateway/fmi2/data/fmi2/resources");

    hashmap_init(&fmu->variables.string.input);
    hashmap_init(&fmu->variables.scalar.input);

    *state = fmu;
    return 0;
}

int test_fmigateway__fmi2_xml_default_setup(void** state)
{
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));

    FmiGateway* fmi_gw = calloc(1, sizeof(FmiGateway));
    fmu->data = (void*)fmi_gw;

    fmu->instance.resource_location = strdup(
        "../../../../tests/cmocka/fmigateway/fmi2/data/fmi2_default/resources");

    hashmap_init(&fmu->variables.string.input);
    hashmap_init(&fmu->variables.scalar.input);

    *state = fmu;
    return 0;
}

int test_fmigateway__fmi2_xml_teardown(void** state)
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


void test_fmigateway__fmi2_xml_default_experiment(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    fmigateway_parse_xml(fmu);

    assert_double_equal(fmi_gw->settings.end_time, 3600.0, 0.0);
    assert_double_equal(fmi_gw->settings.step_size, 0.0005, 0.0);
}


void test_fmigateway__fmi2_xml_runtime(void** state)
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


void test_fmigateway__fmi2_xml_simer_parameters(void** state)
{
    FmuInstanceData* fmu = *state;

    fmigateway_parse_xml(fmu);

    /* valueReference="0" -> Simer_Command (String) */
    assert_string_equal(
        (char*)hashmap_get(&fmu->variables.string.input, "0"), "");
    /* valueReference="1" -> Simer_Command_Selector (Real) */
    assert_double_equal(
        *(double*)hashmap_get(&fmu->variables.scalar.input, "1"), 0.0, 0.0);
}


void test_fmigateway__fmi2_xml_runtime_default(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    /* No VendorAnnotations: _parse_runtime returns early.
       - type stays LEGACY (0)
       - cmds vector is empty
       - log_location is untouched (resource_location fallback via
       fmigateway_parse) */
    fmigateway_parse_xml(fmu);

    assert_int_equal(fmi_gw->settings.runtime.type, FMIGATEWAY_RUNTIME_LEGACY);
    assert_int_equal(vector_len(&fmi_gw->settings.runtime.cmds), 0);
    assert_non_null(fmi_gw->settings.runtime.log_location);
    assert_string_equal(
        fmi_gw->settings.runtime.log_location, fmu->instance.resource_location);
}


void test_fmigateway__fmi2_xml_simer_parameters_default(void** state)
{
    FmuInstanceData* fmu = *state;

    /* No simer.parameter ScalarVariables -> keys absent from both inputs. */
    fmigateway_parse_xml(fmu);

    assert_null(hashmap_get(&fmu->variables.string.input, "0"));  // NOLINT
    assert_null(hashmap_get(&fmu->variables.scalar.input, "1"));
}

void test_fmigateway__fmi2_xml_script_envar_without_session(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    /* session is NULL -> envar list should be discarded without crashing. */
    fmigateway_parse_xml(fmu);

    assert_null(fmi_gw->settings.session);
}


int run_fmigateway__fmi2_xml_parsing_tests(void)
{
    void* s = test_fmigateway__fmi2_xml_setup;
    void* t = test_fmigateway__fmi2_xml_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_xml_default_experiment, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_xml_runtime, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_xml_runtime_default,
            test_fmigateway__fmi2_xml_default_setup, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_xml_simer_parameters, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_xml_simer_parameters_default,
            test_fmigateway__fmi2_xml_default_setup, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__fmi2_xml_script_envar_without_session, s, t),
    };

    return cmocka_run_group_tests_name("FMI2_XML_PARSING", tests, NULL, NULL);
}
