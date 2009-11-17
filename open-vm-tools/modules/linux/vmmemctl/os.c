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

#define	OS_DISABLE_UNLOAD 0
#define	OS_DEBUG          1

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

#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif /* CONFIG_PROC_FS */

#include "compat_sched.h"

#include <asm/uaccess.h>
#include <asm/page.h>

#include "vmmemctl_version.h"
#include "os.h"
#include "vmballoon.h"


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

static int os_timer_thread_loop(void *clientData);


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
   return kmalloc(size, OS_KMALLOC_NOSLEEP);
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
 *-----------------------------------------------------------------------------
 *
 * OS_Identity --
 *
 *      Returns an identifier for the guest OS family.
 *
 * Results:
 *      The identifier
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

BalloonGuest
OS_Identity(void)
{
   return BALLOON_GUEST_LINUX;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageGetLimit --
 *
 *      Predict the maximum achievable balloon size.
 *
 *      In 2.4.x and 2.6.x kernels, the balloon driver can guess the number of pages
 *      that can be ballooned. But, for now let us just pass the totalram-size as the
 *      maximum achievable balloon size. Note that normally (unless guest kernel is
 *      booted with a mem=XX parameter) the totalram-size is equal to alloc.max.
 *
 * Results:
 *      The maximum achievable balloon size in pages.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
OS_ReservedPageGetLimit(void)
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
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageGetPPN --
 *
 *      Convert a page handle (of a physical page previously reserved with
 *      OS_ReservedPageAlloc()) to a ppn.
 *
 *      Use newer alloc_page() interface on 2.4.x kernels.
 *
 * Results:
 *      The ppn.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
OS_ReservedPageGetPPN(PageHandle handle) // IN: A valid page handle
{
   struct page *page = (struct page *)handle;

   return page_to_pfn(page);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageAlloc --
 *
 *      Reserve a physical page for the exclusive use of this driver.
 *
 * Results:
 *      On success: A valid page handle that can be passed to OS_ReservedPageGetPPN()
 *                  or OS_ReservedPageFree().
 *      On failure: PAGE_HANDLE_INVALID
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

PageHandle
OS_ReservedPageAlloc(int canSleep) // IN
{
   struct page *page;

   page = alloc_page(canSleep ? OS_PAGE_ALLOC_CANSLEEP : OS_PAGE_ALLOC_NOSLEEP);
   if (page == NULL) {
      return PAGE_HANDLE_INVALID;
   }

   return (PageHandle)page;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageFree --
 *
 *      Unreserve a physical page previously reserved with OS_ReservedPageAlloc().
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
OS_ReservedPageFree(PageHandle handle) // IN: A valid page handle
{
   struct page *page = (struct page *)handle;

   __free_page(page);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_TimerStart --
 *
 *      Setup the timer callback function, then start it.
 *
 * Results:
 *      On success: TRUE
 *      On failure: FALSE
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
OS_TimerStart(OSTimerHandler *handler, // IN
              void *clientData)        // IN
{
   os_timer *t = &global_state.timer;
   os_status *s = &global_state.status;

   /* initialize the timer structure */
   t->handler = handler;
   t->data = clientData;
   t->period = HZ;

   /* initialize sync objects */
   init_waitqueue_head(&t->delay);

   /* create kernel thread */
   t->task = kthread_run(os_timer_thread_loop, t, "vmmemctl");
   if (IS_ERR(t->task)) {
      printk(KERN_WARNING "%s: unable to create kernel thread\n", s->name);
      return FALSE;
   }
   if (OS_DEBUG) {
      printk(KERN_DEBUG "%s: started kernel thread pid=%d\n", s->name, t->task->pid);
   }

   return TRUE;
}


static int
os_timer_thread_loop(void *clientData) // IN
{
   os_timer *t = clientData;

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

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_TimerStop --
 *
 *      Stop the timer.
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
OS_TimerStop(void)
{
   kthread_stop(global_state.timer.task);
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


#ifdef CONFIG_PROC_FS
static int
os_proc_show(struct seq_file *f, // IN
             void *data)         // IN: Unused
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


static int
os_proc_open(struct inode *inode, // IN: Unused
             struct file *file)   // IN
{
   return single_open(file, os_proc_show, NULL);
}
#endif /* CONFIG_PROC_FS */


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Init --
 *
 *      Called at driver startup, initializes the balloon state and structures.
 *
 * Results:
 *      On success: TRUE
 *      On failure: FALSE
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
OS_Init(const char *name,         // IN
        const char *nameVerbose,  // IN
        OSStatusHandler *handler) // IN
{
   os_state *state = &global_state;
   static int initialized = 0;

   /* initialize only once */
   if (initialized++) {
      return FALSE;
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
   state->status.name_verbose = nameVerbose;

#ifdef CONFIG_PROC_FS
   /* register procfs device */
   global_proc_entry = create_proc_entry("vmmemctl", S_IFREG | S_IRUGO, NULL);
   if (global_proc_entry != NULL) {
      global_proc_entry->proc_fops = &global_proc_fops;
   }
#endif /* CONFIG_PROC_FS */

   /* log device load */
   printk(KERN_INFO "%s initialized\n", state->status.name_verbose);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Cleanup --
 *
 *      Called when the driver is terminating, cleanup initialized structures.
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


int
init_module(void)
{
   if (Balloon_ModuleInit() == BALLOON_SUCCESS) {
      return 0;
   } else {
      return -EAGAIN;
   }
}


void
cleanup_module(void)
{
   /*
    * We cannot use module_exit(Balloon_ModuleCleanup) because compilation
    * would fail for 'Kernel Verify Build Status', see bug #459403.
    */
   Balloon_ModuleCleanup();
}


/* Module information. */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Memory Control Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VMMEMCTL_DRIVER_VERSION_STRING);
MODULE_ALIAS("vmware_vmmemctl");
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
