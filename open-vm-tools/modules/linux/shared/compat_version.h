/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_VERSION_H__
#   define __COMPAT_VERSION_H__

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMKDRIVERS
#include "includeCheck.h"


#ifndef __linux__
#   error "linux-version.h"
#endif


#include <linux/version.h>

#ifndef KERNEL_VERSION
#   error KERNEL_VERSION macro is not defined, environment is busted
#endif


/*
 * Distinguish relevant classes of Linux kernels.
 *
 * The convention is that version X defines all
 * the KERNEL_Y symbols where Y <= X.
 *
 * XXX Do not add more definitions here. This way of doing things does not
 *     scale, and we are going to phase it out soon --hpreg
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 1, 0)
#   define KERNEL_2_1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 2, 0)
#   define KERNEL_2_2
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 1)
#   define KERNEL_2_3_1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 15)
/*   new networking */
#   define KERNEL_2_3_15
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 25)
/*  new procfs */
#   define KERNEL_2_3_25
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 29)
/*  even newer procfs */
#   define KERNEL_2_3_29
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 43)
/*  softnet changes */
#   define KERNEL_2_3_43
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 47)
/*  more softnet changes */
#   define KERNEL_2_3_47
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 99)
/*  name in netdevice struct is array and not pointer */
#   define KERNEL_2_3_99
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
/*  New 'owner' member at the beginning of struct file_operations */
#      define KERNEL_2_4_0
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 8)
/*  New netif_rx_ni() --hpreg */
#   define KERNEL_2_4_8
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 2)
/*  New kdev_t, major()/minor() API --hpreg */
#   define KERNEL_2_5_2
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 5)
/*  New sk_alloc(), pte_offset_map()/pte_unmap() --hpreg */
#   define KERNEL_2_5_5
#endif

/* Linux kernel 3.0 can be called 2.6.40, and 3.1 can be 2.6.41...
 * Use COMPAT_LINUX_VERSION_CHECK_LT iff you need to compare running kernel to
 * versions 3.0 and above.
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
   /* Straight forward comparison if kernel version is 3.0.0 and beyond */
#   define COMPAT_LINUX_VERSION_CHECK_LT(a, b, c) LINUX_VERSION_CODE < KERNEL_VERSION (a, b, c)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 40)
   /* Use b of the check to calculate corresponding c of kernel
    *  version to compare */
#   define COMPAT_LINUX_VERSION_CHECK_LT(a, b, c) LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, (b + 40))
#else
    /* This is anyways lesser than any 3.x versions */
#   define COMPAT_LINUX_VERSION_CHECK_LT(a, b, c) 1
#endif

#endif /* __COMPAT_VERSION_H__ */
