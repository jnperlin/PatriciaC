// -------------------------------------------------------------------------------------
// PATRICIA tree (compressed radix-2 tree, dual-use node design) / unit testing
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------
#include "cpatricia_set.h"
#include "helper_build_tree.h"
#include "unity.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

void setUp(void) {
}
void tearDown(void) {
}

static void test_getbit_z(void) {
    TEST_ASSERT_EQUAL(false, patricia_getbit(NULL, 0, 0));
    TEST_ASSERT_EQUAL(true , patricia_getbit(NULL, 0, 1));
}

static void test_getbit_0(void) {
    uint32_t pattern = htonl(UINT32_C(0x55555555));
    TEST_ASSERT_EQUAL(false, patricia_getbit(&pattern, 0, 0));
    for (unsigned idx = 1; idx <= 32; ++idx) {
        TEST_ASSERT_EQUAL(!(idx & 1), patricia_getbit(&pattern, idx, idx));
        TEST_ASSERT_EQUAL( (idx & 1), patricia_getbit(&pattern, idx, (idx + 1)));
    }
}

static void test_getbit_1(void) {
    uint32_t pattern = htonl(UINT32_C(0xAAAAAAAA));
    TEST_ASSERT_EQUAL(false, patricia_getbit(&pattern, 0, 0));
    for (unsigned idx = 1; idx <= 32; ++idx) {
        TEST_ASSERT_EQUAL((idx & 1), patricia_getbit(&pattern, idx, idx));
        TEST_ASSERT_EQUAL(!(idx & 1), patricia_getbit(&pattern, idx, (idx + 1)));
    }
}

static void test_bitdiff_equ(void) {
    uint32_t pattern = htonl(UINT32_C(0xAAAAAAAA));
    for (unsigned idx = 0; idx <= 32; ++idx) {
        TEST_ASSERT_EQUAL(0, patricia_bitdiff(&pattern, idx, &pattern, idx));
    }
}

static void test_bitdiff_extequ(void) {
    // for a pattern with alternating bits, the diff position will ALWAYS
    // be the length of the shorter one, plus TWO!
    uint32_t pattern = htonl(UINT32_C(0xAAAAAAAA));
    for (unsigned i = 1; i < 32; ++i) {
        TEST_ASSERT_EQUAL((i+2), patricia_bitdiff(&pattern, i, &pattern, (i+1)));
    }
}

static void test_bitdiff_extbit(void) {
    // For a pattern repeating the last bit of the shorter one, the diff position
    // is the length of the shorter pattern, plus ONE!
    uint32_t pattern = htonl(UINT32_C(0xAA000000));
    for (unsigned i = 9; i < 32; ++i) {
        TEST_ASSERT_EQUAL(9, patricia_bitdiff(&pattern, 8, &pattern, i));
    }
}

static void test_bitdiff_extcpl(void) {
    // Fore a pattern repeating the complement of the last bit of the shorter one,
    // the diff position is the length of the longer pattern, plus ONE!
    uint32_t pattern = htonl(UINT32_C(0xAAFFFFFF));
    for (unsigned i = 9; i < 32; ++i) {
        TEST_ASSERT_EQUAL((i+1), patricia_bitdiff(&pattern, 8, &pattern, i));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_getbit_z);
    RUN_TEST(test_getbit_0);
    RUN_TEST(test_getbit_1);
    RUN_TEST(test_bitdiff_equ);
    RUN_TEST(test_bitdiff_extequ);
    RUN_TEST(test_bitdiff_extbit);
    RUN_TEST(test_bitdiff_extcpl);
    return UNITY_END();
}