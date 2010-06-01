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
 * filesystem.c --
 *
 * High-level filesystem operations for the filesystem portion of
 * the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <asm/atomic.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include "compat_completion.h"
#include "compat_cred.h"
#include "compat_dcache.h"
#include "compat_fs.h"
#include "compat_kernel.h"
#include "compat_sched.h"
#include "compat_semaphore.h"
#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_string.h"
#include "compat_uaccess.h"
#include "compat_version.h"

/* Must be included after sched.h. */
#include <linux/smp_lock.h>

#include "filesystem.h"
#include "transport.h"
#include "hgfsDevLinux.h"
#include "hgfsProto.h"
#include "hgfsUtil.h"
#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "vm_assert.h"
#include "vm_basic_types.h"
#include "rpcout.h"
#include "hgfs.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 25)
#define KERNEL_25_FS 0
#else
#define KERNEL_25_FS 1
#endif

#define HGFS_BD_THREAD_NAME "VMware hgfs backdoor handler"

/* Synchronization primitives. */
spinlock_t hgfsBigLock = SPIN_LOCK_UNLOCKED;

/* Other variables. */
compat_kmem_cache *hgfsReqCache = NULL;
compat_kmem_cache *hgfsInodeCache = NULL;

/* Global protocol version switch. */
HgfsOp hgfsVersionOpen;
HgfsOp hgfsVersionRead;
HgfsOp hgfsVersionWrite;
HgfsOp hgfsVersionClose;
HgfsOp hgfsVersionSearchOpen;
HgfsOp hgfsVersionSearchRead;
HgfsOp hgfsVersionSearchClose;
HgfsOp hgfsVersionGetattr;
HgfsOp hgfsVersionSetattr;
HgfsOp hgfsVersionCreateDir;
HgfsOp hgfsVersionDeleteFile;
HgfsOp hgfsVersionDeleteDir;
HgfsOp hgfsVersionRename;
HgfsOp hgfsVersionQueryVolumeInfo;
HgfsOp hgfsVersionCreateSymlink;

/* Private functions. */
static inline unsigned long HgfsComputeBlockBits(unsigned long blockSize);
static compat_kmem_cache_ctor HgfsInodeCacheCtor;
static HgfsSuperInfo *HgfsInitSuperInfo(HgfsMountInfo *mountInfo);
static int HgfsReadSuper(struct super_block *sb,
                         void *rawData,
                         int flags);
static void HgfsResetOps(void);


/* HGFS filesystem high-level operations. */
#if KERNEL_25_FS /* { */
#   if defined VMW_GETSB_2618
static int HgfsGetSb(struct file_system_type *fs_type,
                     int flags,
                     const char *dev_name,
                     void *rawData,
                     struct vfsmount *mnt);
#   elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 70)
static struct super_block *HgfsGetSb(struct file_system_type *fs_type,
                                     int flags,
                                     const char *dev_name,
                                     void *rawData);
#   else
static struct super_block *HgfsGetSb(struct file_system_type *fs_type,
                                     int flags,
                                     char *dev_name,
                                     void *rawData);
#   endif
#else /* } { */
static struct super_block *HgfsReadSuper24(struct super_block *sb,
                                           void *rawData,
                                           int flags);
#endif /* } */

/* HGFS filesystem type structure. */
static struct file_system_type hgfsType = {
   .owner        = THIS_MODULE,
   .name         = HGFS_NAME,

   .fs_flags     = FS_BINARY_MOUNTDATA,
#if KERNEL_25_FS
   .get_sb       = HgfsGetSb,
   .kill_sb      = kill_anon_super,
#else
   .read_super   = HgfsReadSuper24,
#endif
};


/*
 * Private functions implementations.
 */

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsComputeBlockBits --
 *
 *      Given a block size, returns the number of bits in the block, rounded
 *      down. This approach of computing the number of bits per block and
 *      saving it for later use is the same used in NFS.
 *
 * Results:
 *      The number of bits in the block.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static inline unsigned long
