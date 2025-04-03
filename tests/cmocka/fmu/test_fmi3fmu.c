// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/fmu/fmu.h>
#include <dse/clib/collections/hashlist.h>
#include <dse/clib/util/strings.h>
#include <fmi3Functions.h>
#include <fmi3FunctionTypes.h>
#include <fmi3PlatformTypes.h>
#include <mock_interface.h>


#define UNUSED(x) ((void)x)


uint8_t __log_level__; /* LOG_ERROR LOG_INFO LOG_DEBUG LOG_TRACE */


typedef struct {
    fmi3String                     instance_name;
    fmi3String                     token;
    fmi3String                     resource_path;
    fmi3Boolean                    visible;
    fmi3Boolean                    logging_on;
    fmi3Boolean                    event;
    fmi3Boolean                    early_return_allowed;
    fmi3ValueReference*            required_intermediate_variables;
    size_t                         n_required_intermediate_variables;
    fmi3InstanceEnvironment        instance_environment;
    fmi3LogMessageCallback         log;
    fmi3IntermediateUpdateCallback intermediate_update;
} fmi3fmu_test_setup;


void _fmi3_unit_test_logger(fmi3InstanceEnvironment instanceEnvironment,
    fmi3Status status, fmi3String category, fmi3String message)
{
    UNUSED(instanceEnvironment);
    function_called();
    check_expected(status);
    check_expected(category);
    check_expected(message);
}


static void _expected_log(const fmi3Status expected_status,
    const char* expected_category, const char* expected_msg)
{
    expect_function_call(_fmi3_unit_test_logger);
    expect_value(_fmi3_unit_test_logger, status, expected_status);
    expect_string(_fmi3_unit_test_logger, category, expected_category);
    expect_string(_fmi3_unit_test_logger, message, expected_msg);
}

static void _expect_init_logs(
    const TEST_SCENARIO scenario, const int32_t status)
{
    will_return(fmu_create, scenario);
    will_return(fmu_destroy, status);
    _expected_log(fmi3OK, "Debug", "FMU Model instantiated");
    _expected_log(
        fmi3OK, "Debug", "Resource location: data/test_fmu/resources");
    _expected_log(fmi3OK, "Debug", "Build indexes...");
    expect_function_call(__wrap_fmu_load_signal_handlers);
    expect_function_call(_test_fmu_setup);
    expect_function_call(fmu_create);
}

static void _expect_free_instance_logs()
{
    expect_function_call(fmu_destroy);
    expect_function_call(_test_fmu_remove);
    _expected_log(fmi3OK, "Debug", "Release var table");
    _expected_log(fmi3OK, "Debug", "Destroy the index");
    _expected_log(fmi3OK, "Debug", "Release FMI instance resources");
}

int test_fmi3fmu_setup(void** state)
{
    fmi3fmu_test_setup* setup = calloc(1, sizeof(fmi3fmu_test_setup));
    /* Commit with log level in model.yaml set to 6. */
    setup->instance_name = "test_inst";
    setup->token = "{1-22-333-4444-55555-666666-7777777}";
    setup->resource_path = "data/test_fmu/resources";
    setup->visible = false;
    setup->logging_on = true;
    setup->event = false;
    setup->early_return_allowed = false;
    setup->required_intermediate_variables = NULL;
    setup->n_required_intermediate_variables = 0;
    setup->instance_environment = NULL;
    setup->intermediate_update = NULL;
    setup->log = _fmi3_unit_test_logger;

    *state = setup;
    return 0;
}

int test_fmi3fmu_teardown(void** state)
{
    fmi3fmu_test_setup* setup = *state;
    free(setup);
    return 0;
}

void test_fmi3Instantiate_returned_NULL(void** state)
{
    fmi3fmu_test_setup* setup = *state;
    const int           target_version = 3;

    _expect_init_logs(RETURN_NULL, fmi3OK);
    _expected_log(fmi3OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi3Instance inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->token, setup->resource_path, setup->visible, setup->logging_on,
        setup->event, setup->early_return_allowed,
        setup->required_intermediate_variables,
        setup->n_required_intermediate_variables, setup->instance_environment,
        setup->log, setup->intermediate_update);

    FmuInstanceData* fmu_inst = inst;
    assert_ptr_equal(captured_fmu_instance, inst);
    assert_string_equal(fmu_inst->instance.name, setup->instance_name);
    assert_string_equal(fmu_inst->instance.guid, setup->token);
    assert_string_equal(
        fmu_inst->instance.resource_location, setup->resource_path);
    assert_int_equal(fmu_inst->instance.log_enabled, setup->logging_on);
    assert_int_equal(fmu_inst->instance.version, target_version);
    fmi3FreeInstance(inst);
}

void test_fmi3Instantiate_returned_new_instance(void** state)
{
    fmi3fmu_test_setup* setup = *state;

    _expect_init_logs(RETURN_NEW_INSTANCE, fmi3OK);
    _expected_log(fmi3OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi3Instance inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->token, setup->resource_path, setup->visible, setup->logging_on,
        setup->event, setup->early_return_allowed,
        setup->required_intermediate_variables,
        setup->n_required_intermediate_variables, setup->instance_environment,
        setup->log, setup->intermediate_update);

    assert_ptr_not_equal(captured_fmu_instance, inst);

    fmi3FreeInstance(inst);
}

