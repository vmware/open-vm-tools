/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_FREEBSD_H__
#define __COMPAT_FREEBSD_H__ 1

#include <sys/param.h>
#include "compat_vop.h"
#include "compat_mount.h"
#include "compat_priv.h"

/*
 * FreeBSD version 8 and above uses the kproc API instead of the kthread API in its
 * kernel.
 */
#if __FreeBSD_version > 800001
#define compat_kthread_create kproc_create
#define compat_kthread_exit kproc_exit
#else
#define compat_kthread_create kthread_create
#define compat_kthread_exit kthread_exit
#endif

#endif // __COMPAT_FREEBSD_H__
