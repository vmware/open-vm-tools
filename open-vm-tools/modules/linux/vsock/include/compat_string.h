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

#ifndef __COMPAT_STRING_H__
#   define __COMPAT_STRING_H__

#include <linux/string.h>

/*
 * kstrdup was born in 2.6.13. This implementation is almost identical to the
 * one found there.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
#define compat_kstrdup(s, gfp) kstrdup(s, gfp)
#else
#define compat_kstrdup(s, gfp)                                                \
({                                                                            \
   size_t len;                                                                \
   char *buf;                                                                 \
   len = strlen(s) + 1;                                                       \
   buf = kmalloc(len, gfp);                                                   \
   memcpy(buf, s, len);                                                       \
   buf;                                                                       \
})                                                                            
#endif

#endif /* __COMPAT_STRING_H__ */
