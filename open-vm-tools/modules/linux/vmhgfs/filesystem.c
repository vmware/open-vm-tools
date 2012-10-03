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

/* Synchronization primitives. */
DEFINE_SPINLOCK(hgfsBigLock);

/* Other variables. */
compat_kmem_cache *hgfsInodeCache;

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
static int HgfsGetRootDentry(struct super_block *sb, struct dentry **rootDentry);
static int HgfsReadSuper(struct super_block *sb,
                         void *rawData,
                         int flags);
static void HgfsResetOps(void);


/* HGFS filesystem high-level operations. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
static struct dentry *HgfsMount(struct file_system_type *fs_type,
                                int flags,
                                const char *dev_name,
                                void *rawData);
#elif defined(VMW_GETSB_2618)
static int HgfsGetSb(struct file_system_type *fs_type,
                     int flags,
                     const char *dev_name,
                     void *rawData,
                     struct vfsmount *mnt);
#else
static struct super_block *HgfsGetSb(struct file_system_type *fs_type,
                                     int flags,
                                     const char *dev_name,
                                     void *rawData);
#endif

/* HGFS filesystem type structure. */
static struct file_system_type hgfsType = {
   .owner        = THIS_MODULE,
   .name         = HGFS_NAME,

   .fs_flags     = FS_BINARY_MOUNTDATA,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
   .mount        = HgfsMount,
#else
   .get_sb       = HgfsGetSb,
#endif
   .kill_sb      = kill_anon_super,
};

extern int USE_VMCI;

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
   HgfsInodeInfo *iinfo = (HgfsInodeInfo *)slabElem;

   /*
    * VFS usually calls this as part of allocating inodes for us, but since
    * we're doing the allocation now, we need to call it. It'll set up
    * much of the VFS inode members.
    */
   inode_init_once(&iinfo->inode);
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
 *----------------------------------------------------------------------------
 *
 * HgfsGetRootDentry --
 *
 *    Gets the root dentry for a given super block.
 *
 * Results:
 *    zero and a valid root dentry on success
 *    negative value on failure
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsGetRootDentry(struct super_block *sb,       // IN: Super block object
                  struct dentry **rootDentry)   // OUT: Root dentry
{
   int result = -ENOMEM;
   struct inode *rootInode;
   struct dentry *tempRootDentry = NULL;
   struct HgfsAttrInfo rootDentryAttr;
   HgfsInodeInfo *iinfo;

   ASSERT(sb);
   ASSERT(rootDentry);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: entered\n", __func__));

   rootInode = HgfsGetInode(sb, HGFS_ROOT_INO);
   if (rootInode == NULL) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: Could not get the root inode\n",
             __func__));
      goto exit;
   }

   /*
    * On an allocation failure in read_super, the inode will have been
    * marked "bad". If it was, we certainly don't want to start playing with
    * the HgfsInodeInfo. So quietly put the inode back and fail.
    */
   if (is_bad_inode(rootInode)) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: encountered bad inode\n",
             __func__));
      goto exit;
   }

   tempRootDentry = d_make_root(rootInode);
   /*
    * d_make_root() does iput() on failure; if d_make_root() completes
    * successfully then subsequent dput() will do iput() for us, so we
    * should just ignore root inode from now on.
    */
   rootInode = NULL;

   if (tempRootDentry == NULL) {
      LOG(4, (KERN_WARNING "VMware hgfs: %s: Could not get "
              "root dentry\n", __func__));
      goto exit;
   }

   result = HgfsPrivateGetattr(tempRootDentry, &rootDentryAttr, NULL);
   if (result) {
      LOG(4, (KERN_WARNING "VMware hgfs: HgfsReadSuper: Could not"
             "instantiate the root dentry\n"));
      goto exit;
   }

   iinfo = INODE_GET_II_P(tempRootDentry->d_inode);
   iinfo->isFakeInodeNumber = FALSE;
   iinfo->isReferencedInode = TRUE;

   if (rootDentryAttr.mask & HGFS_ATTR_VALID_FILEID) {
      iinfo->hostFileId = rootDentryAttr.hostFileId;
   }

   HgfsChangeFileAttributes(tempRootDentry->d_inode, &rootDentryAttr);
   HgfsDentryAgeReset(tempRootDentry);
   tempRootDentry->d_op = &HgfsDentryOperations;

   *rootDentry = tempRootDentry;
   result = 0;

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: finished\n", __func__));
exit:
   if (result) {
      iput(rootInode);
      dput(tempRootDentry);
      *rootDentry = NULL;
   }
   return result;
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
   int result = 0;
   HgfsSuperInfo *si;
   HgfsMountInfo *mountInfo;
   struct dentry *rootDentry = NULL;

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
   sb->s_d_op = &HgfsDentryOperations;
