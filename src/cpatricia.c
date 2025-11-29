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

#include "cpatricia.h"

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

// -------------------------------------------------------------------------------------
// ==== tree topology relation helpers                                              ====
// -------------------------------------------------------------------------------------

static inline bool _isParentOf(const PTMapNodeT *const p, const PTMapNodeT *const x) {
    return (p->_m_child[0] == x) | (p->_m_child[1] == x); // bitwise OR is intention
}

static inline unsigned _otherIdx(const PTMapNodeT *const p, const PTMapNodeT *const x) {
    return p->_m_child[0] == x;
}

static inline unsigned _childIdx(const PTMapNodeT *const p, const PTMapNodeT *const x) {
    return p->_m_child[1] == x;
}

// -------------------------------------------------------------------------------------
// ==== memory allocation & helpers                                                 ====
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// default node allocator using 'malloc()'
static void*
alloc_wrap(
    void  *unused,
    size_t bytes )
{
    (void)unused;
    return malloc(bytes);
}

// -------------------------------------------------------------------------------------
// default node deallocator using 'free()'
static void
free_wrap(
    void *unused,
    void *obj   )
{
    (void)unused;
    free(obj);
}

// -------------------------------------------------------------------------------------
// Create a node from a bit string, using the raw memory functions provided
static PTMapNodeT*
ptnode_create(
    const PatriciaMapT *tree,
    const void         *keystr,
    uint16_t            bitlen)
{
    // We count raw key bits -- the trailing NUL in an ASCIIZ string is *not* considered
    // to be part of the key! But for the sake of string processing, we add one NUL byte
    // to the end of the key, without accounting for it in the size.
    // Makes printing much safer, at moderate costs.

    unsigned    bytelen = ((unsigned)bitlen + CHAR_BIT - 1) / CHAR_BIT;
    size_t      nodelen = offsetof(PTMapNodeT, data) + bytelen + 1; // reserve one extra NUL byte
    PTMapNodeT *nodeptr = tree->_m_mfunc->fp_alloc(tree->_m_arena, nodelen);
    if (NULL != nodeptr) {
        memset(nodeptr, 0, offsetof(PTMapNodeT, data));
        nodeptr->nbit = bitlen;
        memcpy(nodeptr->data, keystr, bytelen);
        nodeptr->data[bytelen] = '\0';  // ASCIIZ sentinel
    }
    return nodeptr;
}

// -------------------------------------------------------------------------------------
// Node deallocation helper. Checks if there is a function in the hook -- don't call if
// there is none. In such a case the known part of the node will be filled with a easy
// to recognize pattern. (Some pool/arena based allocation strategies support only bulk
// destruction of the arena, and it's easier to have a NULL here than supplying an empty
// default...)
static void
ptnode_free(
    const PatriciaMapT *tree,
    PTMapNodeT         *node)
{
    if (NULL != node) {
        memset(node, 0xFE, offsetof(PTMapNodeT, data));
        node->data[0] = '\0';
        if (NULL != tree->_m_mfunc->fp_free) {
            tree->_m_mfunc->fp_free(tree->_m_arena, node);
        }
    }
}

// -------------------------------------------------------------------------------------
// null-deleter to do just nothing with the given uintptr_t. internal use only.
static void
dummy_deleter(
    uintptr_t data,
    void     *uarg)
{
    // a NOP delete callback
    (void)data;
    (void)uarg;
}

// -------------------------------------------------------------------------------------
// ==== key access : bit extraction & diff position                                =====
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// byte swapper -- try builtins where possible, do SWAR steps to shuffle the bytes in
// a 'size_t' value otherwise.
static size_t
bswapz(
    size_t v)
{
#if defined(__GNUC__) || defined(_clang)
    switch (sizeof(size_t)) {
    case 2: return __builtin_bswap16((uint16_t)v);
    case 4: return __builtin_bswap32((uint32_t)v);
    case 8: return __builtin_bswap64((uint64_t)v);
    }
#endif
    {
        unsigned bits = sizeof(size_t) * CHAR_BIT;
        size_t   mask = (size_t)-1;
        while (bits > CHAR_BIT) {
            bits >>= 1;
            mask  ^= (mask << bits);
            v = ((v & mask) << bits) | ((v >> bits) & mask);
        }
    }
    return v;
}

// -------------------------------------------------------------------------------------
// Count leading zeros in a 'size_t' value. Resorts to builtins where practicable, uses
// a semi-dumb loop as fallback.
static unsigned
patricia_clz(
    size_t v)
{
    if (0 == v) {
        return sizeof(size_t) * CHAR_BIT;
    }

#if defined(__GNUC__) || defined(__clang)
    if (sizeof(int) >= sizeof(size_t))
        return __builtin_clz((unsigned int)v) - (sizeof(int) - sizeof(size_t)) * CHAR_BIT;
    if (sizeof(long int) >= sizeof(size_t))
        return __builtin_clzl((unsigned long int)v) - (sizeof(long int) - sizeof(size_t)) * CHAR_BIT;
    if (sizeof(long long int) >= sizeof(size_t))
        return __builtin_clzll((unsigned long long int)v) - (sizeof(long long int) - sizeof(size_t)) * CHAR_BIT;
#endif
    {
        static const size_t grpmask = ~(size_t)0 << ((sizeof(size_t) - 1) * CHAR_BIT);
        static const size_t msbmask = ~(size_t)0 << (sizeof(size_t) * CHAR_BIT - 1);

        unsigned n = 0;
        while ( !(v & grpmask)) {
            v <<= CHAR_BIT;
            n += CHAR_BIT;
        }
        while (!(v & msbmask)) {
            v <<= 1;
            n += 1;
        }
        return n;
    }
}

