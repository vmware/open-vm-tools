/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * fsutil.c --
 *
 * VFS helper functions that are shared between the FreeBSD and Mac OS
 * implementaitons of HGFS.
 */

#include <sys/types.h>
#include <sys/malloc.h>

#if defined __APPLE__
#  include <libkern/libkern.h>  // for rindex
#endif

#include "fsutil.h"
#include "cpName.h"
#include "hgfsEscape.h"
#include "cpNameLite.h"
#include "os.h"

#if defined __APPLE__
char *rindex(const char *ptr, int chr);
#endif

/*
 * Mac OS sets vnode attributes through the use of a VATTR_RETURN function.
 * FreeBSD sets vnode attributes directly in the structure. To enable a shared
 * implementation of HgfsAttrToBSD and HgfsSetattrCopy, we define VATTR_RETURN
 * for FreeBSD.
 */
#if defined __FreeBSD__
#define VATTR_RETURN(vn, attr, val) \
   do { (vn)-> attr = (val); } while (0)
#endif

/* Local Function Prototypes */

/*
 *----------------------------------------------------------------------------
 *
 * HgfsSubmitRequest --
 *
 *    Places a request on the queue for submission by the worker thread,
 *    then waits for the response.
 *
 *    Both submitting request and waiting for reply are in this function
 *    because the signaling of the request list's condition variable and
 *    waiting on the request's condition variable must be atomic.
 *
 * Results:
 *    Returns zero on success, and an appropriate error code on error.
 *    Note: EINTR is returned if cv_wait_sig() is interrupted.
 *
 * Side effects:
 *    The request list's condition variable is signaled.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSubmitRequest(HgfsSuperInfo *sip,   // IN: Superinfo containing request list,
                                        //     condition variable, and mutex
                  HgfsKReqHandle req)   // IN: Request to submit
{
   int ret = 0;

   ASSERT(sip);
   ASSERT(req);

   /*
    * The process of submitting the request involves putting it on the request
    * list, waking up the backdoor req thread if it is waiting for a request,
    * then atomically waiting for the reply.
    */

   /*
    * Fail the request if a forcible unmount is in progress.
    */
   if (HGFS_MP_IS_FORCEUNMOUNT(sip->vfsp)) {
      HgfsKReq_ReleaseRequest(sip->reqs, req);
      return EIO;
   }

   /* Submit the request & wait for a result. */
   ret = HgfsKReq_SubmitRequest(req);

   if (ret == 0) {
      /* The reply should now be in HgfsKReq_GetPayload(req). */
      DEBUG(VM_DEBUG_SIG, "awoken because reply received.\n");
   } else {
      /* HgfsKReq_SubmitRequest was interrupted, so we'll abandon now. */
      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetStatus --
 *    
 *    Gets the status of the reply packet. If the size of the reply packet
 *    does not lie between the minimum expected size and maximum allowed packet
 *    size, then EPROTO is returned.   
 *
 * Results:
 *    Returns zero on success, and an error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsGetStatus(HgfsKReqHandle req,    // IN: Request that contains reply data
              uint32_t minSize)      // IN: Minimum size expected for the reply
{
   HgfsReply *replyHeader;
   size_t repSize = 0;
   int ret = 0;

   ASSERT(req);
   ASSERT(minSize <= HGFS_PACKET_MAX);  /* we want to know if this fails */

   switch (HgfsKReq_GetState(req)) {

   case HGFS_REQ_ERROR:
      DEBUG(VM_DEBUG_FAIL, "received reply with error.\n");
      ret = EPROTO;
      break;

   case HGFS_REQ_COMPLETED:
      repSize = HgfsKReq_GetPayloadSize(req);
      /*
       * Server sets the packet size equal to size of HgfsReply when it
       * encounters an error. In order to return correct error code, 
       * we should first check the status and then check if packet size
       * lies between minimum expected size and maximum allowed packet size.
       */

      if (repSize >= sizeof *replyHeader) {
         replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
         ret = HgfsStatusToBSD(replyHeader->status);
         if (ret) {
            break;
         }
      }

      if (repSize < minSize || repSize > HGFS_PACKET_MAX) {
         DEBUG(VM_DEBUG_FAIL, "successfully "
               "completed reply is too small/big: !(%d < %" FMTSZ "d < %d).\n",
               minSize, repSize, HGFS_PACKET_MAX);
         ret = EPROTO;   
      }
      break;

   /*
    * If we get here then there is a programming error in this module:
    *  HGFS_REQ_UNUSED should be for requests in the free list
    *  HGFS_REQ_SUBMITTED should be for requests only that are awaiting
    *                     a response
    *  HGFS_REQ_ABANDONED should have returned an error to the client
    */
   default:
      NOT_REACHED();
      ret = EPROTO;        /* avoid compiler warning */
   }

   return ret;
}

/*
 * XXX
 * These were taken and slightly modified from hgfs/driver/solaris/vnode.c.
 * (Which, in turn, took them from hgfs/driver/linux/driver.c.) Should we
 * move them into a hgfs/driver/posix/driver.c?
 */


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetOpenMode --
 *
 *    Based on the flags requested by the process making the open()
 *    syscall, determine which open mode (access type) to request from
 *    the server.
 *
 * Results:
 *    Returns the correct HgfsOpenMode enumeration to send to the
 *    server, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsGetOpenMode(uint32 flags) // IN: Open flags
{
   /*
    * Preprocessor wrapper kept for when this function is factored out
    * into a common file.
    */
#if defined _KERNEL || defined KERNEL
   /*
    * FreeBSD / Mac OS use different values from those in the linux kernel. These are defined in
    * <sys/fcntl.h>.
    */
   #undef O_RDONLY
   #undef O_WRONLY
   #undef O_RDWR

   #define O_RDONLY     FREAD
   #define O_WRONLY     FWRITE
   #define O_RDWR       (FREAD | FWRITE)
#endif

   uint32 mask = O_RDONLY | O_WRONLY | O_RDWR;
   int result = -1;

   DEBUG(VM_DEBUG_LOG, "entered\n");

   /*
    * Mask the flags to only look at the access type.
    */
   flags &= mask;

   /* Pick the correct HgfsOpenMode. */
   switch (flags) {

   case O_RDONLY:
      DEBUG(VM_DEBUG_COMM, "O_RDONLY\n");
      result = HGFS_OPEN_MODE_READ_ONLY;
      break;

   case O_WRONLY:
      DEBUG(VM_DEBUG_COMM, "O_WRONLY\n");
      result = HGFS_OPEN_MODE_WRITE_ONLY;
      break;

   case O_RDWR:
      DEBUG(VM_DEBUG_COMM, "O_RDWR\n");
      result = HGFS_OPEN_MODE_READ_WRITE;
      break;

   default:
      /* This should never happen. */
      NOT_REACHED();
      DEBUG(VM_DEBUG_LOG, "invalid open flags %o\n", flags);
      result = -1;
      break;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetOpenFlags --
 *
 *    Based on the flags requested by the process making the open()
 *    syscall, determine which flags to send to the server to open the
 *    file.
 *
 * Results:
 *    Returns the correct HgfsOpenFlags enumeration to send to the
 *    server, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsGetOpenFlags(uint32 flags) // IN: Open flags
{
   uint32 mask = O_CREAT | O_TRUNC | O_EXCL;
   int result = -1;

   DEBUG(VM_DEBUG_INFO, "entered\n");

   /*
    * Mask the flags to only look at O_CREAT, O_EXCL, and O_TRUNC.
    */

   flags &= mask;

   /* O_EXCL has no meaning if O_CREAT is not set. */
   if (!(flags & O_CREAT)) {
      flags &= ~O_EXCL;
   }

   /* Pick the right HgfsOpenFlags. */
   switch (flags) {

   case 0:
      /* Regular open; fails if file nonexistant. */
      DEBUG(VM_DEBUG_COMM, "0\n");
      result = HGFS_OPEN;
      break;

   case O_CREAT:
      /* Create file; if it exists already just open it. */
      DEBUG(VM_DEBUG_COMM, "O_CREAT\n");
      result = HGFS_OPEN_CREATE;
      break;

   case O_TRUNC:
      /* Truncate existing file; fails if nonexistant. */
      DEBUG(VM_DEBUG_COMM, "O_TRUNC\n");
      result = HGFS_OPEN_EMPTY;
      break;

   case (O_CREAT | O_EXCL):
      /* Create file; fail if it exists already. */
      DEBUG(VM_DEBUG_COMM, "O_CREAT | O_EXCL\n");
      result = HGFS_OPEN_CREATE_SAFE;
      break;

   case (O_CREAT | O_TRUNC):
      /* Create file; if it exists already, truncate it. */
      DEBUG(VM_DEBUG_COMM, "O_CREAT | O_TRUNC\n");
      result = HGFS_OPEN_CREATE_EMPTY;
      break;

   default:
      /*
       * This can only happen if all three flags are set, which
       * conceptually makes no sense because O_EXCL and O_TRUNC are
       * mutually exclusive if O_CREAT is set.
       *
       * However, the open(2) man page doesn't say you can't set all
       * three flags, and certain apps (*cough* Nautilus *cough*) do
       * so. To be friendly to those apps, we just silenty drop the
       * O_TRUNC flag on the assumption that it's safer to honor
       * O_EXCL.
       */
      DEBUG(VM_DEBUG_INFO, "invalid open flags %o.  "
            "Ignoring the O_TRUNC flag.\n", flags);
      result = HGFS_OPEN_CREATE_SAFE;
      break;
   }

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMakeFullName --
 *
 *    Concatenates the path and filename to construct the full path.  This
 *    handles the special cases of . and .. filenames so the Hgfs server
 *    doesn't return an error.
 *
 * Results:
 *    Returns the length of the full path on success, and a negative value on
 *    error.  The full pathname is placed in outBuf.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsMakeFullName(const char *path,      // IN:  Path of directory containing file
                 uint32_t pathLen,      // IN:  Length of path
                 const char *file,      // IN:  Name of file
                 size_t fileLen,        // IN:  Length of filename
                 char *outBuf,          // OUT: Location to write full path
                 ssize_t bufSize)       // IN:  Size of the out buffer
{
   uint32 pathSeparatorLen;
   ASSERT(path);
   ASSERT(file);
   ASSERT(outBuf);


   DEBUG(VM_DEBUG_INFO, "HgfsMakeFullName:\n"
         " path: \"%.*s\" (%d)\n"
         " file: \"%s\" (%zu)\n",
         pathLen, path, pathLen, file, fileLen);

   /*
    * Here there are three possibilities:
    *  o file is ".", in which case we just place path in outBuf
    *  o file is "..", in which case we strip the last component from path and
    *    put that in outBuf
    *  o for all other cases, we concatenate path, a path separator, file, and
    *    a NUL terminator and place it in outBuf
    */

   /* Make sure that the path and a NUL terminator will fit. */
   if (bufSize < pathLen + 1) {
      return HGFS_ERR_INVAL;
   }


   /*
    * Copy path for this file into the caller's buffer.
    * The memset call is important here because it implicitly null terminates
    * outBuf so that rindex can be called in the second case below.
    */
   memset(outBuf, 0, bufSize);
   memcpy(outBuf, path, pathLen);

   /* Handle three cases. */
   if (fileLen == 1 && strncmp(file, ".", 1) == 0) {
      /* NUL terminate and return provided length. */
      outBuf[pathLen] = '\0';
      return pathLen;

   } else if (fileLen == 2 && strncmp(file, "..", 2) == 0) {
      /*
       * Replace the last path separator with a NUL terminator, then return the
       * size of the buffer.
       */
      char *newEnd = rindex(outBuf, DIRSEPC);
      if (!newEnd) {
         /*
          * We should never get here since we name the root vnode "/" in
          * HgfsMount().
          */
         return HGFS_ERR_INVAL;
      }

      *newEnd = '\0';
      return ((uintptr_t)newEnd - (uintptr_t)outBuf);
   } else {
      if (bufSize < pathLen + 1 + fileLen + 1) {
         return HGFS_ERR_INVAL;
      }

      /*
       * If the path consists of just a single path separator, then
       * do not add another path separator. This will ensure that
       * we have only single path separator at the beginning of the
       * filename.
       */
      if (pathLen == 1 && *path == DIRSEPC) {
         pathSeparatorLen = 0;
      } else {
         outBuf[pathLen] = DIRSEPC;
         pathSeparatorLen = DIRSEPSLEN;
      }
      /* Now append the filename and NUL terminator. */
      memcpy(outBuf + pathSeparatorLen + pathLen, file, fileLen);
      outBuf[pathLen + pathSeparatorLen + fileLen] = '\0';

      return pathLen + pathSeparatorLen + fileLen;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetattrCopy --
 *
 *    Sets the Hgfs attributes that need to be modified based on the provided
 *    Solaris attribute structure.
 *
 * Results:
 *    Returns TRUE if changes need to be made, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsSetattrCopy(HgfsVnodeAttr *vap,      // IN:  Attributes to change to
                HgfsAttrV2 *hgfsAttrV2,  // OUT: Hgfs attributes to fill in
                HgfsAttrHint *hints)     // OUT: Hgfs attribute hints
{
   Bool ret = FALSE;

   ASSERT(vap);
   ASSERT(hgfsAttrV2);
   ASSERT(hints);

   memset(hgfsAttrV2, 0, sizeof *hgfsAttrV2);
   memset(hints, 0, sizeof *hints);

   /*
    * Hgfs supports changing these attributes:
    * o mode bits (permissions)
    * o uid/gid
    * o size
    * o access/write times
    */

   if (HGFS_VATTR_MODE_IS_ACTIVE(vap, HGFS_VA_MODE)){
      DEBUG(VM_DEBUG_COMM, "updating permissions.\n");
      hgfsAttrV2->mask |= HGFS_ATTR_VALID_SPECIAL_PERMS |
                          HGFS_ATTR_VALID_OWNER_PERMS |
                          HGFS_ATTR_VALID_GROUP_PERMS |
                          HGFS_ATTR_VALID_OTHER_PERMS;
      hgfsAttrV2->specialPerms = (vap->HGFS_VA_MODE & (S_ISUID | S_ISGID |
                                  S_ISVTX)) >> HGFS_ATTR_SPECIAL_PERM_SHIFT;
      hgfsAttrV2->ownerPerms = (vap->HGFS_VA_MODE & S_IRWXU) >>
                                HGFS_ATTR_OWNER_PERM_SHIFT;
      hgfsAttrV2->groupPerms = (vap->HGFS_VA_MODE & S_IRWXG) >>
                                HGFS_ATTR_GROUP_PERM_SHIFT;
      hgfsAttrV2->otherPerms = vap->HGFS_VA_MODE & S_IRWXO;
      ret = TRUE;
   }

   if (HGFS_VATTR_IS_ACTIVE(vap, HGFS_VA_UID)) {
      DEBUG(VM_DEBUG_COMM, "updating user id.\n");
      hgfsAttrV2->mask |= HGFS_ATTR_VALID_USERID;
      hgfsAttrV2->userId = vap->HGFS_VA_UID;
      ret = TRUE;
   }

   if (HGFS_VATTR_IS_ACTIVE(vap, HGFS_VA_GID)) {
      DEBUG(VM_DEBUG_COMM, "updating group id.\n");
      hgfsAttrV2->mask |= HGFS_ATTR_VALID_GROUPID;
      hgfsAttrV2->groupId = vap->HGFS_VA_GID;
      ret = TRUE;
   }

   if (HGFS_VATTR_IS_ACTIVE(vap, HGFS_VA_ACCESS_TIME_SEC)) {
      DEBUG(VM_DEBUG_COMM, "updating access time.\n");
      *hints |= HGFS_ATTR_HINT_SET_ACCESS_TIME;
      hgfsAttrV2->mask |= HGFS_ATTR_VALID_ACCESS_TIME;
      hgfsAttrV2->accessTime = HGFS_GET_TIME(vap->HGFS_VA_ACCESS_TIME);
      ret = TRUE;
   }

   if (HGFS_VATTR_IS_ACTIVE(vap, HGFS_VA_MODIFY_TIME_SEC)) {
      DEBUG(VM_DEBUG_COMM, "updating write time.\n");
      *hints |= HGFS_ATTR_HINT_SET_WRITE_TIME;
      hgfsAttrV2->mask |= HGFS_ATTR_VALID_WRITE_TIME;
      hgfsAttrV2->writeTime = HGFS_GET_TIME(vap->HGFS_VA_MODIFY_TIME);
      ret = TRUE;
   }

   if (HGFS_VATTR_SIZE_IS_ACTIVE(vap, HGFS_VA_DATA_SIZE)) {
      DEBUG(VM_DEBUG_COMM, "updating size.\n");
      hgfsAttrV2->mask |= HGFS_ATTR_VALID_SIZE;
      hgfsAttrV2->size = vap->HGFS_VA_DATA_SIZE;
      ret = TRUE;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAttrToBSD --
 *
 *    Maps Hgfs attributes to Mac OS/BSD attributes, filling the provided BSD
 *    attribute structure appropriately.
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
HgfsAttrToBSD(struct vnode *vp,             // IN:  The vnode for this file
              const HgfsAttrV2 *hgfsAttrV2, // IN:  Hgfs attributes to copy
              HgfsVnodeAttr *vap)           // OUT: BSD attributes to fill
{
   short mode = 0;
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);

   ASSERT(vp);
   ASSERT(hgfsAttrV2);
   ASSERT(vap);

   /* XXX Update this function to support all V2 attributes. */

   DEBUG(VM_DEBUG_ENTRY, "%p -> %p\n", hgfsAttrV2, vap);

   /*
    * Initialize all fields to zero. We don't need to do this for Mac OS
    * because the VATTR_RETURN macros take care of it for us.
    */
#if defined __FreeBSD__
   VATTR_NULL(vap);
#endif

   if ((hgfsAttrV2->mask & HGFS_ATTR_VALID_TYPE)) {
      /* Set the file type. */
      switch (hgfsAttrV2->type) {
      case HGFS_FILE_TYPE_REGULAR:
         HGFS_VATTR_TYPE_RETURN(vap, VREG);
         DEBUG(VM_DEBUG_ATTR, " Type: VREG\n");
         break;

      case HGFS_FILE_TYPE_DIRECTORY:
         HGFS_VATTR_TYPE_RETURN(vap, VDIR);
         DEBUG(VM_DEBUG_ATTR, " Type: VDIR\n");
         break;

      case HGFS_FILE_TYPE_SYMLINK:
         HGFS_VATTR_TYPE_RETURN(vap, VLNK);
         DEBUG(VM_DEBUG_ATTR, " Type: VLNK\n");
         break;

      default:
         /*
          * There are only the above three filetypes.  If there is an error
          * elsewhere that provides another value, we set the Solaris type to
          * none and ASSERT in devel builds.
          */
         HGFS_VATTR_TYPE_RETURN(vap, VNON);
         DEBUG(VM_DEBUG_FAIL, "invalid HgfsFileType provided.\n");
      }
   } else {
      HGFS_VATTR_TYPE_RETURN(vap, VNON);
      DEBUG(VM_DEBUG_FAIL, "invalid HgfsFileType provided\n");
   }

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_SPECIAL_PERMS) {
      mode |= hgfsAttrV2->specialPerms << HGFS_ATTR_SPECIAL_PERM_SHIFT;
   }

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_OWNER_PERMS) {
      mode |= hgfsAttrV2->ownerPerms << HGFS_ATTR_OWNER_PERM_SHIFT;
   }

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_GROUP_PERMS) {
      mode |= hgfsAttrV2->groupPerms << HGFS_ATTR_GROUP_PERM_SHIFT;
   }

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_OWNER_PERMS) {
      mode |= hgfsAttrV2->otherPerms;
   }

   HGFS_VATTR_MODE_RETURN(vap, mode);

   HGFS_VATTR_NLINK_RETURN(vap, 1);               /* fake */

   if (sip->uidSet || (hgfsAttrV2->mask & HGFS_ATTR_VALID_USERID) == 0) {
      HGFS_VATTR_UID_RETURN(vap, sip->uid);
   } else {
      HGFS_VATTR_UID_RETURN(vap, hgfsAttrV2->userId);
   }

   if (sip->gidSet || (hgfsAttrV2->mask & HGFS_ATTR_VALID_GROUPID) == 0) {
      HGFS_VATTR_GID_RETURN(vap, sip->gid);
   } else {
      HGFS_VATTR_GID_RETURN(vap, hgfsAttrV2->groupId);
   }

   HGFS_VATTR_FSID_RETURN(vap, HGFS_VP_TO_STATFS(vp)->f_fsid.val[0]);

   /* Get the node id calculated for this file in HgfsVnodeGet() */
   HGFS_VATTR_FILEID_RETURN(vap, HGFS_VP_TO_NODEID(vp));

   HGFS_VATTR_BLOCKSIZE_RETURN(vap, HGFS_BLOCKSIZE);

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_SIZE) {
      HGFS_VATTR_BYTES_RETURN(vap, hgfsAttrV2->size);
      HGFS_VATTR_SIZE_RETURN(vap, hgfsAttrV2->size);
   }

   /* 
    * HGFS_SET_TIME does not mark the attribute as supported (unlike
    * VATTR_RETURN on Mac OS) so we have to do it explicitly with calls to
    * VATTR_SET_SUPPORTED. For FreeBSD, HGFS_VATTR_*_SET_SUPPORTED is just a NULL
    * macro.
    */

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
      HGFS_SET_TIME(vap->HGFS_VA_ACCESS_TIME, hgfsAttrV2->accessTime);
      HGFS_VATTR_ACCESS_TIME_SET_SUPPORTED(vap);
   }

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_WRITE_TIME) {
      HGFS_SET_TIME(vap->HGFS_VA_MODIFY_TIME, hgfsAttrV2->writeTime);
      HGFS_VATTR_MODIFY_TIME_SET_SUPPORTED(vap);
   }

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_CHANGE_TIME) {
      HGFS_SET_TIME(vap->HGFS_VA_CHANGE_TIME, hgfsAttrV2->attrChangeTime);
      HGFS_VATTR_CHANGE_TIME_SET_SUPPORTED(vap);
   }

   if (hgfsAttrV2->mask & HGFS_ATTR_VALID_CREATE_TIME) {
      /* Since Windows doesn't keep ctime, we may need to use mtime instead. */
       HGFS_SET_TIME(vap->HGFS_VA_CREATE_TIME, hgfsAttrV2->creationTime);
       HGFS_VATTR_CREATE_TIME_SET_SUPPORTED(vap);
   } else if (hgfsAttrV2->mask & HGFS_ATTR_VALID_WRITE_TIME) {
       DEBUG(VM_DEBUG_ATTR, "Set create time from write time\n");
       vap->HGFS_VA_CREATE_TIME = vap->HGFS_VA_MODIFY_TIME;
       HGFS_VATTR_CREATE_TIME_SET_SUPPORTED(vap);
   } else {
       DEBUG(VM_DEBUG_ATTR, "Do not set create time\n");
   }

   DEBUG(VM_DEBUG_ATTR, "Attrib mask %"FMT64"d\n", hgfsAttrV2->mask);

