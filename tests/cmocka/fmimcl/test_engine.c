// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <dse/testing.h>
#include <dse/logger.h>
#include <dse/clib/util/yaml.h>
#include <dse/clib/collections/set.h>
#include <dse/modelc/runtime.h>
#include <dse/fmimcl/fmimcl.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


typedef struct FmimclMock {
    FmuModel          model;
    ModelInstanceSpec model_instance;
} FmimclMock;


int test_engine_setup(void** state)
{
    /* Load yaml files. */
    const char* yaml_files[] = {
        "data/parser_sort.yaml",
        NULL,
    };
    YamlDocList* doc_list = NULL;
    for (const char** _ = yaml_files; *_ != NULL; _++) {
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
    *state = mock;
    return 0;
}


int test_engine_teardown(void** state)
{
    FmimclMock* mock = *state;
    if (mock) {
        dse_yaml_destroy_doc_list(mock->model_instance.yaml_doc_list);
        free(mock);
    }
    return 0;
}


void test_engine__allocate_source(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Parse the config.
    fmimcl_parse(fmu_model);
    assert_null(fmu_model->data.scalar);
    assert_null(fmu_model->data.name);
    assert_int_equal(fmu_model->data.count, 0);

    // Allocate storage for scalar signals.
    fmimcl_allocate_source(fmu_model);
    assert_non_null(fmu_model->data.scalar);
    assert_non_null(fmu_model->data.name);
    assert_int_equal(fmu_model->data.count, 9 + 4);

    fmimcl_destroy(fmu_model);
}


typedef struct MCT_TC {
    const char* name;
    MarshalKind kind;
    MarshalDir  dir;
    MarshalType type;
    size_t      offset;
    size_t      count;
    int32_t     ref[10];
} MCT_TC;

void test_engine__create_marshal_tables(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Specify the test cases.
    MCT_TC tc[] = {
        {
            .name = "mg-1-2-7",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_RXONLY,
            .type = MARSHAL_TYPE_INT32,
            .offset = 0,
            .count = 2,
            .ref = { 3, 4 },
        },
        {
            .name = "mg-1-3-7",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_TXONLY,
            .type = MARSHAL_TYPE_INT32,
            .offset = 2,
            .count = 1,
            .ref = { 2 },
        },
        {
            .name = "mg-1-5-10",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_LOCAL,
            .type = MARSHAL_TYPE_DOUBLE,
            .offset = 3,
            .count = 1,
            .ref = { 8 },
        },
        {
            .name = "mg-1-2-10",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_RXONLY,
            .type = MARSHAL_TYPE_DOUBLE,
            .offset = 4,
            .count = 1,
            .ref = { 1 },
        },
        {
            .name = "mg-1-3-10",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_TXONLY,
            .type = MARSHAL_TYPE_DOUBLE,
            .offset = 5,
            .count = 2,
            .ref = { 0, 5 },
        },
        {
            .name = "mg-1-2-15",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_RXONLY,
            .type = MARSHAL_TYPE_BOOL,
            .offset = 7,
            .count = 1,
            .ref = { 7 },
        },
        {
            .name = "mg-1-3-15",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_TXONLY,
            .type = MARSHAL_TYPE_BOOL,
            .offset = 8,
            .count = 1,
            .ref = { 6 },
        },

        {
            .name = "mg-2-2-16",
            .kind = MARSHAL_KIND_BINARY,
            .dir = MARSHAL_DIRECTION_RXONLY,
            .type = MARSHAL_TYPE_STRING,
            .offset = 9,
            .count = 2,
            .ref = { 101, 103 },
        },
        {
            .name = "mg-2-3-16",
            .kind = MARSHAL_KIND_BINARY,
            .dir = MARSHAL_DIRECTION_TXONLY,
            .type = MARSHAL_TYPE_STRING,
            .offset = 11,
            .count = 2,
            .ref = { 100, 102 },
        },
    };

    // Parse the config.
    fmimcl_parse(fmu_model);
    fmimcl_allocate_source(fmu_model);

    // Generate the marshal tables.
    fmimcl_generate_marshal_table(fmu_model);

    assert_non_null(fmu_model->data.mg_table);
    size_t        count = 0;
    MarshalGroup* mg;
    for (mg = fmu_model->data.mg_table; mg->name; mg++)
        count++;
    assert_int_equal(count, 7 + 2);
    assert_int_equal(count, ARRAY_SIZE(tc));

    // Check the test cases.
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        mg = &fmu_model->data.mg_table[i];
        MCT_TC* t = &tc[i];
        if (t->name == NULL) continue;
        assert_string_equal(mg->name, t->name);
        assert_int_equal(mg->kind, t->kind);
        assert_int_equal(mg->dir, t->dir);
        assert_int_equal(mg->type, t->type);
        assert_int_equal(mg->count, t->count);
        assert_non_null(mg->target.ref);
        assert_memory_equal(
            mg->target.ref, t->ref, t->count * marshal_type_size(t->type));
        assert_non_null(mg->target.ptr);
        assert_non_null(mg->target._int32);
        assert_int_equal(mg->target._int32[0], 0);
        assert_int_equal(mg->source.offset, t->offset);
        assert_non_null(mg->source.scalar);
        assert_double_equal(mg->source.scalar[0], 0.0, 0.0);
    }

    fmimcl_destroy(fmu_model);
}


