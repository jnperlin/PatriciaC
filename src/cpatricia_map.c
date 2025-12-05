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

// -------------------------------------------------------------------------------------
// helpers to adjust pointers ein both directions: map to set, set to map

static inline PTMapNodeT *s2m(const PTSetNodeT *const np) {
    return (NULL != np) ? (PTMapNodeT *)((char *)np - offsetof(PTMapNodeT, _m_node)) : NULL;
}

static inline PTSetNodeT *m2s(const PTMapNodeT *const np) {
    return (NULL != np) ? (PTSetNodeT*)&np->_m_node : NULL;
}

// -------------------------------------------------------------------------------------
// default node allocator using 'malloc()'
static void*
alloc_wrap(
    void  *unused,
    size_t bytes )
{
    // We assume that malloc handles the alignment stuff on a structure without special
    // help from us.  But this is actually the place where you can start to play all the
    // dirty tricks you need if you go for arena or pool based allocation...

    PTMapNodeT *ptr = malloc(bytes + offsetof(PTMapNodeT, _m_node));
    if (NULL != ptr) {
        // initialise the payload here
        ptr->payload = 0;
    }
    return m2s(ptr);
    (void)unused;
}

// -------------------------------------------------------------------------------------
// default node deallocator using 'free()'
static void
free_wrap(
    void *unused,
    void *obj   )
{
    PTMapNodeT *ptr = s2m(obj);
    if (NULL != ptr) {
        // cleanup paload here
        ptr->payload = 0;
        free(ptr);
    }
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
patrimap_init_ex(
    PatriciaMapT     *t,
    const PTMemFuncT *fp,
    void             *arena)
{
    patriset_init_ex(&t->_m_set, fp, arena);
}

// -------------------------------------------------------------------------------------
/// @brief set up a PATRICIA tree with default memory functions
/// @param t        tree to initialise
void
patrimap_init(
    PatriciaMapT* t)
{
    static const PTMemFuncT mf_memfunc = {
        alloc_wrap,
        free_wrap,
        NULL
    };
    patriset_init_ex(&t->_m_set, &mf_memfunc, NULL);
}

// -------------------------------------------------------------------------------------
/// @brief finalize a PATRICIA tree
/// Destroy all nodes in the tree, without doing anything special to manage the payload
///
/// @param t        tree where all nodes should be flushed
void
patrimap_fini(
    PatriciaMapT *t)
{
    patriset_fini(&t->_m_set);
}

// -------------------------------------------------------------------------------------
/// @brief  lookup (exact match) for a key in the patricia tree
/// @param t        tree to search
/// @param key      storage of key bits
/// @param bitlen   number of key bits
/// @return         node with exact matcing key or @c NULL
const PTMapNodeT *
patrimap_lookup(
    const PatriciaMapT *t,
    const void *key,
    uint16_t bitlen)
{
    return s2m(patriset_lookup(&t->_m_set, key, bitlen));
}

// -------------------------------------------------------------------------------------
/// @brief longest prefix match for a key in the patricia tree
/// @param t        tree to search
/// @param key      storage of key bits
/// @param bitlen   number of key bits
/// @return         node with non-empty longest prefix key or @c NULL
const PTMapNodeT *
patrimap_prefix(
    const PatriciaMapT *t,
    const void *key,
    uint16_t bitlen)
{
    return s2m(patriset_prefix(&t->_m_set, key, bitlen));
}

// -------------------------------------------------------------------------------------
/// @brief  create node with given key & payload, insert into tree
/// @param t        tree to insert into
/// @param key      key data storage
/// @param bitlen   number of bits in key
/// @param inserted opt. storage for 'node created' flag
/// @return         node with matching key (new or existing) or @c NULL on error
const PTMapNodeT *
patrimap_insert(
    PatriciaMapT *t,
    const void *key,
    uint16_t bitlen,
    bool *inserted)
{
    return s2m(patriset_insert(&t->_m_set, key, bitlen, inserted));
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
patrimap_evict(
    PatriciaMapT *t,
    PTMapNodeT *node)
{
    return patriset_evict(&t->_m_set, m2s(node));
}

// -------------------------------------------------------------------------------------
/// @brief remove a node by key, optionally yielding the payload
/// @param t        tree owning the node
/// @param key      key data storage
/// @param bitlen   number of bits in key
/// @return     @c true on success, @c false on error (key not in tree)
bool
patrimap_remove(
    PatriciaMapT *t,
    const void *key,
    uint16_t bitlen)
{
    return patriset_remove(&t->_m_set, key, bitlen);
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
pmapiter_init(
    PTMapIterT       *iter,
    PatriciaMapT     *tree,
    PTMapNodeT const *root,
    bool              dir ,
    EPTIterMode       mode)
{
    psetiter_init(&iter->_m_inner, &tree->_m_set, m2s(root), dir, mode);
}

/// @brief logical forward step of the iterator
/// @param iter iterator to step
/// @return     next node or NULL if end is reached
const PTMapNodeT*
pmapiter_next(
    PTMapIterT *iter)
{
    return s2m(psetiter_next(&iter->_m_inner));
}

/// @brief logical backward step of the iterator
/// @param iter iterator to step
/// @return     next node or NULL if end is reached
const PTMapNodeT*
pmapiter_prev(
    PTMapIterT *iter)
{
    return s2m(psetiter_prev(&iter->_m_inner));
}

/// @brief reset iterator to initial position
/// @param iter iterator to reset
void
pmapiter_reset(
    PTMapIterT *iter)
{
    psetiter_reset(&iter->_m_inner);
}

// -*- that's all folks -*-