/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 * hgfsServerPacketUtil.c --
 *
 * Utility functions for manipulating packet used by hgfs server code
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vmware.h"
#include "hgfsServer.h"
#include "hgfsServerInt.h"
#include "util.h"

static void *HSPUGetBuf(HgfsServerChannelCallbacks *chanCb,
                        MappingType mappingType,
                        HgfsVmxIov *iov,
                        uint32 iovCount,
                        uint32 startIndex,
                        size_t dataSize,
                        size_t bufSize,
                        void **buf,
                        Bool *isAllocated,
                        uint32 *iovMappedCount);
static void HSPUPutBuf(HgfsServerChannelCallbacks *chanCb,
                       MappingType mappingType,
                       HgfsVmxIov *iov,
                       uint32 iovCount,
                       uint32 startIndex,
                       size_t bufSize,
                       void **buf,
                       Bool *isAllocated,
                       uint32 *iovMappedCount);
static void HSPUCopyBufToIovec(HgfsVmxIov *iov,
                               uint32 iovMapped,
                               uint32 startIndex,
                               void *buf,
                               size_t bufSize);
static void HSPUCopyIovecToBuf(HgfsVmxIov *iov,
                               uint32 iovMapped,
                               uint32 startIndex,
                               void *buf,
                               size_t bufSize);
static Bool HSPUMapBuf(HgfsChannelMapVirtAddrFunc mapVa,
                       HgfsChannelUnmapVirtAddrFunc putVa,
                       size_t mapSize,
                       uint32 startIndex,
                       uint32 iovCount,
                       HgfsVmxIov *iov,
                       uint32 *mappedCount);
static void HSPUUnmapBuf(HgfsChannelUnmapVirtAddrFunc unmapVa,
                         uint32 startIndex,
                         HgfsVmxIov *iov,
                         uint32 *mappedCount);



/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_ValidateRequestPacketSize --
 *
 *    Validate an HGFS packet size with the HGFS header in use, the HGFS opcode request
 *    (its arguments) and optionally any opcode request data that is contained in the
 *    size for the packet.
 *
 * Results:
 *    TRUE if the packet size is large enough for the required request data.
 *    FALSE if not.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

