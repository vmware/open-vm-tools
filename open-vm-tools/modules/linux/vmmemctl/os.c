/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
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
 * os.c --
 *
 * 	Wrappers for Linux system functions required by "vmmemctl".
 *	This allows customers to build their own vmmemctl driver for
 *	custom versioned kernels without the need for source code.
 */

/*
 * Compile-Time Options
 */

#define	OS_DISABLE_UNLOAD	(0)
#define	OS_DEBUG		(1)

/*
 * Includes
 */

#include "driver-config.h"

#include "compat_module.h"
#include <linux/types.h>
#include "compat_kernel.h"
#include "compat_completion.h"
#include "compat_mm.h"
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include "compat_sched.h"
#include <asm/uaccess.h>
#include "compat_page.h"
#include "compat_wait.h"
#include "vmmemctl_version.h"

#ifdef	CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#endif	/* CONFIG_PROC_FS */

#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/smp_lock.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 9)
int errno;  /* compat_exit() needs global errno variable. */
#endif

/*
 * Compatibility definitions.
 */

/*
 * Execute as a separate kernel thread on 2.4.x kernels.
 * Allow allocations from high memory  on 2.4.x kernels.
 */
#define	OS_KTHREAD	(1)
#endif

#include "os.h"


/*
 * Constants
 */

#ifdef	OS_KTHREAD
/*
 * Use __GFP_HIGHMEM to allow pages from HIGHMEM zone. We don't
 * allow wait (__GFP_WAIT) for NOSLEEP page allocations. Use 
 * __GFP_NOWARN, if available, to suppress page allocation failure
 * warnings.
 */
#ifdef __GFP_NOWARN
#define OS_PAGE_ALLOC_NOSLEEP	(__GFP_HIGHMEM|__GFP_NOWARN)
#else
#define OS_PAGE_ALLOC_NOSLEEP	(__GFP_HIGHMEM)
#endif

/*
 * GFP_ATOMIC allocations dig deep for free pages. Maybe it is
 * okay because balloon driver uses os_kmalloc_*() to only allocate
 * few bytes, and the allocation requires a new page only occasionally. 
 * Still if __GFP_NOMEMALLOC flag is available, then use it to inform
 * the guest's page allocator not to use emergency pools,
 */
#ifdef __GFP_NOWARN

#ifdef __GFP_NOMEMALLOC
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC|__GFP_NOMEMALLOC|__GFP_NOWARN)
#else
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC|__GFP_NOWARN)
#endif

#else

#ifdef __GFP_NOMEMALLOC
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC|__GFP_NOMEMALLOC)
#else
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC)
#endif

#endif
/*
 * Use GFP_HIGHUSER when executing in a separate kernel thread 
 * context and allocation can sleep.  This is less stressful to
 * the guest memory system, since it allows the thread to block
 * while memory is reclaimed, and won't take pages from emergency
 * low-memory pools.
 */
#define	OS_PAGE_ALLOC_CANSLEEP	(GFP_HIGHUSER)

#else /* OS_KTHREAD not defined */

/* 2.2.x kernel is a special case. The balloon driver is unable
 * to block (sleep) because it is not executing in a separate kernel 
 * thread. Therefore, the driver can only use NOSLEEP page 
 * allocations. 
 *
 * Use __GFP_LOW when available (2.2.x kernels) to avoid stressing
 * the guest memory system, otherwise simply use GFP_ATOMIC, which
 * is always defined (normally as __GFP_HIGH).
 */
#ifdef	__GFP_LOW
#define	OS_PAGE_ALLOC_NOSLEEP	(__GFP_LOW)
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC)
#else
#define	OS_PAGE_ALLOC_NOSLEEP	(GFP_ATOMIC)
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC)
#endif

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 41)
#define OS_USE_SCHEDULE_DELAYED_WORK
#else
#undef OS_USE_SCHEDULE_DELAYED_WORK
#endif

/*
 * Types
 */

typedef struct {
   /* registered state */
   os_timer_handler handler;
   void *data;
   int period;

   /* termination flag */
   volatile int stop;

   /* system structures */
#ifdef	OS_KTHREAD   
   wait_queue_head_t delay;
   compat_completion notifyStart;
   compat_completion notifyStop;
   pid_t pid;
#else
#ifdef OS_USE_SCHEDULE_DELAYED_WORK
   struct work_struct work;
#else
   struct timer_list timer;
   struct tq_struct task;
#endif
#endif
} os_timer;

typedef struct {
   /* registered state */
   os_status_handler handler;
   const char *name_verbose;
   const char *name;
} os_status;

typedef struct {
   os_status status;
   os_timer timer;
   unsigned int totalMemoryPages;
} os_state;

/*
 * Globals
 */

#ifdef	CONFIG_PROC_FS
#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
static int os_proc_read(char *, char **, off_t, int);
static struct proc_dir_entry *global_proc_entry;
#else
static int os_proc_read(char *, char **, off_t, int, int);
static struct proc_dir_entry global_proc_entry = {
   0, 8, "vmmemctl", S_IFREG | S_IRUGO, 1, 0, 0, 0, NULL, os_proc_read,
};
#endif
#endif	/* CONFIG_PROC_FS */

