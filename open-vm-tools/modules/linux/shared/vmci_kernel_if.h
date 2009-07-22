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
 * vmci_kernel_if.h -- 
 * 
 *      This file defines helper functions for VMCI host _and_ guest
 *      kernel code. It must work for Windows, Mac OS, vmkernel, Linux and
 *      Solaris kernels, i.e. using defines where necessary.
 */
 
#ifndef _VMCI_KERNEL_IF_H_
#define _VMCI_KERNEL_IF_H_

#if !defined(linux) && !defined(_WIN32) && !defined(__APPLE__) && \
    !defined(VMKERNEL) && !defined(SOLARIS)
#error "Platform not supported."
#endif

#if defined(_WIN32)
#include <ntddk.h>
#endif 

#if defined(linux) && !defined(VMKERNEL)
#  include "compat_version.h"
#  include "compat_wait.h"
#  include "compat_spinlock.h"
#  include "compat_semaphore.h"
#endif // linux

#ifdef __APPLE__
#  include <IOKit/IOLib.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#endif

#ifdef VMKERNEL
#include "splock.h"
#include "semaphore_ext.h"
#endif

#ifdef SOLARIS
#  include <sys/mutex.h>
#  include <sys/poll.h>
#  include <sys/semaphore.h>
#  include <sys/kmem.h>
#endif

#include "vm_basic_types.h"
#include "vmci_defs.h"

#if defined(__APPLE__)
#  include "dbllnklst.h"
#endif

/* Flags for specifying memory type. */
#define VMCI_MEMORY_NORMAL   0x0
#define VMCI_MEMORY_ATOMIC   0x1
#define VMCI_MEMORY_NONPAGED 0x2

/* Platform specific type definitions. */

#if defined(VMKERNEL)
  typedef SP_SpinLock VMCILock;
  typedef SP_IRQL VMCILockFlags;
  typedef Semaphore VMCIEvent;
  typedef Semaphore VMCIMutex;
  typedef World_ID VMCIHostVmID;
#elif defined(linux)
  typedef spinlock_t VMCILock;
  typedef unsigned long VMCILockFlags;
  typedef wait_queue_head_t VMCIEvent;
  typedef struct semaphore VMCIMutex;
  typedef PPN *VMCIPpnList; /* List of PPNs in produce/consume queue. */
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
#elif defined(_WIN32)
  typedef KSPIN_LOCK VMCILock;
  typedef KIRQL VMCILockFlags;
  typedef KEVENT VMCIEvent;
  typedef FAST_MUTEX VMCIMutex;
  typedef PMDL VMCIPpnList; /* MDL to map the produce/consume queue. */
#elif defined(SOLARIS)
  typedef kmutex_t VMCILock;
  typedef unsigned long VMCILockFlags;
  typedef ksema_t VMCIEvent;
  typedef kmutex_t VMCIMutex;
  typedef PPN *VMCIPpnList; /* List of PPNs in produce/consume queue. */
#endif // VMKERNEL

/* Callback needed for correctly waiting on events. */
typedef int (*VMCIEventReleaseCB)(void *clientData);

/*
 * The VMCI locks use a ranking scheme similar to the one used by
 * vmkernel. While holding a lock L1 with rank R1, only locks with
 * rank higher than R1 may be grabbed. The available ranks for VMCI 
 * locks are (in descending order):
 * - VMCI_LOCK_RANK_HIGH_BH : to be used for locks grabbed while executing
 *   in a bottom half and not held while grabbing other locks.
 * - VMCI_LOCK_RANK_MIDDLE_BH : to be for locks grabbed while executing in a
 *   bottom half and held while grabbing locks of rank VMCI_LOCK_RANK_HIGH_BH.
 * - VMCI_LOCK_RANK_LOW_BH : to be for locks grabbed while executing in a
 *   bottom half and held while grabbing locks of rank
 *   VMCI_LOCK_RANK_MIDDLE_BH.
 * - VMCI_LOCK_RANK_HIGHEST : to be used for locks that are not held while
 *   grabbing other locks except system locks with higher ranks and bottom
 *   half locks.
 * - VMCI_LOCK_RANK_HIGHER : to be used for locks that are held while
 *   grabbing locks of rank VMCI_LOCK_RANK_HIGHEST or higher.
 * - VMCI_LOCK_RANK_HIGH : to be used for locks that are held while
 *   grabbing locks of rank VMCI_LOCK_RANK_HIGHER or higher. This is
 *   the highest lock rank used by core VMCI services
 * - VMCI_LOCK_RANK_MIDDLE : to be used for locks that are held while
 *   grabbing locks of rank VMCI_LOCK_RANK_HIGH or higher.
 * - VMCI_LOCK_RANK_LOW : to be used for locks that are held while
 *   grabbing locks of rank VMCI_LOCK_RANK_MIDDLE or higher.
 * - VMCI_LOCK_RANK_LOWEST : to be used for locks that are held while
 *   grabbing locks of rank VMCI_LOCK_RANK_LOW or higher.
 */
#ifdef VMKERNEL
  typedef SP_Rank VMCILockRank;

  #define VMCI_LOCK_RANK_HIGH_BH        SP_RANK_IRQ_LEAF
  #define VMCI_LOCK_RANK_MIDDLE_BH      (SP_RANK_IRQ_LEAF-1)
  #define VMCI_LOCK_RANK_LOW_BH         SP_RANK_IRQ_LOWEST
  #define VMCI_LOCK_RANK_HIGHEST        SP_RANK_SHM_MGR-1
