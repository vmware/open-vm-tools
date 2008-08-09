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
 * wait_for_completion and friends did not exist before 2.4.9.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 9)

#define compat_complete_and_exit(comp, status) complete_and_exit(comp, status)

#else

#include "compat_completion.h"

/*
 * Used by _syscallX macros. Note that this is global variable, so
 * do not rely on its contents too much. As exit() is only function
 * we use, and we never check return value from exit(), we have
 * no problem...
 */
extern int errno;

/*
 * compat_exit() provides an access to the exit() function. It must 
 * be named compat_exit(), as exit() (with different signature) is 
 * provided by x86-64, arm and other (but not by i386).
 */
#define __NR_compat_exit __NR_exit
static inline _syscall1(int, compat_exit, int, exit_code);

/*
 * See compat_wait_for_completion in compat_completion.h.
 * compat_exit implicitly performs an unlock_kernel, in resident code,
 * ensuring that the thread is no longer running in module code when the
 * module is unloaded.
 */
#define compat_complete_and_exit(comp, status) do { \
   lock_kernel(); \
   compat_complete(comp); \
   compat_exit(status); \
} while (0)

#endif

/*
 * vsnprintf became available in 2.4.10. For older kernels, just fall back on
 * vsprintf.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 10)
#define vsnprintf(str, size, fmt, args) vsprintf(str, fmt, args)
#endif

#endif /* __COMPAT_KERNEL_H__ */
