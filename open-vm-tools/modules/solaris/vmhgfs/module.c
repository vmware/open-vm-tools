/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * Driver module struct hierarchy:
 * -------------------------------
 *
 * (Solaris 9 & 10)
 *
 *  modlinkage --> modldrv --> dev_ops --> cb_ops
 *
 *  - modlinkage & modldrv: module loading/unloading
 *  - dev_ops: operations for driver configuration (attach(), detach(), etc)
 *  - cb_ops: operations on device, entry points to character device (open(),
 *            close(), etc)
 *
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
 *
 * Combination:
 * ------------
 *
 * In this file we are placing both modldrv and modlfs inside one modlinkage
 * structure.  This is achieved by placing pointers to both of these structures
 * in modlinkage's ml_linkage arary of pointers to module structures.  This is
 * done so the two modules can have shared state and to prevent dependency
 * issues between them. This looks like:
 *
 * modlinkage --> modldrv --> as above
 *            \
 *             -> modlfs --> as above
 *
 */

#include "hgfsSolaris.h"
#include "module.h"
#include "debug.h"


/*
 * Macros
 */
#define HGFS_VFSSW_FLAGS        0


/*
 * Structures
 */

/*
 * Device driver structures
 */

/* Structure defining operations on device */
static struct cb_ops HgfsDevCBOps = {
   HgfsDevOpen,         /* open(9E) */
   HgfsDevClose,        /* close(9E) */
   nodev,               /* strategy(9E) */
   nodev,               /* print(9E) */
   nodev,               /* dump(9E) */
   HgfsDevRead,         /* read(9E) */
   HgfsDevWrite,        /* write(9E) */
   nodev,               /* ioctl(9E) */
   nodev,               /* devmap(9E) */
   nodev,               /* mmap(9E) */
   nodev,               /* segmap(9E) */
   HgfsDevChpoll,       /* chpoll(9E) */
   ddi_prop_op,         /* prop_op(9E) */
   NULL,                /* streamtab(9S) */
   (D_NEW | D_MP),      /* cb_flag: D_NEW because this is a new-style
                                    driver and D_MP is required */
   CB_REV,              /* cb_rev */
   nodev,               /* aread(9E) */
   nodev,               /* awrite(9E) */
};


/* Structure defining operations for driver */
static struct dev_ops HgfsDevOps = {
   DEVO_REV,            /* devo_rev */
   0,                   /* devo_refcnt */
   HgfsDevGetinfo,      /* getinfo(9E) */
   nulldev,             /* identify(9E) */
   nulldev,             /* probe(9E) */
   HgfsDevAttach,       /* attach(9E) */
   HgfsDevDetach,       /* detach(9E) */
   nodev,               /* devo_reset */
   &HgfsDevCBOps,       /* devo_cb_ops (points to struct cb_ops) */
   NULL,                /* devo_bus_ops */
   NULL                 /* power(9E) */
};


/* Driver module linkage structure */
static struct modldrv HgfsDevModldrv = {
   &mod_driverops,                      /* drv_modops (don't change) */
   "HGFS Device Interface",             /* drv_linkinfo */
   &HgfsDevOps                          /* drv_dev_ops (points to struct dev_ops) */
};



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
 * Modlinkage shared by device driver and filesystem.
 */
static struct modlinkage HgfsModlinkage = {
   MODREV_1,            /* Module revision: must be MODREV_1 */
   /*
    * We put pointers to /both/ modules in the following NULL terminated array
    * of pointers (ml_linkage[]).  This is what causes both modules to be
    * loaded into the kernel when the single driver is added.
    */
   {
      &HgfsDevModldrv,  /* Driver module structure */
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

   /*
    * Initialize hgfsInstance so filesystem knows device has not been attached
    * yet, and hgfsType so we know whether to free the vfsops in _fini().
    */
   hgfsInstance = HGFS_INSTANCE_UNINITIALIZED;
   hgfsType = HGFS_TYPE_UNINITIALIZED;

   /*
    * For reference:
    *
    * Solaris provides an interface to simplify allocation and deallocation of
    * per-driver-instance state through the DDI Soft State functions.
    *
    * - ddi_soft_state_init() takes a double pointer and uses it as a handle
    *   to access the various per-instance state structures.  This handle will
    *   be passed to the other DDI Soft State functions to indicate which
    *   state list to operate on.  The second argument is the amount of memory
    *   to allocate for each state structure, and the last is the expected number
    *   of instances of the driver (number of state structures).
    *
    * - ddi_soft_state_zalloc() and ddi_soft_state_free() allocate and free a
    *   soft state structure respectively.  The first argument is the
    *   handle pointer (see above) and the second is the item number
    *   associated with this instance.
    *
    * - ddi_get_soft_state_free() takes a handle and item and returns a
    *   pointer to the state structure that was allocated with
    *   ddi_soft_state_zalloc() for that item number.
    *
    *   See man ddi_soft_state_init for more information.
    *
    */
   ret = ddi_soft_state_init(&superInfoHead,
                             sizeof (HgfsSuperInfo), HGFS_EXPECTED_INSTANCES);
   if (ret) {
      goto error;
   }

   ret = mod_install(&HgfsModlinkage);
   if (ret) {
      goto error;
   }

   DEBUG(VM_DEBUG_DONE, "_init() done.\n");
   return 0;

error:
   cmn_err(HGFS_ERROR, "could not install HGFS module.\n");
   ddi_soft_state_fini(&superInfoHead);
   return ret;
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
   HgfsSuperInfo *sip;
   int error;

   DEBUG(VM_DEBUG_ENTRY, "_fini() for HGFS.\n");

   sip = ddi_get_soft_state(superInfoHead, hgfsInstance);

   /*
    * Make sure the device is closed and the fs is not mounted if we have
    * a valid superinfo pointer.
    */
   if (sip && (sip->devOpen || sip->vfsp)) {
      DEBUG(VM_DEBUG_FAIL, "Cannot unload module because either device"
            "is open or file system is mounted\n");
      return -1;
   }

   error = mod_remove(&HgfsModlinkage);
   if (error) {
      cmn_err(HGFS_ERROR, "could not remove HGFS module.\n");
      return error;
   }

#if HGFS_VFS_VERSION > 2
   if (hgfsType != HGFS_TYPE_UNINITIALIZED) {
      vfs_freevfsops_by_type(hgfsType);
   }
   if (sip && sip->vnodeOps) {
      vn_freevnodeops(sip->vnodeOps);
   }
#endif
   ddi_soft_state_fini(&superInfoHead);   /* void return */

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
