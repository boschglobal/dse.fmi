// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/clib/util/strings.h>
#include <dse/modelc/runtime.h>
#include <dse/fmigateway/fmigateway.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


int test_fmigateway__parser_setup(void** state)
{
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));

    FmiGateway* fmi_gw = calloc(1, sizeof(FmiGateway));

    fmi_gw->model = calloc(1, sizeof(ModelGatewayDesc));

    fmu->data = (void*)fmi_gw;

    fmu->instance.resource_location =
        strdup("../../../../tests/cmocka/fmigateway/data");

    fmi_gw->settings.yaml_files = calloc(2, sizeof(char*));

    *state = fmu;
    return 0;
}

int test_fmigateway__parser_teardown(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    if (fmu) {
        dse_yaml_destroy_doc_list(fmi_gw->settings.doc_list);
        /* cleanup. */
        for (const char** _ = fmi_gw->settings.yaml_files; *_; _++) {
            free((char*)*_);
        };
        free(fmu->instance.resource_location);

        if (fmu->data) {
            FmiGatewaySession* session = fmi_gw->settings.session;
            free(fmi_gw->settings.yaml_files);
            dse_yaml_destroy_doc_list(session->model_stack_files);

            if (session->simbus) {
                free(session->simbus->name);
                free(session->simbus->yaml);
                free(session->simbus->envar);
            }
            free(session->simbus);

            if (session->transport) free(session->transport->name);
            free(session->transport);

            for (WindowsModel* model = session->w_models; model && model->name;
                model++) {
                free(model->envar);
                free(model->yaml);
                free(model->name);
            }
            free(session->w_models);

            for (FmiGatewayEnvvar* e = session->envar; e && e->name; e++) {
                free(e->vref);
                free(e->default_value);
            }
            free(session->envar);
            free(session);
            free(fmi_gw->model);
            free(fmi_gw);
        }
    }
    free(fmu);

    return 0;
}


void test_fmigateway__parser_gw_stack_default(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    fmi_gw->settings.yaml_files[0] = dse_path_cat(
        fmu->instance.resource_location, "stack_gw_parser_default.yaml");

    // Test conditions.
    WindowsModel simbus = {
        .name = (char*)"simbus",
        .step_size = 0.0005,
        .end_time = 36000.0,
        .log_level = 6,
        .timeout = 60.0,
        .exe = "simbus.exe",
        .yaml = (char*)"stack.yaml",
    };

    fmigateway_parse(fmu);

    /* Test Stack parsing. */
    assert_false(fmi_gw->settings.session->visibility.models);
    assert_false(fmi_gw->settings.session->visibility.simbus);
    assert_false(fmi_gw->settings.session->visibility.transport);
    assert_false(fmi_gw->settings.session->logging);

    /* Test Gateway parsing. */
    assert_non_null(fmi_gw->settings.doc_list);
    assert_double_equal(fmi_gw->settings.end_time, 36000.0, 0.0);
    assert_double_equal(fmi_gw->settings.step_size, 0.0005, 0.0);
    assert_int_equal(fmi_gw->settings.log_level, 6);
    assert_string_equal(fmi_gw->settings.session->log_location,
        fmu->instance.resource_location);

    /* Test script parsing. */
    assert_null(fmi_gw->settings.session->init_cmd);
    assert_null(fmi_gw->settings.session->shutdown_cmd);

    /* Test Simbus parsing. */
    assert_non_null(fmi_gw->settings.session->simbus);
    WindowsModel* w_model = fmi_gw->settings.session->simbus;
    WindowsModel* TC_sim = &simbus;
    assert_string_equal(TC_sim->name, w_model->name);
    assert_string_equal(TC_sim->exe, w_model->exe);
    assert_string_equal(TC_sim->yaml, w_model->yaml);
    assert_double_equal(TC_sim->step_size, w_model->step_size, 0.0);
    assert_double_equal(TC_sim->end_time, w_model->end_time, 0.0);
    assert_double_equal(TC_sim->timeout, w_model->timeout, 0.0);
    assert_int_equal(TC_sim->log_level, w_model->log_level);
}


