// -------------------------------------------------------------------------------------
// PATRICIA tree MAP (compressed radix-2 tree, dual-use node design)
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------
//  - memory management can be dealt with via user-provided policy
//  - memory functions must also initialise / cleanup payload, if payload requires
// -------------------------------------------------------------------------------------

#ifndef CPATRICIA_MAP_A86A7C45_B842_401F_B245_319CB49D9C79
#define CPATRICIA_MAP_A86A7C45_B842_401F_B245_319CB49D9C79

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cpatricia_set.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Patricia Map node
/// This si a key-value-pair, where the fixed-size value is *prepended* to the var-sized
/// patricia set node that is used for managing the tree.
typedef struct {
    uintptr_t       payload;    ///< @brief user-define payload
    PTSetNodeT      treelnk;    ///< @brief the SET node we're based on
} PTMapNodeT;

/// @brief Typing capsule -- basically a map is a set with additional info
typedef struct {
    PatriciaSetT    tree;       ///< @brief the basic set we're extending
} PatriciaMapT;

extern void              patricia_init_ex(PatriciaMapT *t, const PTMemFuncT *fp, void *arena);
extern void              patricia_init(PatriciaMapT *t);
extern void              patricia_fini(PatriciaMapT *t);

extern const PTMapNodeT *patricia_lookup(const PatriciaMapT *t, const void *key, uint16_t bitlen);
extern const PTMapNodeT *patricia_prefix(const PatriciaMapT *t, const void *key, uint16_t bitlen);
extern const PTMapNodeT *patricia_insert(PatriciaMapT *t, const void *key, uint16_t bitlen, bool *inserted);
extern bool              patricia_evict(PatriciaMapT *t, PTMapNodeT *node);
extern bool              patricia_remove(PatriciaMapT *t, const void *key, uint16_t bitlen, uintptr_t *payload_out);

typedef struct {
    PTSetIterT  inner;  ///< @brief the inner iterator we're using   
} PTMapIterT;

extern void              ptiter_init(PTMapIterT *i, PatriciaMapT *t, const PTMapNodeT *root, bool dir, EPTIterMode mode);
extern const PTMapNodeT *ptiter_next(PTMapIterT *i);
extern const PTMapNodeT *ptiter_prev(PTMapIterT *i);
extern void              ptiter_reset(PTMapIterT *i);

extern void patricia_print(FILE *ofp, PatriciaMapT const *tree);
extern bool patricia_todot(FILE *ofp, PatriciaMapT const *tree, bool (*label)(FILE *, const PTMapNodeT *));

#ifdef __cplusplus
}
#endif

#endif /* CPATRICIA_MAP_A86A7C45_B842_401F_B245_319CB49D9C79 */