// -------------------------------------------------------------------------------------
// Create an infinite bit stream from a finite buffer. After the last bit the engine
// repeats the complement of the last bit ad infinitum.  The result is always a properly
// filled 'size_t' value for quick processing.
typedef struct bitstream_ {
    const unsigned char   *ptr;     // current read position
    unsigned               bits;    // remaining bits
    bool                   last;    // last bit from key value
} BitStreamT;

static size_t
nextbits(
    BitStreamT *bs)
{
    union {
        size_t        szv;
        unsigned char acv[sizeof(size_t)];
    } accu;

    if (bs->bits >= (sizeof(size_t) * CHAR_BIT)) {
        // full-width copy via union -- might end as unaligned load where possible
        memcpy(accu.acv, bs->ptr, sizeof(size_t));
        bs->ptr += sizeof(size_t);
        bs->bits -= sizeof(size_t) * CHAR_BIT;
    } else if (bs->bits == 0) {
        // bit stream exhausted -- return value with bits complement of the last bit
        accu.szv = (size_t)bs->last - 1u;
    } else {
        // partial load. Needs a bit of care
        unsigned bytes = (bs->bits + CHAR_BIT - 1) / CHAR_BIT; // all bytes to load, incl partial
        unsigned ebits = bs->bits % CHAR_BIT;                  // partial bits or zero for full byte

        memcpy(accu.acv, bs->ptr, bytes); // stream it in...
        memset((accu.acv + bytes), ((unsigned)bs->last - 1u), (sizeof(size_t) - bytes));
        bs->ptr += bytes; // ... and update pointer

        if (ebits) {            // fractional byte used?
            if (bs->last) {         // last bit was set?
                accu.acv[bytes - 1] &= ~((unsigned)UCHAR_MAX >> ebits); // 0-flush
            } else {
                accu.acv[bytes - 1] |= ((unsigned)UCHAR_MAX >> ebits); // 1-flush
            }
        }

        bs->bits = 0; // nothing left -- input completely consumed
    }
    return accu.szv;
}

// -------------------------------------------------------------------------------------
/// @brief get a bit from a bit string, unity indexed
/// Get the n-th bit of the key string, where bit 1 is the first bit. Bits below index 1
/// are considered zero, and bits after the last bit are considered to be the complement
/// of the last bit. (Which has a nice corner case for a key of zero length!)
/// @param base     begin of bit string
/// @param bitlen   length of bit string
/// @param bitidx   unity-based index of bit to extract
/// @return         bit value or extension, see description
///
/// @note public only for unit test purposes
bool
patricia_getbit(
    const void *base  ,
    uint16_t    bitlen,
    uint16_t    bitidx)
{
    bool bit = false, neg = false;
    const uint8_t *bytes = base;

    if (bitidx == 0) {
        return false;
    }

    neg = bitidx > bitlen;
    if (0 == bitlen) {
        return neg; // == (bitidx > 0)
    }

    bitidx = (neg ? bitlen : bitidx) - 1; // clamp & index (unity-based) to offset (zero-based)
    bit = (bytes[bitidx >> 3] >> (~bitidx & 7)) & 1;
    return bit ^ neg;
}

// -------------------------------------------------------------------------------------
/// @brief Given two bit strings, calculate the difference bit position.
/// Get the unity-based index of the first bit where the keys differ. This assumes
/// that the key strings extend logically ad infintum with the complement of the
/// last bit in the key, matching the bit extractor logic.
///
/// Of course this could be written in terms of @c patricia_getbit() but that would
/// be a real performance killer.  This function uses a streaming approach to handel
/// batches of bits efficiently.
/// @param p1 memory base of 1st key
/// @param l1 bit length of 1st key
/// @param p2 memory base of 2nd key
/// @param l2 bit length of 2nd key
/// @return unity-base index of first difference or zero on equality of keys
///
/// @note public only for unit test purposes
uint16_t
patricia_bitdiff(
    const void *p1, uint16_t l1,
    const void *p2, uint16_t l2)
{
    // The following union is a helper to find about the endianess of the target.
    // While formally a RUN TIME check, the tests are actually CONSTEXPR, so the
    // compiler might even be able to make this a COMPILE TIME decision...
    static const union { uint32_t i; unsigned char c[4]; } endian = { .i = 1 };

    // A similar rationale holds for the number of bits in a batch limb:
    static const unsigned limb_bits = sizeof(size_t) * CHAR_BIT;

    uint_least16_t bits = (l2 > l1) ? l2 : l1; // maximum of both lengths
    uint_least16_t bpos = 1;                   // unity-counting

    BitStreamT bs1 = {.ptr = p1, .bits = l1, .last = patricia_getbit(p1, l1, l1)};
    BitStreamT bs2 = {.ptr = p2, .bits = l2, .last = patricia_getbit(p2, l2, l2)};

    for (unsigned words = (bits + limb_bits - 1) / limb_bits; words; --words) {
        size_t accu = (nextbits(&bs1) ^ nextbits(&bs2));    // difference pattern
        if (0 != accu) {                                    // any difference found?
            if (endian.c[0] == 1) {                         // little endian target?
                accu = bswapz(accu);                        // swap bytes in pattern
            }
            return (uint16_t)(bpos + patricia_clz(accu));   // use quick bit counting
        }
        bpos += limb_bits;                                  // prepare for next limb
    }
    // If there's no difference, we have two possibilities: The length of both patterns
    // is equal, in which case we return zero, flagging "equal patterns"; otherwise the
    // difference MUST be after the last bit! 
    return (l1 == l2) ? 0 : bits + 1;
}

