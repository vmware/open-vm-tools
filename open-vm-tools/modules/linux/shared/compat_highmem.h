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

#ifndef __COMPAT_HIGHMEM_H__
#   define __COMPAT_HIGHMEM_H__


/*
 *  BIGMEM  (4 GB)         support appeared in 2.3.16: kmap() API added
 *  HIGHMEM (4 GB + 64 GB) support appeared in 2.3.23: kmap() API modified
 *  In 2.3.27, kmap() API modified again
 *
 *   --hpreg
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 27)
#   include <linux/highmem.h>
#else
/* For page_address --hpreg */
#   include <linux/pagemap.h>

#   define kmap(_page) (void*)page_address(_page)
#   define kunmap(_page)
#endif

#endif /* __COMPAT_HIGHMEM_H__ */
