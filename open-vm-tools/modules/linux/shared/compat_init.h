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
 * compat_init.h: Initialization compatibility wrappers.
 */

#ifndef __COMPAT_INIT_H__
#define __COMPAT_INIT_H__

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 2, 0)
#include <linux/init.h>
#endif

#ifndef module_init
#define module_init(x) int init_module(void)     { return x(); }
#endif

#ifndef module_exit
#define module_exit(x) void cleanup_module(void) { x(); }
#endif

#endif /* __COMPAT_INIT_H__ */
