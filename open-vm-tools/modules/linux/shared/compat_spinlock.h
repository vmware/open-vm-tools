/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_SPINLOCK_H__
#   define __COMPAT_SPINLOCK_H__

#include <linux/spinlock.h>

/*
 * Preempt support was added during 2.5.x development cycle, and later
 * it was backported to 2.4.x.  In 2.4.x backport these definitions
 * live in linux/spinlock.h, that's why we put them here (in 2.6.x they
 * are defined in linux/preempt.h which is included by linux/spinlock.h).
 */
#ifdef CONFIG_PREEMPT
#define compat_preempt_disable() preempt_disable()
#define compat_preempt_enable()  preempt_enable()
#else
#define compat_preempt_disable() do { } while (0)
#define compat_preempt_enable()  do { } while (0)
#endif

/* Some older kernels - 2.6.10 and earlier - lack DEFINE_SPINLOCK */
#ifndef DEFINE_SPINLOCK
#define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED
#endif

/* Same goes for DEFINE_RWLOCK */
#ifndef DEFINE_RWLOCK
#define DEFINE_RWLOCK(x)   rwlock_t x = RW_LOCK_UNLOCKED
#endif

#endif /* __COMPAT_SPINLOCK_H__ */
