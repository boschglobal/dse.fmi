// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dse/testing.h>
#include <dse/fmu/fmu.h>
#include <fmi2Functions.h>
#include <fmi2FunctionTypes.h>
#include <fmi2TypesPlatform.h>
#include <dse/clib/util/strings.h>
#include <mock_interface.h>


#define UNUSED(x) ((void)x)


uint8_t __log_level__; /* LOG_ERROR LOG_INFO LOG_DEBUG LOG_TRACE */


typedef struct {
    fmi2String             instance_name;
    fmi2Type               fmu_type;
    fmi2String             fmu_guid;
    fmi2String             fmu_resource_location;
    fmi2Boolean            visible;
    fmi2Boolean            logging_on;
    fmi2CallbackFunctions* functions;
} fmi2fmu_test_setup;

void _fmi2_unit_test_logger(fmi2ComponentEnvironment componentEnvironment,
    fmi2String instanceName, fmi2Status status, fmi2String category,
    fmi2String message, ...)
{
    UNUSED(componentEnvironment);
    UNUSED(instanceName);
    function_called();
    check_expected(status);
    check_expected(category);
    check_expected(message);
}

static void _expected_log(const fmi2Status expected_status,
    const char* expected_category, const char* expected_msg)
{
    expect_function_call(_fmi2_unit_test_logger);
    expect_value(_fmi2_unit_test_logger, status, expected_status);
    expect_string(_fmi2_unit_test_logger, category, expected_category);
    expect_string(_fmi2_unit_test_logger, message, expected_msg);
}

static void _expect_init_logs(
    const TEST_SCENARIO scenario, const int32_t status)
{
    will_return(fmu_create, scenario);
    will_return(fmu_destroy, status);
    _expected_log(fmi2OK, "Debug", "FMU Model instantiated");
    _expected_log(
        fmi2OK, "Debug", "Resource location: data/test_fmu/resources");
    _expected_log(fmi2OK, "Debug", "Build indexes...");
    expect_function_call(__wrap_fmu_load_signal_handlers);
    expect_function_call(_test_fmu_setup);
    expect_function_call(fmu_create);
}

static void _expect_free_instance_logs()
{
    expect_function_call(fmu_destroy);
    expect_function_call(_test_fmu_remove);
    _expected_log(fmi2OK, "Debug", "Release var table");
    _expected_log(fmi2OK, "Debug", "Destroy the index");
    _expected_log(fmi2OK, "Debug", "Release FMI instance resources");
}

int test_fmi2fmu_setup(void** state)
{
    fmi2fmu_test_setup* setup = calloc(1, sizeof(fmi2fmu_test_setup));
    /* Commit with log level in model.yaml set to 6. */
    setup->logging_on = true;
    setup->fmu_guid = "{1-22-333-4444-55555-666666-7777777}";
    setup->fmu_type = fmi2CoSimulation;
    setup->instance_name = "test_inst";
    setup->visible = true;
    setup->fmu_resource_location = "data/test_fmu/resources";
    fmi2CallbackFunctions* callback = calloc(1, sizeof(fmi2CallbackFunctions));
    callback->logger = _fmi2_unit_test_logger;
    setup->functions = callback;
    *state = setup;
    return 0;
}

int test_fmi2fmu_teardown(void** state)
{
    fmi2fmu_test_setup* setup = *state;
    free(setup->functions);
    free(setup);
    return 0;
}

