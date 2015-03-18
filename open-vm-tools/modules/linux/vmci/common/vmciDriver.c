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
 * vmciDriver.c --
 *
 *     VMCI initialization and ioctl handling.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciContext.h"
#include "vmciDatagram.h"
#include "vmciDoorbell.h"
#include "vmciDriver.h"
#include "vmciEvent.h"
#include "vmciHashtable.h"
#include "vmciKernelAPI.h"
#include "vmciQueuePair.h"
#include "vmciResource.h"
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#endif

#define LGPFX "VMCI: "

static VMCIId ctxUpdateSubID = VMCI_INVALID_ID;
static VMCIContext *hostContext;
static Atomic_uint32 vmContextID = { VMCI_INVALID_ID };


/*
 *----------------------------------------------------------------------
 *
 * VMCI_HostInit --
 *
 *      Initializes the host driver specific components of VMCI.
 *
 * Results:
 *      VMCI_SUCCESS if successful, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCI_HostInit(void)
{
   int result;

   /*
    * In theory, it is unsafe to pass an eventHnd of -1 to platforms which use
    * it (VMKernel/Windows/Mac OS at the time of this writing). In practice we
    * are fine though, because the event is never used in the case of the host
    * context.
    */
   result = VMCIContext_InitContext(VMCI_HOST_CONTEXT_ID,
                                    VMCI_DEFAULT_PROC_PRIVILEGE_FLAGS,
                                    -1, VMCI_VERSION, NULL, &hostContext);
   if (result < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to initialize VMCIContext (result=%d).\n",
                    result));
      goto errorExit;
   }

   result = VMCIQPBroker_Init();
   if (result < VMCI_SUCCESS) {
      goto hostContextExit;
   }

   VMCI_DEBUG_LOG(0, (LGPFX"host components initialized.\n"));
   return VMCI_SUCCESS;

hostContextExit:
   VMCIContext_ReleaseContext(hostContext);
errorExit:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_HostCleanup --
 *
 *      Cleans up the host specific components of the VMCI module.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCI_HostCleanup(void)
{
   VMCIContext_ReleaseContext(hostContext);
   VMCIQPBroker_Exit();
}


#if defined(__APPLE__) || defined(VMKERNEL)
/* Windows has its own implementation of this, and Linux doesn't need one. */
/*
 *----------------------------------------------------------------------
 *
 * vmci_device_get --
 *
 *      Verifies that a valid VMCI device is present, and indicates
 *      the callers intention to use the device until it calls
 *      VMCI_DeviceRelease().
 *
 * Results:
 *      TRUE if a valid VMCI device is present, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_device_get)
Bool
vmci_device_get(uint32 *apiVersion,                      // IN/OUT
                VMCI_DeviceShutdownFn *deviceShutdownCB, // UNUSED
                void *userData,                          // UNUSED
                void **deviceRegistration)               // OUT
{
   if (NULL != deviceRegistration) {
      *deviceRegistration = NULL;
   }

   if (*apiVersion > VMCI_KERNEL_API_VERSION) {
      *apiVersion = VMCI_KERNEL_API_VERSION;
      return FALSE;
   }

   if (!VMCI_DeviceEnabled()) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * vmci_device_release --
 *
 *      Indicates that the caller is done using the VMCI device.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_device_release)
void
vmci_device_release(void *deviceRegistration) // UNUSED
{
}
#endif // __APPLE__ || VMKERNEL


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUtilCidUpdate --
 *
 *      Gets called with the new context id if updated or resumed.
 *
 * Results:
 *      Context id.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIUtilCidUpdate(VMCIId subID,               // IN:
                  VMCI_EventData *eventData,  // IN:
                  void *clientData)           // IN:
{
   VMCIEventPayload_Context *evPayload = VMCIEventDataPayload(eventData);

   if (subID != ctxUpdateSubID) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid subscriber (ID=0x%x).\n", subID));
      return;
   }
   if (eventData == NULL || evPayload->contextID == VMCI_INVALID_ID) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid event data.\n"));
      return;
   }
   VMCI_LOG((LGPFX"Updating context from (ID=0x%x) to (ID=0x%x) on event "
             "(type=%d).\n", Atomic_Read(&vmContextID), evPayload->contextID,
             eventData->event));
   Atomic_Write(&vmContextID, evPayload->contextID);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUtil_Init --
 *
 *      Subscribe to context id update event.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIUtil_Init(void)
{
   /*
    * We subscribe to the VMCI_EVENT_CTX_ID_UPDATE here so we can update the
    * internal context id when needed.
    */
   if (vmci_event_subscribe(VMCI_EVENT_CTX_ID_UPDATE,
#if !defined(linux) || defined(VMKERNEL)
                            VMCI_FLAG_EVENT_NONE,
#endif // !linux || VMKERNEL
                            VMCIUtilCidUpdate, NULL,
                            &ctxUpdateSubID) < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to subscribe to event (type=%d).\n",
                    VMCI_EVENT_CTX_ID_UPDATE));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUtil_Exit --
 *
 *      Cleanup
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIUtil_Exit(void)
{
   if (vmci_event_unsubscribe(ctxUpdateSubID) < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to unsubscribe to event (type=%d) with "
                    "subscriber (ID=0x%x).\n", VMCI_EVENT_CTX_ID_UPDATE,
                    ctxUpdateSubID));
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUtil_CheckHostCapabilities --
 *
 *      Verify that the host supports the hypercalls we need. If it does not,
 *      try to find fallback hypercalls and use those instead.
 *
 * Results:
 *      TRUE if required hypercalls (or fallback hypercalls) are
 *      supported by the host, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#define VMCI_UTIL_NUM_RESOURCES 1

