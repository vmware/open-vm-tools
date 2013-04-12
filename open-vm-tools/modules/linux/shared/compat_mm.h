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

#ifndef __COMPAT_MM_H__
#   define __COMPAT_MM_H__


#include <linux/mm.h>


/* 2.2.x uses 0 instead of some define */
#ifndef NOPAGE_SIGBUS
#define NOPAGE_SIGBUS (0)
#endif


/* 2.2.x does not have HIGHMEM support */
#ifndef GFP_HIGHUSER
#define GFP_HIGHUSER (GFP_USER)
#endif


/*
 * In 2.4.14, the logic behind the UnlockPage macro was moved to the
 * unlock_page() function. Later (in 2.5.12), the UnlockPage macro was removed
 * altogether, and nowadays everyone uses unlock_page().
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 14)
#define compat_unlock_page(page) UnlockPage(page)
#else
#define compat_unlock_page(page) unlock_page(page)
#endif


#endif /* __COMPAT_MM_H__ */
