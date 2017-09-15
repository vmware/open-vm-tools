/*********************************************************
 * Copyright (C) 2004-2016 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * module.c --
 *
 *      Structures and modules for loading and unloading of the HGFS module.
 *
 */


/*
 * For reference: Solaris Structures
 * =================================
 *
 * VFS module struct hierarchy:
 * ----------------------------
 *
 * (Solaris 9)
 *
 * modlinkage --> modlfs --> mod_ops
 *                       \
 *                        -> vfssw --> vfsops
 *                                 \
 *                                  -> (*fs_init_routine)()
 *
 *
 * modlfs: - points to module loading/unloading operations structure (mod_ops)
 *         - points to VFS Switch strucutre (vfssw)
 *         - contains extended name of filesystem
 *
 * mod_ops: - contains pointers to _init(), _fini(), and _info(), which handle
 *            loading/unloading module into kernel and providing information
 *
 * vfssw: - points to filesystem initialization routine that is called once at
 *          module load time (not mount time)
 *        - points to vfsops struct that points to fs-specific operations
 *        - contains name of fs (what you would put in /etc/vfstab)
 *        - also contains fs mount options, flags, and mutex
 *
 * vfsops: - points to fs-level functions (mount(), umount(), etc.)
 *
 * (Solaris 10)
 *
 * modlinkage --> modlfs --> mod_ops
 *                       \
 *                        -> vfsdef_v2 --> (*init)()
 *
 * vfsdef_v2:  - this contains a pointer to an initialization routine for the
 *               filesystem that takes different arguments
 *             - we no longer provide an address to a vfsops; now we need to
 *               call vfs_makevfsops() with a preconstructed array of
 *               fs_operation_t that defines each vfs op.  This needs to occur
 *               in the initialization routine.
 *
 * (Build 58 contains the same structure named vfsdev_v3)
 *
 */

#include "hgfsSolaris.h"
#include "module.h"
#include "vnode.h"
#include "filesystem.h"
#include "debug.h"


/*
 * Macros
 */
#define HGFS_VFSSW_FLAGS        0


/*
 * Filesystem structures
 */

#if HGFS_VFS_VERSION == 2
/* VFS Operations Structure */
struct vfsops HgfsVfsOps = {
   HgfsMount,           /* vfs_mount() */
   HgfsUnmount,         /* vfs_unmount() */
   HgfsRoot,            /* vfs_root() */
   HgfsStatvfs,         /* vfs_statvfs() */
   HgfsSync,            /* vfs_sync() */
   HgfsVget,            /* vfs_vget() */
   HgfsMountroot,       /* vfs_mountroot() */
   HgfsReserved,        /* vfs_reserved() */
   HgfsFreevfs          /* vfs_freevfs() */
};


/* VFS Switch structure */
static struct vfssw HgfsVfsSw = {
   HGFS_FS_NAME,        /* Name of filesystem */
   HgfsInit,            /* Initialization routine */
   &HgfsVfsOps,         /* VFS Operations struct */
   HGFS_VFSSW_FLAGS,    /* Flags: see <sys/vfs.h> */
   NULL,                /* Mount options table prototype */
   1,                   /* Count of references */
   { { 0 } }            /* Lock to protect count */
};
#else

/*
 * Different beta builds of Solaris have different versions of this structure.
 * We currently don't support v4 which was out in intermediate beta builds of
 * Solaris 11. Instead we choose to update to the latest revision. But if needed,
 * v4 could be added without significant effort.
 */
#if HGFS_VFS_VERSION == 2
static struct vfsdef_v2 HgfsVfsDef = {
#elif HGFS_VFS_VERSION == 3
static struct vfsdef_v3 HgfsVfsDef = {
#else
static struct vfsdef_v5 HgfsVfsDef = {
#endif
   VFSDEF_VERSION,      /* Structure version: defined in <sys/vfs.h> */
   HGFS_FS_NAME,        /* Name of filesystem */
   HgfsInit,            /* Initialization routine: note this is a different
                         * routine than the one used in 9 */
   HGFS_VFSSW_FLAGS,    /* Filesystem flags */
   NULL                 /* No mount options */
};

#endif  /* Solaris version */


/* Filesystem module structure */
static struct modlfs HgfsModlfs = {
   &mod_fsops,                  /* Module operation structure: for
                                   auto loading/unloading */
   "Host/Guest Filesystem",     /* Name */
#if HGFS_VFS_VERSION== 2
   &HgfsVfsSw                   /* VFS Switch structure */
#else
   &HgfsVfsDef                  /* Filesystem type definition record */
#endif
};


/*
 * Modlinkage containing filesystem.
 */
static struct modlinkage HgfsModlinkage = {
   MODREV_1,            /* Module revision: must be MODREV_1 */
   {
      &HgfsModlfs,      /* FS module structure */
      NULL,             /* NULL terminator */
   }
};


/*
 * Driver autoload functions
 */


/*
 *----------------------------------------------------------------------------
 *
 * _init --
 *    Invoked when module is being loaded into kernel, and is called before
 *    any function in the module.  Any state that spans all instances of the
 *    driver should be allocated and initialized here.
 *
 * Results:
 *    Returns the result of mod_install(9F), which is zero on success and a
 *    non-zero value on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
_init(void)
{
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "_init() for HGFS.\n");

   ret = mod_install(&HgfsModlinkage);
   if (ret) {
      cmn_err(HGFS_ERROR, "could not install HGFS module.\n");
      return ret;
   }

   DEBUG(VM_DEBUG_DONE, "_init() done.\n");
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * _fini --
 *    Invoked when a module is being removed from the kernel.
 *
 * Results:
 *    Returns the result of mod_remove(9F), which is zero on success, and a
 *    non-zero value on failure.
 *
 * Side effects:
 *    The module will be removed from the kernel.
 *
 *----------------------------------------------------------------------------
 */

int
_fini(void)
{
   int error;

   DEBUG(VM_DEBUG_ENTRY, "_fini() for HGFS.\n");

   /*
    * Make sure that the fs is not mounted.
    */
   if (HgfsGetSuperInfo()) {
      DEBUG(VM_DEBUG_FAIL,
            "Cannot unload module because file system is mounted\n");
      return EBUSY;
   }

   error = mod_remove(&HgfsModlinkage);
   if (error) {
      cmn_err(HGFS_ERROR, "could not remove HGFS module.\n");
      return error;
   }

   HgfsFreeVnodeOps();
   HgfsFreeVfsOps();

   DEBUG(VM_DEBUG_DONE, "_fini() done.\n");
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * _info --
 *
 *    Invoked when the modinfo(1M) command is executed.  mod_info(9F) handles
 *    this for us.
 *
 * Results:
 *    Returns mod_info(9F)'s results, which are a non-zero value on success, and
 *    zero on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
_info(struct modinfo *modinfop) // OUT: Filled in with module's information by
{
   DEBUG(VM_DEBUG_ENTRY, "_info().\n");
   ASSERT(modinfop);

   if (!modinfop) {
        cmn_err(HGFS_ERROR, "NULL input in _info\n");
        return EINVAL;
   }

   return mod_info(&HgfsModlinkage, modinfop);
}
