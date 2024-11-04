// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


typedef struct FmimclMock {
    FmuModel          model;
    ModelInstanceSpec model_instance;
} FmimclMock;


static FmimclMock* _create_mock(const char** files)
{
    /* Load yaml files. */
    YamlDocList* doc_list = NULL;
    for (const char** _ = files; *_ != NULL; _++) {
        doc_list = dse_yaml_load_file(*_, doc_list);
    }

    /* Construct the mock object .*/
    FmimclMock* mock = malloc(sizeof(FmimclMock));
    *mock = (FmimclMock) {
        .model = {
            .name = "FMU"
         },
        .model_instance = {
            .name = (char*)"fmu_inst",
            .yaml_doc_list = doc_list,
        },
    };
    mock->model.mcl.model.mi = &mock->model_instance;

    /* Return the mock (caller to free). */
    return mock;
}


int test_parser_setup(void** state)
{
    const char* yaml_files[] = {
        "data/fmu.yaml",
        "data/simulation.yaml",
        NULL,
    };
    *state = _create_mock(yaml_files);
    return 0;
}

int test_parser_setup_sort(void** state)
{
    const char* yaml_files[] = {
        "data/parser_sort.yaml",
        NULL,
    };
    *state = _create_mock(yaml_files);
    return 0;
}

int test_parser_teardown(void** state)
{
    FmimclMock* mock = *state;
    if (mock) {
        dse_yaml_destroy_doc_list(mock->model_instance.yaml_doc_list);
        free(mock);
    }
    return 0;
}


void test_parser__fmu_model(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Run parser.
    assert_null(fmu_model->mcl.adapter);
    assert_null(fmu_model->mcl.version);
    assert_null(fmu_model->m_doc);
    assert_null(fmu_model->path);
    assert_null(fmu_model->handle);
    fmimcl_parse(fmu_model);

    // Check MCL properties.
    assert_non_null(fmu_model->mcl.adapter);
    assert_string_equal(fmu_model->mcl.adapter, "fmi");
    assert_non_null(fmu_model->mcl.version);
    assert_string_equal(fmu_model->mcl.version, "2.0");

    // Check FMU Model properties.
    assert_non_null(fmu_model->name);
    assert_string_equal(fmu_model->name, "FMU");
    assert_non_null(fmu_model->version);
    assert_string_equal(fmu_model->version, "1.48");
    assert_int_equal(fmu_model->cosim, true);
    assert_double_equal(fmu_model->mcl.step_size, 0.0001, 0.0);
    assert_non_null(fmu_model->guid);
    assert_string_equal(
        fmu_model->guid, "{11111111-2222-3333-4444-555555555555}");
    assert_non_null(fmu_model->resource_dir);
    assert_string_equal(fmu_model->resource_dir,
        "dse/build/_out/fmimcl/example/simple/fmu/resources");
    assert_non_null(fmu_model->path);
    assert_string_equal(fmu_model->path, "examples/fmu/fmu/binaries/simple.so");
    assert_null(fmu_model->handle);
    assert_non_null(fmu_model->m_doc);

    // Unload parser objects.
    fmimcl_destroy(fmu_model);
}


