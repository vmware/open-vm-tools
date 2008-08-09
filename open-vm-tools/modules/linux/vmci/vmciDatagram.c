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
 *      Simple Datagram API for the Linux guest driver.
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
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_kernel_if.h"
#include "vmci_infrastructure.h"
#include "vmciInt.h"
#include "vmciUtil.h"
#include "vmciDatagram.h"

typedef struct DatagramHashEntry {
   struct DatagramHashEntry  *next;
   int                       refCount;

   VMCIHandle                handle;
   uint32                    flags;
   VMCIDatagramRecvCB        recvCB;
   void                      *clientData;
   VMCIEvent                 destroyEvent;
} DatagramHashEntry;

#define HASH_TABLE_SIZE 64

/* 
 * Hash table containing all the datagram handles for this VM. It is 
 * synchronized using a single lock but we should consider making it more
 * fine grained, e.g. a per bucket lock or per set of buckets' lock.
 */

typedef struct DatagramHashTable {
   VMCILock lock;
   DatagramHashEntry *entries[HASH_TABLE_SIZE];
} DatagramHashTable; 


static int DatagramReleaseCB(void *clientData);
static int DatagramHashAddEntry(DatagramHashEntry *entry, VMCIId contextID);
static int DatagramHashRemoveEntry(VMCIHandle handle);
static DatagramHashEntry *DatagramHashGetEntry(VMCIHandle handle);
static void DatagramHashReleaseEntry(DatagramHashEntry *entry);
static Bool DatagramHandleUniqueLocked(VMCIHandle handle);
static int DatagramProcessNotify(void *clientData, VMCIDatagram *msg);

DatagramHashTable hashTable;

/*
 *------------------------------------------------------------------------------
 *
 *  DatagramReleaseCB --
 *
 *     Callback to release the datagram entry reference. It is called by the 
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
   DatagramHashEntry *entry = (DatagramHashEntry *)clientData;
   ASSERT(entry);
   DatagramHashReleaseEntry(entry);

   return 0;
}


/*
 *-------------------------------------------------------------------------
 *
 *  DatagramHashAddEntry --
 *     Given a datagram handle entry, adds it to the hashtable of datagram
 *     entries.  Allocates a resource id iff the handle of the given entry
 *     is an invalid one.  0 through VMCI_RESERVED_RESOURCE_ID_MAX are
 *     reserved resource ids.
 *
 *  Result:
 *     VMCI_SUCCESS if added, error if not.
 *     
 *-------------------------------------------------------------------------
 */

static int
DatagramHashAddEntry(DatagramHashEntry *entry, // IN:
                     VMCIId contextID)         // IN:
{
   int idx;
   VMCILockFlags flags;
   static VMCIId datagramRID = VMCI_RESERVED_RESOURCE_ID_MAX + 1;

   ASSERT(entry && contextID != VMCI_INVALID_ID);

   VMCI_GrabLock_BH(&hashTable.lock, &flags);
   if (!VMCI_HANDLE_INVALID(entry->handle) &&
       !DatagramHandleUniqueLocked(entry->handle)) {
      VMCI_ReleaseLock_BH(&hashTable.lock, flags);
      return VMCI_ERROR_DUPLICATE_ENTRY;
   } else if (VMCI_HANDLE_INVALID(entry->handle)) {
      VMCIId oldRID = datagramRID;
      VMCIHandle handle;
      Bool foundRID = FALSE;

      /*
       * Generate a unique datagram rid.  Keep on trying until we wrap around
       * in the RID space.
       */
      ASSERT(oldRID > VMCI_RESERVED_RESOURCE_ID_MAX);
      do {
         handle = VMCI_MAKE_HANDLE(contextID, datagramRID);
         foundRID = DatagramHandleUniqueLocked(handle);
         datagramRID++;
         if (UNLIKELY(!datagramRID)) {
            /*
             * Skip the reserved rids.
             */
            datagramRID = VMCI_RESERVED_RESOURCE_ID_MAX + 1;
         }
      } while (!foundRID && datagramRID != oldRID);

      if (LIKELY(foundRID)) {
         entry->handle = handle;
      } else {
         /*
          * We wrapped around --- no rids were free.
          */
         ASSERT(datagramRID == oldRID);
         VMCI_ReleaseLock_BH(&hashTable.lock, flags);
         return VMCI_ERROR_NO_HANDLE;
      }
   }
   
   ASSERT(!VMCI_HANDLE_INVALID(entry->handle));
   idx = VMCI_Hash(entry->handle, HASH_TABLE_SIZE);

   /* New entry is added to top/front of hash bucket. */
   entry->refCount++;
   entry->next = hashTable.entries[idx];
   hashTable.entries[idx] = entry;
   VMCI_ReleaseLock_BH(&hashTable.lock, flags);

   return VMCI_SUCCESS;
}


