/*********************************************************
 * Copyright (C) 2004-2016, 2021 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/


/*
 * request.c --
 *
 * Implementation of routines used to initialize, allocate, and move requests
 * between lists.
 *
 */

/*
 * Includes
 */
#include "hgfsSolaris.h"
#include "request.h"


/*
 *----------------------------------------------------------------------------
 *
 *  HgfsInitRequestList --
 *
 *   Initializes the request list related members of the HgfsSuperInfo for
 *   this instance of the driver state.
 *
 * Results:
 *   The pending request list, free request list, and associated
 *   synchronization primitives are initialized.
 *
 * Side effects:
 *   Each request is now in the free list and is set to UNUSED.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsInitRequestList(HgfsSuperInfo *sip) // IN: Pointer to superinfo structure
{
   int i;

   DEBUG(VM_DEBUG_REQUEST, "HgfsInitRequestList().\n");

   ASSERT(sip);

   mutex_init(&sip->reqMutex, NULL, MUTEX_DRIVER, NULL);

   /* Initialize free request list */
   DblLnkLst_Init(&sip->reqFreeList);
   mutex_init(&sip->reqFreeMutex, NULL, MUTEX_DRIVER, NULL);
   cv_init(&sip->reqFreeCondVar, NULL, CV_DRIVER, NULL);

   /*
    * Initialize pool of requests
    *
    * Here we are setting each request's id to its index into the requestPool
    * so this can be used as an identifier in reply packets.  Each request's
    * state is also set to UNUSED and is added to the free list.
    */
   for (i = 0; i < ARRAYSIZE(requestPool); i++) {
      requestPool[i].id = i;
      requestPool[i].state = HGFS_REQ_UNUSED;

      DblLnkLst_Init(&requestPool[i].listNode);
      DblLnkLst_LinkLast(&sip->reqFreeList, &requestPool[i].listNode);
   }

   //HgfsDebugPrintReqList(&sip->reqFreeList);
   DEBUG(VM_DEBUG_REQUEST, "HgfsInitRequestList() done.\n");
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCancelAllRequests  --
 *
 *    Cancels all pending (SUBMITTED) requests by signalling the transport
 *    code that they should be forcibly ended.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Threads waiting on requests are woken up with error conditions.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsCancelAllRequests(HgfsSuperInfo *sip)       // IN: Superinfo containing
                                                //     request list
{
   int i;

   DEBUG(VM_DEBUG_REQUEST, "HgfsCancelAllRequests().\n");

   ASSERT(sip);
   ASSERT(mutex_owned(&sip->reqMutex));

   for (i = 0; i < ARRAYSIZE(requestPool); i++) {
      /*
       * Signal that all submitted requests need to be cancelled.
       * We expect that transport implementation wakes up processes
       * waiting on requests
       */
      if (requestPool[i].state == HGFS_REQ_SUBMITTED)
         sip->cancelRequest(&requestPool[i]);
   }

   DEBUG(VM_DEBUG_REQUEST, "HgfsCancelAllRequests() done.\n");
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsListIsEmpty --
 *
 *    Determines whether the provided list is empty.
 *
 *    Note: this assumes it is called with the list lock held because often
 *    callers will need this function to be atomic with other operations.
 *
 * Results:
 *    Returns zero if list is not empty, a positive integer if it is empty.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

INLINE Bool
HgfsListIsEmpty(DblLnkLst_Links *listAnchor)    // IN: Anchor of list to check
{
   ASSERT(listAnchor);

   return (listAnchor == listAnchor->next);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetNewReq --
 *
 *    Allocates and initializes a new request structure from the request pool.
 *    This function blocks until a request is available or it has been
 *    interrupted by a signal.
 *
 * Results:
 *    Returns pointer to allocated HgfsReq on success, and NULL if interrupted
 *    while waiting to be allocated a request structure.
 *
 * Side effects:
 *    Request's state is set to HGFS_REQ_ALLOCATED.
 *    Request is removed from the free list.
 *
 *----------------------------------------------------------------------------
 */

HgfsReq *
HgfsGetNewReq(HgfsSuperInfo *sip)       // IN: Superinfo containing free list
{
   HgfsReq *newReq;

   DEBUG(VM_DEBUG_REQUEST, "HgfsGetNewReq().\n");

   ASSERT(sip);

   /*
    * Here we atomically get the next free request from the free list and set
    * that request's state to ALLOCATED.
    */
   mutex_enter(&sip->reqFreeMutex);

   /* Wait for a request structure if there aren't any free */
   while (HgfsListIsEmpty(&sip->reqFreeList)) {
      /*
       * If the list is empty, we wait on the condition variable which is
       * unconditionally signaled whenever a request is destroyed.
       */
      if (cv_wait_sig(&sip->reqFreeCondVar, &sip->reqFreeMutex) == 0) {
         /*
          * We were interrupted while waiting for a request, so we must return
          * NULL and release the mutex.
          */
         newReq = NULL;
         goto out;
      }
   }

   newReq = HGFS_FREE_REQ_LIST_HEAD(sip);

   HgfsDebugPrintReq("HgfsGetNewReq", newReq);

   /* Failure of these indicates error in program's logic */
   ASSERT(newReq && newReq->state == HGFS_REQ_UNUSED);

   /* Take request off the free list and indicate it has been ALLOCATED */
   DblLnkLst_Unlink1(&newReq->listNode);
   newReq->state = HGFS_REQ_ALLOCATED;

   /* Clear packet of request before allocating to clients. */
   bzero(newReq->packet, sizeof newReq->packet);

   DEBUG(VM_DEBUG_LIST, "Dequeued from free list: %s", newReq->packet);
   HgfsDebugPrintReqList(&sip->reqFreeList);

out:
   mutex_exit(&sip->reqFreeMutex);

   DEBUG(VM_DEBUG_REQUEST, "HgfsGetNewReq() done.\n");
   return newReq;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDestroyReq --
 *
 *    Deallocates a request structure.
 *
 * Results:
 *    Returns void.
 *
 * Side effects:
 *    Request's state is set to HGFS_REQ_UNUSED.
 *    Request is placed on the free list.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDestroyReq(HgfsSuperInfo *sip,      // IN: Superinfo containing free list
               HgfsReq *oldReq)         // IN: Request to destroy
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsDestroyReq().\n");

   /* XXX This should go away later, just for testing */
   if (oldReq->state != HGFS_REQ_COMPLETED) {
      DEBUG(VM_DEBUG_ALWAYS, "HgfsDestroyReq() (oldReq state=%d).\n",
            oldReq->state);
   }

   ASSERT(sip);
   ASSERT(oldReq);
   /* Failure of this check indicates an error in program logic */
   ASSERT(oldReq->state == HGFS_REQ_COMPLETED ||
          oldReq->state == HGFS_REQ_ABANDONED ||
          oldReq->state == HGFS_REQ_ERROR);

   /*
    * To make the request available for other clients we change its state to
    * UNUSED and place it back on the free list.
    */
   mutex_enter(&sip->reqFreeMutex);

   oldReq->state = HGFS_REQ_UNUSED;
   DblLnkLst_LinkLast(&sip->reqFreeList, &oldReq->listNode);
   /* Wake up clients waiting for a request structure */
   cv_signal(&sip->reqFreeCondVar);

   mutex_exit(&sip->reqFreeMutex);

   HgfsDebugPrintReqList(&sip->reqFreeList);

   DEBUG(VM_DEBUG_REQUEST, "HgfsDestroyReq() done.\n");
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSendRequest --
 *
 *    Sends request for execution. The exact details depend on transport used
 *    to communicate with the host.
 *
 *    Note: this assumes it is called with the list lock held because often
 *    callers will need this function to be atomic with other operations.
 *
 * Results:
 *    Returns void.
 *
 * Side effects:
 *    Request's state is set to HGFS_REQ_SUBMITTED.
 *    Request is added to the pending request list.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSendRequest(HgfsSuperInfo *sip,    // IN: Superinfo sructure with methods
                HgfsReq *req)          // IN/OUT: Request to be sent
{
   int ret;

   ASSERT(sip);
   ASSERT(req);
   /* Failure of this check indicates error in program logic */
   ASSERT(req->state == HGFS_REQ_ALLOCATED);

   req->state = HGFS_REQ_SUBMITTED;
   ret = sip->sendRequest(req);
   if (ret) {
      req->state = HGFS_REQ_ERROR;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsWakeWaitingClient --
 *
 *    Wakes up the client waiting on the specified request.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsWakeWaitingClient(HgfsSuperInfo *sip,       // IN: Superinfo with request mutex
                      HgfsReq *req)             // IN: Request to wake client for
{
//   DEBUG(VM_DEBUG_REQUEST, "HgfsWakeWaitingClient().\n");

   ASSERT(sip);
   ASSERT(req);

   /*
    * We need to acquire the mutex before signaling the request's condition
    * variable since it was acquired before sleeping in HgfsSubmitRequest().
    */
   mutex_enter(&sip->reqMutex);

   cv_signal(&req->condVar);

   mutex_exit(&sip->reqMutex);

//   DEBUG(VM_DEBUG_REQUEST, "HgfsWakeWaitingClient() done.\n");
}

