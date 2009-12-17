/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_MUTEX_H__
#   define __COMPAT_MUTEX_H__


/* Blocking mutexes were introduced in 2.6.16. */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)

#include "compat_semaphore.h"

typedef struct semaphore compat_mutex_t;

# define compat_define_mutex(_mx)               DECLARE_MUTEX(_mx)
# define compat_mutex_init(_mx)                 init_MUTEX(_mx)
# define compat_mutex_lock(_mx)                 down(_mx)
# define compat_mutex_lock_interruptible(_mx)   down_interruptible(_mx)
# define compat_mutex_unlock(_mx)               up(_mx)

#else

#include <linux/mutex.h>

typedef struct mutex compat_mutex_t;

# define compat_define_mutex(_mx)               DEFINE_MUTEX(_mx)
# define compat_mutex_init(_mx)                 mutex_init(_mx)
# define compat_mutex_lock(_mx)                 mutex_lock(_mx)
# define compat_mutex_lock_interruptible(_mx)   mutex_lock_interruptible(_mx)
# define compat_mutex_unlock(_mx)               mutex_unlock(_mx)

#endif

#endif /* __COMPAT_MUTEX_H__ */
