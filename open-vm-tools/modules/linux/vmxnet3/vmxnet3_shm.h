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
 * vmxnet3_shm.h --
 *
 *      Definitions for shared memory infrastructure for VMXNET3 linux driver.
 */

#ifndef _VMXNET3_SHM_H_
#define _VMXNET3_SHM_H_

#include "vmxnet3_shm_shared.h"
#include <linux/miscdevice.h>

/*
 * Bumping up the max tx descriptor per packet.
 * We need one more than VMXNET3_SHM_MAX_FRAGS because of partial header copy.
 */
#define VMXNET3_MAX_TXD_PER_PKT_SHM (VMXNET3_SHM_MAX_FRAGS + 1)

struct vmxnet3_shm_mapped_page
{
   struct page *page;
   void *virt;
};

struct vmxnet3_shm_pool
{
   struct list_head list;
   char name[IFNAMSIZ + 16];
   struct kobject kobj;

   struct
   {
      // pages backing the map in virtual address order
      struct vmxnet3_shm_mapped_page pages[SHM_DATA_SIZE];
      unsigned int num_pages;
   } data;

   struct
   {
      // pages backing the map in virtual address order
      struct page *pages[SHM_CTL_SIZE];
      struct vmxnet3_shm_ctl *ptr;
   } ctl;

   struct
   {
      /*
       * This is a stack of free pages. count is the number of free pages, so
       * count - 1 is the topmost free page.
       */
      uint16 count;
      uint16 stack[SHM_DATA_SIZE];
   } allocator;

   struct
   {
      struct vmxnet3_shm_ringentry res[VMXNET3_SHM_MAX_FRAGS];
      int frags;
   } partial_tx;

   struct miscdevice misc_dev;

   wait_queue_head_t rxq;
   spinlock_t alloc_lock, tx_lock, rx_lock;
   struct vmxnet3_adapter *adapter;
};

// Convert ring index to the struct page* or virt address.
#define VMXNET3_SHM_IDX2PAGE(shm, idx) (shm->data.pages[(idx)].page)
#define VMXNET3_SHM_SET_IDX2PAGE(shm, idx, x) (shm->data.pages[(idx)].page = (x))

#define VMXNET3_SHM_SKB_GETIDX(skb) (compat_skb_transport_offset(skb))
#define VMXNET3_SHM_SKB_SETIDX(skb, idx) (compat_skb_set_transport_header(skb, idx))
#define VMXNET3_SHM_SKB_SETLEN(skb, len) (compat_skb_set_network_header(skb, len))
#define VMXNET3_SHM_SKB_GETLEN(skb) (compat_skb_network_offset(skb))

int
vmxnet3_shm_close(struct vmxnet3_adapter *adapter);
int
vmxnet3_shm_open(struct vmxnet3_adapter *adapter, char *name);
int
vmxnet3_shm_user_rx(struct vmxnet3_shm_pool *shm,
                   uint16 idx, uint16 len,
                   int trash, int eop);
void
vmxnet3_free_skbpages(struct vmxnet3_adapter *adapter, struct sk_buff *skb);

uint16
vmxnet3_shm_alloc_page(struct vmxnet3_shm_pool *shm);
void
vmxnet3_shm_free_page(struct vmxnet3_shm_pool *shm, uint16 idx);

int
vmxnet3_shm_start_tx(struct sk_buff *skb, struct net_device *dev);
int
vmxnet3_shm_rx_skb(struct vmxnet3_adapter *adapter, struct sk_buff *skb);

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_dev_kfree_skb_* --
 *
 *      Covers for dev_kfree_skb*. Deal with the shared memory version of skbs.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static inline void
vmxnet3_dev_kfree_skb(struct vmxnet3_adapter *adapter, struct sk_buff *skb)
{
   if (adapter->is_shm) {
      vmxnet3_free_skbpages(adapter, skb);
   }
   compat_dev_kfree_skb(skb, FREE_WRITE);
}

static inline void
vmxnet3_dev_kfree_skb_any(struct vmxnet3_adapter *adapter, struct sk_buff *skb)
{
   if (adapter->is_shm) {
      vmxnet3_free_skbpages(adapter, skb);
   }
   compat_dev_kfree_skb_any(skb, FREE_WRITE);
}

