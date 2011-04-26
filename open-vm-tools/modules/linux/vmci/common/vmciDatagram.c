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
 * vmciDatagram.c --
 *
 *    This file implements the VMCI Simple Datagram API on the host.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciContext.h"
#include "vmciDatagram.h"
#include "vmciDriver.h"
#include "vmciEvent.h"
#include "vmciHashtable.h"
#include "vmciKernelAPI.h"
#include "vmciResource.h"
#include "vmciRoute.h"
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#endif

#define LGPFX "VMCIDatagram: "


/*
 * DatagramEntry describes the datagram entity. It is used for datagram
 * entities created only on the host.
 */
typedef struct DatagramEntry {
   VMCIResource        resource;
   uint32              flags;
   Bool                runDelayed;
   VMCIDatagramRecvCB  recvCB;
   void                *clientData;
   VMCIEvent           destroyEvent;
   VMCIPrivilegeFlags  privFlags;
} DatagramEntry;

/* Mapping between wellknown resource and context. */
typedef struct DatagramWKMapping {
   VMCIHashEntry entry;
   VMCIId        contextID;
} DatagramWKMapping;

typedef struct VMCIDelayedDatagramInfo {
   Bool inDGHostQueue;
   DatagramEntry *entry;
   VMCIDatagram msg;
} VMCIDelayedDatagramInfo;


/* Wellknown mapping hashtable. */
static VMCIHashTable *wellKnownTable = NULL;

static Atomic_uint32 delayedDGHostQueueSize;

static int VMCIDatagramGetPrivFlagsInt(VMCIId contextID, VMCIHandle handle,
                                       VMCIPrivilegeFlags *privFlags);
static void DatagramFreeCB(void *resource);
static int DatagramReleaseCB(void *clientData);

static DatagramWKMapping *DatagramGetWellKnownMap(VMCIId wellKnownID);
static void DatagramReleaseWellKnownMap(DatagramWKMapping *wkMap);


/*------------------------------ Helper functions ----------------------------*/

/*
 *------------------------------------------------------------------------------
 *
 *  DatagramFreeCB --
 *     Callback to free datagram structure when resource is no longer used,
 *     ie. the reference count reached 0.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static void
DatagramFreeCB(void *clientData)
{
   DatagramEntry *entry = (DatagramEntry *)clientData;
   ASSERT(entry);
   VMCI_SignalEvent(&entry->destroyEvent);

   /*
    * The entry is freed in VMCIDatagram_DestroyHnd, who is waiting for the
    * above signal.
    */
}


