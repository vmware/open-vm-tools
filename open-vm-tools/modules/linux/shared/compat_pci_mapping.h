/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_PCI_MAPPING_H__
#define __COMPAT_PCI_MAPPING_H__

#include <asm/types.h>
#include <asm/io.h>
#include <linux/pci.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,41)
typedef u32 dma_addr_t;

static __inline__ int 
get_order(unsigned long size)
{
   int order;

   size = (size - 1) >> (PAGE_SHIFT - 1);
   order = -1;
   do {
      size >>= 1;
      order++;
   } while (size);
   return order;
}

static inline void *
compat_pci_alloc_consistent(struct pci_dev *hwdev, size_t size, dma_addr_t *dma_handle)
{
   void *ptr = (void *)__get_free_pages(GFP_ATOMIC, get_order(size));
   if (ptr) {
      memset(ptr, 0, size);
      *dma_handle = virt_to_phys(ptr);
   }
   return ptr;
}

static inline void
compat_pci_free_consistent(struct pci_dev *hwdev, size_t size, void *vaddr, 
                           dma_addr_t dma_handle)
{
   free_pages((unsigned long)vaddr, get_order(size));
}

static inline dma_addr_t
compat_pci_map_single(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
   return virt_to_phys(ptr);
}

static inline void
compat_pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
                        size_t size, int direction)
{
}

#else
#define compat_pci_alloc_consistent(hwdev, size, dma_handle) \
   pci_alloc_consistent(hwdev, size, dma_handle)
#define compat_pci_free_consistent(hwdev, size, vaddr, dma_handle) \
   pci_free_consistent(hwdev, size, vaddr, dma_handle)
#define compat_pci_map_single(hwdev, ptr, size, direction) \
   pci_map_single(hwdev, ptr, size, direction)
#define compat_pci_unmap_single(hwdev, dma_addr, size, direction) \
   pci_unmap_single(hwdev, dma_addr, size, direction)
#endif

#endif
