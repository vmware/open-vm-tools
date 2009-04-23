/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 *      Main loading and unloading of kernel module.
 */

#include <sys/errno.h>

#include "module.h"
#include "block.h"

static vfsdef_t VMBlockVfsDef = {
   VFSDEF_VERSION,         /* Structure version: defined in <sys/vfs.h> */
   VMBLOCK_FS_NAME,        /* Name of file system */
   VMBlockInit,            /* File system initialization routine */
   VMBLOCK_VFSSW_FLAGS,    /* File system flags */
   NULL                    /* No mount options */
};

/* Filesystem module structure */
static struct modlfs VMBlockModlfs = {
   &mod_fsops,             /* Module operation structure: for
                            * auto loading/unloading */
   "VMBlock File system",  /* Name */
   &VMBlockVfsDef          /* Filesystem type definition record */
};

static struct modlinkage VMBlockModlinkage = {
   MODREV_1,            /* Module revision: must be MODREV_1 */
   {
      &VMBlockModlfs,   /* FS module structure */
      NULL,
   }
};

#ifdef VMX86_DEBUG
/* XXX: Figure out how to pass this in at module load time. */
int LOGLEVEL = 4;
#else
int LOGLEVEL = 0;
#endif

int vmblockType;
vnodeops_t *vmblockVnodeOps;




/*
 * Module loading/unloading/info functions.
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
   int error;

   error = mod_install(&VMBlockModlinkage);
   if (error) {
      Warning("Could not install vmblock module.\n");
      return error;
   }

   error = BlockInit();
   if (error) {
      Warning("Could not initialize blocking.\n");
      mod_remove(&VMBlockModlinkage);
      return error;
   }

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

   error = mod_remove(&VMBlockModlinkage);
   if (error) {
      Warning("Could not remove vmblock module.\n");
      return error;
   }

   BlockCleanup();
   vfs_freevfsops_by_type(vmblockType);
   vn_freevnodeops(vmblockVnodeOps);

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
_info(struct modinfo *modinfop) // OUT: Filled in with module's information
{
   return mod_info(&VMBlockModlinkage, modinfop);
}
