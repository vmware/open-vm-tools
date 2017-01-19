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
static HgfsSuperInfo *HgfsInitSuperInfo(void *rawData,
                                        uint32 mountInfoVersion);
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
 *-----------------------------------------------------------------------------
 *
 * HgfsValidateMountInfo --
 *
 *    Validate the the user mode mounter information.
 *
 * Results:
 *    Zero on success or -EINVAL if we pass in an unknown version.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsValidateMountInfo(void *rawData,             // IN: Fs-specific mount data
                      uint32 *mountInfoVersion)  // OUT: Mount flags
{
   HgfsMountInfoV1 *infoV1;
   HgfsMountInfo *info;
   uint32 *magicNumber;
   int retVal = -EINVAL;

   ASSERT(mountInfoVersion);

   /* Sanity check the incoming user data. */
   if (rawData == NULL) {
      printk(KERN_WARNING LGPFX "%s: error: no user supplied mount data\n",
             __func__);
      goto exit;
   }

   /* Magic number is always first 4 bytes of the header. */
   magicNumber = rawData;
   if (*magicNumber != HGFS_SUPER_MAGIC) {
      printk(KERN_WARNING LGPFX "%s: error: user supplied mount data is not valid!\n",
              __func__);
      goto exit;
   }

   /*
    * Looks like HGFS data, now validate the version so that we can
    * proceed and extract the required settings from the user.
    */
   info = rawData;
   infoV1 = rawData;
   if ((info->version == HGFS_PROTOCOL_VERSION_1 ||
        info->version == HGFS_PROTOCOL_VERSION) &&
        info->infoSize == sizeof *info) {
      /*
       * The current version is validated with the size and magic number.
       * Note the version can be either 1 or 2 as it was not bumped initially.
       * Furthermore, return the version as HGFS_PROTOCOL_VERSION (2) only since
       * the objects are the same and it simplifies field extractions.
       */
      LOG(4, (KERN_DEBUG LGPFX "%s: mount data version %d passed\n",
              __func__, info->version));
      *mountInfoVersion = HGFS_PROTOCOL_VERSION;
      retVal = 0;
   } else if (infoV1->version == HGFS_PROTOCOL_VERSION_1) {
      /*
       * The version 1 is validated with the version and magic number.
       * Note the version can be only be 1 and if so does not collide with version 2 of
       * the header (which would be the info size field).
       */
      LOG(4, (KERN_DEBUG LGPFX "%s: mount data version %d passed\n",
              __func__, info->version));
      *mountInfoVersion = infoV1->version;
      retVal = 0;
   } else {
      /*
       * The version and info size fields could not be validated
       * for the known structure. It is probably a newer version.
       */
      printk(KERN_WARNING LGPFX "%s: error: user supplied mount data version %d\n",
              __func__, infoV1->version);
   }

exit:
   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetMountInfoV1 --
 *
 *    Gets the fields of interest from the user mode mounter version 1.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsGetMountInfoV1(HgfsMountInfoV1 *mountInfo, // IN: version 1 mount data
                   uint32 *mntFlags,           // OUT: Mount flags
                   uint32 *ttl,                // OUT: seconds until revalidate
                   uid_t *uid,                 // OUT: owner
                   gid_t *gid,                 // OUT: group
                   mode_t *fmask,              // OUT: file mask
                   mode_t *dmask,              // OUT: directory mask
                   const char **shareHost,     // OUT: share host name
                   const char **shareDir)      // OUT: share directory
{
   ASSERT(mountInfo);

   *mntFlags = 0;
   /*
    * If the mounter specified a uid or gid, we will prefer them over any uid
    * or gid given to us by the server.
    */
   if (mountInfo->uidSet) {
      *mntFlags |= HGFS_MNT_SET_UID;
      *uid = mountInfo->uid;
   }

   if (mountInfo->gidSet) {
      *mntFlags |= HGFS_MNT_SET_GID;
      *gid = mountInfo->gid;
   }

   *fmask = mountInfo->fmask;
   *dmask = mountInfo->dmask;
   *ttl = mountInfo->ttl;
   *shareHost = mountInfo->shareNameHost;
   *shareDir = mountInfo->shareNameDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetMountInfoV2 --
 *
 *    Gets the fields of interest from the user mode mounter version 2.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsGetMountInfoV2(HgfsMountInfo *mountInfo,   // IN: version 2 mount data
                   uint32 *mntFlags,           // OUT: Mount flags
                   uint32 *ttl,                // OUT: seconds until revalidate
                   uid_t *uid,                 // OUT: owner
                   gid_t *gid,                 // OUT: group
                   mode_t *fmask,              // OUT: file mask
                   mode_t *dmask,              // OUT: directory mask
                   const char **shareHost,     // OUT: share host name
                   const char **shareDir)      // OUT: share directory
{
   ASSERT(mountInfo);

   *mntFlags = 0;

   if ((mountInfo->flags & HGFS_MNTINFO_SERVER_INO) != 0) {
      *mntFlags |= HGFS_MNT_SERVER_INUM;
   }

   /*
    * If the mounter specified a uid or gid, we will prefer them over any uid
    * or gid given to us by the server.
    */
   if (mountInfo->uidSet) {
      *mntFlags |= HGFS_MNT_SET_UID;
      *uid = mountInfo->uid;
   }

   if (mountInfo->gidSet) {
      *mntFlags |= HGFS_MNT_SET_GID;
      *gid = mountInfo->gid;
   }

   *fmask = mountInfo->fmask;
   *dmask = mountInfo->dmask;
   *ttl = mountInfo->ttl;
   *shareHost = mountInfo->shareNameHost;
   *shareDir = mountInfo->shareNameDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetMountInfo --
 *
 *    Gets the fields of interest from the user mode mounter.
 *
 * Results:
 *    Zero on success or -EINVAL if we pass in an unknown version.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsGetMountInfo(void *rawData,            // IN: Fs-specific mount data
                 uint32 mountInfoVersion,  // IN: mount information version
                 uint32 *mntFlags,         // OUT: Mount flags
                 uint32 *ttl,             // OUT: seconds until revalidate
                 uid_t *uid,              // OUT: owner
                 gid_t *gid,              // OUT: group
                 mode_t *fmask,           // OUT: file mask
                 mode_t *dmask,           // OUT: directory mask
                 const char **shareHost,  // OUT: share host name
                 const char **shareDir)   // OUT: share path
{
   int result = 0;

   switch (mountInfoVersion) {
   case HGFS_PROTOCOL_VERSION_1:
      HgfsGetMountInfoV1(rawData,
                         mntFlags,
                         ttl,
                         uid,
                         gid,
                         fmask,
                         dmask,
                         shareHost,
                         shareDir);
      break;
   case HGFS_PROTOCOL_VERSION:
      HgfsGetMountInfoV2(rawData,
                         mntFlags,
                         ttl,
                         uid,
                         gid,
                         fmask,
                         dmask,
                         shareHost,
                         shareDir);
      break;
   default:
      ASSERT(FALSE);
      result = -EINVAL;
   }

   return result;
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
HgfsInitSuperInfo(void *rawData,            // IN: Passed down from the user
                  uint32 mountInfoVersion)  // IN: version
{
   HgfsSuperInfo *si = NULL;
   int result = 0;
   int len;
   char *tmpName = NULL;
   Bool hostValid;
   uint32 mntFlags = 0;
   uint32 ttl = 0;
   uid_t uid = 0;
   gid_t gid = 0;
   mode_t fmask = 0;
   mode_t dmask = 0;
   const char *shareHost;
   const char *shareDir;

   si = kmalloc(sizeof *si, GFP_KERNEL);
   if (!si) {
      result = -ENOMEM;
      goto out_error_si;
   }
   memset(si, 0, sizeof *si);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   result = bdi_setup_and_register(&si->bdi, HGFS_NAME);
   if (result) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: initialize backing device info"
              "failed. (%d)\n", __func__, result));
      goto out_error_si;
   }
#endif

   result = HgfsGetMountInfo(rawData,
                             mountInfoVersion,
                             &mntFlags,
                             &ttl,
                             &uid,
                             &gid,
                             &fmask,
                             &dmask,
                             &shareHost,
                             &shareDir);
   if (result < 0) {
      LOG(6, (KERN_DEBUG LGPFX "%s: error: get mount info %d\n", __func__, result));
      goto out_error_last;
   }

   /*
    * Initialize with the default flags.
    */
   si->mntFlags = mntFlags;

   si->uid = current_uid();
   if ((si->mntFlags & HGFS_MNT_SET_UID) != 0) {
      kuid_t mntUid = make_kuid(current_user_ns(), uid);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
      if (uid_valid(mntUid))
#endif
         si->uid = mntUid;
   }

   si->gid = current_gid();
   if ((si->mntFlags & HGFS_MNT_SET_GID) != 0) {
      kgid_t mntGid = make_kgid(current_user_ns(), gid);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
      if (gid_valid(mntGid))
#endif
         si->gid = mntGid;
   }
   si->fmask = fmask;
   si->dmask = dmask;
   si->ttl = ttl * HZ; // in ticks

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
      goto out_error_last;
   }

   len = strncpy_from_user(tmpName, shareHost, PATH_MAX);
   if (len < 0 || len >= PATH_MAX) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: strncpy_from_user "
              "on host string failed\n"));
      result = len < 0 ? len : -ENAMETOOLONG;
      goto out_error_last;
   }

   hostValid = strcmp(tmpName, ".host") == 0;
   if (!hostValid) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: host string is "
              "invalid\n"));
      result = -EINVAL;
      goto out_error_last;
   }

   /*
    * Perform a simple sanity check on the directory portion: it must begin
    * with forward slash.
    */
   len = strncpy_from_user(tmpName, shareDir, PATH_MAX);
   if (len < 0 || len >= PATH_MAX) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: strncpy_from_user "
              "on dir string failed\n"));
      result = len < 0 ? len : -ENAMETOOLONG;
      goto out_error_last;
   }

   if (*tmpName != '/') {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsInitSuperInfo: dir string is "
              "invalid\n"));
      result = -EINVAL;
      goto out_error_last;
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
      goto out_error_last;
   }
   si->shareNameLen = strlen(si->shareName);

out_error_last:
   if (tmpName) {
      compat___putname(tmpName);
   }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   if (result) {
      bdi_destroy(&si->bdi);
   }
#endif
out_error_si:
   if (result) {
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
   int result = 0;
   HgfsSuperInfo *si;
   struct dentry *rootDentry = NULL;
   uint32 mountInfoVersion;

   ASSERT(sb);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReadSuper: entered\n"));

   /* Sanity check the incoming user data. */
   result = HgfsValidateMountInfo(rawData, &mountInfoVersion);
   if (result < 0) {
      return result;
   }

   /* Setup both our superblock and the VFS superblock. */
   si = HgfsInitSuperInfo(rawData, mountInfoVersion);
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   sb->s_bdi = &si->bdi;
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

   /*
    * Create the root dentry and its corresponding inode.
    */
   result = HgfsInstantiateRoot(sb, &rootDentry);
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
      bdi_destroy(&si->bdi);
      sb->s_bdi = NULL;
#endif
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