/*
 *------------------------------------------------------------------------------
 *
 *  DatagramReleaseCB --
 *
 *     Callback to release the resource reference. It is called by the
 *     VMCI_WaitOnEvent function before it blocks.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static int
DatagramReleaseCB(void *clientData)
{
   DatagramEntry *entry = (DatagramEntry *)clientData;
   ASSERT(entry);
   VMCIResource_Release(&entry->resource);
   return 0;
}


/*
 *------------------------------------------------------------------------------
 *
 * DatagramCreateHnd --
 *
 *      Internal function to create a datagram entry given a handle.
 *
 * Results:
 *      VMCI_SUCCESS if created, negative errno value otherwise.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

static int
DatagramCreateHnd(VMCIId resourceID,            // IN:
                  uint32 flags,                 // IN:
                  VMCIPrivilegeFlags privFlags, // IN:
                  VMCIDatagramRecvCB recvCB,    // IN:
                  void *clientData,             // IN:
                  VMCIHandle *outHandle)        // OUT:

{
   int result;
   VMCIId contextID;
   VMCIHandle handle;
   DatagramEntry *entry;

   ASSERT(recvCB != NULL);
   ASSERT(outHandle != NULL);
   ASSERT(!(privFlags & ~VMCI_PRIVILEGE_ALL_FLAGS));

   if ((flags & VMCI_FLAG_WELLKNOWN_DG_HND) != 0) {
      return VMCI_ERROR_INVALID_ARGS;
   } else {
      if ((flags & VMCI_FLAG_ANYCID_DG_HND) != 0) {
         contextID = VMCI_INVALID_ID;
      } else {
         contextID = VMCI_GetContextID();
         if (contextID == VMCI_INVALID_ID) {
            return VMCI_ERROR_NO_RESOURCES;
         }
      }

      if (resourceID == VMCI_INVALID_ID) {
         resourceID = VMCIResource_GetID(contextID);
         if (resourceID == VMCI_INVALID_ID) {
            return VMCI_ERROR_NO_HANDLE;
         }
      }

      handle = VMCI_MAKE_HANDLE(contextID, resourceID);
   }

   entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_NONPAGED);
   if (entry == NULL) {
      VMCI_WARNING((LGPFX"Failed allocating memory for datagram entry.\n"));
      return VMCI_ERROR_NO_MEM;
   }

   if (!VMCI_CanScheduleDelayedWork()) {
      if (flags & VMCI_FLAG_DG_DELAYED_CB) {
         VMCI_FreeKernelMem(entry, sizeof *entry);
         return VMCI_ERROR_INVALID_ARGS;
      }
      entry->runDelayed = FALSE;
   } else {
      entry->runDelayed = (flags & VMCI_FLAG_DG_DELAYED_CB) ? TRUE : FALSE;
   }

   entry->flags = flags;
   entry->recvCB = recvCB;
   entry->clientData = clientData;
   VMCI_CreateEvent(&entry->destroyEvent);
   entry->privFlags = privFlags;

   /* Make datagram resource live. */
   result = VMCIResource_Add(&entry->resource, VMCI_RESOURCE_TYPE_DATAGRAM,
                             handle, DatagramFreeCB, entry);
   if (result != VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to add new resource (handle=0x%x:0x%x).\n",
                    handle.context, handle.resource));
      VMCI_DestroyEvent(&entry->destroyEvent);
      VMCI_FreeKernelMem(entry, sizeof *entry);
      return result;
   }
   *outHandle = handle;

   return VMCI_SUCCESS;
}


/*------------------------------ Init functions ----------------------------*/