void test_fmigateway__parser_gw_stack(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    fmi_gw->settings.yaml_files[0] =
        dse_path_cat(fmu->instance.resource_location, "stack_gw_parser.yaml");

    // Test conditions.
    WindowsModel simbus = {
        .name = (char*)"simbus",
        .step_size = 0.005,
        .end_time = 0.020,
        .log_level = 4,
        .timeout = 600.0,
        .exe = "simbus.exe",
        .yaml = (char*)"stack.yaml",
    };
    WindowsModel connection = {
        .name = (char*)"transport",
        .exe = "redis.exe",
    };

    fmigateway_parse(fmu);

    /* Test Stack parsing. */
    assert_true(fmi_gw->settings.session->visibility.models);
    assert_true(fmi_gw->settings.session->visibility.simbus);
    assert_true(fmi_gw->settings.session->visibility.transport);
    assert_true(fmi_gw->settings.session->logging);

    /* Test Gateway parsing. */
    assert_non_null(fmi_gw->settings.doc_list);
    assert_double_equal(fmi_gw->settings.end_time, 0.02, 0.0);
    assert_double_equal(fmi_gw->settings.step_size, 0.005, 0.0);
    assert_int_equal(fmi_gw->settings.log_level, 4);
    assert_string_equal(fmi_gw->settings.session->log_location, "./here");
    assert_string_equal(fmi_gw->settings.session->envar[0].name, "envar0");
    assert_string_equal(fmi_gw->settings.session->envar[1].name, "envar1");
    assert_string_equal(fmi_gw->settings.session->envar[2].name, "envar2");

    /* Test script parsing. */
    assert_string_equal(fmi_gw->settings.session->init_cmd, "init_cmd");
    assert_string_equal(fmi_gw->settings.session->shutdown_cmd, "shutdown_cmd");

    /* Test Redis parsing. */
    assert_non_null(fmi_gw->settings.session->transport);
    WindowsModel* w_con = fmi_gw->settings.session->transport;
    WindowsModel* TC_con = &connection;
    assert_string_equal(TC_con->name, w_con->name);
    assert_string_equal(TC_con->exe, w_con->exe);

    /* Test Simbus parsing. */
    assert_non_null(fmi_gw->settings.session->simbus);
    WindowsModel* w_model = fmi_gw->settings.session->simbus;
    WindowsModel* TC_sim = &simbus;
    assert_string_equal(TC_sim->name, w_model->name);
    assert_string_equal(TC_sim->exe, w_model->exe);
    assert_string_equal(TC_sim->yaml, w_model->yaml);
    assert_double_equal(TC_sim->step_size, w_model->step_size, 0.0);
    assert_double_equal(TC_sim->end_time, w_model->end_time, 0.0);
    assert_double_equal(TC_sim->timeout, w_model->timeout, 0.0);
    assert_int_equal(TC_sim->log_level, w_model->log_level);
}


void test_fmigateway__parser_model_stack(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    fmi_gw->settings.yaml_files[0] =
        dse_path_cat(fmu->instance.resource_location, "stack_gw_parser.yaml");

    // Test conditions.
    WindowsModel TC_models[] = {
        {
            .name = (char*)"Model_1",
            .step_size = 0.0005,
            .end_time = 20.0,
            .log_level = 1,
            .timeout = 600.0,
            .exe = "different.exe",
            .yaml = (char*)"stack.yaml model.yaml signalgroup.yaml",
            .envar = (FmiGatewayEnvvar[3]){ { .name = "NCODEC_TRACE_1",
                                                .default_value = (char*)"*" },
                { .name = "NCODEC_TRACE_2", .default_value = (char*)"0x42" } },
        },
        {
            /* Model with default values. */
            .name = (char*)"Model_2",
            .step_size = 0.0005,
            .end_time = 36000.0,
            .log_level = 6,
            .timeout = 60.0,
            .exe = "modelc.exe",
            .yaml = (char*)"stack.yaml model.yaml signalgroup.yaml",
        },
        {
            /* Model with default values. */
            .name = (char*)"Model_3,Model_4",
            .step_size = 0.0005,
            .end_time = 36000.0,
            .log_level = 6,
            .timeout = 60.0,
            .exe = "modelc.exe",
            .yaml = (char*)"stack_models_parser.yaml Model_3.yaml "
                           "signalgroup_3.yaml model_4.yaml signalgroup_4.yaml",
        },
        {
            /* Model with default values. */
            .name = (char*)"Model_5,Model_6",
            .step_size = 0.05,
            .end_time = 20.0,
            .log_level = 1,
            .timeout = 600.0,
            .exe = "different.exe",
            .yaml = (char*)"stack_models_parser.yaml Model_5.yaml "
                           "signalgroup_5.yaml Model_6.yaml signalgroup_6.yaml",
        },
    };

    fmigateway_parse(fmu);

    assert_non_null(fmi_gw->settings.session->w_models);
    for (size_t i = 0; i < ARRAY_SIZE(TC_models); i++) {
        /* Test Simbus parsing. */
        WindowsModel* model = &fmi_gw->settings.session->w_models[i];
        WindowsModel* tc_model = &TC_models[i];
        assert_string_equal(tc_model->name, model->name);
        assert_string_equal(tc_model->yaml, model->yaml);
        assert_double_equal(tc_model->step_size, model->step_size, 0.0);
        assert_double_equal(tc_model->end_time, model->end_time, 0.0);
        assert_double_equal(tc_model->timeout, model->timeout, 0.0);
        assert_int_equal(tc_model->log_level, model->log_level);
        assert_string_equal(tc_model->exe, model->exe);
        int i = 0;
        for (FmiGatewayEnvvar* env = model->envar; env && env->name; env++) {
            assert_string_equal(tc_model->envar[i].name, env->name);
            assert_string_equal(
                tc_model->envar[i].default_value, env->default_value);
            i++;
        }
    }
}


int run_fmigateway__parser_tests(void)
{
    void* s = test_fmigateway__parser_setup;
    void* t = test_fmigateway__parser_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fmigateway__parser_gw_stack_default, s, t),
        cmocka_unit_test_setup_teardown(test_fmigateway__parser_gw_stack, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmigateway__parser_model_stack, s, t)
    };

    return cmocka_run_group_tests_name("PARSER", tests, NULL, NULL);
}
