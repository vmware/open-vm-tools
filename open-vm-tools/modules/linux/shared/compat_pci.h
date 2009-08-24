/*********************************************************
 * Copyright (C) 1999 VMware, Inc. All rights reserved.
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
 * compat_pci.h: PCI compatibility wrappers.
 */

#ifndef __COMPAT_PCI_H__
#define __COMPAT_PCI_H__

#include "compat_ioport.h"
#include <linux/pci.h>
#ifndef KERNEL_2_1
#   include <linux/bios32.h>
#endif

#if KERNEL_VERSION(2, 6, 6) <= LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#   ifndef DMA_BIT_MASK
#      define DMA_BIT_MASK(n) DMA_##n##BIT_MASK
#   endif
#endif


/* 2.0.x has useless struct pci_dev; remap it to our own */
#ifndef KERNEL_2_1
#define pci_dev    vmw_pci_driver_instance
#endif


/* 2.0/2.2 does not have pci driver API */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
struct vmw_pci_driver_instance {
   struct vmw_pci_driver_instance *next;
   void                   *driver_data;
   struct pci_driver      *pcidrv;
#ifdef KERNEL_2_1
   struct pci_dev         *pcidev;
#else
   unsigned char           bus;
   unsigned char           devfn;
   unsigned int            irq;
#endif
};
#endif


/* 2.0 has pcibios_* calls only...  We have to provide pci_* compatible wrappers. */
#ifndef KERNEL_2_1
static inline int
pci_read_config_byte(struct pci_dev *pdev,  // IN: PCI slot
                     unsigned char   where, // IN: Byte to read
                     u8             *value) // OUT: Value read
{
   return pcibios_read_config_byte(pdev->bus, pdev->devfn, where, value);
}

static inline int
pci_read_config_dword(struct pci_dev *pdev,  // IN: PCI slot
                      unsigned char   where, // IN: Dword to read
                      u32            *value) // OUT: Value read
{
   return pcibios_read_config_dword(pdev->bus, pdev->devfn, where, value);
}

static inline int
pci_write_config_dword(struct pci_dev *pdev,  // IN: PCI slot
                       unsigned char   where, // IN: Dword to write
                       u32             value) // IN: Value to write
{
   return pcibios_write_config_dword(pdev->bus, pdev->devfn, where, value);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * compat_pci_name --
 *
 *      Return human readable PCI slot name.  Note that some implementations
 *      return a pointer to the static storage, so returned value may be
 *      overwritten by subsequent calls to this function.
 *
 * Results:
 *      Returns pointer to the string with slot name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 22)
#define compat_pci_name(pdev) pci_name(pdev)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#define compat_pci_name(pdev) (pdev)->slot_name
#elif defined(KERNEL_2_1)
static inline const char*
compat_pci_name(struct pci_dev* pdev)
{
   static char slot_name[12];
   sprintf(slot_name, "%02X:%02X.%X", pdev->bus->number,
           PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
   return slot_name;
}
#else
static inline const char*
compat_pci_name(struct pci_dev* pdev)
{
   static char slot_name[12];
   sprintf(slot_name, "%02X:%02X.%X", pdev->bus,
           PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
   return slot_name;
}
#endif


/* pci_resource_start comes in 4 flavors - 2.0, 2.2, early 2.3, 2.4+ */
#ifndef KERNEL_2_1
static inline unsigned long
compat_pci_resource_start(struct pci_dev *pdev,
                          unsigned int    index)
{
   u32 addr;

   if (pci_read_config_dword(pdev, PCI_BASE_ADDRESS_0 + index * 4, &addr)) {
      printk(KERN_ERR "Unable to read base address %u from PCI slot %s!\n",
             index, compat_pci_name(pdev));
      return ~0UL;
   }
   if (addr & PCI_BASE_ADDRESS_SPACE) {
      return addr & PCI_BASE_ADDRESS_IO_MASK;
   } else {
      return addr & PCI_BASE_ADDRESS_MEM_MASK;
   }
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 1)
#   define compat_pci_resource_start(dev, index) \
       (((dev)->base_address[index] & PCI_BASE_ADDRESS_SPACE) \
          ? ((dev)->base_address[index] & PCI_BASE_ADDRESS_IO_MASK) \
          : ((dev)->base_address[index] & PCI_BASE_ADDRESS_MEM_MASK))
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 43)
#   define compat_pci_resource_start(dev, index) \
       ((dev)->resource[index].start)
#else
#   define compat_pci_resource_start(dev, index) \
       pci_resource_start(dev, index)
#endif

/* since 2.3.15, a new set of s/w res flags IORESOURCE_ is introduced,
 * we fake them by returning either IORESOURCE_{IO, MEM} prior to 2.3.15 since
 * this is what compat_pci_request_region uses
 */
#ifndef KERNEL_2_1
static inline unsigned long
compat_pci_resource_flags(struct pci_dev *pdev,
                          unsigned int    index)
{
   u32 addr;

   if (pci_read_config_dword(pdev, PCI_BASE_ADDRESS_0 + index * 4, &addr)) {
      printk(KERN_ERR "Unable to read base address %u from PCI slot %s!\n",
             index, compat_pci_name(pdev));
      return ~0UL;
   }
   if (addr & PCI_BASE_ADDRESS_SPACE) {
      return IORESOURCE_IO;
   } else {
      return IORESOURCE_MEM;
   }
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 1)
#   define compat_pci_resource_flags(dev, index) \
       (((dev)->base_address[index] & PCI_BASE_ADDRESS_SPACE) \
          ? IORESOURCE_IO: IORESOURCE_MEM)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 15)
    /* IORESOURCE_xxx appeared in 2.3.15 and is set in resource[].flags */
