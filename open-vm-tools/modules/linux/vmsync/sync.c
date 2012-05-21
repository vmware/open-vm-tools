/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * sync.c --
 *
 * Linux "sync driver" implementation.
 *
 * A typical user of vmsync will:
 *
 * - call ioctl() with the SYNC_IOC_FREEZE to freeze a list of paths.
 *   The list should be a colon-separated list of paths to be frozen.
 * - call ioctl() with the SYNC_IOC_THAW command.
 *
 * The driver has an internal timer that is set up as soon as devices
 * are frozen (i.e., after a successful SYNC_IOC_FREEZE). Subsequent calls
 * to SYNC_IOC_FREEZE will not reset the timer. This timer is not designed
 * as a way to protect the driver from being an avenue for a DoS attack
 * (after all, if the user already has CAP_SYS_ADMIN privileges...), but
 * as a way to protect itself from faulty user level apps during testing.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <asm/bug.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <linux/buffer_head.h>
#include <linux/proc_fs.h>

#include "compat_fs.h"
#include "compat_module.h"
#include "compat_namei.h"
#include "compat_mutex.h"
#include "compat_slab.h"
#include "compat_workqueue.h"

#include "syncDriverIoc.h"
#include "vmsync_version.h"

/*
 * After a successful SYNC_IOC_FREEZE ioctl, a timer will be enabled to thaw
 * *all* frozen block devices after this delay.
 */
#define VMSYNC_THAW_TASK_DELAY (30 * HZ)

/* Module information. */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Sync Driver");
MODULE_VERSION(VMSYNC_DRIVER_VERSION_STRING);
MODULE_LICENSE("GPL v2");
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");

static int VmSyncRelease(struct inode* inode,
                         struct file *file);

static long VmSyncUnlockedIoctl(struct file *file,
                                unsigned cmd,
                                unsigned long arg);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
static int VmSyncIoctl(struct inode *inode,
                       struct file *file,
                       unsigned cmd,
                       unsigned long arg);
#endif

static int VmSyncOpen(struct inode *inode,
                      struct file *f);

static struct file_operations VmSyncFileOps = {
   .owner            = THIS_MODULE,
   .open             = VmSyncOpen,
   .release          = VmSyncRelease,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
   .unlocked_ioctl   = VmSyncUnlockedIoctl,
#else
   .ioctl            = VmSyncIoctl,
#endif
};


typedef struct VmSyncBlockDevice {
   struct list_head     list;
   struct block_device  *bdev;
   struct nameidata     nd;
   struct super_block   *sb;
} VmSyncBlockDevice;


typedef struct VmSyncState {
   struct list_head     devices;
   compat_mutex_t       lock;
   compat_delayed_work  thawTask;
} VmSyncState;


/*
 * Serializes freeze operations. Used to make sure that two different
 * fds aren't allowed to freeze the same device.
 */
static compat_mutex_t gFreezeLock;

/* A global count of how many devices are currently frozen by the driver. */
static atomic_t gFreezeCount;

static compat_kmem_cache *gSyncStateCache;
static compat_kmem_cache *gBlockDeviceCache;

