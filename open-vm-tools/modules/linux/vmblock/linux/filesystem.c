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
 *   File system for the vmblock driver.
 *
 */

#include "driver-config.h"
#include "compat_kernel.h"
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/mount.h>
#include "compat_fs.h"
#include "compat_spinlock.h"
#include "compat_namei.h"
#include "compat_slab.h"

#include "os.h"
#include "vmblockInt.h"
#include "filesystem.h"

#define VMBLOCK_ROOT_INO  1
#define GetRootInode(sb)  Iget(sb, NULL, NULL, VMBLOCK_ROOT_INO)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 25)
#   define KERNEL_25_FS 0
#else
#   define KERNEL_25_FS 1
#endif


/* File system operations */
#if KERNEL_25_FS /* { */
#   if defined(VMW_GETSB_2618)
static int FsOpGetSb(struct file_system_type *fsType, int flags,
                     const char *devName, void *rawData, struct vfsmount *mnt);
#   elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 70)
static struct super_block *FsOpGetSb(struct file_system_type *fsType, int flags,
                                     const char *devName, void *rawData);
#   else
static struct super_block *FsOpGetSb(struct file_system_type *fsType, int flags,
                                     char *devName, void *rawData);
#   endif
#else /* } { */
static struct super_block *FsOpReadSuper24(struct super_block *sb, void *rawData,
                                           int flags);
#endif /* } */
static int FsOpReadSuper(struct super_block *sb, void *rawData, int flags);


/* Utility */
static compat_kmem_cache_ctor InodeCacheCtor;


/* Variables */
compat_kmem_cache *VMBlockInodeCache;

/* Local variables */
static char const *fsRoot;
static size_t fsRootLen;
static struct file_system_type fsType = {
   .owner = THIS_MODULE,
   .name = VMBLOCK_FS_NAME,
#if KERNEL_25_FS
   .get_sb = FsOpGetSb,
   .kill_sb = kill_anon_super,
#else
   .read_super = FsOpReadSuper24,
#endif
};


/*
 * Public functions (with respect to the module)
 */

