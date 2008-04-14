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

#if defined(__APPLE__)
#  include <libkern/libkern.h>  // for rindex
#endif

#include "fsutil.h"
#include "cpName.h"
#include "staticEscape.h"
#include "os.h"

#if defined(__APPLE__)
char *rindex(const char *ptr, int chr);
#endif

/*
 * OS X sets vnode attributes through the use of a VATTR_RETURN function.
 * FreeBSD sets vnode attributes directly in the structure. To enable a shared
 * implementation of HgfsAttrToBSD and HgfsSetattrCopy, we define VATTR_RETURN
 * for FreeBSD.
 */
#if defined(__FreeBSD__)
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
 * HgfsValidateReply --
 *
 *    Validates a reply to ensure that its state is set appropriately and the
 *    reply is at least the minimum expected size and not greater than the
 *    maximum allowed packet size.
 *
 * Results:
 *    Returns zero on success, and a non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsValidateReply(HgfsKReqHandle req,    // IN: Request that contains reply data
                  uint32_t minSize)      // IN: Minimum size expected for the reply
{
   ASSERT(req);
   ASSERT(minSize <= HGFS_PACKET_MAX);  /* we want to know if this fails */

   switch (HgfsKReq_GetState(req)) {
   case HGFS_REQ_ERROR:
      DEBUG(VM_DEBUG_FAIL, "received reply with error.\n");
      return -1;

   case HGFS_REQ_COMPLETED:
      if ((HgfsKReq_GetPayloadSize(req) < minSize) || (HgfsKReq_GetPayloadSize(req) > HGFS_PACKET_MAX)) {
         DEBUG(VM_DEBUG_FAIL, "successfully "
               "completed reply is too small/big: !(%d < %" FMTSZ "d < %d).\n",
               minSize, HgfsKReq_GetPayloadSize(req), HGFS_PACKET_MAX);
         return -1;
      } else {
         return 0;
      }
   /*
    * If we get here then there is a programming error in this module:
    *  HGFS_REQ_UNUSED should be for requests in the free list
    *  HGFS_REQ_SUBMITTED should be for requests only that are awaiting
    *                     a response
    *  HGFS_REQ_ABANDONED should have returned an error to the client
    */
   default:
      NOT_REACHED();
      return -1;        /* avoid compiler warning */
   }
}