/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagram_Init --
 *
 *     Initialize Datagram API, ie. register the API functions with their
 *     corresponding vectors.
 *
 *  Result:
 *     None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDatagram_Init(void)
{
   /* Create hash table for wellknown mappings. */
   wellKnownTable = VMCIHashTable_Create(32);
   if (wellKnownTable == NULL) {
      return VMCI_ERROR_NO_RESOURCES;
   }

   Atomic_Write(&delayedDGHostQueueSize, 0);
   return VMCI_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagram_Exit --
 *
 *     Cleanup Datagram API.
 *
 *  Result:
 *     None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIDatagram_Exit(void)
{
   if (wellKnownTable != NULL) {
      VMCIHashTable_Destroy(wellKnownTable);
      wellKnownTable = NULL;
   }
}


/*------------------------------ Public API functions ------------------------*/

/*
 *------------------------------------------------------------------------------
 *
 * VMCIDatagram_CreateHnd --
 *
 *      Creates a host context datagram endpoint and returns a handle to it.
 *
 * Results:
 *      VMCI_SUCCESS if created, negative errno value otherwise.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(VMCIDatagram_CreateHnd)
int
VMCIDatagram_CreateHnd(VMCIId resourceID,          // IN: Optional, generated
                                                   //     if VMCI_INVALID_ID
                       uint32 flags,               // IN:
                       VMCIDatagramRecvCB recvCB,  // IN:
                       void *clientData,           // IN:
                       VMCIHandle *outHandle)      // OUT: newly created handle
{
   if (outHandle == NULL) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (recvCB == NULL) {
      VMCI_DEBUG_LOG(4,
                     (LGPFX"Client callback needed when creating datagram.\n"));
      return VMCI_ERROR_INVALID_ARGS;
   }

   return DatagramCreateHnd(resourceID, flags, VMCI_DEFAULT_PROC_PRIVILEGE_FLAGS,
                            recvCB, clientData, outHandle);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDatagram_CreateHndPriv --
 *
 *      Creates a host context datagram endpoint and returns a handle to it.
 *
 * Results:
 *      VMCI_SUCCESS if created, negative errno value otherwise.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(VMCIDatagram_CreateHndPriv)
int
VMCIDatagram_CreateHndPriv(VMCIId resourceID,           // IN: Optional, generated
                                                        //     if VMCI_INVALID_ID
                           uint32 flags,                // IN:
                           VMCIPrivilegeFlags privFlags,// IN:
                           VMCIDatagramRecvCB recvCB,   // IN:
                           void *clientData,            // IN:
                           VMCIHandle *outHandle)       // OUT: newly created handle
{
   if (outHandle == NULL) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (recvCB == NULL) {
      VMCI_DEBUG_LOG(4,
                     (LGPFX"Client callback needed when creating datagram.\n"));
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (privFlags & ~VMCI_PRIVILEGE_ALL_FLAGS) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   return DatagramCreateHnd(resourceID, flags, privFlags, recvCB, clientData,
                            outHandle);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDatagram_DestroyHnd --
 *
 *      Destroys a handle.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(VMCIDatagram_DestroyHnd)
int
VMCIDatagram_DestroyHnd(VMCIHandle handle)       // IN
{
   DatagramEntry *entry;
   VMCIResource *resource = VMCIResource_Get(handle,
                                             VMCI_RESOURCE_TYPE_DATAGRAM);
   if (resource == NULL) {
      VMCI_DEBUG_LOG(4, (LGPFX"Failed to destroy datagram (handle=0x%x:0x%x).\n",
                         handle.context, handle.resource));
      return VMCI_ERROR_NOT_FOUND;
   }
   entry = RESOURCE_CONTAINER(resource, DatagramEntry, resource);

   VMCIResource_Remove(handle, VMCI_RESOURCE_TYPE_DATAGRAM);

   /*
    * We now wait on the destroyEvent and release the reference we got
    * above.
    */
   VMCI_WaitOnEvent(&entry->destroyEvent, DatagramReleaseCB, entry);

   if ((entry->flags & VMCI_FLAG_WELLKNOWN_DG_HND) != 0) {
      VMCIDatagramRemoveWellKnownMap(handle.resource, VMCI_HOST_CONTEXT_ID);
   }

   /*
    * We know that we are now the only reference to the above entry so
     * can safely free it.
     */
   VMCI_DestroyEvent(&entry->destroyEvent);
   VMCI_FreeKernelMem(entry, sizeof *entry);

   return VMCI_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagramGetPrivFlagsInt --
 *
 *     Internal utilility function with the same purpose as
 *     VMCIDatagram_GetPrivFlags that also takes a contextID.
 *
 *  Result:
 *     VMCI_SUCCESS on success, VMCI_ERROR_INVALID_ARGS if handle is invalid.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIDatagramGetPrivFlagsInt(VMCIId contextID,              // IN
                            VMCIHandle handle,             // IN
                            VMCIPrivilegeFlags *privFlags) // OUT
{
   ASSERT(privFlags);
   ASSERT(contextID != VMCI_INVALID_ID);

   if (contextID == VMCI_HOST_CONTEXT_ID) {
      DatagramEntry *srcEntry;
      VMCIResource *resource;

      resource = VMCIResource_Get(handle, VMCI_RESOURCE_TYPE_DATAGRAM);
      if (resource == NULL) {
         return VMCI_ERROR_INVALID_ARGS;
      }
      srcEntry = RESOURCE_CONTAINER(resource, DatagramEntry, resource);
      *privFlags = srcEntry->privFlags;
      VMCIResource_Release(resource);
   } else if (contextID == VMCI_HYPERVISOR_CONTEXT_ID) {
      *privFlags = VMCI_MAX_PRIVILEGE_FLAGS;
   } else {
      *privFlags = VMCIContext_GetPrivFlags(contextID);
   }

   return VMCI_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagram_GetPrivFlags --
 *
 *     Utilility function that retrieves the privilege flags
 *     associated with a given datagram handle. For hypervisor and
 *     guest endpoints, the privileges are determined by the context
 *     ID, but for host endpoints privileges are associated with the
 *     complete handle.
 *
 *  Result:
 *     VMCI_SUCCESS on success, VMCI_ERROR_INVALID_ARGS if handle is invalid.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDatagram_GetPrivFlags(VMCIHandle handle,             // IN
                          VMCIPrivilegeFlags *privFlags) // OUT
{
   if (privFlags == NULL || handle.context == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   return VMCIDatagramGetPrivFlagsInt(handle.context, handle, privFlags);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDatagramDelayedDispatchCB --
 *
 *      Calls the specified callback in a delayed context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIDatagramDelayedDispatchCB(void *data) // IN
{
   Bool inDGHostQueue;
   VMCIDelayedDatagramInfo *dgInfo = (VMCIDelayedDatagramInfo *)data;

   ASSERT(data);

   dgInfo->entry->recvCB(dgInfo->entry->clientData, &dgInfo->msg);

   VMCIResource_Release(&dgInfo->entry->resource);

   inDGHostQueue = dgInfo->inDGHostQueue;
   VMCI_FreeKernelMem(dgInfo, sizeof *dgInfo + (size_t)dgInfo->msg.payloadSize);

   if (inDGHostQueue) {
      Atomic_Dec(&delayedDGHostQueueSize);
   }
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagramDispatchAsHost --
 *
 *     Dispatch datagram as a host, to the host or other vm context. This
 *     function cannot dispatch to hypervisor context handlers. This should
 *     have been handled before we get here by VMCIDatagramDispatch.
 *
 *  Result:
 *     Number of bytes sent on success, appropriate error code otherwise.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIDatagramDispatchAsHost(VMCIId contextID,  // IN:
                           VMCIDatagram *dg)  // IN:
{
   int retval;
   size_t dgSize;
   VMCIId dstContext;
   VMCIPrivilegeFlags srcPrivFlags;
   char srcDomain[VMCI_DOMAIN_NAME_MAXLEN]; /* Not used on hosted. */
   char dstDomain[VMCI_DOMAIN_NAME_MAXLEN]; /* Not used on hosted. */

   ASSERT(dg);
   ASSERT(VMCI_HostPersonalityActive());

   dgSize = VMCI_DG_SIZE(dg);

   if (contextID == VMCI_HOST_CONTEXT_ID &&
       dg->dst.context == VMCI_HYPERVISOR_CONTEXT_ID) {
      return VMCI_ERROR_DST_UNREACHABLE;
   }

   ASSERT(dg->dst.context != VMCI_HYPERVISOR_CONTEXT_ID);

   /* Chatty. */
   // VMCI_DEBUG_LOG(10, (LGPFX"Sending from (handle=0x%x:0x%x) to "
   //                     "(handle=0x%x:0x%x) (size=%u bytes).\n",
   //                     dg->src.context, dg->src.resource,
   //                     dg->dst.context, dg->dst.resource, (uint32)dgSize));

   /*
    * Check that source handle matches sending context.
    */
   if (dg->src.context != contextID) {
      if (dg->src.context == VMCI_WELL_KNOWN_CONTEXT_ID) {
         /* Determine mapping. */
         DatagramWKMapping *wkMap = DatagramGetWellKnownMap(dg->src.resource);
         if (wkMap == NULL) {
            VMCI_DEBUG_LOG(4, (LGPFX"Sending from invalid well-known resource "
                               "(handle=0x%x:0x%x).\n",
                               dg->src.context, dg->src.resource));
            return VMCI_ERROR_INVALID_RESOURCE;
         }
         if (wkMap->contextID != contextID) {
            VMCI_DEBUG_LOG(4, (LGPFX"Sender context (ID=0x%x) is not owner of "
                               "well-known src datagram entry "
                               "(handle=0x%x:0x%x).\n",
                               contextID, dg->src.context, dg->src.resource));
            DatagramReleaseWellKnownMap(wkMap);
            return VMCI_ERROR_NO_ACCESS;
         }
         DatagramReleaseWellKnownMap(wkMap);
      } else {
         VMCI_DEBUG_LOG(4, (LGPFX"Sender context (ID=0x%x) is not owner of src "
                            "datagram entry (handle=0x%x:0x%x).\n",
                            contextID, dg->src.context, dg->src.resource));
         return VMCI_ERROR_NO_ACCESS;
      }
   }

   if (dg->dst.context == VMCI_WELL_KNOWN_CONTEXT_ID) {
      /* Determine mapping. */
      DatagramWKMapping *wkMap = DatagramGetWellKnownMap(dg->dst.resource);
      if (wkMap == NULL) {
         VMCI_DEBUG_LOG(4, (LGPFX"Sending to invalid wellknown destination "
                            "(handle=0x%x:0x%x).\n",
                            dg->dst.context, dg->dst.resource));
         return VMCI_ERROR_DST_UNREACHABLE;
      }
      dstContext = wkMap->contextID;
      DatagramReleaseWellKnownMap(wkMap);
   } else {
      dstContext = dg->dst.context;
   }

   /*
    * Get hold of privileges of sending endpoint.
    */

   retval = VMCIDatagramGetPrivFlagsInt(contextID, dg->src, &srcPrivFlags);
   if (retval != VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Couldn't get privileges (handle=0x%x:0x%x).\n",
                    dg->src.context, dg->src.resource));
      return retval;
   }

#ifdef VMKERNEL
   /*
    * In the vmkernel, all communicating contexts except the
    * hypervisor context must belong to the same domain. If the
    * hypervisor is the source, the domain doesn't matter.
    */

   if (contextID != VMCI_HYPERVISOR_CONTEXT_ID) {
      retval = VMCIContext_GetDomainName(contextID, srcDomain,
                                         sizeof srcDomain);
      if (retval < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to get domain name for context (ID=0x%x).\n",
                       contextID));
         return retval;
      }
   }
#endif

   /* Determine if we should route to host or guest destination. */
   if (dstContext == VMCI_HOST_CONTEXT_ID) {
      /* Route to host datagram entry. */
      DatagramEntry *dstEntry;
      VMCIResource *resource;

      if (dg->src.context == VMCI_HYPERVISOR_CONTEXT_ID &&
          dg->dst.resource == VMCI_EVENT_HANDLER) {
         return VMCIEvent_Dispatch(dg);
      }

      resource = VMCIResource_Get(dg->dst, VMCI_RESOURCE_TYPE_DATAGRAM);
      if (resource == NULL) {
         VMCI_DEBUG_LOG(4, (LGPFX"Sending to invalid destination "
                            "(handle=0x%x:0x%x).\n",
                            dg->dst.context, dg->dst.resource));
         return VMCI_ERROR_INVALID_RESOURCE;
      }
      dstEntry = RESOURCE_CONTAINER(resource, DatagramEntry, resource);
#ifdef VMKERNEL
      retval = VMCIContext_GetDomainName(VMCI_HOST_CONTEXT_ID, dstDomain,
                                         sizeof dstDomain);
      if (retval < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to get domain name for context (ID=0x%x).\n",
                       VMCI_HOST_CONTEXT_ID));
         VMCIResource_Release(resource);
         return retval;
      }
#endif
      if (VMCIDenyInteraction(srcPrivFlags, dstEntry->privFlags, srcDomain,
                              dstDomain)) {
         VMCIResource_Release(resource);
         return VMCI_ERROR_NO_ACCESS;
      }
      ASSERT(dstEntry->recvCB);

      /*
       * If a VMCI datagram destined for the host is also sent by the
       * host, we always run it delayed. This ensures that no locks
       * are held when the datagram callback runs.
       */

      if (dstEntry->runDelayed ||
          (dg->src.context == VMCI_HOST_CONTEXT_ID &&
           VMCI_CanScheduleDelayedWork())) {
         VMCIDelayedDatagramInfo *dgInfo;

         if (Atomic_FetchAndAdd(&delayedDGHostQueueSize, 1) ==
             VMCI_MAX_DELAYED_DG_HOST_QUEUE_SIZE) {
            Atomic_Dec(&delayedDGHostQueueSize);
            VMCIResource_Release(resource);
            return VMCI_ERROR_NO_MEM;
         }

         dgInfo = VMCI_AllocKernelMem(sizeof *dgInfo + (size_t)dg->payloadSize,
                                      (VMCI_MEMORY_ATOMIC |
                                       VMCI_MEMORY_NONPAGED));
         if (NULL == dgInfo) {
            Atomic_Dec(&delayedDGHostQueueSize);
            VMCIResource_Release(resource);
            return VMCI_ERROR_NO_MEM;
         }

         dgInfo->inDGHostQueue = TRUE;
         dgInfo->entry = dstEntry;
         memcpy(&dgInfo->msg, dg, dgSize);

         retval = VMCI_ScheduleDelayedWork(VMCIDatagramDelayedDispatchCB, dgInfo);
         if (retval < VMCI_SUCCESS) {
            VMCI_WARNING((LGPFX"Failed to schedule delayed work for datagram "
                          "(result=%d).\n", retval));
            VMCI_FreeKernelMem(dgInfo, sizeof *dgInfo + (size_t)dg->payloadSize);
            VMCIResource_Release(resource);
            Atomic_Dec(&delayedDGHostQueueSize);
            return retval;
         }
      } else {
         retval = dstEntry->recvCB(dstEntry->clientData, dg);
         VMCIResource_Release(resource);
         if (retval < VMCI_SUCCESS) {
            return retval;
         }
      }
   } else {
      /* Route to destination VM context. */
      VMCIDatagram *newDG;

#ifdef VMKERNEL
      retval = VMCIContext_GetDomainName(dstContext, dstDomain,
                                         sizeof dstDomain);
      if (retval < VMCI_SUCCESS) {
         VMCI_DEBUG_LOG(4, (LGPFX"Failed to get domain name for context "
                            "(ID=0x%x).\n", dstContext));
         return retval;
      }
#endif
      if (contextID != dstContext &&
         VMCIDenyInteraction(srcPrivFlags, VMCIContext_GetPrivFlags(dstContext),
                             srcDomain, dstDomain)) {
         return VMCI_ERROR_NO_ACCESS;
      }

      /* We make a copy to enqueue. */
      newDG = VMCI_AllocKernelMem(dgSize, VMCI_MEMORY_NORMAL);
      if (newDG == NULL) {
         return VMCI_ERROR_NO_MEM;
      }
      memcpy(newDG, dg, dgSize);
      retval = VMCIContext_EnqueueDatagram(dstContext, newDG);
      if (retval < VMCI_SUCCESS) {
         VMCI_FreeKernelMem(newDG, dgSize);
         return retval;
      }
   }

   /* The datagram is freed when the context reads it. */

   /* Chatty. */
   // VMCI_DEBUG_LOG(10, (LGPFX"Sent datagram (size=%u bytes).\n",
   //                     (uint32)dgSize));

   /*
    * We currently truncate the size to signed 32 bits. This doesn't
    * matter for this handler as it only support 4Kb messages.
    */

   return (int)dgSize;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagramDispatchAsGuest --
 *
 *     Dispatch datagram as a guest, down through the VMX and potentially to
 *     the host.
 *
 *  Result:
 *     Number of bytes sent on success, appropriate error code otherwise.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIDatagramDispatchAsGuest(VMCIDatagram *dg)
{
#if defined(VMKERNEL)
   VMCI_WARNING((LGPFX"Cannot send down to host from VMKERNEL.\n"));
   return VMCI_ERROR_DST_UNREACHABLE;
#else // VMKERNEL
   int retval;
   VMCIResource *resource;

   resource = VMCIResource_Get(dg->src, VMCI_RESOURCE_TYPE_DATAGRAM);
   if (NULL == resource) {
      return VMCI_ERROR_NO_HANDLE;
   }

   retval = VMCI_SendDatagram(dg);
   VMCIResource_Release(resource);
   return retval;
#endif // VMKERNEL
}



