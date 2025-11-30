// -------------------------------------------------------------------------------------
// PATRICIA tree (compressed radix-2 tree, dual-use node design)
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------
//  - memory management can be dealt with via user-provided policy
//  - keys are piggy-packed into the node
//  - bit indexing is PASCAL-like: 0 is invalid, the first bit has index 1
//  - full synthetic root sentinel
//  - 'uintptr_t' payload
// -------------------------------------------------------------------------------------

#ifndef CPATRICIA_A86A7C45_B842_401F_B245_319CB49D9C79
#define CPATRICIA_A86A7C45_B842_401F_B245_319CB49D9C79

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pt_memfunc_ {
    void *(*fp_alloc)(void *arena, size_t bytes); ///< @brief mandatory node allocator
    void  (*fp_free )(void *arena, void *obj);    ///< @brief optional node deleter or NULL
    void  (*fp_kill )(void *);                    ///< @brief optional arena killer
} PTMemFuncT;

typedef struct pt_map_node_ {
    struct pt_map_node_ *_m_child[2];///< child[0]=left, child[1]=right
    uintptr_t            payload;    ///< user-define payload
    uint16_t             _m_bpos;    ///< branching bit position (Pascal index)
    uint16_t             nbit;       ///< key length in bits
    char                 data[1];    ///< piggy-packed key bytes
} PTMapNodeT;

typedef struct patricia_map_ {
    PTMapNodeT          _m_root[1];  ///< root & sentinel
    const PTMemFuncT   *_m_mfunc;    ///< memory core functions
    void               *_m_arena;    ///< allocator arena (or NULL)
} PatriciaMapT;

extern void              patricia_init_ex(PatriciaMapT *t, const PTMemFuncT *fp, void *arena);
extern void              patricia_init(PatriciaMapT *t);
extern void              patricia_fini_ex(PatriciaMapT *t, void (*deleter)(uintptr_t, void*), void *uarg);
extern void              patricia_fini(PatriciaMapT *t);

extern const PTMapNodeT *patricia_lookup(const PatriciaMapT *t, const void *key, uint16_t bitlen);
extern const PTMapNodeT *patricia_prefix(const PatriciaMapT *t, const void *key, uint16_t bitlen);
extern const PTMapNodeT *patricia_insert(PatriciaMapT *t, const void *key, uint16_t bitlen, uintptr_t payload, bool *inserted);
extern bool              patricia_evict(PatriciaMapT *t, PTMapNodeT *node);
extern bool              patricia_remove(PatriciaMapT *t, const void *key, uint16_t bitlen, uintptr_t *payload_out);

// the next are exported for easy unit testing
extern bool     patricia_getbit(const void *base, uint16_t bitlen, uint16_t bitidx);
extern uint16_t patricia_bitdiff(const void *p1, uint16_t l1, const void *p2, uint16_t l2);
extern bool     patricia_equkey(const void *p1, uint16_t l1, const void *p2, uint16_t l2);

// iteration can be fun...
typedef enum {
    ePTMode_preOrder  = 0,
    ePTMode_inOrder   = 1,
    ePTMode_postOrder = 2
} EPTIterMode;

typedef struct {
    PatriciaMapT       *_m_tree;    // tree to use in some places
    const PTMapNodeT   *_m_root;    // root node for iteration, can be subtree 
    const PTMapNodeT   *_m_nodep;   // node to pick up un next step
    const PTMapNodeT   *_m_pstk[8]; // bounded parent FIFO stack, should be 4/8/16
    uint8_t             _m_stkLen;  // number of nodes in stack
    uint8_t             _m_stkTop;  // current top index of stack, round robin fifo!
    uint8_t             _m_state;   // state / way node was entered
    uint8_t             _m_mode;    // pre/in/post order mode flag
    bool                _m_dir;     // direction, true is laft-to-right
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

#endif /* CPATRICIA_A86A7C45_B842_401F_B245_319CB49D9C79 */