HgfsComputeBlockBits(unsigned long blockSize)
{
   uint8 numBits;

   for (numBits = 31; numBits && !(blockSize & (1 << numBits)); numBits--);
   return numBits;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInodeCacheCtor --
 *
 *      Constructor for HGFS inode structures that runs once at slab
 *      allocation. It is called once for each piece of memory that
 *      is used to satisfy HGFS inode allocations; it should only be
 *      used to initialize items that will naturally return to their
 *      initialized state before deallocation (such as locks, list_heads).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsInodeCacheCtor(COMPAT_KMEM_CACHE_CTOR_ARGS(slabElem)) // IN: slab item to initialize
{
#ifdef VMW_EMBED_INODE
   HgfsInodeInfo *iinfo = (HgfsInodeInfo *)slabElem;

   /*
    * VFS usually calls this as part of allocating inodes for us, but since
    * we're doing the allocation now, we need to call it. It'll set up
    * much of the VFS inode members.
    */
   inode_init_once(&iinfo->inode);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsInitSuperInfo --
 *
 *    Allocate and initialize a new HgfsSuperInfo object
 *
 * Results:
 *    Returns a new HgfsSuperInfo object with all its fields initialized,
 *    or an error code cast as a pointer.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static HgfsSuperInfo *
HgfsInitSuperInfo(HgfsMountInfo *mountInfo) // IN: Passed down from the user
{
   HgfsSuperInfo *si = NULL;
   int result = 0;
   int len;
   char *tmpName;
   Bool hostValid;

   si = kmalloc(sizeof *si, GFP_KERNEL);
   if (!si) {
      result = -ENOMEM;
      goto out2;
   }

   /*
    * If the mounter specified a uid or gid, we will prefer them over any uid
    * or gid given to us by the server.
    */
   si->uidSet = mountInfo->uidSet;
   if (si->uidSet) {
      si->uid = mountInfo->uid;
   } else {
      si->uid = current_uid();
   }
   si->gidSet = mountInfo->gidSet;
   if (si->gidSet) {
      si->gid = mountInfo->gid;
   } else {
      si->gid = current_gid();
   }
   si->fmask = mountInfo->fmask;
   si->dmask = mountInfo->dmask;
   si->ttl = mountInfo->ttl * HZ; // in ticks

   /*
    * We don't actually care about this field (though we may care in the
    * future). For now, just make sure it is set to ".host" as a sanity check.
    *
    * We can't call getname() directly because on certain kernels we can't call
    * putname() directly.  For more details, see the change description of
    * change 464782 or the second comment in bug 159623, which fixed the same
    * problem for vmblock.
    */
   tmpName = compat___getname();
   if (!tmpName) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: could not obtain "
              "memory for filename\n"));
      result = -ENOMEM;
      goto out2;
   }

   len = strncpy_from_user(tmpName, mountInfo->shareNameHost, PATH_MAX);
   if (len < 0 || len >= PATH_MAX) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: strncpy_from_user "
              "on host string failed\n"));
      result = len < 0 ? len : -ENAMETOOLONG;
      goto out;
   }

   hostValid = strcmp(tmpName, ".host") == 0;
   if (!hostValid) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: host string is "
              "invalid\n"));
      result = -EINVAL;
      goto out;
   }

   /*
    * Perform a simple sanity check on the directory portion: it must begin
    * with forward slash.
    */
   len = strncpy_from_user(tmpName, mountInfo->shareNameDir, PATH_MAX);
   if (len < 0 || len >= PATH_MAX) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: strncpy_from_user "
              "on dir string failed\n"));
      result = len < 0 ? len : -ENAMETOOLONG;
      goto out;
   }

   if (*tmpName != '/') {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: dir string is "
              "invalid\n"));
      result = -EINVAL;
      goto out;
   }

   /*
    * The SELinux audit subsystem will delay the putname() of a string until
    * the end of a system call so that it may be audited at any point. At that
    * time, it also unconditionally calls putname() on every string allocated
    * by getname().
    *
    * This means we can't safely retain strings allocated by getname() beyond
    * the syscall boundary. So after getting the string, use kstrdup() to
    * duplicate it, and store that (audit-safe) result in the SuperInfo struct.
    */
   si->shareName = compat_kstrdup(tmpName, GFP_KERNEL);
   if (si->shareName == NULL) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: kstrdup on "
              "dir string failed\n"));
      result = -ENOMEM;
      goto out;
   }
   si->shareNameLen = strlen(si->shareName);

  out:
   compat___putname(tmpName);
  out2:
   if (result) {
      /* If we failed, si->shareName couldn't have been allocated. */
      kfree(si);
      si = ERR_PTR(result);
   }
   return si;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsReadSuper --
 *
 *    The main entry point of the filesystem side of the driver. Called when
 *    a userland process does a mount(2) of an hgfs filesystem. This makes the
 *    whole driver transition from its initial state to state 1. Fill the
 *    content of the uninitialized superblock provided by the kernel.
 *
 *    'rawData' is a pointer (that can be NULL) to a kernel buffer (whose
 *    size is <= PAGE_SIZE) that corresponds to the filesystem-specific 'data'
 *    argument passed to mount(2).
 *
 * Results:
 *    zero and initialized superblock on success
 *    negative value on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsReadSuper(struct super_block *sb, // OUT: Superblock object
              void *rawData,          // IN: Fs-specific mount data
              int flags)              // IN: Mount flags
{
   int result;
   HgfsSuperInfo *si;
   HgfsMountInfo *mountInfo;
   struct dentry *rootDentry;

   ASSERT(sb);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReadSuper: entered\n"));

   /* Sanity check the incoming user data. */
   mountInfo = (HgfsMountInfo *)rawData;
   if (!mountInfo ||
       mountInfo->magicNumber != HGFS_SUPER_MAGIC ||
       mountInfo->version != HGFS_PROTOCOL_VERSION) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReadSuper: bad mount data passed "
              "in by user, failing!\n"));
      return -EINVAL;
   }

   /* Setup both our superblock and the VFS superblock. */
   si = HgfsInitSuperInfo(mountInfo);
   if (IS_ERR(si)) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReadSuper: superinfo "
              "init failed\n"));
      return PTR_ERR(si);
   }
   HGFS_SET_SB_TO_COMMON(sb, si);
   sb->s_magic = HGFS_SUPER_MAGIC;
   sb->s_op = &HgfsSuperOperations;

   /*
    * If s_maxbytes isn't initialized, the generic write path may fail. In
    * most kernels, s_maxbytes is initialized by the kernel's superblock
    * allocation routines, but in some, it's up to the filesystem to initialize
    * it. Note that we'll initialize it anyway, because the default value is
    * MAX_NON_LFS, which caps our filesize at 2^32 bytes.
    */