#   define compat_pci_resource_flags(dev, index) ((dev)->resource[index].flags)
#else
#   define compat_pci_resource_flags(dev, index) pci_resource_flags(dev, index)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
static inline unsigned long
compat_pci_resource_len(struct pci_dev *pdev,  // IN
                        unsigned int    index) // IN
{
   u32 addr, mask;
   unsigned char reg = PCI_BASE_ADDRESS_0 + index * 4;

   if (pci_read_config_dword(pdev, reg, &addr) || addr == 0xFFFFFFFF) {
      return 0;
   }

   pci_write_config_dword(pdev, reg, 0xFFFFFFFF);
   pci_read_config_dword(pdev, reg, &mask);
   pci_write_config_dword(pdev, reg, addr);

   if (mask == 0 || mask == 0xFFFFFFFF) {
      return 0;
   }
   if (addr & PCI_BASE_ADDRESS_SPACE) {
      return 65536 - (mask & PCI_BASE_ADDRESS_IO_MASK & 0xFFFF);
   } else {
      return -(mask & PCI_BASE_ADDRESS_MEM_MASK);
   }
}
#else
#define compat_pci_resource_len(dev, index) pci_resource_len(dev, index)
#endif

/* pci_request_region appears in 2.4.20 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 20)
static inline int
compat_pci_request_region(struct pci_dev *pdev, int bar, char *name)
{
   if (compat_pci_resource_len(pdev, bar) == 0) {
      return 0;
   }

   if (compat_pci_resource_flags(pdev, bar) & IORESOURCE_IO) {
      if (!compat_request_region(compat_pci_resource_start(pdev, bar),
                                 compat_pci_resource_len(pdev, bar),
                                 name)) {
         return -EBUSY;
      }
   } else if (compat_pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
      if (!compat_request_mem_region(compat_pci_resource_start(pdev, bar),
                                     compat_pci_resource_len(pdev, bar),
                                     name)) {
         return -EBUSY;
      }
   }

   return 0;
}

static inline void
compat_pci_release_region(struct pci_dev *pdev, int bar)
{
   if (compat_pci_resource_len(pdev, bar) != 0) {
      if (compat_pci_resource_flags(pdev, bar) & IORESOURCE_IO) {
         release_region(compat_pci_resource_start(pdev, bar),
                        compat_pci_resource_len(pdev, bar));
      } else if (compat_pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
         compat_release_mem_region(compat_pci_resource_start(pdev, bar),
                                   compat_pci_resource_len(pdev, bar));
      }
   }
}
#else
#define compat_pci_request_region(pdev, bar, name)  pci_request_region(pdev, bar, name)
#define compat_pci_release_region(pdev, bar)        pci_release_region(pdev, bar)
#endif

/* pci_request_regions appeears in 2.4.3 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 3)
static inline int
compat_pci_request_regions(struct pci_dev *pdev, char *name)
{
   int i;
   
   for (i = 0; i < 6; i++) {
      if (compat_pci_request_region(pdev, i, name)) {
         goto release;
      }
   }
   return 0;

release:
   while (--i >= 0) {
      compat_pci_release_region(pdev, i);
   }
   return -EBUSY;
}
static inline void
compat_pci_release_regions(struct pci_dev *pdev)
{
   int i;
   
   for (i = 0; i < 6; i++) {
      compat_pci_release_region(pdev, i);
   }
}
#else
#define compat_pci_request_regions(pdev, name) pci_request_regions(pdev, name)
#define compat_pci_release_regions(pdev)       pci_release_regions(pdev)
#endif

/* pci_enable_device is available since 2.4.0 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#define compat_pci_enable_device(pdev) (0)
#else
#define compat_pci_enable_device(pdev) pci_enable_device(pdev)
#endif


/* pci_set_master is available since 2.2.0 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 0)
#define compat_pci_set_master(pdev) (0)
#else
#define compat_pci_set_master(pdev) pci_set_master(pdev)
#endif


/* pci_disable_device is available since 2.4.4 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 4)
#define compat_pci_disable_device(pdev) do {} while (0)
#else
#define compat_pci_disable_device(pdev) pci_disable_device(pdev)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
/*
 * Devices supported by particular pci driver.  While 2.4+ kernels
 * can do match on subsystem and class too, we support match on
 * vendor/device IDs only.
 */
