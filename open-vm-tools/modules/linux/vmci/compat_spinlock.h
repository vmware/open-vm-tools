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


/*
 * The spin_lock() API appeared in 2.1.25 in asm/smp_lock.h
 * It moved in 2.1.30 to asm/spinlock.h
 * It moved again in 2.3.18 to linux/spinlock.h
 *
 *   --hpreg
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 18)
#   include <linux/spinlock.h>
#else
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 1, 30)
#      include <asm/spinlock.h>
#   else
typedef struct {} spinlock_t;
#      define spin_lock_init(lock)
#      define spin_lock(lock)
#      define spin_unlock(lock)
#      define spin_lock_irqsave(lock, flags) do {      \
                    save_flags(flags);                 \
                    cli();                             \
                    spin_lock(lock);                   \
                 } while (0)
#      define spin_unlock_irqrestore(lock, flags) do { \
                    spin_unlock(lock);                 \
                    restore_flags(flags);              \
                 } while (0)
#   endif
#endif


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


#endif /* __COMPAT_SPINLOCK_H__ */
