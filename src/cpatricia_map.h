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

#include "cpatricia_set.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Patricia Map node
/// This is a key-value-pair, where the fixed-size value is *prepended* to the var-sized
/// patricia set node that is used for managing the tree.
typedef struct {
    uintptr_t       payload;    ///< @brief user-define payload
    PTSetNodeT      _m_node;    ///< @brief the SET node we're based on
} PTMapNodeT;

/// @brief Typing capsule -- basically a map is a set with additional info
typedef struct {
    PatriciaSetT    _m_set;    ///< @brief the basic set we're extending
} PatriciaMapT;

extern void              patrimap_init_ex(PatriciaMapT *t, const PTMemFuncT *fp, void *arena);
extern void              patrimap_init(PatriciaMapT *t);
extern void              patrimap_fini(PatriciaMapT *t);

extern const PTMapNodeT *patrimap_lookup(const PatriciaMapT *t, const void *key, uint16_t bitlen);
extern const PTMapNodeT *patrimap_prefix(const PatriciaMapT *t, const void *key, uint16_t bitlen);
extern const PTMapNodeT *patrimap_insert(PatriciaMapT *t, const void *key, uint16_t bitlen, bool *inserted);
extern bool              patrimap_evict(PatriciaMapT *t, PTMapNodeT *node);
extern bool              patrimap_remove(PatriciaMapT *t, const void *key, uint16_t bitlen);

typedef struct {
    PTSetIterT _m_inner; ///< @brief the inner iterator we're using   
} PTMapIterT;

extern void              pmapiter_init(PTMapIterT *i, PatriciaMapT *t, const PTMapNodeT *root, bool dir, EPTIterMode mode);
extern const PTMapNodeT *pmapiter_next(PTMapIterT *i);
extern const PTMapNodeT *pmapiter_prev(PTMapIterT *i);
extern void              pmapiter_reset(PTMapIterT *i);

#ifdef __cplusplus
}
#endif

#endif /* CPATRICIA_MAP_A86A7C45_B842_401F_B245_319CB49D9C79 */