void test_parser__fmu_signal(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Test conditions.
    FmuSignal signals[] = {
        {
            .name = "count",
            .variable_name = "count",
            .variable_vref = 2,
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXRX,
            .variable_type = MARSHAL_TYPE_INT32,
        },
        {
            .name = "foo",
            .variable_name = "foo",
            .variable_vref = 0,
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXRX,
            .variable_type = MARSHAL_TYPE_DOUBLE,
        },
        {
            .name = "bar",
            .variable_name = "bar",
            .variable_vref = 1,
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXRX,
            .variable_type = MARSHAL_TYPE_DOUBLE,
        },
        {
            .name = "active",
            .variable_name = "active",
            .variable_vref = 3,
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXRX,
            .variable_type = MARSHAL_TYPE_BOOL,
        },

        {
            .name = "string_rx",
            .variable_name = "string_rx",
            .variable_vref = 5,
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
        },
        {
            .name = "string_ascii85_rx",
            .variable_name = "string_ascii85_rx",
            .variable_vref = 7,
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
            .variable_annotation_encoding = "ascii85",
        },
        {
            .name = "string_tx",
            .variable_name = "string_tx",
            .variable_vref = 4,
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
        },
        {
            .name = "string_ascii85_tx",
            .variable_name = "string_ascii85_tx",
            .variable_vref = 6,
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
            .variable_annotation_encoding = "ascii85",
        },
    };

    // Run parser.
    assert_null(fmu_model->signals);
    assert_null(fmu_model->path);
    assert_null(fmu_model->handle);
    assert_null(fmu_model->m_doc);
    fmimcl_parse(fmu_model);

    // Check FMU Model properties.
    assert_non_null(fmu_model->m_doc);
    assert_non_null(fmu_model->name);
    assert_string_equal(fmu_model->name, "FMU");
    assert_non_null(fmu_model->path);
    assert_string_equal(fmu_model->path, "examples/fmu/fmu/binaries/simple.so");
    assert_null(fmu_model->handle);

    // Check FMU Signal properties.
    assert_non_null(fmu_model->signals);
    size_t count = 0;
    for (FmuSignal* _ = fmu_model->signals; _ && _->name; _++)
        count++;
    assert_int_equal(ARRAY_SIZE(signals), count);
    for (size_t i = 0; i < ARRAY_SIZE(signals); i++) {
        FmuSignal* signal = &fmu_model->signals[i];
        FmuSignal* check = &signals[i];
        assert_string_equal(signal->name, check->name);
        assert_string_equal(signal->variable_name, check->variable_name);
        assert_int_equal(signal->variable_vref, check->variable_vref);
        assert_int_equal(signal->variable_kind, check->variable_kind);
        assert_int_equal(signal->variable_dir, check->variable_dir);
        assert_int_equal(signal->variable_type, check->variable_type);
        if (check->variable_annotation_encoding) {
            assert_non_null(signal->variable_annotation_encoding);
            assert_string_equal(signal->variable_annotation_encoding,
                check->variable_annotation_encoding);
        } else {
            assert_null(signal->variable_annotation_encoding);
        }
    }

    // Unload parser objects.
    fmimcl_destroy(fmu_model);
}


void test_parser__fmu_signal_sorting(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Test conditions:
    FmuSignal signals[] = {
        {
            .name = "integer_2_rx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_INT32,
        },
        {
            .name = "integer_1_rx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_INT32,
        },
        {
            .name = "integer_3_tx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_INT32,
        },
        {
            .name = "real_4_local",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_LOCAL,
            .variable_type = MARSHAL_TYPE_DOUBLE,
        },
        {
            .name = "real_1_rx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_DOUBLE,
        },
        {
            .name = "real_3_tx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_DOUBLE,
        },
        {
            .name = "real_2_tx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_DOUBLE,
        },
        {
            .name = "Boolean_1_rx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_BOOL,
        },
        {
            .name = "Boolean_2_tx",
            .variable_kind = MARSHAL_KIND_PRIMITIVE,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_BOOL,
        },

        {
            .name = "string_rx",
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
        },
        {
            .name = "string_ascii85_rx",
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_RXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
        },
        {
            .name = "string_tx",
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
        },
        {
            .name = "string_ascii85_tx",
            .variable_kind = MARSHAL_KIND_BINARY,
            .variable_dir = MARSHAL_DIRECTION_TXONLY,
            .variable_type = MARSHAL_TYPE_STRING,
        },
    };

    // Run parser.
    fmimcl_parse(fmu_model);

    // Check the order of FMU Signals.
    assert_non_null(fmu_model->signals);
    size_t count = 0;
    for (FmuSignal* _ = fmu_model->signals; _ && _->name; _++)
        count++;
    assert_int_equal(ARRAY_SIZE(signals), count);

    for (size_t i = 0; i < ARRAY_SIZE(signals); i++) {
        FmuSignal* signal = &fmu_model->signals[i];
        FmuSignal* check = &signals[i];
        log_trace("Signal: %s", signal->name);
        assert_string_equal(signal->name, check->name);
        assert_int_equal(signal->variable_kind, check->variable_kind);
        assert_int_equal(signal->variable_dir, check->variable_dir);
        assert_int_equal(signal->variable_type, check->variable_type);
    }

    // Unload parser objects.
    fmimcl_destroy(fmu_model);
}


int run_parser_tests(void)
{
    void* s = test_parser_setup;
    void* s_alt = test_parser_setup_sort;
    void* t = test_parser_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_parser__fmu_model, s, t),
        cmocka_unit_test_setup_teardown(test_parser__fmu_signal, s, t),
        cmocka_unit_test_setup_teardown(
            test_parser__fmu_signal_sorting, s_alt, t),
    };

    return cmocka_run_group_tests_name("PARSER", tests, NULL, NULL);
}