void test_engine__marshal_to_adapter(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Parse the config.
    fmimcl_parse(fmu_model);
    fmimcl_allocate_source(fmu_model);

    // Generate the marshal tables.
    fmimcl_generate_marshal_table(fmu_model);

    assert_non_null(fmu_model->data.mg_table);
    size_t        count = 0;
    MarshalGroup* mg;
    for (mg = fmu_model->data.mg_table; mg->name; mg++)
        count++;
    assert_int_equal(count, 7 + 2);

    // Set the source signals to known values.
    assert_int_equal(fmu_model->data.count, 9 + 4);
    for (size_t i = 0; i < fmu_model->data.count - 2; i++) {
        fmu_model->data.scalar[i] = i + 1;
    }
    fmu_model->data.scalar[7] = true;
    fmu_model->data.scalar[8] = true;

    // This content is sorted on kind/dir, not order in YAML.
    // 100(9), 102(10), 101(11), 103(12)
    fmu_model->data.binary[9] = strdup("foo");
    fmu_model->data.binary_len[9] = strlen("foo") + 1;
    fmu_model->data.binary[10] = strdup("foo_85");
    fmu_model->data.binary_len[10] = strlen("foo_85") + 1;
    fmu_model->data.binary[11] = strdup("bar");
    fmu_model->data.binary_len[11] = strlen("bar") + 1;
    fmu_model->data.binary[12] = strdup("bar_85");
    fmu_model->data.binary_len[12] = strlen("bar_85") + 1;

    // Marshal out: source -> target.
    marshal_group_out(fmu_model->data.mg_table);

    // Check the result (only RX should show change, i.e. input/parameter).
    mg = fmu_model->data.mg_table;
    assert_int_equal(mg[0].target._int32[0], 0);  // 3 output
    assert_int_equal(mg[0].target._int32[1], 0);  // 4
    assert_int_equal(mg[1].target._int32[0], 3);  // 2 input
    assert_double_equal(mg[2].target._double[0], 0.0, 0.0);
    assert_double_equal(mg[3].target._double[0], 0.0, 0.0);
    assert_double_equal(mg[4].target._double[0], 6.0, 0.0);
    assert_double_equal(mg[4].target._double[1], 7.0, 0.0);
    assert_false(mg[5].target._int32[0]);  // 7 output
    assert_true(mg[6].target._int32[0]);   // 6 input

    assert_null(mg[7].target._string[0]);      // 100 output (9)
    assert_null(mg[7].target._string[1]);      // 102 output (11)
    assert_non_null(mg[8].target._string[0]);  // 101 input (10)
    assert_non_null(mg[8].target._string[1]);  // 103 input (12)
    assert_string_equal(mg[8].target._string[0], "bar");
    assert_string_equal(mg[8].target._string[1], "bar_85");
    assert_int_equal(mg[7].target._binary_len[0], 0);
    assert_int_equal(mg[7].target._binary_len[1], 0);
    assert_int_equal(mg[8].target._binary_len[0], 0);  // Not set for strings.
    assert_int_equal(mg[8].target._binary_len[1], 0);  // Not set for strings.

    fmimcl_destroy(fmu_model);
}