#ifdef VMW_SB_HAS_MAXBYTES
   sb->s_maxbytes = MAX_LFS_FILESIZE;
#endif

   /*
    * These two operations will make sure that our block size and the bits
    * per block match up, no matter what HGFS_BLOCKSIZE may be. Granted,
    * HGFS_BLOCKSIZE will always be a power of two, but you never know!
    */
   sb->s_blocksize_bits = HgfsComputeBlockBits(HGFS_BLOCKSIZE);
   sb->s_blocksize = 1 << sb->s_blocksize_bits;

   /*
    * We can't use d_alloc_root() here directly because it requires a valid
    * inode, which only HgfsInstantiate will create. So instead, we'll do the
    * work in pieces. First we'll allocate the dentry and setup its parent
    * and superblock. Then HgfsInstantiate will do the rest, issuing a getattr,
    * getting the inode, and instantiating the dentry with it.
    */
   rootDentry = compat_d_alloc_name(NULL, "/");
   if (rootDentry == NULL) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsReadSuper: Could not allocate "
              "root dentry\n"));
      result = -ENOMEM;
      goto exit;
   }
   rootDentry->d_parent = rootDentry;
   rootDentry->d_sb = sb;
   result = HgfsInstantiate(rootDentry, HGFS_ROOT_INO, NULL);
   if (result) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsReadSuper: Could not instantiate "
              "root dentry\n"));
      goto exit;
   }
   sb->s_root = rootDentry;

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReadSuper: finished %s\n", si->shareName));

  exit:
   if (result) {
      dput(rootDentry);
      kfree(si->shareName);
      kfree(si);
   }
   return result;
}


/*
 * HGFS filesystem high-level operations.
 */

#if KERNEL_25_FS
#if defined VMW_GETSB_2618
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetSb --
 *
 *    Invokes generic kernel code to prepare superblock for
 *    deviceless filesystem.
 *
 * Results:
 *    0 on success
 *    non-zero on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsGetSb(struct file_system_type *fs_type,
	  int flags,
	  const char *dev_name,
	  void *rawData,
          struct vfsmount *mnt)
{
   return get_sb_nodev(fs_type, flags, rawData, HgfsReadSuper, mnt);
}
#else
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetSb --
 *
 *    Invokes generic kernel code to prepare superblock for
 *    deviceless filesystem.
 *
 * Results:
 *    The initialized superblock on success
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 70)
static struct super_block *
HgfsGetSb(struct file_system_type *fs_type,
	  int flags,
	  const char *dev_name,
	  void *rawData)
#else
static struct super_block *
HgfsGetSb(struct file_system_type *fs_type,
	  int flags,
	  char *dev_name,
	  void *rawData)