Bool
HSPU_ValidateRequestPacketSize(HgfsPacket *packet,           // IN: Hgfs Packet
                               size_t requestHeaderSize,     // IN: request header size
                               size_t requestOpSize,         // IN: request op size
                               size_t requestOpDataSize)     // IN: request op data size
{
   size_t bytesRemaining = packet->metaPacketDataSize;
   Bool requestSizeIsOkay = FALSE;

   /*
    * Validate the request buffer size ensuring that the the contained components
    * (request header, the operation arguments and lastly any data) fall within it.
    */

   if (bytesRemaining >= requestHeaderSize) {
      bytesRemaining -= requestHeaderSize;
   } else {
      goto exit;
   }
   if (bytesRemaining >= requestOpSize) {
      bytesRemaining -= requestOpSize;
   } else {
      goto exit;
   }
   if (bytesRemaining >= requestOpDataSize) {
      requestSizeIsOkay = TRUE;
   }

exit:
   return requestSizeIsOkay;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_ValidateReplyPacketSize --
 *
 *    Validate a reply buffer size in an hgfs packet with the reply data
 *    size for the request.
 *
 * Results:
 *    TRUE if the reply buffer size is large enough for the request reply
 *    results. FALSE if not.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

Bool
HSPU_ValidateReplyPacketSize(HgfsPacket *packet,         // IN: Hgfs Packet
                             size_t replyHeaderSize,     // IN: reply header size
                             size_t replyResultSize,     // IN: reply result size
                             size_t replyResultDataSize, // IN: reply result data size
                             Bool useMappedMetaPacket)   // IN: using meta buffer
{
   size_t bytesRemaining;
   Bool replySizeIsOkay = FALSE;

   if (packet->replyPacket != NULL) {
      /* Pre-allocated reply buffer (as used by the backdoor). */
      bytesRemaining = packet->replyPacketSize;
   } else if (useMappedMetaPacket) {
      /* No reply buffer (as used by the VMCI) reuse the metapacket buffer. */
      bytesRemaining = packet->metaPacketSize;
   } else {
      /* No reply buffer but we will allocate the size required. */
      replySizeIsOkay = TRUE;
      goto exit;
   }

   if (bytesRemaining >= replyHeaderSize) {
      bytesRemaining -= replyHeaderSize;
   } else {
      goto exit;
   }
   if (bytesRemaining >= replyResultSize) {
      bytesRemaining -= replyResultSize;
   } else {
      goto exit;
   }
   if (bytesRemaining >= replyResultDataSize) {
      replySizeIsOkay = TRUE;
   }

exit:
   return replySizeIsOkay;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_GetReplyPacket --
 *
 *    Get a reply packet given an hgfs packet.
 *    Guest mappings may be established.
 *
 * Results:
 *    Pointer to reply packet.
 *
 * Side effects:
 *    Buffer may be allocated.
 *-----------------------------------------------------------------------------
 */

void *
HSPU_GetReplyPacket(HgfsPacket *packet,                  // IN/OUT: Hgfs Packet
                    HgfsServerChannelCallbacks *chanCb,  // IN: Channel callbacks
                    size_t replyDataSize,                // IN: Size of reply data
                    size_t *replyPacketSize)             // OUT: Size of reply Packet
{
   if (packet->replyPacket != NULL) {
      /*
       * When we are transferring packets over backdoor, reply packet
       * is a static buffer. Backdoor should always return from here.
       */
      packet->replyPacketDataSize = replyDataSize;
      LOG(4, ("Existing reply packet %s %"FMTSZ"u %"FMTSZ"u\n", __FUNCTION__,
              replyDataSize, packet->replyPacketSize));
      ASSERT(replyDataSize <= packet->replyPacketSize);
   } else if (chanCb != NULL && chanCb->getWriteVa != NULL) {
     /* Can we write directly into guest memory? */
      if (packet->metaPacket != NULL) {
         /*
          * Use the mapped metapacket buffer for the reply.
          * This currently makes assumptions about the mapping -
          * - It is mapped read -write
          * - It is always large enough for any reply
          * This will change as it is grossly inefficient as the maximum size
          * is always mapped and copied no matter how much data it really contains.
          */
         LOG(10, ("%s Using meta packet for reply packet\n", __FUNCTION__));
         ASSERT(BUF_READWRITEABLE == packet->metaMappingType);
         ASSERT(replyDataSize <= packet->metaPacketSize);

         packet->replyPacket = packet->metaPacket;
         packet->replyPacketDataSize = replyDataSize;
         packet->replyPacketSize = packet->metaPacketSize;
         packet->replyPacketIsAllocated = FALSE;
         /*
          * The reply is using the meta buffer so update the valid data size.
          *
          * Note, currently We know the reply size is going to be less than the
          * incoming request valid data size. This will updated when that part is
          * fixed. See the above comment about the assumptions and asserts.
          */
         packet->metaPacketDataSize = packet->replyPacketDataSize;
      } else {
         NOT_IMPLEMENTED();
      }
   } else {
      /* For sockets channel we always need to allocate buffer */
      LOG(10, ("%s Allocating reply packet\n", __FUNCTION__));
      packet->replyPacket = Util_SafeMalloc(replyDataSize);
      packet->replyPacketIsAllocated = TRUE;
      packet->replyPacketDataSize = replyDataSize;
      packet->replyPacketSize = replyDataSize;
   }

   *replyPacketSize = packet->replyPacketSize;
   return packet->replyPacket;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_PutReplyPacket --
 *
 *    Free buffer if reply packet was allocated.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

void
HSPU_PutReplyPacket(HgfsPacket *packet,                  // IN/OUT: Hgfs Packet
                    HgfsServerChannelCallbacks *chanCb)  // IN: Channel callbacks
{
   /*
    * If there wasn't an allocated buffer for the reply, there is nothing to
    * do as the reply is in the metapacket buffer which will be handled by the
    * put on the metapacket.
    */
   if (packet->replyPacketIsAllocated) {
      LOG(10, ("%s Freeing reply packet", __FUNCTION__));
      free(packet->replyPacket);
      packet->replyPacketIsAllocated = FALSE;
      packet->replyPacket = NULL;
      packet->replyPacketSize = 0;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_GetMetaPacket --
 *
 *    Get a meta packet given an hgfs packet.
 *    Guest mappings will be established.
 *
 * Results:
 *    Pointer to meta packet.
 *
 * Side effects:
 *    Buffer may be allocated.
 *-----------------------------------------------------------------------------
 */

void *
HSPU_GetMetaPacket(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                   size_t *metaPacketSize,               // OUT: Size of metaPacket
                   HgfsServerChannelCallbacks *chanCb)   // IN: Channel callbacks
{
   *metaPacketSize = packet->metaPacketDataSize;
   if (packet->metaPacket != NULL) {
      return packet->metaPacket;
   }

   if (packet->metaPacketSize == 0) {
      return NULL;
   }

   packet->metaMappingType = BUF_READWRITEABLE;

   return HSPUGetBuf(chanCb,
                     packet->metaMappingType,
                     packet->iov,
                     packet->iovCount,
                     0,
                     packet->metaPacketDataSize,
                     packet->metaPacketSize,
                     &packet->metaPacket,
                     &packet->metaPacketIsAllocated,
                     &packet->metaPacketMappedIov);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_ValidateDataPacketSize --
 *
 *    Validate a data packet buffer size in an hgfs packet with the required data
 *    size for the request.
 *
 * Results:
 *    TRUE if the data buffer size is valid for the request.
 *    FALSE if not.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

Bool
HSPU_ValidateDataPacketSize(HgfsPacket *packet,     // IN: Hgfs Packet
                            size_t dataSize)        // IN: data size
{
   return (dataSize <= packet->dataPacketSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_GetDataPacketBuf --
 *
 *    Get a data packet given an hgfs packet.
 *    Guest mappings will be established.
 *
 * Results:
 *    Pointer to data packet.
 *
 * Side effects:
 *    Buffer may be allocated.
 *-----------------------------------------------------------------------------
 */

void *
HSPU_GetDataPacketBuf(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                      MappingType mappingType,              // IN: Writeable/Readable
                      HgfsServerChannelCallbacks *chanCb)   // IN: Channel callbacks
{
   if (packet->dataPacket != NULL) {
      return packet->dataPacket;
   }

   if (packet->dataPacketSize == 0) {
      return NULL;
   }

   packet->dataMappingType = mappingType;
   return HSPUGetBuf(chanCb,
                     packet->dataMappingType,
                     packet->iov,
                     packet->iovCount,
                     packet->dataPacketIovIndex,
                     packet->dataPacketDataSize,
                     packet->dataPacketSize,
                     &packet->dataPacket,
                     &packet->dataPacketIsAllocated,
                     &packet->dataPacketMappedIov);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPUGetBuf --
 *
 *    Get a {meta, data} packet given an hgfs packet.
 *    Guest mappings will be established.
 *
 * Results:
 *    Pointer to buffer.
 *
 * Side effects:
 *    Buffer may be allocated.
 *-----------------------------------------------------------------------------
 */

static void *
HSPUGetBuf(HgfsServerChannelCallbacks *chanCb,  // IN: Channel callbacks
           MappingType mappingType,             // IN: Access type Readable/Writeable
           HgfsVmxIov *iov,                     // IN: iov array
           uint32 iovCount,                     // IN: iov array size
           uint32 startIndex,                   // IN: Start index of iov
           size_t dataSize,                     // IN: Size of data in tghe buffer
           size_t bufSize,                      // IN: Size of buffer
           void **buf,                          // OUT: Contigous buffer
           Bool *isAllocated,                   // OUT: Buffer allocated
           uint32 *iovMappedCount)              // OUT: iov mapped count
{
   uint32 iovMapped = 0;
   HgfsChannelMapVirtAddrFunc mapVa;
   Bool releaseMappings = FALSE;

   ASSERT(buf != NULL);

   *buf = NULL;
   *isAllocated = FALSE;

   if (chanCb == NULL) {
      goto exit;
   }

   if (mappingType == BUF_WRITEABLE ||
       mappingType == BUF_READWRITEABLE) {
      mapVa = chanCb->getWriteVa;
   } else {
      ASSERT(mappingType == BUF_READABLE);
      mapVa = chanCb->getReadVa;
   }

   /* Looks like we are in the middle of poweroff. */
   if (mapVa == NULL) {
      goto exit;
   }

   /* Establish guest memory mappings */
   if (!HSPUMapBuf(mapVa,
                   chanCb->putVa,
                   bufSize,
                   startIndex,
                   iovCount,
                   iov,
                   &iovMapped)) {
      /* Guest probably passed us bad physical address */
      goto exit;
   }

   if (iovMapped == 1) {
      /* A single page buffer is contiguous so hold on to guest mappings. */
      *buf = iov[startIndex].va;
      goto exit;
   }

   /* More than one page was mapped. */
   ASSERT(iov[startIndex].len < bufSize);

   LOG(10, ("%s: Hgfs Allocating buffer \n", __FUNCTION__));
   *buf = Util_SafeMalloc(bufSize);
   *isAllocated = TRUE;

   if ((mappingType == BUF_READABLE || mappingType == BUF_READWRITEABLE) &&
       (0 != dataSize)) {
      HSPUCopyIovecToBuf(iov, iovMapped, startIndex, *buf, dataSize);
   }
   releaseMappings = TRUE;


exit:
   if (releaseMappings) {
      HSPUUnmapBuf(chanCb->putVa, startIndex, iov, &iovMapped);
   }
   *iovMappedCount = iovMapped;

   return *buf;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_PutMetaPacket --
 *
 *    Free meta packet buffer if allocated.
 *    Guest mappings will be released.
 *
 * Results:
 *    void.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

void
HSPU_PutMetaPacket(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                   HgfsServerChannelCallbacks *chanCb)   // IN: Channel callbacks
{
   if (packet->metaPacket == NULL) {
      return;
   }

   LOG(4, ("%s Hgfs Putting Meta packet\n", __FUNCTION__));
   HSPUPutBuf(chanCb,
              packet->metaMappingType,
              packet->iov,
              packet->iovCount,
              0,
              packet->metaPacketDataSize,
              &packet->metaPacket,
              &packet->metaPacketIsAllocated,
              &packet->metaPacketMappedIov);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_SetDataPacketSize --
 *
 *    Set the size of the valid data in the data packet buffer.
 *
 * Results:
 *    void.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

void
HSPU_SetDataPacketSize(HgfsPacket *packet,            // IN/OUT: Hgfs Packet
                       size_t dataSize)               // IN: data size
{
   ASSERT(NULL != packet);
   ASSERT(dataSize <= packet->dataPacketSize);
   packet->dataPacketDataSize = dataSize;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_PutDataPacketBuf --
 *
 *    Free data packet buffer if allocated.
 *    Guest mappings will be released.
 *
 * Results:
 *    void.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

void
HSPU_PutDataPacketBuf(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                      HgfsServerChannelCallbacks *chanCb)   // IN: Channel callbacks
{
   if (packet->dataPacket == NULL) {
      return;
   }

   LOG(4, ("%s Hgfs Putting Data packet\n", __FUNCTION__));
   HSPUPutBuf(chanCb,
              packet->dataMappingType,
              packet->iov,
              packet->iovCount,
              packet->dataPacketIovIndex,
              packet->dataPacketDataSize,
              &packet->dataPacket,
              &packet->dataPacketIsAllocated,
              &packet->dataPacketMappedIov);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPUPutBuf --
 *
 *    Free buffer if allocated and release guest mappings.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

void
HSPUPutBuf(HgfsServerChannelCallbacks *chanCb,  // IN: Channel callbacks
           MappingType mappingType,             // IN: Access type Readable/Writeable
           HgfsVmxIov *iov,                     // IN: iov array
           uint32 iovCount,                     // IN: iov array size
           uint32 startIndex,                   // IN: Start index of iov
           size_t bufSize,                      // IN: Size of buffer
           void **buf,                          // OUT: Contigous buffer
           Bool *isAllocated,                   // OUT: Buffer allocated
           uint32 *iovMappedCount)              // OUT: iov mapped count
{
   ASSERT(buf != NULL);

   if (chanCb == NULL || chanCb->putVa == NULL) {
      goto exit;
   }

   if (*isAllocated &&
       (mappingType == BUF_WRITEABLE || mappingType == BUF_READWRITEABLE)) {
      /*
       * 1. Map the iov's if required for the size into host addresses.
       * 2. Write the buffer data into the iov host addresses.
       * 3. Unmap the iov's host virtual addresses.
       */
      if (0 == *iovMappedCount) {
         if (!HSPUMapBuf(chanCb->getWriteVa,
                         chanCb->putVa,
                         bufSize,
                         startIndex,
                         iovCount,
                         iov,
                         iovMappedCount)) {
            goto exit;
         }
      }
      HSPUCopyBufToIovec(iov, *iovMappedCount, startIndex, *buf, bufSize);
   }

   if (0 < *iovMappedCount) {
      HSPUUnmapBuf(chanCb->putVa, startIndex, iov, iovMappedCount);
   }

exit:
   if (*isAllocated) {
      LOG(10, ("%s: Hgfs Freeing buffer \n", __FUNCTION__));
      free(*buf);
      *isAllocated = FALSE;
   }

   *buf = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPUCopyBufToIovec --
 *
 *    Write out buffer to data Iovec.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    @iov is populated with contents of @buf
 *-----------------------------------------------------------------------------
 */

static void
HSPUCopyBufToIovec(HgfsVmxIov *iov,          // IN: iovs (array of mappings)
                   uint32 iovCount,          // IN: iov count of mappings
                   uint32 startIndex,        // IN: start index into iov
                   void *buf,                // IN: Contigous Buffer
                   size_t bufSize)           // IN: Size of buffer
{
   size_t iovIndex;
   size_t endIndex;
   size_t remainingSize;
   size_t copiedAmount = 0;

   ASSERT(buf != NULL);

   for (iovIndex = startIndex,  endIndex = startIndex + iovCount, remainingSize = bufSize;
        iovIndex < endIndex && remainingSize > 0;
        iovIndex++) {
      size_t copyAmount = remainingSize < iov[iovIndex].len ?
                          remainingSize: iov[iovIndex].len;

      ASSERT(iov[iovIndex].va != NULL);

      memcpy(iov[iovIndex].va, (char *)buf + copiedAmount, copyAmount);
      remainingSize -= copyAmount;
      copiedAmount += copyAmount;
   }

   ASSERT(remainingSize == 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPUCopyIovecToBuf --
 *
 *    Read Iovec into the buffer.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    @iov is populated with contents of @buf
 *-----------------------------------------------------------------------------
 */

static void
HSPUCopyIovecToBuf(HgfsVmxIov *iov,          // IN: iovs (array of mappings)
                   uint32 iovCount,          // IN: iov count of mappings
                   uint32 startIndex,        // IN: start index into iov
                   void *buf,                // IN: contigous Buffer
                   size_t bufSize)           // IN: size of buffer
{
   size_t iovIndex;
   size_t endIndex;
   size_t remainingSize;
   size_t copiedAmount = 0;

   for (iovIndex = startIndex, endIndex = startIndex + iovCount, remainingSize = bufSize;
        iovIndex < endIndex && remainingSize > 0;
        iovIndex++) {
      size_t copyAmount = remainingSize < iov[iovIndex].len ?
                          remainingSize : iov[iovIndex].len;

      memcpy((char *)buf + copiedAmount, iov[iovIndex].va, copyAmount);
      copiedAmount += copyAmount;
      remainingSize -= copyAmount;
   }
   ASSERT(copiedAmount == bufSize && remainingSize == 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPUMapBuf --
 *
 *    Map the buffer for the required size.
 *
 * Results:
 *    TRUE if we mapped the requested size and the number of mappings performed.
 *    Otherwise FALSE if something failed, and 0 mappings performed.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

static Bool
HSPUMapBuf(HgfsChannelMapVirtAddrFunc mapVa,    // IN: map virtual address function
           HgfsChannelUnmapVirtAddrFunc putVa,  // IN: unmap virtual address function
           size_t mapSize,                      // IN: size to map
           uint32 startIndex,                   // IN: Start index of iovs to map
           uint32 iovCount,                     // IN: iov count
           HgfsVmxIov *iov,                     // IN/OUT: iovs (array) to map
           uint32 *mappedCount)                 // OUT: mapped iov count
{
   uint32 iovIndex;
   uint32 mappedIovCount;
   size_t remainingSize;
   Bool mapped = TRUE;

   for (iovIndex = startIndex, mappedIovCount = 0, remainingSize = mapSize;
        iovIndex < iovCount && remainingSize > 0;
        iovIndex++, mappedIovCount++) {

      /* Check: Iov in VMCI should never cross page boundary */
      ASSERT(iov[iovIndex].len <= (PAGE_SIZE - PAGE_OFFSET(iov[iovIndex].pa)));

      iov[iovIndex].va = mapVa(&iov[iovIndex]);
      if (NULL == iov[iovIndex].va) {
         /* Failed to map the physical address. */
         break;
      }
      remainingSize = remainingSize < iov[iovIndex].len ?
                      0: remainingSize - iov[iovIndex].len;
   }

   if (0 != remainingSize) {
      /* Something failed in the mappings Undo any mappings we created. */
      HSPUUnmapBuf(putVa, startIndex, iov, &mappedIovCount);
      mapped = FALSE;
   }

   *mappedCount = mappedIovCount;
   return mapped;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPUUnmapBuf --
 *
 *    Unmap the buffer and release guest mappings.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *-----------------------------------------------------------------------------
 */

static void
HSPUUnmapBuf(HgfsChannelUnmapVirtAddrFunc unmapVa, // IN/OUT: Hgfs Packet
             uint32 startIndex,                    // IN: Start index of iovs to map
             HgfsVmxIov *iov,                      // IN/OUT: iovs (array) to map
             uint32 *mappedCount)                  // IN/OUT: iov count to unmap
{
   uint32 iovIndex;
   uint32 endIndex;

   for (iovIndex = startIndex, endIndex = startIndex + *mappedCount;
        iovIndex < endIndex;
        iovIndex++) {
      unmapVa(&iov[iovIndex].context);
      iov[iovIndex].va = NULL;
   }
   *mappedCount = 0;
}
