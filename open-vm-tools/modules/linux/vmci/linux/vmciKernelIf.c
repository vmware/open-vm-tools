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
 * vmciKernelIf.c --
 *
 *      This file implements defines and helper functions for VMCI
 *      host _and_ guest kernel code. This is the linux specific
 *      implementation.
 */

/* Must come before any kernel header file */
#include "driver-config.h"

#if !defined(linux) || defined(VMKERNEL)
#error "Wrong platform."
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
#  error "Linux kernels before 2.6.9 are not supported."
#endif

#include <linux/mm.h>           /* For vmalloc_to_page() and get_user_pages()*/
#include <linux/pagemap.h>      /* For page_cache_release() */
#include <linux/socket.h>       /* For memcpy_{to,from}iovec(). */
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "compat_highmem.h"
#include "compat_interrupt.h"
#include "compat_mm.h"
#include "compat_module.h"
#include "compat_page.h"
#include "compat_pci.h"
#include "compat_sched.h"
#include "compat_semaphore.h"
#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_version.h"
#include "compat_workqueue.h"

#include "vm_assert.h"
#include "vm_basic_types.h"
#include "vmci_iocontrols.h"
#include "vmci_kernel_if.h"
#include "vmciQueue.h"
#include "vmciQueuePair.h"


/*
 * The Kernel specific component of the VMCIQueue structure.
 */

struct VMCIQueueKernelIf {
   VMCIMutex __mutex;               /* Protects the queue. */
   VMCIMutex *mutex;                /* Shared by producer/consumer queues. */
   size_t numPages;                 /* Number of pages incl. header. */
   Bool host;                       /* Host or guest? */
   union {
      struct {
         dma_addr_t *pas;           /* Physical addresses. */
         void **vas;                /* Virtual addresses. */
      } g;                          /* Guest. */
      struct {
         struct page **headerPage;  /* Guest queue header pages. */
         struct page **page;        /* Guest queue pages. */
      } h;                          /* Host. */
   } u;
};

typedef struct VMCIDelayedWorkInfo {
   compat_work work;
   VMCIWorkFn *workFn;
   void *data;
} VMCIDelayedWorkInfo;