/*
 *----------------------------------------------------------------------------
 *
 * VMBlockInitFileSystem --
 *
 *    Initializes the file system and registers it with the kernel.
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
VMBlockInitFileSystem(char const *root)  // IN: directory redirecting to
{
   int ret;

   if (!root) {
      Warning("VMBlockInitFileSystem: root not provided "
              "(missing module parameter?)\n");
      return -EINVAL;
   }

   /*
    * Here we assume that the provided root is valid so the module will load.
    * The mount operation will fail if that is not the case.
    */
   fsRoot = root;
   fsRootLen = strlen(fsRoot);

   if (fsRootLen >= PATH_MAX) {
      return -ENAMETOOLONG;
   }

   /* Initialize our inode slab allocator */
   VMBlockInodeCache = os_kmem_cache_create("VMBlockInodeCache",
                                            sizeof (VMBlockInodeInfo),
                                            0,
                                            InodeCacheCtor);
   if (!VMBlockInodeCache) {
      Warning("VMBlockInitFileSystem: could not initialize inode cache\n");
      return -ENOMEM;
   }

   /* Tell the kernel about our file system */
   ret = register_filesystem(&fsType);
   if (ret < 0) {
      Warning("VMBlockInitFileSystem: could not initialize file system\n");
      kmem_cache_destroy(VMBlockInodeCache);
      return ret;
   }

   LOG(4, "file system registered with root of [%s]\n", fsRoot);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockCleanupFileSystem --
 *
 *    Cleans up file system and unregisters it with the kernel.
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
VMBlockCleanupFileSystem(void)
{
   int ret;

   kmem_cache_destroy(VMBlockInodeCache);

   ret = unregister_filesystem(&fsType);
   if (ret < 0) {
      Warning("VMBlockCleanupFileSystem: could not unregister file system\n");
      return ret;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * GetNextIno --
 *
 *    Gets the next available inode number.
 *
 * Results:
 *    The next available inode number.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

ino_t
GetNextIno(void)
{
   static spinlock_t inoLock = SPIN_LOCK_UNLOCKED;
   static ino_t nextIno = VMBLOCK_ROOT_INO + 1;
   ino_t ret;

   /* Too bad atomic_t's don't provide an atomic increment and read ... */
   spin_lock(&inoLock);
   ret = nextIno++;
   spin_unlock(&inoLock);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * Iget --
 *
 *    Lookup or create a new inode.
 *
 *    Inode creation in detail:
 *    Throughout the file system, we call the VFS iget() function to get a new
 *    inode.  This in turn invokes our file system's SuperOpAllocInode()
 *    function, which allocates an inode info structure (VMBlockInodeInfo)
 *    using the kernel's slab allocator.  When a new slab is created, each
 *    object is initialized with the constructor (InodeCacheCtor()), but that
 *    occurs only once per struct (e.g., when a struct from a slab is freed and
 *    reused, the constructor is not invoked again).  SuperOpAllocInode() then
 *    returns the address of the inode struct that is embedded within the inode
 *    info we have allocated.  iget() also invokes our SuperOpReadInode()
 *    function to do any further file system wide initialization to the inode,
 *    then returns the inode to us (this function).
 *
 *    Note that in older kernels that don't have the alloc_inode operation
 *    (where VMW_EMBED_INODE is undefined), the allocation is delayed until 
 *    this function and is contained within the INODE_TO_IINFO macro.  That
 *    allocation is freed in the SuperOpClearInode() function.
 *
 *    This function then constructs the full path of the actual file name and
 *    does a path_lookup() to see if it exists.  If it does, we save a pointer
 *    to the actual dentry within our inode info for future use.  If it
 *    doesn't, we still provide an inode but indicate that it doesn't exist by
 *    setting the actual dentry to NULL.  Callers that need to handle this case
 *    differently check for the existence of the actual dentry (and actual
 *    inode) to ensure the actual file exists.
 *
 * Results:
 *    A new inode object on success, NULL on error.
 *
 * Side effects:
 *    A path lookup is done for the actual file.
 *
 *----------------------------------------------------------------------------
 */

struct inode *
Iget(struct super_block *sb,    // IN: file system superblock object
     struct inode *dir,         // IN: containing directory
     struct dentry *dentry,     // IN: dentry within directory
     ino_t ino)                 // IN: inode number to assign to new inode
{
   VMBlockInodeInfo *iinfo;
   struct inode *inode;
   struct nameidata actualNd;

   ASSERT(sb);

   inode = iget(sb, ino);
   if (!inode) {
      return NULL;
   }

   iinfo = INODE_TO_IINFO(inode);
   if (!iinfo) {
      Warning("Iget: invalid inode provided, or unable to allocate inode info\n");
      goto error_inode;
   }

   /* Populate iinfo->name with the full path of the target file */
   if (MakeFullName(dir, dentry, iinfo->name, sizeof iinfo->name) < 0) {
      Warning("Iget: could not make full name\n");
      goto error_inode;
   }

   if (compat_path_lookup(iinfo->name, 0, &actualNd)) {
      /*
       * This file does not exist, so we create an inode that doesn't know
       * about its underlying file.  Operations that create files and
       * directories need an inode to operate on even if there is no actual
       * file yet.
       */
      iinfo->actualDentry = NULL;
      return inode;
   }

   iinfo->actualDentry = actualNd.dentry;
   path_release(&actualNd);

   return inode;

error_inode:
   iput(inode);
   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * InodeCacheCtor --
 *
 *    The constructor for inode info structs that occurs once at slab
 *    allocation.  That is, this is called once for each piece of memory that
 *    is used to satisfy inode info allocations; it should only be used to
 *    initialized items that will naturally return to their initialized state
 *    before deallocation (such as locks, list_heads).
 *
 *    We only invoke the inode's initialization routine since all of the inode
 *    info members need to be initialized on each allocation (in
 *    SuperOpReadInode()).
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef VMW_KMEMCR_CTOR_HAS_3_ARGS
static void
InodeCacheCtor(void *slabElem,           // IN: allocated slab item to initialize
               compat_kmem_cache *cache, // IN: cache slab is from
               unsigned long flags)      // IN: flags associated with allocation
#else
static void
InodeCacheCtor(compat_kmem_cache *cache, // IN: cache slab is from
               void *slabElem)           // IN: allocated slab item to initialize
#endif
{
#ifdef VMW_EMBED_INODE
   VMBlockInodeInfo *iinfo = (VMBlockInodeInfo *)slabElem;

   inode_init_once(&iinfo->inode);
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * MakeFullName --
 *
 *    Constructs the full filename from the provided directory and a dentry
 *    contained within it.
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
MakeFullName(struct inode *dir,       // IN : directory
             struct dentry *dentry,   // IN : dentry in that directory
             char *bufOut,            // OUT: output buffer
             size_t bufOutSize)       // IN : size of output buffer
{
   ASSERT(bufOut);

   /*
    * If dir is supplied, contruct the full path of the actual file, otherwise
    * it's the root directory.
    */
   if (dir == NULL) {
      if (fsRootLen >= bufOutSize) {
         Warning("MakeFullName: root path was too long.\n");
         return -ENAMETOOLONG;
      }
      memcpy(bufOut, fsRoot, fsRootLen);
      bufOut[fsRootLen] = '\0';
   } else {
      VMBlockInodeInfo *dirIinfo;

      ASSERT(dir);
      ASSERT(dentry);

      if (!dentry->d_name.name) {
         Warning("MakeFullName: dentry name is empty\n");
         return -EINVAL;
      }

      dirIinfo = INODE_TO_IINFO(dir);
      /*
       * If dirIinfo->name[1] is '\0', then it is "/" and we don't need
       * another '/' between it and the additional name.
       */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
      {
         int ret;

         ret = snprintf(bufOut, bufOutSize,
                        dirIinfo->name[1] == '\0' ? "%s%s" : "%s/%s",
                        dirIinfo->name, dentry->d_name.name);
         if (ret >= bufOutSize) {
            Warning("MakeFullName: path was too long.\n");
            return -ENAMETOOLONG;
         }
      }
#else
      {
         /* snprintf was not exported prior to 2.4.10 */
         size_t dirLen;
         size_t pathSepLen;
         size_t dentryLen;
         size_t pathLen;

         dirLen = strlen(dirIinfo->name);
         pathSepLen = dirLen == 1 ? 0 : 1;
         dentryLen = strlen(dentry->d_name.name);
         pathLen = dirLen + dentryLen + pathSepLen;
         if (pathLen >= bufOutSize) {
            Warning("MakeFullName: path was too long.\n");
            return -ENAMETOOLONG;
         }
         memcpy(bufOut, dirIinfo->name, dirLen);
         if (pathSepLen == 1) {
            ASSERT(dirLen == 1);
            bufOut[dirLen] = '/';
         }
         memcpy(bufOut + dirLen + pathSepLen, dentry->d_name.name, dentryLen);
         bufOut[pathLen] = '\0';
      }
#endif
   }

   return 0;
}


/* File system operations */

/*
 *-----------------------------------------------------------------------------
 *
 * FsOpReadSuper --
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
FsOpReadSuper(struct super_block *sb, // OUT: Superblock object
              void *rawData,          // IN: Fs-specific mount data
              int flags)              // IN: Mount flags
{
   struct inode *rootInode;
   struct dentry *rootDentry;

   if (!sb) {
      Warning("FsOpReadSuper: invalid arg from kernel\n");
      return -EINVAL;
   }

   sb->s_magic = VMBLOCK_SUPER_MAGIC;
   sb->s_blocksize = 1024;
   sb->s_op = &VMBlockSuperOps;

   /*
    * Make root inode and dentry.  Ensure that the directory we are redirecting
    * to has an actual dentry and inode, and that it is in fact a directory.
    */
   rootInode = GetRootInode(sb);
   if (!rootInode) {
      return -EINVAL;
   }

   if (!INODE_TO_IINFO(rootInode) ||
       !INODE_TO_ACTUALDENTRY(rootInode) ||
       !INODE_TO_ACTUALINODE(rootInode) ||
       !S_ISDIR(INODE_TO_ACTUALINODE(rootInode)->i_mode)) {
      iput(rootInode);
      return -EINVAL;
   }

   rootDentry = d_alloc_root(rootInode);
   if (!rootDentry) {
      iput(rootInode);
      return -ENOMEM;
   }
   sb->s_root = rootDentry;

   rootInode->i_op = &RootInodeOps;
   rootInode->i_fop = &RootFileOps;
   rootInode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;

   LOG(4, "%s file system mounted\n", VMBLOCK_FS_NAME);
   return 0;
}


#if KERNEL_25_FS /* { */
#if defined(VMW_GETSB_2618)
/*
 *-----------------------------------------------------------------------------
 *
 * FsOpGetSb --
 *
 *    Invokes generic kernel code to prepare superblock for
 *    deviceless filesystem.
 *
 * Results:
 *    0 on success
 *    negative error code on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
FsOpGetSb(struct file_system_type *fs_type, // IN: file system type of mount
          int flags,                        // IN: mount flags
          const char *dev_name,             // IN: device mounting on
          void *rawData,                    // IN: mount arguments
          struct vfsmount *mnt)             // IN: vfs mount
{
   return get_sb_nodev(fs_type, flags, rawData, FsOpReadSuper, mnt);
}
#else
/*
 *-----------------------------------------------------------------------------
 *
 * FsOpGetSb --
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
FsOpGetSb(struct file_system_type *fs_type, // IN: file system type of mount
          int flags,                        // IN: mount flags
          const char *dev_name,             // IN: device mounting on
          void *rawData)                    // IN: mount arguments
#else
static struct super_block *
FsOpGetSb(struct file_system_type *fs_type, // IN: file system type of mount
          int flags,                        // IN: mount flags
          char *dev_name,                   // IN: device mounting on
          void *rawData)                    // IN: mount arguments
#endif
{
   return get_sb_nodev(fs_type, flags, rawData, FsOpReadSuper);
}
#endif
#else /* } { */

/*
 *-----------------------------------------------------------------------------
 *
 * FsOpReadSuper24 --
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
FsOpReadSuper24(struct super_block *sb,  // OUT: Superblock object
                void *rawData,           // IN : mount arguments
                int flags)               // IN : mount flags
{
   return FsOpReadSuper(sb, rawData, flags) ? NULL : sb;
}
#endif /* } */