// -------------------------------------------------------------------------------------
/// @brief check keys for bitise equality
/// Quick bit stream equality tester. While this could be emulated with the differencer
/// yielding zero, calculating the first bit difference is heavy lifting compared to a
/// simple equality check that will bail out immediately for different key length and
/// use @c memcmp() internally to do a gallopping run over full bytes. 
/// @param p1 memory base of 1st key
/// @param l1 bit length of 1st key
/// @param p2 memory base of 2nd key
/// @param l2 bit length of 2nd key
/// @return @c true if bit strings are equal, @c false otherwise
///
/// @note public only for unit test purposes
bool
patricia_equkey(
    const void *p1, uint16_t l1,
    const void *p2, uint16_t l2)
{
    if (l1 != l2) {
        return false;       // different lengths -> unequal
    }

    const unsigned char *b1 = p1;
    const unsigned char *b2 = p2;
    unsigned bytes = l1 / CHAR_BIT;     // full bytes to compare
    unsigned ebits = l1 % CHAR_BIT;     // extra bits following byte range

    if (memcmp(p1, p2, bytes) != 0) {
        return false;       // memcmp croaks -> unequal
    }

    if (0 != ebits) {
        unsigned char mask = UCHAR_MAX << (CHAR_BIT - ebits);
        if (0 != ((b1[bytes] ^ b2[bytes]) & mask)) {            
            return false;   // mismatch in remaining bits --> unequal
        }
    }

    return true; // exact match
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
    static const PTMemFuncT mf_memfunc = {
        alloc_wrap,
        free_wrap,
        NULL
    };

    memset(t, 0, sizeof(*t));
    if (NULL != fp) {
        t->_m_mfunc = fp;
        t->_m_arena = arena;
    } else {
        t->_m_mfunc = &mf_memfunc;
        t->_m_arena = NULL;
    }
    t->_m_root->_m_child[0] = t->_m_root->_m_child[1] = t->_m_root;
}

// -------------------------------------------------------------------------------------
/// @brief set up a PATRICIA tree with default memory functions
/// @param t        tree to initialise
void
patricia_init(
    PatriciaMapT* t)
{
    patricia_init_ex(t, NULL, NULL);
}

// -------------------------------------------------------------------------------------
/// @brief finalize a PATRICIA tree
/// Destroy all nodes in the tree, with a callback for each payload
///
/// @param t        tree where all nodes should be flushed
/// @param deleter  per-payload callback
/// @param uarg     additional user-owned context data for callback
void
patricia_fini_ex(
    PatriciaMapT *t,
    void (*deleter)(uintptr_t, void*),
    void *uarg)
{
    // Cut tree from root node AASAP
    PTMapNodeT *scan, *list = NULL;
    PTMapNodeT *hold = t->_m_root->_m_child[0];

    t->_m_root->_m_child[0] = t->_m_root->_m_child[1] = t->_m_root;

    // -- force the rightmost leaf to ROOT ---------------------------------------------
    // This is needed ONCE to ensure we have an unambigeous termination condition for
    // the funnel; the bit-position relation will be detroyed on the right subtrees, so
    // we have to set a simple sentinel. The root node is convenient.
    scan = hold;
    while (scan->_m_child[1]->_m_bpos > scan->_m_bpos) {
        scan = scan->_m_child[1];
    }
    scan->_m_child[1] = t->_m_root;

    // -- flatten the tree to a list ---------------------------------------------------
    // Squeezing the tree through a funnel to create a single-linked list of nodes is
    // the only iterative O(N) solution for cleaning up a Sedgewick-style PATRICIA tree.
    // The funnel trick works best for trees with NULL-leaf pointers, but using it here
    // is just more constrained: We MUST NOT delete nodes that went to the funnel
    // immediately, or we would end with dangling uplink pointers. Instead we collect
    // the funnelled nodes on a list after setting their branch position to zero.

    while (t->_m_root != hold) {               // check for sentinel set above
        PTMapNodeT *next = hold->_m_child[0];  // never NULL, subtree intact
        PTMapNodeT *tail = hold->_m_child[1];  // never NULL, but degraded by funnel
        if (next->_m_bpos <= hold->_m_bpos) {
            // left _m_child is an uplink -- continue through the right _m_child next
            next = tail;
        } else {
            // There IS a left _m_child (or even subtree). We graft our tail to the the
            // rightmost link on the right spine of the left subtree, efecctively
            // funnelling the nodes into a sequence.  Every node is visited at most
            // twice, so the whole decomposition is in O(N), even if it might not look
            // like it at first glance.
            scan = next;
            while (scan->_m_child[1]->_m_bpos > scan->_m_bpos) {
                scan = scan->_m_child[1];
            }
            scan->_m_child[1] = tail;
        }
        // Now push node to list of dead nodes, ensuring it will be considered as an
        // uplink node when inspected again during later flattening steps.
        hold->_m_bpos = 0;         // make sure remaining references are seen as uplink
        hold->_m_child[0] = list;  // push to head of the dead-node list
        list = hold;

        // update point-of-interest for next round
        hold = next;
    }

    // -- finally freeing the nodes from the list --------------------------------------
    while (NULL != (hold = list)) {
        list = hold->_m_child[0];                      // pop head from list
        (*deleter)(hold->payload, uarg);
        memset(hold, 0, offsetof(PTMapNodeT, data)); // purge node; paranoia rulez!
        ptnode_free(t, hold);
    }
    if (NULL != t->_m_mfunc->fp_kill) {
        (*t->_m_mfunc->fp_kill)(t->_m_arena);
    }
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
    patricia_fini_ex(t, dummy_deleter, NULL);
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
    // This is not-quite-from-the-textbook implementation that tries to minimise pointer
    // access.

    const PTMapNodeT *node = t->_m_root->_m_child[0];
    unsigned npos, opos = t->_m_root->_m_bpos;
    while ((npos = node->_m_bpos) > opos) {
        opos = npos;
        node = node->_m_child[patricia_getbit(key, bitlen, node->_m_bpos)];
    }
    return patricia_equkey(key, bitlen, node->data, node->nbit) ? node : NULL;
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
    // This is not-quite-from-the-textbook implementation that tries to minimise pointer
    // access. It is also a forward-scan processing algorithm that tries to find
    // candidates on the way down, remembering the last successful match of a key.

    const PTMapNodeT *best = NULL, *node = t->_m_root->_m_child[0];
    unsigned npos, opos = t->_m_root->_m_bpos;
    while ((npos = node->_m_bpos) > opos) {
        if ((node->nbit <= bitlen) && patricia_equkey(key, node->nbit, node->data, node->nbit)) {
            best = node;
        }
        opos = npos;
        node = node->_m_child[patricia_getbit(key, bitlen, node->_m_bpos)];
    }
    return patricia_equkey(key, node->nbit, node->data, node->nbit) ? node : best;
}

