/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * vmci_kernel_if.h --
 *
 *      This file defines helper functions for VMCI host _and_ guest
 *      kernel code. It must work for Windows, Mac OS, vmkernel, Linux and
 *      Solaris kernels, i.e. using defines where necessary.
 */

#ifndef _VMCI_KERNEL_IF_H_
#define _VMCI_KERNEL_IF_H_

#if !defined(__linux__) && !defined(_WIN32) && !defined(__APPLE__) && \
    !defined(VMKERNEL)
#  error "Platform not supported."
#endif

#if defined(_WIN32)
#  include <ntddk.h>
#else
#define UNREFERENCED_PARAMETER(P)
#endif

#if defined(__linux__) && !defined(VMKERNEL)
#  include "driver-config.h"
#  include "compat_cred.h"
#  include "compat_module.h"
#  include "compat_semaphore.h"
#  include "compat_spinlock.h"
#  include "compat_version.h"
#  include <linux/wait.h>
#endif // linux

#ifdef __APPLE__
#  include <IOKit/IOLib.h>
#  include <mach/task.h>
#  include <mach/semaphore.h>
#  include <sys/kauth.h>
#endif

#ifdef VMKERNEL
#  include "splock.h"
#  include "splock_customRanks.h"
#  include "semaphore_ext.h"
#  include "vmkapi.h"
#  include "world_dist.h"
#endif

#include "vm_basic_types.h"
#include "vmci_defs.h"

#if defined(VMKERNEL)
#  include "list.h"
#else
#  include "dbllnklst.h"
#endif

#if defined __cplusplus
extern "C" {
#endif


/* Flags for specifying memory type. */
#define VMCI_MEMORY_NORMAL   0x0
#define VMCI_MEMORY_ATOMIC   0x1
#define VMCI_MEMORY_NONPAGED 0x2

/* Platform specific type definitions. */

#if defined(VMKERNEL)
#  define VMCI_EXPORT_SYMBOL(_SYMBOL)  VMK_MODULE_EXPORT_SYMBOL(_SYMBOL);
#elif defined(__linux__)
#  define VMCI_EXPORT_SYMBOL(_symbol)  EXPORT_SYMBOL(_symbol);
#elif defined(__APPLE__)
#  define VMCI_EXPORT_SYMBOL(_symbol)  __attribute__((visibility("default")))
#else
#  define VMCI_EXPORT_SYMBOL(_symbol)
#endif

#if defined(VMKERNEL)
  typedef MCSLock VMCILock;
  typedef SP_IRQL VMCILockFlags;
  typedef Semaphore VMCIEvent;
  typedef Semaphore VMCIMutex;
  typedef World_ID VMCIHostVmID;
  typedef uint32 VMCIHostUser;
  typedef PPN *VMCIQPGuestMem;
#elif defined(__linux__)
  typedef spinlock_t VMCILock;
  typedef unsigned long VMCILockFlags;
  typedef wait_queue_head_t VMCIEvent;
  typedef struct semaphore VMCIMutex;
  typedef PPN *VMCIPpnList; /* List of PPNs in produce/consume queue. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
  typedef kuid_t VMCIHostUser;
#else
  typedef uid_t VMCIHostUser;
#endif
  typedef VA64 VMCIQPGuestMem;
#elif defined(__APPLE__)
  typedef IOLock *VMCILock;
  typedef unsigned long VMCILockFlags;
  typedef struct {
     IOLock *lock;
     DblLnkLst_Links waiters;
     int buffered;
  } VMCIEvent;
  typedef IOLock *VMCIMutex;
  typedef void *VMCIPpnList; /* Actually a pointer to the C++ Object IOMemoryDescriptor */
  typedef uid_t VMCIHostUser;
  typedef VA64 *VMCIQPGuestMem;
#elif defined(_WIN32)
  typedef KSPIN_LOCK VMCILock;
  typedef KIRQL VMCILockFlags;
  typedef KEVENT VMCIEvent;
  typedef FAST_MUTEX VMCIMutex;
  typedef PMDL VMCIPpnList; /* MDL to map the produce/consume queue. */
  typedef PSID VMCIHostUser;
  typedef VA64 *VMCIQPGuestMem;
#endif // VMKERNEL

/* Callback needed for correctly waiting on events. */
typedef int (*VMCIEventReleaseCB)(void *clientData);

/*
 * Internal locking dependencies within VMCI:
 * * CONTEXTFIRE < CONTEXT, CONTEXTLIST, EVENT, HASHTABLE
 * * DOORBELL < HASHTABLE
 * * QPHIBERNATE < EVENT
 */

#ifdef VMKERNEL
  typedef Lock_Rank VMCILockRank;
  typedef SemaRank VMCISemaRank;

  #define VMCI_SEMA_RANK_QPHEADER       (SEMA_RANK_FS - 1)

  #define VMCI_LOCK_RANK_MAX_NONBLOCK   (MIN(SP_RANK_WAIT, \
                                             SP_RANK_HEAPLOCK_DYNAMIC) - 1)
  #define VMCI_LOCK_RANK_MAX            (SP_RANK_BLOCKABLE_HIGHEST_MAJOR - 2)

  /*
   * Determines whether VMCI locks will be blockable or not. If blockable,
   * all locks will be at or below VMCI_LOCK_RANK_MAX. If not, locks will
   * instead use VMCI_LOCK_RANK_MAX_NONBLOCK as the maximum. The other
   * VMCI_LOCK_RANK_XXX values will be rebased to be non-blocking as well
   * in that case.
   */
  extern Bool vmciBlockableLock;
