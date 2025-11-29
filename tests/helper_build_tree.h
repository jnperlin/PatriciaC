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
#ifndef TEST_HELPER_BUILD_TREE_H
#define TEST_HELPER_BUILD_TREE_H

#include "cpatricia.h" /* your header (the one you pasted earlier) */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Tiny test helpers for building small patricia trees and for
 * comparing iterator output with a reference traversal.
 *
 * Two bit helpers are supplied:
 *  - test_getbit  : standard Pascal-style MSB-first 1-based bit index.
 *  - test_getbit_ext_lastflip : if bitidx > nbit, returns inverse of last bit.
 */

#ifdef __cplusplus
extern "C" {
#endif

int test_getbit(const void *base, uint16_t bitlen, uint16_t bitidx);
int test_getbit_ext_lastflip(const void *base, uint16_t bitlen, uint16_t bitidx);

/* small dynamic array of node pointers used in tests */
typedef struct {
    const PTMapNodeT **a;
    size_t n;
    size_t cap;
} NodeVecT;

void nv_init(NodeVecT *v);
void nv_free(NodeVecT *v);
void nv_push(NodeVecT *v, const PTMapNodeT *n);

/* reference traversals (collect nodes in pre/in/post order) */
void ref_preorder(const PTMapNodeT *root, NodeVecT *out);
void ref_inorder(const PTMapNodeT *root, NodeVecT *out);
void ref_postorder(const PTMapNodeT *root, NodeVecT *out);

/* build a map from array of null-terminated strings using patricia_insert */
bool build_map_from_words(PatriciaMapT *map, const char *words[], uintptr_t start_payload);

/* generate a random key (bytes) and return bitlen, stores bytes in buf.
   buf must be large enough; returns bitlen (>=1), also fills bufbytes */
uint16_t gen_random_key(unsigned seed, uint8_t *buf, size_t bufbytes);

/* build random tree by inserting N random keys into map; returns success */
bool build_random_map(PatriciaMapT *map, unsigned nkeys, unsigned seed);

/* utility: compare iterator output (sequence of nodes) with reference NodeVecT */
bool compare_nodevecs(const NodeVecT *a, const NodeVecT *b);

/* get list of reachable nodes (walk down from root following downlinks) */
void collect_all_nodes(const PTMapNodeT *root, NodeVecT *out);

#ifdef __cplusplus
}
#endif

#endif /* TEST_HELPER_BUILD_TREE_H */
