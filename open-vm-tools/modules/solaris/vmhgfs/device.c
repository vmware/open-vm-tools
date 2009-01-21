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
 * device.c --
 *
 * HGFS device driver for Solaris.
 *
 * Implements a device driver that creates a device in /dev that can be
 * interacted with through standard file IO syscalls.  Implements open(),
 * close(), read(), write(), and poll().
 *
 * read() provides requests from the filesystem to the caller and is
 * synchronized with the HgfsSubmitRequest() function in the filesystem.
 * write() receives replies to those requests and provides them to the
 * filesystem by waking up processes waiting in HgfsSubmitRequest().
 *
 */

#include "hgfsSolaris.h"
#include "device.h"
#include "request.h"
#include "filesystem.h"

/*
 * Local variables
 */
static Bool hgfsAttached;        /* Flag to tell whether device is attached */

/*
 * Function Prototypes
 */
static int HgfsSendRequestToUser(HgfsReq *req, uio_t *uiop);
static INLINE int HgfsGetReplyHeaderFromUser(HgfsReplyHeader *reply, uio_t *uiop);
static int HgfsGetReplyPacketFromUser(HgfsReq *req, ssize_t offset, uio_t *uiop);

/* Local utility functions */
static INLINE HgfsSuperInfo *HgfsDevToSuperInfo(dev_t dev);
static INLINE HgfsSuperInfo *HgfsDevinfoToSuperInfo(dev_info_t *dip);
static INLINE HgfsReq *HgfsReplyToRequest(HgfsReplyHeader *reply);

/*
 * Driver "configuration" functions
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevGetinfo --
 *
 *    The entry point for getinfo(9E).  Carries out commands requested by the
 *    kernel that either return the device information structure or the
 *    instance number.
 *
 * Results:
 *    Returns DDI_SUCCESS on success and DDI_FAILURE on failure.
 *    When infocmd is DDI_INFO_DEVT2DEVINFO, a pointer to the device's
 *    information structure is returned in result.
 *    When infocmd is DDI_INFO_DEVT2INSTANCE, the device's instance number is
 *    returned in result.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevGetinfo(dev_info_t *dip,         // IN: Device info for this device
               ddi_info_cmd_t infocmd,  // IN: Command to carry out
               void *arg,               // IN: Device number (as dev_t) for this device
               void **result)           // OUT: Result of command
{
   minor_t instance;    /* getminor() returns minor_t */
   HgfsSuperInfo *sip;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevGetinfo().\n");

   ASSERT(arg);
   ASSERT(result);

   /* These indicate a kernel error */
   if ((!arg) || (!result)) {
      cmn_err(HGFS_ERROR, "NULL input from Kernel in HgfsDevGetinfo().\n");
      return EINVAL;
   }


   switch (infocmd) {

   /* This wants a dev_info_t from the dev_t: get it from the superinfo */
   case DDI_INFO_DEVT2DEVINFO:
      sip = HgfsDevToSuperInfo((dev_t)arg);
      if (!sip) {
         goto error;
      }

      *result = sip->dip;
      DEBUG(VM_DEBUG_DEVDONE, "HgfsDevGetinfo() done.\n");
      return DDI_SUCCESS;

   /*
    * This wants the instance number, which is the minor number since we set
    * it to be that in HgfsDevAttach()'s call to ddi_create_minor_node().
    */
   case DDI_INFO_DEVT2INSTANCE:
      instance = getminor((dev_t)arg);
      *result = (void *)((uintptr_t)instance);
      DEBUG(VM_DEBUG_DEVDONE, "HgfsDevGetinfo() done.\n");
      return DDI_SUCCESS;

   /* Unrecognized command */
   default:
      goto error;
   }

