// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <fmi2Functions.h>
#include <fmi2FunctionTypes.h>
#include <fmi2TypesPlatform.h>
#include <dse/testing.h>
#include <dse/clib/collections/hashmap.h>
#include <dse/clib/util/strings.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>
#include <dse/fmu/fmu.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


int test_fmu_variable_setup(void** state)
{
    FmuInstanceData* fmu = calloc(1, sizeof(FmuInstanceData));
    hashmap_init(&fmu->variables.scalar.input);
    hashmap_init(&fmu->variables.scalar.output);
    hashmap_init(&fmu->variables.binary.rx);
    hashmap_init(&fmu->variables.binary.tx);
    hashmap_init(&fmu->variables.binary.encode_func);
    hashmap_init(&fmu->variables.binary.decode_func);
    fmu_load_signal_handlers(fmu);
    hashlist_init(&fmu->variables.binary.free_list, 1024);

    fmu->instance.resource_location = (char*)"data/test_fmu/resources";

    *state = fmu;
    return 0;
}


int test_fmu_variable_teardown(void** state)
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


void test_fmu_variable_encoding(void** state)
{
    FmuInstanceData* fmu = *state;
    fmu->variables.vtable.setup(fmu);

    // Working with the second SV (should be binary).
    assert_non_null(fmu->data);
    FmuSignalVector* sv = fmu->data;
    sv++;
    assert_non_null(sv->binary);


    // Check the configuration of encode/decode functions.
    // vr=4, bar_1 (input)
    assert_non_null(hashmap_get(&fmu->variables.binary.encode_func, "4"));
    assert_non_null(hashmap_get(&fmu->variables.binary.decode_func, "4"));
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.encode_func, "4"),
        dse_ascii85_encode);
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.decode_func, "4"),
        dse_ascii85_decode);

    // vr=5, bar_2 (output)
    assert_non_null(hashmap_get(&fmu->variables.binary.encode_func, "5"));
    assert_non_null(hashmap_get(&fmu->variables.binary.decode_func, "5"));
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.encode_func, "5"),
        dse_ascii85_encode);
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.decode_func, "5"),
        dse_ascii85_decode);


    // Remove any codec objects.
    for (size_t i = 0; i < 2; i++) {
        NCodecInstance* nc = sv->ncodec[i];
        if (nc) {
            fmu_ncodec_close(fmu, nc);
            sv->ncodec[i] = NULL;
        }
    }


#define MSG_PLAIN_TXT   "Hello World"
#define MSG_ENCODED_TXT "87cURD]i,\"Ebo7d"

    // Test the operation: message -> tx signal -(encode)-> tx variable
    {
        fmi2ValueReference vr[1] = { 5 };
        fmi2String         value[1] = { NULL };
        dse_buffer_append(&sv->binary[1], &sv->length[1], &sv->buffer_size[1],
            MSG_PLAIN_TXT, strlen(MSG_PLAIN_TXT) + 1);
        fmi2GetString((fmi2Component*)fmu, vr, 1, value);
        assert_non_null(value[0]);
        assert_string_equal(value[0], MSG_ENCODED_TXT);
    }

    // Test the operation: message -> tx signal -(encode)-> tx variable
    {
        fmi2ValueReference vr[1] = { 4 };
        fmi2String         value[1] = { MSG_ENCODED_TXT };

        assert_int_equal(sv->length[0], 0);
        fmi2SetString((fmi2Component*)fmu, vr, 1, value);
        assert_int_equal(sv->length[0], strlen(MSG_PLAIN_TXT) + 1);
        assert_string_equal(sv->binary[0], MSG_PLAIN_TXT);
    }


    // Call the cleanup directly.
    fmu->variables.vtable.remove(fmu);
}


static bool _check_ncodec_param(
    void* ncodec, const char* param, const char* value)
{
    for (int i = 0; i >= 0; i++) {
        NCodecConfigItem nci = ncodec_stat(ncodec, &i);
        if (i < 0) break;
        if (strcmp(nci.name, param) == 0) {
            if (strcmp(nci.value, value) == 0) {
                return true;
            }
        }
    }
    return false;
}

