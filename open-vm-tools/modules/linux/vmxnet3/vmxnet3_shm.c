/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * vmxnet3_shm.c --
 *
 *    Shared memory infrastructure for VMXNET3 linux driver. Used by the
 *    VMXNET3 driver to back its rings with memory from a shared memory
 *    pool that is shared with user space.
 */
#include "driver-config.h"

#include "compat_module.h"
#include <linux/moduleparam.h>

#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_ioport.h"
#include "compat_pci.h"
#include "compat_highmem.h"
#include "compat_init.h"
#include "compat_timer.h"
#include "compat_netdevice.h"
#include "compat_skbuff.h"
#include "compat_interrupt.h"
#include "compat_workqueue.h"

#include <asm/dma.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <asm/checksum.h>

#include <linux/if_vlan.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>

#include "vm_basic_types.h"
#include "vmnet_def.h"
#include "vm_device_version.h"
#include "vmxnet3_version.h"


#include "vmxnet3_int.h"
#include "vmxnet3_shm.h"


static int
vmxnet3_shm_consume_user_tx_queue(struct vmxnet3_shm_pool *shm);

int
vmxnet3_shm_tq_xmit(struct sk_buff *skb,
                    struct vmxnet3_tx_queue *tq,
                    struct vmxnet3_adapter *adapter,
                    struct net_device *netdev);

/*
 *----------------------------------------------------------------------------
 *
 * kernel_rx_idx --
 *
 * Result:
 *    Kernel's current shared memory RX ring index
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static inline uint16
kernel_rx_idx(const struct vmxnet3_shm_pool *shm)
{
   return shm->ctl.ptr->kernel_rxi;
}


/*
 *----------------------------------------------------------------------------
 *
 * inc_kernel_rx_idx --
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    Increment the kernel's shared memory RX ring index
 *
 *----------------------------------------------------------------------------
 */

static inline void
inc_kernel_rx_idx(const struct vmxnet3_shm_pool *shm)
{
   shm->ctl.ptr->kernel_rxi = (shm->ctl.ptr->kernel_rxi + 1) % SHM_RX_RING_SIZE;
}


/*
 *----------------------------------------------------------------------------
 *
 * kernel_rx_idx --
 *
 * Result:
 *    Kernel's current shared memory RX ring index
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static inline uint16
kernel_tx_idx(const struct vmxnet3_shm_pool *shm)
{
   return shm->ctl.ptr->kernel_txi;
}


/*
 *----------------------------------------------------------------------------
 *
 * inc_kernel_tx_idx --
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    Increment the kernel's shared memory TX ring index
 *
 *----------------------------------------------------------------------------
 */

static inline void
inc_kernel_tx_idx(const struct vmxnet3_shm_pool *shm)
{
   shm->ctl.ptr->kernel_txi = (shm->ctl.ptr->kernel_txi + 1) % SHM_TX_RING_SIZE;
}


