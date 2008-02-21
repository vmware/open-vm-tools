/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 *   Dentry operations for the file system of the vmblock driver.
 *
 */

#include "driver-config.h"
#include "compat_fs.h"
#include "compat_namei.h"

#include "vmblockInt.h"
#include "filesystem.h"
#include "block.h"


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
static int DentryOpRevalidate(struct dentry *dentry, struct nameidata *nd);
#else
static int DentryOpRevalidate(struct dentry *dentry, int flags);
#endif

struct dentry_operations LinkDentryOps = {
   .d_revalidate = DentryOpRevalidate,
};


/*
 *----------------------------------------------------------------------------
 *
 * DentryOpRevalidate --
 *
 *    This function is invoked every time the dentry is accessed from the cache
 *    to ensure it is still valid.  We use it to block since any threads
 *    looking up this dentry after the initial lookup should still block if the
 *    block has not been cleared.
 *
 * Results:
 *    1 if the dentry is valid, 0 if it is not.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
static int
DentryOpRevalidate(struct dentry *dentry,  // IN: dentry revalidating
                   struct nameidata *nd)   // IN: lookup flags & intent
#else
static int
DentryOpRevalidate(struct dentry *dentry,  // IN: dentry revalidating
                   int flags)              // IN: lookup flags (e.g., LOOKUP_CONTINUE)
#endif
{
   VMBlockInodeInfo *iinfo;
   struct nameidata actualNd;
   struct dentry *actualDentry;
   int ret;

   if (!dentry || !dentry->d_inode) {
      Warning("DentryOpRevalidate: invalid args from kernel\n");
      return 0;
   }

   iinfo = INODE_TO_IINFO(dentry->d_inode);
   if (!iinfo) {
      Warning("DentryOpRevalidate: dentry has no fs-specific data\n");
      return 0;
   }

   /* Block if there is a pending block on this file */
   BlockWaitOnFile(iinfo->name, NULL);

   /*
    * If the actual dentry has a revalidate function, we'll let it figure out
    * whether the dentry is still valid.  If not, do a path lookup to ensure
    * that the file still exists.
    */
   actualDentry = iinfo->actualDentry;

   if (actualDentry &&
       actualDentry->d_op &&
       actualDentry->d_op->d_revalidate) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
      return actualDentry->d_op->d_revalidate(actualDentry, nd);
#else
      return actualDentry->d_op->d_revalidate(actualDentry, flags);
#endif
   }

   if (compat_path_lookup(iinfo->name, 0, &actualNd)) {
      LOG(4, "DentryOpRevalidate: [%s] no longer exists\n", iinfo->name);
      return 0;
   }
   ret = actualNd.dentry && actualNd.dentry->d_inode;
   path_release(&actualNd);

   LOG(8, "DentryOpRevalidate: [%s] %s revalidated\n",
       iinfo->name, ret ? "" : "not");
   return ret;
}
