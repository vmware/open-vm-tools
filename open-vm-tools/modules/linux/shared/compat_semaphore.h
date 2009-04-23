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

#ifndef __COMPAT_SEMAPHORE_H__
#   define __COMPAT_SEMAPHORE_H__


/* <= 2.6.25 have asm only, 2.6.26 has both, and 2.6.27-rc2+ has linux only. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
#   include <asm/semaphore.h>
#else
#   include <linux/semaphore.h>
#endif


/*
* The init_MUTEX_LOCKED() API appeared in 2.2.18, and is also in
* 2.2.17-21mdk --hpreg
*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
   #ifndef init_MUTEX_LOCKED
      #define init_MUTEX_LOCKED(_sem) *(_sem) = MUTEX_LOCKED
   #endif
   #ifndef DECLARE_MUTEX
      #define DECLARE_MUTEX(name) struct semaphore name = MUTEX
   #endif
   #ifndef DECLARE_MUTEX_LOCKED
      #define DECLARE_MUTEX_LOCKED(name) struct semaphore name = MUTEX_LOCKED
   #endif
#endif


#endif /* __COMPAT_SEMAPHORE_H__ */