error:
   *result = NULL;
   DEBUG(VM_DEBUG_FAIL, "HgfsDevGetinfo() done (FAIL).\n");
   return DDI_FAILURE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevAttach --
 *
 *    The entry point for attach(9E) that is invoked when the driver is loaded
 *    into the kernel.  This allocates and initializes a super info structure
 *    and creates the device entry ('minor node') in the file system.
 *    XXX: do we need to handle suspend and resume commands also?
 *
 * Results:
 *    Returns zero on success, and an appropriate error code on failure.
 *
 * Side effects:
 *    The device entry appears in the filesystem (/devices).
 *    The hgfsAttaches flag is set.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevAttach(dev_info_t *dip,             // IN: Device info for this device
              ddi_attach_cmd_t cmd)        // IN: Command to carry out
{
   int instance; /* ddi_get_instance() returns int */
   HgfsSuperInfo *sip;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevAttach().\n");

   ASSERT(dip);

   /* This indicates a kernel error */
   if (!dip) {
      cmn_err(HGFS_ERROR, "NULL input from Kernel in HgfsDevAttach().\n");
      return EINVAL;
   }

   if (hgfsAttached) {
      cmn_err(HGFS_ERROR, "Device already attached.\n");
      return EIO;
   }

   switch (cmd) {

   /* Attach this device */
   case DDI_ATTACH:
      /* hgfsInstance is used by the filesystem side to get superinfo pointer */
      instance = hgfsInstance = ddi_get_instance(dip);

      /*
       * Here we allocate state for this instance of the driver (see comment
       * in _init() for more information on Solaris' DDI Soft State interface).
       *
       * Once we have the pointer to the state we just allocated, we'll
       * initialize its fields.  In particular:
       * - a pointer to the dev_info_t is stored because HgfsGetInfo() will
       *   need it.
       * - clear the flag indicating whether the device is open.
       * - the request list, along with its mutex and condition variable, are
       *   initialized.
       */
      if (ddi_soft_state_zalloc(superInfoHead, instance) != DDI_SUCCESS) {
         cmn_err(HGFS_ERROR,
                 "could not zalloc state for this instance (%d).\n", instance);
         goto error;
      }

      sip = (HgfsSuperInfo *)ddi_get_soft_state(superInfoHead, instance);
      if (!sip) {
         goto error_free;
      }

      sip->dip = dip;
      sip->devOpen = 0;

      HgfsInitRequestList(sip);

      /*
       * Create the minor node (the /devices entry)
       */
      if (ddi_create_minor_node(dip,            /* device info ptr */
                                HGFS_DEV_NAME,  /* name of device */
                                S_IFCHR,        /* type of device (character) */
                                instance,       /* minor number */
                                DDI_PSEUDO,     /* pseudo device b/c not backed
                                                   by physical device */
                                0               /* flag: not a clone device */
                                ) != DDI_SUCCESS) {

         cmn_err(HGFS_ERROR, "could not create minor node (/devices entry).\n");
         goto error_free;
      }

      /* Report presence of device to system log (syslog/dmesg) */
      ddi_report_dev(dip);

      /*
       * We only want one instance of this driver to ever be running, so set a
       * flag stating the device is attached
       */
      hgfsAttached = TRUE;

      DEBUG(VM_DEBUG_DEVDONE, "HgfsDevAttach() done.\n");
      return DDI_SUCCESS;

   /* Unsupported commands: DDI_PM_RESUME and DDI_RESUME  */
   default:
      goto error;
   }

error_free:
   ddi_soft_state_free(sip, instance);
error:
   DEBUG(VM_DEBUG_FAIL, "HgfsDevAttach() done (FAIL).\n");
   return DDI_FAILURE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevDetach --
 *
 *    The detach(9E) entry point that is invoked when the driver is
 *    unloaded from the kernel.  This cleans up all state allocated in
 *    HgfsDevAttach() and removes the device entry ('minor node').
 *
 * Results:
 *    Returns zero on success, and an appropriate error code on failure.
 *
 * Side effects:
 *    The super info structure for this driver is deallocated.
 *    The device entry (/devices) is no longer valid.
 *    The hgfsAttached flag is cleared.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevDetach(dev_info_t *dip,             // IN: Device info for this device
              ddi_detach_cmd_t cmd)        // IN: Command to carry out
{
   int instance;
   HgfsSuperInfo *sip;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevDetach().\n");

   ASSERT(dip);

   /* This indicates a kernel error */
   if (!dip) {
      cmn_err(HGFS_ERROR, "NULL input from Kernel in HgfsDevDetach().\n");
      return EINVAL;
   }

   switch(cmd) {

   /* Detach this device */
   case DDI_DETACH:
     /*
      * Clean up the state allocated for this instance of the driver:
      *  - Get a pointer to the super info structure,
      *  - Remove the device entry (minor node) from the system,
      *  - Free the super info structure.
      */
     instance = ddi_get_instance(dip);
     sip = (HgfsSuperInfo *)ddi_get_soft_state(superInfoHead, instance);
     if (!sip) {
        cmn_err(HGFS_ERROR, "could not find HgfsSuperInfo on detach.\n");
        goto error;
     }

     ddi_remove_minor_node(dip, NULL); /* NULL means remove all for this dev_info_t */
     ddi_soft_state_free(superInfoHead, instance);

     /* Indicate driver is no longer attached */
     hgfsAttached = 0;

     DEBUG(VM_DEBUG_DEVDONE, "HgfsDevDetach() done.\n");
     return DDI_SUCCESS;

   /* Unsupported commands: DDI_PM_SUSPEND and DDI_SUSPEND */
   default:
     goto error;
   }

error:
   DEBUG(VM_DEBUG_FAIL, "HgfsDevDetach() done (FAIL).\n");
   return DDI_FAILURE;
}



/*
 * Device functions
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevOpen --
 *    The entry point for open(9E) that is invoked when a user process calls
 *    open(2) on the device.  This checks that the specified device and open
 *    type are valid.
 *
 * Results:
 *    Returns zero on success, and an appropriate error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevOpen(dev_t *devp,   // IN: Pointer to device number
            int flag,      // IN: Indicates open mode
                           //     (FREAD, FWRITE, FEXCL, FNDELAY)
            int otyp,      // IN: Type of open (OTYP_CHR, OTYP_BLK, OTYP_LYR)
            cred_t *credp) // IN: Pointer to credentials of caller
{
   HgfsSuperInfo *sip;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevOpen().\n");

   ASSERT(devp);
   ASSERT(credp);

   /* These indicate a kernel error */
   if ((!devp) || (!credp)) {
      cmn_err(HGFS_ERROR, "NULL input from Kernel in HgfsDevOpen().\n");
      return EINVAL;
   }

   /*
    * Here we do a few checks to ensure that this open is valid.  First, our
    * device is a character device so we ensure that the type of open
    * indicates this.  Next we ensure that the caller is root.  Finally, we
    * make sure that the device number specified is valid by attempting to
    * retrieve its state that was allocated in HgfsDevDetach().
    */

   if (otyp != OTYP_CHR) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevOpen() done (FAIL).\n");
      return EINVAL;
   }

   if (!HgfsSuser(credp)) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevOpen() done (FAIL).\n");
      return EINVAL;
   }

   sip = HgfsDevToSuperInfo(*devp);
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevOpen() done (FAIL).\n");
      return ENXIO;
   }

   /* Make sure device isn't opened more than once */
   if (sip->devOpen) {
      return ENXIO;
   }

   sip->devOpen = TRUE;

   DEBUG(VM_DEBUG_DEVDONE, "HgfsDevOpen() done.\n");
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevClose --
 *    The entry point for close(9E), which is invoked when a user process
 *    calls close(2) on the device.  This ensures that the specified device
 *    number is valid (by trying to find its state structure).
 *
 * Results:
 *    Returns zero on success, and an appropriate error code on failure.
 *
 * Side effects:
 *    All pending requests are cancelled.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevClose(dev_t dev,            // IN: Device number
             int flag,             // IN: Mode opened with (see HgfsOpen() comment)
             int otyp,             // IN: Type of open (see HgfsOpen() comment)
             cred_t *credp)        // IN: Pointer to credentials of caller
{
   HgfsSuperInfo *sip;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevClose().\n");

   ASSERT(credp);

   /* This indicates a kernel error */
   if (!credp) {
      cmn_err(HGFS_ERROR, "NULL input from Kernel in HgfsDevClose().\n");
      return EINVAL;
   }

   /*
    * We only allow root to open and close the device.
    */
   if (!HgfsSuser(credp)) {
      return EINVAL;
   }


   /* Just make sure this instance is valid, state is freed in HgfsDevDetach() */
   sip = HgfsDevToSuperInfo(dev);
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevClose() done (FAIL).\n");
      return ENXIO;
   }

   /*
    * This will signify to the filesystem that the device half is no longer
    * present.
    */
   sip->devOpen = 0;

   /*
    * Each submitted request must be told an error has occurred.
    */
   HgfsCancelAllRequests(sip);


   DEBUG(VM_DEBUG_DEVDONE, "HgfsDevClose() done.\n");
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevRead --
 *
 *    Entry point for read(9E) that is invoked when a user process calls
 *    read(2) on the device.
 *
 *    This checks the pending request list for any outstanding requests.  If
 *    the list is empty, this blocks on a condition variable that is signaled
 *    each time a request is enqueued.  The request is removed from the list
 *    and copied to the user's buffer.
 *
 * Results:
 *    Returns zero on success, an appropriate error code on failure.
 *
 * Side effects:
 *    Blocks if the list is empty.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevRead(dev_t dev,          // IN: Device number
            uio_t *uiop,        // IN: Ptr to structure defining read request
            cred_t *credp)      // IN: Ptr to caller's credentials
{
   int ret;
   HgfsSuperInfo *sip;
   HgfsReq *nextReq = NULL;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevRead().\n");

   ASSERT(uiop);
   ASSERT(credp);

   /* These indicate a kernel error */
   if ((!uiop) || (!credp)) {
      cmn_err(HGFS_ERROR, "NULL input from Kernel in HgfsDevRead().\n");
      return EINVAL;
   }

   sip = HgfsDevToSuperInfo(dev);
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevRead() done (FAIL).\n");
      return ENXIO;
   }

   /*
    * We need to check the request list to see if there is anything to read.
    * If not, wait on the list's condition variable so we will be
    * cv_signal()'ed when a request is placed onto the request list (by the
    * filesystem half).  Once a request is there we can remove it from
    * the list and copy it to user space.
    */
   mutex_enter(&sip->reqMutex);

   for (;;) {
      /*
       * The cv_wait_sig(9F) man page says the cv_wait*() functions can return
       * prematurely in certain situations, typically when job control or
       * debugging is used, so it is required that we loop around cv_wait_sig().
       */
      while (HgfsListIsEmpty(&sip->reqList)) {
         DEBUG(VM_DEBUG_COMM, "HgfsDevRead: blocking ...\n");
         if (cv_wait_sig(&sip->reqCondVar, &sip->reqMutex) == 0) {
            /*
             * We received a system signal (e.g., SIGKILL) while waiting for the
             * cv_signal().  Release mutex and return appropriate error code.
             */
            mutex_exit(&sip->reqMutex);
            DEBUG(VM_DEBUG_FAIL,
                  "cv_wait_sig() interrupted by signal in HgfsDevRead().\n");
            return EINTR;
         }
      }

      /*
       * We hold the lock so the next request is guaranteed to be there.  If
       * the next request is still in the submitted state then we process it;
       * otherwise the request must be in the abandoned state and we destroy it
       * and wait for the next one.
       */
      nextReq = HgfsDequeueRequest(sip);
      ASSERT(nextReq);
      if (HgfsReqGetState(nextReq) == HGFS_REQ_SUBMITTED) {
         break;
      }

      ASSERT(HgfsReqGetState(nextReq) == HGFS_REQ_ABANDONED);
      HgfsDestroyReq(sip, nextReq);
   }

   DEBUG(VM_DEBUG_LIST, "HgfsDevRead received request for ID %d", nextReq->id);
   mutex_exit(&sip->reqMutex);

   /*
    * Filesystem ensures that requests are small enough to fit in a packet sent
    * through the backdoor.
    */
   ret = HgfsSendRequestToUser(nextReq, uiop);
   /*
    * If we couldn't copy the request to guestd, set the request's state to
    * error and wake up the client.  The filesystem will have to clean up.
    */
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevRead: could not copy request to user (FAIL).\n");
      HgfsReqSetState(nextReq, HGFS_REQ_ERROR);
      HgfsWakeWaitingClient(sip, nextReq);
   }

   DEBUG(VM_DEBUG_INFO, "resid=%ld\n", uiop->uio_resid);
   DEBUG(VM_DEBUG_DEVDONE, "HgfsDevRead() done.\n");
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevWrite --
 *
 *    Entry point for write(9E), which is invoked when a user process calls
 *    write(2) on the device.
 *
 *    This copies in a reply header, finds the associated request, then copies
 *    the reply packet into the request packet.  It handles cleaning up
 *    requests that have been abandoned.
 *
 * Results:
 *    Returns zero on success, an appropriate error code on failure.
 *
 * Side effects:
 *    The state of the request corresponding to this reply is modified.
 *    The process waiting for this reply is woken up (if it is still waiting).
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevWrite(dev_t dev,         // IN: Device number
             uio_t *uiop,       // IN: Ptr to structure defining write request
             cred_t *credp)     // IN: Ptr to caller's credentials
{
   int ret;
   uint32_t writeSize;
   HgfsSuperInfo *sip;
   HgfsReq *request;
   HgfsReplyHeader reply;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevWrite().\n");

   ASSERT(uiop);
   ASSERT(credp);

   /* These indicate a kernel error */
   if (!uiop || !credp) {
      cmn_err(HGFS_ERROR, "NULL input from kernel in HgfsDevWrite().\n");
      return EINVAL;
   }

   sip = HgfsDevToSuperInfo(dev);
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevWrite(): couldn't get superinfo (FAIL)\n");
      return ENXIO;
   }

   /* We need at least the reply structure to do anything useful. */
   if (uiop->uio_resid < sizeof reply) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevWrite(): too little data written (FAIL).\n");
      return ERANGE;
   }

   /*
    * If this fails there is a problem with guestd or the filesystem isn't
    * splitting requests properly.  This also allows writeSize to be a uint32
    * despite uiop->uio_resid being a ssize_t (which can be a long).
    */
   if (uiop->uio_resid > HGFS_PACKET_MAX) {
      return EINVAL;
   }

   /* Needed to set packet size upon successful receipt of packet. */
   writeSize = uiop->uio_resid;

   /*
    * Copy in just the reply header that will tell us what request this is
    * for and what the status of the reply is.
    */
   ret = HgfsGetReplyHeaderFromUser(&reply, uiop);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL,
            "HgfsDevWrite(): couldn't copy header from user (FAIL).\n");
      return EIO;
   }

   DEBUG(VM_DEBUG_COMM, "HgfsDevWrite: Reply for %d with status %d.\n",
         reply.id, reply.status);

   /*
    * Here we need to determine which request this reply is for, then determine
    * if that requester is still waiting.  If so we copy the contents of the
    * reply into the request's packet and wake up the client; otherwise we
    * destroy the abandoned request and return an error.
    */
   request = HgfsReplyToRequest(&reply);
   if (!request) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevWrite(): invalid id in reply (FAIL).\n");
      return EINVAL;
   }

   /*
    * Acquiring this mutex makes the checking of whether the request has been
    * ABANDONED and setting the request to COMPLETED or ERROR atomic.  It is
    * also acquired in HgfsSubmitRequest().
    */
   mutex_enter(&sip->reqMutex);

   if (HgfsReqGetState(request) == HGFS_REQ_ABANDONED) {
      /* The requesting process is gone so we don't need to wake it up */
      HgfsDestroyReq(sip, request);
      mutex_exit(&sip->reqMutex);
      DEBUG(VM_DEBUG_FAIL, "HgfsDevWrite(): request was abandoned (FAIL).\n");
      return EINTR;  /* XXX: Does guestd need to know this? */
   }

   /*
    * Now that we know which request this reply is for, zero out its packet and
    * copy in the reply header.
    */
   bzero(request->packet, sizeof request->packet);
   memcpy(request->packet, &reply, sizeof reply);

   /* Copy rest of reply into request packet after the header */
   ret = HgfsGetReplyPacketFromUser(request, sizeof reply, uiop);
   if (ret < 0) {
      /*
       * If there was an error while copying the reply packet, we set the state
       * to error and return an error code (in case guestd cares).
       */
      HgfsReqSetState(request, HGFS_REQ_ERROR);
      ret = EIO;
   } else {
      /* ... otherwise we set the packet size and set the state to completed. */
      request->packetSize = writeSize - uiop->uio_resid;
      HgfsReqSetState(request, HGFS_REQ_COMPLETED);
      ret = 0;
   }

   /*
    * Wake up the client waiting on the request (Note we don't call
    * HgfsWakeWaitingClient() since we already hold the lock.)
    */
   cv_signal(&request->condVar);
   mutex_exit(&sip->reqMutex);

   DEBUG(VM_DEBUG_DEVDONE, "HgfsDevWrite() done.\n");
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevChpoll --
 *
 *    Entry point for chpoll(9E), which is invoked (potentially more than
 *    once) when either poll(2) or select(3C) is called by a user process.
 *    This examines the state of the device and returns information indicating
 *    which operations are ready to be performed.
 *
 * Results:
 *    Returns 0 on success, or an error code if an error occurs.
 *    If any of the events polled have occurred, this is specified by filling
 *    in reventsp with the appropriate bits.
 *    If none of the events polled have occurred, reventsp is cleared.  phpp
 *    is also set to point to a pollhead structure if no other file
 *    descriptors polled have had events.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDevChpoll(dev_t dev,                // IN: Device number
              short int events,         // IN: Events that could occur
                                        //     (see man -s 9E chpoll for all
                                        //     events)
              int anyyet,               // IN: Flag (set if other fds have events pending)
              short int *reventsp,      // OUT: Ptr to bitmask of events satisfied
              struct pollhead **phpp)   // OUT: Ptr to a pollhead structure
{
   short revents;
   HgfsSuperInfo *sip;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevChPoll(). (events=%x)\n", events);