#else
  typedef unsigned long VMCILockRank;
  typedef unsigned long VMCISemaRank;

  #define VMCI_LOCK_RANK_MAX            0x0fff

  #define VMCI_SEMA_RANK_QPHEADER       0x0fff
#endif // VMKERNEL
#define VMCI_LOCK_RANK_CONTEXT          VMCI_LOCK_RANK_MAX
#define VMCI_LOCK_RANK_CONTEXTLIST      VMCI_LOCK_RANK_MAX
#define VMCI_LOCK_RANK_DATAGRAMVMK      VMCI_LOCK_RANK_MAX
#define VMCI_LOCK_RANK_EVENT            VMCI_LOCK_RANK_MAX
#define VMCI_LOCK_RANK_HASHTABLE        VMCI_LOCK_RANK_MAX
#define VMCI_LOCK_RANK_RESOURCE         VMCI_LOCK_RANK_MAX
#define VMCI_LOCK_RANK_QPHEADER         VMCI_LOCK_RANK_MAX
#define VMCI_LOCK_RANK_DOORBELL         (VMCI_LOCK_RANK_HASHTABLE - 1)
#define VMCI_LOCK_RANK_CONTEXTFIRE      (MIN(VMCI_LOCK_RANK_CONTEXT, \
                                         MIN(VMCI_LOCK_RANK_CONTEXTLIST, \
                                         MIN(VMCI_LOCK_RANK_EVENT, \
                                             VMCI_LOCK_RANK_HASHTABLE))) - 1)
#define VMCI_LOCK_RANK_QPHIBERNATE      (VMCI_LOCK_RANK_EVENT - 1)
#define VMCI_LOCK_RANK_PACKET_QP        (VMCI_LOCK_RANK_QPHEADER - 1)
//#define VMCI_LOCK_RANK_PACKET_QP        0xffd /* For vVol */

#define VMCI_SEMA_RANK_QUEUEPAIRLIST    (VMCI_SEMA_RANK_QPHEADER - 1)
#define VMCI_SEMA_RANK_GUESTMEM         (VMCI_SEMA_RANK_QUEUEPAIRLIST - 1)

/*
 * Host specific struct used for signalling.
 */

typedef struct VMCIHost {
#if defined(VMKERNEL)
   World_ID vmmWorldID[2];   /*
                              * First one is the active one and the second
                              * one is shadow world during FSR.
                              */
#elif defined(__linux__)
   wait_queue_head_t  waitQueue;
#elif defined(__APPLE__)
   struct Socket *socket; /* vmci Socket object on Mac OS. */
#elif defined(_WIN32)
   KEVENT *callEvent; /* Ptr to userlevel event used when signalling
                       * new pending guestcalls in kernel.
                       */
#endif
} VMCIHost;

/*
 * Guest device port I/O.
 */

#if defined(__linux__)
   typedef unsigned short int VMCIIoPort;
   typedef int VMCIIoHandle;
#elif defined(_WIN32)
   typedef PUCHAR VMCIIoPort;
   typedef int VMCIIoHandle;
#elif defined(__APPLE__)
   typedef unsigned short int VMCIIoPort;
   typedef void *VMCIIoHandle;
#endif // __APPLE__

void VMCI_ReadPortBytes(VMCIIoHandle handle, VMCIIoPort port, uint8 *buffer,
                        size_t bufferLength);


