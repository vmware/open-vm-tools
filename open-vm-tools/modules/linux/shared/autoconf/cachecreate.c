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

#include "compat_version.h"
#include "compat_autoconf.h"

/*
 * All kernels before 2.6.22 take 6 arguments.  All kernels since
 * 2.6.23-rc1 take 5 arguments.  Only kernels between 2.6.22 and
 * 2.6.23-rc1 are questionable - we could ignore them if we wanted,
 * nobody cares about them even now.  But unfortunately RedHat is
 * re-releasing 2.6.X-rc kernels under 2.6.(X-1) name, so they
 * are releasing 2.6.23-rc1 as 2.6.22-5055-something, so we have
 * to do autodetection for them.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)
/* Success... */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#error "This test intentionally fails on 2.6.23 and newer kernels."
#else
#include <linux/slab.h>

struct kmem_cache *kmemtest(void) {
   return kmem_cache_create("test", 12, 0, 0, NULL, NULL);
}
						
#endif
