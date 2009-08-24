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
 *      Wrappers for Linux system functions required by "vmmemctl".
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/kthread.h>

#ifdef	CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif	/* CONFIG_PROC_FS */

#include "compat_sched.h"

#include <asm/uaccess.h>
#include <asm/page.h>

#include "vmmemctl_version.h"
#include "os.h"


/*
 * Constants
 */

/*
 * Use __GFP_HIGHMEM to allow pages from HIGHMEM zone. We don't
 * allow wait (__GFP_WAIT) for NOSLEEP page allocations. Use
 * __GFP_NOWARN, to suppress page allocation failure warnings.
 */
#define OS_PAGE_ALLOC_NOSLEEP	(__GFP_HIGHMEM|__GFP_NOWARN)

/*
 * GFP_ATOMIC allocations dig deep for free pages. Maybe it is
 * okay because balloon driver uses OS_Malloc() to only allocate
 * few bytes, and the allocation requires a new page only occasionally.
 * Still if __GFP_NOMEMALLOC flag is available, then use it to inform
 * the guest's page allocator not to use emergency pools.
 */
#ifdef __GFP_NOMEMALLOC
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC|__GFP_NOMEMALLOC|__GFP_NOWARN)
#else
#define OS_KMALLOC_NOSLEEP	(GFP_ATOMIC|__GFP_NOWARN)
#endif

/*
 * Use GFP_HIGHUSER when executing in a separate kernel thread
 * context and allocation can sleep.  This is less stressful to
 * the guest memory system, since it allows the thread to block
 * while memory is reclaimed, and won't take pages from emergency
 * low-memory pools.
 */
#define	OS_PAGE_ALLOC_CANSLEEP	(GFP_HIGHUSER)

/*
 * Types
 */

typedef struct {
   /* registered state */
   OSTimerHandler *handler;
   void *data;
   int period;

   /* system structures */
   wait_queue_head_t delay;
   struct task_struct *task;
} os_timer;

