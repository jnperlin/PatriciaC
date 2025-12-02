// -------------------------------------------------------------------------------------
// PATRICIA tree (compressed radix-2 tree, dual-use node design)
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------
//
// This implementation uses a compact "dual-use" node representation: every node
// functions both as an internal routing node and as a terminal key holder.  No separate
// node types are required.  Instead of storing explicit pointers to parent nodes, each
// node maintains two child pointers, and the invariant that every node is reachable by
// exactly two pointers is used to reconstruct the topology.  For non-root nodes, these
// two references consist of one downward link from the parent and one upward/self-link
// acting as a parent indicator.
//
// The deletion logic relies critically on this invariant.  Each node that is logically
// removed is guaranteed to have exactly one remaining descendant branch and exactly one
// parent-side reference.  Pointer replacement is guided by comparing a child pointer
// (child[0] or child[1]) with the address of the node itself.  The expression
//      (x->child[i] == x)
// evaluates to true when the pointer is the self-link rather than a real subtree.  When
// used as an index, this boolean selects the *opposite* child pointer, yielding
// precisely the  subtree that must be spliced upward. This is the key operation that
// lets the algorithm collapse a node cleanly without ambiguity.
//
// The resulting structure is extremely compact, avoiding the overhead of explicit
// internal nodes or parent pointers, and is well suited for mutable environments where
// topology can be rewired in place. It does not translate directly to functional or
// persistent settings, where immutable nodes make pointer reuse and dual-role encoding
// impossible. In those environments, classical PATRICIA layouts with explicit internal
// nodes are required.
//
// While the code for deletion appears sparse, correctness hinges on strict maintenance
// of the two-reference invariant and careful handling of the parent-side replacement.
// The combination of compressed paths, dual-role nodes, and topology-by-invariant makes
// this variant one of the most space-efficient practical radix-2 PATRICIA designs for
// mutable data structures.
// -------------------------------------------------------------------------------------
//  - memory management can be dealt with via user-provided policy
//  - keys are piggy-packed into the node
//  - bit indexing is PASCAL-like: 0 is invalid, the first bit has index 1
//  - full synthetic root sentinel
// -------------------------------------------------------------------------------------

#include "cpatricia_map.h"

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

// -------------------------------------------------------------------------------------
// ==== memory allocation & helpers                                                 ====
// -------------------------------------------------------------------------------------

static inline PTMapNodeT *s2m(const PTSetNodeT *const np) {
    return (NULL != np) ? (PTMapNodeT *)((char *)np - sizeof(uintptr_t)) : NULL;
}

static inline PTSetNodeT *m2s(const PTMapNodeT *const np) {
    return (NULL != np) ? (PTSetNodeT*)&np->treelnk : NULL;
}

// -------------------------------------------------------------------------------------
// default node allocator using 'malloc()'
static void*
alloc_wrap(
    void  *unused,
    size_t bytes )
{
    return m2s(malloc(sizeof(uintptr_t) + bytes));
    (void)unused;
}

// -------------------------------------------------------------------------------------
// default node deallocator using 'free()'
static void
free_wrap(
    void *unused,
    void *obj   )
{
    free(s2m(obj));
    (void)unused;
}


// -------------------------------------------------------------------------------------
// ==== Core operations                                                             ====
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
/// @brief set up a PATRICIA tree with the given memory management scheme
/// @param t        tree to initialise
/// @param fp       function pointer block with memory policy functions
/// @param arena    additional data for policy functions
void
patricia_init_ex(
    PatriciaMapT     *t,
    const PTMemFuncT *fp,
    void             *arena)
{
    patriset_init_ex(&t->tree, fp, arena);
}

// -------------------------------------------------------------------------------------
/// @brief set up a PATRICIA tree with default memory functions
/// @param t        tree to initialise
void
patricia_init(
    PatriciaMapT* t)
{
    static const PTMemFuncT mf_memfunc = {
        alloc_wrap,
        free_wrap,
        NULL
    };
    patriset_init_ex(&t->tree, &mf_memfunc, NULL);
}

// -------------------------------------------------------------------------------------
/// @brief finalize a PATRICIA tree
/// Destroy all nodes in the tree, without doing anything special to manage the payload
///
/// @param t        tree where all nodes should be flushed
void
patricia_fini(
    PatriciaMapT *t)
{
    patriset_fini(&t->tree);
}

