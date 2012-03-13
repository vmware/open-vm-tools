/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#define LOGLEVEL_MODULE hgfs
#include "loglevel_user.h"


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
HSPU_GetReplyPacket(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                    size_t *replyPacketSize,   // IN/OUT: Size of reply Packet
                    HgfsTransportSessionInfo *transportSession)  // IN: Session Info
{
   ASSERT(transportSession);
   if (packet->replyPacket) {
      /*
       * When we are transferring packets over backdoor, reply packet
       * is a static buffer. Backdoor should always return from here.
       */
      LOG(4, ("Existing reply packet %s %"FMTSZ"u %"FMTSZ"u\n", __FUNCTION__,
              *replyPacketSize, packet->replyPacketSize));
      ASSERT_DEVEL(*replyPacketSize <= packet->replyPacketSize);
   } else if (transportSession->channelCbTable && transportSession->channelCbTable->getWriteVa) {
     /* Can we write directly into guest memory ? */
      ASSERT_DEVEL(packet->metaPacket);
      if (packet->metaPacket) {
         LOG(10, ("%s Using meta packet for reply packet\n", __FUNCTION__));
         ASSERT_DEVEL(*replyPacketSize <= packet->metaPacketSize);
         packet->replyPacket = packet->metaPacket;
         packet->replyPacketSize = packet->metaPacketSize;
         LOG(10, ("%s Mapping meta packet for reply packet\n", __FUNCTION__));
         packet->replyPacket = HSPU_GetBuf(packet,
                                           0,
                                           &packet->metaPacket,
                                           packet->metaPacketSize,
                                           &packet->metaPacketIsAllocated,
                                           BUF_WRITEABLE,
                                           transportSession);
         /*
          * Really this can never happen, we would have caught bad physical address
          * during getMetaPacket.
          */
         ASSERT(packet->replyPacket);
         packet->replyPacketSize = packet->metaPacketSize;
      }
   } else {
      /* For sockets channel we always need to allocate buffer */
      LOG(10, ("%s Allocating reply packet\n", __FUNCTION__));
      packet->replyPacket = Util_SafeMalloc(*replyPacketSize);
      packet->replyPacketIsAllocated = TRUE;
      packet->replyPacketSize = *replyPacketSize;
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
HSPU_PutReplyPacket(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                    HgfsTransportSessionInfo *transportSession)  // IN: Session Info
{
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
HSPU_GetMetaPacket(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                   size_t *metaPacketSize,    // OUT: Size of metaPacket
                   HgfsTransportSessionInfo *transportSession)  // IN: Session Info
{
   *metaPacketSize = packet->metaPacketSize;
   return HSPU_GetBuf(packet, 0, &packet->metaPacket,
                      packet->metaPacketSize,
                      &packet->metaPacketIsAllocated,
                      BUF_READWRITEABLE, transportSession);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_GetDataPacketIov --
 *
 *    Get a data packet in an iov form given an hgfs packet.
 *    Guest mappings will be established.
 *
 * Results:
 *    Pointer to data packet iov.
 *
 * Side effects:
 *    Buffer may be allocated.
 *-----------------------------------------------------------------------------
 */

void *
HSPU_GetDataPacketIov(HgfsPacket *packet,       // IN/OUT: Hgfs Packet
                      HgfsTransportSessionInfo *transportSession, // IN: Session Info
                      HgfsVaIov iov)            // OUT: I/O vector
{
   NOT_IMPLEMENTED();
   return NULL;

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
HSPU_GetDataPacketBuf(HgfsPacket *packet,       // IN/OUT: Hgfs Packet
                      MappingType mappingType,  // IN: Writeable/Readable
                      HgfsTransportSessionInfo *transportSession) // IN: Session Info
{
   packet->dataMappingType = mappingType;
   return HSPU_GetBuf(packet, packet->dataPacketIovIndex,
                      &packet->dataPacket, packet->dataPacketSize,
                      &packet->dataPacketIsAllocated, mappingType, transportSession);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_GetBuf --
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

void *
HSPU_GetBuf(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
            uint32 startIndex,            // IN: start index of iov
            void **buf,                   // OUT: Contigous buffer
            size_t  bufSize,              // IN: Size of buffer
            Bool *isAllocated,            // OUT: Was buffer allocated ?
            MappingType mappingType,      // IN: Readable/Writeable ?
            HgfsTransportSessionInfo *transportSession)     // IN: Session Info
{
   uint32 iovCount;
   uint32 iovMapped = 0;
   int32 size = bufSize;
   int i;
   void* (*func)(uint64, uint32, char **);
   ASSERT(buf);

   if (*buf) {
      return *buf;
   } else if (bufSize == 0) {
      return NULL;
   }

   if (!transportSession->channelCbTable) {
      return NULL;
   }

   if (mappingType == BUF_WRITEABLE ||
       mappingType == BUF_READWRITEABLE) {
      func = transportSession->channelCbTable->getWriteVa;
   } else {
      ASSERT(mappingType == BUF_READABLE);
      func = transportSession->channelCbTable->getReadVa;
   }

   /* Looks like we are in the middle of poweroff. */
   if (func == NULL) {
      return NULL;
   }

   /* Establish guest memory mappings */
   for (iovCount = startIndex; iovCount < packet->iovCount && size > 0;
        iovCount++) {

      packet->iov[iovCount].token = NULL;

      /* Debugging check: Iov in VMCI should never cross page boundary */
      ASSERT_DEVEL(packet->iov[iovCount].len <=
      (PAGE_SIZE - PAGE_OFFSET(packet->iov[iovCount].pa)));

      packet->iov[iovCount].va = func(packet->iov[iovCount].pa,
                                      packet->iov[iovCount].len,
                                      &packet->iov[iovCount].token);
      ASSERT_DEVEL(packet->iov[iovCount].va);
      if (packet->iov[iovCount].va == NULL) {
         /* Guest probably passed us bad physical address */
         *buf = NULL;
         goto freeMem;
      }
      iovMapped++;
      size -= packet->iov[iovCount].len;
   }

   if (iovMapped > 1) {
      uint32 copiedAmount = 0;
      uint32 copyAmount;
      int32 remainingSize;
      int i;

      /* Seems like more than one page was requested. */
      ASSERT_DEVEL(packet->iov[startIndex].len < bufSize);
      *buf = Util_SafeMalloc(bufSize);
      *isAllocated = TRUE;

      LOG(10, ("%s: Hgfs Allocating buffer \n", __FUNCTION__));

      if (mappingType == BUF_READABLE ||
          mappingType == BUF_READWRITEABLE) {
         /*
          * Since we are allocating seperate buffer, it does not make sense
          * to continue to hold on to mappings. Let's release it, we will
          * reacquire mappings when we need in HSPU_CopyBufToIovec.
          */
         remainingSize = bufSize;
         for (i = startIndex; i < packet->iovCount && remainingSize > 0; i++) {
            copyAmount = remainingSize < packet->iov[i].len ?
                         remainingSize : packet->iov[i].len;
            memcpy((char *)*buf + copiedAmount, packet->iov[i].va, copyAmount);
            copiedAmount += copyAmount;
            remainingSize -= copyAmount;
         }
         ASSERT_DEVEL(copiedAmount == bufSize);
      }
   } else {
      /* We will continue to hold on to guest mappings */
      *buf = packet->iov[startIndex].va;
      return *buf;
   }

freeMem:
   for (i = startIndex; i < iovCount; i++) {
      transportSession->channelCbTable->putVa(&packet->iov[i].token);
      packet->iov[i].va = NULL;
   }

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
HSPU_PutMetaPacket(HgfsPacket *packet,       // IN/OUT: Hgfs Packet
                   HgfsTransportSessionInfo *transportSession) // IN: Session Info
{
   LOG(4, ("%s Hgfs Putting Meta packet\n", __FUNCTION__));
   HSPU_PutBuf(packet, 0, &packet->metaPacket,
               &packet->metaPacketSize,
               &packet->metaPacketIsAllocated,
               BUF_WRITEABLE, transportSession);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_PutDataPacketIov --
 *
 *    Free data packet Iov if allocated.
 *
 * Results:
 *    void.
 *
 * Side effects:
 *    Guest mappings will be released.
 *-----------------------------------------------------------------------------
 */

void
HSPU_PutDataPacketIov()
{
   NOT_IMPLEMENTED();
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
HSPU_PutDataPacketBuf(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                      HgfsTransportSessionInfo *transportSession)  // IN: Session Info
{

   LOG(4, ("%s Hgfs Putting Data packet\n", __FUNCTION__));
   HSPU_PutBuf(packet, packet->dataPacketIovIndex,
               &packet->dataPacket, &packet->dataPacketSize,
               &packet->dataPacketIsAllocated,
               packet->dataMappingType, transportSession);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_PutBuf --
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
HSPU_PutBuf(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
            uint32 startIndex,         // IN: Start of iov
            void **buf,                // IN/OUT: Buffer to be freed
            size_t *bufSize,           // IN: Size of the buffer
            Bool *isAllocated,         // IN: Was buffer allocated ?
            MappingType mappingType,   // IN: Readable / Writeable ?
            HgfsTransportSessionInfo *transportSession)  // IN: Session info
{
   uint32 iovCount = 0;
   int size = *bufSize;
   ASSERT(buf);

   if (!transportSession->channelCbTable) {
      return;
   }

   if (!transportSession->channelCbTable->putVa || *buf == NULL) {
      return;
   }

   if (*isAllocated) {
      if (mappingType == BUF_WRITEABLE) {
         HSPU_CopyBufToIovec(packet, startIndex, *buf, *bufSize, transportSession);
      }
      LOG(10, ("%s: Hgfs Freeing buffer \n", __FUNCTION__));
      free(*buf);
      *isAllocated = FALSE;
   } else {
      for (iovCount = startIndex;
           iovCount < packet->iovCount && size > 0;
           iovCount++) {
         ASSERT_DEVEL(packet->iov[iovCount].token);
         transportSession->channelCbTable->putVa(&packet->iov[iovCount].token);
         size -= packet->iov[iovCount].len;
      }
      LOG(10, ("%s: Hgfs bufSize = %d \n", __FUNCTION__, size));
      ASSERT(size <= 0);
   }
   *buf = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_CopyBufToMetaIovec --
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

void
HSPU_CopyBufToMetaIovec(HgfsPacket *packet,      // IN/OUT: Hgfs packet
                        void *buf,               // IN: Buffer to copy from
                        size_t bufSize,          // IN: Size of buffer
                        HgfsTransportSessionInfo *transportSession)// IN: Session Info
{
   HSPU_CopyBufToIovec(packet, 0, buf, bufSize, transportSession);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_CopyBufToDataIovec --
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

void
HSPU_CopyBufToDataIovec(HgfsPacket *packet,   // IN: Hgfs packet
                        void *buf,            // IN: Buffer to copy from
                        uint32 bufSize,       // IN: Size of buffer
                        HgfsTransportSessionInfo *transportSession)
{
   HSPU_CopyBufToIovec(packet, packet->dataPacketIovIndex, buf, bufSize,
                       transportSession);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HSPU_CopyBufToDataIovec --
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

void
HSPU_CopyBufToIovec(HgfsPacket *packet,       // IN/OUT: Hgfs Packet
                    uint32 startIndex,        // IN: start index into iov
                    void *buf,                // IN: Contigous Buffer
                    size_t bufSize,           // IN: Size of buffer
                    HgfsTransportSessionInfo *transportSession) // IN: Session Info
{
   uint32 iovCount;
   size_t remainingSize = bufSize;
   size_t copyAmount;
   size_t copiedAmount = 0;

   ASSERT(packet);
   ASSERT(buf);

   if (!transportSession->channelCbTable) {
      return;
   }

   ASSERT_DEVEL(transportSession->channelCbTable->getWriteVa);
   if (!transportSession->channelCbTable->getWriteVa) {
      return;
   }

   for (iovCount = startIndex; iovCount < packet->iovCount
        && remainingSize > 0; iovCount++) {
      copyAmount = remainingSize < packet->iov[iovCount].len ?
                   remainingSize: packet->iov[iovCount].len;

      packet->iov[iovCount].token = NULL;

      /* Debugging check: Iov in VMCI should never cross page boundary */
      ASSERT_DEVEL(packet->iov[iovCount].len <=
                  (PAGE_SIZE - PAGE_OFFSET(packet->iov[iovCount].pa)));

      packet->iov[iovCount].va = transportSession->channelCbTable->getWriteVa(packet->iov[iovCount].pa,
                                                     packet->iov[iovCount].len,
                                                     &packet->iov[iovCount].token);
      ASSERT_DEVEL(packet->iov[iovCount].va);
      if (packet->iov[iovCount].va != NULL) {
         memcpy(packet->iov[iovCount].va, (char *)buf + copiedAmount, copyAmount);
         transportSession->channelCbTable->putVa(&packet->iov[iovCount].token);
         remainingSize -= copyAmount;
         copiedAmount += copyAmount;
      } else {
         break;
      }
   }

   ASSERT_DEVEL(remainingSize == 0);
}