#else
  typedef unsigned long VMCILockRank;

  #define VMCI_LOCK_RANK_HIGH_BH        0x4000
  #define VMCI_LOCK_RANK_MIDDLE_BH      0x2000
  #define VMCI_LOCK_RANK_LOW_BH         0x1000
  #define VMCI_LOCK_RANK_HIGHEST        0x0fff
#endif // VMKERNEL
#define VMCI_LOCK_RANK_HIGHER      (VMCI_LOCK_RANK_HIGHEST-1)
#define VMCI_LOCK_RANK_HIGH        (VMCI_LOCK_RANK_HIGHER-1)
#define VMCI_LOCK_RANK_MIDDLE_HIGH (VMCI_LOCK_RANK_HIGH-1)
#define VMCI_LOCK_RANK_MIDDLE      (VMCI_LOCK_RANK_MIDDLE_HIGH-1)
#define VMCI_LOCK_RANK_MIDDLE_LOW  (VMCI_LOCK_RANK_MIDDLE-1)
#define VMCI_LOCK_RANK_LOW         (VMCI_LOCK_RANK_MIDDLE_LOW-1)
#define VMCI_LOCK_RANK_LOWEST      (VMCI_LOCK_RANK_LOW-1)


/*
 * In vmkernel, we try to reduce the amount of memory mapped into the
 * virtual address space by only mapping the memory of buffered
 * datagrams when copying from and to the guest. In other OSes,
 * regular kernel memory is used. VMCIBuffer is used to reference
 * possibly unmapped memory.
 */

#ifdef VMKERNEL
typedef MPN VMCIBuffer;
#define VMCI_BUFFER_INVALID INVALID_MPN
#else
typedef void * VMCIBuffer;
#define VMCI_BUFFER_INVALID NULL
#endif

/*
 * Host specific struct used for signalling.
 */

typedef struct VMCIHost {
#if defined(VMKERNEL)
   World_ID vmmWorldID;
#elif defined(linux)
   wait_queue_head_t  waitQueue;
#elif defined(__APPLE__)
   struct Socket *socket; /* vmci Socket object on Mac OS. */
#elif defined(_WIN32)
   KEVENT *callEvent; /* Ptr to userlevel event used when signalling 
                       * new pending guestcalls in kernel.
                       */
#elif defined(SOLARIS)
   struct pollhead pollhead; /* Per datagram handle pollhead structure to
                              * be treated as a black-box. None of its 
                              * fields should be referenced.
                              */
#endif
} VMCIHost;


void VMCI_InitLock(VMCILock *lock, char *name, VMCILockRank rank);
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
#endif

void *VMCI_AllocKernelMem(size_t size, int flags);
void VMCI_FreeKernelMem(void *ptr, size_t size);
VMCIBuffer VMCI_AllocBuffer(size_t size, int flags);
void *VMCI_MapBuffer(VMCIBuffer buf);
void VMCI_ReleaseBuffer(void *ptr);
void VMCI_FreeBuffer(VMCIBuffer buf, size_t size);
#ifdef SOLARIS
int VMCI_CopyToUser(VA64 dst, const void *src, size_t len, int mode);
#else
int VMCI_CopyToUser(VA64 dst, const void *src, size_t len);
/*
 * Don't need the following for guests, hence no Solaris code for this
 * function.
 */
Bool VMCIWellKnownID_AllowMap(VMCIId wellKnownID,
                              VMCIPrivilegeFlags privFlags);
#endif

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
                           defined(__APPLE__) || defined(SOLARIS))
int VMCI_CopyFromUser(void *dst, VA64 src, size_t len);
#endif

int VMCIMutex_Init(VMCIMutex *mutex);
void VMCIMutex_Destroy(VMCIMutex *mutex);
void VMCIMutex_Acquire(VMCIMutex *mutex);
void VMCIMutex_Release(VMCIMutex *mutex);

#if defined(SOLARIS)
int VMCIKernelIf_Init(void);
void VMCIKernelIf_Exit(void);
#endif		/* SOLARIS  */

#if !defined(VMKERNEL) && (defined(__linux__) || defined(_WIN32) || \
                           defined(SOLARIS) || defined(__APPLE__))
void *VMCI_AllocQueue(uint64 size);
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

#if !defined(VMX86_TOOLS) && !defined(VMKERNEL)
struct PageStoreAttachInfo;
struct VMCIQueue;

int VMCIHost_GetUserMemory(struct PageStoreAttachInfo *attach,
                           struct VMCIQueue *produceQ,
                           struct VMCIQueue *detachQ);
void VMCIHost_ReleaseUserMemory(struct PageStoreAttachInfo *attach,
                                struct VMCIQueue *produceQ,
                                struct VMCIQueue *detachQ);

#ifdef _WIN32
/*
 * Special routine used on the Windows platform to save a queue when
 * its backing memory goes away.
 */

void VMCIHost_SaveProduceQ(struct PageStoreAttachInfo *attach,
                           struct VMCIQueue *produceQ,
                           struct VMCIQueue *detachQ,
                           const uint64 produceQSize);
#endif // _WIN32
#endif // !VMX86_TOOLS && !VMKERNEL


#endif // _VMCI_KERNEL_IF_H_
