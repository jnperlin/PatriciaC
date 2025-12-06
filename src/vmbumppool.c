// -------------------------------------------------------------------------------------
// Virtual memory backed, page-on-demand bump allocator
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------

// =====================================================================================
/* --*-- MEMORY ALLOCATION STRATEGY FOR THE ARENA --*--

The basic idea is to reserve large blocks of the virtual address space first, without
committing memory to them. Pages will be committed on demand when allocation rquires
more core memory.

Pros:
 + Unused space in the pool is just a hole in the address space.
 + Pointers into the pool have the same lifetime as the pool.

Cons:
 + Oversized reservations can cause contention on 32bit systems. (unlikely with 64bit!)

Windows Virtual memory funktions map 1:1 to what we need; for Linux and BSD, we have to
do it slightly different.

Windows:
 + VirtualAlloc() will be used to reserve address space with no access rights
 + VirtualAlloc() with specific addresses will be used to commit pages
 + VirtualFree() can discard the whole mapping

Linux:
 + mmap() with PROT_NONE and MAP_NORESERVE will be used to reserve address space without
   actually allocating memory pages and swap space
 + mprotect() will be used to change access to individual pages on demand, and madvise()
   will be used to ensure that swap space is properly allocated. If madvise() shouldn't
   be used, mmap(...,MAP_FIXED,...) will be used.
 + munmap() will be used to discard the whole mapping

BSD:
 + mmap() with PROT_NONE and MAP_GUARD will be used to reserve address space without
   actually allocating memory pages and swap space
 + mmap() with MAP_FIXED will be used to replace pages in the guard mapping by mappings
   that are backed by swap space and are properly accessible
 + munmap() will be used to discard the whole mapping

While mmap(...,MAP_FIXED,...) can also be used under Linux, the combination of mprotect()
and madvise() should be more effcient and less error-prone.  It just doesn't work
everywhere, so it might get runtime-disabled after the first failed attempt...
*/
// =====================================================================================
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <assert.h>
#include "vmbumppool.h"

#if defined(__unix__)
# include <unistd.h>
#elif defined(_WIN32)
# include <windows.h>
#endif

// -------------------------------------------------------------------------------------
/// @brief virtual memory management page size
///
/// We have to adjust pointers and sizes to page-aligned values. We assume 4kB
/// in the beginning, but that value should be properly adjusted during
/// startup automagically.
static size_t s_pagesize = 4096;

void
vmBump_StaticSetup(void)
{
# if defined(__unix__) && defined(_SC_PAGESIZE)
    s_pagesize = (size_t)sysconf(_SC_PAGESIZE);
# elif defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s_pagesize = si.dwPageSize;
# endif
}

// -------------------------------------------------------------------------------------
#if defined(__clang__) || defined(__GNUC__)

static void __attribute__((constructor(101))) _startup_trampoline(void) { vmBump_StaticSetup(); }

#elif defined(_MSC_VER)

#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) static void(__cdecl *_fp_ensure_pagesize)(void) = vmBump_StaticSetup;

#else

# warning "vmBump_StaticSetup" does not run automatically

#endif
// -------------------------------------------------------------------------------------

// =====================================================================================
#if defined(__linux__) || defined(_lint)    // Linux/POSIX specifc VMEM core functions
// =====================================================================================

#include <sys/mman.h>   // mmap, munmap, mprotect, madvise

// -------------------------------------------------------------------------------------
// syscall / low-level functions to manage raw virtual memory
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
/// @brief reserve a memory area in the virtual address space
///
/// The memory area will be inaccessible after creation with no pages mapped.  Note that
/// we do _not_ reserve space in the swap space -- this would defeat the whole purpose of
/// the arena!
///
/// @param[OUT] paddr   where to store address of area
/// @param      len     length of memory area, will be rounded up to next page size
/// @return     0 onsuccess, else @c errno value
static int
_arena_reserve(
    void **paddr,
    size_t len  )
{
    static const int flags =
#     if defined(__linux__)
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE
#     elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_GUARD
#     else
        MAP_PRIVATE | MAP_ANONYMOUS // fallback
#     endif
        ;

    // allocate region with no access
    int   retv = 0;
    void *addr = mmap(NULL, len, PROT_NONE, flags, -1, 0);
    if (addr == MAP_FAILED) {
        addr = NULL;
        retv = errno;
    }
    *paddr = addr;
    return retv;
}

