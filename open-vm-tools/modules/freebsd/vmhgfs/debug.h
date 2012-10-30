/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * debug.h --
 *
 *      Macros and includes for debugging.
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <sys/param.h>

#if defined __FreeBSD__
#  include <sys/types.h>          // for log(9)
#  include <sys/systm.h>          // for log(9)
#  include <sys/syslog.h>         // for log(9), LOG_* macros
#elif defined __APPLE__
#  include <kern/debug.h>         // for panic
#  if defined VMX86_DEVEL
#    include <pexpert/pexpert.h>  // for kprintf
#  endif
#endif

#include <sys/vnode.h>            // for struct vattr

#include "hgfs_kernel.h"

/*
 * Macros
 */

#define Panic(fmt, ...)         panic(fmt, ##__VA_ARGS__)

#define VM_DEBUG_ALWAYS         (1)
#define VM_DEBUG_FAIL	        VM_DEBUG_ALWAYS
#define VM_DEBUG_NOTSUP         VM_DEBUG_ALWAYS
#define VM_DEBUG_ENTRY          (1 << 1)
#define VM_DEBUG_EXIT           (1 << 2)
#define VM_DEBUG_LOAD	        (1 << 3)
#define VM_DEBUG_INFO           (1 << 4)
#define VM_DEBUG_STRUCT         (1 << 5)
#define VM_DEBUG_LIST           (1 << 6)
#define VM_DEBUG_CHPOLL         (1 << 7)
#define VM_DEBUG_RARE           (1 << 8)
#define VM_DEBUG_COMM           (1 << 9)
#define VM_DEBUG_REQUEST        (1 << 10)
#define VM_DEBUG_LOG            (1 << 11)
#define VM_DEBUG_ATTR           (1 << 12)
#define VM_DEBUG_DEVENTRY       (1 << 13)
#define VM_DEBUG_DEVDONE        (1 << 14)
#define VM_DEBUG_SIG            (1 << 15)
#define VM_DEBUG_ERROR          (1 << 16)
#define VM_DEBUG_HSHTBL         (1 << 17)
#define VM_DEBUG_HANDLE         (1 << 18)
#define VM_DEBUG_STATE          (1 << 19)
#define VM_DEBUG_VNODE          (1 << 20)
#define VM_DEBUG_ALL            (~0)

#if defined VMX86_DEVEL
#define VM_DEBUG_LEV (VM_DEBUG_ALWAYS | VM_DEBUG_ENTRY | VM_DEBUG_EXIT | VM_DEBUG_FAIL)
#endif

#ifdef VM_DEBUG_LEV
#  if defined __FreeBSD__
#    define DEBUG(type, fmt, ...)                                         \
               ((type & VM_DEBUG_LEV) ?                                   \
                (log(LOG_NOTICE, "%s:%u: " fmt,                           \
                     __func__, __LINE__, ##__VA_ARGS__))                  \
                : 0)
#  elif defined __APPLE__
#    define DEBUG(type, fmt, ...)                             \
                 HgfsDebugPrint(type, __func__, __LINE__, fmt, ##__VA_ARGS__)
#  endif
#else
#  define DEBUG(type, ...)
#endif

/*
 * Global functions
 */

void HgfsDebugPrint(int type, const char *funcname, unsigned int linenum, const char *fmt, ...);
void HgfsDebugPrintVattr(const HgfsVnodeAttr *vap);
void HgfsDebugPrintOperation(HgfsKReqHandle req);

#endif // _DEBUG_H_
