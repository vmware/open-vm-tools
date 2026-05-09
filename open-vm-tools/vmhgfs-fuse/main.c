/*********************************************************
 * Copyright (c) 2013,2021 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * main.c --
 *
 * Main entry points for fuse file operations for HGFS
 */

#include "module.h"
#include "cache.h"
#include "filesystem.h"
#include "file.h"

/*
 *----------------------------------------------------------------------
 *
 * getAbsPath
 *
 *    This function takes the file path used by the local file system
 *    (which is relative to the HGFS mount) and generates the HGFS absolute
 *    path to send to the HGFS server.
 *    the absolute path will be allocated, which is the HGFS share and
 *    all other intermediate subdirectories that contain the file or folder.
 *
 * Results:
 *    zero on success, negative number for error.
 *
 * Side effects:
 *    Caller is responsible to free the absolute path if it's different
 *    from the path.
 *
 *----------------------------------------------------------------------
 */

static int
getAbsPath(const char *path, // IN
           char **abspath)   // OUT
{
   size_t size;
   int res = 0;

   if (gState->basePathLen > 0) {
      size = gState->basePathLen + strlen(path) + 1;
      *abspath = (char *)malloc(size);
      if (*abspath == NULL) {
         LOG(4, ("Can't allocate memory!\n"));
         res = -ENOMEM;
         goto exit;
      }
      snprintf(*abspath, size, "%s%s", gState->basePath, path);

   } else {
      *abspath = (char *)path;
   }

exit:
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * freeAbsPath
 *
 *    This function free's the absolute path allocated from getAbsPath
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
freeAbsPath(char *abspath)  // IN
{
   if (gState->basePathLen > 0) {
      free(abspath);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_getattr
 *
 *    Get the attributes from the HGFS server and populate struct stat.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static int
hgfs_getattr(const char *path,           //IN: path of a file/directory
             struct stat *stbuf,         //IN/OUT: file/directoy attribute
             struct fuse_file_info *fi)  //IN/OUT: Unused
#else
static int
hgfs_getattr(const char *path,    //IN: path of a file/directory
             struct stat *stbuf)  //IN/OUT: file/directoy attribute
#endif
{
   HgfsHandle fileHandle = HGFS_INVALID_HANDLE;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *attr = &newAttr;
   uint32 d_type;
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s)\n", path));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsGetAttrCache(abspath, attr);
   LOG(4, ("Retrieve attr from cache. result = %d \n", res));
   if (res != 0) {
      /* Retrieve new complete attribute settings and update the cache. */
      res = HgfsPrivateGetattr(fileHandle, abspath, attr);
      LOG(4, ("Retrieve attr from server. result = %d \n", res));
      if (res == 0 ) {
         HgfsSetAttrCache(abspath, attr);
      }
   }

   if (res < 0) {
      goto exit;
   }

   LOG(4, ("fill stat for %s\n", abspath));

   memset(stbuf, 0, sizeof *stbuf);

   if (attr->mask & HGFS_ATTR_VALID_SPECIAL_PERMS) {
      stbuf->st_mode |= (attr->specialPerms << 9);
   }
   if (attr->mask & HGFS_ATTR_VALID_OWNER_PERMS) {
      stbuf->st_mode |= (attr->ownerPerms << 6);
   }
   if (attr->mask & HGFS_ATTR_VALID_GROUP_PERMS) {
      stbuf->st_mode |= (attr->groupPerms << 3);
   }
   if (attr->mask & HGFS_ATTR_VALID_OTHER_PERMS) {
      stbuf->st_mode |= (attr->otherPerms);
   }

   /* Mask the access mode. */
   switch (attr->type) {
   case HGFS_FILE_TYPE_SYMLINK:
      d_type = DT_LNK;
      break;

   case HGFS_FILE_TYPE_REGULAR:
      d_type = DT_REG;
      break;

   case HGFS_FILE_TYPE_DIRECTORY:
      d_type = DT_DIR;
      break;

   default:
      d_type = DT_UNKNOWN;
      break;
   }

   stbuf->st_mode |= d_type << 12;
   stbuf->st_blksize = HGFS_BLOCKSIZE;
   stbuf->st_blocks = HgfsCalcBlockSize(attr->size);
   stbuf->st_size = attr->size;
   stbuf->st_ino = attr->hostFileId;
   stbuf->st_nlink = 1;
   stbuf->st_uid = attr->userId;
   stbuf->st_gid = attr->groupId;
   stbuf->st_rdev = 0;

   if (attr->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
#if HAVE_STRUCT_STAT_ST_MTIMESPEC
      HGFS_SET_TIME(stbuf->st_atimespec, attr->accessTime);
#elif HAVE_STRUCT_STAT_ST_MTIM
      HGFS_SET_TIME(stbuf->st_atim, attr->accessTime);
#else
      HGFS_SET_TIME(stbuf->st_atime, attr->accessTime);
#endif
   }
   if (attr->mask & HGFS_ATTR_VALID_WRITE_TIME) {
#if HAVE_STRUCT_STAT_ST_MTIMESPEC
      HGFS_SET_TIME(stbuf->st_mtimespec, attr->writeTime);
#elif HAVE_STRUCT_STAT_ST_MTIM
      HGFS_SET_TIME(stbuf->st_mtim, attr->writeTime);
#else
      HGFS_SET_TIME(stbuf->st_mtime, attr->writeTime);
#endif
   }
   if (attr->mask & HGFS_ATTR_VALID_CHANGE_TIME) {
#if HAVE_STRUCT_STAT_ST_MTIMESPEC
      HGFS_SET_TIME(stbuf->st_ctimespec, attr->attrChangeTime);
#elif HAVE_STRUCT_STAT_ST_MTIM
      HGFS_SET_TIME(stbuf->st_ctim, attr->attrChangeTime);
#else
      HGFS_SET_TIME(stbuf->st_ctime, attr->attrChangeTime);
#endif
   }

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_access
 *
 *    Dummy routine, not fully implemented.
 *
 * Results:
 *    Returns zero.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_access(const char *path,  //IN: Path to a file.
            int mask)          //IN: Mask
{
   HgfsHandle fileHandle = HGFS_INVALID_HANDLE;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *attr = &newAttr;
   uint32 effectivePermissions;
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, mask = %#o)\n", path, mask));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsGetAttrCache(path, attr);
   LOG(4, ("Retrieve attr from cache. result = %d \n", res));
   if (res != 0) {
      /* Retrieve new complete attribute settings and update the cache. */
      res = HgfsPrivateGetattr(fileHandle, abspath, attr);
      LOG(4, ("Retrieve attr from server. result = %d \n", res));
      if (res == 0 ) {
         HgfsSetAttrCache(abspath, attr);
      }
   }

   if (res < 0) {
      goto exit;
   }

   if (mask == F_OK) {
      res = 0;  /* assume the above attr retrieval did the validation */
      goto exit;
   }

   if (attr->mask & HGFS_ATTR_VALID_EFFECTIVE_PERMS) {
      effectivePermissions = attr->effectivePerms;
   } else {
     /*
      * If the server did not return actual effective permissions then
      * need to calculate ourselves. However we should avoid unnecessary
      * denial of access so perform optimistic permissions calculation.
      * It is safe since host enforces necessary restrictions regardless of
      * the client's decisions.
      */
     effectivePermissions = (attr->ownerPerms |
                             attr->groupPerms |
                             attr->otherPerms);
  }

  if ((effectivePermissions & mask) != mask) {
     res = -EACCES;
  }

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_readlink
 *
 *    Read the file pointed by the symbolic link.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_readlink(const char *path, //IN: path to a file
              char *buf,        //OUT: buffer to store the filename
              size_t size)      //IN: size of buf
{
   char *abspath = NULL;
   int res = 0;
   HgfsHandle fileHandle = 0;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *attr = &newAttr;

   LOG(4, ("Entry(path = %s, %#"FMTSZ"x)\n", path, size));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   /* The attributes fileName field will hold the symlink target name. */
   res = HgfsPrivateGetattr(fileHandle, abspath, attr);
   LOG(4, ("ReadLink: Path = %s, attr->fileName = %s \n", abspath, attr->fileName));
   if (res < 0) {
      goto exit;
   }

   if (size > strlen(attr->fileName)) {
      Str_Strcpy(buf, attr->fileName,
                 strlen(attr->fileName) + 1);
      LOG(4, ("ReadLink: link target name = %s\n", buf));
   } else {
      res = -ENOBUFS;
   }

exit:
   free(attr->fileName);
   freeAbsPath(abspath);
   LOG(4, ("Exit(%d)\n", res));
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_readdir
 *
 *    Read the directoy file.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static int
hgfs_readdir(const char *path,              //IN: path to a directory
             void *buf,                     //OUT: buffer to fill the dir entry
             fuse_fill_dir_t filler,        //IN: function pointer to fill buf
             off_t offset,                  //IN: offset to read the dir
             struct fuse_file_info *fi,     //IN: file info set by open call
             enum fuse_readdir_flags flags) //IN: unused
#else
static int
hgfs_readdir(const char *path,          //IN: path to a directory
             void *buf,                 //OUT: buffer to fill the dir entry
             fuse_fill_dir_t filler,    //IN: function pointer to fill buf
             off_t offset,              //IN: offset to read the dir
             struct fuse_file_info *fi) //IN: file info set by open call
#endif
{
   char *abspath = NULL;
   int res = 0;
   HgfsHandle fileHandle = HGFS_INVALID_HANDLE;

   LOG(4, ("Entry(path = %s, @ %#"FMT64"x)\n", path, offset));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsDirOpen(abspath, &fileHandle);
   if (res < 0) {
      goto exit;
   }

   fi->fh = fileHandle;
   res = HgfsReaddir(fileHandle, buf, filler);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_mknod
 *
 *    Dummy routine.
 *
 * Results:
 *    Returns -1.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_mknod(const char *path,   //IN: Path to a file
           mode_t mode,        //IN: Mode to set
           dev_t rdev)         //IN: Device type
{
#if defined(__APPLE__)
   LOG(4, ("Entry(path = %s, mode = %#o, %u)\n", path, mode, rdev));
#else
   LOG(4, ("Entry(path = %s, mode = %#o, %"FMT64"u)\n", path, mode, rdev));
#endif
   LOG(4, ("Dummy routine. Not implemented!"));
   LOG(4, ("Exit(0)\n"));
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_mkdir
 *
 *    Create directory.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_mkdir(const char *path,  //IN: path to a new dir
           mode_t mode)       //IN: Mode of dir to be created
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, mode = %#o)\n", path, mode));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsMkdir(abspath, mode);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_unlink
 *
 *    Delete file.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_unlink(const char *path) //IN: path to a file
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s)\n", path));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsDelete(abspath, HGFS_OP_DELETE_FILE);
   if (res == 0) {
      HgfsInvalidateAttrCache(abspath);
   }

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_rmdir
 *
 *    Delete directory.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_rmdir(const char *path) //IN: path to a dir
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s)\n", path));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsDelete(abspath, HGFS_OP_DELETE_DIR);
   if (res == 0) {
      HgfsInvalidateAttrCache(abspath);
   }

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_symlink
 *
 *    Create symbolic link.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_symlink(const char *symname,   //IN: symname target
             const char *source)    //IN: source name
{
   char *absSource = NULL;
   int res;

   LOG(4, ("Entry(from = %s, to = %s)\n", symname, source));
   res = getAbsPath(source, &absSource);
   if (res < 0) {
      goto exit;
   }

   LOG(4, ("symname = %s, abs source = %s)\n", symname, absSource));
   res = HgfsSymlink(absSource, symname);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(absSource);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_rename
 *
 *    Rename file or directory.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static int
hgfs_rename(const char *from,    //IN: from path name
            const char *to,      //IN: to path name
            unsigned int flags)  //IN: unused
#else
static int
hgfs_rename(const char *from,  //IN: from path name
            const char *to)    //IN: to path name
#endif
{
   char *absfrom = NULL;
   char *absto = NULL;
   int res;

   LOG(4, ("Entry(from = %s, to = %s)\n", from, to));
   res = getAbsPath(from, &absfrom);
   if (res < 0) {
      goto exit;
   }
   res = getAbsPath(to, &absto);
   if (res < 0) {
      goto exit;
   }

   res = HgfsRename(absfrom, absto);
   if (res == 0) {
      HgfsInvalidateAttrCache(absfrom);
      HgfsInvalidateAttrCache(absto);
   }

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(absfrom);
   freeAbsPath(absto);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_link
 *
 *    Dummy routine.
 *
 * Results:
 *    Returns -1.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_link(const char *from,  //IN: From path
          const char *to)    //IN: To path
{
   char *absfrom = NULL;
   char *absto = NULL;
   int res = 0;

   LOG(4, ("Entry(from = %s, to = %s)\n", from, to));
   res = getAbsPath(from, &absfrom);
   if (res < 0) {
      goto exit;
   }
   res = getAbsPath(to, &absto);
   if (res < 0) {
      goto exit;
   }

   /*Do nothing.*/
   res = -1;

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(absfrom);
   freeAbsPath(absto);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_chmod
 *
 *    Change access mode of a file.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static int
hgfs_chmod(const char *path,          //IN: path to a file
           mode_t mode,               //IN: mode to set
           struct fuse_file_info *fi) //IN/OUT: unused
#else
static int
hgfs_chmod(const char *path,   //IN: path to a file
           mode_t mode)        //IN: mode to set
#endif
{
   char *abspath = NULL;
   int res;
   HgfsHandle fileHandle = HGFS_INVALID_HANDLE;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *attr = &newAttr;

   LOG(4, ("Entry(path = %s, mode = %#o)\n", path, mode));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   attr->mask = (HGFS_ATTR_VALID_SPECIAL_PERMS |
                 HGFS_ATTR_VALID_OWNER_PERMS |
                 HGFS_ATTR_VALID_GROUP_PERMS |
                 HGFS_ATTR_VALID_OTHER_PERMS);
   attr->specialPerms = (mode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
   attr->ownerPerms = (mode & S_IRWXU) >> 6;
   attr->groupPerms = (mode & S_IRWXG) >> 3;
   attr->otherPerms = mode & S_IRWXO;

   attr->mask |= HGFS_ATTR_VALID_ACCESS_TIME;
   attr->accessTime = attr->attrChangeTime = HGFS_GET_CURRENT_TIME();

   res = HgfsSetattr(abspath, attr);
   if (res < 0) {
      LOG(4, ("path = %s , HgfsSetattr failed. res = %d\n", abspath, res));
      goto exit;
   }

   /* Retrieve new complete attribute settings and update the cache. */
   res = HgfsPrivateGetattr(fileHandle, abspath, attr);
   if (res < 0) {
      LOG(4, ("path = %s , res = %d\n", abspath, res));
      goto exit;
   }
   HgfsSetAttrCache(abspath, attr);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_chown
 *
 *    Change uid or gid of a file.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static int
hgfs_chown(const char *path,           //IN: Path to a file
           uid_t uid,                  //IN: User id
           gid_t gid,                  //IN: Group id
           struct fuse_file_info *fi)  //IN/OUT: unused
#else
static int
hgfs_chown(const char *path,  //IN: Path to a file
           uid_t uid,         //IN: User id
           gid_t gid)         //IN: Group id
#endif
{
   HgfsHandle fileHandle = HGFS_INVALID_HANDLE;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *attr = &newAttr;
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, uid = %u, gid = %u)\n", abspath, uid, gid));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   attr->mask = HGFS_ATTR_VALID_USERID;
   attr->userId = uid;

   attr->mask |= HGFS_ATTR_VALID_GROUPID;
   attr->groupId = gid;

   attr->mask |= HGFS_ATTR_VALID_ACCESS_TIME;
   attr->accessTime = attr->attrChangeTime = HGFS_GET_CURRENT_TIME();

   res = HgfsSetattr(abspath, attr);
   if (res < 0) {
      LOG(4, ("path = %s , HgfsSetattr failed. res = %d\n", abspath, res));
      goto exit;
   }

   /* Retrieve new complete attribute settings and update the cache. */
   res = HgfsPrivateGetattr(fileHandle, abspath, attr);
   if (res < 0) {
      LOG(4, ("path = %s , res = %d\n", abspath, res));
      goto exit;
   }
   HgfsSetAttrCache(abspath, attr);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_truncate
 *
 *    Truncate a file to the given size.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static int
hgfs_truncate(const char *path,           //IN: path to a file
              off_t size,                 //IN: new size
              struct fuse_file_info *fi)  //IN/OUT: unused
#else
static int
hgfs_truncate(const char *path,  //IN: path to a file
              off_t size)        //IN: new size
#endif
{
   HgfsHandle fileHandle = HGFS_INVALID_HANDLE;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *attr = &newAttr;
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, size %"FMT64"x)\n", path, size));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   attr->mask = HGFS_ATTR_VALID_SIZE;
   attr->size = size;

   attr->mask |= (HGFS_ATTR_VALID_WRITE_TIME |
                  HGFS_ATTR_VALID_ACCESS_TIME |
                  HGFS_ATTR_VALID_CHANGE_TIME);
   attr->writeTime = attr->accessTime = attr->attrChangeTime = HGFS_GET_CURRENT_TIME();

   res = HgfsSetattr(abspath, attr);
   if (res < 0) {
      LOG(4, ("path = %s , HgfsSetattr failed. res = %d\n", abspath, res));
      goto exit;
   }

   /* Retrieve new complete attribute settings and update the cache. */
   res = HgfsPrivateGetattr(fileHandle, abspath, attr);
   if (res < 0) {
      LOG(4, ("path = %s , res = %d\n", abspath, res));
      goto exit;
   }
   HgfsSetAttrCache(abspath, attr);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_utimens/hgfs_utime
 *
 *    Update access and write time of a file.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static int
hgfs_utimens(const char *path,              //IN: path to a file
             const struct timespec ts[2],   //IN: new time
             struct fuse_file_info *fi)     //IN/OUT: unused
#else
static int
hgfs_utimens(const char *path,              //IN: path to a file
             const struct timespec ts[2])   //IN: new time
#endif
{
   HgfsHandle fileHandle = HGFS_INVALID_HANDLE;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *attr = &newAttr;
   time_t accessTimeSec, accessTimeNsec, writeTimeSec, writeTimeNsec;
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s)\n", path));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsGetAttrCache(abspath, attr);
   LOG(4, ("Retrieve attr from cache. result = %d \n", res));
   if (res != 0) {
      /* Retrieve new complete attribute settings and update the cache. */
      res = HgfsPrivateGetattr(fileHandle, abspath, attr);
      LOG(4, ("Retrieve attr from server. result = %d \n", res));
      if (res == 0 ) {
         HgfsSetAttrCache(abspath, attr);
      }
   }

   if (res != 0) {
      /* Failed to retrieve file attributes. */
      goto exit;
   }

   if (attr->type == HGFS_FILE_TYPE_SYMLINK) {
      /*
       * utimensat() has a 'flag' parameter which is not available in fuse.
       * by default, let's assume AT_SYMLINK_NOFOLLOW. but since there is
       * neither a way to pass not 'followSymlinks' to setattr, let's simply
       * do nothing for symlink.
       */
      goto exit;
   }

   attr->mask = (HGFS_ATTR_VALID_WRITE_TIME |
                 HGFS_ATTR_VALID_ACCESS_TIME);

   accessTimeSec = ts[0].tv_sec;
   accessTimeNsec = ts[0].tv_nsec;
   writeTimeSec = ts[1].tv_sec;
   writeTimeNsec = ts[1].tv_nsec;
   attr->accessTime = HgfsConvertToNtTime(accessTimeSec, accessTimeNsec);
   attr->writeTime = HgfsConvertToNtTime(writeTimeSec, writeTimeNsec);

   res = HgfsSetattr(abspath, attr);
   if (res < 0) {
      LOG(4, ("abspath = %s , HgfsSetattr failed. res = %d\n", abspath, res));
      goto exit;
   }

   /* Retrieve new complete attribute settings and update the cache. */
   res = HgfsPrivateGetattr(fileHandle, abspath, attr);
   if (res < 0) {
      LOG(4, ("abspath = %s , res = %d\n", abspath, res));
      goto exit;
   }
   HgfsSetAttrCache(abspath, attr);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_open
 *
 *    Open file at a given path.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_open(const char *path,          //IN: path to a file
          struct fuse_file_info *fi) //IN: file info structure
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s)\n", path));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsOpen(abspath, fi);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_create
 *
 *    Create a new file, we do it by calling HgfsOpen.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_create(const char *path,          //IN: path to a file
            mode_t mode,               //IN: file mode
            struct fuse_file_info *fi) //IN: file info structure
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, mode = %#o)\n", path, mode));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsCreate(abspath, mode, fi);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_read
 *
 *    Read the file using the handle, if the handle is zero
 *    then open the file first and then read.
 *
 * Results:
 *    Returns the number of bytes read from the file.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_read(const char *path,          //IN: path to a file
          char *buf,                 //OUT: to store the data
          size_t size,               //IN: size to read
          off_t offset,              //IN: starting point to read
          struct fuse_file_info *fi) //IN: file info structure
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, fi->fh = %#"FMT64"x, %#"FMTSZ"x bytes @ %#"FMT64"x)\n",
           path, fi->fh, size, offset));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   if (fi->fh == HGFS_INVALID_HANDLE) {
      res = HgfsOpen(abspath, fi);
      if (res) {
         goto exit;
      }
   }
   res = HgfsRead(fi, buf, size, offset);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_write
 *
 *    Write to the file using the handle, if the handle is zero
 *    then open the file first and then write.
 *
 * Results:
 *    Returns the number of bytes written to the file.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_write(const char *path,          //IN: path to a file
           const char *buf,           //IN: data to write
           size_t size,               //IN: size to write
           off_t offset,              //IN: starting point to write
           struct fuse_file_info *fi) //IN: file info structure
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, fi->fh = %#"FMT64"x, write %#"FMTSZ"x bytes @ %#"FMT64"x)\n",
           path, fi->fh, size, offset));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   if (fi->fh == HGFS_INVALID_HANDLE) {
      res = HgfsOpen(abspath, fi);
      if (res) {
         goto exit;
      }
   }

   res = HgfsWrite(fi, buf, size, offset);
   if (res >= 0) {
      /*
       * Positive result indicates the number of bytes written.
       * For zero bytes and no error, we still purge the cache
       * this could effect the attributes.
       */
      HgfsInvalidateAttrCache(abspath);
   }

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}

/*
 *----------------------------------------------------------------------
 *
 * hgfs_statfs
 *
 *    Stat the host for total and free bytes on disk.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_statfs(const char *path,      //IN: Path to the filesystem
            struct statvfs *stbuf) //OUT:Struct to fill data
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s)\n", path));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsStatfs(abspath, stbuf);

exit:
   LOG(4, ("Exit(%d)\n", res));
   freeAbsPath(abspath);
   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_release
 *
 *    Release a file.
 *
 * Results:
 *    Returns zero.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
hgfs_release(const char *path,                //IN: path to a file
             struct fuse_file_info *fi)       //IN: file info structure
{
   char *abspath = NULL;
   int res;

   LOG(4, ("Entry(path = %s, fi->fh = %#"FMT64"x)\n", path, fi->fh));
   res = getAbsPath(path, &abspath);
   if (res < 0) {
      goto exit;
   }

   res = HgfsRelease(fi->fh);
   if (0 == res) {
      fi->fh = HGFS_INVALID_HANDLE;
   }

exit:
   LOG(4, ("Exit(0)\n"));
   freeAbsPath(abspath);
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_init
 *
 *    Initialization routine. We spawn the cache purge thread here.
 *
 * Results:
 *    Returns NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if FUSE_MAJOR_VERSION == 3
static void*
hgfs_init(struct fuse_conn_info *conn, // IN: unused
          struct fuse_config *cfg)     // IN/OUT: unused
#else
static void*
hgfs_init(struct fuse_conn_info *conn) // IN: unused
#endif
{
   pthread_t purgeCacheThread;
   int dummy;
   int res;

   LOG(4, ("Entry()\n"));

   /*
    * dummy argument is required for Solaris and FreeBSD while creating
    * thread otherwise the program crashes.
    */
   res = pthread_create(&purgeCacheThread, NULL,
                        HgfsPurgeCache, &dummy);
   if (res < 0) {
      LOG(4, ("Pthread create fail. error = %d\n", res));
   }

   res = HgfsCreateSession();
   if (res < 0) {
      LOG(4, ("Create session failed. error = %d\n", res));
   }

   LOG(4, ("Exit(NULL)\n"));
   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * hgfs_destroy
 *
 *    Cleanup routine.
 *
 * Results:
 *    Returns NULL.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
hgfs_destroy(void *data) // IN: unused
{
   int res;

   LOG(4, ("Entry()\n"));

   res = HgfsDestroySession();
   if (res < 0) {
      LOG(4, ("Destroy session failed. error = %d\n", res));
   }

   HgfsTransportExit();

   free(gState->basePath);

   if (gState->conf != NULL) {
      g_key_file_free(gState->conf);
      gState->conf = NULL;
   }

   LOG(4, ("Exit()\n"));
}


/*--------------------------------------------------------------------------- */
static struct fuse_operations vmhgfs_operations = {
   .getattr     = hgfs_getattr,
   .access      = hgfs_access,
   .readlink    = hgfs_readlink,
   .readdir     = hgfs_readdir,
   .mknod       = hgfs_mknod,
   .mkdir       = hgfs_mkdir,
   .symlink     = hgfs_symlink,
   .unlink      = hgfs_unlink,
   .rmdir       = hgfs_rmdir,
   .rename      = hgfs_rename,
   .link        = hgfs_link,
   .chmod       = hgfs_chmod,
   .chown       = hgfs_chown,
   .truncate    = hgfs_truncate,
   .utimens     = hgfs_utimens,
   .open        = hgfs_open,
   .read        = hgfs_read,
   .write       = hgfs_write,
   .statfs      = hgfs_statfs,
   .release     = hgfs_release,
   .create      = hgfs_create,
   .init        = hgfs_init,
   .destroy     = hgfs_destroy,
};


/*
 *----------------------------------------------------------------------
 *
 * main
 *
 *    Starting point of the program.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
main(int argc,       //IN: Argument count
     char *argv[])   //IN: Argument list
{
   struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
   int res;

   res = vmhgfsPreprocessArgs(&args);
   if (res != 0) {
      fprintf(stderr, "Error parsing arguments!\n");
      exit(1);
   }

   /* Initialization */
   umask(0);
   HgfsResetOps();
   res = HgfsTransportInit();
   if (res != 0) {
      fprintf(stderr, "Error %d cannot open connection!\n", res);
      return res;
   }
   HgfsInitCache();

   return fuse_main(args.argc, args.argv, &vmhgfs_operations, NULL);
}

