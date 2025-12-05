// -------------------------------------------------------------------------------------
// PATRICIA tree SET (compressed radix-2 tree, dual-use node design) for key sets
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
// -------------------------------------------------------------------------------------

#ifndef CPATRICIA_SET_A86A7C45_B842_401F_B245_319CB49D9C79
#define CPATRICIA_SET_A86A7C45_B842_401F_B245_319CB49D9C79

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief memory policy functions
/// Memory allocation can be customized, and for maps, it must be customised.
/// The triplet of function permits implementation of different strategies:
///
///  + integration with the system/runtime allocator
///  + using a special arena for the nodes
///  + incremental build / batch destroy patterns
///
/// The allocator is given a size and must return a pointer to memory of at least that
/// size, aligned to the alignment requirements of a pointer.  For a _map_, additional
/// space for the payload has to be reserved BEFORE the address returned.
///
/// The deallocator gets a node pointer.  For a map, the base pointer has to be revovered
/// from the structure of the real map node.  If the payload contains data that needs
/// cleanup, this is the place to do it.  The deallocator does not really have to free
/// the memory block. That would cause a memory leak, of course, unless the 3rd function
/// is also used:
///
/// The arena killer is called last from the map/set finaliser function, if not NULL.
/// If your deallocator defers freeing memory, this is the final place to do it.
/// (Just think of using a mmap()-based, page-on-demand arena for a set/map with
/// incremental-fill, batch-destroy semantic!)
typedef struct pt_memfunc_ {
    void *(*fp_alloc)(void *arena, size_t bytes); ///< @brief mandatory node allocator
    void  (*fp_free )(void *arena, void *obj);    ///< @brief optional node deleter or NULL
    void  (*fp_kill )(void *);                    ///< @brief optional arena killer
} PTMemFuncT;

/// @brief core structure of a PATRICIA set node
typedef struct pt_set_node_ {
    struct pt_set_node_ *_m_child[2];///< @brief child[0]=left, child[1]=right
# ifdef PATRICIA_TEST_LINKCNT
    unsigned int        lcount;      ///< test only!
# endif
    uint16_t             bpos;       ///< @brief \bold{(RO)} branching bit position (Pascal index)
    uint16_t             nbit;       ///< @brief \bold{(RO)} key length in bits
    char                 data[1];    ///< @brief \bold{(RO)} piggy-packed key bytes
} PTSetNodeT;

/// @brief the PATRICIA node container structure
typedef struct patricia_set_ {
    PTSetNodeT          _m_root[1];  ///< @brief root & sentinel
    const PTMemFuncT   *_m_mfunc;    ///< @brief memory core functions
    void               *_m_arena;    ///< @brief allocator arena (or NULL)
} PatriciaSetT;

extern void              patriset_init_ex(PatriciaSetT *t, const PTMemFuncT *fp, void *arena);
extern void              patriset_init(PatriciaSetT *t);
extern void              patriset_fini(PatriciaSetT *t);

extern const PTSetNodeT *patriset_lookup(const PatriciaSetT *t, const void *key, uint16_t bitlen);
extern const PTSetNodeT *patriset_prefix(const PatriciaSetT *t, const void *key, uint16_t bitlen);
extern const PTSetNodeT *patriset_insert(PatriciaSetT *t, const void *key, uint16_t bitlen, bool *inserted);
extern bool              patriset_evict(PatriciaSetT *t, PTSetNodeT *node);
extern bool              patriset_remove(PatriciaSetT *t, const void *key, uint16_t bitlen);

// the next are exported for easy unit testing
extern unsigned int      patricia_clz(size_t v);
extern size_t            patricia_bswap(size_t v);
extern bool              patricia_getbit(const void *base, uint16_t bitlen, uint16_t bitidx);
extern uint16_t          patricia_bitdiff(const void *p1, uint16_t l1, const void *p2, uint16_t l2);
extern bool              patricia_equkey(const void *p1, uint16_t l1, const void *p2, uint16_t l2);

// iteration can be fun...

/// @brief the 3 modes of tree iteration
typedef enum {
    ePTMode_preOrder  = 0,
    ePTMode_inOrder   = 1,
    ePTMode_postOrder = 2
} EPTIterMode;

/// @brief PATRICIA set iterator structure
/// Iterating a tree without parent pointers or full threading links requires either
/// a full stack of parent nodes or a search for the true parent of the node when
/// going upward in the tree.  Both variants are not very appealing:  The needed size
/// of the stack is not known a priori, and a walk from root to the node is overhead.
///
/// We take a hybrid approach here, with a FIFO or size-bound stack of parents.  On the
/// way down, the FIFO is filled with true parents, throwing off the oldest entries when
/// the queue capicity is reached.  Of course, the cache has to be rebuild regularely.
/// But with a size of 8, this happens after doing 256 steps, and walking down a
/// PATRICIA tree is fast as only bits are extracted -- no full key compares here!
typedef struct {
    const PTSetNodeT   *_m_root;        ///< @brief root node for iteration, can be subtree 
    const PTSetNodeT   *_m_nodep;       ///< @brief node to pick up un next step
    const PTSetNodeT   *_m_pstk[8];     ///< @brief bounded parent FIFO stack, should be 4/8/16
    uint8_t             _m_stkLen;      ///< @brief number of nodes in stack
    uint8_t             _m_stkTop;      ///< @brief current top index of stack, round robin fifo!
    uint8_t             _m_state : 3;   ///< @brief state / way node was entered
    uint8_t             _m_mode  : 2;   ///< @brief pre/in/post order mode flag
    bool                _m_dir;         ///< @brief direction, true is laft-to-right
} PTSetIterT;

extern void              psetiter_init(PTSetIterT *i, PatriciaSetT *t, const PTSetNodeT *root, bool dir, EPTIterMode mode);
extern const PTSetNodeT *psetiter_next(PTSetIterT *i);
extern const PTSetNodeT *psetiter_prev(PTSetIterT *i);
extern void              psetiter_reset(PTSetIterT *i);

extern void patriset_print(FILE *ofp, PatriciaSetT const *tree);
extern bool patriset_todot(FILE *ofp, PatriciaSetT const *tree, bool (*label)(FILE *, const PTSetNodeT *));

#ifdef __cplusplus
}
#endif

#endif /* CPATRICIA_SET_A86A7C45_B842_401F_B245_319CB49D9C79 */