typedef struct {
   /* registered state */
   OSStatusHandler *handler;
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
static struct proc_dir_entry *global_proc_entry;
static int os_proc_open(struct inode *, struct file *);
static struct file_operations global_proc_fops = {
   .open = os_proc_open,
   .read = seq_read,
   .llseek = seq_lseek,
   .release = single_release,
};
#endif	/* CONFIG_PROC_FS */

static os_state global_state;


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Malloc --
 *
 *      Allocates kernel memory.
 *
 * Results:
 *      On success: Pointer to allocated memory
 *      On failure: NULL
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
OS_Malloc(size_t size) // IN
{
   return(kmalloc(size, OS_KMALLOC_NOSLEEP));
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Free --
 *
 *      Free allocated kernel memory.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OS_Free(void *ptr,   // IN
        size_t size) // IN
{
   kfree(ptr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_MemZero --
 *
 *      Fill a memory location with 0s.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OS_MemZero(void *ptr,   // OUT
           size_t size) // IN
{
   memset(ptr, 0, size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_MemCopy --
 *
 *      Copy a memory portion into another location.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OS_MemCopy(void *dest,      // OUT
           const void *src, // IN
           size_t size)     // IN
{
   memcpy(dest, src, size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Snprintf --
 *
 *      Print a string into a bounded memory location.
 *
 * Results:
 *      Number of character printed including trailing \0.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
OS_Snprintf(char *buf,          // OUT
            size_t size,        // IN
            const char *format, // IN
            ...)                // IN
{
   int result;
   va_list args;

   va_start(args, format);
   result = vsnprintf(buf, size, format, args);
   va_end(args);

   return result;
}

/*
 * System-Dependent Operations
 */

const char *
OS_Identity(void)
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
unsigned int
OS_PredictMaxReservedPages(void)
{
   struct sysinfo info;
   os_state *state = &global_state;

   /*
    * si_meminfo() is cheap. Moreover, we want to provide dynamic
    * max balloon size later. So let us call si_meminfo() every
    * iteration.
    */
   si_meminfo(&info);

   /* info.totalram is in pages */
   state->totalMemoryPages = info.totalram;
   return state->totalMemoryPages;
}

/*
 * Use newer alloc_page() interface on 2.4.x kernels.
 * Use "struct page *" value as page handle for clients.
 */
unsigned long
OS_AddrToPPN(unsigned long addr)
{
   struct page *page = (struct page *) addr;

   return page_to_pfn(page);
}

unsigned long
OS_AllocReservedPage(int canSleep)
{
   struct page *page = alloc_page(canSleep ?
                           OS_PAGE_ALLOC_CANSLEEP : OS_PAGE_ALLOC_NOSLEEP);

   return (unsigned long)page;
}

void
OS_FreeReservedPage(unsigned long addr)
{
   /* deallocate page */
   struct page *page = (struct page *) addr;
   __free_page(page);
}

void
OS_TimerInit(OSTimerHandler *handler, // IN
             void *clientData,        // IN
             int period)              // IN
{
   os_timer *t = &global_state.timer;
   t->handler = handler;
   t->data = clientData;
   t->period = period;
}

static int os_timer_thread_loop(void *data)
{
   os_timer *t = (os_timer *) data;

   /* we are running */
   compat_set_freezable();

   /* main loop */
   while (1) {
      /* sleep for specified period */
      wait_event_interruptible_timeout(t->delay,
                                       compat_wait_check_freezing() ||
                                       kthread_should_stop(),
                                       t->period);
      compat_try_to_freeze();
      if (kthread_should_stop()) {
         break;
      }

      /* execute registered handler */
      t->handler(t->data);
   }

   /* terminate */
   return(0);
}

void
OS_TimerStart(void)
{
   os_timer *t = &global_state.timer;
   os_status *s = &global_state.status;

   /* initialize sync objects */
   init_waitqueue_head(&t->delay);

   /* create kernel thread */
   t->task = kthread_run(os_timer_thread_loop, t, "vmmemctl");
   if (IS_ERR(t->task)) {
      /* fail */
      printk(KERN_WARNING "%s: unable to create kernel thread\n", s->name);
   } else if (OS_DEBUG) {
      printk(KERN_DEBUG "%s: started kernel thread pid=%d\n", s->name,
             t->task->pid);
   }
}

void
OS_TimerStop(void)
{
   kthread_stop(global_state.timer.task);
}

unsigned int
OS_TimerHz(void)
{
   return HZ;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Yield --
 *
 *      Yield the CPU, if needed.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      This thread might get descheduled, other threads might get scheduled.
 *
 *-----------------------------------------------------------------------------
 */

void
OS_Yield(void)
{
   cond_resched();
}

#ifdef	CONFIG_PROC_FS
static int os_proc_show(struct seq_file *f,
			void *data)
{
   os_status *s = &global_state.status;
   char *buf = NULL;
   int err = -1;

   if (s->handler == NULL) {
      err = 0;
      goto out;
   }

   buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
   if (buf == NULL) {
      err = -ENOMEM;
      goto out;
   }

   s->handler(buf, PAGE_SIZE);

   if (seq_puts(f, buf) != 0) {
      err = -ENOSPC;
      goto out;
   }

   err = 0;

  out:
   kfree(buf);

   return err;
}

static int os_proc_open(struct inode *inode,
			struct file *file)
{
   return single_open(file, os_proc_show, NULL);
}

#endif

void
OS_Init(const char *name,
        const char *name_verbose,
        OSStatusHandler *handler)
{
   os_state *state = &global_state;
   static int initialized = 0;

   /* initialize only once */
   if (initialized++) {
      return;
   }

   /* prevent module unload with extra reference */
   if (OS_DISABLE_UNLOAD) {
      try_module_get(THIS_MODULE);
   }

   /* zero global state */
   memset(state, 0, sizeof(global_state));

   /* initialize status state */
   state->status.handler = handler;
   state->status.name = name;
   state->status.name_verbose = name_verbose;

#ifdef	CONFIG_PROC_FS
   /* register procfs device */
   global_proc_entry = create_proc_entry("vmmemctl", S_IFREG | S_IRUGO, NULL);
   if (global_proc_entry != NULL) {
      global_proc_entry->proc_fops = &global_proc_fops;
   }
#endif	/* CONFIG_PROC_FS */

   /* log device load */
   printk(KERN_INFO "%s initialized\n", state->status.name_verbose);
}

void
OS_Cleanup(void)
{
   os_status *s = &global_state.status;

#ifdef	CONFIG_PROC_FS
   /* unregister procfs entry */
   remove_proc_entry("vmmemctl", NULL);
#endif	/* CONFIG_PROC_FS */

   /* log device unload */
   printk(KERN_INFO "%s unloaded\n", s->name_verbose);
}

/* Module information. */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Memory Control Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VMMEMCTL_DRIVER_VERSION_STRING);
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