// -------------------------------------------------------------------------------------
/// @brief ensure pages are committed in a previously reserved area
///
/// Ensures a range of pages in an already reserved region is remapped RW and committed,
/// so access is possible without page faults.  This is  where we _really_ reserve pages
/// in the swap space, so it may fail under OOM conditions,  just like malloc().
///
/// @param p    base address of region; must be page-aligned!
/// @param l    length of region; will expand to the next page boundary >= (p + l)
/// @return     0 onsuccess, else @c errno value
static int
_arena_commit(
    void  *p,
    size_t l)
{
# if defined(MADV_POPULATE_WRITE) && VMEMARENA_USE_MADVISE

    static bool use_madvise = true;

    if (use_madvise) {
        // Use a combination of mprotect() and madvise() to change the access rights
        // of an existing mapping and make sure the pages are properly reserved in
        // the sap space, so we don't SIGSEGV unexpectedly.

        // change protection of range to R/W
        if (0 != mprotect(p, l, (PROT_READ | PROT_WRITE))) { // something REALLY bad has happened!
            return errno;
        }

        // prefetch (commit) the page(s)
        if (0 == madvise(p, l, MADV_POPULATE_WRITE)) {
            return 0;
        }
        // mprotect() and madvise() have the same constraints on 'p' and 'l'.
        // ENOMEM is not logged here (allocation simply fails), but all other errors
        // indicate something more sinister...  If madvise()
        // fails after mptrotect() succeeds, we have the unclear/stray error and shoud retry
        // with mmap(), using it in the first place ever after.
        if (ENOMEM == errno) {
            return errno;
        }
        use_madvise = false;
    }
# endif

    // Use mmap() to replace part of the reserved address space with true RAM that is
    // backed by swap space.

    static const int iFlags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;

    if (p != mmap(p, l, (PROT_READ | PROT_WRITE), iFlags, -1, 0)) {
        return errno;
    }
    return 0;
}

// -------------------------------------------------------------------------------------
/// @brief uncommit and remove a reserved memory region
/// @param p    base address as returned by @c sys_vm_reserve()
/// @param l    length if reserved are
/// @return     0 onsuccess, else @c errno value
static int
_arena_release(
    void  *p,
    size_t l)
{
    // uncommit reserved region
    return (0 == munmap(p, l)) ? 0 : errno;
}

// =====================================================================================
#else // assume windows
// =====================================================================================

#include <windows.h>

#ifndef EFAULT
# define EFAULT EINVAL
#endif

static int
winerr_as_errno(void) {
    switch (GetLastError()) {
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY      :
    case ERROR_WORKING_SET_QUOTA: return ENOMEM;

    case ERROR_INVALID_ADDRESS  : return EFAULT;

    case ERROR_ACCESS_DENIED    : return EPERM;

    case ERROR_INVALID_PARAMETER:
    default                     : return EINVAL;
    }
    // VirtualAlloc() doesn't meaningfully return anything else
}

static int _arena_reserve(void **paddr, size_t len)
{
    // allocate region with no access
    return (NULL != (*paddr = VirtualAlloc(NULL, len, MEM_RESERVE, PAGE_NOACCESS))) ?
        0 : winerr_as_errno();
}

static int
_arena_commit(void *p, size_t l)
{
    // commit pages in already reserved range
    return (p == VirtualAlloc(p, l, MEM_COMMIT, PAGE_READWRITE)) ?
        0 : winerr_as_errno();
}

static int
_arena_release(void *p, size_t l)
{
    // decommit reserved region
    (void)l;
    return VirtualFree(p, 0, MEM_RELEASE) ?
        0 : winerr_as_errno();
}

// =====================================================================================
#endif
// =====================================================================================

/// @brief align to next alignment boundary
/// @param base     value to adjust
/// @param asize    alignment size (must be power of 2!)
/// @return         smallest value Z with Z >= base, Z % asize == 0
static inline size_t
topalign(size_t base, size_t asize) {
    assert((asize & (asize - 1u)) == 0); // The K&R test for a power of two
    return (base + (asize - 1u)) & ~(asize - 1u);
}

// -------------------------------------------------------------------------------------
bool
vmBump_init(
    VmBumpPoolT *arena ,
    size_t       blklen,
    size_t       blkcnt)
{
    if (NULL == arena) {
        errno = EINVAL;
        return false;
    }
    memset(arena, 0, sizeof(*arena));
    arena->_m_head = NULL;

    if ((0 == blklen) || (0 == blkcnt)) {
        errno = ERANGE;
        return false;
    }

    if (blklen & (s_pagesize - 1u)) {
        // blocks must be multiples of pages, sorry!!!
        errno = ERANGE;
        return false;
    }

    blklen = topalign(blklen, s_pagesize);
    arena->_m_blks  = blklen;
    arena->_m_limit = blklen * blkcnt;
    if (blkcnt != (arena->_m_limit / blklen)) { // size_t overflow?
        errno = ERANGE;
        return false;
    }
    return true;
}

// -------------------------------------------------------------------------------------
/// @brief destroy the reserved memory area of a string set
/// @param arena    block arena to destroy
void
vmBump_fini(
    VmBumpPoolT *arena)
{
    if (NULL != arena) {
        while (NULL != arena->_m_head) {
            VmBumpPoolBlkT *pblock = arena->_m_head;
            arena->_m_head = pblock->_m_next;
            (void)_arena_release(pblock, pblock->_m_size);
        }
        arena->_m_total = 0u;
    } else {
        errno = EINVAL;
    }
}

