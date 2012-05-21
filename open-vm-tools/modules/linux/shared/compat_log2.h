/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_LOG2_H__
#   define __COMPAT_LOG2_H__

#ifndef LINUX_VERSION_CODE
#   error "Include compat_version.h before compat_log2.h"
#endif

/* linux/log2.h was introduced in 2.6.20. */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
#   include <linux/log2.h>
#endif

/*
 * is_power_of_2 was introduced in 2.6.21. This implementation is almost
 * identical to the one found there.
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#define compat_is_power_of_2(n) is_power_of_2(n)
#else
static inline __attribute__((const))
int compat_is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n && (n - 1)) == 0));
}
#endif

/*
 * rounddown_power_of_two was introduced in 2.6.24. This implementation is
 * similar to the one in log2.h but with input of int instead of long to
 * avoid more version related checks for fls_long().
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#define compat_rounddown_pow_of_two(n) rounddown_pow_of_two(n)
#else
static inline __attribute__((const))
unsigned int compat_rounddown_pow_of_two(unsigned int n)
{
	return 1U << (fls(n) -1);
}
#endif

#endif /* __COMPAT_LOG2_H__ */
