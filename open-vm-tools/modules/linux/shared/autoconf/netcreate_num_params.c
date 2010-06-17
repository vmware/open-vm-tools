/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * During 2.6.33 merge window net_proto_ops->create() method was changed -
 * a new 'kern' field, signalling whether socket is being created by kernel
 * or userspace application, was added to it. Unfortunately, some
 * distributions, such as RHEL 6, have backported the change to earlier
 * kernels, so we can't rely solely on kernel version to determine number of
 * arguments.
 */

#include "compat_version.h"
#include "compat_autoconf.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#   error This compile test intentionally fails.
#else

#include <linux/net.h>

static int TestCreate(struct net *net,
                      struct socket *sock, int protocol,
                      int kern)
{
   return 0;
}

struct net_proto_family testFamily = {
   .create = TestCreate,
};

#endif