void test_fmu_variable_codec(void** state)
{
    FmuInstanceData* fmu = *state;
    fmu->variables.vtable.setup(fmu);

    // Working with the second SV (should be binary).
    assert_non_null(fmu->data);
    FmuSignalVector* sv = fmu->data;
    sv++;
    assert_non_null(sv->binary);


    // Check the configuration of NCodec variables.
    // vr=4, bar_1 (input)
    assert_non_null(hashmap_get(&fmu->variables.binary.encode_func, "4"));
    assert_non_null(hashmap_get(&fmu->variables.binary.decode_func, "4"));
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.encode_func, "4"),
        dse_ascii85_encode);
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.decode_func, "4"),
        dse_ascii85_decode);
    // vr=5, bar_2 (output)
    assert_non_null(hashmap_get(&fmu->variables.binary.encode_func, "5"));
    assert_non_null(hashmap_get(&fmu->variables.binary.decode_func, "5"));
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.encode_func, "5"),
        dse_ascii85_encode);
    assert_ptr_equal(hashmap_get(&fmu->variables.binary.decode_func, "5"),
        dse_ascii85_decode);
    // NCodec objects
    assert_non_null(sv->ncodec[0]);
    assert_non_null(sv->ncodec[1]);
    assert_string_equal(sv->mime_type[0],
        "application/x-automotive-bus; interface=stream; "
        "type=pdu; schema=fbs; swc_id=23; ecu_id=5");
    assert_string_equal(sv->mime_type[1],
        "application/x-automotive-bus; interface=stream; "
        "type=pdu; schema=fbs; swc_id=23; ecu_id=5");
    assert_true(_check_ncodec_param(sv->ncodec[0], "type", "pdu"));
    assert_true(_check_ncodec_param(sv->ncodec[0], "schema", "fbs"));
    assert_true(_check_ncodec_param(sv->ncodec[0], "swc_id", "23"));
    assert_true(_check_ncodec_param(sv->ncodec[0], "ecu_id", "5"));


// Encode a PDU, read the string, decode to binary, or feedback into other side.
#define GREETING "Hello World"
    NCODEC* nc_tx = sv->ncodec[1];
    NCODEC* nc_rx = sv->ncodec[0];
    int     rc;

    // TX a PDU
    rc = ncodec_write(nc_tx, &(struct NCodecPdu){ .id = 42,
                                 .payload = (uint8_t*)GREETING,
                                 .payload_len = strlen(GREETING) + 1,
                                 .swc_id = 0x22 });
    assert_int_equal(rc, strlen(GREETING) + 1);
    ncodec_flush(nc_tx);
    // Loopback TX -> RX
    fmi2ValueReference vr[1] = { 5 };
    fmi2String         value[1] = { NULL };
    fmi2GetString((fmi2Component*)fmu, vr, 1, value);
    assert_non_null(value[0]);
    vr[0] = 4;
    fmi2SetString((fmi2Component*)fmu, vr, 1, value);

    // RX the PDU
    ncodec_seek(nc_rx, 0, NCODEC_SEEK_SET);
    NCodecPdu pdu = {};
    rc = ncodec_read(nc_rx, &pdu);
    assert_int_equal(rc, strlen(GREETING) + 1);
    assert_int_equal(pdu.payload_len, strlen(GREETING) + 1);
    assert_non_null(pdu.payload);
    assert_memory_equal(pdu.payload, GREETING, strlen(GREETING) + 1);
    assert_int_equal(pdu.swc_id, 0x22);
    assert_int_equal(pdu.ecu_id, 5);


    // Call the cleanup directly.
    fmu->variables.vtable.remove(fmu);
}


int run_fmu_variable_tests(void)
{
    void* s = test_fmu_variable_setup;
    void* t = test_fmu_variable_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_fmu_variable_encoding, s, t),
        cmocka_unit_test_setup_teardown(test_fmu_variable_codec, s, t),
    };

    return cmocka_run_group_tests_name("DEFAULT SIGNALS", tests, NULL, NULL);
}
