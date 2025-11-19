// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <dse/testing.h>
#include <dse/fmu/fmu.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


int test_fmu_default_signal_setup(void** state)
{
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));
    hashmap_init(&fmu->variables.scalar.input);
    hashmap_init(&fmu->variables.scalar.output);
    hashmap_init(&fmu->variables.binary.rx);
    hashmap_init(&fmu->variables.binary.tx);
    hashmap_init(&fmu->variables.binary.encode_func);
    hashmap_init(&fmu->variables.binary.decode_func);
    fmu_load_signal_handlers(fmu);

    fmu->instance.resource_location = (char*)"data/test_fmu/resources";

    *state = fmu;
    return 0;
}


int test_fmu_default_signal_teardown(void** state)
{
    FmuInstanceData* fmu = *state;
    hashmap_destroy(&fmu->variables.scalar.input);
    hashmap_destroy(&fmu->variables.scalar.output);
    hashmap_destroy(&fmu->variables.binary.rx);
    hashmap_destroy(&fmu->variables.binary.tx);
    hashmap_destroy(&fmu->variables.binary.encode_func);
    hashmap_destroy(&fmu->variables.binary.decode_func);
    hashlist_destroy(&fmu->variables.binary.free_list);
    if (fmu) free(fmu);
    return 0;
}


typedef struct TC {
    const char* signal[10];
    const char* vref[10];
    uint32_t    count;
    bool        is_binary;
    uint32_t    causality[10];
} TC;

void test_fmu_default_signals(void** state)
{
    FmuInstanceData* fmu = *state;
    fmu->variables.vtable.setup(fmu);

    assert_non_null(fmu->data);
    FmuSignalVector* sv = fmu->data;

    TC tc[] = { { .count = 3,
                    .signal = { "foo_1", "foo_2", "foo_3" },
                    .vref = { "1", "2", "3" },
                    .causality = { 1, 0, 0 } },
        { .count = 2,
            .signal = { "bar_1", "bar_2" },
            .vref = { "4", "5" },
            .causality = { 1, 0 },
            .is_binary = true } };
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        assert_int_equal(tc[i].count, sv[i].count);
        for (uint32_t j = 0; j < sv[i].count; j++) {
            assert_string_equal(tc[i].signal[j], sv[i].signal[j]);
            if (tc[i].is_binary) {
                FmuSignalVectorIndex* idx = NULL;
                /* Input variable. */
                if (tc[i].causality[j]) {
                    idx = hashmap_get(&fmu->variables.binary.rx, tc[i].vref[j]);
                } else {
                    idx = hashmap_get(&fmu->variables.binary.tx, tc[i].vref[j]);
                }
                assert_non_null(idx);
                assert_non_null(idx->sv);
                assert_ptr_equal(idx->sv, &sv[i]);
                assert_int_equal(idx->sv->count, sv[i].count);
                assert_int_equal(idx->vi, j);
            } else {
                double* value = NULL;
                /* Input variable. */
                if (tc[i].causality[j]) {
                    value = hashmap_get(
                        &fmu->variables.scalar.input, tc[i].vref[j]);
                } else {
                    value = hashmap_get(
                        &fmu->variables.scalar.output, tc[i].vref[j]);
                }
                assert_non_null(value);
            }
        }
        if (tc[i].is_binary) {
            assert_non_null(sv[i].length);
            assert_non_null(sv[i].binary);
            assert_non_null(sv[i].buffer_size);
        } else {
            assert_non_null(sv[i].scalar);
        }
    }

    fmu->variables.vtable.remove(fmu);
}


void test_fmu_default_signals_reset(void** state)
{
    FmuInstanceData* fmu = *state;
    fmu->variables.vtable.setup(fmu);

    assert_non_null(fmu->data);

    FmuSignalVectorIndex* idx_i = hashmap_get(&fmu->variables.binary.rx, "4");
    FmuSignalVectorIndex* idx_o = hashmap_get(&fmu->variables.binary.tx, "5");
    assert_non_null(idx_i);
    assert_non_null(idx_o);

    idx_i->sv->length[idx_i->vi] = 42;
    idx_o->sv->length[idx_o->vi] = 43;
    assert_int_equal(idx_i->sv->length[idx_i->vi], 42);
    assert_int_equal(idx_o->sv->length[idx_o->vi], 43);

    fmu->variables.vtable.reset(fmu);

    assert_int_equal(idx_i->sv->length[idx_i->vi], 0);
    assert_int_equal(idx_o->sv->length[idx_o->vi], 0);

    fmu->variables.vtable.remove(fmu);
}


