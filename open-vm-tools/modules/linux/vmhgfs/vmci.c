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
#include <linux/interrupt.h>  /* for spin_lock_bh */
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
#include "vmciKernelAPI1.h"

static Bool HgfsVmciChannelOpen(HgfsTransportChannel *channel);
static void HgfsVmciChannelClose(HgfsTransportChannel *channel);
static HgfsReq * HgfsVmciChannelAllocate(size_t payloadSize);
void HgfsVmciChannelFree(HgfsReq *req);
static int HgfsVmciChannelSend(HgfsTransportChannel *channel, HgfsReq *req);
static void HgfsRequestAsyncDispatch(char *payload, uint32 size);

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

static spinlock_t vmciRequestProcessLock;

typedef struct HgfsShmemPage {
   uint64 va;
   uint64 pa;
   Bool free;
} HgfsShmemPage;

typedef struct HgfsShmemPages {
   HgfsShmemPage *list;
   uint32 totalPageCount;
   uint32 freePageCount;
} HgfsShmemPages;

HgfsShmemPages gHgfsShmemPages;
#define HGFS_VMCI_SHMEM_PAGES (16)


/*
 *----------------------------------------------------------------------
 *
 * HgfsRequestAsyncDispatch --
 *
 *   XXX Main dispatcher function. Currently just a stub. Needs to run
 *   in atomic context.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsRequestAsyncDispatch(char *payload, // IN: request header
                         uint32 size)   // IN: size of payload
{
   HgfsRequest *reqHeader = (HgfsRequest *)payload;

   LOG(4, (KERN_WARNING "Size in Dispatch %u\n", size));

   switch (reqHeader->op) {
   case HGFS_OP_NOTIFY_V4: {
      LOG(4, (KERN_WARNING "Calling HGFS_OP_NOTIFY_V4 dispatch function\n"));
      break;
   }
   default:
      LOG(4, (KERN_WARNING "%s: Unknown opcode = %d", __func__, reqHeader->op));
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRequestAsyncShmemDispatch --
 *
 *    Shared memory dispatcher. It extracts packets from the shared
 *    memory and dispatches to the main hgfs dispatcher function. When
 *    the buffer is larger than 4K, we may fail do deliver notifications.
 *    Main dispatcher function should run in atomic context.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsRequestAsyncShmemDispatch(HgfsAsyncIov *iov, // IN: request vectors
                              uint32 count)      // IN: number of iovs
{
   uint32 i;
   char *buf = NULL;
   uint32 size = 0;
   Bool chainStarted = FALSE;
   uint32 offset = 0;
   uint32 copySize;
   uint64 prevIndex = -1;
   uint64 currIndex;
   size_t va;

   LOG(10, (KERN_WARNING "%s count = %u\n",__FUNCTION__, count));

   /*
    * When requests cross 4K boundary we have to chain pages together
    * since guest passes 4k pages to the host. Here is how chaining works
    *
    * - All the vectors except the last one in the chain sets iov[].chain
    * to TRUE.
    * - Every iov[].len field indicates remaining bytes. So the first
    * vector will contain total size of the request while the last vector
    * will contain only size of data present in last vector.
    */

   for (i = 0; i < count; i++) {
      va = (size_t)iov[i].va;
      currIndex = iov[i].index;

      if (LIKELY(!iov[i].chain)) {
         /* When the chain ends we dispatch the datagram.*/
         if (!chainStarted) {
            buf = (char *)va;
            LOG(8, (KERN_WARNING " Chain wasn't started...\n"));
            size = iov[i].len;
         } else {
            memcpy(buf + offset, (char *)va, iov[i].len);
         }
         ASSERT(buf && size);
         HgfsRequestAsyncDispatch(buf, size);
         if (chainStarted) {
            /* Well chain just ended, we shall free the buffer. */
            chainStarted = FALSE;
            kfree(buf);
         }
      } else {
           if (!chainStarted) {
              LOG(8, (KERN_WARNING "Started chain ...\n"));
              size = iov[i].len;
              buf = kmalloc(size, GFP_ATOMIC);
              ASSERT_DEVEL(buf);
              if (!buf) {
                 /* Skip this notification, move onto next. */
                 i += (size - 1) / PAGE_SIZE;
                 continue;
              }
              chainStarted = TRUE;
              offset = 0;
           }
           copySize = MIN(iov[i].len, PAGE_SIZE);
           memcpy(buf + offset, (char *)va, copySize);
           offset += copySize;
      }

      if (currIndex != prevIndex) {
         /* This is new page. Mark is as free. */
         gHgfsShmemPages.list[currIndex].free = TRUE;
         gHgfsShmemPages.freePageCount++;
      }
      prevIndex = currIndex;
   }

   ASSERT(gHgfsShmemPages.freePageCount <= gHgfsShmemPages.totalPageCount);
   LOG(8, (KERN_WARNING "Page count %u %u ...\n", gHgfsShmemPages.freePageCount,
           gHgfsShmemPages.totalPageCount));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelPassGuestPages --
 *
 *      Passes down free pages to the hgfs Server. HgfsServer will use this pages
 *      for sending change notification, oplock breaks etc.
 *
 *      XXX It seems safe to call VMCIDatagram_Send in atomic context.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsVmciChannelPassGuestPages(HgfsTransportChannel *channel) // IN:
{
   Bool retVal = TRUE;
   int ret;
   int i;
   int j = 0;
   size_t transportHeaderSize;
   HgfsVmciTransportHeader *transportHeader = NULL;
   VMCIDatagram *dg;

   if (!gHgfsShmemPages.freePageCount) {
      return TRUE;
   }

   transportHeaderSize = sizeof (HgfsVmciTransportHeader) +
          (gHgfsShmemPages.freePageCount - 1) * sizeof (HgfsAsyncIov);

   dg = kmalloc(sizeof *dg + transportHeaderSize, GFP_ATOMIC);
   if (!dg) {
      LOG(4, (KERN_WARNING "%s failed to allocate\n", __func__));
      retVal = FALSE;
      goto exit;
   }

   transportHeader = VMCI_DG_PAYLOAD(dg);

   for (i = 0; i < gHgfsShmemPages.totalPageCount; i++) {
      if (gHgfsShmemPages.list[i].free) {
         transportHeader->asyncIov[j].index = i;
         transportHeader->asyncIov[j].va = gHgfsShmemPages.list[i].va;
         transportHeader->asyncIov[j].pa = gHgfsShmemPages.list[i].pa;
         transportHeader->asyncIov[j].len = PAGE_SIZE;
         j++;
      }
   }

   dg->src = *(VMCIHandle *)channel->priv;
   dg->dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, VMCI_HGFS_TRANSPORT);
   dg->payloadSize = transportHeaderSize;

   transportHeader->version = HGFS_VMCI_VERSION_1;
   ASSERT(gHgfsShmemPages.freePageCount == j);
   transportHeader->iovCount = j;
   transportHeader->pktType = HGFS_TH_REP_GET_PAGES;

   LOG(10, (KERN_WARNING "Sending %d Guest pages \n", i));
   if ((ret = VMCIDatagram_Send(dg)) < VMCI_SUCCESS) {
      if (ret == HGFS_VMCI_TRANSPORT_ERROR) {
         LOG(0, (KERN_WARNING "HGFS Transport error occured. Don't blame VMCI\n"));
      }
      retVal = FALSE;
   }