int VMCI_InitLock(VMCILock *lock, char *name, VMCILockRank rank);
void VMCI_CleanupLock(VMCILock *lock);
void VMCI_GrabLock(VMCILock *lock, VMCILockFlags *flags);
void VMCI_ReleaseLock(VMCILock *lock, VMCILockFlags flags);
void VMCI_GrabLock_BH(VMCILock *lock, VMCILockFlags *flags);
void VMCI_ReleaseLock_BH(VMCILock *lock, VMCILockFlags flags);

void VMCIHost_InitContext(VMCIHost *hostContext, uintptr_t eventHnd);
void VMCIHost_ReleaseContext(VMCIHost *hostContext);
void VMCIHost_SignalCall(VMCIHost *hostContext);
void VMCIHost_ClearCall(VMCIHost *hostContext);
Bool VMCIHost_WaitForCallLocked(VMCIHost *hostContext,
                                VMCILock *lock,
                                VMCILockFlags *flags,
                                Bool useBH);
#ifdef VMKERNEL
int VMCIHost_ContextToHostVmID(VMCIHost *hostContext, VMCIHostVmID *hostVmID);
int VMCIHost_ContextHasUuid(VMCIHost *hostContext, const char *uuid);
void VMCIHost_SetActiveHnd(VMCIHost *hostContext, uintptr_t eventHnd);
Bool VMCIHost_RemoveHnd(VMCIHost *hostContext, uintptr_t eventHnd);
Bool VMCIHost_IsActiveHnd(VMCIHost *hostContext, uintptr_t eventHnd);
void VMCIHost_SetInactiveHnd(VMCIHost *hostContext, uintptr_t eventHnd);
uint32 VMCIHost_NumHnds(VMCIHost *hostContext);
uintptr_t VMCIHost_GetActiveHnd(VMCIHost *hostContext);
void VMCIHost_SignalBitmap(VMCIHost *hostContext);
void VMCIHost_SignalBitmapAlways(VMCIHost *hostContext);
void VMCIHost_SignalCallAlways(VMCIHost *hostContext);
#endif

#if defined(_WIN32)
   /*
    * On Windows, Driver Verifier will panic() if we leak memory when we are
    * unloaded.  It dumps the leaked blocks for us along with callsites, which
    * it handily tracks, but if we embed ExAllocate() inside a function, then
    * the callsite is useless.  So make this a macro on this platform only.
    */
#  define VMCI_AllocKernelMem(_sz, _f)                       \
      ExAllocatePoolWithTag((((_f) & VMCI_MEMORY_NONPAGED) ? \
                             NonPagedPool : PagedPool),      \
                            (_sz), 'MMTC')
#else // _WIN32
void *VMCI_AllocKernelMem(size_t size, int flags);
#endif // _WIN32
void VMCI_FreeKernelMem(void *ptr, size_t size);

int VMCI_CopyToUser(VA64 dst, const void *src, size_t len);
Bool VMCIWellKnownID_AllowMap(VMCIId wellKnownID,
                              VMCIPrivilegeFlags privFlags);

int VMCIHost_CompareUser(VMCIHostUser *user1, VMCIHostUser *user2);

void VMCI_CreateEvent(VMCIEvent *event);
void VMCI_DestroyEvent(VMCIEvent *event);
void VMCI_SignalEvent(VMCIEvent *event);
void VMCI_WaitOnEvent(VMCIEvent *event, VMCIEventReleaseCB releaseCB,
		      void *clientData);
#if (defined(__APPLE__) || defined(__linux__) || defined(_WIN32)) && !defined(VMKERNEL)
Bool VMCI_WaitOnEventInterruptible(VMCIEvent *event,
                                   VMCIEventReleaseCB releaseCB,
                                   void *clientData);
#endif

#if !defined(VMKERNEL) && (defined(__linux__) || defined(_WIN32) || \
                           defined(__APPLE__))
int VMCI_CopyFromUser(void *dst, VA64 src, size_t len);
#endif

typedef void (VMCIWorkFn)(void *data);
Bool VMCI_CanScheduleDelayedWork(void);
int VMCI_ScheduleDelayedWork(VMCIWorkFn *workFn, void *data);

int VMCIMutex_Init(VMCIMutex *mutex, char *name, VMCILockRank rank);
void VMCIMutex_Destroy(VMCIMutex *mutex);
void VMCIMutex_Acquire(VMCIMutex *mutex);
void VMCIMutex_Release(VMCIMutex *mutex);