/*
 *-------------------------------------------------------------------------
 *
 *  DatagramHashRemoveEntry --
 *
 *  Result:
 *     VMCI_SUCCESS if removed, VMCI_ERROR_NO_HANDLE if not found.
 *     
 *-------------------------------------------------------------------------
 */

static int
DatagramHashRemoveEntry(VMCIHandle handle)
{ 
   int result = VMCI_ERROR_NOT_FOUND;
   VMCILockFlags flags;
   DatagramHashEntry *prev, *cur;
   int idx = VMCI_Hash(handle, HASH_TABLE_SIZE);

   prev = NULL;
   VMCI_GrabLock_BH(&hashTable.lock, &flags);
   cur = hashTable.entries[idx];
   while (TRUE) {
      if (cur == NULL) {
	 break;
      }
      if (VMCI_HANDLE_EQUAL(cur->handle, handle)) {
	 /* Remove entry and break. */
	 if (prev) {
	    prev->next = cur->next;
	 } else {
	    hashTable.entries[idx] = cur->next;
	 }

	 cur->refCount--;
	 
	 /* 
	  * We know that DestroyHnd still has a reference so refCount must be 
	  * at least 1.
	  */	  
	 ASSERT(cur->refCount > 0);
	 result = VMCI_SUCCESS;
	 break;
      }
      prev = cur;
      cur = cur->next;
   }
   VMCI_ReleaseLock_BH(&hashTable.lock, flags);

   return result;
}


/*
 *-------------------------------------------------------------------------
 *
 *  DatagramHashGetEntry --
 *
 *  Result:
 *     None.
 *     
 *-------------------------------------------------------------------------
 */

static DatagramHashEntry *
DatagramHashGetEntry(VMCIHandle handle)
{
   VMCILockFlags flags;
   DatagramHashEntry *cur;
   int idx = VMCI_Hash(handle, HASH_TABLE_SIZE);
   
   VMCI_GrabLock_BH(&hashTable.lock, &flags);
   for (cur = hashTable.entries[idx]; cur != NULL; cur = cur->next) {
      if (VMCI_HANDLE_EQUAL(cur->handle, handle)) {
	 cur->refCount++;
	 break;
      }
   }
   VMCI_ReleaseLock_BH(&hashTable.lock, flags);
   
   return cur;
}


/*
 *-------------------------------------------------------------------------
 *
 *  DatagramHashReleaseEntry --
 *
 *  Result:
 *     None.
 *     
 *-------------------------------------------------------------------------
 */

static void
DatagramHashReleaseEntry(DatagramHashEntry *entry)
{
   VMCILockFlags flags;

   VMCI_GrabLock_BH(&hashTable.lock, &flags);
   entry->refCount--;

   /* Check if this is last reference and signal the destroy event if so. */
   if (entry->refCount == 0) { 
      VMCI_SignalEvent(&entry->destroyEvent);
   }
   VMCI_ReleaseLock_BH(&hashTable.lock, flags);
}


/*
 *------------------------------------------------------------------------------
 *
 *  DatagramHandleUniqueLocked --
 *
 *     Checks whether the given handle is already in the hash
 *     table. Assumes that the caller to have the hash table lock.
 *
 *  Result:
 *     None.
 *     
 *------------------------------------------------------------------------------
 */

