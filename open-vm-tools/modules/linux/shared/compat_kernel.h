/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_KERNEL_H__
#   define __COMPAT_KERNEL_H__

#include <asm/unistd.h>
#include <linux/kernel.h>

/*
 * container_of was introduced in 2.5.28 but it's easier to check like this.
 */
#ifndef container_of
#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

/*
 * vsnprintf became available in 2.4.10. For older kernels, just fall back on
 * vsprintf.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 10)
#define vsnprintf(str, size, fmt, args) vsprintf(str, fmt, args)
#endif

#endif /* __COMPAT_KERNEL_H__ */
