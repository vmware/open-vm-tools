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

#ifndef __COMPAT_PAGEMAP_H__
#   define __COMPAT_PAGEMAP_H__


#include <linux/pagemap.h>

/*
 * AOP_FLAG_NOFS was defined in the same changeset that
 * grab_cache_page_write_begin() was introduced.
 */
#ifdef AOP_FLAG_NOFS
#define compat_grab_cache_page_write_begin(mapping, index, flags) \
         grab_cache_page_write_begin((mapping), (index), (flags))
#else
#define compat_grab_cache_page_write_begin(mapping, index, flags) \
         __grab_cache_page((mapping), (index));
#endif

#endif /* __COMPAT_PAGEMAP_H__ */