#if defined(_WIN32) || defined(__APPLE__)
int VMCIKernelIf_Init(void);
void VMCIKernelIf_Exit(void);
#if defined(_WIN32)
void VMCIKernelIf_DrainDelayedWork(void);
#endif // _WIN32
#endif // _WIN32 || __APPLE__

#if !defined(VMKERNEL) && \
    (defined(__linux__) || defined(_WIN32) || defined(__APPLE__))
void *VMCI_AllocQueue(uint64 size, uint32 flags);
void VMCI_FreeQueue(void *q, uint64 size);
typedef struct PPNSet {
  uint64      numProducePages;
  uint64      numConsumePages;
  VMCIPpnList producePPNs;
  VMCIPpnList consumePPNs;
  Bool        initialized;
} PPNSet;
int VMCI_AllocPPNSet(void *produceQ, uint64 numProducePages, void *consumeQ,
                     uint64 numConsumePages, PPNSet *ppnSet);
void VMCI_FreePPNSet(PPNSet *ppnSet);
int VMCI_PopulatePPNList(uint8 *callBuf, const PPNSet *ppnSet);
#endif

struct VMCIQueue;

struct PageStoreAttachInfo;
struct VMCIQueue *VMCIHost_AllocQueue(uint64 queueSize);
void VMCIHost_FreeQueue(struct VMCIQueue *queue, uint64 queueSize);

#if defined(VMKERNEL)
typedef World_Handle *VMCIGuestMemID;
#define INVALID_VMCI_GUEST_MEM_ID  NULL
#else
typedef uint32 VMCIGuestMemID;
#define INVALID_VMCI_GUEST_MEM_ID  0
#endif

#if defined(VMKERNEL) || defined(__linux__)  || defined(_WIN32) || \
    defined(__APPLE__)
  struct QueuePairPageStore;
  int VMCIHost_RegisterUserMemory(struct QueuePairPageStore *pageStore,
                                  struct VMCIQueue *produceQ,
                                  struct VMCIQueue *consumeQ);
  void VMCIHost_UnregisterUserMemory(struct VMCIQueue *produceQ,
                                     struct VMCIQueue *consumeQ);
  int VMCIHost_MapQueues(struct VMCIQueue *produceQ,
                         struct VMCIQueue *consumeQ,
                         uint32 flags);
  int VMCIHost_UnmapQueues(VMCIGuestMemID gid,
                           struct VMCIQueue *produceQ,
                           struct VMCIQueue *consumeQ);
  void VMCI_InitQueueMutex(struct VMCIQueue *produceQ,
                           struct VMCIQueue *consumeQ);
  void VMCI_CleanupQueueMutex(struct VMCIQueue *produceQ,
                              struct VMCIQueue *consumeQ);
  int VMCI_AcquireQueueMutex(struct VMCIQueue *queue, Bool canBlock);
  void VMCI_ReleaseQueueMutex(struct VMCIQueue *queue);
#else // Below are the guest OS'es without host side support.
#  define VMCI_InitQueueMutex(_pq, _cq)
#  define VMCI_CleanupQueueMutex(_pq, _cq) do { } while (0)
#  define VMCI_AcquireQueueMutex(_q, _cb) VMCI_SUCCESS
#  define VMCI_ReleaseQueueMutex(_q) do { } while (0)
#  define VMCIHost_RegisterUserMemory(_ps, _pq, _cq) VMCI_ERROR_UNAVAILABLE
#  define VMCIHost_UnregisterUserMemory(_pq, _cq) do { } while (0)
#  define VMCIHost_MapQueues(_pq, _cq, _f) VMCI_SUCCESS
#  define VMCIHost_UnmapQueues(_gid, _pq, _cq) VMCI_SUCCESS
#endif

#if defined(VMKERNEL)
  void VMCIHost_MarkQueuesAvailable(struct VMCIQueue *produceQ,
                                    struct VMCIQueue *consumeQ);
  void VMCIHost_MarkQueuesUnavailable(struct VMCIQueue *produceQ,
                                      struct VMCIQueue *consumeQ);
  int VMCIHost_RevalidateQueues(struct VMCIQueue *produceQ,
                                struct VMCIQueue *consumeQ);
#else
#  define VMCIHost_MarkQueuesAvailable(_q, _p) do { } while (0)
#  define VMCIHost_MarkQueuesUnavailable(_q, _p) do { } while(0)
#endif

#if defined(VMKERNEL) || defined(__linux__)
   void VMCI_LockQueueHeader(struct VMCIQueue *queue);
   void VMCI_UnlockQueueHeader(struct VMCIQueue *queue);
