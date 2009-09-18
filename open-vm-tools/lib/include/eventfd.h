/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * eventfd.h --
 *
 *    eventfd interface.  Use only if platform does not have
 *    its own.
 */

#ifndef EVENTFD_H
#define EVENTFD_H

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"

#include <fcntl.h>
#include <errno.h>

typedef uint64 eventfd_t;

#define EFD_CLOEXEC  O_CLOEXEC
#define EFD_NONBLOCK O_NONBLOCK

/* Only Linux eventfd implementation is available. */
#if defined(__linux__) && !defined(N_PLAT_NLM)
#  define VMWARE_EVENTFD_REAL

int eventfd(int count, int flags);
int eventfd_read(int fd, eventfd_t *value);
int eventfd_write(int fd, eventfd_t value);

#else

static INLINE int
eventfd(int count, int flags)
{
   errno = ENOSYS;
   return -1;
}

static INLINE int
eventfd_read(int fd, eventfd_t *value)
{
   errno = ENOSYS;
   return -1;
}

static INLINE int
eventfd_write(int fd, eventfd_t value)
{
   errno = ENOSYS;
   return -1;
}

#endif

#endif /* EVENTFD_H */