/*
 * XXX: These were taken directly from hgfs/solaris/vnode.c.  Should we
 * move them to hgfsUtil.c or similar?  (And Solaris took them from the Linux
 * implementation.)
 */


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscapeBuffer --
 *
 *    Escape any characters that are not legal in a linux filename,
 *    which is just the character "/". We also of course have to
 *    escape the escape character, which is "%".
 *
 *    sizeBufOut must account for the NUL terminator.
 *
 *    XXX: See the comments in staticEscape.c and staticEscapeW.c to understand
 *    why this interface sucks.
 *
 * Results:
 *    On success, the size (excluding the NUL terminator) of the
 *    escaped, NUL terminated buffer.
 *    On failure (bufOut not big enough to hold result), negative value.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsEscapeBuffer(char const *bufIn, // IN:  Buffer with unescaped input
                 uint32 sizeIn,     // IN:  Size of input buffer (chars)
                 uint32 sizeBufOut, // IN:  Size of output buffer (bytes)
                 char *bufOut)      // OUT: Buffer for escaped output
{
   /*
    * This is just a wrapper around the more general escape
    * routine; we pass it the correct bitvector and the
    * buffer to escape. [bac]
    */
   EscBitVector bytesToEsc;

   ASSERT(bufIn);
   ASSERT(bufOut);

   /* Set up the bitvector for "/" and "%" */
   EscBitVector_Init(&bytesToEsc);
   EscBitVector_Set(&bytesToEsc, (unsigned char)'%');
   EscBitVector_Set(&bytesToEsc, (unsigned char)'/');

   return StaticEscape_Do('%',
                          &bytesToEsc,
                          bufIn,
                          sizeIn,
                          sizeBufOut,
                          bufOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnescapeBuffer --
 *
 *    Unescape a buffer that was escaped using HgfsEscapeBuffer.
 *
 *    The unescaping is done in place in the input buffer, and
 *    can not fail.
 *
 * Results:
 *    The size (excluding the NUL terminator) of the unescaped, NUL
 *    terminated buffer.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsUnescapeBuffer(char *bufIn,   // IN: Buffer to be unescaped
                   uint32 sizeIn) // IN: Size of input buffer
{
   /*
    * This is just a wrapper around the more general unescape
    * routine; we pass it the correct escape characer and the
    * buffer to unescape. [bac]
    */
   ASSERT(bufIn);
   return StaticEscape_Undo('%', bufIn, sizeIn);
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
#if defined(_KERNEL) || defined(KERNEL)
   /*
    * FreeBSD / OS X use different values from those in the linux kernel. These are defined in
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
      char *newEnd = rindex(outBuf, '/');
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
       * The CPName_ConvertTo function handles multiple path separators
       * at the beginning of the filename, so we skip the checks to limit
       * them to one.  This also enables clobbering newEnd above to work
       * properly on base shares (named "//sharename") that need to turn into
       * "/".
       */
      outBuf[pathLen] = '/';

      /* Now append the filename and NUL terminator. */
      memcpy(outBuf + pathLen + 1, file, fileLen);
      outBuf[pathLen + 1 + fileLen] = '\0';

      return pathLen + 1 + fileLen;
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
HgfsSetattrCopy(HGFS_VNODE_ATTR *vap,    // IN:  Attributes to change to
                HgfsAttr *hgfsAttr,      // OUT: Hgfs attributes to fill in
                HgfsAttrChanges *update) // OUT: Hgfs attribute changes to make
{
   Bool ret = FALSE;

   ASSERT(vap);
   ASSERT(hgfsAttr);
   ASSERT(update);

   memset(hgfsAttr, 0, sizeof *hgfsAttr);
   memset(update, 0, sizeof *update);

   /*
    * Hgfs supports changing these attributes:
    * o mode bits (permissions)
    * o size
    * o access/write times
    */

#if defined(__APPLE__)
   if (VATTR_IS_ACTIVE(vap, va_mode)) {
      DEBUG(VM_DEBUG_COMM, "updating permissions.\n");
      *update |= HGFS_ATTR_PERMISSIONS;
      hgfsAttr->permissions = (vap->va_mode & S_IRWXU) >> HGFS_ATTR_MODE_SHIFT;
      ret = TRUE;
   }

   if (VATTR_IS_ACTIVE(vap, va_data_size)) {
      DEBUG(VM_DEBUG_COMM, "updating size.\n");
      *update |= HGFS_ATTR_SIZE;
      hgfsAttr->size = vap->va_data_size;
      ret = TRUE;
   }

   if (VATTR_IS_ACTIVE(vap, va_access_time)) {
      DEBUG(VM_DEBUG_COMM, "updating access time.\n");
      *update |= HGFS_ATTR_ACCESS_TIME;
      hgfsAttr->accessTime = HGFS_GET_TIME(vap->va_access_time);
      ret = TRUE;
   }

   if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
      DEBUG(VM_DEBUG_COMM, "updating write time.\n");
      *update |= HGFS_ATTR_WRITE_TIME;
      hgfsAttr->writeTime = HGFS_GET_TIME(vap->va_modify_time);
      ret = TRUE;
   }
#elif defined(__FreeBSD__)
   if (vap->va_mode != (mode_t)VNOVAL) {
      DEBUG(VM_DEBUG_COMM, "updating permissions.\n");
      *update |= HGFS_ATTR_PERMISSIONS;
      hgfsAttr->permissions = (vap->va_mode & S_IRWXU) >> HGFS_ATTR_MODE_SHIFT;
      ret = TRUE;
   }

   if (vap->va_size != (u_quad_t)VNOVAL) {
      DEBUG(VM_DEBUG_COMM, "updating size.\n");
      *update |= HGFS_ATTR_SIZE;
      hgfsAttr->size = vap->va_size;
      ret = TRUE;
   }

   if (vap->va_atime.tv_sec != VNOVAL) {
      DEBUG(VM_DEBUG_COMM, "updating access time.\n");
      *update |= HGFS_ATTR_ACCESS_TIME;
      hgfsAttr->accessTime = HGFS_GET_TIME(vap->va_atime);
      ret = TRUE;
   }

   if (vap->va_mtime.tv_sec != VNOVAL) {
      DEBUG(VM_DEBUG_COMM, "updating write time.\n");
      *update |= HGFS_ATTR_WRITE_TIME;
      hgfsAttr->writeTime = HGFS_GET_TIME(vap->va_mtime);
      ret = TRUE;
   }
#endif

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAttrToBSD --
 *
 *    Maps Hgfs attributes to Solaris attributes, filling the provided Solaris
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
              HGFS_VNODE_ATTR *vap)         // OUT: BSD attributes to fill
{
   ASSERT(vp);
   ASSERT(hgfsAttrV2);
   ASSERT(vap);

   /* XXX Update this function to support all V2 attributes. */

   DEBUG(VM_DEBUG_ENTRY, "%p -> %p\n", hgfsAttrV2, vap);

   /*
    * Initialize all fields to zero. We don't need to do this for OS X
    * because the VATTR_RETURN macros take care of it for us.
    */
#if defined(__FreeBSD__)
   VATTR_NULL(vap);
#endif

   /* Set the file type. */
   switch (hgfsAttrV2->type) {
   case HGFS_FILE_TYPE_REGULAR:
      VATTR_RETURN(vap, va_type, VREG);
      DEBUG(VM_DEBUG_ATTR, " Type: VREG\n");
      break;

   case HGFS_FILE_TYPE_DIRECTORY:
      VATTR_RETURN(vap, va_type, VDIR);
      DEBUG(VM_DEBUG_ATTR, " Type: VDIR\n");
      break;

   default:
      /*
       * There are only the above two filetypes.  If there is an error
       * elsewhere that provides another value, we set the Solaris type to
       * none and ASSERT in devel builds.
       */
      VATTR_RETURN(vap, va_type, VNON);
      DEBUG(VM_DEBUG_FAIL, "invalid HgfsFileType provided.\n");
      ASSERT(0);
   }

   /* We only have permissions for owners. */
   VATTR_RETURN(vap, va_mode, (hgfsAttrV2->ownerPerms << HGFS_ATTR_MODE_SHIFT));

   VATTR_RETURN(vap, va_nlink, 1);               /* fake */
   VATTR_RETURN(vap, va_uid, 0);                 /* XXX root? */
   VATTR_RETURN(vap, va_gid, 0);                 /* XXX root? */

   VATTR_RETURN(vap, va_fsid, HGFS_VP_TO_STATFS(vp)->f_fsid.val[0]);

   /* Get the node id calculated for this file in HgfsVnodeGet() */
   VATTR_RETURN(vap, va_fileid, HGFS_VP_TO_NODEID(vp));
   DEBUG(VM_DEBUG_ATTR, "*HgfsAttrToBSD: fileName %s\n",
         HGFS_VP_TO_FILENAME(vp));

    /*
     * Some of the attribute names or meanings have changed slightly between
     * OS X and FreeBSD. We handle these special case items here.
     */

    DEBUG(VM_DEBUG_ATTR, " Setting size to %"FMT64"u\n", hgfsAttrV2->size);

#if defined(__APPLE__)
    DEBUG(VM_DEBUG_ATTR, " Node ID: %"FMT64"u\n", vap->va_fileid);

    /* va_iosize is the Optimal I/O blocksize. */
    VATTR_RETURN(vap, va_iosize, HGFS_BLOCKSIZE);
    VATTR_RETURN(vap, va_data_size, hgfsAttrV2->size);

    HGFS_SET_TIME(vap->va_access_time, hgfsAttrV2->accessTime);
    HGFS_SET_TIME(vap->va_modify_time, hgfsAttrV2->writeTime);

   /* Since Windows doesn't keep ctime, we may need to use mtime instead. */
   if (HGFS_SET_TIME(vap->va_create_time, hgfsAttrV2->attrChangeTime)) {
      vap->va_create_time = vap->va_modify_time;
   }

   /* HGFS_SET_TIME does not mark the attribute as supported (unlike
    * VATTR_RETURN on OS X) so we have to do it explicitly with calls to
    * VATTR_SET_SUPPORTED.
    */
   VATTR_SET_SUPPORTED(vap, va_access_time);
   VATTR_SET_SUPPORTED(vap, va_modify_time);
   VATTR_SET_SUPPORTED(vap, va_create_time);

#elif defined(__FreeBSD__)
    DEBUG(VM_DEBUG_ATTR, " Node ID: %ld\n", vap->va_fileid);

    VATTR_RETURN(vap, va_bytes, hgfsAttrV2->size);
    VATTR_RETURN(vap, va_size, hgfsAttrV2->size);
    VATTR_RETURN(vap, va_blocksize, HGFS_BLOCKSIZE);

    HGFS_SET_TIME(vap->va_atime, hgfsAttrV2->accessTime);
    HGFS_SET_TIME(vap->va_mtime, hgfsAttrV2->writeTime);
   /* Since Windows doesn't keep ctime, we may need to use mtime instead. */
   if (HGFS_SET_TIME(vap->va_ctime, hgfsAttrV2->attrChangeTime)) {
      vap->va_ctime = vap->va_mtime;
   }

   /*
    * This is only set for FreeBSD as there does not seem to be an analogous
    * attribute for OS X.
    */
   DEBUG(VM_DEBUG_ATTR, " Setting birthtime\n");
   HGFS_SET_TIME(vap->va_birthtime, hgfsAttrV2->creationTime);

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
 *      implemented for OS X because it is not exported by the OS X kernel.
 *
 * Results:
 *      Pointer to the last instance of chr in the string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

#if defined(__APPLE__)
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