/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagram_Dispatch --
 *
 *     Dispatch datagram.  This will determine the routing for the datagram
 *     and dispatch it accordingly.
 *
 *  Result:
 *     Number of bytes sent on success, appropriate error code otherwise.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDatagram_Dispatch(VMCIId contextID,
                      VMCIDatagram *dg,
                      Bool fromGuest)
{
   int retval;
   VMCIRoute route;

   ASSERT(dg);
   ASSERT_ON_COMPILE(sizeof(VMCIDatagram) == 24);

   if (VMCI_DG_SIZE(dg) > VMCI_MAX_DG_SIZE) {
      VMCI_DEBUG_LOG(4, (LGPFX"Payload (size=%"FMT64"u bytes) too big to "
                         "send.\n", dg->payloadSize));
      return VMCI_ERROR_INVALID_ARGS;
   }

   retval = VMCI_Route(&dg->src, &dg->dst, fromGuest, &route);
   if (retval < VMCI_SUCCESS) {
      return retval;
   }

   if (VMCI_ROUTE_AS_HOST == route) {
      if (VMCI_INVALID_ID == contextID) {
         contextID = VMCI_HOST_CONTEXT_ID;
      }
      return VMCIDatagramDispatchAsHost(contextID, dg);
   }

   if (VMCI_ROUTE_AS_GUEST == route) {
      return VMCIDatagramDispatchAsGuest(dg);
   }

   VMCI_WARNING((LGPFX"Unknown route (%d) for datagram.\n", route));
   return VMCI_ERROR_DST_UNREACHABLE;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDatagram_InvokeGuestHandler --
 *
 *     Invoke the handler for the given datagram.  This is intended to be
 *     called only when acting as a guest and receiving a datagram from the
 *     virtual device.
 *
 *  Result:
 *     VMCI_SUCCESS on success, other error values on failure.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDatagram_InvokeGuestHandler(VMCIDatagram *dg) // IN
{
#if defined(VMKERNEL)
   VMCI_WARNING((LGPFX"Cannot dispatch within guest in VMKERNEL.\n"));
   return VMCI_ERROR_DST_UNREACHABLE;
#else // VMKERNEL
   int retval;
   VMCIResource *resource;
   DatagramEntry *dstEntry;

   ASSERT(dg);

   resource = VMCIResource_Get(dg->dst, VMCI_RESOURCE_TYPE_DATAGRAM);
   if (NULL == resource) {
      VMCI_DEBUG_LOG(4, (LGPFX"destination (handle=0x%x:0x%x) doesn't exist.\n",
                         dg->dst.context, dg->dst.resource));
      return VMCI_ERROR_NO_HANDLE;
   }

   dstEntry = RESOURCE_CONTAINER(resource, DatagramEntry, resource);
   if (dstEntry->runDelayed) {
      VMCIDelayedDatagramInfo *dgInfo;

      dgInfo = VMCI_AllocKernelMem(sizeof *dgInfo + (size_t)dg->payloadSize,
                                   (VMCI_MEMORY_ATOMIC | VMCI_MEMORY_NONPAGED));
      if (NULL == dgInfo) {
         VMCIResource_Release(resource);
         retval = VMCI_ERROR_NO_MEM;
         goto exit;
      }

      dgInfo->inDGHostQueue = FALSE;
      dgInfo->entry = dstEntry;
      memcpy(&dgInfo->msg, dg, VMCI_DG_SIZE(dg));

      retval = VMCI_ScheduleDelayedWork(VMCIDatagramDelayedDispatchCB, dgInfo);
      if (retval < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to schedule delayed work for datagram "
                       "(result=%d).\n", retval));
         VMCI_FreeKernelMem(dgInfo, sizeof *dgInfo + (size_t)dg->payloadSize);
         VMCIResource_Release(resource);
         dgInfo = NULL;
         goto exit;
      }
   } else {
      dstEntry->recvCB(dstEntry->clientData, dg);
      VMCIResource_Release(resource);
      retval = VMCI_SUCCESS;
   }