typedef struct {
    double var_1;
    double var_2;
} VarTable;

void test_fmu_var_table(void** state)
{
    /* Setup the FMU. */
    FmuInstanceData* fmu = *state;
    fmu->variables.vtable.setup(fmu);
    assert_non_null(fmu->data);
    double* var_1 = hashmap_get(&fmu->variables.scalar.input, "1");
    double* var_2 = hashmap_get(&fmu->variables.scalar.output, "2");
    assert_non_null(var_1);
    assert_non_null(var_2);

    /* Configure the Var Table. */
    VarTable* vt = malloc(sizeof(VarTable));
    *vt = (VarTable){
        .var_1 = fmu_register_var(fmu, 1, true, offsetof(VarTable, var_1)),
        .var_2 = fmu_register_var(fmu, 2, false, offsetof(VarTable, var_2)),
    };
    fmu_register_var_table(fmu, vt);
    assert_double_equal(vt->var_1, 0, 0);
    assert_double_equal(vt->var_2, 0, 0);

    /* Check the marshalling. */
    VarTable* v = fmu_var_table(fmu);
    assert_non_null(v);
    assert_ptr_equal(v, vt);
    *var_1 = 42;
    *var_2 = 24;
    for (FmuVarTableMarshalItem* mi = fmu->var_table.marshal_list;
        mi && mi->variable; mi++) {
        *mi->variable = *mi->signal;
    }
    assert_double_equal(v->var_1, 42, 0);
    assert_double_equal(v->var_2, 24, 0);
    assert_double_equal(*var_1, 42, 0);
    assert_double_equal(*var_2, 24, 0);
    v->var_1 = 24;
    v->var_2 = 42;
    for (FmuVarTableMarshalItem* mi = fmu->var_table.marshal_list;
        mi && mi->variable; mi++) {
        *mi->signal = *mi->variable;
    }
    assert_double_equal(v->var_1, 24, 0);
    assert_double_equal(v->var_2, 42, 0);
    assert_double_equal(*var_1, 24, 0);
    assert_double_equal(*var_2, 42, 0);

    /* Finished. */
    fmu->variables.vtable.remove(fmu);
    free(fmu->var_table.table);
    free(fmu->var_table.marshal_list);
}


void test_fmu_lookup_ncodec(void** state)
{
    /* Setup the FMU. */
    FmuInstanceData* fmu = *state;
    fmu->variables.vtable.setup(fmu);
    assert_non_null(fmu->data);
    FmuSignalVectorIndex* idx_4 = hashmap_get(&fmu->variables.binary.rx, "4");
    FmuSignalVectorIndex* idx_5 = hashmap_get(&fmu->variables.binary.tx, "5");
    assert_non_null(idx_4);
    assert_non_null(idx_5);
    assert_non_null(idx_4->sv->ncodec[idx_4->vi]);
    assert_non_null(idx_5->sv->ncodec[idx_5->vi]);

    /* Check the loopup. */
    assert_ptr_equal(
        idx_4->sv->ncodec[idx_4->vi], fmu_lookup_ncodec(fmu, 4, true));
    assert_ptr_equal(
        idx_5->sv->ncodec[idx_5->vi], fmu_lookup_ncodec(fmu, 5, false));

    /* Finished. */
    fmu->variables.vtable.remove(fmu);
    free(fmu->var_table.table);
    free(fmu->var_table.marshal_list);
}


int run_fmu_default_signal_tests(void)
{
    void* s = test_fmu_default_signal_setup;
    void* t = test_fmu_default_signal_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_fmu_default_signals, s, t),
        cmocka_unit_test_setup_teardown(test_fmu_default_signals_reset, s, t),
        cmocka_unit_test_setup_teardown(test_fmu_var_table, s, t),
        cmocka_unit_test_setup_teardown(test_fmu_lookup_ncodec, s, t),
    };

    return cmocka_run_group_tests_name("DEFAULT SIGNALS", tests, NULL, NULL);
}