// -------------------------------------------------------------------------------------
/// @brief allocate a new core block at least big enough to fulfill an allocation
/// @param arena    string set to work on
/// @param size     required allocation size
/// @param align    required alignment of returned base address
/// @return         0 onsuccess, else @c errno value
static int
mpool_morecore(
    VmBumpPoolT *arena,
    size_t       size ,
    size_t       align)
{
    size_t           msize;                 // size of memory block
    size_t           mslag  = 0;            // lost memory at end of current block
    int              retv   = 0;
    VmBumpPoolBlkT *pblock = arena->_m_head; // last block of raw memory

    // we check the raw input size first to be <= 128kB. That permits us to do
    // a lot of calculations *without* checking for overflows, as they simply
    // cannot happen!
    if (size > (size_t)0x20000UL) { // too big to be useful?
        return ERANGE;
    }

    // get the needed min VM block size and slag size of current block first
    msize = topalign(sizeof(VmBumpPoolBlkT), align) + size;
    if (NULL != pblock) {
        mslag = topalign(pblock->_m_used, s_pagesize) - pblock->_m_used;
    }

    // check if getting more RAM would blow the limit
    if ((arena->_m_limit <= arena->_m_total) || ((arena->_m_limit - arena->_m_total) < (msize + mslag))) {
        return ENOMEM;
    }

    // align memory size to next page boundary, than see if we must expand
    msize = topalign(msize, s_pagesize);
    if (msize < arena->_m_blks) {
        msize = arena->_m_blks;
    }

    // reserve an address area of the given size; fail if we can't...
    retv = _arena_reserve((void**)& pblock, msize);
    if (0 != retv) {
        return retv;
    }

    // Commit the 1st page so we can write the bookkeeping stuff.  If we
    // can't, release the reservation and fail.
    retv = _arena_commit(pblock, s_pagesize);
    if (0 != retv) {
        (void)_arena_release(pblock, msize);
        return retv;
    }

    // Now we've finally got it! Push the new block onto the chain and
    // initialise the block header.
    pblock->_m_size  = msize;
    pblock->_m_used = sizeof(VmBumpPoolBlkT);

    pblock->_m_next  = arena->_m_head;
    arena->_m_head  = pblock;
    arena->_m_total += pblock->_m_used + mslag;

    return 0;
}

// -------------------------------------------------------------------------------------
/// @brief allocate bytes (with alignment) in the memory pool
/// @param arena    string set to work on
/// @param bytes    required allocation size
/// @param align    required alignment of returned base address
/// @return         pointer to memory, @c NULL on error
void*
vmBump_alloc(
    VmBumpPoolT *arena,
    size_t       bytes,
    size_t       align)
{
    int              retv;
    size_t           base, mend;    // base & end of allocation area
    size_t           cplo, cphi;    // lo/hi range of committed pages
    size_t           need, have;    // needed/available freespace in current block
    VmBumpPoolBlkT *pblock;        // memory block to carve out

    if (NULL == arena) {
        errno = EINVAL;
        return NULL;
    }

    // without any core, try to get a 1st block; bail out if that fails!
    if ((NULL == arena->_m_head) && (0 != (retv = mpool_morecore(arena, bytes, align)))) {
        errno = retv;
        return NULL;
    }

again:  // we might come back to this if 1st block cannot fullfill the request!
    pblock = arena->_m_head;            // block to carve out
    base = pblock->_m_used;             // end of current allocation
    cplo = topalign(base, s_pagesize);  // end of current commit area
    base = topalign(base, align);       // properly aligned base to return
    mend = base + bytes;                // new end of allocated area
    cphi = topalign(mend, s_pagesize);  // required new end of commit area

    need = mend - pblock->_m_used;
    have = pblock->_m_size - pblock->_m_used;
    if (need > have) {
        // the request does not fit into remaining size of block, get a new core block
        // and retry.  Fail if no new core memory is available.
        retv = mpool_morecore(arena, bytes, align);
        if (0 == retv) {
            goto again;
        }
        errno = retv;
        return NULL;
    } else if (cphi != cplo) {
        // the request fits into the remaining space of the core block, but we have to
        // commit more memory pages to the virtual address space.  Fails if the commit
        // cannot get us the RAM and swap space.
        retv = _arena_commit(((char *)pblock + cplo), (cphi - cplo));
        if (0 != retv) {
            errno = retv;
            return NULL;
        }
    }
    // If we reach this point, we have enough writeable memory mapped into our address
    // space to honor the request.  Keep track of the new end-of-allocation and return
    // a pointer to the properly aligned base.
    arena->_m_total += (mend - pblock->_m_used);
    pblock->_m_used = mend;
    return (char*)pblock + base;
}

// -------------------------------------------------------------------------------------
/// @brief get attribute from arena
/// @param arena    arena to query
/// @param what     property to get
/// @return         the value or @c (size_t)-1 on error
size_t
vmBump_getattr(
    VmBumpPoolT *arena,
    EVmBumpAttr  what )
{
    switch (what) {
    case eVmBumpAtt_BlkLen  : return arena->_m_blks;
    case eVmBumpAtt_Limit   : return arena->_m_limit;
    case eVmBumpAtt_Total   : return arena->_m_total;
    default                 : return (size_t)-1;
    }
}
// -*- that's all folks -*-