void test_engine__marshal_from_adapter(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Parse the config.
    fmimcl_parse(fmu_model);
    fmimcl_allocate_source(fmu_model);

    // Generate the marshal tables.
    fmimcl_generate_marshal_table(fmu_model);

    assert_non_null(fmu_model->data.mg_table);
    size_t        count = 0;
    MarshalGroup* mg;
    for (mg = fmu_model->data.mg_table; mg->name; mg++)
        count++;
    assert_int_equal(count, 7 + 2);

    // Set the target signals/variables to known values.
    assert_int_equal(fmu_model->data.count, 9 + 4);
    mg = fmu_model->data.mg_table;
    mg[0].target._int32[0] = 10;
    mg[0].target._int32[1] = 20;
    mg[1].target._int32[0] = 30;

    mg[2].target._double[0] = 24.0;

    mg[3].target._double[0] = 40.0;
    mg[4].target._double[0] = 50.0;
    mg[4].target._double[1] = 60.0;
    mg[5].target._int32[0] = true;
    mg[6].target._int32[0] = true;

    // This content is sorted on kind/dir, not order in YAML.
    // 7-0(9), 7-1(11), 8-0(10), 8-1(12)
    mg[7].target._string[0] = strdup("foo");
    mg[7].target._string[1] = strdup("foo_85");
    mg[8].target._string[0] = strdup("bar");
    mg[8].target._string[1] = strdup("bar_85");


    // Marshal in: target -> source.
    marshal_group_in(fmu_model->data.mg_table);


    // Check the result (only TX should show change).
    assert_double_equal(fmu_model->data.scalar[0], 10.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[1], 20.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[2], 0.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[3], 24.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[4], 40.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[5], 0.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[6], 0.0, 0.0);
    assert_true(fmu_model->data.scalar[7]);
    assert_false(fmu_model->data.scalar[8]);

    assert_non_null(fmu_model->data.binary[9]);   // 100 output (9)
    assert_non_null(fmu_model->data.binary[10]);  // 101 input (10)
    assert_null(fmu_model->data.binary[11]);      // 102 output (11)
    assert_null(fmu_model->data.binary[12]);      // 103 input (12)
    assert_string_equal(fmu_model->data.binary[9], "foo");
    assert_string_equal(fmu_model->data.binary[10], "foo_85");
    assert_int_equal(fmu_model->data.binary_len[9], strlen("foo") + 1);
    assert_int_equal(fmu_model->data.binary_len[10], strlen("foo_85") + 1);
    assert_int_equal(fmu_model->data.binary_len[11], 0);
    assert_int_equal(fmu_model->data.binary_len[12], 0);

    // Target variables (inwards) are owned by "FMU", so marshal code does not
    // release.
    free(mg[7].target._string[0]);
    free(mg[7].target._string[1]);

    fmimcl_destroy(fmu_model);
}


int run_engine_tests(void)
{
    void* s = test_engine_setup;
    void* t = test_engine_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_engine__allocate_source, s, t),
        cmocka_unit_test_setup_teardown(
            test_engine__create_marshal_tables, s, t),
        cmocka_unit_test_setup_teardown(test_engine__marshal_to_adapter, s, t),
        cmocka_unit_test_setup_teardown(
            test_engine__marshal_from_adapter, s, t),
    };

    return cmocka_run_group_tests_name("ENGINE", tests, NULL, NULL);
}
