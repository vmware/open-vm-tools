/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

#include <linux/list.h>
#include "compat_sched.h"
#include "compat_spinlock.h"
#include "compat_wait.h"

#include "hgfs.h" /* For common HGFS definitions. */
#include "vm_basic_types.h"

/* Macros for accessing the payload portion of the HGFS request packet. */
#define HGFS_REQ_PAYLOAD(hgfsReq) ((hgfsReq)->packet + HGFS_CLIENT_CMD_LEN)

/*
 * HGFS_REQ_STATE_ALLOCATED:
 *    The filesystem half has allocated the request from the slab
 *    allocator. The request is not on any list.
 *
 * HGFS_REQ_STATE_UNSENT:
 *    The filesystem half of the driver has filled in the request fields
 *    and placed the request in the global unsent list. It is now the
 *    backdoor handler's responsibility to submit this request to
 *    the backdoor. Requests in this state are on the global unsent list.
 *
 * HGFS_REQ_STATE_SENT:
 *    The backdoor handler has sent the packet, but the reply will arrive
 *    asynchronously. The request is now on the sent list, and whenever
 *    the reply arrives, the backdoor handler will remove the request from
 *    the sent list and stuff the reply into the request's packet buffer.
 *    
 * HGFS_REQ_STATE_ERROR:
 *    The backdoor handler encountered an error while sending the request
 *    or getting the reply. The filesystem half of the driver should
 *    free the request. Requests in this state are not on any list.
 *
 * HGFS_REQ_STATE_COMPLETED:
 *    The backdoor handler sent the request and received a reply. The reply
 *    is stuffed in the request's packet buffer. Requests in this state
 *    are not on any list.
 */
typedef enum {
   HGFS_REQ_STATE_ALLOCATED,
   HGFS_REQ_STATE_UNSENT,
   HGFS_REQ_STATE_SENT,
   HGFS_REQ_STATE_ERROR,
   HGFS_REQ_STATE_COMPLETED,
} HgfsState;

/*
 * A request to be sent to the user process.
 */
typedef struct HgfsReq {
 
   /* Links to place the object on various lists. */
   struct list_head list;
 
   /* 
    * When clients wait for the reply to a request, they'll wait on this
    * wait queue.
    */
   wait_queue_head_t queue;

   /* Current state of the request. */
   HgfsState state;

   /* ID of this request */
   uint32 id;

   /* Total size of the payload.*/
   size_t payloadSize;

   /* 
    * Packet of data, for both incoming and outgoing messages.
    * Include room for the command.
    */
   char packet[HGFS_PACKET_MAX + HGFS_CLIENT_CMD_LEN];
} HgfsReq;

/* Public functions (with respect to the entire module). */
HgfsReq *HgfsGetNewRequest(void);
int HgfsSendRequest(HgfsReq *req);
void HgfsFreeRequest(HgfsReq *req);

#endif // _HGFS_DRIVER_REQUEST_H_
