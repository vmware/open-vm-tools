/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
 * iovector.c --
 *
 *      I/O vector management code.
 */

#ifdef _WIN32
#include <windows.h>
#endif

#include "vmware.h"
#include "util.h"
#include "iovector.h"

#define LGPFX   "IOV: "

/*
 * Structure used when duplicating iov.
 */
struct VMIOVecAndEntries {
   VMIOVec iov; /* has to be first */
   struct iovec e[0];
};


/*
 *---------------------------------------------------------------------------
 *
 * IOV_Log --
 *
 *      Logs the content of an iov to the log file.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
IOV_Log(const VMIOVec *iov)     // IN
{
   if (iov) {
      uint32 i;
      Log("###### dumping content of iov ######\n");
      Log("%s\n", iov->read ? "READ" : "WRITE");
      Log("startSector = %"FMT64"d\n", iov->startSector);
      Log("numSectors = %"FMT64"d\n", iov->numSectors);
      Log("numBytes = %"FMT64"d\n", iov->numBytes);
      Log("numEntries = %d\n", iov->numEntries);
      for (i = 0; i < iov->numEntries; i++) {
         Log("  entries[%d] = %p / %"FMTSZ"u\n", 
             i, iov->entries[i].iov_base, (size_t)iov->entries[i].iov_len);
      }
   } else {
      Log("###### iov is NULL!! ######\n");
   }
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_Zero --
 *
 *      Zeros the content of an iov.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
IOV_Zero(VMIOVec *iov)   // IN
{
   uint64 numBytesLeft;
   int i;

   ASSERT(iov);
   ASSERT(iov->read);

   numBytesLeft = iov->numBytes;
   i = 0;

   while (numBytesLeft > 0) {
      size_t c = MIN(numBytesLeft, iov->entries[i].iov_len);
      void *buf;

      VERIFY(i < iov->numEntries);
      buf = iov->entries[i].iov_base;
      ASSERT(buf && buf != LAZY_ALLOC_MAGIC);
      memset(buf, 0, c);
      numBytesLeft -= c;
      i++;
   }
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_Allocate --
 *
 *      Allocates a brand new iov to be freed with IOV_Free.
 *
 * Results:
 *      A VMIOVec*.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

VMIOVec*
IOV_Allocate(int numEntries)          // IN
{
   struct VMIOVecAndEntries *iov;

   iov = Util_SafeMalloc(sizeof *iov + numEntries * sizeof(struct iovec));
   iov->iov.entries = iov->e;
   iov->iov.allocEntries = NULL;
   iov->iov.numEntries = numEntries;

   return &iov->iov;
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_DuplicateStatic --
 *
 *      Duplicate an iov, potentially using a static'ly allocated array of
 *      struct iovec. 
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
IOV_DuplicateStatic(VMIOVec *iovIn,                     // IN
                    int numStaticEntries,               // IN
                    struct iovec *staticEntries,        // IN
                    VMIOVec *iovOut)                    // OUT
{
   ASSERT(staticEntries);
   ASSERT(iovIn);
   ASSERT(iovOut);

   Util_Memcpy(iovOut, iovIn, sizeof *iovOut);
   if (iovIn->numEntries <= numStaticEntries) {
      iovOut->allocEntries = NULL;
      iovOut->entries = staticEntries;
   } else {
      iovOut->allocEntries = Util_SafeMalloc(iovIn->numEntries * 
                                             sizeof(struct iovec));
      iovOut->entries = iovOut->allocEntries;
   }
   Util_Memcpy(iovOut->entries, iovIn->entries, 
          iovIn->numEntries * sizeof(struct iovec));
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_MakeSingleIOV --
 *
 *      Fills in an iov.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
IOV_MakeSingleIOV(VMIOVec* v,              // IN/OUT
                  struct iovec* entry,     // IN
                  SectorType startSector,  // IN
                  SectorType dataLen,      // IN
                  uint32 sectorSize,       // IN
                  uint8* buffer,           // IN
                  Bool read)               // IN
{
   ASSERT(v);
   ASSERT(entry);

   v->read = read;
   v->startSector = startSector;
   v->numSectors = dataLen;
   v->numBytes = dataLen * sectorSize;
   v->numEntries = 1;
   v->entries = entry;
   v->allocEntries = entry;
   entry->iov_base = (char *)buffer;
   entry->iov_len = (size_t) v->numBytes;
}


/*
 *-----------------------------------------------------------------------------
 *
 * IOV_IsZero --
 *
 *      Tell if an iov is full of zeros. Used when we are about to write an iov
 *      in a grain, if it's zero and the grain does not exist, we just do
 *      nothing.
 *
 * Result:
 *      TRUE/FALSE
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
IOV_IsZero(VMIOVec* iov)      // IN: the iov to scan
{
   uint32 i;

   for (i = 0; i < iov->numEntries; i++) {
      if (!Util_BufferIsEmpty(iov->entries[i].iov_base,
                              iov->entries[i].iov_len)) {
         return FALSE;
      }
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * IOVSplitList --
 *
 *      Utility function to split an iovec (byte granularity scatter-gather
 *      array) into two-- an initial one that's exactly a whole number of
 *      sectors long, and the remainder.  If the table entry that finishes
 *      off the requested region is actually larger than the amount of space
 *      left in the region, it is truncated and the remaining bytes and
 *      their location are returned in overlap. The size of the region is
 *      passed in via regionV->numSectors, and the rest of regionV is
 *      filled in with to describe a request for exactly that region.
 *
 * Results:
 *      Pointer to the first remaining entry from the original entries list.
 *
 * Side effects:
 *      overlap filled in if the original entries list doesn't have a clean
 *      break on the region boundary, otherwise overlap->iov_len == 0.
 *      Also in cases over overlap, the last entry of entries is truncated.
 *
 *----------------------------------------------------------------------------
 */