#endif

   /*
    * If s_maxbytes isn't initialized, the generic write path may fail. In
    * most kernels, s_maxbytes is initialized by the kernel's superblock
    * allocation routines, but in some, it's up to the filesystem to initialize
    * it. Note that we'll initialize it anyway, because the default value is
    * MAX_NON_LFS, which caps our filesize at 2^32 bytes.
    */
   sb->s_maxbytes = MAX_LFS_FILESIZE;

   /*
    * These two operations will make sure that our block size and the bits
    * per block match up, no matter what HGFS_BLOCKSIZE may be. Granted,
    * HGFS_BLOCKSIZE will always be a power of two, but you never know!
    */
   sb->s_blocksize_bits = HgfsComputeBlockBits(HGFS_BLOCKSIZE);
   sb->s_blocksize = 1 << sb->s_blocksize_bits;

   result = HgfsGetRootDentry(sb, &rootDentry);
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsMount --
 *
 *    Invokes generic kernel code to mount a deviceless filesystem.
 *
 * Results:
 *    Mount's root dentry structure on success
 *    ERR_PTR()-encoded negative error code on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

struct dentry *
HgfsMount(struct file_system_type *fs_type, // IN: file system type of mount
          int flags,                        // IN: mount flags
          const char *dev_name,             // IN: device mounting on
          void *rawData)                    // IN: mount arguments
{
   return mount_nodev(fs_type, flags, rawData, HgfsReadSuper);
}
#elif defined VMW_GETSB_2618
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

static struct super_block *
HgfsGetSb(struct file_system_type *fs_type,
	  int flags,
	  const char *dev_name,
	  void *rawData)
{
   return get_sb_nodev(fs_type, flags, rawData, HgfsReadSuper);
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

   if (USE_VMCI) {
      hgfsVersionRead = HGFS_OP_READ_FAST_V4;
      hgfsVersionWrite = HGFS_OP_WRITE_FAST_V4;
   }

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

   /* Setup the inode slab allocator. */
   hgfsInodeCache = compat_kmem_cache_create("hgfsInodeCache",
                                             sizeof (HgfsInodeInfo),
                                             0,
                                             SLAB_HWCACHE_ALIGN,
                                             HgfsInodeCacheCtor);
   if (hgfsInodeCache == NULL) {
      printk(KERN_WARNING "VMware hgfs: failed to create inode allocator\n");
      return FALSE;
   }

   /* Initialize the transport. */
   HgfsTransportInit();

   /*
    * Register the filesystem. This should be the last thing we do
    * in init_module.
    */
   if (register_filesystem(&hgfsType)) {
      printk(KERN_WARNING "VMware hgfs: failed to register filesystem\n");
      kmem_cache_destroy(hgfsInodeCache);
      return FALSE;
   }
   LOG(4, (KERN_DEBUG "VMware hgfs: Module Loaded\n"));

   return TRUE;
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

   LOG(4, (KERN_DEBUG "VMware hgfs: Module Unloaded\n"));
   return success;
}