static compat_kmem_cache_ctor VmSyncBlockDeviceCtor;
static compat_kmem_cache_ctor VmSyncStateCtor;


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncThawDevices --
 *
 *    Thaws all currently frozen devices.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Devices are thawed, thaw task is cancelled.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmSyncThawDevices(void  *_state)  // IN
{
   struct list_head *cur, *tmp;
   VmSyncBlockDevice *dev;
   VmSyncState *state;

   state = (VmSyncState *) _state;

   compat_mutex_lock(&state->lock);
   cancel_delayed_work(&state->thawTask);
   list_for_each_safe(cur, tmp, &state->devices) {
      dev = list_entry(cur, VmSyncBlockDevice, list);
      if (dev->sb != NULL && dev->sb->s_frozen != SB_UNFROZEN) {
         thaw_bdev(dev->bdev, dev->sb);
         atomic_dec(&gFreezeCount);
      }
      list_del_init(&dev->list);
      kmem_cache_free(gBlockDeviceCache, dev);
   }
   compat_mutex_unlock(&state->lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncThawDevicesCallback --
 *
 *    Wrapper around VmSyncThawDevices used by the work queue.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    See VmSyncThawDevices.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmSyncThawDevicesCallback(compat_delayed_work_arg data) // IN
{
   VmSyncState *state = COMPAT_DELAYED_WORK_GET_DATA(data,
                                                     VmSyncState, thawTask);
   VmSyncThawDevices(state);
}

/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncAddPath --
 *
 *    Adds the block device associated with the path to the internal list
 *    of devices to be frozen.
 *
 * Results:
 *    0 on success.
 *    -EINVAL if path doesn't point to a freezable mount.
 *    -EALREADY if path is already frozen.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmSyncAddPath(const VmSyncState *state,   // IN
              const char *path,           // IN
              struct list_head *pathList) // IN
{
   int result;
   struct list_head *cur, *tmp;
   struct inode *inode;
   struct nameidata nd;
   VmSyncBlockDevice *dev;

   if ((result = compat_path_lookup(path, LOOKUP_FOLLOW, &nd)) != 0) {
      goto exit;
   }
   inode = compat_vmw_nd_to_dentry(nd)->d_inode;

   /*
    * Abort if the inode's superblock isn't backed by a block device, or if
    * the superblock is already frozen.
    */
   if (inode->i_sb->s_bdev == NULL ||
       inode->i_sb->s_frozen != SB_UNFROZEN) {
      result = (inode->i_sb->s_bdev == NULL) ? -EINVAL : -EALREADY;
      compat_path_release(&nd);
      goto exit;
   }

   /*
    * Check if we've already added the block device to the list.
    */
   list_for_each_safe(cur, tmp, &state->devices) {
      dev = list_entry(cur, VmSyncBlockDevice, list);
      if (dev->bdev == inode->i_sb->s_bdev) {
         result = 0;
         compat_path_release(&nd);
         goto exit;
      }
   }

   /*
    * Allocate a new entry and add it to the list.
    */
   dev = kmem_cache_alloc(gBlockDeviceCache, GFP_KERNEL);
   if (dev == NULL) {
      result = -ENOMEM;
      compat_path_release(&nd);
      goto exit;
   }

   /*
    * Whenever we add a device to the "freeze list", the reference to
    * the nameidata struct is retained until the device is actually
    * frozen; this ensures the kernel knows the path is being used.
    * Here we copy the nameidata struct so we can release our reference
    * at that time.
    */
   dev->bdev = inode->i_sb->s_bdev;
   memcpy(&dev->nd, &nd, sizeof nd);
   list_add_tail(&dev->list, pathList);
   result = 0;

exit:
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncFreezeDevices --
 *
 *    Tries to freeze all the devices provided by the user.
 *
 * Results:
 *    o on success, -errno on error.
 *
 * Side effects:
 *    A task is scheduled to automatically thaw devices after a timeout.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmSyncFreezeDevices(VmSyncState *state,            // IN
                    const char __user *userPaths)  // IN
{
   int result = 0;
   char *paths;
   char *currPath;
   char *nextSep;
   struct list_head *cur, *tmp;
   struct list_head pathList;
   VmSyncBlockDevice *dev;

   INIT_LIST_HEAD(&pathList);

   /*
    * XXX: Using getname() will restrict the list of paths to PATH_MAX.
    * Although this is not ideal, it shouldn't be a problem. We need an
    * upper bound anyway.
    */
   paths = getname(userPaths);
   if (IS_ERR(paths)) {
      return PTR_ERR(paths);
   }

   compat_mutex_lock(&gFreezeLock);
   compat_mutex_lock(&state->lock);

   /*
    * First, try to add all paths to the list of paths to be frozen.
    */
   currPath = paths;
   do {
      nextSep = strchr(currPath, ':');
      if (nextSep != NULL) {
         *nextSep = '\0';
      }
      result = VmSyncAddPath(state, currPath, &pathList);
      /*
       * Due to the way our user level app decides which paths to freeze
       * now, we need to ignore EINVAL since there's no way to detect
       * from user-land which paths are freezable or not.
       */
      if (result != 0 && result != -EINVAL) {
         break;
      } else {
         result = 0;
      }
      currPath = nextSep + 1;
   } while (nextSep != NULL);

   /*
    * If adding all the requested paths worked, then freeze them.
    * Otherwise, clean the list. Make sure we only touch the devices
    * added in the current call.
    */
   list_for_each_safe(cur, tmp, &pathList) {
      dev = list_entry(cur, VmSyncBlockDevice, list);
      if (result == 0) {
         dev->sb = freeze_bdev(dev->bdev);
         compat_path_release(&dev->nd);
         if (dev->sb != NULL) {
            atomic_inc(&gFreezeCount);
         }
         list_move_tail(&dev->list, &state->devices);
      } else {
         list_del_init(&dev->list);
         kmem_cache_free(gBlockDeviceCache, dev);
      }
   }

   compat_mutex_unlock(&state->lock);
   compat_mutex_unlock(&gFreezeLock);

   if (result == 0) {
      compat_schedule_delayed_work(&state->thawTask, VMSYNC_THAW_TASK_DELAY);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncQuery --
 *
 *    Writes the number of devices currently frozen by the driver to the
 *    given address. The address should be in user space and be able to
 *    hold an int32_t.
 *
 * Results:
 *    0 on success, -errno on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static inline int
VmSyncQuery(void __user *dst) // OUT
{
   int32_t active;
   int result = 0;

   active = (int32_t) atomic_read(&gFreezeCount);
   if (copy_to_user(dst, &active, sizeof active)) {
      result = -EFAULT;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncUnlockedIoctl --
 *
 *    Handles the IOCTLs recognized by the driver.
 *
 *    - SYNC_IOC_FREEZE: freezes the block device associated with the
 *      path passed as a parameter.
 *
 *    - SYNC_IOC_THAW: thaws all currently frozen block devices.
 *
 *    - SYNC_IOC_QUERY: returns the number of block devices currently
 *      frozen by the driver. This is a global view of the driver state
 *      and doesn't reflect any fd-specific data.
 *
 * Results:
 *    0 on success, -errno otherwise.
 *
 * Side effects:
 *    See ioctl descriptions above.
 *
 *-----------------------------------------------------------------------------
 */

static long
VmSyncUnlockedIoctl(struct file *file,   // IN
                    unsigned cmd,        // IN
                    unsigned long arg)   // IN/OUT
{
   int result = -ENOTTY;
   VmSyncState *state;

   state = (VmSyncState *) file->private_data;

   switch (cmd) {
   case SYNC_IOC_FREEZE:
      if (!capable(CAP_SYS_ADMIN)) {
         result = -EPERM;
         break;
      }
      result = VmSyncFreezeDevices(state, (const char __user *) arg);
      break;

   case SYNC_IOC_THAW:
      if (!capable(CAP_SYS_ADMIN)) {
         result = -EPERM;
         break;
      }
      VmSyncThawDevices(state);
      result = 0;
      break;

   case SYNC_IOC_QUERY:
      result = VmSyncQuery((void __user *)arg);
      break;

   default:
      printk(KERN_DEBUG "vmsync: unknown ioctl %d\n", cmd);
      break;
   }
   return result;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncIoctl --
 *
 *    Wrapper around VmSyncUnlockedIoctl for kernels < 2.6.11, which don't
 *    support unlocked_ioctl.
 *
 * Results:
 *    See VmSyncUnlockedIoctl.
 *
 * Side effects:
 *    See VmSyncUnlockedIoctl.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmSyncIoctl(struct inode *inode, // IN
            struct file *file,   // IN
            unsigned cmd,        // IN
            unsigned long arg)   // IN/OUT
{
   return (int) VmSyncUnlockedIoctl(file, cmd, arg);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncOpen --
 *
 *    Instantiates a new state object and attached it to the file struct.
 *
 * Results:
 *    0, or -ENOMEM if can't allocate memory.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmSyncOpen(struct inode *inode,  // IN
           struct file *f)       // IN
{
   if (capable(CAP_SYS_ADMIN)) {
      f->private_data = kmem_cache_alloc(gSyncStateCache, GFP_KERNEL);
      if (f->private_data == NULL) {
         return -ENOMEM;
      }
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncRelease --
 *
 *    If the fd was used to freeze devices, then thaw all frozen block devices.
 *
 * Results:
 *    Returns 0.
 *
 * Side effects:
 *    Calls VmSyncThawDevices.
 *
 *-----------------------------------------------------------------------------
 */

static int
VmSyncRelease(struct inode *inode,  // IN
              struct file *file)    // IN
{
   if (capable(CAP_SYS_ADMIN)) {
      VmSyncState *state = (VmSyncState *) file->private_data;
      if (!cancel_delayed_work(&state->thawTask)) {
         flush_scheduled_work();
      }
      VmSyncThawDevices(state);
      kmem_cache_free(gSyncStateCache, state);
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncBlockDeviceCtor --
 *
 *    Constructor for VmSyncBlockDevice objects.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmSyncBlockDeviceCtor(COMPAT_KMEM_CACHE_CTOR_ARGS(slabelem))  // IN
{
   VmSyncBlockDevice *dev = slabelem;

   INIT_LIST_HEAD(&dev->list);
   dev->bdev = NULL;
   dev->sb = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VmSyncStateCtor --
 *
 *    Constructor for VmSyncState objects.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmSyncStateCtor(COMPAT_KMEM_CACHE_CTOR_ARGS(slabelem))  // IN
{
   VmSyncState *state = slabelem;

   INIT_LIST_HEAD(&state->devices);
   COMPAT_INIT_DELAYED_WORK(&state->thawTask,
                            VmSyncThawDevicesCallback, state);
   compat_mutex_init(&state->lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * init_module --
 *
 *    Initializes the structures used by the driver, and creates the
 *    proc file used by the driver to receive commands.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
init_module(void)
{
   struct proc_dir_entry *controlProcEntry;

   atomic_set(&gFreezeCount, 0);
   compat_mutex_init(&gFreezeLock);

   /* Create the slab allocators for the module. */
   gBlockDeviceCache = compat_kmem_cache_create("VmSyncBlockDeviceCache",
                                                sizeof (VmSyncBlockDevice),
                                                0,
                                                SLAB_HWCACHE_ALIGN,
                                                VmSyncBlockDeviceCtor);
   if (gBlockDeviceCache == NULL) {
      printk(KERN_ERR "vmsync: no memory for block dev slab allocator\n");
      return -ENOMEM;
   }

   gSyncStateCache = compat_kmem_cache_create("VmSyncStateCache",
                                              sizeof (VmSyncState),
                                              0,
                                              SLAB_HWCACHE_ALIGN,
                                              VmSyncStateCtor);
   if (gSyncStateCache == NULL) {
      printk(KERN_ERR "vmsync: no memory for sync state slab allocator\n");
      kmem_cache_destroy(gBlockDeviceCache);
      return -ENOMEM;
   }

   /* Create /proc/driver/vmware-sync */
   controlProcEntry = create_proc_entry("driver/vmware-sync",
                                        S_IFREG | S_IRUSR | S_IRGRP | S_IROTH,
                                        NULL);
   if (!controlProcEntry) {
      printk(KERN_ERR "vmsync: could not create /proc/driver/vmware-sync\n");
      kmem_cache_destroy(gSyncStateCache);
      kmem_cache_destroy(gBlockDeviceCache);
      return -EINVAL;
   }

   controlProcEntry->proc_fops = &VmSyncFileOps;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * cleanup_module --
 *
 *    Unregisters the proc file used by the driver.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
cleanup_module(void)
{
   remove_proc_entry("driver/vmware-sync", NULL);
   kmem_cache_destroy(gSyncStateCache);
   kmem_cache_destroy(gBlockDeviceCache);
}

