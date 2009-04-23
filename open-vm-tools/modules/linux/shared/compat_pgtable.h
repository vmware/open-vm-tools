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

#ifndef __COMPAT_PGTABLE_H__
#   define __COMPAT_PGTABLE_H__


#if defined(CONFIG_PARAVIRT) && defined(CONFIG_HIGHPTE)
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 21)
#      include <asm/paravirt.h>
#      undef paravirt_map_pt_hook
#      define paravirt_map_pt_hook(type, va, pfn) do {} while (0)
#   endif
#endif
#include <asm/pgtable.h>


/* pte_page() API modified in 2.3.23 to return a struct page * --hpreg */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 23)
#   define compat_pte_page pte_page
#else
#   include "compat_page.h"

#   define compat_pte_page(_pte) virt_to_page(pte_page(_pte))
#endif


/* Appeared in 2.5.5 --hpreg */
#ifndef pte_offset_map
/*  Appeared in SuSE 8.0's 2.4.18 --hpreg */
#   ifdef pte_offset_atomic
#      define pte_offset_map pte_offset_atomic
#      define pte_unmap pte_kunmap
#   else
#      define pte_offset_map pte_offset
#      define pte_unmap(_pte)
#   endif
#endif


/* Appeared in 2.5.74-mmX --petr */
#ifndef pmd_offset_map
#   define pmd_offset_map(pgd, address) pmd_offset(pgd, address)
#   define pmd_unmap(pmd)
#endif


/*
 * Appeared in 2.6.10-rc2-mm1.  Older kernels did L4 page tables as 
 * part of pgd_offset, or they did not have L4 page tables at all.
 * In 2.6.11 pml4 -> pgd -> pmd -> pte hierarchy was replaced by
 * pgd -> pud -> pmd -> pte hierarchy.
 */
#ifdef PUD_MASK
#   define compat_pgd_offset(mm, address)   pgd_offset(mm, address)
#   define compat_pgd_present(pgd)          pgd_present(pgd)
#   define compat_pud_offset(pgd, address)  pud_offset(pgd, address)
#   define compat_pud_present(pud)          pud_present(pud)
typedef pgd_t  compat_pgd_t;
typedef pud_t  compat_pud_t;
#elif defined(pml4_offset)
#   define compat_pgd_offset(mm, address)   pml4_offset(mm, address)
#   define compat_pgd_present(pml4)         pml4_present(pml4)
#   define compat_pud_offset(pml4, address) pml4_pgd_offset(pml4, address)
#   define compat_pud_present(pgd)          pgd_present(pgd)
typedef pml4_t compat_pgd_t;
typedef pgd_t  compat_pud_t;
#else
#   define compat_pgd_offset(mm, address)   pgd_offset(mm, address)
#   define compat_pgd_present(pgd)          pgd_present(pgd)
#   define compat_pud_offset(pgd, address)  (pgd)
#   define compat_pud_present(pud)          (1)
typedef pgd_t  compat_pgd_t;
typedef pgd_t  compat_pud_t;
#endif


#define compat_pgd_offset_k(mm, address) pgd_offset_k(address)


/* Introduced somewhere in 2.6.0, + backported to some 2.4 RedHat kernels */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0) && !defined(pte_pfn)
#   define pte_pfn(pte) page_to_pfn(compat_pte_page(pte))
#endif


/* A page_table_lock field is added to struct mm_struct in 2.3.10 --hpreg */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 10)
#   define compat_get_page_table_lock(_mm) (&(_mm)->page_table_lock)
#else
#   define compat_get_page_table_lock(_mm) NULL
#endif


/*
 * Define VM_PAGE_KERNEL_EXEC for vmapping executable pages.
 *
 * On ia32 PAGE_KERNEL_EXEC was introduced in 2.6.8.1.  Unfortunately it accesses
 * __PAGE_KERNEL_EXEC which is not exported for modules.  So we use
 * __PAGE_KERNEL and just cut _PAGE_NX bit from it.
 *
 * For ia32 kernels before 2.6.8.1 we use PAGE_KERNEL directly, these kernels
 * do not have noexec support.
 *
 * On x86-64 situation is a bit better: they always supported noexec, but
 * before 2.6.8.1 flag was named PAGE_KERNEL_EXECUTABLE, and it was renamed
 * to PAGE_KERNEL_EXEC when ia32 got noexec too (see above).
 */
#ifdef CONFIG_X86
#ifdef _PAGE_NX
#define VM_PAGE_KERNEL_EXEC __pgprot(__PAGE_KERNEL & ~_PAGE_NX)
#else
#define VM_PAGE_KERNEL_EXEC PAGE_KERNEL
#endif
#else
#ifdef PAGE_KERNEL_EXECUTABLE
#define VM_PAGE_KERNEL_EXEC PAGE_KERNEL_EXECUTABLE
#else
#define VM_PAGE_KERNEL_EXEC PAGE_KERNEL_EXEC
#endif
#endif


#endif /* __COMPAT_PGTABLE_H__ */
