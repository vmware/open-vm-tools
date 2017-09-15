/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_CRED_H__
#   define __COMPAT_CRED_H__


/*
 * Include linux/cred.h via linux/sched.h - it is not nice, but
 * as cpp does not have #ifexist...
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
#include <linux/sched.h>
#else
#include <linux/cred.h>
#endif

#if !defined(current_fsuid) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
#define current_uid() (current->uid)
#define current_euid() (current->euid)
#define current_fsuid() (current->fsuid)
#define current_gid() (current->gid)
#define current_egid() (current->egid)
#define current_fsgid() (current->fsgid)
#endif

#if !defined(cap_set_full)
/* cap_set_full was removed in kernel version 3.0-rc4. */
#define cap_set_full(_c) do { (_c) = CAP_FULL_SET; } while (0)
#endif

#if !defined(GLOBAL_ROOT_UID)
#define GLOBAL_ROOT_UID (0)
#endif

#endif /* __COMPAT_CRED_H__ */