static struct iovec *
IOVSplitList(VMIOVec *regionV,      // IN/OUT: VMIOVec for this region
             struct iovec *entries, // IN/OUT: iovec to be split
             struct iovec *endPtr,  // IN: pointer to after last entry
             struct iovec *overlap, // OUT: overlap info if last truncated
             uint32 sectorSize)     // IN: # bytes in a sector
{
   struct iovec *curEntry;
  
   curEntry = entries;
   regionV->entries = curEntry;
   regionV->numEntries = 0;
   regionV->numBytes = 0;
   ASSERT(curEntry < endPtr); /* Better be at least one entry */

   do {
      regionV->numEntries++;
      regionV->numBytes += curEntry->iov_len;

      if (regionV->numBytes > regionV->numSectors * sectorSize) {
         int spillover;
         
         spillover = (int) (regionV->numBytes -
                                            regionV->numSectors * sectorSize);
         ASSERT(spillover < curEntry->iov_len);
         ASSERT(spillover > 0);

         /*
          * Truncate the last overlapping entry and store the excess. After
          * we finish this region, we'll smash this last entry with its
          * remainder and just move on to the next region.
          */

         regionV->numBytes -= spillover;
         curEntry->iov_len -= spillover;
         overlap->iov_len = spillover;
         overlap->iov_base = (char *)curEntry->iov_base + curEntry->iov_len;
         break;
      } else if (regionV->numBytes == regionV->numSectors * sectorSize) {
         /*
          * Clean finish. The last entry for this region will be handled
          * with no overlap, so increment past it for the start of the next
          * region's entries.
          */

         overlap->iov_len = 0;
         curEntry++;
         break;
      }
      curEntry++;
   } while (curEntry < endPtr);

   ASSERT(regionV->numBytes == regionV->numSectors * sectorSize);

   return curEntry;
}


