/*********************************************************
 * Copyright (C) 2002,2014 VMware, Inc. All rights reserved.
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

#ifndef __PGTBL_H__
#   define __PGTBL_H__


#include <linux/highmem.h>

#include "compat_pgtable.h"
#include "compat_spinlock.h"
#include "compat_page.h"

/*
 *-----------------------------------------------------------------------------
 *
 * PgtblPte2MPN --
 *
 *    Returns the page structure associated to a Page Table Entry.
 *
 *    This function is not allowed to schedule() because it can be called while
 *    holding a spinlock --hpreg
 *
 * Results:
 *    INVALID_MPN on failure
 *    mpn         on success
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE MPN
PgtblPte2MPN(pte_t *pte)   // IN
{
   MPN mpn;
   if (pte_present(*pte) == 0) {
      return INVALID_MPN;
   }
   mpn = pte_pfn(*pte);
   if (mpn >= INVALID_MPN) {
      return INVALID_MPN;
   }
   return mpn;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblPte2Page --
 *
 *    Returns the page structure associated to a Page Table Entry.
 *
 *    This function is not allowed to schedule() because it can be called while
 *    holding a spinlock --hpreg
 *
 * Results:
 *    The page structure if the page table entry points to a physical page
 *    NULL if the page table entry does not point to a physical page
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE struct page *
PgtblPte2Page(pte_t *pte) // IN
{
   if (pte_present(*pte) == 0) {
      return NULL;
   }

   return compat_pte_page(*pte);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblPGD2PTELocked --
 *
 *    Walks through the hardware page tables to try to find the pte
 *    associated to a virtual address.
 *
 * Results:
 *    pte. Caller must call pte_unmap if valid pte returned.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE pte_t *
PgtblPGD2PTELocked(compat_pgd_t *pgd,    // IN: PGD to start with
                   VA addr)              // IN: Address in the virtual address
                                         //     space of that process
{
   compat_pud_t *pud;
   pmd_t *pmd;
   pte_t *pte;

   if (compat_pgd_present(*pgd) == 0) {
      return NULL;
   }

   pud = compat_pud_offset(pgd, addr);
   if (compat_pud_present(*pud) == 0) {
      return NULL;
   }

   pmd = pmd_offset_map(pud, addr);
   if (pmd_present(*pmd) == 0) {
      pmd_unmap(pmd);
      return NULL;
   }

   pte = pte_offset_map(pmd, addr);
   pmd_unmap(pmd);
   return pte;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblVa2PTELocked --
 *
 *    Walks through the hardware page tables to try to find the pte
 *    associated to a virtual address.
 *
 * Results:
 *    pte. Caller must call pte_unmap if valid pte returned.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE pte_t *
PgtblVa2PTELocked(struct mm_struct *mm, // IN: Mm structure of a process
                  VA addr)              // IN: Address in the virtual address
                                        //     space of that process
{
   return PgtblPGD2PTELocked(compat_pgd_offset(mm, addr), addr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblVa2MPNLocked --
 *
 *    Retrieve MPN for a given va.
 *
 *    Caller must call pte_unmap if valid pte returned. The mm->page_table_lock
 *    must be held, so this function is not allowed to schedule() --hpreg
 *
 * Results:
 *    INVALID_MPN on failure
 *    mpn         on success
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE MPN
PgtblVa2MPNLocked(struct mm_struct *mm, // IN: Mm structure of a process
                  VA addr)              // IN: Address in the virtual address
{
   pte_t *pte;

   pte = PgtblVa2PTELocked(mm, addr);
   if (pte != NULL) {
      MPN mpn = PgtblPte2MPN(pte);
      pte_unmap(pte);
      return mpn;
   }
   return INVALID_MPN;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
/*
 *-----------------------------------------------------------------------------
 *
 * PgtblKVa2MPNLocked --
 *
 *    Retrieve MPN for a given kernel va.
 *
 *    Caller must call pte_unmap if valid pte returned. The mm->page_table_lock
 *    must be held, so this function is not allowed to schedule() --hpreg
 *
 * Results:
 *    INVALID_MPN on failure
 *    mpn         on success
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE MPN
PgtblKVa2MPNLocked(struct mm_struct *mm, // IN: Mm structure of a caller
                   VA addr)              // IN: Address in the virtual address
{
   pte_t *pte;

   pte = PgtblPGD2PTELocked(compat_pgd_offset_k(mm, addr), addr);
   if (pte != NULL) {
      MPN mpn = PgtblPte2MPN(pte);
      pte_unmap(pte);
      return mpn;
   }
   return INVALID_MPN;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblVa2PageLocked --
 *
 *    Return the "page" struct for a given va.
 *
 * Results:
 *    struct page or NULL.  The mm->page_table_lock must be held, so this 
 *    function is not allowed to schedule() --hpreg
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE struct page *
PgtblVa2PageLocked(struct mm_struct *mm, // IN: Mm structure of a process
                   VA addr)              // IN: Address in the virtual address
{
   pte_t *pte;

   pte = PgtblVa2PTELocked(mm, addr);
   if (pte != NULL) {
      struct page *page = PgtblPte2Page(pte);
      pte_unmap(pte);
      return page;
   } else {
      return NULL;
   }
} 


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblVa2MPN --
 *
 *    Walks through the hardware page tables of the current process to try to
 *    find the page structure associated to a virtual address.
 *
 * Results:
 *    Same as PgtblVa2MPNLocked()
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE MPN
PgtblVa2MPN(VA addr)  // IN
{
   struct mm_struct *mm;
   MPN mpn;

   /* current->mm is NULL for kernel threads, so use active_mm. */
   mm = current->active_mm;
   if (compat_get_page_table_lock(mm)) {
      spin_lock(compat_get_page_table_lock(mm));
   }
   mpn = PgtblVa2MPNLocked(mm, addr);
   if (compat_get_page_table_lock(mm)) {
      spin_unlock(compat_get_page_table_lock(mm));
   }
   return mpn;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
/*
 *-----------------------------------------------------------------------------
 *
 * PgtblKVa2MPN --
 *
 *    Walks through the hardware page tables of the current process to try to
 *    find the page structure associated to a virtual address.
 *
 * Results:
 *    Same as PgtblVa2MPNLocked()
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE MPN
PgtblKVa2MPN(VA addr)  // IN
{
   struct mm_struct *mm = current->active_mm;
   MPN mpn;

   if (compat_get_page_table_lock(mm)) {
      spin_lock(compat_get_page_table_lock(mm));
   }
   mpn = PgtblKVa2MPNLocked(mm, addr);
   if (compat_get_page_table_lock(mm)) {
      spin_unlock(compat_get_page_table_lock(mm));
   }
   return mpn;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * PgtblVa2Page --
 *
 *    Walks through the hardware page tables of the current process to try to
 *    find the page structure associated to a virtual address.
 *
 * Results:
 *    Same as PgtblVa2PageLocked()
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE struct page *
PgtblVa2Page(VA addr) // IN
{
   struct mm_struct *mm = current->active_mm;
   struct page *page;

   if (compat_get_page_table_lock(mm)) {
      spin_lock(compat_get_page_table_lock(mm));
   }
   page = PgtblVa2PageLocked(mm, addr);
   if (compat_get_page_table_lock(mm)) {
      spin_unlock(compat_get_page_table_lock(mm));
   }
   return page;
}


#endif /* __PGTBL_H__ */
