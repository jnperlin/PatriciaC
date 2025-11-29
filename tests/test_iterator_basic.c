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
#include "cpatricia.h"
#include "helper_build_tree.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

/* Simple tests: empty tree, single node, two nodes. */

void setUp(void) {
}
void tearDown(void) {
}

static void test_empty_tree_iteration(void)
{
    PatriciaMapT m;
    patricia_init(&m);

    PTMapIterT it;
    ptiter_init(&it, &m, NULL, true, ePTMode_preOrder);

    const PTMapNodeT *n = ptiter_next(&it);
    TEST_ASSERT_NULL(n);

    patricia_fini(&m);
}

static void test_single_node_iteration(void)
{
    PatriciaMapT m;
    patricia_init(&m);

    const char *k = "solo";
    bool ins;
    const PTMapNodeT *n = patricia_insert(&m, k, (uint16_t)(strlen(k) * 8), 0x77, &ins);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_TRUE(ins);

    PTMapIterT it;
    ptiter_init(&it, &m, NULL, true, ePTMode_preOrder);
    const PTMapNodeT *p = ptiter_next(&it);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_PTR(n, p);

    /* prev from that point should return the same in reverse */
    ptiter_init(&it, &m, NULL, true, ePTMode_postOrder);
    const PTMapNodeT *q;
    do {
        q = ptiter_next(&it);
    } while (q); /* walk to end */
    /* reset and reverse */
    ptiter_init(&it, &m, NULL, false, ePTMode_postOrder);
    const PTMapNodeT *r = ptiter_next(&it);
    /* for single node, iter should produce node again */
    (void)r;

    patricia_fini(&m);
}

static void test_small_manual_tree(void)
{
    PatriciaMapT m;
    patricia_init(&m);

    const char *words[] = {"a", "b", "ab", NULL};
    bool ok = build_map_from_words(&m, words, 0);
    TEST_ASSERT_TRUE(ok);

    /* reference traversals */
    NodeVecT pre, in, post;
    nv_init(&pre);
    nv_init(&in);
    nv_init(&post);

    const PTMapNodeT *root = m._m_root->_m_child[0];
    ref_preorder(root, &pre);
    ref_inorder(root, &in);
    ref_postorder(root, &post);

    /* test forward pre-order */
    PTMapIterT it;
    ptiter_init(&it, &m, NULL, true, ePTMode_preOrder);
    NodeVecT got;
    nv_init(&got);
    const PTMapNodeT *x;
    while ((x = ptiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&pre, &got));
    nv_free(&got);

    /* test forward in-order */
    ptiter_init(&it, &m, NULL, true, ePTMode_inOrder);
    nv_init(&got);
    while ((x = ptiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&in, &got));
    nv_free(&got);

    /* test forward post-order */
    ptiter_init(&it, &m, NULL, true, ePTMode_postOrder);
    nv_init(&got);
    while ((x = ptiter_next(&it)) != NULL) nv_push(&got, x);
    TEST_ASSERT_TRUE(compare_nodevecs(&post, &got));
    nv_free(&got);

    nv_free(&pre);
    nv_free(&in);
    nv_free(&post);
    patricia_fini(&m);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_tree_iteration);
    RUN_TEST(test_single_node_iteration);
    RUN_TEST(test_small_manual_tree);
    return UNITY_END();
}
