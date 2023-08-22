/*********************************************************
 * Copyright (C) 1998-2023 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * iovector.h --
 *
 *      iov management code API.
 */

#ifndef _IOVECTOR_H_
#define _IOVECTOR_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Ugly definition of struct iovec.
 */
#if defined(__linux__) || defined(sun) || defined(__APPLE__) || \
    defined(__FreeBSD__) || defined(__EMSCRIPTEN__)
#include <sys/uio.h>    // for struct iovec
#else

#ifndef HAS_IOVEC
struct iovec {
   void *iov_base; /* Starting address. */
   size_t iov_len; /* Length in bytes. */
};
#endif   // HAS_IOVEC

#endif

/*
 * An I/O Vector.
 */
typedef struct VMIOVec {
   SectorType startSector;
   SectorType numSectors;
   uint64 numBytes;             /* Total bytes from all of the entries */
   uint32 numEntries;           /* Total number of entries */
   Bool read;                   /* is it a readv operation? else it's write */
   struct iovec *entries;       /* Array of entries (dynamically allocated) */
   struct iovec *allocEntries;  /* The original array that can be passed to free().
                                 * NULL if entries is on a stack. */
} VMIOVec;

#define LAZY_ALLOC_MAGIC      ((void*)0xF0F0)

VMIOVec* IOV_Split(VMIOVec *regionV,
                   SectorType numSectors,
                   uint32 sectorSize);

void IOV_Log(const VMIOVec *iov);
void IOV_Zero(VMIOVec *iov);
Bool IOV_IsZero(VMIOVec* iov);
VMIOVec* IOV_Duplicate(VMIOVec* iovIn);
VMIOVec* IOV_Allocate(int numEntries);
void IOV_Free(VMIOVec* iov);
void IOV_DuplicateStatic(VMIOVec *iovIn,
                         int numStaticEntries,
                         struct iovec *staticEntries,
                         VMIOVec *iovOut);

void IOV_MakeSingleIOV(VMIOVec* v,
                       struct iovec* iov,
                       SectorType startSector,
                       SectorType dataLen,
                       uint32 sectorSize,
                       uint8* buffer,
                       Bool read);

void IOV_WriteIovToBuf(struct iovec const *entries,
                       int numEntries,
                       uint8 *bufOut,
                       size_t bufSize);

void IOV_WriteBufToIov(const uint8 *bufIn,
                       size_t bufSize,
                       struct iovec const *entries,
                       int numEntries);

size_t
IOV_WriteIovToBufPlus(struct iovec* entries,
                      int numEntries,
                      uint8* bufOut,
                      size_t bufSize,
                      size_t iovOffset);

size_t
IOV_WriteBufToIovPlus(uint8* bufIn,
                      size_t bufSize,
                      struct iovec* entries,
                      int numEntries,
                      size_t iovOffset);

size_t
IOV_WriteIovToIov(VMIOVec *srcIov,
                  VMIOVec *dstIov,
                  uint32 sectorSizeShift);

/*
 *-----------------------------------------------------------------------------
 *
 * IOV_ASSERT, IOV_Assert --
 *
 *      Checks that the 'numEntries' iovecs in 'iov' are non-null and have
 *      nonzero lengths.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Assert-fails if the iovec is invalid.
 *
 *-----------------------------------------------------------------------------
 */


#if VMX86_DEBUG
#define IOV_ASSERT(IOVEC, NUM_ENTRIES) IOV_Assert(IOVEC, NUM_ENTRIES)
void IOV_Assert(struct iovec *iov,       // IN: iovector
                uint32 numEntries);      // IN: # of entries in 'iov'
#else
#define IOV_ASSERT(IOVEC, NUM_ENTRIES) ((void) 0)
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* #ifndef _IOVECTOR_H_ */
