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


/* The get_page() API appeared in 2.3.7 --hpreg */
/* Sometime during development it became function instead of macro --petr */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0) && !defined(get_page) 
#   define get_page(_page) atomic_inc(&(_page)->count)
/* The __free_page() API is exported in 2.1.67 --hpreg */
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 1, 67)
#      define put_page __free_page
#   else
#      include "compat_page.h"

#      define page_to_phys(_page) (page_to_pfn(_page) << PAGE_SHIFT)
#      define put_page(_page) free_page(page_to_phys(_page))
#   endif
#endif


/* page_count() is 2.4.0 invention. Unfortunately unavailable in some RedHat 
 * kernels (for example 2.4.21-4-RHEL3). */
/* It is function since 2.6.0, and hopefully RedHat will not play silly games
 * with mm_inline.h again... */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0) && !defined(page_count)
#  define page_count(page) atomic_read(&(page)->count)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#  define compat_vm_pgoff(vma) ((vma)->vm_offset >> PAGE_SHIFT)

static inline unsigned long compat_do_mmap_pgoff(struct file *file, unsigned long addr,
   unsigned long len, unsigned long prot,
   unsigned long flag, unsigned long pgoff)
{
   unsigned long ret = -EINVAL;

   if (pgoff < 1 << (32 - PAGE_SHIFT)) {
      ret = do_mmap(file, addr, len, prot, flag, pgoff << PAGE_SHIFT);
   }
   return ret;
}

#else
#  define compat_vm_pgoff(vma) (vma)->vm_pgoff
#  ifdef VMW_SKAS_MMAP
#    define compat_do_mmap_pgoff(f, a, l, p, g, o) \
				do_mmap_pgoff(current->mm, f, a, l, p, g, o)
#  else
#    define compat_do_mmap_pgoff(f, a, l, p, g, o) \
				do_mmap_pgoff(f, a, l, p, g, o)
#  endif
#endif


/* 2.2.x uses 0 instead of some define */
#ifndef NOPAGE_SIGBUS
#define NOPAGE_SIGBUS (0)
#endif


/* 2.2.x does not have HIGHMEM support */
#ifndef GFP_HIGHUSER
#define GFP_HIGHUSER (GFP_USER)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)

#include "compat_page.h"

static inline struct page * alloc_pages(unsigned int gfp_mask, unsigned int order)
{
   unsigned long addr;
   
   addr = __get_free_pages(gfp_mask, order);
   if (!addr) {
      return NULL;
   }
   return virt_to_page(addr);
}
#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

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

/*
 * In 2.4.10, vmtruncate was changed from returning void to returning int.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 10)
#define compat_vmtruncate(inode, size)                                        \
({                                                                            \
   int result = 0;                                                            \
   vmtruncate(inode, size);                                                   \
   result;                                                                    \
})
#else
#define compat_vmtruncate(inode, size) vmtruncate(inode, size)
#endif


#endif /* __COMPAT_MM_H__ */