   ASSERT(reventsp);
   ASSERT(phpp);

   /* These indicate a kernel error */
   if ((!reventsp) || (!phpp)) {
      cmn_err(HGFS_ERROR, "NULL input from Kernel in HgfsDevChpoll().\n");
      return EINVAL;
   }

   sip = HgfsDevToSuperInfo(dev);
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDevChPoll() done (FAIL).\n");
      return ENXIO;
   }

   /*
    * This comment is for reference, can remove in future:
    *
    * Algorithm to implement (chpoll(9E) and p195 DDK docs):
    *
    * if (events are satisfied now) {
    *   *reventsp = (mask of satisfied events);
    * } else {
    *   *reventsp = 0;
    *   if (!anyyet) {
    *           *phpp = &(local pollhead structure);
    *   }
    * }
    * return 0;
    *
    * The pollhead structure should not be referenced by the driver.
    */

   revents = 0;
   /* Clear flag indicating the need to call pollwakeup() on write() */
   sip->pollwakeupOnWrite = FALSE;

   /*
    * Caller asked to read: if the request list is not empty then they can
    * read without blocking.
    */
   if (events & HGFS_POLL_READ) {
      mutex_enter(&sip->reqMutex);

      if ( !HgfsListIsEmpty(&sip->reqList) ) {
         /* Set just those bits the caller asked for */
         revents |= (events & HGFS_POLL_READ);
      }
      DEBUG(VM_DEBUG_CHPOLL, "HgfsChpoll(): HGFS_POLL_READ, revents=%d", revents);

      mutex_exit(&sip->reqMutex);
   }

   /*
    * Caller asked to write: they can always write.
    */
   if (events & HGFS_POLL_WRITE) {
      revents |= (events & HGFS_POLL_WRITE);
      DEBUG(VM_DEBUG_CHPOLL, "HgfsChpoll(): HGFS_POLL_WRITE, revent=%d\n", revents);
   }

   /*
    * No events have occurred
    */
   if (revents == 0) {
      DEBUG(VM_DEBUG_CHPOLL, "HgfsChpoll(): no events\n");
      *reventsp = 0;
      if (anyyet == 0) {
         /*
          * No other file descriptors have had events, so we set the pollhead
          * structure and a flag indicating the need to call pollwakeup() on
          * a successful completion of a write().
          */
         *phpp = &sip->hgfsPollhead;
         DEBUG(VM_DEBUG_CHPOLL, "HgfsChpoll(): setting pollwakeupOnWrite.\n");
         sip->pollwakeupOnWrite = TRUE;
      }
   }

   *reventsp = revents;

   DEBUG(VM_DEBUG_DEVDONE, "HgfsDevChPoll() done. (revents=%x)\n", revents);
   return 0;
}