static Bool
DatagramHandleUniqueLocked(VMCIHandle handle)
{
   Bool unique = TRUE;
   DatagramHashEntry *entry;
   int idx = VMCI_Hash(handle, HASH_TABLE_SIZE);

   entry = hashTable.entries[idx];
   while (entry) {
      if (VMCI_HANDLE_EQUAL(entry->handle, handle)) {
	 unique = FALSE;
	 break;
      }
      entry = entry->next;
   }

   return unique;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDatagram_CreateHnd --
 *
 *      Creates a datagram endpoint and returns a handle to it.
 *
 * Results:
 *      Returns handle if success, negative errno value otherwise.
 *
 * Side effects:
 *      Datagram endpoint is created both in guest and on host.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCIDatagram_CreateHnd);
#endif

int
VMCIDatagram_CreateHnd(VMCIId resourceID,          // IN:
                       uint32 flags,               // IN:
                       VMCIDatagramRecvCB recvCB,  // IN:
                       void *clientData,           // IN:
                       VMCIHandle *outHandle)      // OUT:
   
{
   int result;
   DatagramHashEntry *entry;
   VMCIHandle handle;
   VMCIId contextID = VMCI_GetContextID();

   if (!recvCB || !outHandle) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   /* Validate contextID. */
   if (contextID == VMCI_INVALID_ID) {
      return VMCI_ERROR_NO_RESOURCES;
   }

   if ((flags & VMCI_FLAG_WELLKNOWN_DG_HND) != 0) {
      VMCIDatagramWellKnownMapMsg wkMsg;  
      if (resourceID == VMCI_INVALID_ID) {
	 return VMCI_ERROR_INVALID_ARGS;
      }
      wkMsg.hdr.dst.context = VMCI_HYPERVISOR_CONTEXT_ID;
      wkMsg.hdr.dst.resource = VMCI_DATAGRAM_REQUEST_MAP;
      wkMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
      wkMsg.hdr.payloadSize = sizeof wkMsg - VMCI_DG_HEADERSIZE;
      wkMsg.wellKnownID = resourceID;
      result = VMCI_SendDatagram((VMCIDatagram *)&wkMsg);
      if (result < VMCI_SUCCESS) {
	 VMCI_LOG(("Failed to reserve wellknown id %d, error %d.\n",
		   resourceID, result));
	 return result;
      }

      handle = VMCI_MAKE_HANDLE(VMCI_WELL_KNOWN_CONTEXT_ID, resourceID);
   } else {
      if (resourceID == VMCI_INVALID_ID) {
         handle = VMCI_INVALID_HANDLE;
      } else {
         handle = VMCI_MAKE_HANDLE(contextID, resourceID);
      }
   }

   /* Update local datastructure. */
   entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_NONPAGED);
   if (entry == NULL) {
      return VMCI_ERROR_NO_MEM;
   }
   
   entry->handle = handle;
   entry->flags = flags;
   entry->recvCB = recvCB;
   entry->clientData = clientData;
   entry->refCount = 0;
   VMCI_CreateEvent(&entry->destroyEvent);

   result = DatagramHashAddEntry(entry, contextID);
   if (result != VMCI_SUCCESS) {
      VMCI_LOG(("Failed to add new entry, err 0x%x.\n", result));
      VMCI_DestroyEvent(&entry->destroyEvent);
      VMCI_FreeKernelMem(entry, sizeof *entry);
      return result;
   }

   ASSERT(!VMCI_HANDLE_INVALID(entry->handle));
   *outHandle = entry->handle;
   return VMCI_SUCCESS;
}

		      
/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDatagram_DestroyHnd --
 *
 *      Destroys a handle.
 *
 * Results:
 *      VMCI_SUCCESS or error code.
 *
 * Side effects:
 *      Host and guest state is cleaned up.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCIDatagram_DestroyHnd);
#endif