// -------------------------------------------------------------------------------------
/// @brief  create node with given key & payload, insert into tree
/// @param t        tree to insert into
/// @param key      key data storage
/// @param bitlen   number of bits in key
/// @param payload  payload to set in key
/// @param inserted opt. storage for 'node created' flag
/// @return         node with matching key (new or existing) or @c NULL on error
const PTMapNodeT *
patricia_insert(
    PatriciaMapT *t,
    const void *key,
    uint16_t bitlen,
    uintptr_t payload,
    bool *inserted)
{
    // we do some pointer tracking here, but this time we do it with two pointers: we
    // need them both for the insert position!

    PTMapNodeT *last, *next;
    last = t->_m_root;
    next = t->_m_root->_m_child[0];
    while (next->_m_bpos > last->_m_bpos) {
        last = next;
        next = last->_m_child[patricia_getbit(key, bitlen, last->_m_bpos)];
    }
    // We have to make a trade-off here: If we assume that duplicates are rare, we can
    // simply calculate the 1st diff bitr position (potentially expensiv) and return the
    // the node if there's no difference.  If OTOH duplicates are common, it's cheaper
    // on average to test for equality for quick bail-out and do the heavy lifting only
    // if it's really needed.  We take the 2nd option here!
    if (patricia_equkey(key, bitlen, next->data, next->nbit)) {
        if (inserted) {
            *inserted = false;
        }
        return next; // existing node
    }

    // Ok, time to get serious... We NEED the branch position!
    unsigned bpos = patricia_bitdiff(key, bitlen, next->data, next->nbit);
    assert(0 != bpos);

    // Obviously, we need to create a new node -- which may fail, of course.
    PTMapNodeT *node = ptnode_create(t, key, bitlen);
    if (NULL == node) {
        // Darn. Game Over, player one!
        if (inserted) {
            *inserted = false;
        }
        return node; // existing node
    }
    node->_m_bpos = bpos;
    node->payload = payload;

    // Find insert parent -- another walk, but this time depth-limited by thenew branch
    // position we calculated.
    bool pdir = false;
    last = t->_m_root;
    next = t->_m_root->_m_child[0];
    while ((next->_m_bpos > last->_m_bpos) && (next->_m_bpos < bpos)) {
        last = next;
        pdir = patricia_getbit(key, bitlen, last->_m_bpos);
        next = last->_m_child[pdir];
    }

    // Link node between last (parent) and next (a _m_child -- or uplink!) Note that our own key
    // bit at the branch position defines which of the link point back to the new node
    // itself; the _m_child link from the parent goes into the other slot.
    bool ndir = patricia_getbit(key, bitlen, bpos);
    node->_m_child[ ndir] = node;
    node->_m_child[!ndir] = next;

    // Now we link the new node into the parent node. We remembered where to do that.
    last->_m_child[pdir] = node;

    // Ok, that was a real success...
    if (inserted) {
        *inserted = true;
    }
    return node;
}

// -------------------------------------------------------------------------------------
// ==== Deletion by key or node pointer                                             ====
// -------------------------------------------------------------------------------------

// Deletion itself is simple once you understand WHAT to aand HOW to get the node
// pointers involved.  In any case it make sense to do a simple full walk from tne root
// down to the leaf, but when compared with the simple lookup/prefix trackers we now
// register 4 different pointers.  But once this is done, replacement becomes rather
// succinct.  Looks very arcane, though.

