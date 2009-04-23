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

#ifndef __COMPAT_PAGE_H__
#   define __COMPAT_PAGE_H__


#include <linux/mm.h>
#include <asm/page.h>


/* The pfn_to_page() API appeared in 2.5.14 and changed to function during 2.6.x */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0) && !defined(pfn_to_page)
#   define pfn_to_page(_pfn) (mem_map + (_pfn))
#   define page_to_pfn(_page) ((_page) - mem_map)
#endif


/* The virt_to_page() API appeared in 2.4.0 --hpreg */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0) && !defined(virt_to_page)
#   define virt_to_page(_kvAddr) pfn_to_page(MAP_NR(_kvAddr))
#endif


/*
 * The get_order() API appeared at some point in 2.3.x, and was then backported
 * in 2.2.17-21mdk and in the stock 2.2.18. Because we can only detect its
 * definition through makefile tricks, we provide our own for now --hpreg
 */
static inline int
compat_get_order(unsigned long size) // IN
{
   int order;

   size = (size - 1) >> (PAGE_SHIFT - 1);
   order = -1;
   do {
      size >>= 1;
      order++;
   } while (size);

   return order;
}

/* 
 * BUG() was added to <asm/page.h> in 2.2.18, and was moved to <asm/bug.h>
 * in 2.5.58.
 * 
 * XXX: Technically, this belongs in some sort of "compat_asm_page.h" file, but
 * since our compatibility wrappers don't distinguish between <asm/xxx.h> and
 * <linux/xxx.h>, putting it here is reasonable.
 */
#ifndef BUG
#define BUG() do {                                                            \
   printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__);                      \
  __asm__ __volatile__(".byte 0x0f,0x0b");                                    \
} while (0)
#endif

#endif /* __COMPAT_PAGE_H__ */
