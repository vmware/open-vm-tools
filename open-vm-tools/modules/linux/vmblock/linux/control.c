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
   ssize_t i;
   char *filename;

#ifdef VMX86_DEVEL
   if (cmd == VMBLOCK_LIST_FILEBLOCKS) {
      BlockListFileBlocks();
      return 0;
   }
#endif

   /*
    * XXX: Can we GPL our modules already?  This is gross.  On kernels 2.6.6
    * through 2.6.12 when CONFIG_AUDITSYSCALL is defined, putname() turns into
    * a macro that calls audit_putname(), which happens to only be exported to
    * GPL modules (until 2.6.9).  Here we work around this by calling
    * __getname() and __putname() to get our path buffer directly,
    * side-stepping the syscall auditing and doing the copy from user space
    * ourself.  Change this back once we GPL the module.
    */
   filename = __getname();
   if (!filename) {
      Warning("ControlFileOpWrite: Could not obtain memory for filename.\n");
      return -ENOMEM;
   }

   /*
    * XXX: __getname() returns a pointer to a PATH_MAX-sized buffer.
    * Hard-coding this size is also gross, but it's our only option here and
    * InodeOpLookup() already set a bad example by doing this.
    */
   ret = strncpy_from_user(filename, buf, PATH_MAX);
   if (ret < 0 || ret >= PATH_MAX) {
      Warning("ControlFileOpWrite: Could not access provided user buffer.\n");
      ret = ret < 0 ? ret : -ENAMETOOLONG;
      goto exit;
   }

   /* Remove all trailing path separators. */
   for (i = ret - 1; i >= 0 && filename[i] == '/'; i--) {
      filename[i] = '\0';
   }

   if (i < 0) {
      ret = -EINVAL;
      goto exit;
   }

   switch (cmd) {
   case VMBLOCK_ADD_FILEBLOCK:
      ret = BlockAddFileBlock(filename, file);
      break;
   case VMBLOCK_DEL_FILEBLOCK:
      ret = BlockRemoveFileBlock(filename, file);
      break;
   default:
      Warning("ControlFileOpWrite: unrecognized command (%u) recieved\n",
              (unsigned)cmd);
      ret = -EINVAL;
      break;
   }

exit:
   __putname(filename);
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