// struct holding the result of a full tracked tree walk
typedef struct {
    PTMapNodeT *npar;   // true DOWNWARD link parent
    PTMapNodeT *over;   // grandpa in treewalk (node visited before last)
    PTMapNodeT *last;   // last node before current one
    PTMapNodeT *node;   // final/current node in iteration
} NodeLinksT;

// do a tree walk from root to node with root reached from the uplink, while also
// registering the downlink parent of node while going down.
static bool
_pwalk(
    NodeLinksT       * const out ,
    const PTMapNodeT * const root,
    const PTMapNodeT * const node)
{
    const PTMapNodeT *over = root, *last = root, *next = root->_m_child[0];

    if ((NULL == node) || (root == node)) {
        return false;
    }

    while (next->_m_bpos > last->_m_bpos) {
        if (node == next) {
            out->npar = (PTMapNodeT*)last;
        }
        over = last;
        last = next;
        last = next;
        next = next->_m_child[patricia_getbit(node->data, node->nbit, next->_m_bpos)];
    }
    out->node = (PTMapNodeT*)next;
    assert((over->_m_child[0] == last) || (over->_m_child[1] == last));
    assert((last->_m_child[0] == next) || (last->_m_child[1] == next));
    assert(node == out->node);

    out->over = (PTMapNodeT *)over;
    out->last = (PTMapNodeT *)last;
    out->node = (PTMapNodeT *)next;
    return (node == next);
}

// -------------------------------------------------------------------------------------
// remove a node by a previous walk result.
//
// 'walk->node' is the node to remove, 'walk->last' the previous node encountered on the
// way to 'walk->node', and 'walk->over' was the node visted on the way down to
// 'walk->last'. Since only the path from 'last' to 'node' is not a downlink, 'over'
// is the true parent of last.
//
// See "https://algs4.cs.princeton.edu/code/edu/princeton/cs/algs4/PatriciaST.java.html"
//
// The major difference is that this implementation does not use bit tests on the key to
// decide whether left or right child connections must be changed; since we can evaluate
// the topological relationships (is parent or left/right child) we can rearrange the
// nodes based on topological decisions.  That's bad enough already ;)
//
// =====================================================================================
// Remove a node from a Sedgewick-style PATRICIA tree with threaded uplinks.
//
// Deletion in this representation is purely a topological operation.  No key copying,
// no bit tests are performed, and no subtree rotations occur.  Every node has:
//
//   - two child pointers, each either a downlink or a threaded uplink
//   - a branch position bpos that determines its discriminating bit
//
// On search descent, only edges for which child->bpos > parent->bpos are downlinks; all
// others are uplinks.  This invariant allows us to distinguish structural tree edges
// from threaded edges without examining key bits.
//
// The tree walk records exactly three consecutive nodes on the search path:
//
//       g  ->  p  ->  x
//
// where:
//   g  = node visited before p   ("grandparent")
//   p  = node visited before x   ("parent on path", may be x itself)
//   x  = node matching the key   ("node to remove")
//
// In addition, the walk records:
//   z  = the true downward parent of x (i.e., the unique node whose child[]
//        points to x *as a downlink*).  This is needed only when x != p.
//
// The deletion has exactly two logical steps.  These steps cover both cases:
//   (1) trivial case: p == x  (x has a self-link and is its own predecessor)
//   (2) general case: p != x  (p is the predecessor that replaces x)
//
// -------------------------------------------------------------------------------------
// STEP I  –  Bypass (g -> p -> x) by linking g directly to the survivor.
//
// Between p and x, one pointer is the path edge down to x, and the other pointer is
// the surviving subtree (or threaded link).  By construction:
//
//     survivor = p->child[_otherIdx(p, x)]
//
// which correctly yields the non-x child even in the self-link case:
//
//     if p == x:
//         p->child[0] == p  => index=1 (take child[1])
//         p->child[1] == p  => index=0 (take child[0])
//
// The edge g->child[*] that pointed to p is rewired to this survivor.  This removes p
// from the search path regardless of whether p == x.
//
// -------------------------------------------------------------------------------------
// STEP II — Replace x with p in the true tree (only if x != p).
//
// If p != x, then p is the predecessor that must occupy x's position in the tree.
// The true downward parent z is the only node whose child[] must be updated:
//
//       z->child[_childIdx(z, x)] = p
//
// Then p adopts both children and the branch position of x.  This preserves the PATRICIA
// invariants:
//
//   - discriminator bits remain correct
//   - downlink/uplink topology is preserved
//   - threaded edges still point to the correct nodes
//
// When p == x, z does not exist (x's only parent is the threaded parent g) and no
// replacement is needed; Step I fully specifies the topology change.
//
// -------------------------------------------------------------------------------------
// After these two rewires (one always, one conditionally), no other structure in the
// tree changes.  All invariants of the PATRICIA representation remain valid, and the
// removed node x may be safely freed.
//
// To understand WHY that's all that must be done may take longer than writing it up ;)
static void
_evict(
    PatriciaMapT     * const t   ,
    const NodeLinksT * const walk)
{
    PTMapNodeT *x = walk->node;
    PTMapNodeT *p = walk->last;
    PTMapNodeT *g = walk->over;
    assert(_isParentOf(p, x));
    assert(_isParentOf(g, p));

    (void)_isParentOf;    // only used in DEBUG build assertions

    // Step I: In all cases, we have to bypass 'p' in the path 'g' -> 'p' -> 'x'.
    g->_m_child[_childIdx(g, p)] = p->_m_child[_otherIdx(p, x)];

    // Step II: IF 'x' != 'p', replace 'x' with 'p' in the tree. This needs access
    // the downward link to 'x', which we have registered on our way down to 'p'.
    if (x != p) {
        PTMapNodeT *z = walk->npar;
        assert(_isParentOf(z, x)); // true downlink parent

        // replace the link to 'x' in 'z' with 'p'
        z->_m_child[_childIdx(z, x)] = p;

        // re-link 'p' with the children of 'x' and copy the branch position
        p->_m_child[0] = x->_m_child[0];
        p->_m_child[1] = x->_m_child[1];
        p->_m_bpos = x->_m_bpos;
    }

    memset(x, 0, offsetof(PTMapNodeT, data)); // purge node; paranoia rulez!
    ptnode_free(t, x);
}

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
    NodeLinksT nodes;
    if (_pwalk(&nodes, t->_m_root, node)) {
        _evict(t, &nodes);
        return true;
    }
    return false;
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
    NodeLinksT nodes;
    if (_pwalk(&nodes, t->_m_root, patricia_lookup(t, key, bitlen))) {
        if (payload_out) {
            *payload_out = nodes.node->payload;
        }
        _evict(t, &nodes);
        return true;
    }
    return false;
}

