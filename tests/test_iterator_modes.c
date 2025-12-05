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
#include <string.h>

void setUp(void) {
}
void tearDown(void) {
}

static void test_all_orders_for_map(PatriciaMapT *m) {
    /* reference vectors from root */
    NodeVecT pre, in, post;
    nv_init(&pre);
    nv_init(&in);
    nv_init(&post);

    const PTMapNodeT *root = s2m(m->_m_set._m_root->_m_child[0]);
    ref_preorder(root, &pre);
    ref_inorder(root, &in);
    ref_postorder(root, &post);

    PTMapIterT it;
    const PTMapNodeT *x;
    NodeVecT got;
    /* pre-order */
    pmapiter_init(&it, m, NULL, true, ePTMode_preOrder);
    nv_init(&got);
    while ((x = pmapiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&pre, &got));
    nv_free(&got);

    /* in-order */
    pmapiter_init(&it, m, NULL, true, ePTMode_inOrder);
    nv_init(&got);
    while ((x = pmapiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&in, &got));
    nv_free(&got);

    /* post-order */
    pmapiter_init(&it, m, NULL, true, ePTMode_postOrder);
    nv_init(&got);
    while ((x = pmapiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&post, &got));
    nv_free(&got);

    nv_free(&pre);
    nv_free(&in);
    nv_free(&post);
}

static void test_modes_on_example_map(void) {
    PatriciaMapT m;
    patrimap_init(&m);

    const char *words[] = {"alpha", "alpine", "al", "beta", "bet", "z", "zero", NULL};
    build_map_from_words(&m, words, 0);

    test_all_orders_for_map(&m);

    patrimap_fini(&m);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_modes_on_example_map);
    return UNITY_END();
}
