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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/mount.h>

#include "compat_fs.h"
#include "compat_namei.h"

#include "os.h"
#include "vmblockInt.h"
#include "filesystem.h"

#define VMBLOCK_ROOT_INO  1
#define GetRootInode(sb)  Iget(sb, NULL, NULL, VMBLOCK_ROOT_INO)

static struct inode *GetInode(struct super_block *sb, ino_t ino);

/* File system operations */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
static struct dentry *FsOpMount(struct file_system_type *fsType, int flags,
                                const char *devName, void *rawData);
#elif defined(VMW_GETSB_2618)
static int FsOpGetSb(struct file_system_type *fsType, int flags,
                     const char *devName, void *rawData, struct vfsmount *mnt);
#else
static struct super_block *FsOpGetSb(struct file_system_type *fsType, int flags,
                                     const char *devName, void *rawData);
#endif
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
   .mount = FsOpMount,
#else
   .get_sb = FsOpGetSb,
#endif
   .kill_sb = kill_anon_super,
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

   ret = unregister_filesystem(&fsType);
   if (ret < 0) {
      Warning("VMBlockCleanupFileSystem: could not unregister file system\n");
      return ret;
   }

   kmem_cache_destroy(VMBlockInodeCache);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 *  VMBlockReadInode --
 *
 *    A filesystem wide function that is called to initialize a new inode.
 *    This is called from two different places depending on the kernel version.
 *    In older kernels that provide the iget() interface, this function is
 *    called by the kernel as part of inode initialization (from
 *    SuperOpReadInode). In newer kernels that call iget_locked(), this
 *    function is called by filesystem code to initialize the new inode.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
VMBlockReadInode(struct inode *inode)  // IN: Inode to initialize
{
   VMBlockInodeInfo *iinfo = INODE_TO_IINFO(inode);

   iinfo->name[0] = '\0';
   iinfo->nameLen = 0;
   iinfo->actualDentry = NULL;
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
   static atomic_t nextIno = ATOMIC_INIT(VMBLOCK_ROOT_INO + 1);

   return (ino_t) atomic_inc_return(&nextIno);
}


/*
 *----------------------------------------------------------------------------
 *
 * GetInode --
 *
 *    This function replaces iget() and should be called instead of it. In newer
 *    kernels that have removed the iget() interface,  GetInode() obtains an inode
 *    and if it is a new one, then initializes the inode by calling
 *    VMBlockReadInode(). In older kernels that support the iget() interface,
 *    VMBlockReadInode() is called by iget() internally by the superblock function
 *    SuperOpReadInode.
 *
 * Results:
 *    A new inode object on success, NULL on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static struct inode *
GetInode(struct super_block *sb, // IN: file system superblock object
	 ino_t ino)              // IN: inode number to assign to new inode
{
   struct inode *inode;

   inode = iget_locked(sb, ino);
   if (!inode) {
      return NULL;
   } else if (inode->i_state & I_NEW) {
      VMBlockReadInode(inode);
      unlock_new_inode(inode);
   }
   return inode;
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

   inode = GetInode(sb, ino);
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

   iinfo->actualDentry = compat_vmw_nd_to_dentry(actualNd);
   compat_path_release(&actualNd);

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

static void
InodeCacheCtor(COMPAT_KMEM_CACHE_CTOR_ARGS(slabElem))  // IN: allocated slab item to initialize
{
   VMBlockInodeInfo *iinfo = slabElem;

   inode_init_once(&iinfo->inode);
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
      int ret;

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
      ret = snprintf(bufOut, bufOutSize,
                     dirIinfo->name[1] == '\0' ? "%s%s" : "%s/%s",
                     dirIinfo->name, dentry->d_name.name);
      if (ret >= bufOutSize) {
         Warning("MakeFullName: path was too long.\n");
         return -ENAMETOOLONG;
      }
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

   rootDentry = d_make_root(rootInode);
   if (!rootDentry) {
      return -ENOMEM;
   }
   sb->s_root = rootDentry;

   rootInode->i_op = &RootInodeOps;
   rootInode->i_fop = &RootFileOps;
   rootInode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;

   LOG(4, "%s file system mounted\n", VMBLOCK_FS_NAME);
   return 0;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
/*
 *-----------------------------------------------------------------------------
 *
 * FsOpMount --
 *
 *    Invokes generic kernel code to mount a deviceless filesystem.
 *
 * Results:
 *    Mount's root dentry tructure on success
 *    ERR_PTR()-encoded negative error code on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

struct dentry *
FsOpMount(struct file_system_type *fs_type, // IN: file system type of mount
          int flags,                        // IN: mount flags
          const char *dev_name,             // IN: device mounting on
          void *rawData)                    // IN: mount arguments
{
   return mount_nodev(fs_type, flags, rawData, FsOpReadSuper);
}
#elif defined(VMW_GETSB_2618)
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

static struct super_block *
FsOpGetSb(struct file_system_type *fs_type, // IN: file system type of mount
          int flags,                        // IN: mount flags
          const char *dev_name,             // IN: device mounting on
          void *rawData)                    // IN: mount arguments
{
   return get_sb_nodev(fs_type, flags, rawData, FsOpReadSuper);
}
#endif

