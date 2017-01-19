/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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

/*
 * dentry.c --
 *
 * Dentry operations for the filesystem portion of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include "compat_fs.h"
#include "compat_kernel.h"
#include "compat_namei.h"
#include "compat_version.h"

#include "inode.h"
#include "module.h"
#include "vm_assert.h"

/* HGFS dentry operations. */
static int HgfsDentryRevalidate(struct dentry *dentry,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
                                unsigned int flags
#else
                                struct nameidata *nd
#endif
);

/* HGFS dentry operations structure. */
struct dentry_operations HgfsDentryOperations = {
   .d_revalidate     = HgfsDentryRevalidate,
};

/*
 * HGFS dentry operations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsDentryRevalidate --
 *
 *    Called by namei.c every time a dentry is looked up in the dcache
 *    to determine if it is still valid.
 *
 *    If the entry is found to be invalid, namei calls dput on it and
 *    returns NULL, which causes a new lookup to be done in the actual
 *    filesystem, which in our case means that HgfsLookup is called.
 *
 * Results:
 *    Positive value if the entry IS valid.
 *    Zero if the entry is NOT valid.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsDentryRevalidate(struct dentry *dentry,  // IN: Dentry to revalidate
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
                     unsigned int flags      // IN: Lookup flags & intent
#else
                     struct nameidata *nd    // IN: Lookup flags & intent
#endif
)
{
   int error;
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDentryRevalidate: calling "
           "HgfsRevalidate\n"));

   ASSERT(dentry);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
   if (flags & LOOKUP_RCU) {
      return -ECHILD;
   }
#elif defined(LOOKUP_RCU) /* Introduced in 2.6.38 */
   if (nd && (nd->flags & LOOKUP_RCU)) {
      return -ECHILD;
   }
#endif

   /* Just call HgfsRevaliate, which does the right thing. */
   error = HgfsRevalidate(dentry);
   if (error) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDentryRevalidate: invalid\n"));

      if (dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode)) {
         shrink_dcache_parent(dentry);
      }
      d_drop(dentry);

      return 0;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDentryRevalidate: valid\n"));
   return 1;
}