// -------------------------------------------------------------------------------------
/// @brief  lookup (exact match) for a key in the patricia tree
/// @param t        tree to search
/// @param key      storage of key bits
/// @param bitlen   number of key bits
/// @return         node with exact matcing key or @c NULL
const PTMapNodeT *
patricia_lookup(
    const PatriciaMapT *t,
    const void *key,
    uint16_t bitlen)
{
    return s2m(patriset_lookup(&t->tree, key, bitlen));
}

// -------------------------------------------------------------------------------------
/// @brief longest prefix match for a key in the patricia tree
/// @param t        tree to search
/// @param key      storage of key bits
/// @param bitlen   number of key bits
/// @return         node with non-empty longest prefix key or @c NULL
const PTMapNodeT *
patricia_prefix(
    const PatriciaMapT *t,
    const void *key,
    uint16_t bitlen)
{
    return s2m(patriset_prefix(&t->tree, key, bitlen));
}

// -------------------------------------------------------------------------------------
/// @brief  create node with given key & payload, insert into tree
/// @param t        tree to insert into
/// @param key      key data storage
/// @param bitlen   number of bits in key
/// @param inserted opt. storage for 'node created' flag
/// @return         node with matching key (new or existing) or @c NULL on error
const PTMapNodeT *
patricia_insert(
    PatriciaMapT *t,
    const void *key,
    uint16_t bitlen,
    bool *inserted)
{
    return s2m(patriset_insert(&t->tree, key, bitlen, inserted));
}

// -------------------------------------------------------------------------------------
// ==== Deletion by key or node pointer                                             ====
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
/// @brief remove a node by identity from a PATRICIA key
/// @param t    tree owning the node
/// @param node node to remove from tree
/// @return     @c true on success, @c false on error (node not in tree)
bool
patricia_evict(
    PatriciaMapT *t,
    PTMapNodeT *node)
{
    return patriset_evict(&t->tree, m2s(node));
}

// -------------------------------------------------------------------------------------
/// @brief remove a node by key, optionally yielding the payload
/// @param t        tree owning the node
/// @param key      key data storage
/// @param bitlen   number of bits in key
/// @param payload_out (opt) where to store the payload of the deleted node
/// @return     @c true on success, @c false on error (node not in tree)
bool
patricia_remove(
    PatriciaMapT *t,
    const void *key,
    uint16_t bitlen,
    uintptr_t *payload_out)
{
    if (NULL != payload_out) {
        PTSetNodeT const *np = patriset_lookup(&t->tree, key, bitlen);
        if (NULL != np) {
            *payload_out = s2m(np)->payload;
        }
        return patriset_evict(&t->tree, (PTSetNodeT*)np);
    } else {
        return patriset_remove(&t->tree, key, bitlen);
    }
}

// -------------------------------------------------------------------------------------
// ==== Iteration can be fun, actually ;)                                           ====
// -------------------------------------------------------------------------------------
/// @brief set up an iterator
/// @param iter iterator to operate on 
/// @param tree patricia tree owning the nodes
/// @param root root of the subtree to iterate or @c NULL for full tree
/// @param dir  @c true for left-to-right, false for right-to-left
/// @param mode enumeration mode for the nodes
void
ptiter_init(
    PTMapIterT       *iter,
    PatriciaMapT     *tree,
    PTMapNodeT const *root,
    bool              dir ,
    EPTIterMode       mode)
{
    psiter_init(&iter->inner, &tree->tree, m2s(root), dir, mode);
}

/// @brief logical forward step of the iterator
/// @param iter iterator to step
/// @return     next node or NULL if end is reached
const PTMapNodeT*
ptiter_next(
    PTMapIterT *iter)
{
    return s2m(psiter_next(&iter->inner));
}

/// @brief logical backward step of the iterator
/// @param iter iterator to step
/// @return     next node or NULL if end is reached
const PTMapNodeT*
ptiter_prev(
    PTMapIterT *iter)
{
    return s2m(psiter_prev(&iter->inner));
}

/// @brief reset iterator to initial position
/// @param iter iterator to reset
void
ptiter_reset(
    PTMapIterT *iter)
{
    psiter_reset(&iter->inner);
}

// -*- that's all folks -*-