/*
 * Functions to copy to and from user space.
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSendRequestToUser --
 *
 *    Sends a requst to the user.
 *
 * Results:
 *    Returns zero on success, and non-zero on error.
 *
 * Side effects:
 *    The uio_resid field of uiop will be decremented by the number of bytes
 *    read.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsSendRequestToUser(HgfsReq *req,     // IN: Request to send
                      uio_t *uiop)      // IN: User's read request
{
   int ret;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsSendRequestToUser().\n");

   ASSERT(req);
   ASSERT(uiop);

   /*
    * If the buffer provided by guestd is not large enough to hold the largest
    * packet, then either the filesystem or guestd is doing something wrong.
    * (uio_resid is the number of bytes we can write to the user's buffer.)
    */
   ASSERT(req->packetSize <= HGFS_PACKET_MAX);
   ASSERT(uiop->uio_resid >= req->packetSize);

   DEBUG(VM_DEBUG_INFO, "HgfsSendRequestToUser: uiomove(%p, %d, UIO_READ, %p)\n",
           req->packet, req->packetSize, uiop);

   /*
    * uiomove(9F) will handle copying of data from kernel buffer to user buffer
    * for us.  It understands the format of the uio_t that describes the
    * user's read request and verifies user address is valid.  It returns zero
    * on success and an error code otherwise.
    */
   ret = uiomove(req->packet,       // Kernel src address
                 req->packetSize,   // Number of bytes to transfer
                 UIO_READ,          // rwflag: READ
                 uiop);             // Pointer to user's read request

   DEBUG(VM_DEBUG_DEVDONE,
         "HgfsSendRequestToUser() sent %d bytes in request (ret=%d).\n",
         req->packetSize, ret);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetReplyHeaderFromUser --
 *
 *    Copies the reply header from the user into the provided reply
 *
 * Results:
 *    Returns zero on success, non-zero on error.
 *
 * Side effects:
 *    uiop's uio_resid is decremented by the number of bytes that were copied.
 *
 *----------------------------------------------------------------------------
 */