// -------------------------------------------------------------------------------------
// ==== showing tree as crude indented text (strring keys assumed)                  ====
// -------------------------------------------------------------------------------------
static void
fprint_tree(
    FILE *ofp, const PTMapNodeT * const node, unsigned level, unsigned flags)
{
    if (0 == flags) {
        for (unsigned i = 0; i < level; ++i)
            fputs("    ", ofp);
        fprintf(ofp, "+--(%p|%zu)--> '%s(%u)'\n", (void*)node, node->payload, node->data, node->_m_bpos);
    } else {
        if (flags & 2)
            fprint_tree(ofp, node->_m_child[1], (level + 1), (node->_m_child[1]->_m_bpos > node->_m_bpos ? 3 : 0));
        for (unsigned i = 0; i < level; ++i)
            fputs("    ", ofp);
        fprintf(ofp, "[%2u, %p] \n", node->_m_bpos, (void *)node);
        if (flags & 1)
            fprint_tree(ofp, node->_m_child[0], (level + 1), (node->_m_child[0]->_m_bpos > node->_m_bpos ? 3 : 0));
    }
}

/// @brief dump a tree as crude indented text
/// @param ofp  where to write to
/// @param pmap what to dump
/// @note assumes key data is NUL-terminated ASCII text -- be prepared for fun if it's not!
void
patricia_print(
    FILE               *ofp ,
    PatriciaMapT const *pmap)
{
    fprint_tree(ofp, pmap->_m_root->_m_child[0], 0, 3);
}

// -------------------------------------------------------------------------------------
// ==== Iteration can be fun, actually ;)                                           ====
// -------------------------------------------------------------------------------------
// The description below was for regular depth-first, left-to-right traversal.  Doing a
// right-to-left traversal is simple enough, just mirroring the procedure by flipping
// the mapping fro 1st/2nd child to the actual child slot index!
// This can be expressed by logical bit-flipping transformations, and the step function
// makes heavy use of these properties: One function that handles both directions as
// well as all three types of enumeration with minimal fuzz, forward and backward.
// But it may need careful reading to see that it really covers it all. 
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// Continuation save points for resumable iteration
// -------------------------------------------------------------------------------------
// To allow the traversal to pause and resume, the iterator exposes a compact
// "continuation state" containing:
//
//     * The current `node` pointer.
//     * The `dir` (DOWN, LEFTUP, RIGHTUP).
//     * The parent stack (only necessary ancestors).
//
// A save point may be taken at exactly the following program points:
//
//   * Immediately after PRE dispatch (fresh entry into a node).
//   * Immediately after IN dispatch (left subtree completed).
//   * Immediately after POST dispatch (whole subtree completed).
//
// Between these points, the FSM performs internal transitions that should not be
// externally exposed.  Saving only at the dispatch boundaries ensures that no partial
// subtree work must be reconstructed when resuming.
//
// Restoring a continuation reinstates the FSM invariants exactly as if no time had
// passed.  The only requirement is that the tree structure itself remains unchanged
// (no insertions or deletions) while the traversal is paused.
//
// If you want to do deletion while iterating, POST-ORDER traversal is actually required.
// Since deletion only changes the subtree rooted at the node to delete, going to the
// successor befor delting a node ensures no nodes are missed during deletion, since the
// sub-tree rooted at the node to delete will never be visited again during iteration.
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// Parent-stack discipline
// -------------------------------------------------------------------------------------
// A bounded parent stack supports upward movement without storing parent links inside
// nodes.  The stack contains only ancestors whose right subtree may yet need traversal.
//
// Invariants:
//   * When descending (dir == DOWN) push the current node only if its right subtree
//     still must be explored later.
//   * When returning from a left child, we will check the right sibling.
//   * When returning from a right child, the parent is finished and may be popped.
//
// After popping, a parent pointer is never reused; the stack never retains a "dead"
// ancestor whose right subtree is done.  This makes pushes/pops amortized O(1) and
// avoids redundant pushes.
//
// Since the stack is bounded, pushing into a full stack ejects the oldest node.  This
// must be recovered by a new tree traversal, filling the stack with parent nodes from
// 'higher' nodes.  The bigger the bound, the fewer recovery walks have to be done.
// A stack size of 8 requires a recovery load every 256 steps, while 16 requires a
// reload after 65536 steps.
// -------------------------------------------------------------------------------------

