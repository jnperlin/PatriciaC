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
#define _GNU_SOURCE
#include "helper_build_tree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

uint16_t
str2bits(const char *s)
{
    return s ? (strnlen(s, (UINT16_MAX / CHAR_BIT)) * CHAR_BIT) : 0;
}

/* -------- bit helpers ---------------------------------------------------- */

/* Pascal/MSB-first 1-based bit extraction */
int test_getbit(const void *base, uint16_t bitlen, uint16_t bitidx) {
    if (bitidx == 0) return 0;
    if (bitidx > bitlen) return 0;
    const unsigned char *b = (const unsigned char *)base;
    uint16_t zero_based = bitidx - 1;
    unsigned off = zero_based >> 3;
    unsigned bit_in_byte = zero_based & 7;
    return (b[off] >> (7 - bit_in_byte)) & 1;
}

/* Extended variant: if the requested bit index is beyond bitlen, return the
 * inverse (flipped) value of the final bit. This supports tests that rely on
 * an 'extend by last bit flipped' semantic.
 */
int test_getbit_ext_lastflip(const void *base, uint16_t bitlen, uint16_t bitidx) {
    if (bitlen == 0) return 0;
    if (bitidx == 0) return 0;
    if (bitidx <= bitlen) {
        return test_getbit(base, bitlen, bitidx);
    }
    /* flip last bit */
    int last = test_getbit(base, bitlen, bitlen);
    return !last;
}

/* -------- NodeVec ------------------------------------------------------- */

void nv_init(NodeVecT *v) {
    v->a = NULL;
    v->n = 0;
    v->cap = 0;
}
void nv_free(NodeVecT *v) {
    free(v->a);
    v->a = NULL;
    v->n = 0;
    v->cap = 0;
}
void nv_push(NodeVecT *v, const PTMapNodeT *n) {
    if (v->n == v->cap) {
        size_t nc = v->cap ? v->cap * 2 : 16;
        const PTMapNodeT **na = (const PTMapNodeT **)realloc(v->a, nc * sizeof(*na));
        if (!na) return;
        v->a = na;
        v->cap = nc;
    }
    v->a[v->n++] = n;
}

/* -------- reference traversals ----------------------------------------- */

/* helper to detect a real downlink */
static const PTMapNodeT *downchild(const PTMapNodeT *p, int dir) {
    const PTSetNodeT *c = p->_m_node._m_child[dir];
    if (c->bpos > p->_m_node.bpos) return s2m(c);
    return NULL;
}

void ref_preorder(const PTMapNodeT *root, NodeVecT *out) {
    if (!root) return;
    nv_push(out, root);
    const PTMapNodeT *l = downchild(root, 0);
    if (l) ref_preorder(l, out);
    const PTMapNodeT *r = downchild(root, 1);
    if (r) ref_preorder(r, out);
}

void ref_inorder(const PTMapNodeT *root, NodeVecT *out) {
    if (!root) return;
    const PTMapNodeT *l = downchild(root, 0);
    if (l) ref_inorder(l, out);
    nv_push(out, root);
    const PTMapNodeT *r = downchild(root, 1);
    if (r) ref_inorder(r, out);
}

void ref_postorder(const PTMapNodeT *root, NodeVecT *out) {
    if (!root) return;
    const PTMapNodeT *l = downchild(root, 0);
    if (l) ref_postorder(l, out);
    const PTMapNodeT *r = downchild(root, 1);
    if (r) ref_postorder(r, out);
    nv_push(out, root);
}

/* -------- small helpers ------------------------------------------------- */

/* Build a patricia map from a NUL-terminated list of strings (C keys). The
 * keys are encoded as bytes; bitlen = strlen * 8.
 */
bool build_map_from_words(PatriciaMapT *map, const char *words[], uintptr_t start_payload) {
    if (!map) return false;
    for (uintptr_t i = 0; words[i]; ++i) {
        bool ins = false;
        PTMapNodeT *n =
            (PTMapNodeT*)patrimap_insert(map, words[i], (uint16_t)(strlen(words[i]) * 8),  &ins);
        if (!n || !ins) return false;
        n->payload = (uintptr_t)(start_payload + i);
    }
    return true;
}

/* basic random generator; returns bit-length and fills buf bytes with random bytes */
uint16_t gen_random_key(unsigned seed, uint8_t *buf, size_t bufbytes) {
    /* simple LCG for deterministic behaviour in tests */
    uint32_t state = seed ? seed : 0xC0FFEE;
    size_t bytes = (seed % (bufbytes ? bufbytes : 8)) + 1; /* at least 1 byte */
    for (size_t i = 0; i < bytes; ++i) {
        state = state * 1664525u + 1013904223u;
        buf[i] = (uint8_t)(state >> 16);
    }
    return (uint16_t)(bytes * 8);
}

/* Build a random map: insert nkeys random keys */
bool build_random_map(PatriciaMapT *map, unsigned nkeys, unsigned seed) {
    if (!map) return false;
    uint8_t tmp[32];
    for (unsigned i = 0; i < nkeys; ++i) {
        uint16_t bitlen = gen_random_key(seed + i * 7 + 3, tmp, sizeof tmp);
        bool ins = false;
        PTMapNodeT *n = (PTMapNodeT*)patrimap_insert(map, tmp, bitlen, &ins);
        if (!n) return false;
        n->payload = (uintptr_t)i;
    }
    return true;
}

/* collect all nodes by doing a postorder (we rely on root sentinel to be root) */
void collect_all_nodes(const PTMapNodeT *root, NodeVecT *out) {
    if (!root) return;
    /* walk full tree by downlinks */
    const PTMapNodeT *l = downchild(root, 0);
    if (l) collect_all_nodes(l, out);
    const PTMapNodeT *r = downchild(root, 1);
    if (r) collect_all_nodes(r, out);
    nv_push(out, root);
}

/* compare two NodeVecs for same length and same pointer sequence */
bool compare_nodevecs(const NodeVecT *a, const NodeVecT *b) {
    if (a->n != b->n) return false;
    for (size_t i = 0; i < a->n; ++i) {
        if (a->a[i] != b->a[i]) return false;
    }
    return true;
}