static Bool
VMCIUtilCheckHostCapabilities(void)
{
   int result;
   VMCIResourcesQueryMsg *msg;
   uint32 msgSize = sizeof(VMCIResourcesQueryHdr) +
      VMCI_UTIL_NUM_RESOURCES * sizeof(VMCI_Resource);
   VMCIDatagram *checkMsg = VMCI_AllocKernelMem(msgSize, VMCI_MEMORY_NONPAGED);

   if (checkMsg == NULL) {
      VMCI_WARNING((LGPFX"Check host: Insufficient memory.\n"));
      return FALSE;
   }

   checkMsg->dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                    VMCI_RESOURCES_QUERY);
   checkMsg->src = VMCI_ANON_SRC_HANDLE;
   checkMsg->payloadSize = msgSize - VMCI_DG_HEADERSIZE;
   msg = (VMCIResourcesQueryMsg *)VMCI_DG_PAYLOAD(checkMsg);

   msg->numResources = VMCI_UTIL_NUM_RESOURCES;
   msg->resources[0] = VMCI_GET_CONTEXT_ID;

   result = VMCI_SendDatagram(checkMsg);
   VMCI_FreeKernelMem(checkMsg, msgSize);

   /* We need the vector. There are no fallbacks. */
   return (result == 0x1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_CheckHostCapabilities --
 *
 *      Tell host which guestcalls we support and let each API check
 *      that the host supports the hypercalls it needs. If a hypercall
 *      is not supported, the API can check for a fallback hypercall,
 *      or fail the check.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      Fallback mechanisms may be enabled in the API and vmmon.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCI_CheckHostCapabilities(void)
{
   Bool result = VMCIEvent_CheckHostCapabilities();
   result &= VMCIDatagram_CheckHostCapabilities();
   result &= VMCIUtilCheckHostCapabilities();

   if (!result) {
      /* If it failed, then make sure this goes to the system event log. */
      VMCI_WARNING((LGPFX"Host capability checked failed.\n"));
   } else {
      VMCI_DEBUG_LOG(0, (LGPFX"Host capability check passed.\n"));
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_ReadDatagramsFromPort --
 *
 *      Reads datagrams from the data in port and dispatches them. We
 *      always start reading datagrams into only the first page of the
 *      datagram buffer. If the datagrams don't fit into one page, we
 *      use the maximum datagram buffer size for the remainder of the
 *      invocation. This is a simple heuristic for not penalizing
 *      small datagrams.
 *
 *      This function assumes that it has exclusive access to the data
 *      in port for the duration of the call.
 *
 * Results:
 *      No result.
 *
 * Side effects:
 *      Datagram handlers may be invoked.
 *
 *----------------------------------------------------------------------
 */

void
VMCI_ReadDatagramsFromPort(VMCIIoHandle ioHandle,  // IN
                           VMCIIoPort dgInPort,    // IN
                           uint8 *dgInBuffer,      // IN
                           size_t dgInBufferSize)  // IN
{
   VMCIDatagram *dg;
   size_t currentDgInBufferSize = PAGE_SIZE;
   size_t remainingBytes;

   ASSERT(dgInBufferSize >= PAGE_SIZE);

   VMCI_ReadPortBytes(ioHandle, dgInPort, dgInBuffer, currentDgInBufferSize);
   dg = (VMCIDatagram *)dgInBuffer;
   remainingBytes = currentDgInBufferSize;

   while (dg->dst.resource != VMCI_INVALID_ID || remainingBytes > PAGE_SIZE) {
      unsigned dgInSize;

      /*
       * When the input buffer spans multiple pages, a datagram can
       * start on any page boundary in the buffer.
       */

      if (dg->dst.resource == VMCI_INVALID_ID) {
         ASSERT(remainingBytes > PAGE_SIZE);
         dg = (VMCIDatagram *)ROUNDUP((uintptr_t)dg + 1, PAGE_SIZE);
         ASSERT((uint8 *)dg < dgInBuffer + currentDgInBufferSize);
         remainingBytes = (size_t)(dgInBuffer + currentDgInBufferSize - (uint8 *)dg);
         continue;
      }

      dgInSize = VMCI_DG_SIZE_ALIGNED(dg);

      if (dgInSize <= dgInBufferSize) {
         int result;

         /*
          * If the remaining bytes in the datagram buffer doesn't
          * contain the complete datagram, we first make sure we have
          * enough room for it and then we read the reminder of the
          * datagram and possibly any following datagrams.
          */

         if (dgInSize > remainingBytes) {

            if (remainingBytes != currentDgInBufferSize) {

               /*
                * We move the partial datagram to the front and read
                * the reminder of the datagram and possibly following
                * calls into the following bytes.
                */

               memmove(dgInBuffer, dgInBuffer + currentDgInBufferSize - remainingBytes,
                       remainingBytes);

               dg = (VMCIDatagram *)dgInBuffer;
            }

            if (currentDgInBufferSize != dgInBufferSize) {
               currentDgInBufferSize = dgInBufferSize;
            }

            VMCI_ReadPortBytes(ioHandle, dgInPort, dgInBuffer + remainingBytes,
                               currentDgInBufferSize - remainingBytes);
         }

         /* We special case event datagrams from the hypervisor. */
         if (dg->src.context == VMCI_HYPERVISOR_CONTEXT_ID &&
             dg->dst.resource == VMCI_EVENT_HANDLER) {
            result = VMCIEvent_Dispatch(dg);
         } else {
            result = VMCIDatagram_InvokeGuestHandler(dg);
         }
         if (result < VMCI_SUCCESS) {
            VMCI_DEBUG_LOG(4, (LGPFX"Datagram with resource (ID=0x%x) failed "
                               "(err=%d).\n", dg->dst.resource, result));
         }

         /* On to the next datagram. */
         dg = (VMCIDatagram *)((uint8 *)dg + dgInSize);
      } else {
         size_t bytesToSkip;

         /*
          * Datagram doesn't fit in datagram buffer of maximal size. We drop it.
          */

         VMCI_DEBUG_LOG(4, (LGPFX"Failed to receive datagram (size=%u bytes).\n",
                            dgInSize));

         bytesToSkip = dgInSize - remainingBytes;
         if (currentDgInBufferSize != dgInBufferSize) {
            currentDgInBufferSize = dgInBufferSize;
         }
         for (;;) {
            VMCI_ReadPortBytes(ioHandle, dgInPort, dgInBuffer, currentDgInBufferSize);
            if (bytesToSkip <= currentDgInBufferSize) {
               break;
            }
            bytesToSkip -= currentDgInBufferSize;
         }
         dg = (VMCIDatagram *)(dgInBuffer + bytesToSkip);
      }

      remainingBytes = (size_t) (dgInBuffer + currentDgInBufferSize - (uint8 *)dg);

      if (remainingBytes < VMCI_DG_HEADERSIZE) {
         /* Get the next batch of datagrams. */

         VMCI_ReadPortBytes(ioHandle, dgInPort, dgInBuffer, currentDgInBufferSize);
         dg = (VMCIDatagram *)dgInBuffer;
         remainingBytes = currentDgInBufferSize;
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * vmci_get_context_id --
 *
 *    Returns the current context ID.  Note that since this is accessed only
 *    from code running in the host, this always returns the host context ID.
 *
 * Results:
 *    Context ID.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_get_context_id)
VMCIId
vmci_get_context_id(void)
{
   if (VMCI_GuestPersonalityActive()) {
      if (Atomic_Read(&vmContextID) == VMCI_INVALID_ID) {
         uint32 result;
         VMCIDatagram getCidMsg;
         getCidMsg.dst =  VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                           VMCI_GET_CONTEXT_ID);
         getCidMsg.src = VMCI_ANON_SRC_HANDLE;
         getCidMsg.payloadSize = 0;
         result = VMCI_SendDatagram(&getCidMsg);
         Atomic_Write(&vmContextID, result);
      }
      return Atomic_Read(&vmContextID);
   } else if (VMCI_HostPersonalityActive()) {
      return VMCI_HOST_CONTEXT_ID;
   }
   return VMCI_INVALID_ID;
}


/*
 *----------------------------------------------------------------------
 *
 * vmci_version --
 *
 *     Returns the version of the VMCI driver.
 *
 * Results:
 *      Returns a version number.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_version)
uint32
vmci_version(void)
{
   return VMCI_VERSION;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_SharedInit --
 *
 *      Initializes VMCI components shared between guest and host
 *      driver. This registers core hypercalls.
 *
 * Results:
 *      VMCI_SUCCESS if successful, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCI_SharedInit(void)
{
   int result;

   result = VMCIResource_Init();
   if (result < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to initialize VMCIResource (result=%d).\n",
                    result));
      goto errorExit;
   }

   result = VMCIContext_Init();
   if (result < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to initialize VMCIContext (result=%d).\n",
                    result));
      goto resourceExit;
   }

   result = VMCIDatagram_Init();
   if (result < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to initialize VMCIDatagram (result=%d).\n",
                    result));
      goto contextExit;
   }

   result = VMCIEvent_Init();
   if (result < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to initialize VMCIEvent (result=%d).\n",
                    result));
      goto datagramExit;
   }

   result = VMCIDoorbell_Init();
   if (result < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to initialize VMCIDoorbell (result=%d).\n",
                    result));
      goto eventExit;
   }

   VMCI_DEBUG_LOG(0, (LGPFX"shared components initialized.\n"));
   return VMCI_SUCCESS;

eventExit:
   VMCIEvent_Exit();
datagramExit:
   VMCIDatagram_Exit();
contextExit:
   VMCIContext_Exit();
resourceExit:
   VMCIResource_Exit();
errorExit:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_SharedCleanup --
 *
 *      Cleans up VMCI components shared between guest and host
 *      driver.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCI_SharedCleanup(void)
{
   VMCIDoorbell_Exit();
   VMCIEvent_Exit();
   VMCIDatagram_Exit();
   VMCIContext_Exit();
   VMCIResource_Exit();
}