#if defined __APPLE__
   DEBUG(VM_DEBUG_ATTR, "Supported %lld, active %lld\n", vap->va_supported,
         vap->va_active);
#endif

   HgfsDebugPrintVattr(vap);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsStatusToBSD --
 *
 *    Convert a cross-platform HGFS status code to its Linux-kernel specific
 *    counterpart.
 *
 * Results:
 *    Zero if the converted status code represents success, negative error
 *    otherwise. Unknown status codes are converted to the more generic
 *    "protocol error" status code to maintain forwards compatibility.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsStatusToBSD(HgfsStatus hgfsStatus) // IN: Hgfs status msg to be converted
{
   switch (hgfsStatus) {
   case HGFS_STATUS_SUCCESS:
      return 0;

   case HGFS_STATUS_NO_SUCH_FILE_OR_DIR:
   case HGFS_STATUS_INVALID_NAME:
      return ENOENT;

   case HGFS_STATUS_INVALID_HANDLE:
      return EBADF;

   case HGFS_STATUS_OPERATION_NOT_PERMITTED:
      return EPERM;

   case HGFS_STATUS_FILE_EXISTS:
      return EEXIST;

   case HGFS_STATUS_NOT_DIRECTORY:
      return ENOTDIR;

   case HGFS_STATUS_DIR_NOT_EMPTY:
      return ENOTEMPTY;

   case HGFS_STATUS_PROTOCOL_ERROR:
      return EPROTO;

   case HGFS_STATUS_ACCESS_DENIED:
   case HGFS_STATUS_SHARING_VIOLATION:
      return EACCES;

   case HGFS_STATUS_NO_SPACE:
      return ENOSPC;

   case HGFS_STATUS_OPERATION_NOT_SUPPORTED:
      return EOPNOTSUPP;

   case HGFS_STATUS_NAME_TOO_LONG:
      return ENAMETOOLONG;

   case HGFS_STATUS_GENERIC_ERROR:
      return EIO;

   default:
      DEBUG(VM_DEBUG_LOG, "VMware hgfs: %s: unknown "
	    "error: %u\n", __FUNCTION__, hgfsStatus);
      return EIO;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * rindex --
 *
 *      Search a character string for the last instance of chr. This is only
 *      implemented for Mac OS because it is not exported by the Mac OS kernel.
 *
 * Results:
 *      Pointer to the last instance of chr in the string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

#if defined __APPLE__
char *
rindex(const char *ptr, // IN: String to search.
       int chr)         // IN: Char to look for.
{
   char *result = NULL;

   ASSERT(ptr);

   for (; *ptr != '\0'; ptr++) {
      if (*ptr == chr) {
	 result = (char *)ptr;
      }
   }

   return result;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAttemptToCreateShare --
 *
 *      Checks if an attempt to create a new share is made.
 *
 * Results:
 *      Returns FALSE if not such attempt is made, TRUE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsAttemptToCreateShare(const char *path,  // IN: Path
                         int flag)          // IN: flag
{
   int ret = FALSE;
   ASSERT(path);

   /* 
    * If the first character is the path seperator and 
    * and there are no more path seperators present in the
    * path, then with the create flag (O_CREAT) set, we believe
    * that user has attempted to create new a share. This operation
    * is not permitted and hence EPERM error code is returned.
    */
   if ((flag & O_CREAT) && path[0] == DIRSEPC &&
        strchr(path + DIRSEPSLEN, DIRSEPC) == NULL) {
       ret = TRUE;
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsNameToWireEncoding --
 *      1) Input string is converted into precomposed form.
 *      2) Precomposed string is then converted to cross platform string.
 *      3) Cross platform string is finally unescaped.
 *
 * Results:
 *      Returns the size (excluding the NULL terminator)  on success and
 *      negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsNameToWireEncoding(const char *bufIn,  // IN: Buffer to be normalized
                       uint32 bufInSize,   // IN: Size of input buffer
                       char *bufOut,       // OUT: Normalized string will be stored here
                       uint32 bufOutSize)  // IN: Size of output buffer
{
   char *precomposedBuf = NULL; // allocated from M_TEMP; free when done.
   const char *utf8Buf;
   int ret = 0;

   if (os_utf8_conversion_needed()) {
      /* Allocating precomposed buffer to be equal to Output buffer. */
      precomposedBuf = os_malloc(bufOutSize, M_WAITOK);
      if (!precomposedBuf) {
         return -ENOMEM;
      }

      ret = os_path_to_utf8_precomposed(bufIn, bufInSize, precomposedBuf, bufOutSize);
      if (ret < 0) {
         DEBUG(VM_DEBUG_FAIL, "os_path_to_utf8_precomposed failed.");
         ret = -EINVAL;
         goto out;
      }
      utf8Buf = precomposedBuf;
   } else {
      utf8Buf = bufIn;
   }

   ret = CPName_ConvertTo(utf8Buf, bufOutSize, bufOut);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL,
            "CPName_ConvertTo: Conversion to cross platform name failed.\n");
      ret = -ENAMETOOLONG;
      goto out;
   }

out:
   if (precomposedBuf != NULL) {
      os_free(precomposedBuf, bufOutSize);
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsNameFromWireEncoding --
 *      1) Converts input from CPName form if necessary.
 *      2) Result is converted into decomposed form.
 *
 * Results:
 *      Returns the size (excluding the NULL terminator)  on success and
 *      negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsNameFromWireEncoding(const char *bufIn,  // IN: Buffer to be encoded
                         uint32 inputLength, // IN: Number of characters in the input
                         char *bufOut,       // OUT: Encoded buffer will be stored here
                         uint32 bufOutSize)  // IN: Size of output buffer
{
   size_t escapedLen;
   int ret = 0;

   /* Output buffer needs one additional byte for NUL terminator. */
   if (inputLength >= bufOutSize) {
      return -ENOMEM;
   }
   escapedLen = HgfsEscape_GetSize(bufIn, inputLength);
   if (escapedLen != 0) {
      HgfsEscape_Do(bufIn, inputLength, bufOutSize, bufOut);
   } else {
      escapedLen = inputLength;
      memcpy(bufOut, bufIn, inputLength);
   }
   CPNameLite_ConvertFrom(bufOut, escapedLen, '/');

   if (os_utf8_conversion_needed()) {
      size_t decomposedLen;
      char *decomposedBuf = NULL;
      /*
       * The decomposed form a string can be a lot bigger than the input
       * buffer size. We allocate a buffer equal to the output buffer.
       */
      decomposedBuf = os_malloc(bufOutSize, M_WAITOK);
      if (!decomposedBuf) {
         DEBUG(VM_DEBUG_FAIL, "Not enough memory for decomposed buffer size %d.\n",
			   bufOutSize);
         return -ENOMEM;
      }
      /*
       * Convert the input buffer into decomposed form. Higher layers in
       * Mac OS expects the name to be in decomposed form.
       */
      ret = os_component_to_utf8_decomposed(bufOut, escapedLen, decomposedBuf,
                                            &decomposedLen, bufOutSize);
      /*
       * If the decomposed name didn't fit in the buffer or it contained
       * illegal utf8 characters, return back to the caller.
       * os_component_to_utf8_decomposed returns 0 on success or OS_ERR on failure.
       */
      if (ret != 0){
         DEBUG(VM_DEBUG_FAIL, "os_component_to_utf8_decomposed failed.\n");
         ret = -EINVAL;
      } else {
         if (decomposedLen < bufOutSize) {
            ret = decomposedLen;
            memcpy(bufOut, decomposedBuf, decomposedLen + 1);
          } else {
            DEBUG(VM_DEBUG_FAIL, "Output buffer is too small.\n");
            ret = -ENOMEM;
         }
      }
      os_free(decomposedBuf, bufOutSize);
   } else {
      ret = escapedLen;
   }
   return ret;
}