#else
#  define VMCI_LockQueueHeader(_q) NOT_IMPLEMENTED()
#  define VMCI_UnlockQueueHeader(_q) NOT_IMPLEMENTED()
#endif

#if defined(VMKERNEL)
   int VMCI_QueueHeaderUpdated(struct VMCIQueue *produceQ);
#else
#  define VMCI_QueueHeaderUpdated(_q) VMCI_SUCCESS
#endif

#if (!defined(VMKERNEL) && defined(__linux__)) || defined(_WIN32) ||  \
    defined(__APPLE__)
  int VMCIHost_GetUserMemory(VA64 produceUVA, VA64 consumeUVA,
                             struct VMCIQueue *produceQ,
                             struct VMCIQueue *consumeQ);
  void VMCIHost_ReleaseUserMemory(struct VMCIQueue *produceQ,
                                  struct VMCIQueue *consumeQ);
#else
#  define VMCIHost_GetUserMemory(_puva, _cuva, _pq, _cq) VMCI_ERROR_UNAVAILABLE
#  define VMCIHost_ReleaseUserMemory(_pq, _cq) NOT_IMPLEMENTED()
#endif

#if defined(_WIN32)
    Bool VMCI_EnqueueToDevNull(struct VMCIQueue *queue);
    int VMCI_ConvertToLocalQueue(struct VMCIQueue *queueInfo,
                                 struct VMCIQueue *otherQueueInfo,
                                 uint64 size, Bool keepContent,
                                 void **oldQueue);
    void VMCI_RevertToNonLocalQueue(struct VMCIQueue *queueInfo,
                                    void *nonLocalQueue, uint64 size);
    void VMCI_FreeQueueBuffer(void *queue, uint64 size);
    Bool VMCI_CanCreate(void);
#else // _WIN32
#  define VMCI_EnqueueToDevNull(_q) FALSE
#  define VMCI_ConvertToLocalQueue(_pq, _cq, _s, _oq, _kc) VMCI_ERROR_UNAVAILABLE
#  define VMCI_RevertToNonLocalQueue(_q, _nlq, _s)
#  define VMCI_FreeQueueBuffer(_q, _s)
#  define VMCI_CanCreate() TRUE
#endif // !_WIN32
Bool VMCI_GuestPersonalityActive(void);
Bool VMCI_HostPersonalityActive(void);


#if defined(VMKERNEL)
  typedef List_Links VMCIListItem;
  typedef List_Links VMCIList;

#  define VMCIList_Init(_l)   List_Init(_l)
#  define VMCIList_InitEntry(_e)  List_InitElement(_e)
#  define VMCIList_Empty(_l)   List_IsEmpty(_l)
#  define VMCIList_Insert(_e, _l) List_Insert(_e, LIST_ATREAR(_l))
#  define VMCIList_Remove(_e) List_Remove(_e)
#  define VMCIList_Scan(_cur, _l) LIST_FORALL(_l, _cur)
#  define VMCIList_ScanSafe(_cur, _next, _l) LIST_FORALL_SAFE(_l, _cur, _next)
#  define VMCIList_Entry(_elem, _type, _field) List_Entry(_elem, _type, _field)
#  define VMCIList_First(_l) (VMCIList_Empty(_l)?NULL:List_First(_l))
#else
  typedef DblLnkLst_Links VMCIListItem;
  typedef DblLnkLst_Links VMCIList;

#  define VMCIList_Init(_l)   DblLnkLst_Init(_l)
#  define VMCIList_InitEntry(_e)   DblLnkLst_Init(_e)
#  define VMCIList_Empty(_l)   (!DblLnkLst_IsLinked(_l))
#  define VMCIList_Insert(_e, _l) DblLnkLst_LinkLast(_l, _e)
#  define VMCIList_Remove(_e) DblLnkLst_Unlink1(_e)
#  define VMCIList_Scan(_cur, _l) DblLnkLst_ForEach(_cur, _l)
#  define VMCIList_ScanSafe(_cur, _next, _l) DblLnkLst_ForEachSafe(_cur, _next, _l)
#  define VMCIList_Entry(_elem, _type, _field) DblLnkLst_Container(_elem, _type, _field)
#  define VMCIList_First(_l) (VMCIList_Empty(_l)?NULL:(_l)->next)
#endif


#if defined __cplusplus
} // extern "C"
#endif

#endif // _VMCI_KERNEL_IF_H_