/*
 *----------------------------------------------------------------------------
 *
 * user_rx_idx --
 *
 * Result:
 *    Users's current shared memory RX ring index
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static inline uint16
user_rx_idx(const struct vmxnet3_shm_pool *shm)
{
   return shm->ctl.ptr->user_rxi;
}

/*
 *----------------------------------------------------------------------------
 *
 * kernel_rx_entry --
 *
 * Result:
 *    Kernel's current shared memory RX ring entry
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static inline struct vmxnet3_shm_ringentry *
kernel_rx_entry(const struct vmxnet3_shm_pool *shm)
{
   return &shm->ctl.ptr->rx_ring[kernel_rx_idx(shm)];
}

/*
 *----------------------------------------------------------------------------
 *
 * kernel_tx_entry --
 *
 * Result:
 *    Kernel's current shared memory TX ring entry
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static inline struct vmxnet3_shm_ringentry *
kernel_tx_entry(const struct vmxnet3_shm_pool *shm)
{
   return &shm->ctl.ptr->tx_ring[kernel_tx_idx(shm)];
}


/*
 *----------------------------------------------------------------------------
 *
 * user_rx_entry --
 *
 * Used by vmxnet3_shm_chardev_poll
 *
 * Result:
 *    User's current shared memory RX ring entry
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static inline struct vmxnet3_shm_ringentry *
user_rx_entry(const struct vmxnet3_shm_pool *shm)
{
   return &shm->ctl.ptr->rx_ring[user_rx_idx(shm)];
}


// kobject type
static void
vmxnet3_shm_pool_release(struct kobject *kobj);

static const struct kobj_type vmxnet3_shm_pool_type = {
   .release = vmxnet3_shm_pool_release
};

// vm operations
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
static int
vmxnet3_shm_chardev_fault(struct vm_area_struct *vma,
                          struct vm_fault *vmf);

static struct vm_operations_struct vmxnet3_shm_vm_ops = {
   .fault = vmxnet3_shm_chardev_fault,
};
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 1)
static struct page *
vmxnet3_shm_chardev_nopage(struct vm_area_struct *vma,
                           unsigned long address,
                           int *type);

static struct vm_operations_struct vmxnet3_shm_vm_ops = {
   .nopage = vmxnet3_shm_chardev_nopage,
};
#else
static struct page *
vmxnet3_shm_chardev_nopage(struct vm_area_struct *vma,
                           unsigned long address,
                           int unused);

static struct vm_operations_struct vmxnet3_shm_vm_ops = {
   .nopage = vmxnet3_shm_chardev_nopage,
};
#endif

// file operations
static int vmxnet3_shm_chardev_mmap(struct file *filp,
                                    struct vm_area_struct *vma);

static int vmxnet3_shm_chardev_open(struct inode * inode,
                                    struct file * filp);

static int vmxnet3_shm_chardev_release(struct inode * inode,
                                       struct file * filp);

static unsigned int vmxnet3_shm_chardev_poll(struct file *filp,
                                             poll_table *wait);

static long vmxnet3_shm_chardev_ioctl(struct file *filp,
                                      unsigned int cmd,
                                      unsigned long arg);

#ifndef HAVE_UNLOCKED_IOCTL
static int vmxnet3_shm_chardev_old_ioctl(struct inode *inode,
                                         struct file *filp,
                                         unsigned int cmd,
                                         unsigned long arg);
#endif

static struct file_operations shm_fops = {
   .owner = THIS_MODULE,
   .mmap = vmxnet3_shm_chardev_mmap,
   .open = vmxnet3_shm_chardev_open,
   .release = vmxnet3_shm_chardev_release,
   .poll = vmxnet3_shm_chardev_poll,
#ifdef HAVE_UNLOCKED_IOCTL
   .unlocked_ioctl = vmxnet3_shm_chardev_ioctl,
#ifdef CONFIG_COMPAT
   .compat_ioctl = vmxnet3_shm_chardev_ioctl,
#endif
#else
   .ioctl = vmxnet3_shm_chardev_old_ioctl,
#endif
};

static LIST_HEAD(vmxnet3_shm_list);
static spinlock_t vmxnet3_shm_list_lock = SPIN_LOCK_UNLOCKED;

////////////////////////////////// vmxnet3_shm_pool kobject


//// Lifecycle

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
#define compat_kobject_init(kobj, ktype) { kobject_init(kobj, (struct kobj_type *) ktype); }
#else
#define compat_kobject_init(kobj, _ktype) {  \
   (kobj)->ktype = (struct kobj_type *) _ktype; \
   kobject_init(kobj); \
   }
#endif


/*
 *----------------------------------------------------------------------------
 *
 * vmxnet3_shm_init_allocator --
 *
 * Zero all shared memory data pages and fill the allocator with them.
 *
 * Result:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static void
vmxnet3_shm_init_allocator(struct vmxnet3_shm_pool *shm)
{
   int i;

   shm->allocator.count = 0;
   for (i = 1; i < shm->data.num_pages; i++) {
      struct page *page = VMXNET3_SHM_IDX2PAGE(shm, i);
      void *virt = kmap(page);
      memset(virt, 0, PAGE_SIZE);
      kunmap(page);

      shm->allocator.stack[shm->allocator.count++] = i;

      VMXNET3_ASSERT(i != SHM_INVALID_IDX);
   }
   VMXNET3_ASSERT(shm->allocator.count <= SHM_DATA_SIZE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_pool_reset --
 *
 *    Clean up after userspace has closed the device
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
vmxnet3_shm_pool_reset(struct vmxnet3_shm_pool *shm)
{
   int err = 0;
   printk(KERN_INFO "resetting shm pool\n");

   /*
    * Reset_work may be in the middle of resetting the device, wait for its
    * completion.
    */
   while (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &shm->adapter->state)) {
      compat_msleep(1);
   }

   if (compat_netif_running(shm->adapter->netdev)) {
      vmxnet3_quiesce_dev(shm->adapter);
   }

   vmxnet3_shm_init_allocator(shm);

   if (compat_netif_running(shm->adapter->netdev)) {
      err = vmxnet3_activate_dev(shm->adapter);
   }

   memset(shm->ctl.ptr, 0, PAGE_SIZE);

   clear_bit(VMXNET3_STATE_BIT_RESETTING, &shm->adapter->state);

   if (err) {
      vmxnet3_force_close(shm->adapter);
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_pool_create --
 *
 *    Allocate and initialize shared memory pool. Allocates the data and
 *    control pages, resets them to zero, initializes locks, registers the
 *    character device, etc. Creates virtual address mappings for the pool,
 *    but does not set up DMA yet.
 *
 * Results:
 *    The new shared memory pool object, or NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

struct vmxnet3_shm_pool *
vmxnet3_shm_pool_create(struct vmxnet3_adapter *adapter,
                        char *name)
{
   int i;
   unsigned long flags;
   struct vmxnet3_shm_pool *shm;
   struct vmxnet3_shm_ctl *ctl_ptr;
   struct page *ctl_page;

   // Allocate shm_pool kobject
   shm = kmalloc(sizeof(*shm), GFP_KERNEL);
   if (shm == NULL) {
      goto fail_shm;
   }
   memset(shm, 0, sizeof(*shm));
   compat_kobject_init(&shm->kobj, &vmxnet3_shm_pool_type);
   //shm->kobj.ktype = &vmxnet3_shm_pool_type;
   //kobject_init(&shm->kobj);
   snprintf(shm->name, sizeof(shm->name), "vmxnet_%s_shm", name);
   kobject_set_name(&shm->kobj, shm->name);
   shm->adapter = adapter;

   // Allocate data pages
   shm->data.num_pages = SHM_DATA_SIZE;
   for (i = 1; i < shm->data.num_pages; i++) {
      struct page *page = alloc_page(GFP_KERNEL);
      if (page == NULL) {
         goto fail_data;
      }

      VMXNET3_SHM_SET_IDX2PAGE(shm, i, page);

      VMXNET3_ASSERT(i != SHM_INVALID_IDX);
   }

   // Allocate control page
   ctl_page = alloc_page(GFP_KERNEL);
   if (ctl_page == NULL) {
      goto fail_ctl;
   }
   ctl_ptr = (void*)kmap(ctl_page);
   shm->ctl.pages[0] = ctl_page;
   shm->ctl.ptr = ctl_ptr;

   // Reset data and control pages
   vmxnet3_shm_init_allocator(shm);
   memset(shm->ctl.ptr, 0, PAGE_SIZE);

   // Register char device
   shm->misc_dev.minor = MISC_DYNAMIC_MINOR;
   shm->misc_dev.name = shm->name;
   shm->misc_dev.fops = &shm_fops;
   if (misc_register(&shm->misc_dev)) {
      printk(KERN_ERR "failed to register vmxnet3_shm character device\n");
      goto fail_cdev;
   }

   // Initialize locks
   spin_lock_init(&shm->alloc_lock);
   spin_lock_init(&shm->tx_lock);
   spin_lock_init(&shm->rx_lock);
   init_waitqueue_head(&shm->rxq);

   spin_lock_irqsave(&vmxnet3_shm_list_lock, flags);
   list_add(&shm->list, &vmxnet3_shm_list);
   spin_unlock_irqrestore(&vmxnet3_shm_list_lock, flags);

   printk(KERN_INFO "created vmxnet shared memory pool %s\n", shm->name);

   return shm;

fail_cdev:
   kunmap(ctl_page);
   __free_page(ctl_page);

fail_data:
fail_ctl:
   for (i = 0; i < shm->data.num_pages; i++) {
      if (VMXNET3_SHM_IDX2PAGE(shm, i) != NULL) {
         __free_page(VMXNET3_SHM_IDX2PAGE(shm, i));
      }
   }

   kfree(shm);

fail_shm:
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_pool_release --
 *
 *    Release a shared memory pool.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
vmxnet3_shm_pool_release(struct kobject *kobj)
{
   int i;
   unsigned long flags;
   struct vmxnet3_shm_pool *shm = container_of(kobj, struct vmxnet3_shm_pool, kobj);

   spin_lock_irqsave(&vmxnet3_shm_list_lock, flags);
   list_del(&shm->list);
   spin_unlock_irqrestore(&vmxnet3_shm_list_lock, flags);

   misc_deregister(&shm->misc_dev);

   // Free control pages
   for (i = 0; i < SHM_CTL_SIZE; i++) {
      kunmap(shm->ctl.pages[i]);
      __free_page(shm->ctl.pages[i]);
   }

   // Free data pages
   for (i = 1; i < SHM_DATA_SIZE; i++) {
      __free_page(VMXNET3_SHM_IDX2PAGE(shm, i));
   }

   kfree(shm);

   printk(KERN_INFO "destroyed vmxnet shared memory pool %s\n", shm->name);
}


//// Shared memory pool management

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_alloc_page --
 *
 *    Allocate a page from the shared memory area.
 *
 * Results:
 *    Index to page or SHM_INVALID_IDX on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

uint16
vmxnet3_shm_alloc_page(struct vmxnet3_shm_pool *shm)
{
   uint16 idx;
   unsigned long flags;

   spin_lock_irqsave(&shm->alloc_lock, flags);
   if (shm->allocator.count == 0) {
      idx = SHM_INVALID_IDX;
   } else {
      idx = shm->allocator.stack[--shm->allocator.count];
      VMXNET3_ASSERT(idx != SHM_INVALID_IDX);
   }
   //printk(KERN_INFO "allocator count: %d (alloc idx: %d)\n", shm->allocator.count, idx);
   spin_unlock_irqrestore(&shm->alloc_lock, flags);

   return idx;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_free_page --
 *
 *    Free a page back to the shared memory area
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
vmxnet3_shm_free_page(struct vmxnet3_shm_pool *shm,
                      uint16 idx)
{
   unsigned long flags;

   spin_lock_irqsave(&shm->alloc_lock, flags);
   VMXNET3_ASSERT(shm->allocator.count < SHM_DATA_SIZE);
   shm->allocator.stack[shm->allocator.count++] = idx;
   //printk(KERN_INFO "allocator count: %d (freed idx: %d)\n", shm->allocator.count, idx);
   spin_unlock_irqrestore(&shm->alloc_lock, flags);
}


//// Char device

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_addr2idx --
 *
 *    Convert user space address into index into the shared memory pool.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static inline unsigned long
vmxnet3_shm_addr2idx(struct vm_area_struct *vma,
                     unsigned long address)
{
   return vma->vm_pgoff + ((address - vma->vm_start) >> PAGE_SHIFT);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_fault --
 *
 *    mmap fault handler. Called if the user space requests a page for
 *    which there is no shared memory mapping yet. We need to lookup
 *    the page we want to back the shared memory mapping with.
 *
 * Results:
 *    The page backing the user space address.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet3_shm_chardev_fault(struct vm_area_struct *vma,
                          struct vm_fault *vmf)
{
   struct vmxnet3_shm_pool *shm = vma->vm_private_data;
   unsigned long address = (unsigned long)vmf->virtual_address;
   unsigned long idx = vmxnet3_shm_addr2idx(vma, address);
   struct page *pageptr;

   if (idx >= SHM_DATA_START && idx < SHM_DATA_START + SHM_DATA_SIZE) {
      pageptr = VMXNET3_SHM_IDX2PAGE(shm, idx - SHM_DATA_START);
   } else if (idx >= SHM_CTL_START && idx < SHM_CTL_START + SHM_CTL_SIZE) {
      pageptr = shm->ctl.pages[idx - SHM_CTL_START];
   } else {
      pageptr = NULL;
   }

   if (pageptr) {
      get_page(pageptr);
   }

   vmf->page = pageptr;

   return pageptr ? VM_FAULT_MINOR : VM_FAULT_ERROR;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 1)

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_nopage --
 *
 *    mmap nopage handler. Called if the user space requests a page for
 *    which there is no shared memory mapping yet. We need to lookup
 *    the page we want to back the shared memory mapping with.
 *
 * Results:
 *    The page backing the user space address.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static struct page *
vmxnet3_shm_chardev_nopage(struct vm_area_struct *vma,
                           unsigned long address,
                           int *type)
{
   struct vmxnet3_shm_pool *shm = vma->vm_private_data;
   unsigned long idx = vmxnet3_shm_addr2idx(vma, address);
   struct page *pageptr;

   if (idx >= SHM_DATA_START && idx < SHM_DATA_START + SHM_DATA_SIZE) {
      pageptr = VMXNET3_SHM_IDX2PAGE(shm, idx - SHM_DATA_START);
   } else if (idx >= SHM_CTL_START && idx < SHM_CTL_START + SHM_CTL_SIZE) {
      pageptr = shm->ctl.pages[idx - SHM_CTL_START];
   } else {
      pageptr = NULL;
   }

   if (pageptr) {
      get_page(pageptr);
   }

   if (type) {
      *type = pageptr ? VM_FAULT_MINOR : VM_FAULT_SIGBUS;
   }

   return pageptr;
}

#else
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_nopage --
 *
 *    mmap nopage handler. Called if the user space requests a page for
 *    which there is no shared memory mapping yet. We need to lookup
 *    the page we want to back the shared memory mapping with.
 *
 * Results:
 *    The page backing the user space address.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static struct page *
vmxnet3_shm_chardev_nopage(struct vm_area_struct *vma,
                           unsigned long address,
                           int unused)
{
   struct vmxnet3_shm_pool *shm = vma->vm_private_data;
   unsigned long idx = vmxnet3_shm_addr2idx(vma, address);
   struct page *pageptr;

   if (idx >= SHM_DATA_START && idx < SHM_DATA_START + SHM_DATA_SIZE) {
      pageptr = VMXNET3_SHM_IDX2PAGE(shm, idx - SHM_DATA_START);
   } else if (idx >= SHM_CTL_START && idx < SHM_CTL_START + SHM_CTL_SIZE) {
      pageptr = shm->ctl.pages[idx - SHM_CTL_START];
   } else {
      pageptr = NULL;
   }

   if (pageptr) {
      get_page(pageptr);
   }

   return pageptr;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_mmap --
 *
 *    Setup mmap.
 *
 * Results:
 *    Always 0.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet3_shm_chardev_mmap(struct file *filp,
                         struct vm_area_struct *vma)
{
   vma->vm_private_data = filp->private_data;
   vma->vm_ops = &vmxnet3_shm_vm_ops;
   vma->vm_flags |= VM_RESERVED;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_poll --
 *
 *    Poll called from user space. We consume the TX queue and then go to
 *    sleep until we get woken up by an interrupt.
 *
 * Results:
 *    Poll mask.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static unsigned int
vmxnet3_shm_chardev_poll(struct file *filp,
                         poll_table *wait)
{
   struct vmxnet3_shm_pool *shm = filp->private_data;
   unsigned int mask = 0;
   unsigned long flags;
   struct vmxnet3_shm_ringentry *re;

   // consume TX queue
   if (vmxnet3_shm_consume_user_tx_queue(shm) == -1) {
      // the device has been closed, let the user space
      // know there is activity, so that it gets a chance
      // to read the channelBad flag.
      mask |= POLLIN;
      return mask;
   }

   // Wait on the rxq for an interrupt to wake us
   poll_wait(filp, &shm->rxq, wait);

   // Check if the user's current RX entry is full
   spin_lock_irqsave(&shm->rx_lock, flags);
   re = user_rx_entry(shm);
   if (re->own) {
      // XXX: We need a comment that explains what this does.
      mask |= POLLIN | POLLRDNORM;
   }
   spin_unlock_irqrestore(&shm->rx_lock, flags);

   return mask;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_ioctl --
 *
 *    Handle ioctls from user space.
 *
 * Results:
 *    Return code depends on ioctl.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static long
vmxnet3_shm_chardev_ioctl(struct file *filp,
                          unsigned int cmd,
                          unsigned long arg)
{
   struct vmxnet3_shm_pool *shm = filp->private_data;
   uint16 idx;
   uint16 idx1;
   int i;

   switch(cmd) {

      case SHM_IOCTL_TX:
         vmxnet3_shm_consume_user_tx_queue(shm);
         return 0;

      case SHM_IOCTL_ALLOC_ONE:
         idx = vmxnet3_shm_alloc_page(shm);
         return idx;

      case SHM_IOCTL_ALLOC_MANY:
         for (i = 0; i < arg; i++) {
            idx = vmxnet3_shm_alloc_page(shm);
            if (idx != SHM_INVALID_IDX) {
               if (vmxnet3_shm_user_rx(shm, idx, 0, 1, 1)) {
                  vmxnet3_shm_free_page(shm, idx);
                  return SHM_INVALID_IDX;
               }
            } else {
               return SHM_INVALID_IDX;
            }
         }
         return 0;

      case SHM_IOCTL_ALLOC_ONE_AND_MANY:
         idx1 = vmxnet3_shm_alloc_page(shm);
         if (idx1 == SHM_INVALID_IDX) {
            return SHM_INVALID_IDX;
         }
         for (i = 0; i < arg - 1; i++) {
            idx = vmxnet3_shm_alloc_page(shm);
            if (idx != SHM_INVALID_IDX) {
               if (vmxnet3_shm_user_rx(shm, idx, 0, 1, 1)) {
                  vmxnet3_shm_free_page(shm, idx);
                  vmxnet3_shm_free_page(shm, idx1);
                  return SHM_INVALID_IDX;
               }
            } else {
               vmxnet3_shm_free_page(shm, idx1);
               return SHM_INVALID_IDX;
            }
         }
         return idx1;

      case SHM_IOCTL_FREE_ONE:
         if (arg != SHM_INVALID_IDX && arg < SHM_DATA_SIZE) {
            vmxnet3_shm_free_page(shm, arg);
         }
         return 0;
   }

   return -ENOTTY;
}

#ifndef HAVE_UNLOCKED_IOCTL
static int vmxnet3_shm_chardev_old_ioctl(struct inode *inode,
                                         struct file *filp,
                                         unsigned int cmd,
                                         unsigned long arg)
{
   return vmxnet3_shm_chardev_ioctl(filp, cmd, arg);
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_find_by_minor --
 *
 *    Find the right shared memory pool based on the minor number of the
 *    char device.
 *
 * Results:
 *    Pointer to the shared memory pool, or NULL on error.
 *
 * Side effects:
 *    Takes a reference on the kobj of the shm object.
 *
 *-----------------------------------------------------------------------------
 */

static struct vmxnet3_shm_pool *
vmxnet3_shm_chardev_find_by_minor(unsigned int minor)
{
   struct vmxnet3_shm_pool *shm, *tmp;
   unsigned long flags;

   spin_lock_irqsave(&vmxnet3_shm_list_lock, flags);

   list_for_each_entry_safe(shm, tmp, &vmxnet3_shm_list, list) {
      if (shm->misc_dev.minor == minor && kobject_get(&shm->kobj)) {
         spin_unlock_irqrestore(&vmxnet3_shm_list_lock, flags);
         return shm;
      }
   }

   spin_unlock_irqrestore(&vmxnet3_shm_list_lock, flags);

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_open --
 *
 *    Find the right shared memory pool based on the minor number of the
 *    char device.
 *
 * Results:
 *    0 on success or -ENODEV if no device exists with the given minor number
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet3_shm_chardev_open(struct inode * inode,
                         struct file * filp)
{
   // Stash pointer to shm in file so file ops can use it
   filp->private_data = vmxnet3_shm_chardev_find_by_minor(iminor(inode));
   if (filp->private_data == NULL) {
      return -ENODEV;
   }

   // XXX: What does this do??
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
//   filp->f_mapping->backing_dev_info = &directly_mappable_cdev_bdi;
#endif

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_chardev_release --
 *
 *    Closing the char device. Release the ref count on the shared memory
 *    pool, perform cleanup.
 *
 * Results:
 *    Always 0.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet3_shm_chardev_release(struct inode * inode,
                            struct file * filp)
{
   struct vmxnet3_shm_pool *shm = filp->private_data;

   if (shm->adapter) {
      vmxnet3_shm_pool_reset(shm);
   } else {
      vmxnet3_shm_init_allocator(shm);
      memset(shm->ctl.ptr, 0, PAGE_SIZE);
   }
   
   kobject_put(&shm->kobj);

   return 0;
}


//// TX and RX

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_free_skbpages --
 *
 *    Free the shared memory pages (secretly) backing this skb.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
vmxnet3_free_skbpages(struct vmxnet3_adapter *adapter,
                      struct sk_buff *skb)
{
   int i;

   vmxnet3_shm_free_page(adapter->shm, VMXNET3_SHM_SKB_GETIDX(skb));
   for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
      struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];

      vmxnet3_shm_free_page(adapter->shm, (unsigned long)frag->page);
   }

   skb_shinfo(skb)->nr_frags = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_start_tx --
 *
 *    The shared memory vmxnet version of the hard_start_xmit routine.
 *    Just frees the given packet as we do not intend to transmit any
 *    packet given to us by the TCP/IP stack.
 *
 * Results:
 *    Always 0 for success.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
vmxnet3_shm_start_tx(struct sk_buff *skb,
                     struct net_device *dev)
{
   compat_dev_kfree_skb_irq(skb, FREE_WRITE);
   return COMPAT_NETDEV_TX_OK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_tx_pkt --
 *
 *    Send a packet (collection of ring entries) using h/w tx routine.
 *
 *    Protected by shm.tx_lock
 *
 * Results:
 *    0 on success. Negative value to indicate error
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static inline int
vmxnet3_shm_tx_pkt(struct vmxnet3_adapter *adapter,
                   struct vmxnet3_shm_ringentry *res,
                   int frags)
{
   struct sk_buff* skb;
   int i;

   skb = dev_alloc_skb(100);
   if (skb == NULL) {
      for (i = 0; i < frags; i++) {
         vmxnet3_shm_free_page(adapter->shm, res[i].idx);
      }
      VMXNET3_ASSERT(FALSE);
      return -ENOMEM;
   }

   VMXNET3_SHM_SKB_SETIDX(skb, res[0].idx);
   VMXNET3_SHM_SKB_SETLEN(skb, res[0].len);

   for (i = 1; i < frags; i++) {
      struct skb_frag_struct *frag = skb_shinfo(skb)->frags +
                                     skb_shinfo(skb)->nr_frags;

      VMXNET3_ASSERT(skb_shinfo(skb)->nr_frags < MAX_SKB_FRAGS);

      frag->page = (struct page*)(unsigned long)res[i].idx;
      frag->page_offset = 0;
      frag->size = res[i].len;
      skb_shinfo(skb)->nr_frags ++;
   }

   {
      struct vmxnet3_tx_queue *tq = &adapter->tx_queue;
      int ret;
      skb->protocol = htons(ETH_P_IPV6);
      adapter->shm->ctl.ptr->stats.kernel_tx += frags; // XXX: move to better place
      ret = vmxnet3_shm_tq_xmit(skb, tq, adapter, adapter->netdev);
      if (ret == COMPAT_NETDEV_TX_BUSY) {
         vmxnet3_dev_kfree_skb(adapter, skb);
      }

      return ret;
   }

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_tq_xmit --
 *
 *    Wrap vmxnet3_tq_xmit holding the netdev tx lock to better emulate the
 *    Linux stack. Also check for a stopped tx queue to avoid racing with
 *    vmxnet3_close.
 *
 * Results:
 *    Same as vmxnet3_tq_xmit.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
int
vmxnet3_shm_tq_xmit(struct sk_buff *skb,
                    struct vmxnet3_tx_queue *tq,
                    struct vmxnet3_adapter *adapter,
                    struct net_device *netdev)
{
   int ret = COMPAT_NETDEV_TX_BUSY;
   compat_netif_tx_lock(netdev);
   if (!netif_queue_stopped(netdev)) {
      ret = vmxnet3_tq_xmit(skb, tq, adapter, netdev);
   }
   compat_netif_tx_unlock(netdev);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_tx_re --
 *
 *    Add one entry to the partial TX array. If re->eop is set, i.e. if
 *    the packet is complete, TX the partial packet.
 *
 * Results:
 *    1 if eop
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet3_shm_tx_re(struct vmxnet3_shm_pool *shm,
                  struct vmxnet3_shm_ringentry re)
{
   shm->partial_tx.res[shm->partial_tx.frags++] = re;

   if (re.eop) {
      int status = vmxnet3_shm_tx_pkt(shm->adapter,
                                     shm->partial_tx.res,
                                     shm->partial_tx.frags);
      if (status < 0) {
         VMXNET3_LOG("vmxnet3_shm_tx_pkt failed %d\n", status);
      }
      shm->partial_tx.frags = 0;
      return 1;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_consume_user_tx_queue --
 *
 *    Consume all packets in the user TX queue and send full
 *    packets to the device
 *
 * Results:
 *    0 on success.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet3_shm_consume_user_tx_queue(struct vmxnet3_shm_pool *shm)
{
   unsigned long flags;
   struct vmxnet3_shm_ringentry *re;

   spin_lock_irqsave(&shm->tx_lock, flags);

   // Check if the device has been closed
   if (shm->adapter == NULL) {
      spin_unlock_irqrestore(&shm->tx_lock, flags);
      return -1;
   }

   /*
    * Loop through each full entry in the user TX ring. Discard trash frags and
    * add the others to the partial TX array. If an entry has eop set, TX the
    * partial packet.
    */
   while ((re = kernel_tx_entry(shm))->own) {
      if (re->trash) {
         vmxnet3_shm_free_page(shm, re->idx);
         shm->ctl.ptr->stats.kernel_tx++;
      } else {
         vmxnet3_shm_tx_re(shm, *re);
      }
      inc_kernel_tx_idx(shm);
      *re = RE_ZERO;
   }

   spin_unlock_irqrestore(&shm->tx_lock, flags);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_user_desc_available --
 *
 *    Checks if we have num_entries ring entries available on the rx ring.
 *
 * Results:
 *    0 for yes
 *    -ENOMEM for not enough entries available
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet3_shm_user_desc_available(struct vmxnet3_shm_pool *shm,
                                uint16 num_entries)
{
   struct vmxnet3_shm_ringentry *re;
   uint16 reIdx = kernel_rx_idx(shm);

   while (num_entries > 0) {
      re = &shm->ctl.ptr->rx_ring[reIdx];
      if (re->own) {
         return -ENOMEM;
      }
      reIdx = (reIdx + 1) % SHM_RX_RING_SIZE;
      num_entries--;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_rx_skb --
 *
 *    Receives an skb into the rx ring. If we can't receive all fragments,
 *    the entire skb is dropped.
 *
 * Results:
 *    0 for success
 *    -ENOMEM for not enough entries available
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
vmxnet3_shm_rx_skb(struct vmxnet3_adapter *adapter,
                   struct sk_buff *skb)
{
   int ret;
   int i;
   int num_entries = 1 + skb_shinfo(skb)->nr_frags;
   int eop = (num_entries == 1);

   if (vmxnet3_shm_user_desc_available(adapter->shm, num_entries) == -ENOMEM) {
      vmxnet3_dev_kfree_skb_irq(adapter, skb);
      return -ENOMEM;
   }

   ret = vmxnet3_shm_user_rx(adapter->shm,
                            VMXNET3_SHM_SKB_GETIDX(skb),
                            VMXNET3_SHM_SKB_GETLEN(skb),
                            0 /* trash */,
                            eop);
   if (ret != 0) {
      VMXNET3_ASSERT(FALSE);
      printk(KERN_ERR "vmxnet3_shm_user_rx failed on frag 0\n");
   }

   for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
      struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];
      unsigned long shm_idx = (unsigned long)frag->page;

      eop = (i == skb_shinfo(skb)->nr_frags - 1);

      ret = vmxnet3_shm_user_rx(adapter->shm,
                               shm_idx,
                               frag->size,
                               0 /* trash */,
                               eop);
      if (ret != 0) {
         VMXNET3_ASSERT(FALSE);
         printk(KERN_ERR "vmxnet3_shm_user_rx failed on frag 1+\n");
      }
   }


   /*
    * Do NOT use the vmxnet3 version of kfree_skb, as we handed
    * ownership of shm pages to the user space, thus we must not
    * free them again.
    */
   skb_shinfo(skb)->nr_frags = 0;
   compat_dev_kfree_skb_irq(skb, FREE_WRITE);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_user_rx --
 *
 *    Put one packet fragment into the shared memory RX ring
 *
 * Results:
 *    0 on success.
 *    Negative value on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
vmxnet3_shm_user_rx(struct vmxnet3_shm_pool *shm,
                    uint16 idx,
                    uint16 len,
                    int trash,
                    int eop)
{
   struct vmxnet3_shm_ringentry *re = kernel_rx_entry(shm);
   shm->ctl.ptr->stats.kernel_rx++;
   if (re->own) {
      return -ENOMEM;
   }
   inc_kernel_rx_idx(shm);
   re->idx = idx;
   re->len = len;
   re->trash = trash;
   re->eop = eop;
   re->own = TRUE;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_open --
 *
 *    Called when the vmxnet3 device is opened. Allocates the per-device
 *    shared memory pool.
 *
 * Results:
 *    0 on success.
 *    Negative value on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
vmxnet3_shm_open(struct vmxnet3_adapter *adapter,
                 char *name)
{
   adapter->shm = vmxnet3_shm_pool_create(adapter, name);
   if (adapter->shm == NULL) {
      printk(KERN_ERR "failed to create shared memory pool\n");
      return -ENOMEM;
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_shm_close --
 *
 *    Called when the vmxnet3 device is closed. Does not free the per-device
 *    shared memory pool. The character device might still be open. Thus
 *    freeing the shared memory pool is tied to the ref count on
 *    shm->kobj dropping to zero instead.
 *
 * Results:
 *    0 on success.
 *    Negative value on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
vmxnet3_shm_close(struct vmxnet3_adapter *adapter)
{
   unsigned long flags;

   // Can't unset the lp pointer if a TX is in progress
   spin_lock_irqsave(&adapter->shm->tx_lock, flags);
   adapter->shm->adapter = NULL;
   spin_unlock_irqrestore(&adapter->shm->tx_lock, flags);

   // Mark the channel as 'in bad state' 
   adapter->shm->ctl.ptr->channelBad = 1;
   
   kobject_put(&adapter->shm->kobj);
   
   wake_up(&adapter->shm->rxq);

   return 0;
}
