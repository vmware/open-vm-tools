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
 * vmciGuestDs.c
 *
 * Implements the client-access API to the VMCI discovery service in
 * the guest kernel.
 *
 */

#ifdef __linux__
#  include "driver-config.h"

#  define EXPORT_SYMTAB

#  include <linux/module.h>
#  include "compat_kernel.h"
#  include "compat_pci.h"
#elif defined(_WIN32)
#  include <ntddk.h>
#elif defined(SOLARIS)
#  include <sys/ddi.h>
#  include <sys/sunddi.h>
#else
#  error "Platform not support by VMCI datagram API."
#endif // linux

#include "vm_basic_types.h"
#include "vm_atomic.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_kernel_if.h"
#include "vmci_infrastructure.h"
#include "vmciInt.h"
#include "vmciUtil.h"
#include "vmciDatagram.h"

static Atomic_uint32 MsgIdCounter = { 0 };

typedef struct VMCIDsRecvData {
   VMCIHost context;
   VMCILock lock;
   int status;
   uint8 buffer[VMCI_DS_MAX_MSG_SIZE];
} VMCIDsRecvData;

static int VMCIDsDoCall(int action, const char *name, VMCIHandle handle,
                        VMCIHandle *handleOut);
static int VMCIDsRecvCB(void *clientData, struct VMCIDatagram *msg);

/*
 *-------------------------------------------------------------------------
 *
 *  VMCIDs_Lookup --
 *
 *       Look up a handle in the VMCI discovery service based on 
 *       the given name.
 *
 *  Results:
 *       Error code. 0 if success.
 *     
 *  Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCIDs_Lookup);
#endif

int 
VMCIDs_Lookup(const char *name,	  // IN
              VMCIHandle *out)    // 
{
   return VMCIDsDoCall(VMCI_DS_ACTION_LOOKUP, name, VMCI_INVALID_HANDLE, out);
}


/*
 *-------------------------------------------------------------------------
 *
 *  VMCIDsDoCall --
 *
 *       Serialize a call into the CDS wire-format, send it across
 *       the VMCI device, wait for a response, and return
 *       the results.
 *
 *  Results:
 *       Error code. 0 if success.
 *     
 *  Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */


static int
VMCIDsDoCall(int action,            // IN
             const char *name,      // IN
             VMCIHandle handle,	    // IN: For the "register" action
             VMCIHandle *handleOut) // OUT: For the "lookup" action
{
   int8 *sendBuffer = NULL;
   const size_t sendBufferSize = VMCI_DS_MAX_MSG_SIZE + sizeof(VMCIDatagram);
   int nameLen, requestSize, res;
   uint32 savedMsgIdCounter;
   VMCIDsReplyHeader *reply;
   VMCIHandle dsHandle = VMCI_INVALID_HANDLE;
   VMCIDsRecvData *recvData = NULL;
   VMCIDatagram *dgram;
   VMCIDsRequestHeader *request;
   VMCILockFlags flags;
   
   nameLen = strlen(name);
   if (nameLen + sizeof *request > sendBufferSize) {
      res = VMCI_ERROR_INVALID_ARGS;
      goto out;
   }

   sendBuffer = VMCI_AllocKernelMem(sendBufferSize, VMCI_MEMORY_NONPAGED);
   if (sendBuffer == NULL) {
      res = VMCI_ERROR_NO_MEM;
      goto out;
   }

   recvData = VMCI_AllocKernelMem(sizeof *recvData, VMCI_MEMORY_NONPAGED);
   if (recvData == NULL) {
      res = VMCI_ERROR_NO_MEM;
      goto out;
   }

   VMCIHost_InitContext(&recvData->context, (uintptr_t) recvData);
   VMCI_InitLock(&recvData->lock, "VMCIDsRecvHandler", VMCI_LOCK_RANK_MIDDLE_BH);

   savedMsgIdCounter = Atomic_FetchAndInc(&MsgIdCounter);

   dgram = (VMCIDatagram *) sendBuffer;
   request = (VMCIDsRequestHeader *) (sendBuffer + sizeof *dgram);

   /* Serialize request. */
   request->action = action;
   request->msgid = savedMsgIdCounter;  
   request->handle = handle;
   request->nameLen = nameLen;
   memcpy(request->name, name, nameLen + 1);
   
   requestSize = sizeof *request + nameLen;

   if (VMCIDatagram_CreateHnd(VMCI_INVALID_ID, 0, VMCIDsRecvCB, 
                              recvData, &dsHandle) != VMCI_SUCCESS) {
      res = VMCI_ERROR_NO_HANDLE;
      goto out;
   }
   
   dgram->dst = VMCI_DS_HANDLE;
   dgram->src = dsHandle;
   dgram->payloadSize = requestSize;
   
   /* Send the datagram to CDS. */
   res = VMCIDatagram_Send(dgram);
   if (res <=  0) {
      goto out;
   }

   /* Block here waiting for the reply */
   VMCI_GrabLock_BH(&recvData->lock, &flags);
   VMCIHost_WaitForCallLocked(&recvData->context, &recvData->lock, &flags, TRUE);
   VMCI_ReleaseLock_BH(&recvData->lock, flags);
   
   if (recvData->status != VMCI_SUCCESS) {
      res = recvData->status;
      goto out;
   }

   reply = (VMCIDsReplyHeader *) recvData->buffer;
   /* Check that the msgid matches what we expect. */
   if (reply->msgid != savedMsgIdCounter) {
      res = VMCI_ERROR_GENERIC;
      goto out;
   }
   
   if (handleOut != NULL) {
      *handleOut = reply->handle;
   }
   
   res = reply->code;

out:
   if (!VMCI_HANDLE_EQUAL(dsHandle, VMCI_INVALID_HANDLE)) {
      VMCIDatagram_DestroyHnd(dsHandle);
   }
   if (recvData) {
      VMCI_CleanupLock(&recvData->lock);
      VMCIHost_ReleaseContext(&recvData->context);
      VMCI_FreeKernelMem(recvData, sizeof *recvData);
   }
   if (sendBuffer) {
      VMCI_FreeKernelMem(sendBuffer, sendBufferSize);
   }
   return res;
}

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDsRecvCB --
 *
 *      Receive callback for the Discovery Service query datagram
 *      handle.
 *
 * Results:
 *      If the received payload is not larger than the MAX, it is
 *      copied into clientData.
 *
 * Side effects:
 *      Signals the thread waiting for the reply.
 *
 *-----------------------------------------------------------------------------
 */


static int 
VMCIDsRecvCB(void *clientData,          // IN: client data for handler
             struct VMCIDatagram *msg)  // IN
{
   VMCIDsRecvData *recvData = clientData;
   VMCILockFlags flags;

   ASSERT(msg->payloadSize <= VMCI_DS_MAX_MSG_SIZE);
   if (msg->payloadSize <= VMCI_DS_MAX_MSG_SIZE) {
      memcpy(recvData->buffer, VMCI_DG_PAYLOAD(msg), (size_t)msg->payloadSize);
      recvData->status = VMCI_SUCCESS;
   } else {
      recvData->status = VMCI_ERROR_PAYLOAD_TOO_LARGE;
   }

   VMCI_GrabLock_BH(&recvData->lock, &flags);
   VMCIHost_SignalCall(&recvData->context);
   VMCI_ReleaseLock_BH(&recvData->lock, flags);
   return 0;
}