extern struct pci_dev *vmci_pdev;


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_InitLock
 *
 *      Initializes the lock. Must be called before use.
 *
 * Results:
 *      Always VMCI_SUCCESS.
 *
 * Side effects:
 *      Thread can block.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCI_InitLock(VMCILock *lock,    // IN:
              char *name,        // IN: Unused on Linux
              VMCILockRank rank) // IN: Unused on Linux
{
   spin_lock_init(lock);
   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_CleanupLock
 *
 *      Cleanup the lock. Must be called before deallocating lock.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Deletes kernel lock state
 *
 *-----------------------------------------------------------------------------
 */

void
VMCI_CleanupLock(VMCILock *lock)
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_GrabLock
 *
 *      Grabs the given lock. XXX Fill in specific lock requirements. XXX Move
 *      locking code into hostif if VMCI stays in vmmon.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Thread can block.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCI_GrabLock(VMCILock *lock,       // IN
              VMCILockFlags *flags) // OUT: used to restore irql on windows
{
   spin_lock(lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_ReleaseLock
 *
 *      Releases the given lock. XXX Move locking code into hostif if VMCI
 *      stays in vmmon.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      A thread blocked on this lock may wake up.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCI_ReleaseLock(VMCILock *lock,      // IN
                 VMCILockFlags flags) // IN
{
   spin_unlock(lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_GrabLock_BH
 *
 *      Grabs the given lock and for linux kernels disables bottom half execution.
 *      This should be used with locks accessed both from bottom half/tasklet
 *      contexts, ie. guestcall handlers, and from process contexts to avoid
 *      deadlocks where the process has the lock and gets descheduled due to a
 *      bh/tasklet coming in.
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
VMCI_GrabLock_BH(VMCILock *lock,        // IN
                 VMCILockFlags *flags)  // OUT: used to restore
{
   spin_lock_bh(lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_ReleaseLock_BH
 *
 *      Releases the given lock and for linux kernels reenables bottom half
 *      execution.
 *      This should be used with locks accessed both from bottom half/tasklet
 *      contexts, ie. guestcall handlers, and from process contexts to avoid
 *      deadlocks where the process has the lock and get descheduled due to a
 *      bh/tasklet coming in.
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
VMCI_ReleaseLock_BH(VMCILock *lock,        // IN
                    VMCILockFlags flags)   // IN
{
   spin_unlock_bh(lock);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIHost_InitContext --
 *
 *      Host-specific initialization of VMCI context state.
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
VMCIHost_InitContext(VMCIHost *hostContext, // IN
                     uintptr_t eventHnd)    // IN: Unused
{
   init_waitqueue_head(&hostContext->waitQueue);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIHost_ReleaseContext --
 *
 *      Host-specific release of state allocated by
 *      VMCIHost_InitContext.
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
VMCIHost_ReleaseContext(VMCIHost *hostContext) // IN
{
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIHost_SignalCall --
 *
 *      Signal to userlevel that a VMCI call is waiting.
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
VMCIHost_SignalCall(VMCIHost *hostContext)     // IN
{
   wake_up(&hostContext->waitQueue);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIHost_WaitForCallLocked --
 *
 *      Wait until a VMCI call is pending or the waiting thread is
 *      interrupted. It is assumed that a lock is held prior to
 *      calling this function. The lock will be released during the
 *      wait. The correctnes of this funtion depends on that the same
 *      lock is held when the call is signalled.
 *
 * Results:
 *      TRUE on success
 *      FALSE if the wait was interrupted.
 *
 * Side effects:
 *      The call may block.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCIHost_WaitForCallLocked(VMCIHost *hostContext, // IN
                           VMCILock *lock,        // IN
                           VMCILockFlags *flags,  // IN
                           Bool useBH)            // IN

{
   DECLARE_WAITQUEUE(wait, current);

   /*
    * The thread must be added to the wait queue and have its state
    * changed while holding the lock - otherwise a signal may change
    * the state in between and have it overwritten causing a loss of
    * the event.
    */

   add_wait_queue(&hostContext->waitQueue, &wait);
   current->state = TASK_INTERRUPTIBLE;

   if (useBH) {
      VMCI_ReleaseLock_BH(lock, *flags);
   } else {
      VMCI_ReleaseLock(lock, *flags);
   }

   schedule();

   if (useBH) {
      VMCI_GrabLock_BH(lock, flags);
   } else {
      VMCI_GrabLock(lock, flags);
   }

   current->state = TASK_RUNNING;

   remove_wait_queue(&hostContext->waitQueue, &wait);

   if (signal_pending(current)) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIHost_ClearCall --
 *
 *      Clear the pending call signal.
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
VMCIHost_ClearCall(VMCIHost *hostContext)     // IN
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_CompareUser --
 *
 *      Determines whether the two users are the same.
 *
 * Results:
 *      VMCI_SUCCESS if equal, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int VMCIHost_CompareUser(VMCIHostUser *user1,
                         VMCIHostUser *user2)
{
   if (!user1 || !user2) {
      return VMCI_ERROR_INVALID_ARGS;
   }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
#  define vmw_uid_eq(a, b) uid_eq(a, b)
#else
#  define vmw_uid_eq(a, b) ((a) == (b))
#endif

   return vmw_uid_eq(*user1, *user2) ? VMCI_SUCCESS : VMCI_ERROR_GENERIC;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_AllocKernelMem
 *
 *      Allocate some kernel memory for the VMCI driver.
 *
 * Results:
 *      The address allocated or NULL on error.
 *
 *
 * Side effects:
 *      memory is malloced
 *----------------------------------------------------------------------
 */

void *
VMCI_AllocKernelMem(size_t size, int flags)
{
   void *ptr;

   if ((flags & VMCI_MEMORY_ATOMIC) != 0) {
      ptr = kmalloc(size, GFP_ATOMIC);
   } else {
      ptr = kmalloc(size, GFP_KERNEL);
   }

   return ptr;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_FreeKernelMem
 *
 *      Free kernel memory allocated for the VMCI driver.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      memory is freed.
 *----------------------------------------------------------------------
 */

void
VMCI_FreeKernelMem(void *ptr,   // IN:
                   size_t size) // IN: Unused on Linux
{
   kfree(ptr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_CopyToUser --
 *
 *      Copy memory to the user application from a kernel buffer. This
 *      function may block, so don't call it while holding any kind of
 *      lock.
 *
 * Results:
 *      0 on success.
 *      Nonzero on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
VMCI_CopyToUser(VA64 dst,        // OUT: Destination user VA.
                const void *src, // IN: Source kernel VA.
                size_t len)      // IN: Number of bytes to copy.
{
   return copy_to_user(VMCIVA64ToPtr(dst), src, len) ? -EFAULT : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_CopyFromUser --
 *
 *      Copy memory from the user application to a kernel buffer. This
 *      function may block, so don't call it while holding any kind of
 *      lock.
 *
 * Results:
 *      0 on success.
 *      Nonzero on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCI_CopyFromUser(void *dst,  // OUT: Kernel VA
                  VA64 src,   // IN: User VA
                  size_t len) // IN
{
   return copy_from_user(dst, VMCIVA64ToPtr(src), len);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCIDelayedWorkCB
 *
 *      Called in a worker thread context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
VMCIDelayedWorkCB(compat_work_arg work) // IN
{
   VMCIDelayedWorkInfo *delayedWorkInfo;

   delayedWorkInfo = COMPAT_WORK_GET_DATA(work, VMCIDelayedWorkInfo, work);
   ASSERT(delayedWorkInfo);
   ASSERT(delayedWorkInfo->workFn);

   delayedWorkInfo->workFn(delayedWorkInfo->data);

   VMCI_FreeKernelMem(delayedWorkInfo, sizeof *delayedWorkInfo);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCI_CanScheduleDelayedWork --
 *
 *      Checks to see if the given platform supports delayed work callbacks.
 *
 * Results:
 *      TRUE if it does. FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
VMCI_CanScheduleDelayedWork(void)
{
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCI_ScheduleDelayedWork --
 *
 *      Schedule the specified callback.
 *
 * Results:
 *      Zero on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VMCI_ScheduleDelayedWork(VMCIWorkFn  *workFn,   // IN
                         void *data)            // IN
{
   VMCIDelayedWorkInfo *delayedWorkInfo;

   ASSERT(workFn);

   delayedWorkInfo = VMCI_AllocKernelMem(sizeof *delayedWorkInfo,
                                         VMCI_MEMORY_ATOMIC);
   if (!delayedWorkInfo) {
      return VMCI_ERROR_NO_MEM;
   }

   delayedWorkInfo->workFn = workFn;
   delayedWorkInfo->data = data;

   COMPAT_INIT_WORK(&delayedWorkInfo->work, VMCIDelayedWorkCB,
                    delayedWorkInfo);

   compat_schedule_work(&delayedWorkInfo->work);

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_CreateEvent --
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
VMCI_CreateEvent(VMCIEvent *event)  // IN:
{
   init_waitqueue_head(event);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_DestroyEvent --
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
VMCI_DestroyEvent(VMCIEvent *event)  // IN:
{
   /* Nothing to do. */
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_SignalEvent --
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
VMCI_SignalEvent(VMCIEvent *event)  // IN:
{
   wake_up(event);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_WaitOnEvent --
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
VMCI_WaitOnEvent(VMCIEvent *event,              // IN:
                 VMCIEventReleaseCB releaseCB,  // IN:
                 void *clientData)              // IN:
{
   /*
    * XXX Should this be a TASK_UNINTERRUPTIBLE wait? I'm leaving it
    * as it was for now.
    */
   VMCI_WaitOnEventInterruptible(event, releaseCB, clientData);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_WaitOnEventInterruptible --
 *
 * Results:
 *      True if the wait was interrupted by a signal, false otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCI_WaitOnEventInterruptible(VMCIEvent *event,              // IN:
                              VMCIEventReleaseCB releaseCB,  // IN:
                              void *clientData)              // IN:
{
   DECLARE_WAITQUEUE(wait, current);

   if (event == NULL || releaseCB == NULL) {
      return FALSE;
   }

   add_wait_queue(event, &wait);
   current->state = TASK_INTERRUPTIBLE;

   /*
    * Release the lock or other primitive that makes it possible for us to
    * put the current thread on the wait queue without missing the signal.
    * Ie. on Linux we need to put ourselves on the wait queue and set our
    * stateto TASK_INTERRUPTIBLE without another thread signalling us.
    * The releaseCB is used to synchronize this.
    */
   releaseCB(clientData);

   schedule();
   current->state = TASK_RUNNING;
   remove_wait_queue(event, &wait);

   return signal_pending(current);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMutex_Init --
 *
 *      Initializes the mutex. Must be called before use.
 *
 * Results:
 *      Success.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIMutex_Init(VMCIMutex *mutex,  // IN
               char *name,        // IN: Unused
               VMCILockRank rank) // IN: Unused
{
   sema_init(mutex, 1);
   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMutex_Destroy --
 *
 *      Destroys the mutex.  Does nothing on Linux.
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
VMCIMutex_Destroy(VMCIMutex *mutex) // IN: Unused
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMutex_Acquire --
 *
 *      Acquires the mutex.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Thread may block.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIMutex_Acquire(VMCIMutex *mutex) // IN:
{
   down(mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMutex_Release --
 *
 *      Releases the mutex.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May wake up the thread blocking on this mutex.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIMutex_Release(VMCIMutex *mutex) // IN:
{
   up(mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_AllocQueue --
 *
 *      Allocates kernel queue pages of specified size with IOMMU mappings,
 *      plus space for the queue structure/kernel interface and the queue
 *      header.
 *
 * Results:
 *      Pointer to the queue on success, NULL otherwise.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

void *
VMCI_AllocQueue(uint64 size,  // IN: size of queue (not including header)
                uint32 flags) // IN: queuepair flags
{
   size_t i;
   VMCIQueue *queue;
   const size_t numPages = CEILING(size, PAGE_SIZE) + 1;
   const size_t pasSize = numPages * sizeof *queue->kernelIf->u.g.pas;
   const size_t vasSize = numPages * sizeof *queue->kernelIf->u.g.vas;
   const size_t queueSize =
      sizeof *queue + sizeof *(queue->kernelIf) + pasSize + vasSize;

   /*
    * Size should be enforced by VMCIQPair_Alloc(), double-check here.
    * Allocating too much on Linux can cause the system to become
    * unresponsive, because we allocate page-by-page, and we allow the
    * system to wait for pages rather than fail.
    */

   if (size > VMCI_MAX_GUEST_QP_MEMORY) {
      ASSERT(FALSE);
      return NULL;
   }

   queue = vmalloc(queueSize);
   if (!queue) {
      return NULL;
   }

   queue->qHeader = NULL;
   queue->savedHeader = NULL;
   queue->kernelIf = (VMCIQueueKernelIf *)(queue + 1);
   queue->kernelIf->mutex = NULL;
   queue->kernelIf->numPages = numPages;
   queue->kernelIf->u.g.pas = (dma_addr_t *)(queue->kernelIf + 1);
   queue->kernelIf->u.g.vas =
      (void **)((uint8 *)queue->kernelIf->u.g.pas + pasSize);
   queue->kernelIf->host = FALSE;

   for (i = 0; i < numPages; i++) {
      queue->kernelIf->u.g.vas[i] =
         dma_alloc_coherent(&vmci_pdev->dev, PAGE_SIZE,
                            &queue->kernelIf->u.g.pas[i], GFP_KERNEL);
      if (!queue->kernelIf->u.g.vas[i]) {
         VMCI_FreeQueue(queue, i * PAGE_SIZE); /* Size excl. the header. */
         return NULL;
      }
   }

   /* Queue header is the first page. */
   queue->qHeader = queue->kernelIf->u.g.vas[0];

   return (void *)queue;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_FreeQueue --
 *
 *      Frees kernel VA space for a given queue and its queue header, and
 *      frees physical data pages.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Memory is freed.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCI_FreeQueue(void *q,     // IN:
               uint64 size) // IN: size of queue (not including header)
{
   VMCIQueue *queue = q;

   if (queue) {
      uint64 i;

      /* Given size does not include header, so add in a page here. */
      for (i = 0; i < CEILING(size, PAGE_SIZE) + 1; i++) {
         dma_free_coherent(&vmci_pdev->dev, PAGE_SIZE,
                           queue->kernelIf->u.g.vas[i],
                           queue->kernelIf->u.g.pas[i]);
      }
      vfree(queue);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_AllocPPNSet --
 *
 *      Allocates two list of PPNs --- one for the pages in the produce queue,
 *      and the other for the pages in the consume queue. Intializes the list
 *      of PPNs with the page frame numbers of the KVA for the two queues (and
 *      the queue headers).
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      Memory may be allocated.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCI_AllocPPNSet(void *prodQ,            // IN:
                 uint64 numProducePages, // IN: for queue plus header
                 void *consQ,            // IN:
                 uint64 numConsumePages, // IN: for queue plus header
                 PPNSet *ppnSet)         // OUT:
{
   VMCIPpnList producePPNs;
   VMCIPpnList consumePPNs;
   VMCIQueue *produceQ = prodQ;
   VMCIQueue *consumeQ = consQ;
   uint64 i;

   if (!produceQ || !numProducePages || !consumeQ || !numConsumePages ||
       !ppnSet) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (ppnSet->initialized) {
      return VMCI_ERROR_ALREADY_EXISTS;
   }

   producePPNs =
      VMCI_AllocKernelMem(numProducePages * sizeof *producePPNs,
                          VMCI_MEMORY_NORMAL);
   if (!producePPNs) {
      return VMCI_ERROR_NO_MEM;
   }

   consumePPNs =
      VMCI_AllocKernelMem(numConsumePages * sizeof *consumePPNs,
                          VMCI_MEMORY_NORMAL);
   if (!consumePPNs) {
      VMCI_FreeKernelMem(producePPNs, numProducePages * sizeof *producePPNs);
      return VMCI_ERROR_NO_MEM;
   }

   for (i = 0; i < numProducePages; i++) {
      unsigned long pfn;

      producePPNs[i] = pfn = produceQ->kernelIf->u.g.pas[i] >> PAGE_SHIFT;

      /*
       * Fail allocation if PFN isn't supported by hypervisor.
       */

      if (sizeof pfn > sizeof *producePPNs && pfn != producePPNs[i]) {
         goto ppnError;
      }
   }
   for (i = 0; i < numConsumePages; i++) {
      unsigned long pfn;

      consumePPNs[i] = pfn = consumeQ->kernelIf->u.g.pas[i] >> PAGE_SHIFT;

      /*
       * Fail allocation if PFN isn't supported by hypervisor.
       */

      if (sizeof pfn > sizeof *consumePPNs && pfn != consumePPNs[i]) {
         goto ppnError;
      }
   }

   ppnSet->numProducePages = numProducePages;
   ppnSet->numConsumePages = numConsumePages;
   ppnSet->producePPNs = producePPNs;
   ppnSet->consumePPNs = consumePPNs;
   ppnSet->initialized = TRUE;
   return VMCI_SUCCESS;

ppnError:
   VMCI_FreeKernelMem(producePPNs, numProducePages * sizeof *producePPNs);
   VMCI_FreeKernelMem(consumePPNs, numConsumePages * sizeof *consumePPNs);
   return VMCI_ERROR_INVALID_ARGS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_FreePPNSet --
 *
 *      Frees the two list of PPNs for a queue pair.
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
VMCI_FreePPNSet(PPNSet *ppnSet) // IN:
{
   ASSERT(ppnSet);
   if (ppnSet->initialized) {
      /* Do not call these functions on NULL inputs. */
      ASSERT(ppnSet->producePPNs && ppnSet->consumePPNs);
      VMCI_FreeKernelMem(ppnSet->producePPNs,
                         ppnSet->numProducePages * sizeof *ppnSet->producePPNs);
      VMCI_FreeKernelMem(ppnSet->consumePPNs,
                         ppnSet->numConsumePages * sizeof *ppnSet->consumePPNs);
   }
   memset(ppnSet, 0, sizeof *ppnSet);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_PopulatePPNList --
 *
 *      Populates the list of PPNs in the hypercall structure with the PPNS
 *      of the produce queue and the consume queue.
 *
 * Results:
 *      VMCI_SUCCESS.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCI_PopulatePPNList(uint8 *callBuf,       // OUT:
                     const PPNSet *ppnSet) // IN:
{
   ASSERT(callBuf && ppnSet && ppnSet->initialized);
   memcpy(callBuf, ppnSet->producePPNs,
          ppnSet->numProducePages * sizeof *ppnSet->producePPNs);
   memcpy(callBuf + ppnSet->numProducePages * sizeof *ppnSet->producePPNs,
          ppnSet->consumePPNs,
          ppnSet->numConsumePages * sizeof *ppnSet->consumePPNs);

   return VMCI_SUCCESS;
}


#ifdef __KERNEL__


/*
 *-----------------------------------------------------------------------------
 *
 * __VMCIMemcpyToQueue --
 *
 *      Copies from a given buffer or iovector to a VMCI Queue.  Uses
 *      kmap()/kunmap() to dynamically map/unmap required portions of the queue
 *      by traversing the offset -> page translation structure for the queue.
 *      Assumes that offset + size does not wrap around in the queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
__VMCIMemcpyToQueue(VMCIQueue *queue,   // OUT:
                    uint64 queueOffset, // IN:
                    const void *src,    // IN:
                    size_t size,        // IN:
                    Bool isIovec)       // IN: if src is a struct iovec *
{
   VMCIQueueKernelIf *kernelIf = queue->kernelIf;
   size_t bytesCopied = 0;

   while (bytesCopied < size) {
      const uint64 pageIndex = (queueOffset + bytesCopied) / PAGE_SIZE;
      const size_t pageOffset = (queueOffset + bytesCopied) & (PAGE_SIZE - 1);
      void *va;
      size_t toCopy;

      if (kernelIf->host) {
         va = kmap(kernelIf->u.h.page[pageIndex]);
      } else {
         va = kernelIf->u.g.vas[pageIndex + 1]; /* Skip header. */
      }

      ASSERT(va);
      if (size - bytesCopied > PAGE_SIZE - pageOffset) {
         /* Enough payload to fill up from this page. */
         toCopy = PAGE_SIZE - pageOffset;
      } else {
         toCopy = size - bytesCopied;
      }

      if (isIovec) {
         struct iovec *iov = (struct iovec *)src;
         int err;

         /* The iovec will track bytesCopied internally. */
         err = memcpy_fromiovec((uint8 *)va + pageOffset, iov, toCopy);
         if (err != 0) {
            if (kernelIf->host) {
               kunmap(kernelIf->u.h.page[pageIndex]);
            }
            return VMCI_ERROR_INVALID_ARGS;
         }
      } else {
         memcpy((uint8 *)va + pageOffset, (uint8 *)src + bytesCopied, toCopy);
      }

      bytesCopied += toCopy;
      if (kernelIf->host) {
         kunmap(kernelIf->u.h.page[pageIndex]);
      }
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * __VMCIMemcpyFromQueue --
 *
 *      Copies to a given buffer or iovector from a VMCI Queue.  Uses
 *      kmap()/kunmap() to dynamically map/unmap required portions of the queue
 *      by traversing the offset -> page translation structure for the queue.
 *      Assumes that offset + size does not wrap around in the queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
__VMCIMemcpyFromQueue(void *dest,             // OUT:
                      const VMCIQueue *queue, // IN:
                      uint64 queueOffset,     // IN:
                      size_t size,            // IN:
                      Bool isIovec)           // IN: if dest is a struct iovec *
{
   VMCIQueueKernelIf *kernelIf = queue->kernelIf;
   size_t bytesCopied = 0;

   while (bytesCopied < size) {
      const uint64 pageIndex = (queueOffset + bytesCopied) / PAGE_SIZE;
      const size_t pageOffset = (queueOffset + bytesCopied) & (PAGE_SIZE - 1);
      void *va;
      size_t toCopy;

      if (kernelIf->host) {
         va = kmap(kernelIf->u.h.page[pageIndex]);
      } else {
         va = kernelIf->u.g.vas[pageIndex + 1]; /* Skip header. */
      }

      ASSERT(va);
      if (size - bytesCopied > PAGE_SIZE - pageOffset) {
         /* Enough payload to fill up this page. */
         toCopy = PAGE_SIZE - pageOffset;
      } else {
         toCopy = size - bytesCopied;
      }

      if (isIovec) {
         struct iovec *iov = (struct iovec *)dest;
         int err;

         /* The iovec will track bytesCopied internally. */
         err = memcpy_toiovec(iov, (uint8 *)va + pageOffset, toCopy);
         if (err != 0) {
            if (kernelIf->host) {
               kunmap(kernelIf->u.h.page[pageIndex]);
            }
            return VMCI_ERROR_INVALID_ARGS;
         }
      } else {
         memcpy((uint8 *)dest + bytesCopied, (uint8 *)va + pageOffset, toCopy);
      }

      bytesCopied += toCopy;
      if (kernelIf->host) {
         kunmap(kernelIf->u.h.page[pageIndex]);
      }
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpyToQueue --
 *
 *      Copies from a given buffer to a VMCI Queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIMemcpyToQueue(VMCIQueue *queue,   // OUT:
                  uint64 queueOffset, // IN:
                  const void *src,    // IN:
                  size_t srcOffset,   // IN:
                  size_t size,        // IN:
                  int bufType,        // IN: Unused
                  Bool canBlock)      // IN: Unused
{
   ASSERT(canBlock || !queue->kernelIf->host);

   return __VMCIMemcpyToQueue(queue, queueOffset,
                              (uint8 *)src + srcOffset, size, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpyFromQueue --
 *
 *      Copies to a given buffer from a VMCI Queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIMemcpyFromQueue(void *dest,             // OUT:
                    size_t destOffset,      // IN:
                    const VMCIQueue *queue, // IN:
                    uint64 queueOffset,     // IN:
                    size_t size,            // IN:
                    int bufType,            // IN: Unused
                    Bool canBlock)          // IN: Unused
{
   ASSERT(canBlock || !queue->kernelIf->host);

   return __VMCIMemcpyFromQueue((uint8 *)dest + destOffset,
                                queue, queueOffset, size, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpyToQueueLocal --
 *
 *      Copies from a given buffer to a local VMCI queue. On Linux, this is the
 *      same as a regular copy.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIMemcpyToQueueLocal(VMCIQueue *queue,     // OUT
                       uint64 queueOffset,   // IN
                       const void *src,      // IN
                       size_t srcOffset,     // IN
                       size_t size,          // IN
                       int bufType,          // IN: Unused
                       Bool canBlock)        // IN: Unused
{
   ASSERT(canBlock || !queue->kernelIf->host);

   return __VMCIMemcpyToQueue(queue, queueOffset,
                              (uint8 *)src + srcOffset, size, FALSE);;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpyFromQueueLocal --
 *
 *      Copies to a given buffer from a VMCI Queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIMemcpyFromQueueLocal(void *dest,             // OUT:
                         size_t destOffset,      // IN:
                         const VMCIQueue *queue, // IN:
                         uint64 queueOffset,     // IN:
                         size_t size,            // IN:
                         int bufType,            // IN: Unused
                         Bool canBlock)          // IN: Unused
{
   ASSERT(canBlock || !queue->kernelIf->host);

   return __VMCIMemcpyFromQueue((uint8 *)dest + destOffset,
                                queue, queueOffset, size, FALSE);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCIMemcpyToQueueV --
 *
 *      Copies from a given iovec from a VMCI Queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VMCIMemcpyToQueueV(VMCIQueue *queue,      // OUT:
                   uint64 queueOffset,    // IN:
                   const void *src,       // IN: iovec
                   size_t srcOffset,      // IN: ignored
                   size_t size,           // IN:
                   int bufType,           // IN: Unused
                   Bool canBlock)         // IN: Unused
{
   ASSERT(canBlock || !queue->kernelIf->host);

   /*
    * We ignore srcOffset because src is really a struct iovec * and will
    * maintain offset internally.
    */
   return __VMCIMemcpyToQueue(queue, queueOffset, src, size, TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCIMemcpyFromQueueV --
 *
 *      Copies to a given iovec from a VMCI Queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
VMCIMemcpyFromQueueV(void *dest,              // OUT: iovec
                     size_t destOffset,       // IN: ignored
                     const VMCIQueue *queue,  // IN:
                     uint64 queueOffset,      // IN:
                     size_t size,             // IN:
                     int bufType,             // IN: Unused
                     Bool canBlock)           // IN: Unused
{
   ASSERT(canBlock || !queue->kernelIf->host);

   /*
    * We ignore destOffset because dest is really a struct iovec * and will
    * maintain offset internally.
    */
   return __VMCIMemcpyFromQueue(dest, queue, queueOffset, size, TRUE);
}

#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIWellKnownID_AllowMap --
 *
 *      Checks whether the calling context is allowed to register for the given
 *      well known service ID.  Currently returns FALSE if the service ID is
 *      within the reserved range and VMCI_PRIVILEGE_FLAG_TRUSTED is not
 *      provided as the input privilege flags.  Otherwise returns TRUE.
 *      XXX TODO access control based on host configuration information; this
 *      will be platform specific implementation.
 *
 * Results:
 *      Boolean value indicating access granted or denied.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCIWellKnownID_AllowMap(VMCIId wellKnownID,           // IN:
                         VMCIPrivilegeFlags privFlags) // IN:
{
   if (wellKnownID < VMCI_RESERVED_RESOURCE_ID_MAX &&
       !(privFlags & VMCI_PRIVILEGE_FLAG_TRUSTED)) {
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_AllocQueue --
 *
 *      Allocates kernel VA space of specified size plus space for the queue
 *      and kernel interface.  This is different from the guest queue allocator,
 *      because we do not allocate our own queue header/data pages here but
 *      share those of the guest.
 *
 * Results:
 *      A pointer to an allocated and initialized VMCIQueue structure or NULL.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VMCIQueue *
VMCIHost_AllocQueue(uint64 size) // IN:
{
   VMCIQueue *queue;
   const size_t numPages = CEILING(size, PAGE_SIZE) + 1;
   const size_t queueSize = sizeof *queue + sizeof *(queue->kernelIf);
   const size_t queuePageSize = numPages * sizeof *queue->kernelIf->u.h.page;

   queue = VMCI_AllocKernelMem(queueSize + queuePageSize, VMCI_MEMORY_NORMAL);
   if (queue) {
      queue->qHeader = NULL;
      queue->savedHeader = NULL;
      queue->kernelIf = (VMCIQueueKernelIf *)(queue + 1);
      queue->kernelIf->host = TRUE;
      queue->kernelIf->mutex = NULL;
      queue->kernelIf->numPages = numPages;
      queue->kernelIf->u.h.headerPage =
         (struct page **)((uint8*)queue + queueSize);
      queue->kernelIf->u.h.page = &queue->kernelIf->u.h.headerPage[1];
      memset(queue->kernelIf->u.h.headerPage, 0,
             (sizeof *queue->kernelIf->u.h.headerPage *
              queue->kernelIf->numPages));
   }

   return queue;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_FreeQueue --
 *
 *      Frees kernel memory for a given queue (header plus translation
 *      structure).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Memory is freed.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIHost_FreeQueue(VMCIQueue *queue,   // IN:
                   uint64 queueSize)   // IN:
{
   if (queue) {
      const uint queueSize = sizeof *queue + sizeof *(queue->kernelIf);
      VMCI_FreeKernelMem(queue, queueSize);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_InitQueueMutex()
 *
 *       Initialize the mutex for the pair of queues.  This mutex is used to
 *       protect the qHeader and the buffer from changing out from under any
 *       users of either queue.  Of course, it's only any good if the mutexes
 *       are actually acquired.  Queue structure must lie on non-paged memory
 *       or we cannot guarantee access to the mutex.
 *
 * Results:
 *       None.
 *
 * Side Effects:
 *       None.
 *
 *----------------------------------------------------------------------------
 */

void
VMCI_InitQueueMutex(VMCIQueue *produceQ, // IN/OUT
                    VMCIQueue *consumeQ) // IN/OUT
{
   ASSERT(produceQ);
   ASSERT(consumeQ);
   ASSERT(produceQ->kernelIf);
   ASSERT(consumeQ->kernelIf);

   /*
    * Only the host queue has shared state - the guest queues do not
    * need to synchronize access using a queue mutex.
    */

   if (produceQ->kernelIf->host) {
      produceQ->kernelIf->mutex = &produceQ->kernelIf->__mutex;
      consumeQ->kernelIf->mutex = &produceQ->kernelIf->__mutex;
      sema_init(produceQ->kernelIf->mutex, 1);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_CleanupQueueMutex()
 *
 *       Cleans up the mutex for the pair of queues.
 *
 * Results:
 *       None.
 *
 * Side Effects:
 *       None.
 *
 *----------------------------------------------------------------------------
 */

void
VMCI_CleanupQueueMutex(VMCIQueue *produceQ, // IN/OUT
                       VMCIQueue *consumeQ) // IN/OUT
{
   ASSERT(produceQ);
   ASSERT(consumeQ);
   ASSERT(produceQ->kernelIf);
   ASSERT(consumeQ->kernelIf);

   if (produceQ->kernelIf->host) {
      produceQ->kernelIf->mutex = NULL;
      consumeQ->kernelIf->mutex = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_AcquireQueueMutex()
 *
 *       Acquire the mutex for the queue.  Note that the produceQ and
 *       the consumeQ share a mutex.  So, only one of the two need to
 *       be passed in to this routine.  Either will work just fine.
 *
 * Results:
 *       VMCI_SUCCESS always.
 *
 * Side Effects:
 *       May block the caller.
 *
 *----------------------------------------------------------------------------
 */

int
VMCI_AcquireQueueMutex(VMCIQueue *queue, // IN
                       Bool canBlock)    // IN: Unused
{
   ASSERT(queue);
   ASSERT(queue->kernelIf);

   if (queue->kernelIf->host) {
      ASSERT(canBlock);
      ASSERT(queue->kernelIf->mutex);
      down(queue->kernelIf->mutex);
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_ReleaseQueueMutex()
 *
 *       Release the mutex for the queue.  Note that the produceQ and
 *       the consumeQ share a mutex.  So, only one of the two need to
 *       be passed in to this routine.  Either will work just fine.
 *
 * Results:
 *       None.
 *
 * Side Effects:
 *       May block the caller.
 *
 *----------------------------------------------------------------------------
 */

void
VMCI_ReleaseQueueMutex(VMCIQueue *queue) // IN
{
   ASSERT(queue);
   ASSERT(queue->kernelIf);

   if (queue->kernelIf->host) {
      ASSERT(queue->kernelIf->mutex);
      up(queue->kernelIf->mutex);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_LockQueueHeader()
 *
 *       Acquire a spinlock guarding the queue header.  Note that the produceQ
 *       and the consumeQ share the lock mutex.  So, only one of the two need to
 *       be passed in to this routine.  Either will work just fine.
 *
 * Results:
 *       None.
 *
 * Side Effects:
 *       None.
 *
 *----------------------------------------------------------------------------
 */

void
VMCI_LockQueueHeader(VMCIQueue *queue) // IN
{
   ASSERT(queue);
   ASSERT(queue->kernelIf);
   ASSERT(!queue->kernelIf->host);

   /*
    * We don't support non-blocking on the host right now, so we won't get
    * here for a host queue.  And there's no lock required on the guest.  So
    * this is a NOP.
    */
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_UnlockQueueHeader()
 *
 *       Release the spinlock guarding the queue header.
 *
 * Results:
 *       None.
 *
 * Side Effects:
 *       None.
 *
 *----------------------------------------------------------------------------
 */

void
VMCI_UnlockQueueHeader(VMCIQueue *queue) // IN
{
   ASSERT(queue);
   ASSERT(queue->kernelIf);
   ASSERT(!queue->kernelIf->host);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIReleasePageStorePages --
 *
 *       Helper function to release pages in the PageStoreAttachInfo
 *       previously obtained using get_user_pages.
 *
 * Results:
 *       None.
 *
 * Side Effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIReleasePages(struct page **pages,  // IN
                 uint64 numPages,      // IN
                 Bool dirty)           // IN
{
   int i;

   for (i = 0; i < numPages; i++) {
      ASSERT(pages[i]);
      if (dirty) {
         set_page_dirty(pages[i]);
      }
      page_cache_release(pages[i]);
      pages[i] = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_RegisterUserMemory --
 *
 *       Registers the specification of the user pages used for backing a queue
 *       pair. Enough information to map in pages is stored in the OS specific
 *       part of the VMCIQueue structure.
 *
 * Results:
 *       VMCI_SUCCESS on sucess, negative error code on failure.
 *
 * Side Effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIHost_RegisterUserMemory(QueuePairPageStore *pageStore,  // IN
                            VMCIQueue *produceQ,            // OUT
                            VMCIQueue *consumeQ)            // OUT
{
   VA64 produceUVA;
   VA64 consumeUVA;

   ASSERT(produceQ->kernelIf->u.h.headerPage &&
          consumeQ->kernelIf->u.h.headerPage);

   /*
    * The new style and the old style mapping only differs in that we either
    * get a single or two UVAs, so we split the single UVA range at the
    * appropriate spot.
    */

   produceUVA = pageStore->pages;
   consumeUVA = pageStore->pages + produceQ->kernelIf->numPages * PAGE_SIZE;
   return VMCIHost_GetUserMemory(produceUVA, consumeUVA, produceQ, consumeQ);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_UnregisterUserMemory --
 *
 *       Releases and removes the references to user pages stored in the attach
 *       struct.
 *
 * Results:
 *       None
 *
 * Side Effects:
 *       Pages are released from the page cache and may become
 *       swappable again.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIHost_UnregisterUserMemory(VMCIQueue *produceQ,         // IN/OUT
                              VMCIQueue *consumeQ)         // IN/OUT
{
   ASSERT(produceQ->kernelIf);
   ASSERT(consumeQ->kernelIf);
   ASSERT(!produceQ->qHeader && !consumeQ->qHeader);

   VMCIReleasePages(produceQ->kernelIf->u.h.headerPage,
                    produceQ->kernelIf->numPages, TRUE);
   memset(produceQ->kernelIf->u.h.headerPage, 0,
          (sizeof *produceQ->kernelIf->u.h.headerPage *
           produceQ->kernelIf->numPages));
   VMCIReleasePages(consumeQ->kernelIf->u.h.headerPage,
                    consumeQ->kernelIf->numPages, TRUE);
   memset(consumeQ->kernelIf->u.h.headerPage, 0,
          (sizeof *consumeQ->kernelIf->u.h.headerPage *
           consumeQ->kernelIf->numPages));
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_MapQueues --
 *
 *       Once VMCIHost_RegisterUserMemory has been performed on a
 *       queue, the queue pair headers can be mapped into the
 *       kernel. Once mapped, they must be unmapped with
 *       VMCIHost_UnmapQueues prior to calling
 *       VMCIHost_UnregisterUserMemory.
 *
 * Results:
 *       VMCI_SUCCESS if pages are mapped, appropriate error code otherwise.
 *
 * Side Effects:
 *       Pages are pinned.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIHost_MapQueues(VMCIQueue *produceQ,  // IN/OUT
                   VMCIQueue *consumeQ,  // IN/OUT
                   uint32 flags)         // UNUSED
{
   int result;

   if (!produceQ->qHeader || !consumeQ->qHeader) {
      struct page *headers[2];

      if (produceQ->qHeader != consumeQ->qHeader) {
         return VMCI_ERROR_QUEUEPAIR_MISMATCH;
      }

      if (produceQ->kernelIf->u.h.headerPage == NULL ||
          *produceQ->kernelIf->u.h.headerPage == NULL) {
         return VMCI_ERROR_UNAVAILABLE;
      }

      ASSERT(*produceQ->kernelIf->u.h.headerPage &&
             *consumeQ->kernelIf->u.h.headerPage);

      headers[0] = *produceQ->kernelIf->u.h.headerPage;
      headers[1] = *consumeQ->kernelIf->u.h.headerPage;

      produceQ->qHeader = vmap(headers, 2, VM_MAP, PAGE_KERNEL);
      if (produceQ->qHeader != NULL) {
         consumeQ->qHeader =
            (VMCIQueueHeader *)((uint8 *)produceQ->qHeader + PAGE_SIZE);
         result = VMCI_SUCCESS;
      } else {
         Log("vmap failed\n");
         result = VMCI_ERROR_NO_MEM;
     }
   } else {
      result = VMCI_SUCCESS;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_UnmapQueues --
 *
 *       Unmaps previously mapped queue pair headers from the kernel.
 *
 * Results:
 *       VMCI_SUCCESS always.
 *
 * Side Effects:
 *       Pages are unpinned.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIHost_UnmapQueues(VMCIGuestMemID gid,   // IN
                     VMCIQueue *produceQ,  // IN/OUT
                     VMCIQueue *consumeQ)  // IN/OUT
{
   if (produceQ->qHeader) {
      ASSERT(consumeQ->qHeader);

      if (produceQ->qHeader < consumeQ->qHeader) {
         vunmap(produceQ->qHeader);
      } else {
         vunmap(consumeQ->qHeader);
      }
      produceQ->qHeader = NULL;
      consumeQ->qHeader = NULL;
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_GetUserMemory --
 *
 *
 *       Lock the user pages referenced by the {produce,consume}Buffer
 *       struct into memory and populate the {produce,consume}Pages
 *       arrays in the attach structure with them.
 *
 * Results:
 *       VMCI_SUCCESS on sucess, negative error code on failure.
 *
 * Side Effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIHost_GetUserMemory(VA64 produceUVA,       // IN
                       VA64 consumeUVA,       // IN
                       VMCIQueue *produceQ,   // OUT
                       VMCIQueue *consumeQ)   // OUT
{
   int retval;
   int err = VMCI_SUCCESS;

   down_write(&current->mm->mmap_sem);
   retval = get_user_pages(current,
                           current->mm,
                           (VA)produceUVA,
                           produceQ->kernelIf->numPages,
                           1, 0,
                           produceQ->kernelIf->u.h.headerPage,
                           NULL);
   if (retval < produceQ->kernelIf->numPages) {
      Log("get_user_pages(produce) failed (retval=%d)\n", retval);
      VMCIReleasePages(produceQ->kernelIf->u.h.headerPage, retval, FALSE);
      err = VMCI_ERROR_NO_MEM;
      goto out;
   }

   retval = get_user_pages(current,
                           current->mm,
                           (VA)consumeUVA,
                           consumeQ->kernelIf->numPages,
                           1, 0,
                           consumeQ->kernelIf->u.h.headerPage,
                           NULL);
   if (retval < consumeQ->kernelIf->numPages) {
      Log("get_user_pages(consume) failed (retval=%d)\n", retval);
      VMCIReleasePages(consumeQ->kernelIf->u.h.headerPage, retval, FALSE);
      VMCIReleasePages(produceQ->kernelIf->u.h.headerPage,
                       produceQ->kernelIf->numPages, FALSE);
      err = VMCI_ERROR_NO_MEM;
   }

out:
   up_write(&current->mm->mmap_sem);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHost_ReleaseUserMemory --
 *       Release the reference to user pages stored in the attach
 *       struct
 *
 * Results:
 *       None
 *
 * Side Effects:
 *       Pages are released from the page cache and may become
 *       swappable again.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIHost_ReleaseUserMemory(VMCIQueue *produceQ,  // IN/OUT
                           VMCIQueue *consumeQ)  // IN/OUT
{
   ASSERT(produceQ->kernelIf->u.h.headerPage);

   VMCIHost_UnregisterUserMemory(produceQ, consumeQ);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_ReadPortBytes --
 *
 *      Copy memory from an I/O port to kernel memory.
 *
 * Results:
 *      No results.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCI_ReadPortBytes(VMCIIoHandle handle,  // IN: Unused
                   VMCIIoPort port,      // IN
                   uint8 *buffer,        // OUT
                   size_t bufferLength)  // IN
{
   insb(port, buffer, bufferLength);
}
