/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
   /*
    * We ASSERT() this because this function must be called before the device is
    * opened to ensure the request lists are ready to be used by the
    * filesystem.  (The filesystem won't mount unless the device is open.)
    * By ASSERT()ing this condition we also obviate the need to acquire the
    * mutex when adding requests to reqFreeList in the for loop below, since we
    * are guaranteed (in practice) to have exclusive access.
    */
   ASSERT(!sip->devOpen);

   /* None of these fail */

   /* Initialize pending request list */
   DblLnkLst_Init(&sip->reqList);
   mutex_init(&sip->reqMutex, NULL, MUTEX_DRIVER, NULL);
   cv_init(&sip->reqCondVar, NULL, CV_DRIVER, NULL);

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
      mutex_init(&requestPool[i].stateLock, NULL, MUTEX_DRIVER, NULL);

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
 *    Cancels all pending requests by removing them from the request list and
 *    waking up the clients waiting for replies.  Also cancels requests that
 *    have already been sent to guestd (and are not on the pending list) and
 *    are either abandoned or still have clients waiting for them.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The pending request list is empty and all requests are in the free list.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsCancelAllRequests(HgfsSuperInfo *sip)       // IN: Superinfo containing
                                                //     request list
{
   DblLnkLst_Links *currNode, *nextNode;
   HgfsReq *currRequest;
   int i;

   DEBUG(VM_DEBUG_REQUEST, "HgfsCancelAllRequests().\n");

   ASSERT(sip);

   /* Traverse list to make sure we cancel all pending requests on the list. */
   mutex_enter(&sip->reqMutex);

   DEBUG(VM_DEBUG_REQUEST, "HgfsCancelAllRequests(): traversing pending request list.\n");

   for (currNode = HGFS_REQ_LIST_HEAD_NODE(sip);
        currNode != &sip->reqList;
        currNode = nextNode) {

      /* Get the next element while we still have a pointer to it. */
      nextNode = currNode->next;

      DblLnkLst_Unlink1(currNode);
      currRequest = DblLnkLst_Container(currNode, HgfsReq, listNode);

      if (HgfsReqGetState(currRequest) == HGFS_REQ_ABANDONED) {
         /*
          * If the client is no longer waiting, clean up for it by destroying
          * the request.
          */
         HgfsDestroyReq(sip, currRequest);
      } else {
         /*
          * ... otherwise indicate an error and wakeup the client.  Note that
          * we can't call HgfsWakeWaitingClient() because we already hold the
          * reqMutex lock.
          *
          * Also, make sure that all non-ABANDONED requests are SUBMITTED.
          */
         ASSERT(HgfsReqGetState(currRequest) == HGFS_REQ_SUBMITTED);

         HgfsReqSetState(currRequest, HGFS_REQ_ERROR);
         cv_signal(&currRequest->condVar);
      }
   }

   DEBUG(VM_DEBUG_REQUEST, "HgfsCancelAllRequests(): traversing request pool.\n");

   /*
    * Now look for abandoned and submitted requests that are in the pool but
    * not on the pending list (because they were already sent to guestd).
    */
   for (i = 0; i < ARRAYSIZE(requestPool); i++) {
      /*
       * As above, abandoned requests are cleaned up and we wake up the clients
       * for submitted requests.
       */
      if (HgfsReqGetState(&requestPool[i]) == HGFS_REQ_ABANDONED) {
         HgfsDestroyReq(sip, &requestPool[i]);
      } else if (HgfsReqGetState(&requestPool[i]) == HGFS_REQ_SUBMITTED) {
         HgfsReqSetState(&requestPool[i], HGFS_REQ_ERROR);
         cv_signal(&requestPool[i].condVar);
      }
   }

   mutex_exit(&sip->reqMutex);

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
   ASSERT(newReq && (HgfsReqGetState(newReq) == HGFS_REQ_UNUSED));

   /* Take request off the free list and indicate it has been ALLOCATED */
   DblLnkLst_Unlink1(&newReq->listNode);
   HgfsReqSetState(newReq, HGFS_REQ_ALLOCATED);

   /* Clear packet of request before allocating to clients. */
   bzero(newReq->packet, sizeof newReq->packet);

out:
   mutex_exit(&sip->reqFreeMutex);

   DEBUG(VM_DEBUG_LIST, "Dequeued from free list: %s", newReq->packet);
   HgfsDebugPrintReqList(&sip->reqFreeList);
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

INLINE void
HgfsDestroyReq(HgfsSuperInfo *sip,      // IN: Superinfo containing free list
               HgfsReq *oldReq)         // IN: Request to destroy
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsDestroyReq().\n");

   /* XXX This should go away later, just for testing */
   if (HgfsReqGetState(oldReq) != HGFS_REQ_COMPLETED) {
      DEBUG(VM_DEBUG_ALWAYS, "HgfsDestroyReq() (oldReq state=%d).\n",
           HgfsReqGetState(oldReq));
   }

   ASSERT(sip);
   ASSERT(oldReq);
   /* Failure of this check indicates an error in program logic */
   ASSERT((HgfsReqGetState(oldReq) == HGFS_REQ_COMPLETED) ||
          (HgfsReqGetState(oldReq) == HGFS_REQ_ABANDONED) ||
          (HgfsReqGetState(oldReq) == HGFS_REQ_ERROR));

   /*
    * To make the request available for other clients we change its state to
    * UNUSED and place it back on the free list.
    */
   mutex_enter(&sip->reqFreeMutex);

   HgfsReqSetState(oldReq, HGFS_REQ_UNUSED);
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
 * HgfsEnqueueRequest --
 *
 *    Adds the provided request to the end of the request list.
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

INLINE void
HgfsEnqueueRequest(HgfsSuperInfo *sip,  // IN: Superinfo containing request list
                   HgfsReq *newReq)     // IN: Request to enqueue
{
   DEBUG(VM_DEBUG_REQUEST, "HgfsEnqueueRequest().\n");

   ASSERT(sip);
   ASSERT(newReq);
   /* Failure of this check indicates error in program logic */
   ASSERT(HgfsReqGetState(newReq) == HGFS_REQ_ALLOCATED);

   /*
    * This simply changes the state and places the request on the pending
    * request list.  Signaling the (potentially sleeping) device is handled by
    * the caller (HgfsSubmitRequest()).
    */
   HgfsReqSetState(newReq, HGFS_REQ_SUBMITTED);
   DblLnkLst_LinkLast(&sip->reqList, &newReq->listNode);

   DEBUG(VM_DEBUG_LIST, "Enqueued on pending list: %s", newReq->packet);
   HgfsDebugPrintReqList(&sip->reqList);
   DEBUG(VM_DEBUG_REQUEST, "HgfsEnqueueRequest() done.\n");
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDequeueRequest --
 *
 *    Removes the next request from the list.
 *
 *    Note: this assumes it is called with the list lock held because often
 *    callers will need this function to be atomic with other operations.
 *
 * Results:
 *    Returns a pointer to the request removed from the list, or NULL if the
 *    list is empty.
 *
 * Side effects:
 *    Request is removed from the pending request list.
 *
 *
 *----------------------------------------------------------------------------
 */

INLINE HgfsReq *
HgfsDequeueRequest(HgfsSuperInfo *sip)  // IN: Superinfo containing request list
{
   HgfsReq *nextReq;

   DEBUG(VM_DEBUG_REQUEST, "HgfsDequeueRequest().\n");

   ASSERT(sip);

   if (HgfsListIsEmpty(&sip->reqList)) {
      return NULL;
   }

   nextReq = HGFS_REQ_LIST_HEAD(sip);

   /*
    * nextReq should never be NULL since we are called with the lock held and
    * we just checked if the list was empty.  The request's state should only
    * be SUBMITTED or ABANDONED.  (Errors occurring while copying the request
    * to guestd happen after removal from this list.)
    */
   ASSERT(nextReq);
   ASSERT(HgfsReqGetState(nextReq) == HGFS_REQ_SUBMITTED ||
          HgfsReqGetState(nextReq) == HGFS_REQ_ABANDONED);

   DblLnkLst_Unlink1(&nextReq->listNode);

   DEBUG(VM_DEBUG_LIST, "Dequeued from pending list: %s", nextReq->packet);
   HgfsDebugPrintReqList(&sip->reqList);
   DEBUG(VM_DEBUG_REQUEST, "HgfsDequeueRequest() done.\n");

   return nextReq;
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


/*
 * These two state functions will help prevent forgetting to lock.
 */


/*
 *----------------------------------------------------------------------------
 * HgfsSetState --
 *
 *    Sets the state of the request.
 *
 * Results:
 *    Returns void.
 *
 * Side effects:
 *    The state of the request is modified.
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsReqSetState(HgfsReq *req,           // IN: Request to alter state of
                HgfsReqState state)     // IN: State to set request to
{
//   DEBUG(VM_DEBUG_REQUEST, "HgfsReqSetState().\n");

   ASSERT(req);

   mutex_enter(&req->stateLock);

   req->state = state;

   mutex_exit(&req->stateLock);

//   DEBUG(VM_DEBUG_REQUEST, "HgfsReqSetState() done.\n");
}



/*
 *----------------------------------------------------------------------------
 *
 * HgfsReqGetState --
 *
 *    Retrieves state of provided request.
 *
 * Results:
 *    Returns state of request.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

INLINE HgfsReqState
HgfsReqGetState(HgfsReq *req)   // IN: Request to retrieve state of
{
   HgfsReqState state;

   DEBUG(VM_DEBUG_REQUEST, "HgfsReqGetState().\n");

   ASSERT(req);

   mutex_enter(&req->stateLock);

   state = req->state;

   mutex_exit(&req->stateLock);

   DEBUG(VM_DEBUG_REQUEST, "HgfsReqGetState() done.\n");
   return state;
}