/*                              
 *----------------------------------------------------------------------------
 *
 * IOV_Split --
 *
 *      Utility function useful for iterating over VMIOVec.  You setup
 *      numSectors and then pass in the vector for the whole remaining
 *      transfer.  The code creates a VMIOVec to describe the subset of the
 *      transfer contained in the region and adjusts origV so that it describes
 *      the remainder.  
 *
 * Results:
 *      a VMIOVec* describing the first numSectors sectors of origV.
 *
 * Side effects:
 *      See above-- origV is split into regionV and the remainder with
 *      overlap filled in if the last entry of the region straddled the
 *      boundary.  Otherwise overlap->iov_len is set to zero.
 *
 *----------------------------------------------------------------------------
 */

VMIOVec*
IOV_Split(VMIOVec *origV,         // IN/OUT: VMIOVec for whole xfer
          SectorType numSectors,  // IN
          uint32 sectorSize)      // IN: # bytes in a sector
{  
   struct VMIOVecAndEntries *v;
   int cpySize;
   VMIOVec* iov;

   ASSERT(origV);
   ASSERT(numSectors > 0);
   ASSERT(numSectors <= origV->numSectors);

   /*
    * The resulting iov cannot have more entries than the incoming one.
    */

   v = Util_SafeMalloc(sizeof *v + origV->numEntries * sizeof(struct iovec));
   iov = &v->iov;
   Util_Memcpy(iov, origV, sizeof *iov);
   iov->allocEntries = NULL;
   iov->numSectors = numSectors;

   /*
    * Handle lazy allocation of backing store.
    */

   if (origV->entries->iov_base == LAZY_ALLOC_MAGIC && 
       origV->entries->iov_len == 0) {

      ASSERT(origV->numEntries == 1);
      iov->entries = v->e;
      Util_Memcpy(iov->entries, origV->entries, sizeof(struct iovec));

      iov->numBytes = iov->numSectors * sectorSize;

      origV->startSector += numSectors;
      origV->numSectors -= numSectors;
      origV->numBytes -= iov->numBytes;

      return iov;
   }

   /* See if the region is the whole thing */
   if (origV->numSectors == numSectors) {
      cpySize = origV->numEntries * sizeof *origV->entries;
      iov->entries = v->e;
      Util_Memcpy(iov->entries, origV->entries, cpySize);

      origV->startSector += numSectors;
      origV->numSectors = 0;

      origV->numEntries = 0;
      origV->numBytes = 0;
   } else {
      void* tmpPtr;
      struct iovec overlap = { 0, };

      origV->startSector += numSectors;
      origV->numSectors -= numSectors;
      origV->entries = IOVSplitList(iov, origV->entries,
                                    origV->entries + origV->numEntries, 
                                    &overlap, sectorSize);

      cpySize = iov->numEntries * sizeof *iov->entries;
      tmpPtr = iov->entries;
      iov->entries = v->e;
      Util_Memcpy(iov->entries, tmpPtr, cpySize);

      origV->numEntries -= iov->numEntries;
      if (overlap.iov_len != 0) { 
         origV->entries->iov_len = overlap.iov_len;
         origV->entries->iov_base = overlap.iov_base;
         origV->numEntries++; 
      }
      origV->numBytes -= iov->numBytes;
   }
   ASSERT(iov->numEntries > 0);

   return iov;
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_WriteIovToBuf --
 *
 *      This function takes an iov and a buffer as input and writes the content
 *      of the buffers pointed to by the iov into buf.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
IOV_WriteIovToBuf(struct iovec const *entries, // IN
                  int numEntries,              // IN
                  uint8 *bufOut,               // OUT
                  size_t bufSize)              // IN
{
   size_t count = 0;
   int i;

   ASSERT(entries);
   ASSERT(bufOut);

   for (i = 0; i < numEntries; i++) {
      size_t numBytes;

      ASSERT(entries[i].iov_base);
      ASSERT(entries[i].iov_base != LAZY_ALLOC_MAGIC);

      numBytes = MIN(bufSize - count, entries[i].iov_len);

      Util_Memcpy(&bufOut[count], entries[i].iov_base, numBytes);
      count += numBytes;

      if (count >= bufSize) {
         return;
      }

      VERIFY(count <= bufSize);
   }
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_Duplicate --
 *     
 *      Allocates a brand new iov, the resulting iov should be free'd using
 *      IOV_Free.
 * 
 * Result:
 *      A duplicated iov.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

VMIOVec*
IOV_Duplicate(VMIOVec* iovIn)     // IN
{
   struct VMIOVecAndEntries* v;

   v = Util_SafeMalloc(sizeof *v + iovIn->numEntries * sizeof(struct iovec));
   Util_Memcpy(&v->iov, iovIn, sizeof *iovIn);
   v->iov.allocEntries = NULL;
   v->iov.entries = v->e;
   Util_Memcpy(v->iov.entries, iovIn->entries,
          iovIn->numEntries * sizeof(struct iovec));

   return &v->iov;
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_Free --
 *     
 *      Frees an iov.
 * 
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
IOV_Free(VMIOVec* iov)     // IN
{
   ASSERT(iov);
   if (iov->allocEntries) {
      free(iov->allocEntries);
      iov->allocEntries = NULL;
   }
   free(iov);
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_WriteBufToIov --
 *
 *      This function copies the content of bufIn into the buffer pointed to by
 *      entries. It basically does the opposite of IOV_WriteIovToBuf.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
IOV_WriteBufToIov(const uint8 *bufIn,          // IN
                  size_t bufSize,              // IN
                  struct iovec const *entries, // OUT
                  int numEntries)              // IN
{
   size_t count = 0;
   int i;

   ASSERT(entries);
   VERIFY_BUG(29009, bufIn);

   for (i = 0; i < numEntries; i++) {
      size_t numBytes;

      ASSERT(entries[i].iov_base);
      ASSERT(entries[i].iov_base != LAZY_ALLOC_MAGIC);

      numBytes = MIN(bufSize - count, entries[i].iov_len);

      Util_Memcpy(entries[i].iov_base, &bufIn[count], numBytes);
      count += numBytes;
      if (count >= bufSize) {
         return;
      }
      VERIFY(count <= bufSize);
   }
}


/*
 *---------------------------------------------------------------------------
 *
 * IOVFindFirstEntryOffset --
 *
 *      This function takes an iov and a byte offset and returns the
 *      index of the first entry and offset in that entry where copy starts.
 * 
 * Result:
 *      If offset is within iov, returns the index of the first entry and
 *      sets entryOffset. Otherwise, return numEntries.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static int
IOVFindFirstEntryOffset(struct iovec* entries,        // IN
                        int numEntries,               // IN
                        size_t iovOffset,             // IN
                        size_t *entryOffsetp)         // OUT

{
   size_t entryLen = 0, entryOffset = 0;
   int i;

   ASSERT(entries);
   ASSERT(entryOffsetp);

   /* find the entry where to start */
   for (i = 0; (iovOffset >= entryOffset) && (i < numEntries); i++) {
      entryLen = entries[i].iov_len;
      entryOffset += entryLen;
   }

   if (iovOffset >= entryOffset) {
      /* iov offset is outside the iov - copy nothing */
      Log(LGPFX"%s:%d i %d (of %d), offsets: entry %"FMTSZ"u, iov %"FMTSZ"u "
          "invalid iov offset\n",
          __FILE__, __LINE__, i, numEntries, entryOffset, iovOffset);

      return numEntries;
   }

   /* i is index in next entry. Set entryOffset to offset in current entry */
   entryOffset = iovOffset - (entryOffset - entryLen);
   ASSERT(entryOffset < entryLen);

   *entryOffsetp = entryOffset;

   return i - 1;
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_WriteIovToBufPlus --
 *
 *      This function takes an iov and a buffer as input and writes the content
 *      of the buffers pointed to by the iov into buf.
 *      It is similar to IOV_WriteIovToBuf but copy may start at any point
 *      within the iov and may only partially overlap.
 *      iovOffset is the offset in bytes within the iov where to start copying.
 * 
 * Result:
 *      Returns the number of bytes copied.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

size_t
IOV_WriteIovToBufPlus(struct iovec* entries,    // IN
                  int numEntries,               // IN
                  uint8* bufOut,                // OUT
                  size_t bufSize,               // IN
                  size_t iovOffset)             // IN
{
   size_t entryLen, entryOffset;
   size_t count = bufSize;
   int i;

   VERIFY_BUG(29009, bufOut);

   i = IOVFindFirstEntryOffset(entries, numEntries, iovOffset, &entryOffset);

   for (; count && (i < numEntries); i++) {
      char *base = (char *)(entries[i].iov_base) + entryOffset;
      size_t iov_len = entries[i].iov_len;

      ASSERT(entries[i].iov_base || entries[i].iov_len == 0);
      ASSERT(entries[i].iov_base != LAZY_ALLOC_MAGIC);

      if (iov_len <= 0) {
         continue;
      }
      entryLen = MIN(count, iov_len - entryOffset);

      Util_Memcpy(bufOut, base, entryLen);

      count -= entryLen;
      bufOut += entryLen;
      entryOffset = 0;
   }

   ASSERT(count <= bufSize);

   return bufSize - count;
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_WriteBufToIovPlus --
 *
 *      This function copies the content of bufIn into the buffer pointed to by
 *      entries. It is similar to IOV_WriteBufToIov but the buffer may be
 *      copied anywhere within the iov and may only partially overlap.
 *      iovOffset is the offset in bytes within the iov where to start copying.
 * 
 * Result:
 *      Returns the number of bytes copied.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

size_t
IOV_WriteBufToIovPlus(uint8* bufIn,             // IN
                      size_t bufSize,           // IN
                      struct iovec* entries,    // OUT
                      int numEntries,           // IN
                      size_t iovOffset)         // IN
{
   size_t entryLen, entryOffset;
   size_t count = bufSize;
   int i;

   VERIFY_BUG(29009, bufIn);

   i = IOVFindFirstEntryOffset(entries, numEntries, iovOffset, &entryOffset);

   for (; count && (i < numEntries); i++) {
      char *base = (char *)(entries[i].iov_base) + entryOffset;
      size_t iov_len = entries[i].iov_len;

      VERIFY_BUG(33859, entries[i].iov_base || entries[i].iov_len == 0);
      ASSERT(entries[i].iov_base != LAZY_ALLOC_MAGIC);

      if (iov_len <= 0) {
         continue;
      }
      entryLen = MIN(count, iov_len - entryOffset);
      
      Util_Memcpy(base, bufIn, entryLen);

      count -= entryLen;
      bufIn += entryLen;
      entryOffset = 0;
   }

   ASSERT(count <= bufSize);

   return bufSize - count;
}


/*
 *---------------------------------------------------------------------------
 *
 * IOV_WriteIovToIov --
 *
 *      This function copies the overlapping portion from the source iov
 *      to the target iov, that is the region defined by:
 *      startSector = MAX(srcIov->startSector, dstIov->startSector)
 *      numSectors = MIN(<last src sector>, <last dst sector>) - startSector.
 *
 *      sectorSizeShift is conversion factor between sector and byte.
 *
 *      NOTE: assume that iov->numBytes is the actual number of bytes to copy.
 *      Do not copy beyond numBytes for either src or dst iov.
 * 
 * Result:
 *      Returns the number of bytes copied.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

size_t
IOV_WriteIovToIov(VMIOVec *srcIov,        // IN
                  VMIOVec *dstIov,        // OUT
                  uint32 sectorSizeShift) // IN
{
   size_t entryLen = 0, srcEntryOffset, copyLen, retval;
   uint64 srcStartByte, dstStartByte, startByte, endByte;
   int64 count, srcIovOffset, dstIovOffset;
   struct iovec *srcEntries = srcIov->entries;
   uint32 srcNumEntries = srcIov->numEntries;
   struct iovec *dstEntries = dstIov->entries;
   uint32 dstNumEntries = dstIov->numEntries;
   int i;

   ASSERT(srcIov);
   ASSERT(dstIov);

   /* find start byte address for src, dst and common region */
   srcStartByte = srcIov->startSector << sectorSizeShift;
   dstStartByte = dstIov->startSector << sectorSizeShift;
   startByte = MAX(srcStartByte, dstStartByte);

   /* find num bytes and end byte address for common region */
   endByte = srcStartByte + srcIov->numBytes;
   count = dstStartByte + dstIov->numBytes;
   endByte = MIN(endByte, count);

   count = endByte - startByte;

   /* count is number of bytes to copy, [startByte,endByte) is region to copy */
   if (count <= 0) {    /* no overlap */
      Log(LGPFX"%s:%d iov [%"FMT64"u:%"FMT64"u] and [%"FMT64"u:%"FMT64"u] - "
          "no overlap!\n", __FILE__, __LINE__, srcIov->startSector,
          srcIov->numSectors, dstIov->startSector, dstIov->numSectors);

      return 0;
   }

   srcEntries = srcIov->entries;

   ASSERT(srcEntries);
   ASSERT(dstIov->entries);

   /* srcIovOffset is byte offset where to start copy in src iov */
   srcIovOffset = startByte - srcStartByte;
   /* dstIovOffset is byte offset where to start copy in dst iov */
   dstIovOffset = startByte - dstStartByte;

   ASSERT(srcIovOffset >= 0);
   ASSERT(dstIovOffset >= 0);

   retval = (size_t)count;

   /* first find the src entry where to start */
   i = IOVFindFirstEntryOffset(srcEntries, srcNumEntries,
                               (size_t) srcIovOffset, &srcEntryOffset);

   for (; count && (i < srcNumEntries); i++) {
      size_t iov_len = srcEntries[i].iov_len;

      ASSERT(srcEntries[i].iov_base || srcEntries[i].iov_len == 0);
      ASSERT(srcEntries[i].iov_base != LAZY_ALLOC_MAGIC);

      if (iov_len <= 0) {
         continue;
      }
      entryLen = MIN(count, iov_len - srcEntryOffset);

      copyLen = IOV_WriteBufToIovPlus(
                           (uint8 *)(srcEntries[i].iov_base) + srcEntryOffset,
                                      entryLen, dstEntries,
                                      dstNumEntries, dstIovOffset);

      if (copyLen == 0) {  /* finished */
         break;
      }
      ASSERT(copyLen <= entryLen);

      count -= copyLen;
      dstIovOffset += copyLen;
      srcEntryOffset = 0;
   }

   ASSERT(count <= retval);

   return retval - count;
}


#ifdef VMX86_DEBUG
/*
 *-----------------------------------------------------------------------------
 *
 * IOV_Assert --
 *
 *      Checks that the 'numEntries' iovecs in 'iov' are non-null and have
 *      nonzero lengths.
 *
 *      Meant to be called via IOV_ASSERT macro.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Assert-fails if the iovec is invalid.
 *
 *-----------------------------------------------------------------------------
 */

void
IOV_Assert (struct iovec *iov,          // IN: iovec to check
            uint32 numEntries)          // IN: # of entries in 'iov'
{
   ASSERT(iov);
   ASSERT(numEntries);

   for (; numEntries-- > 0; iov++) {
      ASSERT(iov->iov_base);
      ASSERT(iov->iov_len);
   }
}
#endif