void test_fmi3Instantiate_returned_the_same_instance(void** state)
{
    fmi3fmu_test_setup* setup = *state;

    _expect_init_logs(RETURN_THE_SAME_INSTANCE, fmi3OK);
    _expected_log(fmi3OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi3Instance inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->token, setup->resource_path, setup->visible, setup->logging_on,
        setup->event, setup->early_return_allowed,
        setup->required_intermediate_variables,
        setup->n_required_intermediate_variables, setup->instance_environment,
        setup->log, setup->intermediate_update);

    assert_ptr_equal(captured_fmu_instance, inst);

    fmi3FreeInstance(inst);
}

void test_fmi3Instantiate_returned_errno(void** state)
{
    fmi3fmu_test_setup* setup = *state;

    char expected_errno_msg[100];
    snprintf(expected_errno_msg, sizeof(expected_errno_msg),
        "The FMU was not created correctly! (errro = %d)", EACCES);

    _expect_init_logs(SET_ERRNO, fmi3OK);
    _expected_log(fmi3Error, "Error", expected_errno_msg);
    _expected_log(fmi3OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi3Instance inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->token, setup->resource_path, setup->visible, setup->logging_on,
        setup->event, setup->early_return_allowed,
        setup->required_intermediate_variables,
        setup->n_required_intermediate_variables, setup->instance_environment,
        setup->log, setup->intermediate_update);

    assert_ptr_equal(captured_fmu_instance, inst);
    assert_int_equal(errno, EACCES);
    errno = 0;  // reset errno
    fmi3FreeInstance(inst);
}

void test_fmi3Instantiate_uri_scheme(void** state)
{
    fmi3fmu_test_setup* setup = *state;
    char                uri[100];
    snprintf(uri, sizeof(uri), "%s%s", FILE_URI_SCHEME, setup->resource_path);

    _expect_init_logs(RETURN_NULL, fmi3OK);
    _expected_log(fmi3OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi3Instance inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->token, uri, setup->visible, setup->logging_on, setup->event,
        setup->early_return_allowed, setup->required_intermediate_variables,
        setup->n_required_intermediate_variables, setup->instance_environment,
        setup->log, setup->intermediate_update);

    FmuInstanceData* fmu_inst = inst;
    assert_string_equal(
        fmu_inst->instance.resource_location, setup->resource_path);
    assert_ptr_equal(captured_fmu_instance, inst);
    fmi3FreeInstance(inst);
}

void test_fmi3Instantiate_short_scheme(void** state)
{
    fmi3fmu_test_setup* setup = *state;
    char                uri[100];
    snprintf(
        uri, sizeof(uri), "%s%s", FILE_URI_SHORT_SCHEME, setup->resource_path);

    _expect_init_logs(RETURN_NULL, fmi3OK);
    _expected_log(fmi3OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi3Instance inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->token, uri, setup->visible, setup->logging_on, setup->event,
        setup->early_return_allowed, setup->required_intermediate_variables,
        setup->n_required_intermediate_variables, setup->instance_environment,
        setup->log, setup->intermediate_update);

    FmuInstanceData* fmu_inst = inst;
    assert_string_equal(
        fmu_inst->instance.resource_location, setup->resource_path);
    assert_ptr_equal(captured_fmu_instance, inst);
    fmi3FreeInstance(inst);
}

void test_fmi3FreeInstance_returned_error(void** state)
{
    fmi3fmu_test_setup* setup = *state;
    const int32_t       error_value = -1;

    _expect_init_logs(RETURN_NULL, error_value);
    _expected_log(fmi3OK, "Debug", "FMU Var Table is not configured");
    expect_function_call(fmu_destroy);
    _expected_log(fmi3Error, "Error",
        "Error while releasing the allocated specialised model.");
    expect_function_call(_test_fmu_remove);
    _expected_log(fmi3OK, "Debug", "Release var table");
    _expected_log(fmi3OK, "Debug", "Destroy the index");
    _expected_log(fmi3OK, "Debug", "Release FMI instance resources");


    fmi3Instance inst = fmi3InstantiateCoSimulation(setup->instance_name,
        setup->token, setup->resource_path, setup->visible, setup->logging_on,
        setup->event, setup->early_return_allowed,
        setup->required_intermediate_variables,
        setup->n_required_intermediate_variables, setup->instance_environment,
        setup->log, setup->intermediate_update);

    assert_ptr_equal(captured_fmu_instance, inst);
    fmi3FreeInstance(inst);
}

int run_fmu3fmi_tests(void)
{
    void*                   s = test_fmi3fmu_setup;
    void*                   t = test_fmi3fmu_teardown;
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fmi3Instantiate_returned_NULL, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi3Instantiate_returned_new_instance, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi3Instantiate_returned_the_same_instance, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi3Instantiate_returned_errno, s, t),
        cmocka_unit_test_setup_teardown(test_fmi3Instantiate_uri_scheme, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi3Instantiate_short_scheme, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi3FreeInstance_returned_error, s, t),
    };

    return cmocka_run_group_tests_name("test_fmi3fmu", tests, NULL, NULL);
}

int main()
{
    int rc = 0;
    rc |= run_fmu3fmi_tests();
    return rc;
}
