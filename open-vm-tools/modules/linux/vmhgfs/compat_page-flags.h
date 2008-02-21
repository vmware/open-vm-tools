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

#ifndef __COMPAT_PAGE_FLAGS_H__
#   define __COMPAT_PAGE_FLAGS_H__

/* No page-flags.h prior to 2.5.12. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 12)
#   include <linux/page-flags.h>
#endif

/* 
 * The pgoff_t type was introduced in 2.5.20, but we'll look for it by 
 * definition since it's more convenient. Note that we want to avoid a
 * situation where, in the future, a #define is changed to a typedef, 
 * so if pgoff_t is not defined in some future kernel, we won't define it.
 */
#if !defined(pgoff_t) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#define pgoff_t unsigned long
#endif

/*
 * set_page_writeback() was introduced in 2.6.6. Prior to that, callers were
 * using the SetPageWriteback() macro directly, so that's what we'll use.
 * Prior to 2.5.12, the writeback bit didn't exist, so we don't need to do
 * anything.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 12)
#define compat_set_page_writeback(page)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 6)
#define compat_set_page_writeback(page) SetPageWriteback(page)
#else
#define compat_set_page_writeback(page) set_page_writeback(page)
#endif

/*
 * end_page_writeback() was introduced in 2.5.12. Prior to that, it looks like
 * there was no page writeback bit, and everything the function accomplished
 * was done by unlock_page(), so we'll define it out.
 *
 * Note that we could just #define end_page_writeback to nothing and avoid 
 * needing the compat_ prefix, but this is more complete with respect to
 * compat_set_page_writeback. 
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 12)
#define compat_end_page_writeback(page)
#else
#define compat_end_page_writeback(page) end_page_writeback(page)
#endif

#endif /* __COMPAT_PAGE_FLAGS_H__ */
