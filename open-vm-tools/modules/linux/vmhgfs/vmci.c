/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmci.c --
 *
 * Provides VMCI transport channel to the HGFS client.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <asm/io.h>

#include "compat_mm.h"
#include "hgfsProto.h"
#include "hgfsTransport.h"
#include "module.h"
#include "request.h"
#include "transport.h"
#include "vm_assert.h"
#include "vmci_call_defs.h"
#include "vmci_defs.h"
#include "vmciGuestKernelAPI.h"

static Bool HgfsVmciChannelOpen(HgfsTransportChannel *channel);
static void HgfsVmciChannelClose(HgfsTransportChannel *channel);
static HgfsReq * HgfsVmciChannelAllocate(size_t payloadSize);
void HgfsVmciChannelFree(HgfsReq *req);
static int HgfsVmciChannelSend(HgfsTransportChannel *channel, HgfsReq *req);

int USE_VMCI = 0;
module_param(USE_VMCI, int, 0444);

static HgfsTransportChannel channel = {
   .name = "vmci",
   .ops.open = HgfsVmciChannelOpen,
   .ops.close = HgfsVmciChannelClose,
   .ops.allocate = HgfsVmciChannelAllocate,
   .ops.free = HgfsVmciChannelFree,
   .ops.send = HgfsVmciChannelSend,
   .priv = NULL,
   .status = HGFS_CHANNEL_NOTCONNECTED
};


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelCallback --
 *
 *      Called when VMCI datagram is received.
 *
 * Results:
 *      Always 0.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int HgfsVmciChannelCallback(void *data, VMCIDatagram *dg)
{
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelOpen --
 *
 *      Open VMCI channel.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsVmciChannelOpen(HgfsTransportChannel *channel) // IN: Channel
{
   HgfsVmciTransportHeader transportHeader;
   VMCIDatagram *dg;
   int ret;

   ASSERT(channel->status == HGFS_CHANNEL_NOTCONNECTED);
   ASSERT(channel->priv == NULL);

   if (USE_VMCI == 0) {
      return FALSE;
   }

   channel->priv = kmalloc(sizeof(VMCIHandle), GFP_KERNEL);
   if (NULL == channel->priv) {
      return FALSE;
   }

   ret = VMCIDatagram_CreateHnd(VMCI_INVALID_ID,        /* Resource ID */
                                VMCI_FLAG_DG_NONE,      /* Flags */
                                HgfsVmciChannelCallback,/* Datagram Recv Callback*/
                                NULL,                   /* Callback data */
                                channel->priv);         /* VMCI outhandle */
   if (ret != VMCI_SUCCESS) {
      LOG(1, (KERN_WARNING "Failed to create VMCI handle %d\n", ret));
      kfree(channel->priv);
      return FALSE;
   }

   transportHeader.version = HGFS_VMCI_VERSION_1;
   transportHeader.iovCount = 0;

   /*
    * Send a datagram to the VMX with the HgfsTransportHeader as the datagram
    * payload
    */
   dg = kmalloc(sizeof *dg + sizeof transportHeader, GFP_KERNEL);
   if (NULL == dg) {
      LOG(4, (KERN_WARNING "%s failed to allocate\n", __func__));
      VMCIDatagram_DestroyHnd(*(VMCIHandle *)channel->priv);
      kfree(channel->priv);
      return FALSE;
   }

   memcpy(VMCI_DG_PAYLOAD(dg), &transportHeader, sizeof transportHeader);

   dg->src = *(VMCIHandle *)channel->priv;
   dg->dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, VMCI_HGFS_TRANSPORT);
   dg->payloadSize = sizeof transportHeader;

   if ((ret = VMCIDatagram_Send(dg)) < VMCI_SUCCESS) {
      LOG(4, (KERN_WARNING "Failure with %d\n", ret));
      VMCIDatagram_DestroyHnd(*(VMCIHandle *)channel->priv);
      kfree(dg);
      kfree(channel->priv);
      return FALSE;
   }

   kfree(dg);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelClose --
 *
 *      Destroy vmci handle.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsVmciChannelClose(HgfsTransportChannel *channel) // IN: Channel
{
   ASSERT(channel->priv != NULL);

   VMCIDatagram_DestroyHnd(*(VMCIHandle *)channel->priv);
   kfree(channel->priv);
   channel->priv = NULL;

   LOG(8, ("VMware hgfs: %s: vmci closed.\n", __func__));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelAllocate --
 *
 *      Allocate request in the way that is suitable for sending through
 *      vmci. Today, we just allocate a page for the request and we ignore
 *      payloadSize. We need this to support variable sized requests in future.
 *
 * Results:
 *      NULL on failure; otherwise address of the new request.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsReq *
HgfsVmciChannelAllocate(size_t payloadSize) // IN: Ignored
{
   HgfsReq *req = NULL;
   const size_t size = PAGE_SIZE;

   req = kmalloc(size, GFP_KERNEL);
   if (likely(req)) {
      req->payload = req->buffer + sizeof (HgfsVmciTransportStatus);
      req->bufferSize = size - sizeof (HgfsVmciTransportStatus) - sizeof *req;
   }

   /* We asked for PAGE_SIZE, it should be page aligned */
   ASSERT(((long)req & 0x00000fff) == 0);
   LOG(10, (KERN_WARNING "%s: Allocated Request\n", __func__));
   return req;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelFree --
 *
 *     Free previously allocated request.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsVmciChannelFree(HgfsReq *req)
{
   ASSERT(req);
   kfree(req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsVmciChannelSend --
 *
 *     Send a request via vmci.
 *
 * Results:
 *     0 on success, negative error on failure.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsVmciChannelSend(HgfsTransportChannel *channel, // IN: Channel
                    HgfsReq *req)                  // IN: request to send
{
   int ret;
   int iovCount = 0;
   HgfsReply *reply;
   VMCIDatagram *dg;
   HgfsVmciTransportHeader *transportHeader;
   HgfsVmciTransportStatus *transportStatus;
   size_t transportHeaderSize;
   size_t bufferSize;
   size_t total;
   uint64 pa;
   uint64 len;
   size_t va;
   int j;

   ASSERT(req);
   ASSERT(req->state == HGFS_REQ_STATE_UNSENT || req->state == HGFS_REQ_STATE_ALLOCATED);
   ASSERT(req->payloadSize <= req->bufferSize);

   LOG(4, ("VMware hgfs: %s: VMCI sending.\n", __func__));

   /*
    +------------+
    +   page 1   + <----- We can have request starting from here
    +------------+
    +   page 2   +
    +------------+
    +   page 3   + <----- ..and ending here
    +------------+
    */

   /* Note that req->bufferSize does not include chunk used by the transport. */
   total = req->bufferSize + sizeof (HgfsVmciTransportStatus);
   bufferSize = 0;

   /* Calculate number of entries for metaPacket */
   iovCount = 1;
   va = (size_t)req->buffer;
   len = total < (PAGE_SIZE - va % PAGE_SIZE) ? total : (PAGE_SIZE - va % PAGE_SIZE);
   total -= len;
   iovCount += (total + PAGE_SIZE - 1)/ PAGE_SIZE;

   ASSERT(iovCount >= 1);
   transportHeaderSize = sizeof *transportHeader +
                         (iovCount + req->numEntries - 1) * sizeof (HgfsIov);
   dg = kmalloc(sizeof *dg + transportHeaderSize, GFP_KERNEL);
   if (NULL == dg) {
      LOG(4, (KERN_WARNING "%s failed to allocate\n", __func__));
      return -ENOMEM;
   }

   /* Initialize datagram */
   dg->src = *(VMCIHandle *)channel->priv;
   dg->dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, VMCI_HGFS_TRANSPORT);
   dg->payloadSize = transportHeaderSize;

   transportHeader = VMCI_DG_PAYLOAD(dg);

   /* Initialize transport header */
   transportHeader->version = HGFS_VMCI_VERSION_1;

   total = req->bufferSize + sizeof (HgfsVmciTransportStatus);
   bufferSize = 0;
   for (iovCount = 0; bufferSize < req->bufferSize; iovCount++) {
      /*
       * req->buffer should have been allocated by kmalloc()/ __get_free_pages().
       * Specifically, it cannot be a buffer that is mapped from high memory.
       * virt_to_phys() does not work for those.
       */
      pa = virt_to_phys(req->buffer + bufferSize);
      len = total < (PAGE_SIZE - pa % PAGE_SIZE) ? total : (PAGE_SIZE - pa % PAGE_SIZE);
      bufferSize += len;
      total -= len;
      transportHeader->iov[iovCount].pa = pa;
      transportHeader->iov[iovCount].len = len;
      LOG(8, ("iovCount = %u PA = %"FMT64"x len=%u\n", iovCount,
              transportHeader->iov[iovCount].pa, transportHeader->iov[iovCount].len));
   }

   /* Right now we do not expect discontigous request packet */
   ASSERT(iovCount == 1);
   ASSERT(total == 0);
   ASSERT(bufferSize == req->bufferSize + sizeof (HgfsVmciTransportStatus));

   LOG(8, ("Size of request is %Zu %Zu\n", req->payloadSize, sizeof (HgfsRequest)));

   for (j = 0; j < req->numEntries; j++, iovCount++) {
      /* I will have to probably do page table walk here, haven't figured it out yet */
      transportHeader->iov[iovCount].pa = page_to_phys(req->dataPacket[j].page);
      transportHeader->iov[iovCount].pa += req->dataPacket[j].offset;
      transportHeader->iov[iovCount].len = req->dataPacket[j].len;
      LOG(8, ("iovCount = %u PA = %"FMT64"x len=%u\n", iovCount,
              transportHeader->iov[iovCount].pa,
              transportHeader->iov[iovCount].len));
   }

   transportHeader->iovCount = iovCount;

   /* Initialize transport Status */
   transportStatus = (HgfsVmciTransportStatus *)req->buffer;
   transportStatus->status = HGFS_VMCI_IO_PENDING;
   transportStatus->flags = 0;
   transportStatus->size = req->bufferSize + sizeof (HgfsVmciTransportStatus);

   LOG(8, (KERN_WARNING "Physical addr is %"FMT64"x len=%u iovCount=%u numEntries=%u\n",
           transportHeader->iov[0].pa,
           transportHeader->iov[0].len,
           transportHeader->iovCount,
           req->numEntries));
   LOG(8, (KERN_WARNING "Id = %u op = %u\n",
           ((HgfsRequest *)req->payload)->id,
           ((HgfsRequest *)req->payload)->op));

   if((ret = VMCIDatagram_Send(dg)) < VMCI_SUCCESS) {
      if (ret == HGFS_VMCI_TRANSPORT_ERROR) {
         LOG(0, (KERN_WARNING "HGFS Transport error occured. Don't blame VMCI\n"));
      }
      req->state = HGFS_REQ_STATE_UNSENT;
      kfree(dg);
      return -EIO;
   }

   LOG(8, (KERN_WARNING "VMware hgfs: %s: VMCI reply received.\n", __func__));

   /* For HgfsVmciStage2 everything should complete sync. */
   ASSERT(transportStatus->status == HGFS_VMCI_IO_COMPLETE);

   if (transportStatus->status == HGFS_VMCI_IO_COMPLETE) {
      reply = (HgfsReply *)req->payload;
      req->payloadSize = transportStatus->size;
      ASSERT(transportStatus->size <= (req->bufferSize + sizeof (HgfsVmciTransportStatus)));
      HgfsCompleteReq(req);
      LOG(8, (KERN_WARNING "IO_COMPLETE: id = %u status = %u\n",
              (uint32)reply->id, (uint32)reply->status));
   }

   kfree(dg);
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetVmciChannel --
 *
 *     Initialize Vmci channel.
 *
 * Results:
 *     Always return pointer to Vmci channel.
 *
 * Side effects:
 *     None
 *
 *----------------------------------------------------------------------
 */

HgfsTransportChannel*
HgfsGetVmciChannel(void)
{
   return &channel;
}