exit:
   if (retVal) {
      /* We successfully sent pages the the host. Mark all pages as allocated */
      for (i = 0; i < gHgfsShmemPages.totalPageCount; i++) {
         gHgfsShmemPages.list[i].free = FALSE;
      }
      gHgfsShmemPages.freePageCount = 0;
   }
   kfree(dg);
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelCompleteRequest --
 *
 *      Completes the request that was serviced asynchronously by the server.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Request may be removed from the queue and sleeping thread is woken up.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsVmciChannelCompleteRequest(uint64 id) // IN: Request ID
{
   HgfsVmciTransportStatus *transportStatus;
   HgfsReq *req;

   spin_lock_bh(&vmciRequestProcessLock);

   /* Reference is taken here */
   req = HgfsTransportGetPendingRequest(id);
   if (!req) {
      LOG(0, (KERN_WARNING "No request with id %"FMT64"u \n", id));
      goto exit;
   }

   transportStatus = (HgfsVmciTransportStatus *)req->buffer;
   if (transportStatus->status != HGFS_TS_IO_COMPLETE) {
      LOG(0, (KERN_WARNING "Request not completed with id %"FMT64"u \n", id));
      goto exit;
   }

   /* Request is completed (yay!), let's remove it from the list */
   HgfsTransportRemovePendingRequest(req);

   req->payloadSize = transportStatus->size;
   HgfsCompleteReq(req);

exit:
   if (req) {
      /* Drop the reference taken in *GetPendingRequest */
      HgfsRequestPutRef(req);
   }
   spin_unlock_bh(&vmciRequestProcessLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelCallback --
 *
 *      Called when VMCI datagram is received. Note: This function runs inside
 *      tasklet. It means that this function cannot run concurrently with
 *      itself, thus it is safe to manipulate gHgfsShmemPages without locks. If this
 *      ever changes, please consider using appropriate locks.
 *
 * Results:
 *      0 on Success, < 0 on Failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int HgfsVmciChannelCallback(void *data,       // IN: unused
                                   VMCIDatagram *dg) // IN: datagram
{
   HgfsVmciAsyncReply *reply  = (HgfsVmciAsyncReply *)VMCI_DG_PAYLOAD(dg);
   HgfsTransportChannel *channel;

   LOG(10, (KERN_WARNING "Received VMCI channel Callback \n"));

   if (reply->version != HGFS_VMCI_VERSION_1) {
      return HGFS_VMCI_VERSION_MISMATCH;
   }

   switch (reply->pktType) {

   case HGFS_ASYNC_IOREP:
      LOG(10, (KERN_WARNING "Received ID%"FMT64"x \n", reply->response.id));
      HgfsVmciChannelCompleteRequest(reply->response.id);
      break;

   case HGFS_ASYNC_IOREQ_SHMEM:
      HgfsRequestAsyncShmemDispatch(reply->shmem.iov, reply->shmem.count);
      break;

   case HGFS_ASYNC_IOREQ_GET_PAGES:
      channel = HgfsGetVmciChannel();
      LOG(10, (KERN_WARNING "Should send pages to the host\n"));
      HgfsVmciChannelPassGuestPages(channel);
      break;

   default:
      ASSERT(0);
      return HGFS_VMCI_TRANSPORT_ERROR;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelOpen --
 *
 *      Opens VMCI channel and passes guest pages to the host.
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
   int ret;
   int i;

   ASSERT(channel->status == HGFS_CHANNEL_NOTCONNECTED);
   ASSERT(channel->priv == NULL);
   memset(&gHgfsShmemPages, 0, sizeof gHgfsShmemPages);

   if (USE_VMCI == 0) {
      goto error;
   }

   spin_lock_init(&vmciRequestProcessLock);

   channel->priv = kmalloc(sizeof(VMCIHandle), GFP_KERNEL);
   if (!channel->priv) {
      goto error;
   }

   ret = VMCIDatagram_CreateHnd(VMCI_INVALID_ID,        /* Resource ID */
                                VMCI_FLAG_DG_NONE,      /* Flags */
                                HgfsVmciChannelCallback,/* Datagram Recv Callback */
                                NULL,                   /* Callback data */
                                channel->priv);         /* VMCI outhandle */
   if (ret != VMCI_SUCCESS) {
      LOG(1, (KERN_WARNING "Failed to create VMCI handle %d\n", ret));
      goto error;
   }

   gHgfsShmemPages.list = kmalloc(sizeof *gHgfsShmemPages.list * HGFS_VMCI_SHMEM_PAGES,
                                  GFP_KERNEL);
   if (!gHgfsShmemPages.list) {
      goto error;
   }

   memset(gHgfsShmemPages.list, 0, sizeof *gHgfsShmemPages.list * HGFS_VMCI_SHMEM_PAGES);

   for (i = 0; i < HGFS_VMCI_SHMEM_PAGES; i++) {
      gHgfsShmemPages.list[i].va = __get_free_page(GFP_KERNEL);
      if (!gHgfsShmemPages.list[i].va) {
         LOG(1, (KERN_WARNING "__get_free_page returned error \n"));
         if (i == 0) {
            /* Ouch. We failed on first call to __get_free_page */
            goto error;
         }
         /* It's ok. We can still send few pages to the host */
         break;
      }
      gHgfsShmemPages.list[i].pa = virt_to_phys((void *)(size_t)gHgfsShmemPages.list[i].va);
      gHgfsShmemPages.list[i].free = TRUE;
   }

   gHgfsShmemPages.totalPageCount = i;
   gHgfsShmemPages.freePageCount = i;

   ret = HgfsVmciChannelPassGuestPages(channel);
   if (!ret) {
      for (i = 0; i < gHgfsShmemPages.totalPageCount; i++) {
         LOG(1, (KERN_WARNING "Freeing pages\n"));
         free_page(gHgfsShmemPages.list[i].va);
      }
      LOG(1, (KERN_WARNING "Failed to pass pages to the guest %d\n", ret));
      goto error;
   }

   return TRUE;

error:
   kfree(gHgfsShmemPages.list);
   kfree(channel->priv);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVmciChannelTerminateSession --
 *
 *      Terminate session with the server.
 *
 * Results:
 *      0 on success and < 0 on error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsVmciChannelTerminateSession(HgfsTransportChannel *channel) {

   int ret = 0;
   VMCIDatagram *dg;
   HgfsVmciTransportHeader *transportHeader;

   dg = kmalloc(sizeof *dg + sizeof *transportHeader, GFP_KERNEL);
   if (NULL == dg) {
      LOG(4, (KERN_WARNING "%s failed to allocate\n", __func__));
      return -ENOMEM;
   }

   /* Initialize datagram */
   dg->src = *(VMCIHandle *)channel->priv;
   dg->dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, VMCI_HGFS_TRANSPORT);
   dg->payloadSize = sizeof *transportHeader;

   transportHeader = VMCI_DG_PAYLOAD(dg);
   transportHeader->version = HGFS_VMCI_VERSION_1;
   transportHeader->iovCount = 0;
   transportHeader->pktType = HGFS_TH_TERMINATE_SESSION;

   LOG(1, (KERN_WARNING "Terminating session with host \n"));
   if ((ret = VMCIDatagram_Send(dg)) < VMCI_SUCCESS) {
      if (ret == HGFS_VMCI_TRANSPORT_ERROR) {
         LOG(0, (KERN_WARNING "HGFS Transport error occured. Don't blame VMCI\n"));
      }
      LOG(0, (KERN_WARNING "Cannot communicate with Server.\n"));
   } else {
      int i;
      for (i = 0; i < gHgfsShmemPages.totalPageCount; i++) {
         free_page(gHgfsShmemPages.list[i].va);
      }
   }

   kfree(dg);
   return ret;
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
   HgfsVmciChannelTerminateSession(channel);
   VMCIDatagram_DestroyHnd(*(VMCIHandle *)channel->priv);
   kfree(channel->priv);
   kfree(gHgfsShmemPages.list);
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
   VMCIDatagram *dg;
   HgfsVmciTransportHeader *transportHeader;
   HgfsVmciTransportStatus *transportStatus;
   size_t transportHeaderSize;
   size_t bufferSize;
   size_t total;
   uint64 pa;
   uint64 len;
   uint64 id;
   int j;

   ASSERT(req);
   ASSERT(req->buffer);
   ASSERT(req->state == HGFS_REQ_STATE_UNSENT || req->state == HGFS_REQ_STATE_ALLOCATED);
   ASSERT(req->payloadSize <= req->bufferSize);

   /* Note that req->bufferSize does not include chunk used by the transport. */
   total = req->bufferSize + sizeof (HgfsVmciTransportStatus);

   /* Calculate number of entries for metaPacket */
   iovCount = (total + (size_t)req->buffer % PAGE_SIZE - 1)/ PAGE_SIZE + 1;
   ASSERT(total + (size_t)req->buffer % PAGE_SIZE <= PAGE_SIZE);

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

   LOG(0, (KERN_WARNING "Size of request is %Zu\n", req->payloadSize));

   for (j = 0; j < req->numEntries; j++, iovCount++) {
      /* I will have to probably do page table walk here, haven't figured it out yet */
      ASSERT(req->dataPacket);
      transportHeader->iov[iovCount].pa = page_to_phys(req->dataPacket[j].page);
      transportHeader->iov[iovCount].pa += req->dataPacket[j].offset;
      transportHeader->iov[iovCount].len = req->dataPacket[j].len;
      LOG(8, ("iovCount = %u PA = %"FMT64"x len=%u\n", iovCount,
              transportHeader->iov[iovCount].pa,
              transportHeader->iov[iovCount].len));
   }

   transportHeader->iovCount = iovCount;
   transportHeader->pktType = HGFS_TH_REQUEST;

   /* Initialize transport Status */
   transportStatus = (HgfsVmciTransportStatus *)req->buffer;
   transportStatus->status = HGFS_TS_IO_PENDING;
   transportStatus->size = req->bufferSize + sizeof (HgfsVmciTransportStatus);

   /*
    * Don't try to set req->state after VMCIDatagram_Send().
    * It may be too late then. We could have received a datagram by then and
    * datagram handler expects request's state to be submitted.
    */
   req->state = HGFS_REQ_STATE_SUBMITTED;
   id = req->id;

   if ((ret = VMCIDatagram_Send(dg)) < VMCI_SUCCESS) {
      if (ret == HGFS_VMCI_TRANSPORT_ERROR) {
         LOG(0, (KERN_WARNING "HGFS Transport error occured. Don't blame VMCI\n"));
      } else if (ret == HGFS_VMCI_VERSION_MISMATCH) {
         LOG(0, (KERN_WARNING "Version mismatch\n"));
      }
      req->state = HGFS_REQ_STATE_UNSENT;
      kfree(dg);
      return -EIO;
   }

   LOG(0, (KERN_WARNING "Hgfs Received response\n"));
   HgfsVmciChannelCompleteRequest(id);

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
