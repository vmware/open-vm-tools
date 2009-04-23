/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_COMPLETION_H__
#   define __COMPAT_COMPLETION_H__

/*
 * The kernel's completion objects were made available for module use in 2.4.9.
 * 
 * Between 2.4.0 and 2.4.9, we implement completions on our own using 
 * waitqueues and counters. This was done so that we could safely support
 * functions like complete_all(), which cannot be implemented using semaphores.
 *
 * Prior to that, the waitqueue API is substantially different, and since none 
 * of our modules that are built against older kernels need complete_all(), 
 * we fallback on a simple semaphore-based implementation. 
 */

/* 
 * Native completions.
 */ 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 9)

#include <linux/completion.h>
#define compat_completion struct completion
#define compat_init_completion(comp) init_completion(comp)
#define COMPAT_DECLARE_COMPLETION DECLARE_COMPLETION
#define compat_wait_for_completion(comp) wait_for_completion(comp)
#define compat_complete(comp) complete(comp)

/* complete_all() was exported in 2.6.6. */
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 6)
#  include "compat_wait.h"
#  include "compat_list.h"
#  include "compat_spinlock.h"
#  include "compat_sched.h"
#  define compat_complete_all(x)         \
      ({                                 \
          struct list_head *currLinks;   \
          spin_lock(&(x)->wait.lock);    \
          (x)->done += UINT_MAX/2;       \
                                         \
          list_for_each(currLinks, &(x)->wait.task_list) { \
             wait_queue_t *currQueue = list_entry(currLinks, wait_queue_t, task_list); \
             wake_up_process(currQueue->task); \
          }                              \
          spin_unlock(&(x)->wait.lock);  \
      })
# else
#  define compat_complete_all complete_all
# endif

/* 
 * Completions via waitqueues.
 */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)

/*
 * Kernel completions in 2.4.9 and beyond use a counter and a waitqueue, and 
 * our implementation is quite similar. Because __wake_up_common() is not 
 * exported, our implementations of compat_complete() and compat_complete_all()
 * are somewhat racy: the counter is incremented outside of the waitqueue's 
 * lock. 
 *
 * As a result, our completion cannot guarantee in-order wake ups. For example,
 * suppose thread A is entering compat_complete(), thread B is sleeping inside
 * compat_wait_for_completion(), and thread C is just now entering
 * compat_wait_for_completion(). If Thread A is scheduled first and increments 
 * the counter, then gets swapped out, thread C may get scheduled and will 
 * quickly go through compat_wait_for_completion() (since done != 0) while 
 * thread B continues to sleep, even though thread B should have been the one 
 * to wake up.
 */

#include <asm/current.h>
#include "compat_sched.h"
#include "compat_list.h"
#include <linux/smp_lock.h> // for lock_kernel()/unlock_kernel()
#include "compat_wait.h"

typedef struct compat_completion {
   unsigned int done;
   wait_queue_head_t wq;
} compat_completion;

#define compat_init_completion(comp) do { \
   (comp)->done = 0; \
   init_waitqueue_head(&(comp)->wq); \
} while (0)
#define COMPAT_DECLARE_COMPLETION(comp) \
   compat_completion comp = { \
     .done = 0, \
     .wq = __WAIT_QUEUE_HEAD_INITIALIZER((comp).wq), \
   }

/*
 * Locking and unlocking the kernel lock here ensures that the thread
 * is no longer running in module code: compat_complete_and_exit
 * performs the sequence { lock_kernel(); up(comp); compat_exit(); }, with
 * the final unlock_kernel performed implicitly by the resident kernel
 * in do_exit.
 */
#define compat_wait_for_completion(comp) do { \
   spin_lock_irq(&(comp)->wq.lock); \
   if (!(comp)->done) { \
      DECLARE_WAITQUEUE(wait, current); \
      wait.flags |= WQ_FLAG_EXCLUSIVE; \
      __add_wait_queue_tail(&(comp)->wq, &wait); \
      do { \
         __set_current_state(TASK_UNINTERRUPTIBLE); \
         spin_unlock_irq(&(comp)->wq.lock); \
         schedule(); \
         spin_lock_irq(&(comp)->wq.lock); \
      } while (!(comp)->done); \
      __remove_wait_queue(&(comp)->wq, &wait); \
   } \
   (comp)->done--; \
   spin_unlock_irq(&(comp)->wq.lock); \
   lock_kernel(); \
   unlock_kernel(); \
} while (0)

/* XXX: I don't think I need to touch the BKL. */
#define compat_complete(comp) do { \
   unsigned long flags; \
   spin_lock_irqsave(&(comp)->wq.lock, flags); \
   (comp)->done++; \
   spin_unlock_irqrestore(&(comp)->wq.lock, flags); \
   wake_up(&(comp)->wq); \
} while (0)

#define compat_complete_all(comp) do { \
   unsigned long flags; \
   spin_lock_irqsave(&(comp)->wq.lock, flags); \
   (comp)->done += UINT_MAX / 2; \
   spin_unlock_irqrestore(&(comp)->wq.lock, flags); \
   wake_up_all(&(comp)->wq); \
} while (0)

/*
 * Completions via semaphores.
 */ 
#else

#include "compat_semaphore.h"
#define compat_completion struct semaphore 
#define compat_init_completion(comp) init_MUTEX_LOCKED(comp)
#define COMPAT_DECLARE_COMPLETION(comp) DECLARE_MUTEX_LOCKED(comp) 

#define compat_wait_for_completion(comp) do { \
   down(comp); \
   lock_kernel(); \
   unlock_kernel(); \
} while (0)

#define compat_complete(comp) up(comp)

#endif

#endif /* __COMPAT_COMPLETION_H__ */