static os_state global_state;

/*
 * Simple Wrappers
 */

void * CDECL
os_kmalloc_nosleep(unsigned int size)
{
   return(kmalloc(size, OS_KMALLOC_NOSLEEP));
}

void CDECL
os_kfree(void *obj, unsigned int size)
{
   kfree(obj);
}

void CDECL
os_bzero(void *s, unsigned int n)
{
   memset(s, 0, n);
}

void CDECL
os_memcpy(void *dest, const void *src, unsigned int size)
{
   memcpy(dest, src, size);
}

int CDECL
os_sprintf(char *str, const char *format, ...)
{
   int result;
   va_list args;

   va_start(args, format);
   result = vsprintf(str, format, args);
   va_end(args);

   return(result);
}

/*
 * System-Dependent Operations
 */

char * CDECL
os_identity(void)
{
   return("linux");
}

/*
 * Predict the maximum achievable balloon size.
 *
 * In 2.4.x and 2.6.x kernels, the balloon driver can guess the number of pages
 * that can be ballooned. But, for now let us just pass the totalram-size as the 
 * maximum achievable balloon size. Note that normally (unless guest kernel is
 * booted with a mem=XX parameter) the totalram-size is equal to alloc.max.
 *
 * Returns the maximum achievable balloon size in pages
 */  
unsigned int CDECL
os_predict_max_balloon_pages(void)
{
   struct sysinfo info;
   os_state *state = &global_state;

#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)

   /* 
    * In 2.4.0 and later, si_meminfo() is cheap. Moreover, we want to provide
    * dynamic max balloon size later. So let us call si_meminfo() every 
    * iteration. 
    */
   si_meminfo(&info);
   
   /* In 2.4.x and later kernels, info.totalram is in pages */
   state->totalMemoryPages = info.totalram;
   return(state->totalMemoryPages);

#else 

   /* 2.2.x kernel */
   if (!state->totalMemoryPages) {
      si_meminfo(&info); /* In 2.2.x, si_meminfo() is a costly operation */
      /* In 2.2.x kernels, info.totalram is in bytes */
      state->totalMemoryPages = info.totalram >> PAGE_SHIFT;
   }
   return(state->totalMemoryPages);

#endif
}

/*
 * Use newer alloc_page() interface on 2.4.x kernels.
 * Use "struct page *" value as page handle for clients.
 */
unsigned long CDECL
os_addr_to_ppn(unsigned long addr)
{
   struct page *page = (struct page *) addr;
   return(page_to_pfn(page));
}

unsigned long CDECL
os_alloc_reserved_page(int can_sleep)
{
   struct page *page;
   /* allocate page */
   if (can_sleep) {
#ifdef OS_KTHREAD
      page = alloc_page(OS_PAGE_ALLOC_CANSLEEP);
#else
      return 0;
#endif
   } else {
      page = alloc_page(OS_PAGE_ALLOC_NOSLEEP);
   }
   return((unsigned long) page);
}

void CDECL
os_free_reserved_page(unsigned long addr)
{
   /* deallocate page */
   struct page *page = (struct page *) addr;
   __free_page(page);
}

#ifndef	OS_KTHREAD
static void os_timer_add(os_timer *t);

static void os_timer_bh(void *data)
{
   os_timer *t = (os_timer *) data;

   if (!t->stop) {
      /* execute registered handler, rearm timer */
      (*(t->handler))(t->data);
      os_timer_add(t);
   }
}

#ifndef OS_USE_SCHEDULE_DELAYED_WORK
static void os_timer_internal(ulong data)
{
   os_timer *t = (os_timer *) data;

   /* perform real work in registered bottom-half handler */
   queue_task(&t->task, &tq_immediate);
   mark_bh(IMMEDIATE_BH);
}
#endif

static void os_timer_add(os_timer *t)
{
#ifdef OS_USE_SCHEDULE_DELAYED_WORK
   schedule_delayed_work(&t->work, t->period);
#else
   /* schedule timer callback */
   struct timer_list *timer = &t->timer;
   timer->expires = jiffies + t->period;
   add_timer(timer);
#endif
}

static void os_timer_remove(os_timer *t)
{
#ifdef OS_USE_SCHEDULE_DELAYED_WORK
   cancel_delayed_work(&t->work);
   flush_scheduled_work();
#else
   /* deschedule timer callback */
   struct timer_list *timer = &t->timer;
   (void) del_timer(timer);
#endif
}
#endif

void CDECL
os_timer_init(os_timer_handler handler, void *data, int period)
{
   os_timer *t = &global_state.timer;
   t->handler = handler;
   t->data = data;
   t->period = period;
   t->stop = 0;
#ifndef OS_KTHREAD
#ifdef OS_USE_SCHEDULE_DELAYED_WORK
   INIT_WORK(&t->work, os_timer_bh, t);
#else
   t->task.routine = os_timer_bh;
   t->task.data = t;
   /* initialize timer state */
   init_timer(&t->timer);
   t->timer.function = os_timer_internal;
   t->timer.data = (ulong) t;
#endif
#endif
}

