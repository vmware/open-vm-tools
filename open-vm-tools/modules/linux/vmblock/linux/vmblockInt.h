/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * vmblockInt.h --
 *
 *   Definitions and prototypes for entire module.
 *
 *   The module is split into two halves, a control half and a file system
 *   half, and the halves communicate through the blocking functionality in
 *   block.c.  The control half creates a device node for a user space program
 *   (running as root) to add and delete blocks on files in the file system's
 *   namespace.  The file system provides links to the contents of the
 *   directory it is redirecting to and blocks according to the file blocks set
 *   through the control half.
 */

#ifndef __VMBLOCKINT_H__
#define __VMBLOCKINT_H__

#include <linux/version.h>
#include <linux/mm.h>

#include "vmblock.h"
#include "vm_basic_types.h"
#include "vm_assert.h"

#ifdef __KERNEL__
#ifdef VMX86_DEVEL
extern int LOGLEVEL_THRESHOLD;
#  define LOG(level, fmt, args...)                              \
     ((void) (LOGLEVEL_THRESHOLD >= (level) ?                   \
              printk(KERN_DEBUG "VMBlock: " fmt, ## args) :     \
              0)                                                \
     )
#else
#  define LOG(level, fmt, args...)
#endif
#define Warning(fmt, args...)                                   \
     printk(KERN_WARNING "VMBlock warning: " fmt, ## args)
/*
 * Some kernel versions, bld-2.4.21-32.EL_x86_64-ia32e-RHEL3 and perhaps more,
 * don't define __user in uaccess.h, so let's do it here so we don't have to
 * ifdef all the __user annotations.
 */
#ifndef __user
#define __user
#endif
#endif /* __KERNEL__ */

#define VMBLOCK_CONTROL_MODE       S_IRUSR | S_IFREG

/*
 * Our modules may be compatible with kernels built for different processors.
 * This can cause problems, so we add a reference to the __alloc_pages symbol
 * below since it is versioned per-processor and will cause modules to only
 * load on kernels built for the same processor as our module.
 *
 * XXX This should go in driver-config.h, but vmmon's hostKernel.h is retarded.
 */
static const void *forceProcessorCheck __attribute__((unused)) = __alloc_pages;


/*
 * Initialization and cleanup routines for control and file system halves of
 * vmblock driver
 */
int VMBlockInitControlOps(void);
int VMBlockCleanupControlOps(void);
int VMBlockInitFileSystem(char const *root);
int VMBlockCleanupFileSystem(void);

#endif /* __VMBLOCK_H__ */