struct pci_device_id {
   unsigned int vendor, device;
   unsigned long driver_data;
};
#define PCI_DEVICE(vend, dev)   .vendor = (vend), .device = (dev)

/* PCI driver */
struct pci_driver {
   const char *name;
   const struct pci_device_id *id_table;
   int   (*probe)(struct pci_dev* dev, const struct pci_device_id* id);
   void  (*remove)(struct pci_dev* dev);
};


/*
 * Note that this is static variable.  Maybe everything below should be in
 * separate compat_pci.c file, but currently only user of this file is vmxnet,
 * and vmxnet has only one file, so it is fine.  Also with vmxnet all
 * functions below are called just once, so difference between 'inline' and
 * separate compat_pci.c should be very small.
 */

static struct vmw_pci_driver_instance *pci_driver_instances = NULL;

#ifdef KERNEL_2_1
#define vmw_pci_device(instance) (instance)->pcidev
#else
#define vmw_pci_device(instance) (instance)
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * pci_register_driver --
 *
 *      Create driver instances for all matching PCI devices in the box.
 *
 * Results:
 *      Returns 0 for success, negative error value for failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static inline int
pci_register_driver(struct pci_driver *drv)
{
   const struct pci_device_id *chipID;

   for (chipID = drv->id_table; chipID->vendor; chipID++) {
#ifdef KERNEL_2_1
      struct pci_dev *pdev;

      for (pdev = NULL;
           (pdev = pci_find_device(chipID->vendor, chipID->device, pdev)) != NULL; ) {
#else
      int adapter;
      unsigned char bus, devfn, irq;

      for (adapter = 0;
           pcibios_find_device(chipID->vendor, chipID->device, adapter,
                               &bus, &devfn) == 0;
           adapter++) {
#endif
         struct vmw_pci_driver_instance *pdi;
         int err;

         pdi = kmalloc(sizeof *pdi, GFP_KERNEL);
         if (!pdi) {
            printk(KERN_ERR "Not enough memory.\n");
            break;
         }
         pdi->pcidrv = drv;
#ifdef KERNEL_2_1
         pdi->pcidev = pdev;
#else
         pdi->bus = bus;
         pdi->devfn = devfn;
         if (pci_read_config_byte(pdi, PCI_INTERRUPT_LINE, &irq)) {
            pdi->irq = -1;
         } else {
            pdi->irq = irq;
         }
#endif
         pdi->driver_data = NULL;
         pdi->next = pci_driver_instances;
         pci_driver_instances = pdi;
         err = drv->probe(vmw_pci_device(pdi), chipID);
         if (err) {
            pci_driver_instances = pdi->next;
            kfree(pdi);
         }
      }
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * compat_pci_unregister_driver --
 *
 *      Shut down PCI driver - unbind all device instances from driver.
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
pci_unregister_driver(struct pci_driver *drv)
{
   struct vmw_pci_driver_instance **ppdi;

   ppdi = &pci_driver_instances;
   while (1) {
      struct vmw_pci_driver_instance *pdi = *ppdi;

      if (!pdi) {
         break;
      }
      if (pdi->pcidrv == drv) {
         drv->remove(vmw_pci_device(pdi));
         *ppdi = pdi->next;
         kfree(pdi);
      } else {
         ppdi = &pdi->next;
      }
   }
}
#else
/* provide PCI_DEVICE for early 2.4.x kernels */
#ifndef PCI_DEVICE
#define PCI_DEVICE(vend, dev)   .vendor = (vend), .device = (dev), \
                                .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#endif
#endif


/* provide dummy MODULE_DEVICE_TABLE for 2.0/2.2 */
#ifndef MODULE_DEVICE_TABLE
#define MODULE_DEVICE_TABLE(bus, devices)
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * pci_set_drvdata --
 *
 *      Set per-device driver's private data.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *
 * pci_get_drvdata --
 *
 *      Retrieve per-device driver's private data.
 *
 * Results:
 *      per-device driver's data previously set by pci_set_drvdata,
 *      or NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#ifndef KERNEL_2_1
/* 2.0.x is simple, we have driver_data directly in pci_dev */
#define pci_set_drvdata(pdev, data) do { (pdev)->driver_data = (data); } while (0)
#define pci_get_drvdata(pdev)       (pdev)->driver_data
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
/* 2.2.x is trickier, we have to find driver instance first */
static inline void
pci_set_drvdata(struct pci_dev *pdev, void* data)
{
   struct vmw_pci_driver_instance *pdi;

   for (pdi = pci_driver_instances; pdi; pdi = pdi->next) {
      if (pdi->pcidev == pdev) {
         pdi->driver_data = data;
         return;
      }
   }
   printk(KERN_ERR "pci_set_drvdata issued for unknown device %p\n", pdev);
}

static inline void *
pci_get_drvdata(struct pci_dev *pdev)
{
   struct vmw_pci_driver_instance *pdi;

   for (pdi = pci_driver_instances; pdi; pdi = pdi->next) {
      if (pdi->pcidev == pdev) {
         return pdi->driver_data;
      }
   }
   printk(KERN_ERR "pci_get_drvdata issued for unknown device %p\n", pdev);
   return NULL;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,48)
#   define PCI_DMA_BIDIRECTIONAL        0
#   define PCI_DMA_TODEVICE             1
#   define PCI_DMA_FROMDEVICE           2
#   define PCI_DMA_NONE                 3
#endif

/*
 * Power Management related compat wrappers.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 10)
#   define compat_pci_save_state(pdev)      pci_save_state((pdev), NULL)
#   define compat_pci_restore_state(pdev)   pci_restore_state((pdev), NULL)
#else
#   define compat_pci_save_state(pdev)      pci_save_state((pdev))
#   define compat_pci_restore_state(pdev)   pci_restore_state((pdev))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
#   define pm_message_t          u32
#   define compat_pci_choose_state(pdev, state)  (state)
#   define PCI_D0               0
#   define PCI_D3hot            3
#else
#   define compat_pci_choose_state(pdev, state)  pci_choose_state((pdev), (state))
#endif

/* 2.6.14 changed the PCI shutdown callback */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
#   define COMPAT_PCI_SHUTDOWN(func)               .driver = { .shutdown = (func), }
#   define COMPAT_PCI_DECLARE_SHUTDOWN(func, var)  (func)(struct device *(var))
#   define COMPAT_PCI_TO_DEV(dev)                  (to_pci_dev(dev))
#else
#   define COMPAT_PCI_SHUTDOWN(func)               .shutdown = (func)
#   define COMPAT_PCI_DECLARE_SHUTDOWN(func, var)  (func)(struct pci_dev *(var))
#   define COMPAT_PCI_TO_DEV(dev)                  (dev)
#endif

/* 2.6.26 introduced the device_set_wakeup_enable() function */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#   define compat_device_set_wakeup_enable(dev, val) do{}while(0)
#else
#   define compat_device_set_wakeup_enable(dev, val) \
       device_set_wakeup_enable(dev, val)
#endif

#endif /* __COMPAT_PCI_H__ */