#endif
{
   return get_sb_nodev(fs_type, flags, rawData, HgfsReadSuper);
}
#endif
#else


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsReadSuper24 --
 *
 *    Compatibility wrapper for 2.4.x kernels read_super.
 *    Converts success to sb, and failure to NULL.
 *
 * Results:
 *    The initialized superblock on success
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static struct super_block *
HgfsReadSuper24(struct super_block *sb,
		void *rawData,
		int flags) {
   return HgfsReadSuper(sb, rawData, flags) ? NULL : sb;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsResetOps --
 *
 *      Reset ops with more than one opcode back to the desired opcode.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsResetOps(void)
{
   hgfsVersionOpen            = HGFS_OP_OPEN_V3;
   hgfsVersionRead            = HGFS_OP_READ_V3;
   hgfsVersionWrite           = HGFS_OP_WRITE_V3;
   hgfsVersionClose           = HGFS_OP_CLOSE_V3;
   hgfsVersionSearchOpen      = HGFS_OP_SEARCH_OPEN_V3;
   hgfsVersionSearchRead      = HGFS_OP_SEARCH_READ_V3;
   hgfsVersionSearchClose     = HGFS_OP_SEARCH_CLOSE_V3;
   hgfsVersionGetattr         = HGFS_OP_GETATTR_V3;
   hgfsVersionSetattr         = HGFS_OP_SETATTR_V3;
   hgfsVersionCreateDir       = HGFS_OP_CREATE_DIR_V3;
   hgfsVersionDeleteFile      = HGFS_OP_DELETE_FILE_V3;
   hgfsVersionDeleteDir       = HGFS_OP_DELETE_DIR_V3;
   hgfsVersionRename          = HGFS_OP_RENAME_V3;
   hgfsVersionQueryVolumeInfo = HGFS_OP_QUERY_VOLUME_INFO_V3;
   hgfsVersionCreateSymlink   = HGFS_OP_CREATE_SYMLINK_V3;
}


/*
 * Public function implementations.
 */

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInitFileSystem --
 *
 *      Initializes the file system and registers it with the kernel.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsInitFileSystem(void)
{
   /* Initialize primitives. */
   HgfsResetOps();

   /* Setup the request slab allocator. */
   hgfsReqCache = compat_kmem_cache_create("hgfsReqCache",
                                           sizeof (HgfsReq),
                                           0,
                                           SLAB_HWCACHE_ALIGN,
                                           NULL);
   if (hgfsReqCache == NULL) {
      printk(KERN_WARNING "VMware hgfs: failed to create request allocator\n");
      goto error_caches;
   }

   /* Setup the inode slab allocator. */
   hgfsInodeCache = compat_kmem_cache_create("hgfsInodeCache",
                                             sizeof (HgfsInodeInfo),
                                             0,
                                             SLAB_HWCACHE_ALIGN,
                                             HgfsInodeCacheCtor);
   if (hgfsInodeCache == NULL) {
      printk(KERN_WARNING "VMware hgfs: failed to create inode allocator\n");
      goto error_caches;
   }

   /* Initialize the transport. */
   HgfsTransportInit();

   /*
    * Register the filesystem. This should be the last thing we do
    * in init_module.
    */
   if (register_filesystem(&hgfsType)) {
      printk(KERN_WARNING "VMware hgfs: failed to register filesystem\n");
      goto error_caches;
   }
   LOG(4, (KERN_DEBUG "VMware hgfs: Module Loaded\n"));
#ifdef HGFS_ENABLE_WRITEBACK
   LOG(4, (KERN_DEBUG "VMware hgfs: writeback cache enabled\n"));
#endif
   return TRUE;

error_caches:
   if (hgfsInodeCache != NULL) {
      kmem_cache_destroy(hgfsInodeCache);
   }
   if (hgfsReqCache != NULL) {
      kmem_cache_destroy(hgfsReqCache);
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCleanupFileSystem --
 *
 *      Cleans up file system and unregisters it with the kernel.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsCleanupFileSystem(void)
{
   Bool success = TRUE;

/* FIXME: Check actual kernel version when RR's modules went in */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 45)
   if (MOD_IN_USE) {
      printk(KERN_WARNING "VMware hgfs: filesystem in use, removal failed\n");
      success = FALSE;
   }
#endif

  /*
   * Unregister the filesystem. This should be the first thing we do in
   * the module cleanup code.
   */
   if (unregister_filesystem(&hgfsType)) {
      printk(KERN_WARNING "VMware hgfs: failed to unregister filesystem\n");
      success = FALSE;
   }

   /* Transport cleanup. */
   HgfsTransportExit();

   /* Destroy the inode and request slabs. */
   kmem_cache_destroy(hgfsInodeCache);
   kmem_cache_destroy(hgfsReqCache);

   LOG(4, (KERN_DEBUG "VMware hgfs: Module Unloaded\n"));
   return success;
}