exit:
   return retval;
#endif // VMKERNEL
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDatagram_Send --
 *
 *      Sends the payload to the destination datagram handle.
 *
 * Results:
 *      Returns number of bytes sent if success, or error code if failure.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(VMCIDatagram_Send)
int
VMCIDatagram_Send(VMCIDatagram *msg) // IN
{
   if (msg == NULL) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   return VMCIDatagram_Dispatch(VMCI_INVALID_ID, msg, FALSE);
}


/*
 *------------------------------------------------------------------------------
 *
 * DatagramGetWellKnownMap --
 *
 *      Gets a mapping between handle and wellknown resource.
 *
 * Results:
 *      DatagramWKMapping * if found, NULL if not.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

static DatagramWKMapping *
DatagramGetWellKnownMap(VMCIId wellKnownID)  // IN:
{
   VMCIHashEntry *entry;
   DatagramWKMapping *wkMap = NULL;
   VMCIHandle wkHandle = VMCI_MAKE_HANDLE(VMCI_WELL_KNOWN_CONTEXT_ID,
                                          wellKnownID);
   entry = VMCIHashTable_GetEntry(wellKnownTable, wkHandle);
   if (entry != NULL) {
      wkMap = RESOURCE_CONTAINER(entry, DatagramWKMapping, entry);
   }
   return wkMap;
}


/*
 *------------------------------------------------------------------------------
 *
 * DatagramReleaseWellKnownMap --
 *
 *      Releases a wellknown mapping.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

static void
DatagramReleaseWellKnownMap(DatagramWKMapping *wkMap)  // IN:
{
   if (VMCIHashTable_ReleaseEntry(wellKnownTable, &wkMap->entry) ==
       VMCI_SUCCESS_ENTRY_DEAD) {
      VMCI_FreeKernelMem(wkMap, sizeof *wkMap);
   }
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDatagramRequestWellKnownMap --
 *
 *      Creates a mapping between handle and wellknown resource. If resource
 *      is already used we fail the request.
 *
 * Results:
 *      VMCI_SUCCESS if created, negative errno value otherwise.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDatagramRequestWellKnownMap(VMCIId wellKnownID,           // IN:
                                VMCIId contextID,             // IN:
                                VMCIPrivilegeFlags privFlags) // IN:
{
   int result;
   DatagramWKMapping *wkMap;
   VMCIHandle wkHandle = VMCI_MAKE_HANDLE(VMCI_WELL_KNOWN_CONTEXT_ID,
                                          wellKnownID);

   if (privFlags & VMCI_PRIVILEGE_FLAG_RESTRICTED ||
       !VMCIWellKnownID_AllowMap(wellKnownID, privFlags)) {
      return VMCI_ERROR_NO_ACCESS;
   }

   wkMap = VMCI_AllocKernelMem(sizeof *wkMap, VMCI_MEMORY_NONPAGED);
   if (wkMap == NULL) {
      return VMCI_ERROR_NO_MEM;
   }

   VMCIHashTable_InitEntry(&wkMap->entry, wkHandle);
   wkMap->contextID = contextID;

   /* Fails if wkHandle (wellKnownID) already exists. */
   result = VMCIHashTable_AddEntry(wellKnownTable, &wkMap->entry);
   if (result != VMCI_SUCCESS) {
      VMCI_FreeKernelMem(wkMap, sizeof *wkMap);
      return result;
   }
   result = VMCIContext_AddWellKnown(contextID, wellKnownID);
   if (UNLIKELY(result < VMCI_SUCCESS)) {
      VMCIHashTable_RemoveEntry(wellKnownTable, &wkMap->entry);
      VMCI_FreeKernelMem(wkMap, sizeof *wkMap);
   }
   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDatagramRemoveWellKnownMap --
 *
 *      Removes a mapping between handle and wellknown resource. Checks if
 *      mapping belongs to calling context.
 *
 * Results:
 *      VMCI_SUCCESS if removed, negative errno value otherwise.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDatagramRemoveWellKnownMap(VMCIId wellKnownID,  // IN:
                               VMCIId contextID)    // IN:
{
   int result = VMCI_ERROR_NO_ACCESS;
   DatagramWKMapping *wkMap = DatagramGetWellKnownMap(wellKnownID);
   if (wkMap == NULL) {
      VMCI_DEBUG_LOG(4, (LGPFX"Failed to remove well-known mapping between "
                         "resource (ID=0x%x) and context (ID=0x%x).\n",
                         wellKnownID, contextID));
      return VMCI_ERROR_NOT_FOUND;
   }

   if (contextID == wkMap->contextID) {
      VMCIHashTable_RemoveEntry(wellKnownTable, &wkMap->entry);
      VMCIContext_RemoveWellKnown(contextID, wellKnownID);
      result = VMCI_SUCCESS;
   }
   DatagramReleaseWellKnownMap(wkMap);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDatagram_Sync --
 *
 *      Use this as a synchronization point when setting globals, for example,
 *      during device shutdown.
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
VMCIDatagram_Sync(void)
{
   VMCIResource_Sync();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDatagram_CheckHostCapabilities --
 *
 *      Verify that the host supports the resources we need.
 *      None are required for datagrams since they are implicitly supported.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCIDatagram_CheckHostCapabilities(void)
{
   return TRUE;
}
