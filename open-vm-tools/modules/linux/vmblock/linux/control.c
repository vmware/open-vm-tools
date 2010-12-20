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
 * control.c --
 *
 *   Control operations for the vmblock driver.
 *
 */

#include "driver-config.h"
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/fs.h>

#include <asm/uaccess.h>

#include "vmblockInt.h"
#include "block.h"


/* procfs initialization/cleanup functions */
static int SetupProcDevice(void);
static int CleanupProcDevice(void);

/* procfs entry file operations */
ssize_t ControlFileOpWrite(struct file *filp, const char __user *buf,
                           size_t cmd, loff_t *ppos);
static int ControlFileOpRelease(struct inode *inode, struct file *file);


static struct proc_dir_entry *controlProcDirEntry;
struct file_operations ControlFileOps = {
   .owner   = THIS_MODULE,
   .write   = ControlFileOpWrite,
   .release = ControlFileOpRelease,
};


/* Public initialization/cleanup routines */

/*
 *----------------------------------------------------------------------------
 *
 * VMBlockInitControlOps --
 *
 *    Sets up state for control operations.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VMBlockInitControlOps(void)
{
   int ret;

   ret = BlockInit();
   if (ret < 0) {
      Warning("VMBlockInitControlOps: could not initialize blocking ops.\n");
      return ret;
   }

   ret = SetupProcDevice();
   if (ret < 0) {
      Warning("VMBlockInitControlOps: could not setup proc device.\n");
      BlockCleanup();
      return ret;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockCleanupControlOps --
 *
 *    Cleans up state for control operations.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VMBlockCleanupControlOps(void)
{
   int ret;

   ret = CleanupProcDevice();
   if (ret < 0) {
      Warning("VMBlockCleanupControlOps: could not cleanup proc device.\n");
      return ret;
   }

   BlockCleanup();
   return 0;
}


/* Private initialization/cleanup routines */


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockSetProcEntryOwner --
 *
 *    Sets proc_dir_entry owner if necessary:
 *
 *    Before version 2.6.24 kernel prints nasty warning when in-use
 *    directory entry is destroyed, which happens when module is unloaded.
 *    We try to prevent this warning in most cases by setting owner to point
 *    to our module, so long operations (like current directory pointing to
 *    directory we created) prevent module from unloading.  Since 2.6.24 this
 *    situation is handled without nastygrams, allowing module unload even
 *    when current directory points to directory created by unloaded module,
 *    so we do not have to set owner anymore.  And since 2.6.29 we must not
 *    set owner at all, as there is none...
 *
 * Results:
 *    None.  Always succeeds.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
VMBlockSetProcEntryOwner(struct proc_dir_entry *entry) // IN/OUT: directory entry
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
   entry->owner = THIS_MODULE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * SetupProcDevice --
 *
 *    Adds entries to /proc used to control file blocks.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
SetupProcDevice(void)
{
   struct proc_dir_entry *controlProcEntry;
   struct proc_dir_entry *controlProcMountpoint;

   /* Create /proc/fs/vmblock */
   controlProcDirEntry = proc_mkdir(VMBLOCK_CONTROL_PROC_DIRNAME, NULL);
   if (!controlProcDirEntry) {
      Warning("SetupProcDevice: could not create /proc/"
              VMBLOCK_CONTROL_PROC_DIRNAME "\n");
      return -EINVAL;
   }

   VMBlockSetProcEntryOwner(controlProcDirEntry);

   /* Create /proc/fs/vmblock/mountPoint */
   controlProcMountpoint = proc_mkdir(VMBLOCK_CONTROL_MOUNTPOINT,
                                      controlProcDirEntry);
   if (!controlProcMountpoint) {
      Warning("SetupProcDevice: could not create "
              VMBLOCK_MOUNT_POINT "\n");
      remove_proc_entry(VMBLOCK_CONTROL_PROC_DIRNAME, NULL);
      return -EINVAL;
   }

   VMBlockSetProcEntryOwner(controlProcMountpoint);

   /* Create /proc/fs/vmblock/dev */
   controlProcEntry = create_proc_entry(VMBLOCK_CONTROL_DEVNAME,
                                        VMBLOCK_CONTROL_MODE,
                                        controlProcDirEntry);
   if (!controlProcEntry) {
      Warning("SetupProcDevice: could not create " VMBLOCK_DEVICE "\n");
      remove_proc_entry(VMBLOCK_CONTROL_MOUNTPOINT, controlProcDirEntry);
      remove_proc_entry(VMBLOCK_CONTROL_PROC_DIRNAME, NULL);
      return -EINVAL;
   }

   controlProcEntry->proc_fops = &ControlFileOps;
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * CleanupProcDevice --
 *
 *    Removes /proc entries for controlling file blocks.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
CleanupProcDevice(void)
{
   if (controlProcDirEntry) {
      remove_proc_entry(VMBLOCK_CONTROL_MOUNTPOINT, controlProcDirEntry);
      remove_proc_entry(VMBLOCK_CONTROL_DEVNAME, controlProcDirEntry);
      remove_proc_entry(VMBLOCK_CONTROL_PROC_DIRNAME, NULL);
   }
   return 0;
}


/* procfs file operations */


/*
 *----------------------------------------------------------------------------
 *
 * ExecuteBlockOp --
 *
 *    Copy block name from user buffer into kernel space, canonicalize it
 *    by removing all trailing path separators, and execute desired block
 *    operation.
 *
 * Results:
 *    0 on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
ExecuteBlockOp(const char __user *buf,                // IN: buffer with name
               const os_blocker_id_t blocker,         // IN: blocker ID (file)
               int (*blockOp)(const char *filename,   // IN: block operation
                              const os_blocker_id_t blocker))
{
   char *name;
   int i;
   int retval;

   name = getname(buf);
   if (IS_ERR(name)) {
      return PTR_ERR(name);
   }

   for (i = strlen(name) - 1; i >= 0 && name[i] == '/'; i--) {
      name[i] = '\0';
   }

   retval = i < 0 ? -EINVAL : blockOp(name, blocker);

   putname(name);

   return retval;
}

/*
 *----------------------------------------------------------------------------
 *
 * ControlFileOpWrite --
 *
 *    write implementation for our control file.  This accepts either add or
 *    delete commands and the buffer contains the file to block.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

ssize_t
ControlFileOpWrite(struct file *file,       // IN: Opened file, used for ID
                   const char __user *buf,  // IN: NUL-terminated filename
                   size_t cmd,              // IN: VMBlock command (usually count)
                   loff_t *ppos)            // IN/OUT: File offset (unused)
{
   int ret;

   switch (cmd) {
   case VMBLOCK_ADD_FILEBLOCK:
      ret = ExecuteBlockOp(buf, file, BlockAddFileBlock);
      break;

   case VMBLOCK_DEL_FILEBLOCK:
      ret = ExecuteBlockOp(buf, file, BlockRemoveFileBlock);
      break;

#ifdef VMX86_DEVEL
   case VMBLOCK_LIST_FILEBLOCKS:
      BlockListFileBlocks();
      ret = 0;
      break;
#endif

   default:
      Warning("ControlFileOpWrite: unrecognized command (%u) recieved\n",
              (unsigned)cmd);
      ret = -EINVAL;
      break;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * ControlFileOpRelease --
 *
 *    Called when the file is closed.
 *
 * Results:
 *    Zero on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
ControlFileOpRelease(struct inode *inode,  // IN
                     struct file *file)    // IN
{
   BlockRemoveAllBlocks(file);
   return 0;
}