static inline void
vmxnet3_dev_kfree_skb_irq(struct vmxnet3_adapter *adapter, struct sk_buff *skb)
{
   if (adapter->is_shm) {
      vmxnet3_free_skbpages(adapter, skb);
   }
   compat_dev_kfree_skb_irq(skb, FREE_WRITE);
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_skb_* --
 *
 *      Covers for (compat_)skb_*. Deal with the shared memory version of skbs.
 *
 * Results:
 *      Depends.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static inline unsigned int
vmxnet3_skb_headlen(struct vmxnet3_adapter *adapter, struct sk_buff *skb)
{
   if (adapter->is_shm) {
      return VMXNET3_SHM_SKB_GETLEN(skb);
   } else {
      return compat_skb_headlen(skb);
   }
}

static inline void
vmxnet3_skb_put(struct vmxnet3_adapter *adapter, struct sk_buff *skb, unsigned int len)
{
   if (!adapter->is_shm) {
      skb_put(skb, len);
   } else {
      unsigned int oldlen = VMXNET3_SHM_SKB_GETLEN(skb);
      VMXNET3_SHM_SKB_SETLEN(skb, len + oldlen);
   }
}

static inline struct sk_buff*
vmxnet3_dev_alloc_skb(struct vmxnet3_adapter *adapter, unsigned long length)
{
   if (adapter->is_shm) {
      int idx;
      struct sk_buff* skb;
      idx = vmxnet3_shm_alloc_page(adapter->shm);
      if (idx == SHM_INVALID_IDX) {
         return NULL;
      }

      // The length is arbitrary because that memory shouldn't be used
      skb = dev_alloc_skb(100);
      if (skb == NULL) {
         vmxnet3_shm_free_page(adapter->shm, idx);
         return NULL;
      }

      VMXNET3_SHM_SKB_SETIDX(skb, idx);
      VMXNET3_SHM_SKB_SETLEN(skb, 0);

      return skb;
   } else {
      return dev_alloc_skb(length);
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_map_* --
 *
 *      Covers for pci_map_*. Deal with the shared memory version of skbs.
 *
 * Results:
 *      DMA address
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static inline dma_addr_t
vmxnet3_map_single(struct vmxnet3_adapter *adapter,
                   struct sk_buff * skb,
                   size_t offset,
                   size_t len,
                   int direction)
{
   if (adapter->is_shm) {
      unsigned long shm_idx = VMXNET3_SHM_SKB_GETIDX(skb);
      struct page *real_page = VMXNET3_SHM_IDX2PAGE(adapter->shm, shm_idx);
      return pci_map_page(adapter->pdev,
                          real_page,
                          offset,
                          len,
                          direction);
   } else {
      return pci_map_single(adapter->pdev,
                            skb->data + offset,
                            len,
                            direction);
   }

}

static inline dma_addr_t
vmxnet3_map_page(struct vmxnet3_adapter *adapter,
                 struct page *page,
                 size_t offset,
                 size_t len,
                 int direction)
{
   if (adapter->is_shm) {
      unsigned long shm_idx = (unsigned long)page;
      page = VMXNET3_SHM_IDX2PAGE(adapter->shm, shm_idx);
   }

   return pci_map_page(adapter->pdev,
                       page,
                       offset,
                       len,
                       direction);
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet3_(put|alloc)_page --
 *
 *      Allocation and release of pages. Either use regular or shared memory
 *      pages.
 *
 * Results:
 *      Depends
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static inline void
vmxnet3_put_page(struct vmxnet3_adapter *adapter,
                 struct page *page)
{
   if (!adapter->is_shm) {
      put_page(page);
   } else {
      vmxnet3_shm_free_page(adapter->shm, (unsigned long)page);
   }
}

static inline void *
vmxnet3_alloc_page(struct vmxnet3_adapter *adapter)
{
   if (adapter->is_shm) {
      return (void*) (unsigned long) vmxnet3_shm_alloc_page(adapter->shm);
   } else {
      return alloc_page(GFP_ATOMIC);
   }
}


#endif
