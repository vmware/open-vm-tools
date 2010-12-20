/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * requestInt.h --
 *
 *	Internal declarations for the HgfsRequest module.  Filesystem code
 *	should not include this file.
 */

#ifndef _requestInt_H_
#define _requestInt_H_

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#if defined __FreeBSD__
#  include <sys/libkern.h>       // common string, memcpy, etc userland routines
#  include <vm/uma.h>            // for the UMA (slab) allocator
#elif defined __APPLE__
#  include <string.h>
#endif


#include "vm_assert.h"

#include "os.h"
#include "request.h"
#include "debug.h"

#if defined __APPLE__
 #include "hgfsTransport.h"
 #define HGFS_REQUEST_PREFIX_LENGTH MAX(HGFS_CLIENT_CMD_LEN, sizeof (HgfsVmciTransportStatus))
#else
 #define HGFS_REQUEST_PREFIX_LENGTH HGFS_CLIENT_CMD_LEN
#endif


/*
 * Data types
 */
struct HgfsTransportChannel;


/*
 * In-kernel representation of an Hgfs request.  These objects are kept on zero,
 * one, or two lists at any time.
 *
 * (Ideal) Lifecycle of an Hgfs request:
 *   - File system calls HgfsKReq_AllocateRequest to allocate a request.  The
 *     new request's reference count is initialized to one, and it is placed
 *     in the filesystem's requests container.
 *   - File system calls HgfsKReq_SubmitRequest to submit the request for
 *     processing via the backdoor.  At this point, request is inserted on a
 *     global work item list and its reference count is bumped.
 *   - Worker thread removes request from the work item list.  Reference count is
 *     unchanged as the reference is simply transferred from the work item list to
 *     the worker thread itself.
 *   - When the worker thread receives a reply, it updates the request's state,
 *     copies in the reply data, and decrements the reference count.
 *
 * At any point, the file system may abort a request with
 * HgfsKReq_ReleaseRequest.  Doing so will involve decrementing the object's
 * reference count, since the file system is giving up its reference.  Whoever
 * reduces the reference count to zero is responsible for freeing it.
 *
 * Special case -- Forced unmount of a file system:
 *
 * If the user forcibly unmounts the file system, the following work is done.
 *   - For each request object associated with a file system
 *     - If the item is on the work item list, it is removed from that list.  The
 *       canceling thread is then responsible for decrementing the object's
 *       reference count.
 *     - The request's state is set to HGFS_REQ_ERROR, and a wakeup signal is
 *       sent to the stateCv.  (If the file system had not yet submitted the
 *       request, it will immediately return as a failure at submission time.)
 *     - Without anything left to do with this request, the cancellation thread
 *       drops the reference count, and if it reaches zero, frees the object.
 */
typedef struct HgfsKReqObject {
   DblLnkLst_Links fsNode;      // Link between object and its parent file system.
   DblLnkLst_Links pendingNode; // Link between object and pending request list.
   DblLnkLst_Links sentNode;    // Link between object and sent request list.

   unsigned int refcnt;         // Object reference count
   HgfsKReqState state;         // Indicates state of request
   OS_MUTEX_T *stateLock;       // Protects state: ...
   OS_CV_T  stateCv;            // Condition variable to wait for and signal
                                // presence of reply. Used with the stateLock
                                // above.

   uint32_t id;                 // The unique identifier of this request.
                                // Typically just incremented sequentially
                                // from zero.
   size_t payloadSize;          // Total size of payload
   void *ioBuf;                 // Pointer to memory descriptor.
                                // Used for MacOS over VMCI.

   /* On which channel was the request allocated/sent ?. */
   struct HgfsTransportChannel *channel;
   /*
    * The file system is concerned only with the payload portion of an Hgfs
    * request packet, but the RPC message opens with the command string "f ".
    *
    * Strangely, the HgfsBd_Dispatch routine takes a pointer to the payload, but indexes
    * -backwards- from that pointer to get to the RPC command. (This was actually done
    * because we wanted to vary the command - async vs. sync - on the fly without
    * performing another allocation. So the buffer is sized for any command plus the
    * packet, and the command is varied by the transport layer.) So, anyway, effectively
    * all of __rpc_packet will be sent across the backdoor, but the file system will only
    * muck with _payload.
    *
    * VMCI:
    * Mac OS X is capable of using VMCI in which case _command will have
    * HgfsVmciTransportStatus.
    *
    */
   struct {
      char      _command[HGFS_REQUEST_PREFIX_LENGTH];
      char      _payload[HGFS_PACKET_MAX];      // Contains both the request and
                                                // its reply.
   } __rpc_packet;
} HgfsKReqObject;

#define command __rpc_packet._command
#define payload __rpc_packet._payload

/*
 * Opaque container for a file system's request objects.  File system operates
 * only on a typedef'd handle.  (See request.h.)
 */
typedef struct HgfsKReqContainer {
   OS_MUTEX_T *listLock;
   DblLnkLst_Links list;
} HgfsKReqContainer;

/*
 * Current state & instruction for the HgfsKReq worker thread.
 */
typedef struct HgfsKReqWState {
   Bool running;        // Is worker running?
   Bool exit;           // Set this to TRUE at module unload time.
} HgfsKReqWState;


/*
 * Module internal variables
 */

/* Workitem list anchor */
extern DblLnkLst_Links hgfsKReqWorkItemList;

/* Workitem list lock. */
extern OS_MUTEX_T *hgfsKReqWorkItemLock;

extern OS_CV_T hgfsKReqWorkItemCv;

/* UMA zone (slab) for allocating HgfsKReqs. */
extern OS_ZONE_T *hgfsKReqZone;

/* Process structure for the worker thread */
extern OS_THREAD_T hgfsKReqWorkerThread;
extern HgfsKReqWState hgfsKReqWorkerState;


/*
 * Function prototypes
 */

extern void HgfsKReqWorker(void *arg);


#endif // ifndef _requestInt_H_
