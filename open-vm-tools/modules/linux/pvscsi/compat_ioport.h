/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_IOPORT_H__
#   define __COMPAT_IOPORT_H__


#include <linux/ioport.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
static inline void *
compat_request_region(unsigned long start, unsigned long len, const char *name)
{
   if (check_region(start, len)) {
      return NULL;
   }
   request_region(start, len, name);
   return (void*)1;
}
#else
#define compat_request_region(start, len, name) request_region(start, len, name)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 7)
/* mmap io support starts from 2.3.7, fail the call for kernel prior to that */
static inline void *
compat_request_mem_region(unsigned long start, unsigned long len, const char *name)
{
   return NULL;
}

static inline void
compat_release_mem_region(unsigned long start, unsigned long len)
{
   return;
}
#else
#define compat_request_mem_region(start, len, name) request_mem_region(start, len, name)
#define compat_release_mem_region(start, len)       release_mem_region(start, len)
#endif

/* these two macro defs are needed by compat_pci_request_region */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 3, 15)
#   define IORESOURCE_IO    0x00000100
#   define IORESOURCE_MEM   0x00000200
#endif

#endif /* __COMPAT_IOPORT_H__ */
