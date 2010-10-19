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

#ifndef DMA_BIT_MASK
#  define DMA_BIT_MASK(n) DMA_##n##BIT_MASK
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
#   define COMPAT_PCI_SHUTDOWN(func)               .driver = { .shutdown = (func), }
#   define COMPAT_PCI_DECLARE_SHUTDOWN(func, var)  (func)(struct device *(var))
#   define COMPAT_PCI_TO_DEV(dev)                  (to_pci_dev(dev))
#else
#   define COMPAT_PCI_SHUTDOWN(func)               .shutdown = (func)
#   define COMPAT_PCI_DECLARE_SHUTDOWN(func, var)  (func)(struct pci_dev *(var))
#   define COMPAT_PCI_TO_DEV(dev)                  (dev)
#endif

/* 2.6.26 introduced the device_set_wakeup_enable() function */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
#   define compat_device_set_wakeup_enable(dev, val) do {} while(0)
#else
#   define compat_device_set_wakeup_enable(dev, val) \
       device_set_wakeup_enable(dev, val)
#endif

#endif /* __COMPAT_PCI_H__ */
