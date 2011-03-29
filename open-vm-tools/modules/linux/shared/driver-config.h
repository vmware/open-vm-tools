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

/*
 * Sets the proper defines from the Linux header files
 *
 * This file must be included before the inclusion of any kernel header file,
 * with the exception of linux/autoconf.h and linux/version.h --hpreg
 */

#ifndef __VMX_CONFIG_H__
#define __VMX_CONFIG_H__

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMKDRIVERS
#include "includeCheck.h"

#include "compat_version.h"
#include "compat_autoconf.h"

/*
 * We rely on Kernel Module support.  Check here.
 */
#ifndef CONFIG_MODULES
#   error "No Module support in this kernel.  Please configure with CONFIG_MODULES"
#endif

/*
 * 2.2 kernels still use __SMP__ (derived from CONFIG_SMP
 * in the main Makefile), so we do it here.
 */

#ifdef CONFIG_SMP
#   define __SMP__ 1
#endif

#if defined(CONFIG_MODVERSIONS) && defined(KERNEL_2_1)
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,60)
/*
 * MODVERSIONS might be already defined when using kernel's Makefiles.
 */
#      ifndef MODVERSIONS
#         define MODVERSIONS
#      endif
#      include <linux/modversions.h>
#   endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
/*
 * Force the uintptr_t definition to come from linux/types.h instead of vm_basic_types.h.
 */
#   include <linux/types.h>
#   define _STDINT_H 1
#endif

#ifndef __KERNEL__
#   define __KERNEL__
#endif

#endif
