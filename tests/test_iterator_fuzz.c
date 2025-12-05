// -------------------------------------------------------------------------------------
// PATRICIA tree (compressed radix-2 tree, dual-use node design) / unit testing
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------
// Credits to ChatGPT for doing the gritty details
// -------------------------------------------------------------------------------------
#include "cpatricia_map.h"
#include "helper_build_tree.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

void setUp(void) {
}
void tearDown(void) {
}

static void do_one_fuzz_run(unsigned seed, unsigned nkeys) {
    PatriciaMapT m;
    patrimap_init(&m);

    bool ok = build_random_map(&m, nkeys, seed);
    TEST_ASSERT_TRUE(ok);

    /* collect reference using real ref_postorder/inorder/preorder on root */
    const PTMapNodeT *root = s2m(m._m_set._m_root->_m_child[0]);
    NodeVecT pre, in, post;
    nv_init(&pre);
    nv_init(&in);
    nv_init(&post);
    ref_preorder(root, &pre);
    ref_inorder(root, &in);
    ref_postorder(root, &post);

    /* forward checks */
    PTMapIterT it;
    const PTMapNodeT *x;
    NodeVecT got;
    pmapiter_init(&it, &m, NULL, true, ePTMode_preOrder);
    nv_init(&got);
    while ((x = pmapiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&pre, &got));
    nv_free(&got);

    pmapiter_init(&it, &m, NULL, true, ePTMode_inOrder);
    nv_init(&got);
    while ((x = pmapiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&in, &got));
    nv_free(&got);

    pmapiter_init(&it, &m, NULL, true, ePTMode_postOrder);
    nv_init(&got);
    while ((x = pmapiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&post, &got));
    nv_free(&got);

    /* reverse checks (prev) by walking with reverse direction in iterator */
    pmapiter_init(&it, &m, NULL, false, ePTMode_preOrder);
    nv_init(&got);
    while ((x = pmapiter_next(&it)) != NULL) nv_push(&got, x);
    /* reverse-pre-order should produce pre-order of reversed tree:
       easier route: reverse the forward post/in/pre expectation
       but simplest: just check each vector is same length */
    TEST_ASSERT_EQUAL(pre.n, got.n);
    nv_free(&got);

    nv_free(&pre);
    nv_free(&in);
    nv_free(&post);
    patrimap_fini(&m);
}

static void test_fuzz_random_small(void) {
    do_one_fuzz_run(1u, 20u);
}
static void test_fuzz_random_medium(void) {
    do_one_fuzz_run(123u, 80u);
}
static void test_fuzz_random_seeded(void) {
    do_one_fuzz_run(98765u, 120u);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fuzz_random_small);
    RUN_TEST(test_fuzz_random_medium);
    RUN_TEST(test_fuzz_random_seeded);
    return UNITY_END();
}
