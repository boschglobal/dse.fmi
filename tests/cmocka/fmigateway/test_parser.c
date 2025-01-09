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
    fmi_gw->settings.yaml_files[0] =
        dse_path_cat(fmu->instance.resource_location, "model_parser.yaml");

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
            free(fmi_gw->settings.yaml_files);
            free(fmi_gw->settings.session->w_models);
            free(fmi_gw->settings.session);
            free(fmi_gw->model);
            free(fmi_gw);
        }
    }
    free(fmu);

    return 0;
}

void test_fmigateway__parser(void** state)
{
    FmuInstanceData* fmu = *state;
    FmiGateway*      fmi_gw = fmu->data;

    // Test conditions.
    WindowsModel pi[] = {
        {
            .name = "Model_1",
            .step_size = 0.1,
            .end_time = 0.2,
            .log_level = 6,
            .timeout = 60.0,
            .path = "foo",
            .file = "bar",
            .yaml = "foo;bar",
            .show_process = true,
        },
        {
            .name = "Model_2",
            .step_size = 0.15,
            .end_time = 0.25,
            .log_level = 5,
            .timeout = 61.0,
            .path = "foo_2",
            .file = "bar_2",
            .yaml = "foo_2;bar_2",
            .show_process = false,
        },
    };

    assert_null(fmi_gw->settings.end_time);
    assert_null(fmi_gw->settings.log_level);
    assert_null(fmi_gw->settings.session);

    fmigateway_parse(fmu);

    assert_non_null(fmi_gw->settings.doc_list);
    assert_double_equal(fmi_gw->settings.end_time, 0.02, 0.0);
    assert_double_equal(fmi_gw->settings.step_size, 0.005, 0.0);
    assert_int_equal(fmi_gw->settings.log_level, 6);
    assert_string_equal(fmi_gw->settings.log_location, "foo/bar");
    assert_non_null(fmi_gw->settings.session);
    assert_string_equal(fmi_gw->settings.session->init.path, "foo");
    assert_string_equal(fmi_gw->settings.session->init.file, "bar");
    assert_string_equal(
        fmi_gw->settings.session->shutdown.path, "foo_shutdown");
    assert_string_equal(
        fmi_gw->settings.session->shutdown.file, "bar_shutdown");

    assert_non_null(fmi_gw->settings.session->w_models);
    for (size_t i = 0; i < ARRAY_SIZE(pi); i++) {
        WindowsModel* w_model = &fmi_gw->settings.session->w_models[i];
        WindowsModel* TC_model = &pi[i];
        assert_string_equal(TC_model->name, w_model->name);
        assert_string_equal(TC_model->path, w_model->path);
        assert_string_equal(TC_model->file, w_model->file);
        assert_string_equal(TC_model->yaml, w_model->yaml);
        assert_double_equal(TC_model->step_size, w_model->step_size, 0.0);
        assert_double_equal(TC_model->end_time, w_model->end_time, 0.0);
        assert_double_equal(TC_model->timeout, w_model->timeout, 0.0);
        assert_int_equal(TC_model->log_level, w_model->log_level);
        assert_int_equal(TC_model->show_process, w_model->show_process);
    }
}


int run_fmigateway__parser_tests(void)
{
    void* s = test_fmigateway__parser_setup;
    void* t = test_fmigateway__parser_teardown;

    const struct CMUnitTest tests[] = { cmocka_unit_test_setup_teardown(
        test_fmigateway__parser, s, t) };

    return cmocka_run_group_tests_name("PARSER", tests, NULL, NULL);
}
