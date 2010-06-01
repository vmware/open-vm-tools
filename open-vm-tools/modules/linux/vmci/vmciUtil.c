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
 * vmciUtil.c
 *
 * Small utility function for allocating kernel memory and copying data.
 *
 */

#ifdef __linux__
#  include "driver-config.h"

#  define EXPORT_SYMTAB

#  include <linux/module.h>
#  include <linux/module.h>
#  include "compat_kernel.h"
#  include "compat_slab.h"
#  include "compat_wait.h"
#  include "compat_interrupt.h"
#elif defined(_WIN32)
#  ifndef WINNT_DDK
#     error  This file only works with the NT ddk 
#  endif // WINNT_DDK
#  include <ntddk.h>
#elif defined(SOLARIS)
#  include <sys/ddi.h>
#  include <sys/sunddi.h>
#  include <sys/disp.h>
#elif defined(__APPLE__)
#  include <IOKit/IOLib.h>
#else 
#error "platform not supported."
#endif //linux

#define LGPFX "VMCIUtil: "

#include "vmware.h"
#include "vm_atomic.h"
#include "vmci_defs.h"
#include "vmci_kernel_if.h"
#include "vmciGuestKernelIf.h"
#include "vmciInt.h"
#include "vmciProcess.h"
#include "vmciDatagram.h"
#include "vmciUtil.h"
#include "vmciEvent.h"

static void VMCIUtilCidUpdate(VMCIId subID, VMCI_EventData *eventData,
                              void *clientData);

static VMCIId ctxUpdateSubID = VMCI_INVALID_ID;
static Atomic_uint32 vmContextID = { VMCI_INVALID_ID };


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
   if (VMCIEvent_Subscribe(VMCI_EVENT_CTX_ID_UPDATE, VMCIUtilCidUpdate, NULL,
                           &ctxUpdateSubID) < VMCI_SUCCESS) {
      VMCI_LOG(("VMCIUtil: Failed to subscribe to event %d.\n", 
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
   if (VMCIEvent_Unsubscribe(ctxUpdateSubID) < VMCI_SUCCESS) {
      VMCI_LOG(("VMCIUtil: Failed to unsubscribe to event %d with subscriber "
                "id %d.\n", VMCI_EVENT_CTX_ID_UPDATE, ctxUpdateSubID));
   }
}


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
      VMCI_LOG(("VMCIUtil: Invalid subscriber id. %d.\n", subID));
      return;
   }
   if (eventData == NULL || evPayload->contextID == VMCI_INVALID_ID) {
      VMCI_LOG(("VMCIUtil: Invalid event data.\n"));
      return;
   }
   VMCI_LOG(("VMCIUtil: Updating context id from 0x%x to 0x%x on event %d.\n",
             Atomic_Read(&vmContextID),
             evPayload->contextID,
             eventData->event));
   Atomic_Write(&vmContextID, evPayload->contextID);
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

Bool
VMCIUtil_CheckHostCapabilities(void)
{
   int result;
   VMCIResourcesQueryMsg *msg;
   uint32 msgSize = sizeof(VMCIResourcesQueryHdr) + 
      VMCI_UTIL_NUM_RESOURCES * sizeof(VMCI_Resource);
   VMCIDatagram *checkMsg = VMCI_AllocKernelMem(msgSize, VMCI_MEMORY_NONPAGED);

   if (checkMsg == NULL) {
      VMCI_LOG((LGPFX"Check host: Insufficient memory.\n"));
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
 * VMCI_GetContextID --
 *
 *      Returns the context id. 
 *
 * Results:
 *      Context id.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCI_GetContextID);
#endif

VMCIId
VMCI_GetContextID(void)
{
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
   result &= VMCIProcess_CheckHostCapabilities();
   result &= VMCIDatagram_CheckHostCapabilities();
   result &= VMCIUtil_CheckHostCapabilities();

   VMCI_LOG((LGPFX"Host capability check: %s\n", result ? "PASSED" : "FAILED"));

   return result;
}

/*
 *----------------------------------------------------------------------
 *
 * VMCI_Version --
 *
 *     Returns the version of the VMCI guest driver.
 *
 * Results: 
 *      Returns a version number.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCI_Version);
#endif

uint32
VMCI_Version()
{
   return VMCI_VERSION_NUMBER;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_InInterrupt --
 *
 *     Determines if we are running in tasklet/dispatch level or above.
 *
 * Results: 
 *      TRUE if tasklet/dispatch or above, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCI_InInterrupt()
{
#if defined(_WIN32)
   return KeGetCurrentIrql() >= DISPATCH_LEVEL;
#elif defined(__linux__)
   return in_interrupt();
#elif defined(SOLARIS)
   return servicing_interrupt();   /* servicing_interrupt is not part of DDI. */
#elif defined(__APPLE__)
   /*
    * All interrupt servicing is handled by IOKit functions, by the time the IOService
    * interrupt handler is called we're no longer in an interrupt dispatch level.
    */
   return false;
#endif //
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_DeviceGet --
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

#ifdef __linux__
EXPORT_SYMBOL(VMCI_DeviceGet);
#endif

Bool
VMCI_DeviceGet(void)
{
   return VMCI_DeviceEnabled();
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_DeviceRelease --
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

#ifdef __linux__
EXPORT_SYMBOL(VMCI_DeviceRelease);
#endif

void
VMCI_DeviceRelease(void)
{
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
            result = VMCIDatagram_Dispatch(dg->src.context, dg);
         }
         if (result < VMCI_SUCCESS) {
            VMCI_LOG(("Datagram with resource %d failed with err %x.\n",
                      dg->dst.resource, result));
         }
         
         /* On to the next datagram. */
         dg = (VMCIDatagram *)((uint8 *)dg + dgInSize);
      } else {
         size_t bytesToSkip;
         
         /*
          * Datagram doesn't fit in datagram buffer of maximal size. We drop it.
          */

         VMCI_LOG(("Failed to receive datagram of size %u.\n",
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
 *----------------------------------------------------------------------
 *
 * VMCIContext_GetPrivFlags --
 *
 *      Provided for compatibility with the host VMCI API.
 *
 * Results:
 *      Always returns VMCI_NO_PRIVILEGE_FLAGS.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCIContext_GetPrivFlags);
#endif

VMCIPrivilegeFlags
VMCIContext_GetPrivFlags(VMCIId contextID) // IN
{
   return VMCI_NO_PRIVILEGE_FLAGS;
}