int
VMCIDatagram_DestroyHnd(VMCIHandle handle)       // IN
{
   DatagramHashEntry *entry = DatagramHashGetEntry(handle);
   if (entry == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }
   
   DatagramHashRemoveEntry(entry->handle);

   /*
    * We wait for destroyEvent to be signalled. The resource is released
    * as part of the wait.
    */
   VMCI_WaitOnEvent(&entry->destroyEvent, DatagramReleaseCB, entry);

   
   if ((entry->flags & VMCI_FLAG_WELLKNOWN_DG_HND) != 0) {
      int result;
      VMCIDatagramWellKnownMapMsg wkMsg;

      wkMsg.hdr.dst.context = VMCI_HYPERVISOR_CONTEXT_ID;
      wkMsg.hdr.dst.resource = VMCI_DATAGRAM_REMOVE_MAP;
      wkMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
      wkMsg.hdr.payloadSize = sizeof wkMsg - VMCI_DG_HEADERSIZE;
      wkMsg.wellKnownID = entry->handle.resource;
      result = VMCI_SendDatagram((VMCIDatagram *)&wkMsg);
      if (result < VMCI_SUCCESS) {
	 VMCI_LOG(("Failed to remove well-known mapping for resource %d.\n",
		   entry->handle.resource));
      }
   }
   
   /* We know we are now holding the last reference so we can free the entry. */
   VMCI_DestroyEvent(&entry->destroyEvent);
   VMCI_FreeKernelMem(entry, sizeof *entry);

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
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
 *-----------------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCIDatagram_Send);
#endif