static const PTMapNodeT*
iter_child(
    PTMapNodeT const *node,
    bool              dir )
{
    if (NULL != node) {
        PTMapNodeT *next = node->_m_child[dir];
        node = (node->_m_bpos < next->_m_bpos) ? next : NULL;
    }
    return node;
}

static void
iter_parentPush(
    PTMapIterT       *iter, 
    PTMapNodeT const *node)
{
    static const unsigned pstkSize = sizeof(iter->_m_pstk) / sizeof(*iter->_m_pstk);

    iter->_m_pstk[iter->_m_stkTop] = node;
    iter->_m_stkTop = (iter->_m_stkTop + 1) & (pstkSize - 1);
    iter->_m_stkLen += (iter->_m_stkLen < pstkSize);
}

static const PTMapNodeT *
iter_parentPop(
    PTMapIterT       *iter,
    PTMapNodeT const *node)
{
    static const unsigned pstkSize = sizeof(iter->_m_pstk) / sizeof(*iter->_m_pstk);
    
    const PTMapNodeT *last, *next;

    // try to pop nod from stack first
    while (0 != iter->_m_stkLen) {
        --iter->_m_stkLen;
        iter->_m_stkTop = (iter->_m_stkTop - 1) & (pstkSize - 1);
        next = iter->_m_pstk[iter->_m_stkTop];
        if (_isParentOf(next, node) && (next->_m_bpos < node->_m_bpos)) {
            return next;
        }
    }

    // The parent of the root does not exist in our context.  We find this out anyway
    // by a full tree walk below, but the check is cheap compared to a full walk.
    if (node == iter->_m_root) {
        return NULL;
    }

    // stack exhausted. Walk down the tree and register parents on the way down
    last = iter->_m_root;
    next = last->_m_child[patricia_getbit(node->data, node->nbit, last->_m_bpos)];
    while ((next != node) && (next->_m_bpos > last->_m_bpos)) {
        iter_parentPush(iter, last);
        last = next;
        next = last->_m_child[patricia_getbit(node->data, node->nbit, last->_m_bpos)];
    }

    // We really should have ended at 'node' here, but if we don't, flag failure!
    if ((next != node) || (next->_m_bpos <= last->_m_bpos)) {
        iter->_m_stkLen = 0;
        return NULL;
    }
    return last;
}

// -------------------------------------------------------------------------------------
// Traversal FSM invariants
// -------------------------------------------------------------------------------------
// The iterator implements a left-to-right depth-first traversal over a static binary
// tree.  The FSM maintains exactly one "cursor" pointer (`node`) and one "direction"
// indicator (`dir`) describing the most recent movement.
//
//   HEAD  : We're BEFORE the first reachable node (init state)
//   DOWN  : we just descended into `node` from its parent.
//   C1-UP : returned to `node` from first in-order subtree.
//   C2-UP : returned to `node` from second in-order subtree.
//   TAIL  : we're AFTER the last reachable node (end state)
//
// The meaning of `dir` is historical: it encodes how we arrived at the current node,
// not what we intend to do next. (And the current node is NULL for HEAD/TAIL!)
//
// The FSM preserves these invariants:
//   * If dir == DOWN, `node` is freshly entered; no children were visited.
//   * If dir == C1-UP, the entire first subtree has been traversed; second is unvisited.
//   * If dir == C2-UP, both subtrees are complete and we will ascend.
//
// No state encodes a partially-processed subtree.  All transitions go to a sibling
// subtree, to a parent, or to a newly-entered node. (Or drop out of the tree, finally.)
// -------------------------------------------------------------------------------------
typedef enum {  // node was entered from ...
    iDir_head,  // ... before first reachable node
    iDir_down,  // ... parent node, way down
    iDir_upC1,  // ... first in-order child node
    iDir_upC2,  // ... second in-order child
    iDir_tail   // ... from last reachable node
} EWayIn;

typedef enum {  // leave node with ...
    oDir_root,  // ... root of tree
    oDir_dnC1,  // ... first in-order child
    oDir_dnC2,  // ... second in-order child
    oDir_up,    // ... parent of node
    oDir_null   // ... NULL node
} EWayOut;

typedef struct {
    uint8_t odir : 3;   
    uint8_t idir : 3;
    uint8_t mode : 2;
} IterTransT;

typedef IterTransT IterTableT[1 + iDir_tail - iDir_head];

// transition table for stepping forward
static const IterTableT fwdTable = {
    /* iDir_head */ {oDir_root, iDir_tail, -1               },
    /* iDir_down */ {oDir_dnC1, iDir_upC1, ePTMode_preOrder },
    /* iDir_upC1 */ {oDir_dnC2, iDir_upC2, ePTMode_inOrder  },
    /* iDir_upC2 */ {oDir_up  , iDir_tail, ePTMode_postOrder},
    /* iDir_tail */ {oDir_null, iDir_tail, -1               }
};

