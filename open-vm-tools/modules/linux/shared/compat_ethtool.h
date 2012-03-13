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

#ifndef _COMPAT_ETHTOOL_H
#define _COMPAT_ETHTOOL_H

/*
 * ethtool is a userspace utility for getting and setting ethernet device
 * settings. Kernel support for it was first published in 2.4.0-test11, but
 * only in 2.4.15 were the ethtool_value struct and the ETHTOOL_GLINK ioctl
 * added to ethtool.h (together, because the ETHTOOL_GLINK ioctl expects a 
 * single value response).
 *
 * Likewise, ioctls for getting and setting TSO were published in 2.4.22.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#   include <linux/ethtool.h>

#   ifndef ETHTOOL_GLINK
#      define ETHTOOL_GLINK 0x0a

typedef struct {
	__u32 cmd;
	__u32 data;
} compat_ethtool_value;

#   else

typedef struct ethtool_value compat_ethtool_value;
#   endif

#   ifndef ETHTOOL_GTSO
#      define ETHTOOL_GTSO 0x1E
#      define ETHTOOL_STSO 0x1F
#   endif
#endif

#if COMPAT_LINUX_VERSION_CHECK_LT(3, 3, 0)
#   define compat_ethtool_rxfh_indir_default(i, num_queues) (i % num_queues)
#else
#   define compat_ethtool_rxfh_indir_default(i, num_queues) ethtool_rxfh_indir_default(i, num_queues)
#endif

#endif /* _COMPAT_ETHTOOL_H */