int
VMCIDatagram_Send(VMCIDatagram *msg) // IN		
{
   uint32 retval;
   DatagramHashEntry *entry;

   if (msg == NULL) {
      VMCI_LOG(("Invalid datagram.\n"));
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (VMCI_DG_SIZE(msg) > VMCI_MAX_DG_SIZE) {
      VMCI_LOG(("Payload size %"FMT64"u too big to send.\n", msg->payloadSize));
      return VMCI_ERROR_INVALID_ARGS;
   }

   /* Check srcHandle exists otherwise fail. */
   entry = DatagramHashGetEntry(msg->src);
   if (entry == NULL) {
      VMCI_LOG(("Couldn't find handle 0x%x:0x%x.\n", 
		msg->src.context, msg->src.resource));
      return VMCI_ERROR_INVALID_ARGS;
   }
   
   retval = VMCI_SendDatagram(msg);
   DatagramHashReleaseEntry(entry);

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDatagram_Dispatch --
 *
 *      Forwards the datagram corresponding entry's callback.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code if not.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIDatagram_Dispatch(VMCIId contextID,  // IN: unused
		      VMCIDatagram *msg) // IN
{
   DatagramHashEntry *entry;
   
   ASSERT(msg);

   entry = DatagramHashGetEntry(msg->dst);
   if (entry == NULL) {
      VMCI_LOG(("destination handle 0x%x:0x%x doesn't exists.\n",
		msg->dst.context, msg->dst.resource));
      return VMCI_ERROR_NO_HANDLE;
   }
   
   if (entry->recvCB) {
      entry->recvCB(entry->clientData, msg);
   } else {
      VMCI_LOG(("no handle callback for handle 0x%x:0x%x payload of "
		"size %"FMT64"d.\n", 
		msg->dst.context, msg->dst.resource, msg->payloadSize));
   }
   DatagramHashReleaseEntry(entry);

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDatagram_Init --
 *
 *      Register guest call handlers.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void 
VMCIDatagram_Init(void)
{
   int i;
   
   VMCI_InitLock(&hashTable.lock,
                 "VMCIDatagramHashtable",
                 VMCI_LOCK_RANK_MIDDLE_BH);
   for (i = 0; i < HASH_TABLE_SIZE; i++) {
      hashTable.entries[i] = NULL;
   }
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


/*
 *-----------------------------------------------------------------------------
 *
 * DatagramProcessNotify --
 *
 *      Callback to send a notificaton to a vmci process. Creates datagram
 *      copy and signals the process.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
DatagramProcessNotify(void *clientData,   // IN:
                      VMCIDatagram *msg)  // IN:
{
   VMCIDatagramProcess *dgmProc = (VMCIDatagramProcess *) clientData;
   size_t dgmSize;
   VMCIDatagram *dgm;
   DatagramQueueEntry *dqEntry;
   VMCILockFlags flags;
   
   ASSERT(dgmProc != NULL && msg != NULL);
   dgmSize = VMCI_DG_SIZE(msg);
   ASSERT(dgmSize <= VMCI_MAX_DG_SIZE);

   dgm = VMCI_AllocKernelMem(dgmSize,
			     VMCI_MEMORY_NONPAGED | VMCI_MEMORY_ATOMIC);
   if (!dgm) {
      VMCI_LOG(("VMCI: Failed to allocate datagram of size %d bytes.\n",
		(uint32)dgmSize));
      return VMCI_ERROR_NO_MEM;
   }
   memcpy(dgm, msg, dgmSize);

   /* Allocate datagram queue entry and add it to the target fd's queue. */
   dqEntry = VMCI_AllocKernelMem(sizeof *dqEntry, 
				 VMCI_MEMORY_NONPAGED | VMCI_MEMORY_ATOMIC);
   if (dqEntry == NULL) {
      VMCI_FreeKernelMem(dgm, dgmSize);
      VMCI_LOG(("VMCI: Failed to allocate memory for process datagram.\n"));
      return VMCI_ERROR_NO_MEM;
   }
   dqEntry->dg = dgm;

   VMCI_GrabLock_BH(&dgmProc->datagramQueueLock, &flags);
   if (dgmProc->datagramQueueSize + dgmSize >= VMCI_MAX_DATAGRAM_QUEUE_SIZE) {
      VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
      VMCI_FreeKernelMem(dgm, dgmSize);
      VMCI_FreeKernelMem(dqEntry, sizeof *dqEntry);
      VMCI_LOG(("VMCI: Datagram process receive queue is full.\n"));
      return VMCI_ERROR_NO_RESOURCES;
   }

   LIST_QUEUE(&dqEntry->listItem, &dgmProc->datagramQueue);
   dgmProc->pendingDatagrams++;
   dgmProc->datagramQueueSize += dgmSize;
#ifdef SOLARIS
   /* 
    * Release the lock here for Solaris. Otherwise, a deadlock 
    * may occur since pollwakeup(9F) (invoked from VMCIHost_SignalCall)
    * and poll_common (invoked from poll(2)) try to grab a common lock.
    * The man pages of pollwakeup(9F) and chpoll(9E) talk about this.
    */
   VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
#endif
   VMCIHost_SignalCall(&dgmProc->host);
#ifndef SOLARIS
   /* For platforms other than Solaris, release the lock here. */
   VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
#endif

   DEBUG_ONLY(VMCI_LOG(("VMCI: Sent datagram with resource id %d and size %u.\n",
                        msg->dst.resource, (uint32)dgmSize));)
   /* dqEntry and dgm are freed when user reads call.. */

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIDatagramProcess_Create --
 *
 *      Creates a new VMCIDatagramProcess object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIDatagramProcess_Create(VMCIDatagramProcess **outDgmProc,    // IN:
                           VMCIDatagramCreateInfo *createInfo)  // IN:
{
   VMCIDatagramProcess *dgmProc;

   ASSERT(createInfo);
   ASSERT(outDgmProc);
   dgmProc = VMCI_AllocKernelMem(sizeof *dgmProc, VMCI_MEMORY_NONPAGED);
   if (dgmProc == NULL) {
      return VMCI_ERROR_NO_MEM;
   }

   VMCI_InitLock(&dgmProc->datagramQueueLock, "VMCIDgmProc",
		 VMCI_LOCK_RANK_MIDDLE_BH);
   VMCIHost_InitContext(&dgmProc->host, createInfo->eventHnd);
   dgmProc->pendingDatagrams = 0;
   dgmProc->datagramQueueSize = 0;
   dgmProc->datagramQueue = NULL;

   /*
    * We pass the result and corresponding handle to user level via the 
    * createInfo.
    */
   createInfo->result = VMCIDatagram_CreateHnd(createInfo->resourceID,
					       createInfo->flags,
					       DatagramProcessNotify,
					       (void *)dgmProc,
					       &dgmProc->handle);
   if (createInfo->result < VMCI_SUCCESS) {
      VMCI_FreeKernelMem(dgmProc, sizeof *dgmProc);
      return createInfo->result;
   }
   createInfo->handle = dgmProc->handle;

   *outDgmProc = dgmProc;
   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIDatagramProcess_Destroy --
 *
 *      Destroys a VMCIDatagramProcess object.
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
VMCIDatagramProcess_Destroy(VMCIDatagramProcess *dgmProc) // IN:
{
   ListItem *curr, *next;
   DatagramQueueEntry *dqEntry;
   VMCILockFlags flags;

   if (!dgmProc) {
      return;
   }

   if (!VMCI_HANDLE_EQUAL(dgmProc->handle, VMCI_INVALID_HANDLE)) {

      /* 
       * We block in destroy so we know that there can be no more 
       * callbacks to DatagramProcessNotifyCB when we return from
       * this call.
       */
      VMCIDatagram_DestroyHnd(dgmProc->handle);
      dgmProc->handle = VMCI_INVALID_HANDLE;
   }

   /* Flush dgmProc's call queue. */
   VMCI_GrabLock_BH(&dgmProc->datagramQueueLock, &flags);
   LIST_SCAN_SAFE(curr, next, dgmProc->datagramQueue) {
      dqEntry = LIST_CONTAINER(curr, DatagramQueueEntry, listItem);
      LIST_DEL(curr, &dgmProc->datagramQueue);
      ASSERT(dqEntry && dqEntry->dg);
      VMCI_FreeKernelMem(dqEntry->dg, VMCI_DG_SIZE(dqEntry->dg));
      VMCI_FreeKernelMem(dqEntry, sizeof *dqEntry);
   }
   VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
   VMCIHost_ReleaseContext(&dgmProc->host);
   VMCI_CleanupLock(&dgmProc->datagramQueueLock);
   VMCI_FreeKernelMem(dgmProc, sizeof *dgmProc);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIDatagramProcess_ReadCall --
 *
 *      Dequeues the next guest call and returns it to user level.
 *
 * Results:
 *      0 on success, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIDatagramProcess_ReadCall(VMCIDatagramProcess *dgmProc, // IN:
                             size_t maxSize,               // IN: max size of dg
                             VMCIDatagram **dg)            // OUT: 
{
   DatagramQueueEntry *dqEntry;
   ListItem *listItem;
   VMCILockFlags flags;

   ASSERT(dgmProc);
   ASSERT(dg);

   /* Dequeue the next dgmProc datagram queue entry. */
   VMCI_GrabLock_BH(&dgmProc->datagramQueueLock, &flags);

   /*
    * Currently, we do not support blocking read of datagrams on Mac and
    * Solaris. XXX: This will go away soon.
    */

#if defined(SOLARIS) || defined(__APPLE__)
   if (dgmProc->pendingDatagrams == 0) {
      VMCIHost_ClearCall(&dgmProc->host);
      VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
      VMCI_LOG(("VMCI: No datagrams pending.\n"));
      return VMCI_ERROR_NO_MORE_DATAGRAMS;
   }
#else
   while (dgmProc->pendingDatagrams == 0) {
      VMCIHost_ClearCall(&dgmProc->host);
      if (!VMCIHost_WaitForCallLocked(&dgmProc->host, &dgmProc->datagramQueueLock,
                                      &flags, TRUE)) {
         VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
         VMCI_LOG(("VMCI: Blocking read of datagram interrupted.\n"));
         return VMCI_ERROR_NO_MORE_DATAGRAMS;
      }
   }
#endif

   listItem = LIST_FIRST(dgmProc->datagramQueue);
   ASSERT (listItem != NULL);

   dqEntry = LIST_CONTAINER(listItem, DatagramQueueEntry, listItem);
   ASSERT(dqEntry->dg);

   /* Check the size of the userland buffer. */
   if (maxSize < VMCI_DG_SIZE(dqEntry->dg)) {
      VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
      VMCI_LOG(("VMCI: Caller's buffer is too small.\n"));
      return VMCI_ERROR_NO_MEM;
   }
   
   LIST_DEL(listItem, &dgmProc->datagramQueue);
   dgmProc->pendingDatagrams--;
   dgmProc->datagramQueueSize -= VMCI_DG_SIZE(dqEntry->dg);
   if (dgmProc->pendingDatagrams == 0) {
      VMCIHost_ClearCall(&dgmProc->host);
   }
   VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);

   *dg = dqEntry->dg;
   VMCI_FreeKernelMem(dqEntry, sizeof *dqEntry);

   return VMCI_SUCCESS;
}

