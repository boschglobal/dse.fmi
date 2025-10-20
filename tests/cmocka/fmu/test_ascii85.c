// Copyright 2024 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdio.h>
#include <dse/testing.h>


#define UNUSED(x)     ((void)x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))


extern char* dse_ascii85_encode(const char* source, size_t len);
extern char* dse_ascii85_decode(const char* source, size_t* len);


int test_ascii85_setup(void** state)
{
    UNUSED(state);
    return 0;
}


int test_ascii85_teardown(void** state)
{
    UNUSED(state);
    return 0;
}


typedef struct TC_EN {
    const char* base;
    const char* enc;
} TC_EN;

void test_ascii85__encode(void** state)
{
    UNUSED(state);

    TC_EN tc[] = {
        {
            .base = "Man is distinguished, not only by his reason, but by this "
                    "singular passion from other animals, which is a lust of "
                    "the mind, that by a perseverance of delight in the "
                    "continued and indefatigable generation of knowledge, "
                    "exceeds the short vehemence of any carnal pleasure.",
            .enc = "9jqo^BlbD-BleB1DJ+*+F(f,q/0JhKF<GL>Cj@.4Gp$d7F!,L7@<6@)/"
                   "0JDEF<G%<+EV:2F!,O<DJ+*.@<*K0@<6L(Df-\\0Ec5e;DffZ(EZee.Bl."
                   "9pF\"AGXBPCsi+DGm>@3BB/F*&OCAfu2/"
                   "AKYi(DIb:@FD,*)+C]U=@3BN#EcYf8ATD3s@q?d$AftVqCh[NqF<G:8+EV:"
                   ".+Cf>-FD5W8ARlolDIal(DId<j@<?3r@:F%a+D58'ATD4$Bl@l3De:,-"
                   "DJs`8ARoFb/0JMK@qB4^F!,R<AKZ&-DfTqBG%G>uD.RTpAKYo'+CT/"
                   "5+Cei#DII?(E,9)oF*2M7/c",
        },
        { .base = "Please encode this", .enc = ":i']OF(HJ*DI[TqAKZ).Bla" },
        { .base = "This is an encoded sentence 01234567",
            .enc = "<+oue+DGm>@;[3!DI[TqARlp)ASuU$DI[6#0JP==1c70M" },
        { .base = "This is an encoded sentence 01234567",
            .enc = "<+oue+DGm>@;[3!DI[TqARlp)ASuU$DI[6#0JP==1c70M" }
    };

    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        char* encoded = dse_ascii85_encode(tc[i].base, strlen(tc[i].base));
        assert_string_equal(encoded, tc[i].enc);
        free(encoded);
    }
}

void test_ascii85__decode(void** state)
{
    UNUSED(state);

    TC_EN tc[] = {
        {
            .enc = "9jqo^BlbD-BleB1DJ+*+F(f,q/0JhKF<GL>Cj@.4Gp$d7F!,L7@<6@)/"
                   "0JDEF<G%<+EV:2F!,O<DJ+*.@<*K0@<6L(Df-\\0Ec5e;DffZ(EZee.Bl."
                   "9pF\"AGXBPCsi+DGm>@3BB/F*&OCAfu2/"
                   "AKYi(DIb:@FD,*)+C]U=@3BN#EcYf8ATD3s@q?d$AftVqCh[NqF<G:8+EV:"
                   ".+Cf>-FD5W8ARlolDIal(DId<j@<?3r@:F%a+D58'ATD4$Bl@l3De:,-"
                   "DJs`8ARoFb/0JMK@qB4^F!,R<AKZ&-DfTqBG%G>uD.RTpAKYo'+CT/"
                   "5+Cei#DII?(E,9)oF*2M7/c",
            .base = "Man is distinguished, not only by his reason, but by this "
                    "singular passion from other animals, which is a lust of "
                    "the mind, that by a perseverance of delight in the "
                    "continued and indefatigable generation of knowledge, "
                    "exceeds the short vehemence of any carnal pleasure.",
        },
        { .base = "Please encode this", .enc = ":i']OF(HJ*DI[TqAKZ).Bla" },
        { .base = "This is an encoded sentence 01234567",
            .enc = "<+oue+DGm>@;[3!DI[TqARlp)ASuU$DI[6#0JP==1c70M" }
    };
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        size_t dec_len = 0;
        char*  base = dse_ascii85_decode(tc[i].enc, &dec_len);
        assert_string_equal(base, tc[i].base);
        free(base);
    }
}


typedef struct TC_RT {
    const char data[1000];
    size_t     data_len;
} TC_RT;

void test_ascii85__roundtrip(void** state)
{
    UNUSED(state);

    TC_RT tc[] = {
        {.data = { 0xff, 0xff, 0xff, 0xff, }, .data_len = 4 },
        {.data = { 0x00, }, .data_len = 1 },
        {.data = { 0x00, 0x00, }, .data_len = 2 },
        {.data = { 0x00, 0x00, 0x00, }, .data_len = 3 },
        {.data = { 0x00, 0x00, 0x00, 0x00, }, .data_len = 4 },
        {.data = { 0x00, 0x00, 0x00, 0x00, 0x00 }, .data_len = 5 },
        {.data = {
            0x74, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x53, 0x46, 0x52, 0x41, 0x9a, 0xff, 0xff, 0xff,
            0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x04, 0x00, 0x00, 0x00, 0xb2, 0xff, 0xff, 0xff,
            0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
            0xcc, 0xff, 0xff, 0xff, 0xf2, 0x03, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x01, 0x02, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
        }, .data_len = (16 * 4) },
        {.data = {
            0x52, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x53, 0x50, 0x44, 0x55, 0xbc, 0xff, 0xff, 0xff,
            0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x04, 0x00, 0x00, 0x00, 0xdc, 0xff, 0xff, 0xff,
            0x2a, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
            0x22, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
            0x0c, 0x00, 0x00, 0x00, 0x48, 0x65, 0x6c, 0x6c,
            0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64, 0x00,
            0x10, 0x00, 0x14, 0x00, 0x04, 0x00, 0x08, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x10, 0x00,
            0x06, 0x00, 0x08, 0x00, 0x04, 0x00,
        }, .data_len = 86 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(tc); i++) {
        char*  enc_string = dse_ascii85_encode(tc[i].data, tc[i].data_len);
        size_t dec_len = 0;
        char*  dec_string = dse_ascii85_decode(enc_string, &dec_len);
        assert_int_equal(tc[i].data_len, dec_len);
        assert_memory_equal(tc[i].data, dec_string, dec_len);
        free(enc_string);
        free(dec_string);
    }
}


int run_ascii85_tests(void)
{
    void* s = test_ascii85_setup;
    void* t = test_ascii85_teardown;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_ascii85__encode, s, t),
        cmocka_unit_test_setup_teardown(test_ascii85__decode, s, t),
        cmocka_unit_test_setup_teardown(test_ascii85__roundtrip, s, t),
    };

    return cmocka_run_group_tests_name("ASCII85", tests, NULL, NULL);
}