// transition table for stepping backward
static const IterTableT revTable = {
    /* iDir_head */ {oDir_null, iDir_head, -1               },
    /* iDir_down */ {oDir_dnC2, iDir_upC2, ePTMode_postOrder},
    /* iDir_upC1 */ {oDir_up  , iDir_head, ePTMode_preOrder },
    /* iDir_upC2 */ {oDir_dnC1, iDir_upC1, ePTMode_inOrder  },
    /* iDir_tail */ {oDir_root, iDir_head, -1               }
};

// the stpping function
static const PTMapNodeT*
iter_step(
    PTMapIterT *iter,
    const IterTableT ttable)
{
    bool              yield;
    EWayOut           odir;
    EWayIn            idir = iter->_m_state;
    PTMapNodeT const *next = iter->_m_nodep, *last = NULL;

    do {
        last = next;

        yield = ttable[idir].mode == iter->_m_mode;
        odir  = ttable[idir].odir;
        idir  = ttable[idir].idir;  // failure default -- normally replaced below

        switch (odir) {
        case oDir_root:
            next = iter->_m_root;
            if (NULL != next) {
                iter->_m_stkLen = 0;
                iter->_m_stkTop = 0;
                idir = iDir_down;
            }
            break;

        case oDir_dnC1:
        case oDir_dnC2:
            next = iter_child(last, ((odir == oDir_dnC2) == iter->_m_dir));
            if (NULL != next) {
                iter_parentPush(iter, last);
                idir = iDir_down;
            } else {
                next = last;    // we shift state, but NOT the position!
            }
            break;

        case oDir_up:
            next = iter_parentPop(iter, last);
            if (NULL != next) {
                idir = (last == next->_m_child[iter->_m_dir]) ? iDir_upC2 : iDir_upC1;
            }
            break;

        case oDir_null:
            next = NULL;
            break;
        }
    } while (!yield && (odir != oDir_null));

    iter->_m_nodep = next;
    iter->_m_state = idir;
    return last;
}

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
    memset(iter, 0, sizeof(*iter));
    iter->_m_tree  = tree;
    iter->_m_root  = root ? root : iter_child(tree->_m_root, 0);
    iter->_m_dir   = dir;
    iter->_m_mode  = mode;
    iter->_m_state = iDir_head;
}

/// @brief logical forward step of the iterator
/// @param iter iterator to step
/// @return     next node or NULL if end is reached
const PTMapNodeT*
ptiter_next(
    PTMapIterT *iter)
{
    return iter_step(iter, fwdTable);
}

/// @brief logical backward step of the iterator
/// @param iter iterator to step
/// @return     next node or NULL if end is reached
const PTMapNodeT*
ptiter_prev(
    PTMapIterT *iter)
{
    return iter_step(iter, revTable);
}

/// @brief reset iterator to initial position
/// @param iter iterator to reset
void
ptiter_reset(
    PTMapIterT *iter)
{
    iter->_m_state  = iDir_head;
}

// -------------------------------------------------------------------------------------
// ==== Creating graphviz DOT files is easy with iteration working                  ====
// -------------------------------------------------------------------------------------

static void
_2dot_edges(FILE* ofp, PTMapNodeT const *node)
{
    PTMapNodeT const *next;
    for (int idx = 0; idx < 2; ++idx) {
        next = node->_m_child[idx];
        if (next->_m_bpos > node->_m_bpos) {
            fprintf(ofp, "  N%p:s%c -> N%p;\n", (void *)node, "we"[idx], (void *)next);
        } else if (next == node) {
            fprintf(ofp,
                    "  N%p:n%c -> N%p:s%c [constraint=false,color=red];\n",
                    (void *)node, "we"[idx],
                    (void *)next, "we"[idx]);
        } else {
            fprintf(ofp, "  N%p:n%c -> N%p [constraint=false,color=red];\n", (void *)node, "we"[idx], (void *)next);
            //fprintf(ofp, "  N%p -> N%p [constraint=false,color=red];\n", (void *)node, (void *)next);
        }
    }
}

static bool
node2label(FILE* ofp, const PTMapNodeT* node)
{
    unsigned char uch;
    const char   *scp;
    fprintf(ofp, "[%hu]", (unsigned short)node->_m_bpos);
    for (scp = node->data; 0 != (uch = *scp); ++scp) {
        if (uch == '"') {
            fputs("\\\"", ofp);
        } else if (uch < ' ') {
            fprintf(ofp, "\\%03hho", uch);
        } else {
            fputc(uch, ofp);
        }
    }
    return true;
}

bool
patricia_todot(
    FILE *ofp,
    PatriciaMapT const *tree,
    bool (*label)(FILE*, const PTMapNodeT*))
{
    PTMapIterT        iter;
    PTMapNodeT const *node;
   
    if (NULL == label) {
        label = node2label;
    }
    ptiter_init(&iter, (PatriciaMapT*)tree, NULL, true, ePTMode_preOrder);
    fputs("digraph G {\n", ofp);

    fprintf(ofp, "  N%p [label=\"R\",shape=doublecircle,style=filled];\n", (void *)tree->_m_root);
    _2dot_edges(ofp, tree->_m_root);

    while (NULL != (node = ptiter_next(&iter))) {
        fprintf(ofp, "  N%p [label=\"", (void *)node);
        label(ofp, node);
        fputs("\";\n", ofp);
        _2dot_edges(ofp, node);
    }
    fputs("}\n", ofp);
    return true;
}

// -*- that's all folks -*-