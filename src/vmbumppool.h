// -------------------------------------------------------------------------------------
// Virtual memory backed, page-on-demand bump allocator
// -------------------------------------------------------------------------------------
// This file is part of "PatriciaC" by J.Perlinger.
//
// PatriciaC by J.Perlinger is marked CC0 1.0. To view a copy of this mark,
//    visit https://creativecommons.org/publicdomain/zero/1.0/
//
// -------------------------------------------------------------------------------------

#ifndef VMEMARENA_A86A7C45_B842_401F_B245_319CB49D9C79
#define VMEMARENA_A86A7C45_B842_401F_B245_319CB49D9C79

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief header of VM mapping blocks
/// a link pointer, the total size and the bytes consumed so far...
typedef struct _VmBumpPoolBlkS {
    struct _VmBumpPoolBlkS  *_m_next;   //!< next pool block, LIFO
    size_t                   _m_size;   //!< total (brutto) size of this block, incl. this header
    size_t                   _m_used;   //!< current MBRK value (mapping end, byte offset)
} VmBumpPoolBlkT;

/// @brief memory block pool for bump allocation
/// The arena is just the head of the block list and some accounting data.
typedef struct _VmBumpPoolS {
    struct _VmBumpPoolBlkS  *_m_head;   //!< start of block list
    size_t                   _m_blks;   //!< minimum/recommended block size
    size_t                   _m_total;  //!< total used bytes (node + string data)
    size_t                   _m_limit;  //!< limit for used bytes
} VmBumpPoolT;

/// @brief enum to describe get/set attributes
typedef enum {
    eVmBumpAtt_BlkLen = 1,  //!< block length of string set
    eVmBumpAtt_Limit,       //!< total allocation limit
    eVmBumpAtt_Total        //!< current total allocation
} EVmBumpAttr;

extern void     vmBump_StaticSetup(void);

extern bool     vmBump_init(VmBumpPoolT *arena, size_t blksize, size_t limit);
extern void     vmBump_fini(VmBumpPoolT *arena);
extern void    *vmBump_alloc(VmBumpPoolT *arena, size_t bytes, size_t align);
extern size_t   vmBump_getattr(VmBumpPoolT *arena, EVmBumpAttr what);

#ifdef __cplusplus
}
#endif

#endif // VMEMARENA_A86A7C45_B842_401F_B245_319CB49D9C79
