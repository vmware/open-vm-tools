/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * request.h --
 *
 * Functions dealing with the creation, deletion, and sending of HGFS
 * requests are defined here.
 */

#ifndef _HGFS_DRIVER_REQUEST_H_
#define _HGFS_DRIVER_REQUEST_H_

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/wait.h>
#include "compat_sched.h"
#include "compat_spinlock.h"

#include "hgfs.h" /* For common HGFS definitions. */
#include "vm_basic_types.h"
#include "vm_basic_defs.h"

/* Macros for accessing the payload portion of the HGFS request packet. */
#define HGFS_REQ_PAYLOAD(hgfsReq) ((hgfsReq)->payload)

/* XXX: Needs change when VMCI is supported. */
#define HGFS_REQ_PAYLOAD_V3(hgfsReq) (HGFS_REQ_PAYLOAD(hgfsReq) + sizeof(HgfsRequest))
#define HGFS_REP_PAYLOAD_V3(hgfsRep) (HGFS_REQ_PAYLOAD(hgfsRep) + sizeof(HgfsReply))

/*
 * HGFS_REQ_STATE_ALLOCATED:
 *    The filesystem half has allocated the request from the slab
 *    allocator. The request is not on any list.
 *
 * HGFS_REQ_STATE_UNSENT:
 *    The filesystem half of the driver has filled in the request fields
 *    and placed the request in the global unsent list. It is now the
 *    request handler's responsibility to submit this request to
 *    the channel. Requests in this state are on the global unsent list.
 *
 * HGFS_REQ_STATE_SUBMITTED:
 *    The packet has been sent, but the reply will arrive asynchronously.
 *    The request will be on the hgfsRepPending list, and whenever
 *    the reply arrives, the reply handler will remove the request from
 *    the hgfsRepPending list and stuff the reply into the request's
 *    packet buffer.
 *
 *    This is only for asynchronous channel communication.
 *
 * HGFS_REQ_STATE_COMPLETED:
 *    The request handler sent the request and received a reply. The reply
 *    is stuffed in the request's packet buffer. Requests in this state
 *    are not on any list.
 */
typedef enum {
   HGFS_REQ_STATE_ALLOCATED,
   HGFS_REQ_STATE_UNSENT,
   HGFS_REQ_STATE_SUBMITTED,
   HGFS_REQ_STATE_COMPLETED,  /* Both header and payload were received. */
} HgfsState;

/*
 * Each page that is sent from guest to host is described in the following
 * format.
 */
typedef struct HgfsDataPacket {
   struct page *page;
   uint32 offset;
   uint32 len;
} HgfsDataPacket;

/*
 * A request to be sent to the user process.
 */
typedef struct HgfsReq {

   /* Reference count */
   struct kref kref;

   /* Links to place the object on various lists. */
   struct list_head list;

   /* ID of the transport (its address) */
   void *transportId;

   /*
    * When clients wait for the reply to a request, they'll wait on this
    * wait queue.
    */
   wait_queue_head_t queue;

   /* Current state of the request. */
   HgfsState state;

   /* ID of this request */
   uint32 id;

   /* Pointer to payload in the buffer */
   void *payload;

   /* Total size of the payload.*/
   size_t payloadSize;

   /*
    * Size of the data buffer (below), not including size of chunk
    * used by transport. Must be enough to hold both request and
    * reply (but not at the same time). Initialized in channels.
    */
   size_t bufferSize;

  /*
   * Used by read and write calls. Hgfs client passes in
   * pages to the vmci channel using datapackets and vmci channel
   * uses it to pass PA's to the host.
   */
   HgfsDataPacket *dataPacket;

   /* Number of entries in data packet */
   uint32 numEntries;

   /*
    * Packet of data, for both incoming and outgoing messages.
    * Include room for the command.
    */
   unsigned char buffer[];
} HgfsReq;

/* Public functions (with respect to the entire module). */
HgfsReq *HgfsGetNewRequest(void);
HgfsReq *HgfsCopyRequest(HgfsReq *req);
int HgfsSendRequest(HgfsReq *req);
HgfsReq *HgfsRequestGetRef(HgfsReq *req);
void HgfsRequestPutRef(HgfsReq *req);
#define HgfsFreeRequest(req)  HgfsRequestPutRef(req)
HgfsStatus HgfsReplyStatus(HgfsReq *req);
void HgfsCompleteReq(HgfsReq *req);
void HgfsFailReq(HgfsReq *req, int error);

#endif // _HGFS_DRIVER_REQUEST_H_