void test_fmi2Instantiate_returned_null(void** state)
{
    fmi2fmu_test_setup* setup = *state;

    _expect_init_logs(RETURN_NULL, fmi2OK);
    _expected_log(fmi2OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi2Component* inst = fmi2Instantiate(setup->instance_name, setup->fmu_type,
        setup->fmu_guid, setup->fmu_resource_location, setup->functions,
        setup->visible, setup->logging_on);

    assert_ptr_equal(captured_fmu_instance, inst);

    fmi2FreeInstance(inst);
}

void test_fmi2Instantiate_returned_new_instance(void** state)
{
    fmi2fmu_test_setup* setup = *state;

    _expect_init_logs(RETURN_NEW_INSTANCE, fmi2OK);
    _expected_log(fmi2OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi2Component* inst = fmi2Instantiate(setup->instance_name, setup->fmu_type,
        setup->fmu_guid, setup->fmu_resource_location, setup->functions,
        setup->visible, setup->logging_on);

    assert_ptr_not_equal(captured_fmu_instance, inst);
    fmi2FreeInstance(inst);
}

void test_fmi2Instantiate_returned_the_same_instance(void** state)
{
    fmi2fmu_test_setup* setup = *state;

    _expect_init_logs(RETURN_THE_SAME_INSTANCE, fmi2OK);
    _expected_log(fmi2OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi2Component* inst = fmi2Instantiate(setup->instance_name, setup->fmu_type,
        setup->fmu_guid, setup->fmu_resource_location, setup->functions,
        setup->visible, setup->logging_on);

    assert_ptr_equal(captured_fmu_instance, inst);
    fmi2FreeInstance(inst);
}

void test_fmi2Instantiate_errno(void** state)
{
    fmi2fmu_test_setup* setup = *state;

    char expected_errno_msg[100];
    snprintf(expected_errno_msg, sizeof(expected_errno_msg),
        "The FMU was not created correctly! (errro = %d)", EACCES);

    _expect_init_logs(SET_ERRNO, fmi2OK);
    _expected_log(fmi2Error, "Error", expected_errno_msg);
    _expected_log(fmi2OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi2Component* inst = fmi2Instantiate(setup->instance_name, setup->fmu_type,
        setup->fmu_guid, setup->fmu_resource_location, setup->functions,
        setup->visible, setup->logging_on);

    assert_ptr_equal(captured_fmu_instance, inst);
    assert_int_equal(errno, EACCES);
    errno = 0;  // reset errno
    fmi2FreeInstance(inst);
}

void test_fmi2Instantiate_uri_scheme(void** state)
{
    fmi2fmu_test_setup* setup = *state;
    char                uri[100];
    snprintf(uri, sizeof(uri), "%s%s", FILE_URI_SCHEME,
        setup->fmu_resource_location);

    _expect_init_logs(RETURN_NULL, fmi2OK);
    _expected_log(fmi2OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi2Component inst = fmi2Instantiate(setup->instance_name, setup->fmu_type,
        uri, setup->fmu_resource_location, setup->functions, setup->visible,
        setup->logging_on);

    FmuInstanceData* fmu_inst = inst;
    assert_string_equal(
        fmu_inst->instance.resource_location, setup->fmu_resource_location);
    assert_ptr_equal(captured_fmu_instance, inst);
    fmi2FreeInstance(inst);
}

void test_fmi2Instantiate_short_scheme(void** state)
{
    fmi2fmu_test_setup* setup = *state;
    char                uri[100];
    snprintf(uri, sizeof(uri), "%s%s", FILE_URI_SHORT_SCHEME,
        setup->fmu_resource_location);

    _expect_init_logs(RETURN_NULL, fmi2OK);
    _expected_log(fmi2OK, "Debug", "FMU Var Table is not configured");
    _expect_free_instance_logs();

    fmi2Component inst = fmi2Instantiate(setup->instance_name, setup->fmu_type,
        uri, setup->fmu_resource_location, setup->functions, setup->visible,
        setup->logging_on);

    FmuInstanceData* fmu_inst = inst;
    assert_string_equal(
        fmu_inst->instance.resource_location, setup->fmu_resource_location);
    assert_ptr_equal(captured_fmu_instance, inst);
    fmi2FreeInstance(inst);
}

void test_fmi2FreeInstance_returned_error(void** state)
{
    fmi2fmu_test_setup* setup = *state;
    const int32_t       error_value = -1;

    _expect_init_logs(RETURN_NULL, error_value);
    _expected_log(fmi2OK, "Debug", "FMU Var Table is not configured");
    expect_function_call(fmu_destroy);
    _expected_log(fmi2Error, "Error", "Could not release model");
    expect_function_call(_test_fmu_remove);
    _expected_log(fmi2OK, "Debug", "Release var table");
    _expected_log(fmi2OK, "Debug", "Destroy the index");
    _expected_log(fmi2OK, "Debug", "Release FMI instance resources");


    fmi2Component* inst = fmi2Instantiate(setup->instance_name, setup->fmu_type,
        setup->fmu_guid, setup->fmu_resource_location, setup->functions,
        setup->visible, setup->logging_on);

    assert_ptr_equal(captured_fmu_instance, inst);
    fmi2FreeInstance(inst);
}

int run_fmu2fmi_tests(void)
{
    void* s = test_fmi2fmu_setup;
    void* t = test_fmi2fmu_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fmi2Instantiate_returned_null, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi2Instantiate_returned_new_instance, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi2Instantiate_returned_the_same_instance, s, t),
        cmocka_unit_test_setup_teardown(test_fmi2Instantiate_uri_scheme, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi2Instantiate_short_scheme, s, t),
        cmocka_unit_test_setup_teardown(test_fmi2Instantiate_errno, s, t),
        cmocka_unit_test_setup_teardown(
            test_fmi2FreeInstance_returned_error, s, t),
    };

    return cmocka_run_group_tests_name("test_fmi2fmu", tests, NULL, NULL);
}

int main()
{
    int rc = 0;
    rc |= run_fmu2fmi_tests();
    return rc;
}
