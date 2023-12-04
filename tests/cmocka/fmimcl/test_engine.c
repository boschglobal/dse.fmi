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


void test_engine__allocate_scalar_source(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Parse the config.
    fmimcl_parse(fmu_model);
    assert_null(fmu_model->data.scalar);
    assert_null(fmu_model->data.name);
    assert_int_equal(fmu_model->data.count, 0);

    // Allocate storage for scalar signals.
    fmimcl_allocate_scalar_source(fmu_model);
    assert_non_null(fmu_model->data.scalar);
    assert_non_null(fmu_model->data.name);
    assert_int_equal(fmu_model->data.count, 6);

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
            .name = "mg-1-2-10",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_RXONLY,
            .type = MARSHAL_TYPE_DOUBLE,
            .offset = 3,
            .count = 1,
            .ref = { 1 },
        },
        {
            .name = "mg-1-3-10",
            .kind = MARSHAL_KIND_PRIMITIVE,
            .dir = MARSHAL_DIRECTION_TXONLY,
            .type = MARSHAL_TYPE_DOUBLE,
            .offset = 4,
            .count = 2,
            .ref = { 0, 5 },
        },
    };

    // Parse the config.
    fmimcl_parse(fmu_model);
    fmimcl_allocate_scalar_source(fmu_model);

    // Generate the marshal tables.
    fmimcl_generate_marshal_table(fmu_model);

    assert_non_null(fmu_model->data.mg_table);
    size_t        count = 0;
    MarshalGroup* mg;
    for (mg = fmu_model->data.mg_table; mg->name; mg++)
        count++;
    assert_int_equal(count, 4);
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
    fmimcl_allocate_scalar_source(fmu_model);

    // Generate the marshal tables.
    fmimcl_generate_marshal_table(fmu_model);

    assert_non_null(fmu_model->data.mg_table);
    size_t        count = 0;
    MarshalGroup* mg;
    for (mg = fmu_model->data.mg_table; mg->name; mg++)
        count++;
    assert_int_equal(count, 4);

    // Set the source signals to known values.
    assert_int_equal(fmu_model->data.count, 6);
    for (size_t i = 0; i < fmu_model->data.count; i++) {
        fmu_model->data.scalar[i] = i + 1;
    }

    // Marshal out.
    marshal_group_out(fmu_model->data.mg_table);

    // Check the result (only RX should show change).
    mg = fmu_model->data.mg_table;
    assert_int_equal(mg[0].target._int32[0], 0);
    assert_int_equal(mg[0].target._int32[1], 0);
    assert_int_equal(mg[1].target._int32[0], 3);
    assert_double_equal(mg[2].target._double[0], 0.0, 0.0);
    assert_double_equal(mg[3].target._double[0], 5.0, 0.0);
    assert_double_equal(mg[3].target._double[1], 6.0, 0.0);

    fmimcl_destroy(fmu_model);
}


void test_engine__marshal_from_adapter(void** state)
{
    FmimclMock* mock = *state;
    FmuModel*   fmu_model = &mock->model;

    // Parse the config.
    fmimcl_parse(fmu_model);
    fmimcl_allocate_scalar_source(fmu_model);

    // Generate the marshal tables.
    fmimcl_generate_marshal_table(fmu_model);

    assert_non_null(fmu_model->data.mg_table);
    size_t        count = 0;
    MarshalGroup* mg;
    for (mg = fmu_model->data.mg_table; mg->name; mg++)
        count++;
    assert_int_equal(count, 4);

    // Set the target signals/variables to known values.
    assert_int_equal(fmu_model->data.count, 6);
    mg = fmu_model->data.mg_table;
    mg[0].target._int32[0] = 10;
    mg[0].target._int32[1] = 20;
    mg[1].target._int32[0] = 30;
    mg[2].target._double[0] = 40.0;
    mg[3].target._double[0] = 50.0;
    mg[3].target._double[1] = 60.0;

    // Marshal in.
    marshal_group_in(fmu_model->data.mg_table);

    // Check the result (only TX should show change).
    assert_double_equal(fmu_model->data.scalar[0], 10.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[1], 20.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[2], 0.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[3], 40.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[4], 0.0, 0.0);
    assert_double_equal(fmu_model->data.scalar[5], 0.0, 0.0);


    fmimcl_destroy(fmu_model);
}


int run_engine_tests(void)
{
    void* s = test_engine_setup;
    void* t = test_engine_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_engine__allocate_scalar_source, s, t),
        cmocka_unit_test_setup_teardown(
            test_engine__create_marshal_tables, s, t),
        cmocka_unit_test_setup_teardown(test_engine__marshal_to_adapter, s, t),
        cmocka_unit_test_setup_teardown(
            test_engine__marshal_from_adapter, s, t),
    };

    return cmocka_run_group_tests_name("ENGINE", tests, NULL, NULL);
}