static INLINE int
HgfsGetReplyHeaderFromUser(HgfsReplyHeader *header,  // OUT: Header to copy into
                           uio_t *uiop)              // IN: User's write request
{
   ASSERT(header);
   ASSERT(uiop);

   DEBUG(VM_DEBUG_COMM,
         "HgfsGetReplyHeaderFromUser(): writing %"FMTSZ"u bytes"
         " into request packet's header.\n", sizeof *header);

   /* Only copy in the header, uio_resid decremented for us by uiomove() */
   return uiomove(header, sizeof *header, UIO_WRITE, uiop);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetReplyPacketFromUser --
 *
 *    Copies the rest of the user's reply into the offset of the request's
 *    packet.
 *
 * Results:
 *    Returns zero on success, non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsGetReplyPacketFromUser(HgfsReq *req,        // IN/OUT: Request to copy into
                           ssize_t offset,      // IN: Offset in packet to
                                                //     start copying at
                           uio_t *uiop)         // IN: User's write request
{
   ASSERT(req);
   ASSERT(uiop);


   /*
    * If guestd has more to write than the max room left in the packet then it
    * is doing something wrong.
    */
   ASSERT(uiop->uio_resid <= HGFS_PACKET_MAX - offset);

   DEBUG(VM_DEBUG_COMM,
         "HgfsGetReplyPacketFromUser(): writing %ld bytes into request packet.\n",
         uiop->uio_resid);

   /* Write the reply into the specified buffer */
   return uiomove(req->packet + offset, uiop->uio_resid, UIO_WRITE, uiop);
}



/*
 * Utility functions
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDevToSuperInfo --
 *
 *    Gets a pointer to the super info structure from a device number (dev_t).
 *
 * Results:
 *    Returns a pointer to the super info struct, or NULL if it cannot be
 *    found.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE HgfsSuperInfo *
HgfsDevToSuperInfo(dev_t dev)         // IN: Device number
{
   minor_t instance;

   DEBUG(VM_DEBUG_DEVENTRY, "HgfsDevToSuperInfo().\n");

   instance = getminor(dev);

   DEBUG(VM_DEBUG_RARE,
         "HgfsDevToSuperInfo: getting ptr to instance %d's state.\n", instance);

   return (HgfsSuperInfo *)ddi_get_soft_state(superInfoHead, (int)instance);
}


/*
 *----------------------------------------------------------------------------
 *
 *  HgfsDevinfoToSuperInfo --
 *
 *    Gets a pointer to the super info from a device information struct.
 *    XXX: currently unused since HgfsDevDetach needs value of instance
 *    itself.
 *
 * Results:
 *    Returns a pointer to the super info structure, or NULL if it cannot be
 *    found.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE HgfsSuperInfo *
HgfsDevinfoToSuperInfo(dev_info_t *dip) // IN: Device information structure
{
   int instance;

   ASSERT(dip);

   instance = ddi_get_instance(dip);

   DEBUG(VM_DEBUG_RARE,
         "HgfsDevinfoToSuperInfo: getting ptr to instance %d's state.\n", instance);

   return (HgfsSuperInfo *)ddi_get_soft_state(superInfoHead, instance);
}



/*
 *----------------------------------------------------------------------------
 *
 * HgfsReplyToRequest --
 *
 *    Determines the request that corresponds with the provided reply.
 *
 * Results:
 *    Returns a pointer to the request, or NULL if it cannot be located.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE HgfsReq *
HgfsReplyToRequest(HgfsReplyHeader *reply)      // IN: Reply to find request of
{
   ASSERT(reply);

   if (reply->id > HGFS_MAX_OUTSTANDING_REQS) {
      return NULL;
   }

   /*
    * When this function is called, the request should only be in the
    * SUBMITTED, ABANDONED, or ERROR states.
    */
   ASSERT((HgfsReqGetState(&requestPool[reply->id]) == HGFS_REQ_SUBMITTED) ||
          (HgfsReqGetState(&requestPool[reply->id]) == HGFS_REQ_ABANDONED) ||
          (HgfsReqGetState(&requestPool[reply->id]) == HGFS_REQ_ERROR));

   DEBUG(VM_DEBUG_INFO, "HgfsRequestToReply: reply for request id %d\n", reply->id);

   return &requestPool[reply->id];
}