#ifdef	OS_KTHREAD
static int os_timer_thread_loop(void *data)
{
   os_timer *t = (os_timer *) data;

   /* detach thread */
   lock_kernel();
   compat_daemonize("vmmemctl");
   unlock_kernel();
   
   /* we are running */
   compat_complete(&t->notifyStart);
   compat_set_freezable();

   /* main loop */
   while (1) {
      /* sleep for specified period */
      wait_event_interruptible_timeout(t->delay, t->stop, t->period);
      compat_try_to_freeze();
      if (t->stop) {
         break;
      }

      /* execute registered handler */
      (*(t->handler))(t->data);
   }

   /* terminate */
   compat_complete_and_exit(&t->notifyStop, 0);
   return(0);
}

static int os_timer_thread_start(os_timer *t)
{
   os_status *s = &global_state.status;

   /* initialize sync objects */
   compat_init_completion(&t->notifyStart);
   compat_init_completion(&t->notifyStop);   
   init_waitqueue_head(&t->delay);

   /* create kernel thread */
   t->pid = kernel_thread(os_timer_thread_loop, t, 0);
   if (t->pid < 0) {
      /* fail */
      printk(KERN_WARNING "%s: unable to create kernel thread (%d)\n", s->name, t->pid);
      return(-1);
   }

   if (OS_DEBUG) {
      printk(KERN_DEBUG "%s: started kernel thread pid=%d\n", s->name, t->pid);
   }

   /* block until started... Why?! */
   compat_wait_for_completion(&t->notifyStart);
   return(0);
}

static void os_timer_thread_stop(os_timer *t)
{
   wake_up_interruptible(&t->delay);
   compat_wait_for_completion(&t->notifyStop);
}
#endif

void CDECL
os_timer_start(void)
{
   os_timer *t = &global_state.timer;

   /* clear termination flag */
   t->stop = 0;

#ifdef	OS_KTHREAD
   os_timer_thread_start(t);
#else
   os_timer_add(t);
#endif
}

void CDECL
os_timer_stop(void)
{
   os_timer *t = &global_state.timer;

   /* set termination flag */
   t->stop = 1;

#ifdef	OS_KTHREAD
   os_timer_thread_stop(t);
#else
   os_timer_remove(t);
#endif
}

unsigned int CDECL
os_timer_hz(void)
{
   return HZ;
}

void CDECL
os_yield(void)
{
#ifdef OS_KTHREAD
   cond_resched();
#else
   /* Do nothing.  Timer callbacks should not sleep. */
#endif
}

#ifdef	CONFIG_PROC_FS
#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
static int os_proc_read(char *buf,
                        char **start,
                        off_t offset,
                        int length)
#else
static int os_proc_read(char *buf,
                        char **start,
                        off_t offset,
                        int length,
                        int unused)
#endif
{
   os_status *s = &global_state.status;

   /* done if no handler */
   if (s->handler == NULL) {
      return(0);
   }

   /* invoke registered handler */
   return(s->handler(buf));
}
#endif

void CDECL
os_init(const char *name,
        const char *name_verbose,
        os_status_handler handler)
{
   os_state *state = &global_state;
   static int initialized = 0;

   /* initialize only once */
   if (initialized++) {
      return;
   }

   /* prevent module unload with extra reference */
   if (OS_DISABLE_UNLOAD) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 48)
      MOD_INC_USE_COUNT;
#else
      try_module_get(THIS_MODULE);
#endif
   }

   /* zero global state */
   memset(state, 0, sizeof(global_state));

   /* initialize status state */
   state->status.handler = handler;
   state->status.name = name;
   state->status.name_verbose = name_verbose;

#ifdef	CONFIG_PROC_FS
   /* register procfs device */
#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
   global_proc_entry = create_proc_entry("vmmemctl", S_IFREG | S_IRUGO, NULL);
   if (global_proc_entry != NULL) {
      global_proc_entry->get_info = os_proc_read;
   }
#else
   proc_register(&proc_root, &global_proc_entry);
#endif
#endif	/* CONFIG_PROC_FS */

   /* log device load */
   printk(KERN_INFO "%s initialized\n", state->status.name_verbose);
}

void CDECL
os_cleanup(void)
{
   os_status *s = &global_state.status;
   int err;

#ifdef	CONFIG_PROC_FS
   /* unregister procfs entry */
#if	LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
   remove_proc_entry("vmmemctl", NULL);
   err = 0;
#else
   if ((err = proc_unregister(&proc_root, global_proc_entry.low_ino)) != 0) {
      printk(KERN_WARNING "%s: unable to unregister procfs entry (%d)\n", s->name, err);
   }
#endif
#endif	/* CONFIG_PROC_FS */

   /* log device unload */
   printk(KERN_INFO "%s unloaded\n", s->name_verbose);
}

/* Module information. */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Memory Control Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VMMEMCTL_DRIVER_VERSION_STRING);
