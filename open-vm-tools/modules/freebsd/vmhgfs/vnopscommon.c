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
 * vnopscommon.c --
 *
 * Common VFS vnop implementations that are shared between both Mac OS and FreeBSD.
 */

#include <sys/param.h>          // for everything
#include <sys/vnode.h>          // for struct vnode
#include <sys/dirent.h>         // for struct dirent

#include "fsutil.h"
#include "debug.h"
#include "vnopscommon.h"
#include "transport.h"
#include "cpName.h"
#include "os.h"

/* Local function prototypes */
int HgfsGetNextDirEntry(HgfsSuperInfo *sip, HgfsHandle handle,
                               uint32_t offset, char *nameOut, size_t nameSize,
                               HgfsFileType *type, Bool *done);
int HgfsDirOpen(HgfsSuperInfo *sip, struct vnode *vp, HgfsOpenType openType);
int HgfsFileOpen(HgfsSuperInfo *sip, struct vnode *vp,
                 int flag, int permissions, HgfsOpenType openType);
int HgfsDirClose(HgfsSuperInfo *sip, struct vnode *vp);
int HgfsFileClose(HgfsSuperInfo *sip, struct vnode *vp, int flag);
int HgfsDoRead(HgfsSuperInfo *sip, HgfsHandle handle, uint64_t offset,
                      uint32_t size, struct uio *uiop);
int HgfsDoWrite(HgfsSuperInfo *sip, HgfsHandle handle, int ioflag,
                       uint64_t offset, uint32_t size, struct uio *uiop);
int HgfsDelete(HgfsSuperInfo *sip, const char *filename, HgfsOp op);
static int HgfsDoGetattrInt(const char *path, const HgfsHandle handle, HgfsSuperInfo *sip,
			    HgfsAttrV2 *hgfsAttrV2);
static int HgfsDoGetattrByName(const char *path, HgfsSuperInfo *sip, HgfsAttrV2 *hgfsAttrV2);
int HgfsReadlinkInt(struct vnode *vp, struct uio *uiop);
static int HgfsQueryAttrInt(const char *path, HgfsHandle handle, HgfsSuperInfo *sip,
                            HgfsKReqHandle req);
static int HgfsRenewHandle(struct vnode *vp, HgfsSuperInfo *sip, HgfsHandle *handle);
static int HgfsRefreshDirHandle(struct vnode *vp, HgfsSuperInfo *sip, HgfsHandle *handle);
static int HgfsGetNewHandle(struct vnode *vp, HgfsSuperInfo *sip, HgfsFile *fp,
                            HgfsHandle *handle);


#if 0
static int HgfsDoGetattrByHandle(HgfsHandle handle, HgfsSuperInfo *sip, HgfsAttrV2 *hgfsAttrV2);
#endif

#define HGFS_CREATE_DIR_MASK (HGFS_CREATE_DIR_VALID_FILE_NAME | \
                              HGFS_CREATE_DIR_VALID_SPECIAL_PERMS | \
                              HGFS_CREATE_DIR_VALID_OWNER_PERMS | \
                              HGFS_CREATE_DIR_VALID_GROUP_PERMS | \
                              HGFS_CREATE_DIR_VALID_OTHER_PERMS)

/*
 *----------------------------------------------------------------------------
 *
 * HgfsRenameInt --
 *
 *      Renames the provided source name in the source directory with the
 *      destination name in the destination directory.  A RENAME request is sent
 *      to the Hgfs server.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsRenameInt(struct vnode *fvp,          // IN: "from" file
	      struct vnode *tdvp,         // IN: "to" parent directory
	      struct vnode *tvp,          // IN: "to" file
	      struct componentname *tcnp) // IN: "to" pathname info
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(fvp);
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsRequestRenameV3 *request;
   HgfsReplyRenameV3 *reply;
   HgfsFileNameV3 *newNameP;
   char *srcFullPath = NULL;    // will point to fvp's filename; don't free
   char *dstFullPath = NULL;    // allocated from M_TEMP; free when done.
   uint32 srcFullPathLen;
   uint32 dstFullPathLen;
   uint32 reqBufferSize;
   uint32 reqSize;
   uint32 repSize;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s -> %.*s/%.*s).\n",
         HGFS_VP_TO_FILENAME_LENGTH(fvp), HGFS_VP_TO_FILENAME(fvp),
         HGFS_VP_TO_FILENAME_LENGTH(tdvp), HGFS_VP_TO_FILENAME(tdvp),
         (int)tcnp->cn_namelen, tcnp->cn_nameptr);

   /* No cross-device renaming. */
   if (HGFS_VP_TO_MP(fvp) != HGFS_VP_TO_MP(tdvp)) {
      ret = EXDEV;
      goto out;
   }

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      goto out;
   }

   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestRenameV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   /* Initialize the request header */
   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_RENAME_V3);
   request->hints = 0;
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_PACKET_MAX - (reqSize - 2);

   /* Make the full path of the source. */
   srcFullPath = HGFS_VP_TO_FILENAME(fvp);
   srcFullPathLen = HGFS_VP_TO_FILENAME_LENGTH(fvp);

   /* Make the full path of the destination. */
   dstFullPath = os_malloc(MAXPATHLEN, M_WAITOK);
   if (!dstFullPath) {
      ret = ENOMEM;
      goto destroyOut;
   }

   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(tdvp),
                          HGFS_VP_TO_FILENAME_LENGTH(tdvp),
                          tcnp->cn_nameptr,
                          tcnp->cn_namelen,
                          dstFullPath,
                          MAXPATHLEN);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "could not construct full path of dest.\n");
      ret = ENAMETOOLONG;
      goto destroyOut;
   }
   dstFullPathLen = ret;

   /* Ensure both names will fit in one request. */
   if ((reqSize + srcFullPathLen + dstFullPathLen) > HGFS_PACKET_MAX) {
      DEBUG(VM_DEBUG_FAIL, "names too big for one request.\n");
      ret = EPROTO;
      goto destroyOut;
   }

   request->oldName.flags = 0;
   request->oldName.fid = HGFS_INVALID_HANDLE;
   request->oldName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;

   /*
    * Convert an input string to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   ret = HgfsNameToWireEncoding(srcFullPath, srcFullPathLen + 1,
                                request->oldName.name, reqBufferSize);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "Couldn't encode to wire format\n");
      ret = -ret;
      goto destroyOut;
   }
   request->oldName.length = ret;
   reqSize += ret;
   reqBufferSize -= ret;

   /*
    * The new name is placed directly after the old name in the packet and we
    * access it through this pointer.
    */
   newNameP = (HgfsFileNameV3 *)((char *)&request->oldName +
                                  sizeof request->oldName +
                                  request->oldName.length);
   newNameP->flags = 0;
   newNameP->fid = HGFS_INVALID_HANDLE;
   newNameP->caseType = HGFS_FILE_NAME_CASE_SENSITIVE;

   ret = HgfsNameToWireEncoding(dstFullPath, dstFullPathLen + 1,
                                newNameP->name, reqBufferSize);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "Couldn't encode to wire format.\n");
      ret = -ret;
      goto destroyOut;
   }
   newNameP->length = ret;
   reqSize += ret;

   /* The request's size includes the header, request and both filenames. */
   HgfsKReq_SetPayloadSize(req, reqSize);

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* HgfsSubmitRequest() destroys the request if necessary. */
      goto out;
   }

   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);

   ret = HgfsGetStatus(req, repSize);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      goto destroyOut;
   }

   /* Successfully renamed file on the server. */

destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);

out:
   if (dstFullPath != NULL) {
      os_free(dstFullPath, MAXPATHLEN);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%d).\n", ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReaddirInt --
 *
 *      Reads as many entries from the directory as will fit in to the provided
 *      buffer.  Each directory entry is read by calling HgfsGetNextDirEntry().
 *
 *      "The vop_readdir() method reads chunks of the directory into a uio
 *      structure.  Each chunk can contain as many entries as will fit within
 *      the size supplied by the uio structure.  The uio_resid structure member
 *      shows the size of the getdents request in bytes, which is divided by the
 *      size of the directory entry made by the vop_readdir() method to
 *      calculate how many directory entries to return." (Solaris Internals,
 *      p555)
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsReaddirInt(struct vnode *vp, // IN    : Directory vnode to get entries from.
	       struct uio *uiop, // IN/OUT: Buffer to place dirents in.
	       int *eofp)        // IN/OUT: Have all entries been read?
{

   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   HgfsHandle handle;
   uint64_t offset;
   Bool done;
   char *fullName = NULL;       /* Hashed to generate inode number */
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n",  HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /* uio_offset is a signed quantity. */
   if (HGFS_UIOP_TO_OFFSET(uiop) < 0) {
      DEBUG(VM_DEBUG_FAIL, "fed negative offset.\n");
      ret = EINVAL;
      goto out;
   }

   /*
    * In order to fill the user's buffer with directory entries, we must
    * iterate on HGFS_OP_SEARCH_READ requests until either the user's buffer is
    * full or there are no more entries.  Each call to HgfsGetNextDirEntry()
    * fills in the name and attribute structure for the next entry.  We then
    * escape that name and place it in a kernel buffer that's the same size as
    * the user's buffer.  Once there are no more entries or no more room in the
    * buffer, we copy it to user space.
    */

   /*
    * We need to get the handle for this open directory to send to the Hgfs
    * server in our requests.
    */
   ret = HgfsGetOpenFileHandle(vp, &handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "could not get handle.\n");
      ret = EINVAL;
      goto out;
   }

   if (HGFS_UIOP_TO_OFFSET(uiop) == 0) {
      ret = HgfsRefreshDirHandle(vp, sip, &handle);
   }

   /*
    * Allocate 1K (MAXPATHLEN) buffer for inode number generation.
    */
   fullName = os_malloc(MAXPATHLEN, M_WAITOK);
   if (!fullName) {
      ret = ENOMEM;
      goto out;
   }

   /*
    * Loop until one of the following conditions is met:
    *  o An error occurs while reading a directory entry
    *  o There are no more directory entries to read
    *  o The buffer is full and cannot hold the next entry
    *
    * We request dentries from the Hgfs server based on their index in the
    * directory.  The offset value is initialized to the value specified in
    * the user's io request and is incremented each time through the loop.
    *
    * dirp is incremented by the record length each time through the loop and
    * is used to determine where in the kernel buffer we write to.
    */
   for (offset = HGFS_UIOP_TO_OFFSET(uiop), done = 0; /* Nothing */ ; offset++) {
      struct dirent dirent, *dirp = &dirent;
      char nameBuf[sizeof dirp->d_name];
      HgfsFileType fileType = HGFS_FILE_TYPE_REGULAR;

      DEBUG(VM_DEBUG_COMM,
            "HgfsReaddir: getting directory entry at offset %"FMT64"u.\n", offset);

      DEBUG(VM_DEBUG_HANDLE, "** handle=%d, file=%s\n",
            handle, HGFS_VP_TO_FILENAME(vp));

      bzero(dirp, sizeof *dirp);

      ret = HgfsGetNextDirEntry(sip, handle, offset, nameBuf, sizeof nameBuf,
                                &fileType, &done);
      /* If the filename was too long, we skip to the next entry ... */
      if (ret == EOVERFLOW) {
         continue;
      } else if (ret == EBADF) {
         /*
          * If we got invalid handle from the server, this was because user
          * enabled/disabled the shared folders. We should get a new handle
          * from the server, now.
          */
         ret = HgfsRenewHandle(vp, sip, &handle);
         if (ret == 0) {
            /*
             * Now we have valid handle, let's try again from the same
             * offset.
             */
            offset--;
            continue;
         } else {
            ret = EBADF;
            goto out;
         }
      } else if (ret) {
         if (ret != EPROTO) {
            ret = EINVAL;
         }
         DEBUG(VM_DEBUG_FAIL, "failure occurred in HgfsGetNextDirEntry\n");
         goto out;
      /*
       * ... and if there are no more entries, we set the end of file pointer
       * and break out of the loop.
       */
      } else if (done == TRUE) {
         DEBUG(VM_DEBUG_COMM, "Done reading directory entries.\n");
         if (eofp != NULL) {
            *eofp = TRUE;
         }
         break;
      }
      /*
       * Convert an input string to utf8 decomposed form and then escape its
       * buffer.
       */
      ret = HgfsNameFromWireEncoding(nameBuf, strlen(nameBuf), dirp->d_name,
                                     sizeof dirp->d_name);
      /*
       * If the name didn't fit in the buffer or illegal utf8 characters
       * were encountered, skip to the next entry.
       */
      if (ret < 0) {
         DEBUG(VM_DEBUG_FAIL, "HgfsNameFromWireEncoding failed.\n");
         continue;
      }

      /* Fill in the directory entry. */
      dirp->d_namlen = ret;
      dirp->d_reclen = sizeof(*dirp);    // NB: d_namlen must be set first!
      dirp->d_type =
         (fileType == HGFS_FILE_TYPE_REGULAR) ? DT_REG :
         (fileType == HGFS_FILE_TYPE_DIRECTORY) ? DT_DIR :
         DT_UNKNOWN;

      /*
       * Make sure there is enough room in the buffer for the entire directory
       * entry. If not, we just break out of the loop and copy what we have an set
       * the return value to be 0.
       */
      if (dirp->d_reclen > HGFS_UIOP_TO_RESID(uiop)) {
         DEBUG(VM_DEBUG_INFO, "ran out of room in the buffer.\n");
	 ret = 0;
         break;
      }


      ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(vp),           // Directorie's name
                             HGFS_VP_TO_FILENAME_LENGTH(vp),    // Length
                             dirp->d_name,                      // Name of file
                             dirp->d_namlen,                    // Length of filename
                             fullName,                          // Destination buffer
                             MAXPATHLEN);                       // Size of this buffer

      /* Skip this entry if the full path was too long. */
      if (ret < 0) {
         continue;
      }

      /*
       * Place the node id, which serves the purpose of inode number, for this
       * filename directory entry.  As long as we are using a dirent64, this is
       * okay since ino_t is also a u_longlong_t.
       */
      HgfsNodeIdGet(&sip->fileHashTable, fullName, (uint32_t)ret,
                    &dirp->d_fileno);

      /* Copy out this directory entry. */
      ret = uiomove((caddr_t)dirp, dirp->d_reclen, uiop);
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "uiomove failed.\n");
         goto out;
      }
   }

   /*
    * uiomove(9) will have incremented the uio offset by the number of bytes
    * written.  We reset it here to the fs-specific offset in our directory so
    * the next time we are called it is correct.  (Note, this does not break
    * anything and /is/ how this field is intended to be used.)
    */
   HGFS_UIOP_SET_OFFSET(uiop, offset);

   DEBUG(VM_DEBUG_EXIT, "done (ret=%d, *eofp=%d).\n", ret, *eofp);
out:
   if (fullName != NULL) {
      os_free(fullName, MAXPATHLEN);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetattrInt --
 *
 *      "Gets the attributes for the supplied vnode." (Solaris Internals, p536)
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsGetattrInt(struct vnode *vp,      // IN : vnode of the file
	       HgfsVnodeAttr *vap)  // OUT: attributes container
{

   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   HgfsAttrV2 hgfsAttrV2;
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /* XXX It would be nice to do a GetattrByHandle when possible here. */
   ret = HgfsDoGetattrByName(HGFS_VP_TO_FILENAME(vp), sip, &hgfsAttrV2);

   if (!ret) {
      /*
       * HgfsDoGetattr obtained attributes from the hgfs server so
       * map the attributes into BSD attributes.
       */

      HgfsAttrToBSD(vp, &hgfsAttrV2, vap);
   }

   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetattrInt --
 *
 *      Maps the Mac OS/FreeBsd attributes to Hgfs attributes (by calling
 *      HgfsSetattrCopy()) and sends a set attribute request to the Hgfs server.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on error.
 *
 * Side effects:
 *      The file on the host will have new attributes.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSetattrInt(struct vnode *vp,     // IN : vnode of the file
               HgfsVnodeAttr *vap)   // IN : attributes container
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsRequestSetattrV3 *request;
   HgfsReplySetattrV3 *reply;
   uint32 reqSize;
   uint32 reqBufferSize;
   uint32 repSize;
   char *fullPath = NULL;
   uint32 fullPathLen;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));
   ASSERT(vp);
   ASSERT(vap);

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      goto out;
   }

   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestSetattrV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_SETATTR_V3);

   request->reserved = 0;

   /*
    * Fill the attributes and hint fields of the request.  If no updates are
    * needed then we will just return success without sending the request.
    */
   if (HgfsSetattrCopy(vap, &request->attr, &request->hints) == FALSE) {
      DEBUG(VM_DEBUG_COMM, "don't need to update attributes.\n");
      ret = 0;
      goto destroyOut;
   }

   fullPath = HGFS_VP_TO_FILENAME(vp);
   fullPathLen = HGFS_VP_TO_FILENAME_LENGTH(vp);

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);

   /*
    * Convert an input string to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   ret = HgfsNameToWireEncoding(fullPath, fullPathLen + 1,
                                request->fileName.name,
                                reqBufferSize);

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "Could not encode to wire format");
      ret = -ret;
      goto destroyOut;
   }

   request->fileName.fid = HGFS_INVALID_HANDLE;
   request->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
   request->fileName.flags = 0;
   request->fileName.length = ret;

   reqSize += ret;

   /* The request's size includes the header, request and filename. */
   HgfsKReq_SetPayloadSize(req, reqSize);

   if (!request->attr.mask) {
      /* they were trying to set filerev or vaflags, which we ignore */
      ret = 0;
      goto destroyOut;
   }

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* HgfsSubmitRequest() destroys the request if necessary. */
      goto out;
   }

   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);

   ret = HgfsGetStatus(req, repSize);
   if (ret) {
      if (ret == EPROTO) {
         DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      }
      goto destroyOut;
   } else {
      if (HGFS_VATTR_SIZE_IS_ACTIVE(vap, HGFS_VA_DATA_SIZE)) {
         HgfsSetFileSize(vp, vap->HGFS_VA_DATA_SIZE);
      }
   }

destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);
out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopRmdir --
 *
 *      Removes the specified name from the provided vnode.  Sends a DELETE
 *      request by calling HgfsDelete() with the filename and correct opcode to
 *      indicate deletion of a directory.
 *
 *      "Removes the directory pointed to by the supplied vnode." (Solaris
 *      Internals, p537)
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsRmdirInt(struct vnode *dvp,          // IN: parent directory
	     struct vnode *vp,           // IN: directory to remove
	     struct componentname *cnp)  // IN: Only used for debugging
{
   int ret = 0;
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(dvp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s/%.*s)\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   ret = HgfsDelete(sip, HGFS_VP_TO_FILENAME(vp), HGFS_OP_DELETE_DIR_V3);
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s/%.*s -> %d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRemoveInt --
 *
 *      Composes the full pathname of this file and sends a DELETE_FILE request
 *      by calling HgfsDelete().
 *
 * Results:
 *      Returns 0 on success or a non-zero error code on error.
 *
 * Side effects:
 *      If successful, the file specified will be deleted from the host's
 *      filesystem.
 *
 *----------------------------------------------------------------------------
 */

int HgfsRemoveInt(struct vnode *vp) // IN: Vnode to delete
{

   int ret = 0;

   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /* Removing directories is a no-no; save that for VNOP_RMDIR. */
   if (HGFS_VP_TO_VTYPE(vp) == VDIR) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRemove(). on dir ret EPERM\n");
      ret = EPERM;
      goto out;
   }

    os_FlushRange(vp, 0, HGFS_VP_TO_FILESIZE(vp));
    os_SetSize(vp, 0);

   /* We can now send the delete request. */
   ret = HgfsDelete(sip, HGFS_VP_TO_FILENAME(vp), HGFS_OP_DELETE_FILE_V3);

out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCloseInt --
 *
 *      Called by HgfsVnopClose under Mac OS or HgfsVopClose under FreeBSD to
 *      close a file.
 *
 *      "Closes the file given by the supplied vnode.  When this is the last
 *      close, some filesystems use vnop_close() to initiate a writeback of
 *      outstanding dirty pages by checking the reference cound in the vnode."
 *      (Solaris Internals, p536)
 *
 * Results:
 *      Always returns 0 success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsCloseInt(struct vnode *vp, // IN: Vnode to close.
             int mode)         // IN: Mode of vnode being closed.
{
   int ret = 0;
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s, %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
        mode);

   /*
    * If we are closing a directory we need to send a SEARCH_CLOSE request,
    * but if we are closing a regular file we need to send a CLOSE request.
    * Other file types are not supported by the Hgfs protocol.
    */

   switch (HGFS_VP_TO_VTYPE(vp)) {
   case VDIR:
      ret = HgfsDirClose(sip, vp);
      break;

   case VREG:
      ret = HgfsFileClose(sip, vp, mode);
      break;

   default:
      DEBUG(VM_DEBUG_FAIL, "unsupported filetype %d.\n",
	    HGFS_VP_TO_VTYPE(vp));
      ret = EINVAL;
      break;
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d -> 0)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
        ret);
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsOpenInt --
 *
 *      Invoked when open(2) is called on a file in our filesystem.  Sends an
 *      OPEN request to the Hgfs server with the filename of this vnode.
 *
 *      "Opens a file referenced by the supplied vnode.  The open() system call
 *      has already done a vnop_lookup() on the path name, which returned a vnode
 *      pointer and then calls to vnop_open().  This function typically does very
 *      little since most of the real work was performed by vnop_lookup()."
 *      (Solaris Internals, p537)
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      If the HgfsFile for this file does not already have a handle, it is
 *      given one that can be used for future read and write requests.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsOpenInt(struct vnode *vp,           // IN: Vnode to open.
            int mode,                   // IN: Mode of vnode being opened.
            HgfsOpenType openType)      // IN: TRUE if called outside of VNOP_OPEN.
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s, %d, %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         mode, openType);

   switch(HGFS_VP_TO_VTYPE(vp)) {
   case VDIR:
      DEBUG(VM_DEBUG_LOG, "opening a directory\n");
      ret = HgfsDirOpen(sip, vp, openType);
      break;

   case VREG:
      /*
       * If HgfsCreate() was called prior to this then is would set permissions
       * in HgfsFile that we need to pass to HgfsFileOpen.
       * If HgfsCreate has not been called then file already exists and permissions
       * are ignored by HgfsFileOpen.
       */
      DEBUG(VM_DEBUG_LOG, "opening a file with flag %x\n", mode);
      ret = HgfsFileOpen(sip, vp, mode, HGFS_VP_TO_PERMISSIONS(vp), openType);
      break;

   default:
      DEBUG(VM_DEBUG_FAIL,
            "HgfsOpen: unrecognized file of type %d.\n", HGFS_VP_TO_VTYPE(vp));
      ret = EINVAL;
      break;
   }

   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp),
         HGFS_VP_TO_FILENAME(vp), ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsLookupInt --
 *
 *      Looks in the provided directory for the specified filename.  If we cannot
 *      determine the vnode locally (i.e, the vnode is not the root vnode of the
 *      filesystem provided by dvp or in our hashtable), we send a getattr
 *      request to the server and allocate a vnode and internal filesystem state
 *      for this file.
 *
 * Results:
 *      Returns zero on success and ENOENT if the file cannot be found
 *      If file is found, a vnode representing the file is returned in vpp.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsLookupInt(struct vnode *dvp,         // IN : directory vnode
	      struct vnode **vpp,        // OUT: ptr to vnode if it exists
	      struct componentname *cnp) // IN : pathname to component
{
   HgfsAttrV2 attrV2;
   HgfsSuperInfo *sip;
   char *path = NULL;
   int ret = 0;
   int len = 0;

   ASSERT(dvp);
   ASSERT(vpp);
   ASSERT(cnp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s, %.*s).\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         (int)cnp->cn_namelen, cnp->cn_nameptr);

   if (cnp->cn_flags & ISDOTDOT) {
      HgfsFile *fp = HGFS_VP_TO_FP(dvp);
      ASSERT(fp);
      if (fp->parent == NULL) {
         return EIO; // dvp is root directory
      } else {
#if defined __FreeBSD__
         vref(fp->parent);
#else
         vnode_get(fp->parent);
#endif
         *vpp = fp->parent;
         return 0;
      }
   }
   if (cnp->cn_namelen == 1 && *cnp->cn_nameptr == '.') {
#if defined __FreeBSD__
      vref(dvp);
#else
      vnode_get(dvp);
#endif
      *vpp = dvp;
      return 0;
   }

   /*
    * Get pointer to the superinfo.  If the device is not attached,
    * hgfsInstance will not be valid and we immediately return an error.
    */
   sip = HGFS_VP_TO_SIP(dvp);
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "couldn't acquire superinfo.\n");
      return ENOTSUP;
   }

   /* Snag a pathname buffer */
   path = os_malloc(MAXPATHLEN, M_WAITOK);
   if (!path) {
      return ENOMEM;
   }

   /* Construct the full path for this lookup. */
   len = HgfsMakeFullName(HGFS_VP_TO_FILENAME(dvp),        // Path to this file
                          HGFS_VP_TO_FILENAME_LENGTH(dvp), // Length of path
                          cnp->cn_nameptr,                 // File's name
                          cnp->cn_namelen,                 // Filename length
                          path,                            // Destination buffer
                          MAXPATHLEN);                     // Size of dest buffer
   if (len < 0) {
      DEBUG(VM_DEBUG_FAIL, "LookupInt length is less than zero\n");
      ret = EINVAL;
      goto out;
   }

   DEBUG(VM_DEBUG_LOAD, "full path is \"%s\"\n", path);

   /* See if the lookup is really for the root vnode. */
   if (strcmp(path, "/") == 0) {
      DEBUG(VM_DEBUG_INFO, "returning the root vnode.\n");
      *vpp = sip->rootVnode;
      /*
       * If we are returning the root vnode, then we need to get a reference
       * to it. Under Mac OS this gets an I/O Count.
       */
      HGFS_VPP_GET_IOCOUNT(vpp);
      goto out;
   };

   /* Send a Getattr request to the Hgfs server. */
   ret = HgfsDoGetattrByName(path, sip, &attrV2);

   /*
    * If this is the final pathname component & the user is attempt a CREATE
    * or RENAME, just return without a leaf vnode.  (This differs from
    * Solaris where ENOENT would be returned in all cases.)
    */
   if (ret == ENOENT) {
      if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
          cnp->cn_flags & ISLASTCN) {
         ret = EJUSTRETURN;
         DEBUG(VM_DEBUG_FAIL, "GetattrByName error %d for \"%s\".\n", ret, path);
	 goto out;
      }
   }

   /* Got an error from HgfsDoGetattrByName, return it to the caller. */
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "GetattrByName error %d for \"%s\".\n", ret, path);
      goto out;
   }

   ret = HgfsVnodeGet(vpp,           // Location to write vnode's address
                      dvp,                     // Parent vnode
                      sip,                     // Superinfo
                      HGFS_VP_TO_MP(dvp),      // VFS for our filesystem
                      path,                    // Full name of the file
                      attrV2.type,             // Type of file
                      &sip->fileHashTable,     // File hash table
                      FALSE,                   // Not a new file creation
                      0,                       // No permissions - not a new file
                      attrV2.size);            // File size

   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "couldn't create vnode for \"%s\".\n", path);
      goto out;
   }

   /*
    * Either we will have a cache hit or called HgfsVnodeGet. Both of these
    * paths guarantees that *vpp will be set to a vnode.
    */
   ASSERT(*vpp);

   DEBUG(VM_DEBUG_LOAD, "assigned vnode %p to %s\n", *vpp, path);

   ret = 0;     /* Return success */

out:
   if (path != NULL) {
      os_free(path, MAXPATHLEN);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s, %.*s -> %d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         (int)cnp->cn_namelen, cnp->cn_nameptr, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCreateInt --
 *
 *      Called by either HgfsVnopCreate under Mac OS or HgfsVopCreate under
 *      FreeBSD when the user is trying to create a file by calling open() with
 *      the O_CREAT flag specified.
 *
 *      The kernel calls the open entry point which calls (HgfsOpenInt()) after
 *      calling this function, so here all we do is consruct the vnode and
 *      save the filename and permission bits for the file to be created within
 *      our filesystem internal state.
 *
 * Results:
 *      Returns zero on success and an appropriate error code on error.
 *
 * Side effects:
 *      If the file doesn't exist, a vnode will be created.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsCreateInt(struct vnode *dvp,         // IN : Directory vnode
              struct vnode **vpp,        // OUT: Pointer to new vnode
              struct componentname *cnp, // IN : Location to create new vnode
              int mode)                  // IN : Mode of vnode being created.
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(dvp);
   char *fullname = NULL;       // allocated from M_TEMP; free when done.
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s/%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         (int)cnp->cn_namelen, cnp->cn_nameptr);

   if (*vpp != NULL) {
      DEBUG(VM_DEBUG_FAIL, "vpp (%p) not null\n", vpp);
      ret = EEXIST;
      goto out;
   }

   /* If we have gotten to this point then we know that we need to create a
    * new vnode. The actual file will be created on the HGFS server in the
    * HgfsOpenInt call should happen right after this call.
    */
   fullname = os_malloc(MAXPATHLEN, M_WAITOK);
   if (!fullname) {
      ret = ENOMEM;
      goto out;
   }

   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(dvp),  // Name of directory to create in
                          HGFS_VP_TO_FILENAME_LENGTH(dvp), // Length of name
                          cnp->cn_nameptr,           // Name of file to create
                          cnp->cn_namelen,           // Length of new filename
                          fullname,                  // Buffer to write full name
                          MAXPATHLEN);               // Size of this buffer

   if (ret >= 0) {
      /* Create the vnode for this file. */
      ret = HgfsVnodeGet(vpp, dvp, sip, HGFS_VP_TO_MP(dvp), fullname,
                         HGFS_FILE_TYPE_REGULAR, &sip->fileHashTable, TRUE,
                         mode, 0);
      /* HgfsVnodeGet() guarantees this. */
      ASSERT(ret != 0 || *vpp);
      /*
       * NOTE: This is a temporary workaround.
       * This condition may occur because we look up vnodes by file name in the
       * vnode cache.
       * There is a race condition when file is already deleted but still referenced -
       * thus vnode still exist. If a new file with the same name is created I
       * can neither use the vnode of the deleted file nor insert a new vnode with
       * the same name - thus I fail the request. This behavior is not correct and will
       * be fixed after further restructuring if the source code.
       */
      if (ret == EEXIST) {
         ret = EIO;
      }
   } else {
      DEBUG(VM_DEBUG_FAIL, "couldn't create full path name.\n");
      ret = ENAMETOOLONG;
   }

out:
   if (fullname != NULL) {
      os_free(fullname, MAXPATHLEN);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s/%.*s -> %d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         (int)cnp->cn_namelen, cnp->cn_nameptr, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReadInt --
 *
 *      Called by HgfsVnopRead under Mac OS or HgfsVopRead under FreeBSD to read
 *      a file.
 *
 *      We call HgfsDoRead() to fill the user's buffer until the request is met
 *      or the file has no more data.  This is done since we can only transfer
 *      HGFS_IO_MAX bytes in any one request.
 *
 *      "Reads the range supplied for the given vnode.  vop_read() typically
 *      maps the requested range of a file into kernel memory and then uses
 *      vop_getpage() to do the real work." (Solaris Internals, p537)
 *
 * Results:
 *      Returns zero on success and an error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsReadInt(struct vnode *vp, // IN    : Vnode to read from
            struct uio *uiop, // IN/OUT: Buffer to write data into.
            Bool pagingIo)    // IN: True if the read is a result of a page fault
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   HgfsHandle handle;
   uint64_t offset;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /* We can't read from directories, that's what readdir() is for. */
   if (HGFS_VP_TO_VTYPE(vp) != VREG) {
      DEBUG(VM_DEBUG_FAIL, "Can only read regular files.\n");
      ret = (HGFS_VP_TO_VTYPE(vp) == VDIR) ? EISDIR : EPERM;
      DEBUG(VM_DEBUG_FAIL, "Read not a reg file type %d ret %d.\n", HGFS_VP_TO_VTYPE(vp), ret);
      return ret;
   }

   /* off_t is a signed quantity */
   if (HGFS_UIOP_TO_OFFSET(uiop) < 0) {
      DEBUG(VM_DEBUG_FAIL, "given negative offset.\n");
      return EINVAL;
   }

   /* This is where the user wants to start reading from in the file. */
   offset = HGFS_UIOP_TO_OFFSET(uiop);

   /*
    * We need to get the handle for the requests sent to the Hgfs server.  Note
    * that this is guaranteed to not change until a close(2) is called on this
    * vnode, so it's safe and correct to acquire it outside the loop below.
    */
   ret = HgfsGetOpenFileHandle(vp, &handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "could not get handle.\n");
      return EINVAL;
   }

   /* Flush mmaped data to maintain data coherence between mmap and read. */
   if (!pagingIo) {
      ret = os_FlushRange(vp, offset, HGFS_UIOP_TO_RESID(uiop));
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "could not flush data.\n");
         return EINVAL;
      }
   }

   /*
    * Here we loop around HgfsDoRead with requests less than or equal to
    * HGFS_IO_MAX until one of the following conditions is met:
    *  (1) All the requested data has been read
    *  (2) The file has no more data
    *  (3) An error occurred
    *
    * Since HgfsDoRead() calls uiomove(9), we know condition (1) is met when
    * the uio structure's uio_resid is decremented to zero.  If HgfsDoRead()
    * returns 0 we know condition (2) was met, and if it returns less than 0 we
    * know condtion (3) was met.
    */
   do {
      uint32_t size;

      /* Request at most HGFS_IO_MAX bytes */
      size = (HGFS_UIOP_TO_RESID(uiop) > HGFS_IO_MAX) ? HGFS_IO_MAX :
                                                        HGFS_UIOP_TO_RESID(uiop);

      DEBUG(VM_DEBUG_INFO, "offset=%"FMT64"d, uio_offset=%"FMT64"d\n",
            offset, HGFS_UIOP_TO_OFFSET(uiop));
      DEBUG(VM_DEBUG_HANDLE, "** handle=%d, file=%s to read %u\n",
            handle, HGFS_VP_TO_FILENAME(vp), size);

      if (size == 0) {
         /* For a zero byte length read we return success. */
         DEBUG(VM_DEBUG_EXIT, "size of 0 ret -> 0.\n");
         return 0;
      }

      /* Send one read request. */
      ret = HgfsDoRead(sip, handle, offset, size, uiop);
      if (ret == 0) {
         /* On end of file we return success */
         DEBUG(VM_DEBUG_EXIT, "end of file reached.\n");
         return 0;
      } else if (ret == -EBADF) { // Stale host handle
         ret = HgfsRenewHandle(vp, sip, &handle);
         if (ret == 0) {
            ret = HgfsDoRead(sip, handle, offset, size, uiop);
            if (ret < 0) {
               DEBUG(VM_DEBUG_FAIL, "Failed to read from a fresh handle.\n");
               return -ret;
            }
         } else {
            DEBUG(VM_DEBUG_FAIL, "Failed to get a fresh handle.\n");
            return EBADF;
         }
      } else if (ret < 0) {
         /*
          * HgfsDoRead() returns the negative of an appropriate error code to
          * differentiate between success and error cases.  We flip the sign
          * and return the appropriate error code.  See the HgfsDoRead()
          * function header for a fuller explanation.
          */
         DEBUG(VM_DEBUG_FAIL, "HgfsDoRead() failed, error %d.\n", ret);
         return -ret;
      }

      /* Bump the offset past where we have already read. */
      offset += ret;
   } while (HGFS_UIOP_TO_RESID(uiop));

   /* We fulfilled the user's read request, so return success. */
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> 0)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsWriteInt --
 *
 *      Called by HgfsVnopWrite under Mac OS or HgfsVopWrite under FreeBSD.
 *
 *      We call HgfsDoWrite() once with requests less than or equal to
 *      HGFS_IO_MAX bytes until the user's write request has completed.
 *
 * Results:
 *      Returns 0 on success and error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsWriteInt(struct vnode *vp, // IN    : the vnode of the file
             struct uio *uiop, // IN/OUT: location of data to be written
             int ioflag,       // IN    : hints & other directives
             Bool pagingIo)    // IN: True if the write is originated by memory manager
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   HgfsHandle handle;
   uint64_t offset;
   int ret = 0;
   int error = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /* Skip write requests for 0 bytes. */
   if (HGFS_UIOP_TO_RESID(uiop) == 0) {
      DEBUG(VM_DEBUG_INFO, "write of 0 bytes requested.\n");
      error = 0;
      goto out;
   }

   DEBUG(VM_DEBUG_INFO, "file is %s\n", HGFS_VP_TO_FILENAME(vp));

   /* Off_t is a signed type. */
   if (HGFS_UIOP_TO_OFFSET(uiop) < 0) {
      DEBUG(VM_DEBUG_FAIL, "given negative offset.\n");
      error = EINVAL;
      goto out;
   }

   /* This is where the user will begin writing into the file. */
   offset = HGFS_UIOP_TO_OFFSET(uiop);

   /* Get the handle we need to supply the Hgfs server. */
   ret = HgfsGetOpenFileHandle(vp, &handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "could not get handle.\n");
      error = EINVAL;
      goto out;
   }

   /* Flush mmaped data to maintain data coherence between mmap and read. */
   if (!pagingIo && (ioflag & IO_APPEND) == 0) {
      ret = os_FlushRange(vp, offset, HGFS_UIOP_TO_RESID(uiop));
   }

   /*
    * We loop around calls to HgfsDoWrite() until either (1) we have written all
    * of our data or (2) an error has occurred.  HGFS_UIOP_TO_RESID(uiop) is decremented
    * by uiomove(9F) inside HgfsDoWrite(), so condition (1) is met when it
    * reaches zero.  Condition (2) occurs when HgfsDoWrite() returns less than
    * zero.
    */
   do {
      uint32_t size;

      DEBUG(VM_DEBUG_INFO, "** offset=%"FMT64"d, uio_offset=%"FMT64"d\n",
            offset, HGFS_UIOP_TO_OFFSET(uiop));
      DEBUG(VM_DEBUG_HANDLE, "** handle=%d, file=%s\n",
            handle, HGFS_VP_TO_FILENAME(vp));

      /* Write at most HGFS_IO_MAX bytes. */
      size = (HGFS_UIOP_TO_RESID(uiop) > HGFS_IO_MAX) ? HGFS_IO_MAX : HGFS_UIOP_TO_RESID(uiop);

      /* Send one write request. */
      ret = HgfsDoWrite(sip, handle, ioflag, offset, size, uiop);
      if (ret == -EBADF) { // Stale host handle
         ret = HgfsRenewHandle(vp, sip, &handle);
         if (ret == 0) {
            ret = HgfsDoWrite(sip, handle, ioflag, offset, size, uiop);
            if (ret < 0) {
               DEBUG(VM_DEBUG_FAIL, "Failed to write to a fresh handle.\n");
               error = -ret;
               break;
            }
         } else {
            DEBUG(VM_DEBUG_FAIL, "Failed to get a fresh handle, error %d.\n", ret);
            error = EBADF;
            break;
         }
      } else if (ret < 0) {
         /*
          * As in HgfsRead(), we need to flip the sign.  See the comment in the
          * function header of HgfsDoWrite() for a more complete explanation.
          */
         DEBUG(VM_DEBUG_INFO, "HgfsDoWrite failed, returning %d\n", -ret);
         error = -ret;
         break;
      }

      /* Increment the offest by the amount already written. */
      offset += ret;

   } while (HGFS_UIOP_TO_RESID(uiop));

   /* Need to notify memory manager if written data extended the file. */
   if (!pagingIo && (offset > HGFS_VP_TO_FILESIZE(vp))) {
      if ((ioflag & IO_APPEND) == 0) {
         os_SetSize(vp, offset);
      } else {
         off_t oldSize = HGFS_VP_TO_FILESIZE(vp);
         off_t writtenData = offset - HGFS_UIOP_TO_OFFSET(uiop);
         os_SetSize(vp, oldSize + writtenData);
      }
   }

out:
   /* We have completed the user's write request, so return. */
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp), error);

   return error;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMkdirInt --
 *
 *      Makes a directory named dirname in the directory specified by the dvp
 *      vnode by sending a CREATE_DIR request, then allocates a vnode for this
 *      new directory and writes its address into vpp.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      If successful, a directory is created on the host's filesystem.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsMkdirInt(struct vnode *dvp,         // IN : directory vnode
	     struct vnode **vpp,        // OUT: pointer to new directory vnode
	     struct componentname *cnp, // IN : pathname to component
	     int mode)                  // IN : mode to create dir
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(dvp);
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsRequestCreateDirV3 *request;
   HgfsReplyCreateDirV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   uint32 reqBufferSize;
   char *fullName = NULL;       // allocated from M_TEMP; free when done.
   uint32 fullNameLen;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s/%.*s,%d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         (int)cnp->cn_namelen, cnp->cn_nameptr, mode);

   /*
    * We need to construct the full path of the directory to create then send
    * a CREATE_DIR request.  If successful we will create a vnode and fill in
    * vpp with a pointer to it.
    *
    * Note that unlike in HgfsCreate(), *vpp is always NULL.
    */

   /* Construct the complete path of the directory to create. */
   fullName = os_malloc(MAXPATHLEN, M_WAITOK);
   if (!fullName) {
      ret = ENOMEM;
      goto out;
   }

   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(dvp),        // Parent directory
                          HGFS_VP_TO_FILENAME_LENGTH(dvp), // Length of name
                          cnp->cn_nameptr,                 // Name of file to create
                          cnp->cn_namelen,                 // Length of filename
                          fullName,                        // Buffer to write full name
                          MAXPATHLEN);                     // Size of this buffer

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "couldn't create full path name.\n");
      ret = ENAMETOOLONG;
      goto out;
   }
   fullNameLen = ret;

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      goto out;
   }

   /* Initialize the request's contents. */
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestCreateDirV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_CREATE_DIR_V3);

   request->fileAttr = 0;
   request->mask = HGFS_CREATE_DIR_MASK;
   request->specialPerms = (mode & (S_ISUID | S_ISGID | S_ISVTX)) >>
                              HGFS_ATTR_SPECIAL_PERM_SHIFT;
   request->ownerPerms = (mode & S_IRWXU) >> HGFS_ATTR_OWNER_PERM_SHIFT;
   request->groupPerms = (mode & S_IRWXG) >> HGFS_ATTR_GROUP_PERM_SHIFT;
   request->otherPerms = mode & S_IRWXO;
   request->fileName.flags = 0;
   request->fileName.fid = HGFS_INVALID_HANDLE;
   request->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);

   /*
    * Convert an input string to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   ret = HgfsNameToWireEncoding(fullName, fullNameLen + 1,
                                request->fileName.name,
                                reqBufferSize);

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL,"Could not encode to wire format");
      ret = -ret;
      goto destroyOut;
   }

   request->fileName.length = ret;
   reqSize += ret;

   /* Set the size of this request. */
   HgfsKReq_SetPayloadSize(req, reqSize);

   /* Send the request to guestd. */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* Request is destroyed in HgfsSubmitRequest() if necessary. */
      goto out;
   }

   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);

   ret = HgfsGetStatus(req, repSize);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      goto destroyOut;
   }

   ret = HgfsVnodeGet(vpp, dvp, sip, HGFS_VP_TO_MP(dvp), fullName,
                      HGFS_FILE_TYPE_DIRECTORY, &sip->fileHashTable, TRUE,
                      mode, 0);
   if (ret) {
      ret = EIO;
      DEBUG(VM_DEBUG_FAIL, "Error encountered creating vnode ret = %d\n", ret);
      goto destroyOut;
   }

   ASSERT(*vpp);
   ret = 0;

destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);
out:
   if (fullName != NULL) {
      os_free(fullName, MAXPATHLEN);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s/%.*s -> %d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         (int)cnp->cn_namelen, cnp->cn_nameptr, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDirOpen --
 *
 *      Invoked when HgfsOpen() is called with a vnode of type VDIR.
 *
 *      Sends a SEARCH_OPEN request to the Hgfs server.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDirOpen(HgfsSuperInfo *sip,         // IN: Superinfo pointer
            struct vnode *vp,           // IN: Vnode of directory to open
            HgfsOpenType openType)      // IN: type of VNOP_OPEN.
{
   char *fullPath;
   uint32 fullPathLen;
   int ret;
   HgfsFile *fp;
   HgfsHandle handle;

   ASSERT(sip);
   ASSERT(vp);

   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s,%d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         openType);

   /*
    *  If the directory is already opened then we are done.
    *  There is no different open modes for directories thus the handle is compatible.
    */
   os_rw_lock_lock_exclusive(fp->handleLock);
   ret = HgfsCheckAndReferenceHandle(vp, 0, openType);
   if (ret ==  ENOENT) {  // Handle is not set, need to get one from the host

      if (HGFS_IS_ROOT_VNODE(sip, vp)) {
         fullPath = "";
         fullPathLen = 0;
      } else {
         fullPath = HGFS_VP_TO_FILENAME(vp);
         fullPathLen = HGFS_VP_TO_FILENAME_LENGTH(vp);
      }

      ret = HgfsSendOpenDirRequest(sip, fullPath, fullPathLen, &handle);
      if (ret == 0) {
         /*
          * We successfully received a reply, so we need to save the handle in
          * this file's HgfsOpenFile and return success.
          */
         HgfsSetOpenFileHandle(vp, handle, HGFS_OPEN_MODE_READ_ONLY, openType);
      }
   }
   os_rw_lock_unlock_exclusive(fp->handleLock);

   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRequestHostFileHandle --
 *
 *    Sends a open request to the server to get a file handle.
 *    If client needs a readonly handle the function first asks for
 *    read-write handle since this handle may be shared between multiple
 *    file descriptors. If getting read-write handle fails the function
 *    sends another request for readonly handle.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsRequestHostFileHandle(HgfsSuperInfo *sip,   // IN: Superinfo pointer
                          struct vnode *vp,     // IN: Vnode of file to open
                          int *openMode,        // IN OUT: open mode
                          int openFlags,        // IN: flags for the open request
                          int permissions,      // IN: Permissions for new files
                          HgfsHandle *handle)   // OUT: file handle
{
   char *fullPath;
   uint32 fullPathLen;
   int ret;

   fullPath = HGFS_VP_TO_FILENAME(vp);
   fullPathLen = HGFS_VP_TO_FILENAME_LENGTH(vp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s, %d, %d, %o)\n", fullPathLen, fullPath,
         *openMode, openFlags, permissions);

   /* First see if we can get the most permissive read/write open mode */
   ret = HgfsSendOpenRequest(sip, HGFS_OPEN_MODE_READ_WRITE, openFlags,
                             permissions, fullPath, fullPathLen, handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Open failed %d, re-submitting original mode = %d.\n",
            ret, *openMode);
      if (ret == EACCES && HGFS_OPEN_MODE_READ_WRITE != *openMode) {
         /*
          * Failed to open in read/write open mode because of denied access.
          * It means file's permissions do not allow opening for read/write.
          * However caller does not need this mode and may be satisfied with
          * less permissive mode.
          * Try exact open mode now.
          */
         DEBUG(VM_DEBUG_FAIL, "RW mode failed, re-submitting original mode = %d.\n",
               *openMode);
         ret = HgfsSendOpenRequest(sip, *openMode, openFlags,
                                   permissions, fullPath, fullPathLen, handle);
      }
   } else {
      *openMode = HGFS_OPEN_MODE_READ_WRITE;
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", fullPathLen, fullPath, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileOpen --
 *
 *      Invoked when HgfsOpen() is called with a vnode of type VREG.  Sends
 *      a OPEN request to the Hgfs server.
 *
 *      Note that this function doesn't need to handle creations since the
 *      HgfsCreate() entry point is called by the kernel for that.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsFileOpen(HgfsSuperInfo *sip,        // IN: Superinfo pointer
             struct vnode *vp,          // IN: Vnode of file to open
             int flag,                  // IN: Flags of open
             int permissions,           // IN: Permissions of open (only when creating)
             HgfsOpenType openType)     // IN: initiator type for the open
{
   int ret;
   int openMode;
   int openFlags;
   HgfsHandle handle;
   HgfsFile *fp;

   ASSERT(sip);
   ASSERT(vp);

   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s,%d,%d,%o)\n",
         HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         flag, openType, permissions);

   /*
    * Check if the user is trying to create a new share. This check was
    * mainly implemented to address the issue with Mac OS. When the user
    * attempts to create a file in the root folder, the server returns ENOENT
    * error code. However, Mac OS specifically checks for this case. If Mac OS asks for
    * the creation of a new file and if it gets ENOENT as a return error code,
    * then it assumes that the error was because of some race condition and tries it
    * again. Thus, returning ENOENT to the Mac OS puts the guest kernel into infinite
    * loop. In order to resolve this issue, before passing on the request to the
    * server, we validate if user is attempting to create a new share. If yes,
    * we return EACCES as the error code.
    */
   if (HgfsAttemptToCreateShare(HGFS_VP_TO_FILENAME(vp), flag)) {
      DEBUG (VM_DEBUG_LOG, "An attempt to create a new share was made.\n");
      ret = EACCES;
      goto out;
   }

   /* Convert FreeBSD modes to Hgfs modes */
   openMode = HgfsGetOpenMode((uint32_t)flag);
   if (openMode < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetOpenMode failed.\n");
      ret = EINVAL;
      goto out;
   }
   DEBUG(VM_DEBUG_COMM, "open mode is %x\n", openMode);

   /* Convert FreeBSD flags to Hgfs flags */
   openFlags = HgfsGetOpenFlags((uint32_t)flag);
   if (openFlags < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetOpenFlags failed.\n");
      ret = EINVAL;
      goto out;
   }

   os_rw_lock_lock_exclusive(fp->handleLock);
   /*
    *  If the file is already opened, verify that it is opened in a compatible mode.
    *  If it is true then add reference to vnode and grant the access, otherwise
    *  deny the access.
    */
   ret = HgfsCheckAndReferenceHandle(vp, openMode, openType);
   if (ret == ENOENT) {  // Handle is not set, need to get one from the host
      ret = HgfsRequestHostFileHandle(sip, vp, &openMode, openFlags,
                                      permissions, &handle);
      /*
       * We successfully received a reply, so we need to save the handle in
       * this file's HgfsOpenFile and return success.
       */
      if (ret == 0) {
         HgfsSetOpenFileHandle(vp, handle, openMode, openType);
      }
   }

   os_rw_lock_unlock_exclusive(fp->handleLock);

out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRefreshDirHandle --
 *
 *      Refresh the directory HgfsHandle for the vnode.
 *      Needed when original handle become stale because the directory contents
 *      have changed either in the VM or on the remote server.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsRefreshDirHandle(struct vnode *vp,          // IN: Vnode of file to open
                     HgfsSuperInfo *sip,        // IN: Superinfo pointer
                     HgfsHandle *handle)        // IN/OUT: handle to close and reopen
{
   int ret = 0;
   HgfsFile *fp;

   ASSERT(vp);
   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   os_rw_lock_lock_exclusive(fp->handleLock);

   ASSERT(HGFS_VP_TO_VTYPE(vp) == VDIR);

   if (fp->handle != *handle) {
      /* Handle has been refreshed in another thread. */
      *handle = fp->handle;
   } else {
      /* Close the existing handle and open a new one from the host. */
      HgfsCloseServerDirHandle(sip, *handle);
      ret = HgfsGetNewHandle(vp, sip, fp, handle);
   }

   os_rw_lock_unlock_exclusive(fp->handleLock);

   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRenewHandle --
 *
 *      Renew the HgfsHandle for the vnode. Needed when original handle
 *      become stale because HGFS has been disabled and re-enabled or VM
 *      has been suspened and then resumed.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsRenewHandle(struct vnode *vp,          // IN: Vnode of file to open
                HgfsSuperInfo *sip,        // IN: Superinfo pointer
                HgfsHandle *handle)        // IN/OUT: Pointer to the stale handle
{
   int result = 0;
   HgfsFile *fp;

   ASSERT(vp);
   fp = HGFS_VP_TO_FP(vp);
   ASSERT(fp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   os_rw_lock_lock_exclusive(fp->handleLock);

   if (fp->handle != *handle) {
      /* Handle has been refreshed in another thread. */
      *handle = fp->handle;
   } else {
      result = HgfsGetNewHandle(vp, sip, fp, handle);
   }
   os_rw_lock_unlock_exclusive(fp->handleLock);

   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         result);
   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetNewHandle --
 *
 *      Get a new HgfsHandle for the vnode. Needed when original handle
 *      become stale because HGFS has been disabled and re-enabled or VM
 *      has been suspened and then resumed, or folder contents have changed.
 *
 *      NOTE: file lock must be acquired when calling this function.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsGetNewHandle(struct vnode *vp,          // IN: Vnode of file to open
                 HgfsSuperInfo *sip,        // IN: Superinfo pointer
                 HgfsFile *fp,              // IN/OUT: file node pointer
                 HgfsHandle *handle)        // IN OUT: Pointer to the stale handle
{
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /* Retrieve a new handle from the host. */
   if (HGFS_VP_TO_VTYPE(vp) == VREG) {
      ret = HgfsRequestHostFileHandle(sip, vp, (int *)&fp->mode,
                                      HGFS_OPEN, 0, handle);
   } else if (HGFS_VP_TO_VTYPE(vp) == VDIR) {
      char *fullPath = HGFS_VP_TO_FILENAME(vp);
      uint32 fullPathLen = HGFS_VP_TO_FILENAME_LENGTH(vp);

      ret = HgfsSendOpenDirRequest(sip, fullPath, fullPathLen, handle);
   } else {
      goto out;
   }

   if (ret == 0) {
      fp->handle = *handle;
   }

out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDirClose --
 *
 *      Invoked when HgfsClose() is called with a vnode of type VDIR.
 *
 *      Sends an SEARCH_CLOSE request to the Hgfs server.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDirClose(HgfsSuperInfo *sip,        // IN: Superinfo pointer
             struct vnode *vp)          // IN: Vnode of directory to close
{
   int ret = 0;
   HgfsHandle handleToClose;

   ASSERT(sip);
   ASSERT(vp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /*
    * Check to see if we should close the file handle on the host ( which happen when
    * the reference count of the current handle become 0.
    */
   if (HgfsReleaseOpenFileHandle(vp, OPENREQ_OPEN, &handleToClose) == 0) {
      ret = HgfsCloseServerDirHandle(sip, handleToClose);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileClose --
 *
 *      Invoked when HgfsClose() is called with a vnode of type VREG.
 *
 *      Sends a CLOSE request to the Hgfs server.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsFileClose(HgfsSuperInfo *sip,       // IN: Superinfo pointer
              struct vnode *vp,         // IN: Vnode of file to close
              int flags)                // IN: The mode flags for the close
{
   int ret = 0;
   HgfsHandle handleToClose;

   ASSERT(sip);
   ASSERT(vp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s,%d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         flags);

   /*
    * Check to see if we should close the file handle on the host ( which happen when
    * the reference count of the current handle become 0.
    */
   if (HgfsReleaseOpenFileHandle(vp, OPENREQ_OPEN, &handleToClose) == 0) {
      ret = HgfsCloseServerFileHandle(sip, handleToClose);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDoRead --
 *
 *      Sends a single READ request to the Hgfs server and writes the contents
 *      into the user's buffer if successful.
 *
 *      This function is called repeatedly by HgfsRead() with requests of size
 *      less than or equal to HGFS_IO_MAX.
 *
 *      Note that we return the negative of an appropriate error code in this
 *      function so we can differentiate between success and failure.  On success
 *      we need to return the number of bytes read, but FreeBSD's error codes are
 *      positive so we negate them before returning.  If callers want to return
 *      these error codes to the Kernel, they will need to flip their sign.
 *
 * Results:
 *      Returns number of bytes read on success and a negative value on error.
 *
 * Side effects:
 *      On success, size bytes are written into the user's buffer.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDoRead(HgfsSuperInfo *sip,  // IN: Superinfo pointer
           HgfsHandle handle,   // IN: Server's handle to read from
           uint64_t offset,     // IN: File offset to read at
           uint32_t size,       // IN: Number of bytes to read
           struct uio *uiop)    // IN: Defines user's read request
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestReadV3 *request;
   HgfsReplyReadV3 *reply;
   uint32 reqSize;
   int ret;

   ASSERT(sip);
   ASSERT(uiop);
   ASSERT(size <= HGFS_IO_MAX); // HgfsRead() should guarantee this

   DEBUG(VM_DEBUG_ENTRY, "Enter(%u,%"FMT64"u,%u)\n", handle, offset, size);

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      ret = -ret;
      goto out;
   }

   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestReadV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_READ_V3);

   /* Indicate which file, where in the file, and how much to read. */
   request->file = handle;
   request->offset = offset;
   request->requiredSize = size;
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);

   HgfsKReq_SetPayloadSize(req, reqSize);

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /*
       * We need to flip the sign of the return value to indicate error; see
       * the comment in the function header.  HgfsSubmitRequest() handles
       * destroying the request if necessary, so we don't here.
       */
      DEBUG(VM_DEBUG_FAIL, " hgfssubmitrequest failed\n");
      ret = -ret;
      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
   reply = (HgfsReplyReadV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);

   ret = HgfsGetStatus(req, sizeof *replyHeader);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      if (ret != EPROTO && ret != EBADF) {
         ret = EACCES;
      }
      ret = -ret;
      goto destroyOut;
   }

   /*
    * Now perform checks on the actualSize.  There are three cases:
    *  o actualSize is less than or equal to size, which indicates success
    *  o actualSize is zero, which indicates the end of the file (and success)
    *  o actualSize is greater than size, which indicates a server error
    */
   if (reply->actualSize <= size) {
      /* If we didn't get any data, we don't need to copy to the user. */
      if (reply->actualSize == 0) {
         goto success;
      }

      /* Perform the copy to the user */
      ret = uiomove(reply->payload, reply->actualSize, uiop);
      if (ret) {
         ret = -EIO;
         goto destroyOut;
      }

      /* We successfully copied the payload to the user's buffer */
      goto success;

   } else {
      /* We got too much data: server error. */
      DEBUG(VM_DEBUG_FAIL, "received too much data in payload.\n");
      ret = -EPROTO;
      goto destroyOut;
   }

success:
   ret = reply->actualSize;
destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);
out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%u -> %d)\n", handle, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDoWrite --
 *
 *      Sends a single WRITE request to the Hgfs server with the contents of
 *      the user's buffer.
 *
 *      This function is called repeatedly by HgfsWrite() with requests of size
 *      less than or equal to HGFS_IO_MAX.
 *
 *      Note that we return the negative of an appropriate error code in this
 *      function so we can differentiate between success and failure.  On success
 *      we need to return the number of bytes written, but FreeBSD's error codes are
 *      positive so we negate them before returning.  If callers want to return
 *      these error codes to the kernel, they will need to flip their sign.
 *
 * Results:
 *      Returns number of bytes written on success and a negative value on error.
 *
 * Side effects:
 *      On success, size bytes are written to the file specified by the handle.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDoWrite(HgfsSuperInfo *sip, // IN: Superinfo pointer
            HgfsHandle handle,  // IN: Handle representing file to write to
            int ioflag,         // IN: Flags for write
            uint64_t offset,    // IN: Where in the file to begin writing
            uint32_t size,      // IN: How much data to write
            struct uio *uiop)   // IN: Describes user's write request
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestWriteV3 *request;
   HgfsReplyWriteV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   int ret;

   ASSERT(sip);
   ASSERT(uiop);
   ASSERT(size <= HGFS_IO_MAX); // HgfsWrite() guarantees this

   DEBUG(VM_DEBUG_ENTRY, "Enter(%u,%d,%"FMT64"u,%u)\n", handle, ioflag, offset, size);

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      ret = -ret;
      goto out;
   }

   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestWriteV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_WRITE_V3);

   request->file = handle;
   request->flags = 0;
   request->offset = offset;
   request->requiredSize = size;
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);

   if (ioflag & IO_APPEND) {
      DEBUG(VM_DEBUG_COMM, "writing in append mode.\n");
      request->flags |= HGFS_WRITE_APPEND;
   }

   DEBUG(VM_DEBUG_COMM, "requesting write of %d bytes.\n", size);

   /* Copy the data the user wants to write into the payload. */
   ret = uiomove(request->payload, request->requiredSize, uiop);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL,
            "HgfsDoWrite: uiomove(9F) failed copying data from user.\n");
      ret = -EIO;
      goto destroyOut;
   }

   /* We subtract one so request's 'char payload[1]' member isn't double counted. */
   HgfsKReq_SetPayloadSize(req, reqSize + request->requiredSize - 1);

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /*
       * As in HgfsDoRead(), we need to flip the sign of the error code
       * returned by HgfsSubmitRequest().
       */
      DEBUG(VM_DEBUG_FAIL, "HgfsSubmitRequest failed.\n");
      ret = -ret;
      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);

   ret = HgfsGetStatus(req, sizeof *replyHeader);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      if (ret != EPROTO && ret != EBADF) {
         ret = EACCES;
      }
      ret = -ret;
      goto destroyOut;
   }

   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);

   if (HgfsKReq_GetPayloadSize(req) != repSize) {
      DEBUG(VM_DEBUG_FAIL,
            "Error: invalid size of reply on successful reply.\n");
      ret = -EPROTO;
      goto destroyOut;
   }

   reply = (HgfsReplyWriteV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);

   /* The write was completed successfully, so return the amount written. */
   ret = reply->actualSize;

destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);
out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%u -> %d)\n", handle, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDelete --
 *
 *      Sends a request to delete a file or directory.
 *
 * Results:
 *      Returns 0 on success or an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDelete(HgfsSuperInfo *sip,          // IN: Superinfo
           const char *filename,        // IN: Full name of file to remove
           HgfsOp op)                   // IN: Hgfs operation this delete is for
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestDeleteV3 *request;
   HgfsReplyDeleteV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   uint32 reqBufferSize;
   int ret;

   ASSERT(sip);
   ASSERT(filename);
   ASSERT((op == HGFS_OP_DELETE_FILE_V3) || (op == HGFS_OP_DELETE_DIR_V3));

   DEBUG(VM_DEBUG_ENTRY, "Enter(%s,%d)\n", filename, op);

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      goto out;
   }

   /* Initialize the request's contents. */
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestDeleteV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, op);
   request->hints = 0;
   request->fileName.fid = HGFS_INVALID_HANDLE;
   request->fileName.flags = 0;
   request->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);

   /*
    * Convert an input string to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   ret = HgfsNameToWireEncoding(filename, strlen(filename) + 1,
                                request->fileName.name,
                                reqBufferSize);

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "Could not encode to wire format");
      ret = -ret;
      goto destroyOut;
   }

   request->fileName.length = ret;
   reqSize += ret;

   /* Set the size of our request.  */
   HgfsKReq_SetPayloadSize(req, reqSize);

   DEBUG(VM_DEBUG_COMM, "deleting \"%s\"\n", filename);

   /* Submit our request to guestd. */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* HgfsSubmitRequest() handles destroying the request if necessary. */
      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);

   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);

   ret = HgfsGetStatus(req, repSize);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      goto destroyOut;
   }

destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);
out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%s -> %d)\n", filename, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetNextDirEntry --
 *
 *      Writes the name of the directory entry matching the handle and offset to
 *      nameOut.  Also records the entry's type (file, directory) in type.  This
 *      requires sending a SEARCH_READ request.
 *
 * Results:
 *      Returns zero on success and an error code on error.  The done value is
 *      set if there are no more directory entries.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsGetNextDirEntry(HgfsSuperInfo *sip,         // IN: Superinfo pointer
                    HgfsHandle handle,          // IN: Handle for request
                    uint32_t offset,            // IN: Offset
                    char *nameOut,              // OUT: Location to write name
                    size_t nameSize,            // IN : Size of nameOut
                    HgfsFileType *type,         // OUT: Entry's type
                    Bool *done)                 // OUT: Whether there are any more
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestSearchReadV3 *request;
   HgfsReplySearchReadV3 *reply;
   HgfsDirEntry *dirent;
   uint32 reqSize;
   uint32 repSize;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%u,%u)\n", handle, offset);

   ASSERT(sip);
   ASSERT(nameOut);
   ASSERT(done);

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      DEBUG(VM_DEBUG_FAIL, "couldn't get req.\n");
      goto out;
   }

   /*
    * Fill out the search read request that will return a single directory
    * entry for the provided handle at the given offset.
    */
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestSearchReadV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_SEARCH_READ_V3);

   request->search = handle;
   request->offset = offset;
   request->flags = 0;
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   HgfsKReq_SetPayloadSize(req, reqSize);

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* HgfsSubmitRequest will destroy the request if necessary. */
      DEBUG(VM_DEBUG_FAIL, "HgfsSubmitRequest failed.\n");
      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);

   ret = HgfsGetStatus(req, sizeof *replyHeader);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      goto destroyOut;
   }

   DEBUG(VM_DEBUG_COMM, "received reply for ID %d\n",
         replyHeader->id);
   DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", replyHeader->status);

   reply = (HgfsReplySearchReadV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);
   reply->count = 1;
   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply) + sizeof *dirent;
   dirent = (HgfsDirEntry *)reply->payload;

   /* Make sure we got an entire reply (excluding filename) */
   if (HgfsKReq_GetPayloadSize(req) < repSize) {
      DEBUG(VM_DEBUG_FAIL, "server didn't provide entire reply.\n");
      ret = EFAULT;
      goto destroyOut;
   }

   /* See if there are no more filenames to read */
   if (dirent->fileName.length <= 0) {
      DEBUG(VM_DEBUG_LOG, "no more directory entries.\n");
      *done = TRUE;
      ret = 0;         /* return success */
      goto destroyOut;
   }

   /* Make sure filename isn't too long */
   if ((dirent->fileName.length >= nameSize) ||
       (dirent->fileName.length > HGFS_PAYLOAD_MAX(repSize)) ) {
      DEBUG(VM_DEBUG_FAIL, "filename is too long.\n");
      ret = EOVERFLOW;
      goto destroyOut;
   }

   /*
    * Everything is all right, copy filename to caller's buffer.  Note that even though
    * the hgfs SearchRead reply holds lots of information about the file's attributes,
    * FreeBSD directory entries do not currently need any of that information except the
    * file type.
    */
   memcpy(nameOut, dirent->fileName.name, dirent->fileName.length);
   nameOut[dirent->fileName.length] = '\0';
   *type = dirent->attr.type;
   ret = 0;

destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);
out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%u -> %d)\n", handle, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReadlinkInt --
 *
 *      Reads a symbolic link target.
 *
 * Results:
 *      Either 0 on success or a BSD error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsReadlinkInt(struct vnode *vp,   // IN : File vnode
                struct uio *uiop)   // OUT: Attributes from hgfs server
{
   HgfsKReqHandle req = NULL;
   int ret = 0;
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   HgfsReplyGetattrV3 *reply;
   HgfsReply *replyHeader;
   uint32 outLength;
   char* outBuffer = NULL;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp));

   /* This operation is valid only for symbolic links. */
   if (HGFS_VP_TO_VTYPE(vp) != VLNK) {
      DEBUG(VM_DEBUG_FAIL, "Must be a symbolic link.\n");
      ret = EINVAL;
      goto out;
   }

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      goto out;
   }

   ret = HgfsQueryAttrInt(HGFS_VP_TO_FILENAME(vp), 0, sip, req);
   if (ret != 0) {
      DEBUG(VM_DEBUG_FAIL, "Error %d reading symlink name.\n", ret);
      goto out;
   }

   outLength = HGFS_UIOP_TO_RESID(uiop);
   outBuffer = os_malloc(outLength, M_WAITOK);
   if (outBuffer != NULL) {
      DEBUG(VM_DEBUG_FAIL, "No memory for symlink name.\n");
      ret = ENOMEM;
      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
   reply = (HgfsReplyGetattrV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);
   if (reply->symlinkTarget.name[reply->symlinkTarget.length - 1] == '\0') {
      ret = EINVAL; // Not a well formed name
      goto out;
   }

   ret = HgfsNameFromWireEncoding(reply->symlinkTarget.name,
                                  reply->symlinkTarget.length,
                                  outBuffer, outLength);
   if (ret < 0) {
      ret = -ret;  // HgfsNameFromWireEncoding returns negative error code
      DEBUG(VM_DEBUG_FAIL, "Error converting link wire format length is %d, name is %s\n",
            reply->symlinkTarget.length, reply->symlinkTarget.name);
      goto out;
   }

   ret = uiomove(outBuffer, MIN(ret, outLength), uiop);
   if (ret != 0) {
      DEBUG(VM_DEBUG_FAIL, "Failed %d copying into user buffer.\n", ret);
   }

out:
   if (outBuffer != NULL) {
      os_free(outBuffer, outLength);
   }
   if (req != NULL) {
      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d\n", HGFS_VP_TO_FILENAME_LENGTH(vp), HGFS_VP_TO_FILENAME(vp),
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSymlnikInt --
 *
 *      Creates symbolic link on the host.
 *
 * Results:
 *      Either 0 on success or a BSD error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSymlinkInt(struct vnode *dvp,         // IN : directory vnode
               struct vnode **vpp,        // OUT: pointer to new symlink vnode
               struct componentname *cnp, // IN : pathname to component
               char *targetName)          // IN : Symbolic link target
{
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(dvp);
   HgfsKReqHandle req = NULL;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestSymlinkCreateV3 *request;
   HgfsReplySymlinkCreateV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   uint32 reqBufferSize;
   int ret;
   char *fullName = NULL;
   uint32 fullNameLen;
   HgfsFileNameV3 *fileNameP;
   int nameOffset;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s/%.*s,%s)\n",
                         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
                         (int)cnp->cn_namelen, cnp->cn_nameptr,
                         targetName);

   fullName = os_malloc(MAXPATHLEN, M_WAITOK);
   if (!fullName) {
      ret = ENOMEM;
      goto out;
   }

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      goto out;
   }

   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(dvp),        // Parent directory
                          HGFS_VP_TO_FILENAME_LENGTH(dvp), // Length of name
                          cnp->cn_nameptr,                 // Name of file to create
                          cnp->cn_namelen,                 // Length of filename
                          fullName,                        // Buffer to write full name
                          MAXPATHLEN);                     // Size of this buffer

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "couldn't create full path name.\n");
      ret = ENAMETOOLONG;
      goto out;
   }
   fullNameLen = ret;

   /* Initialize the request's contents. */
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestSymlinkCreateV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_CREATE_SYMLINK_V3);

   request->reserved = 0;

   request->symlinkName.flags = 0;
   request->symlinkName.fid = HGFS_INVALID_HANDLE;
   request->symlinkName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);

   /*
    * Convert an input string to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   ret = HgfsNameToWireEncoding(fullName, fullNameLen + 1,
                                request->symlinkName.name,
                                reqBufferSize);

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL,"Could not encode file name to wire format");
      ret = -ret;
      goto out;
   }
   request->symlinkName.length = ret;
   reqSize += ret;

   fileNameP = (HgfsFileNameV3 *)((char*)&request->symlinkName +
                                  sizeof request->symlinkName +
                                  request->symlinkName.length);
   fileNameP->flags = 0;
   fileNameP->fid = HGFS_INVALID_HANDLE;
   fileNameP->caseType = HGFS_FILE_NAME_CASE_SENSITIVE;

   /*
    * Currently we have different name formats for file names and for symbolic
    * link targets. Flie names are always absolute and on-wire representation does
    * not include leading path separator. HgfsNameToWireEncoding removes
    * leading path separator from the name. However symbolic link targets may be
    * either absolute or relative. To distinguish between them the leading path separator
    * must be preserved for absolute symbolic link target.
    * In the long term we should fix the protocol and have only one name
    * format which is suitable for all names.
    * The following code compensates for this problem before there is such
    * universal name representation.
    */
   if (*targetName == '/') {
      fileNameP->length = 1;
      reqSize += 1;
      *fileNameP->name = '\0';
      targetName++;
   } else {
      fileNameP->length = 0;
   }
   /*
    * Convert symbolic link target to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   nameOffset = fileNameP->name - (char*)requestHeader;
   ret = HgfsNameToWireEncoding(targetName, strlen(targetName) + 1,
                                fileNameP->name + fileNameP->length,
                                HGFS_PACKET_MAX - nameOffset -
                                fileNameP->length);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL,"Could not encode file name to wire format");
      ret = -ret;
      goto out;
   }
   fileNameP->length += ret;

   reqSize += ret;

   /* Set the size of this request. */
   HgfsKReq_SetPayloadSize(req, reqSize);

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* Request is destroyed in HgfsSubmitRequest() if necessary. */
      req = NULL;
      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
   reply = (HgfsReplySymlinkCreateV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);
   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);
   ret = HgfsGetStatus(req, repSize);
   if (ret == 0) {
      ret = HgfsVnodeGet(vpp, dvp, sip, HGFS_VP_TO_MP(dvp), fullName,
                         HGFS_FILE_TYPE_SYMLINK, &sip->fileHashTable, TRUE, 0, 0);
      if (ret) {
         ret = EIO;
      }
   } else {
      DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
   }

   ASSERT(ret != 0 || *vpp != NULL);

out:
   if (req) {
      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }
   if (fullName != NULL) {
      os_free(fullName, MAXPATHLEN);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s/%.*s -> %d)\n",
         HGFS_VP_TO_FILENAME_LENGTH(dvp), HGFS_VP_TO_FILENAME(dvp),
         (int)cnp->cn_namelen, cnp->cn_nameptr,
         ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDoGetattrByName --
 *
 *      Send a name getattr request to the hgfs server and put the result in
 *      hgfsAttr.
 *
 * Results:
 *      Either 0 on success or a BSD error code on failure. The hgfsAttr field
 *      is only filled out on success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDoGetattrByName(const char *path,       // IN : Path to get attributes for
		    HgfsSuperInfo *sip,     // IN : SuperInfo block of hgfs mount.
		    HgfsAttrV2 *hgfsAttrV2) // OUT: Attributes from hgfs server
{
   int ret;
   DEBUG(VM_DEBUG_ENTRY, "Enter(%s)\n", path);
   ret = HgfsDoGetattrInt(path, 0, sip, hgfsAttrV2);
   DEBUG(VM_DEBUG_EXIT, "Exit(%s -> %d)\n", path, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDoGetattrByName --
 *
 *      Send a handle getattr request to the hgfs server and put the result in
 *      hgfsAttr.
 *
 * Results:
 *      Either 0 on success or a BSD error code on failure. The hgfsAttr field
 *      is only filled out on success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

#if 0
static int
HgfsDoGetattrByHandle(HgfsHandle handle,      // IN : Hgfs handle for attr request
		      HgfsSuperInfo *sip,     // IN : SuperInfo block for hgfs mount
		      HgfsAttrV2 *hgfsAttrV2) // OUT: Attributes from hgfs server
{
   int ret;
   DEBUG(VM_DEBUG_ENTRY, "Enter(%u)\n", handle);
   ret =  HgfsDoGetattrInt(NULL, handle, sip, hgfsAttrV2);
   DEBUG(VM_DEBUG_EXIT, "Exit(%u -> %d)\n", handle, ret);
}
#endif

/*
 *----------------------------------------------------------------------------
 *
 * HgfsDoGetattrInt --
 *
 *      Internal function that actually sends a getattr request to the hgfs
 *      server and puts the results in hgfsAttrV2. This function should only
 *      be called by HgfsDoGetattrByName or HgfsDoGetattrByHandle and will do
 *      a getattr by filename if path is non-NULL. Otherwise it does a getattr by
 *      handle.
 *
 *
 * Results:
 *      Either 0 on success or a BSD error code on failure. The hgfsAttr field
 *      is only filled out on success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDoGetattrInt(const char *path,       // IN : Path to get attributes for
		 HgfsHandle handle,      // IN : Handle to get attribues for
		 HgfsSuperInfo *sip,     // IN : SuperInfo block for hgfs mount
		 HgfsAttrV2 *hgfsAttrV2) // OUT: Attributes from hgfs server
{
   HgfsKReqHandle req;
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%s,%u)\n", (path != NULL ? path : "null"), handle);
   ASSERT(hgfsAttrV2);

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      return ret;
   }

   ret = HgfsQueryAttrInt(path, handle, sip, req);
   if (ret == 0) {
      HgfsReplyGetattrV3 *reply;
      HgfsReply *replyHeader;
      replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
      reply = (HgfsReplyGetattrV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);

      /* Fill out hgfsAttrV2 with the results from the server. */
      memcpy(hgfsAttrV2, &reply->attr, sizeof *hgfsAttrV2);
      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%s,%u -> %d)\n", (path != NULL ? path : "null"), handle, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsQueryAttrInt --
 *
 *      Internal function that actually sends a getattr request to the hgfs
 *      server and puts the results in hgfsAttrV2. This function does
 *      a getattr by filename if path is non-NULL. Otherwise it does a getattr by
 *      handle.
 *
 *
 * Results:
 *      Either 0 on success or a BSD error code on failure. When function
 *      succeeds a valid hgfs request is returned and it must be de-allocaed
 *      by the caller.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsQueryAttrInt(const char *path,       // IN : Path to get attributes for
		 HgfsHandle handle,      // IN : Handle to get attribues for
		 HgfsSuperInfo *sip,     // IN : SuperInfo block for hgfs mount
		 HgfsKReqHandle req)     // IN/OUT: preacllocated hgfs request
{
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestGetattrV3 *request;
   HgfsReplyGetattrV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   uint32 reqBufferSize;
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "Enter(%s,%u)\n", (path != NULL ? path : "null"), handle);
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestGetattrV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_GETATTR_V3);
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);

   /*
    * Per the calling conventions of this function, if the path is NULL then
    * this is a Getattr by handle.
    */
   if (path == NULL) {
      request->hints = HGFS_ATTR_HINT_USE_FILE_DESC;
      request->fileName.fid = handle;
      request->fileName.flags = HGFS_FILE_NAME_USE_FILE_DESC;
      request->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
      request->fileName.length = 0;

   } else {
      /* Do a Getattr by path. */
      request->hints = 0;
      request->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      request->fileName.fid = HGFS_INVALID_HANDLE;
      request->fileName.flags = 0;

      /*
       * Convert an input string to utf8 precomposed form, convert it to
       * the cross platform name format and finally unescape any illegal
       * filesystem characters.
       */
      ret = HgfsNameToWireEncoding(path, strlen(path) + 1,
                                   request->fileName.name,
                                   reqBufferSize);

      if (ret < 0) {
         DEBUG(VM_DEBUG_FAIL, "Could not encode to wire format");
         ret = -ret;
         goto destroyOut;
      }
      request->fileName.length = ret;
      reqSize += ret;
   }

   /* Packet size includes the header, request and its payload. */
   HgfsKReq_SetPayloadSize(req, reqSize);

   DEBUG(VM_DEBUG_COMM, "sending getattr request for ID %d\n",
         requestHeader->id);
   DEBUG(VM_DEBUG_COMM, " fileName.length: %d\n", request->fileName.length);
   DEBUG(VM_DEBUG_COMM, " fileName.name: \"%s\"\n", request->fileName.name);

   /*
    * Submit the request and wait for the reply.  HgfsSubmitRequest handles
    * destroying the request on both error and interrupt cases.
    */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* HgfsSubmitRequest destroys the request if necessary */
      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);

   ret = HgfsGetStatus(req, sizeof *replyHeader);
   if (ret) {
      if (ret == EPROTO) {
         DEBUG(VM_DEBUG_FAIL, "Error encountered for ID = %d\n"
               "with status %d.\n", replyHeader->id, replyHeader->status);
      }
      goto destroyOut;
   }

   reply = (HgfsReplyGetattrV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);

   DEBUG(VM_DEBUG_COMM, "received reply for ID %d\n", replyHeader->id);
   DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", replyHeader->status);
   DEBUG(VM_DEBUG_COMM, " file type: %d\n", reply->attr.type);
   DEBUG(VM_DEBUG_COMM, " file size: %llu\n", (long long unsigned)reply->attr.size);
   DEBUG(VM_DEBUG_COMM, " permissions: %o\n", reply->attr.ownerPerms);
   DEBUG(VM_DEBUG_COMM, " permissions: %o\n", reply->attr.groupPerms);
   DEBUG(VM_DEBUG_COMM, " permissions: %o\n", reply->attr.otherPerms);
   DEBUG(VM_DEBUG_COMM, " hostFileId: %llu\n", (long long unsigned)reply->attr.hostFileId);

   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply) + reply->symlinkTarget.length;

   /* The GetAttr succeeded, ensure packet contains correct amount of data. */
   if (HgfsKReq_GetPayloadSize(req) != repSize) {
      DEBUG(VM_DEBUG_COMM, "HgfsLookup: invalid packet size received for \"%s\".\n",
             path);
      ret = EFAULT;
      goto destroyOut;
   }

destroyOut:
   if (ret != 0) {
      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }

out:
   DEBUG(VM_DEBUG_EXIT, "Exit(%s,%u -> %d)\n", (path != NULL ? path : "null"), handle, ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * IsModeCompatible --
 *
 *      Checks if the requested mode is compatible with permissions.
 *
 * Results:
 *       Returns TRUE if the mode is compatible, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
IsModeCompatible(HgfsAccessMode mode,  // IN: Requested open mode
                 uint32 permissions)   // IN: Effective user permissions
{
   if ((permissions & HGFS_PERM_READ) == 0) {
      if ((mode & (HGFS_MODE_GENERIC_READ |
                   HGFS_MODE_READ_DATA |
                   HGFS_MODE_LIST_DIRECTORY |
                   HGFS_MODE_READ_ATTRIBUTES |
                   HGFS_MODE_READ_EXTATTRIBUTES |
                   HGFS_MODE_READ_SECURITY)) != 0) {
         return FALSE;
      }
   }

   if ((permissions & HGFS_PERM_WRITE) == 0) {
      if ((mode & (HGFS_MODE_GENERIC_WRITE |
                   HGFS_MODE_WRITE_DATA |
                   HGFS_MODE_APPEND_DATA |
                   HGFS_MODE_DELETE |
                   HGFS_MODE_ADD_SUBDIRECTORY |
                   HGFS_MODE_DELETE_CHILD |
                   HGFS_MODE_WRITE_ATTRIBUTES |
                   HGFS_MODE_WRITE_EXTATTRIBUTES |
                   HGFS_MODE_WRITE_SECURITY |
                   HGFS_MODE_TAKE_OWNERSHIP |
                   HGFS_MODE_ADD_FILE)) != 0) {
         return FALSE;
      }
   }

   if ((permissions & HGFS_PERM_EXEC) == 0) {
      if ((mode & (HGFS_MODE_GENERIC_EXECUTE |
                   HGFS_MODE_TRAVERSE_DIRECTORY)) != 0) {
         return FALSE;
      }
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAccessInt --
 *
 *      Check to ensure the user has the specified type of access to the file.
 *
 * Results:
 *      Returns 0 if access is allowed and a non-zero error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsAccessInt(struct vnode *vp,     // IN: Vnode to check access for
              HgfsAccessMode mode)  // IN: Access mode requested.
{
   int ret = 0;
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);
   HgfsAttrV2 hgfsAttrV2;

   DEBUG(VM_DEBUG_ENTRY, "HgfsAccessInt(%.*s,%d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp),
         HGFS_VP_TO_FILENAME(vp), mode);

   ret = HgfsDoGetattrByName(HGFS_VP_TO_FILENAME(vp), sip, &hgfsAttrV2);
   if (ret == 0) {
      uint32 effectivePermissions;
      if (hgfsAttrV2.mask & HGFS_ATTR_VALID_EFFECTIVE_PERMS) {
         effectivePermissions = hgfsAttrV2.effectivePerms;
      } else {
         /*
          * If the server did not return actual effective permissions then
          * need to calculate ourselves. However we should avoid unnecessary denial of
          * access so perform optimistic permissions calculation.
          * It is safe since host enforces necessary restrictions regardless of
          * the client's decisions.
          */
         effectivePermissions =
            hgfsAttrV2.ownerPerms | hgfsAttrV2.groupPerms | hgfsAttrV2.otherPerms;
      }
      if (!IsModeCompatible(mode, effectivePermissions)) {
         ret = EACCES;
         DEBUG(VM_DEBUG_FAIL, "HgfsAccessInt denied access: %s (%d, %d)\n",
               HGFS_VP_TO_FILENAME(vp), mode, effectivePermissions);
      }
   } else {
      DEBUG(VM_DEBUG_FAIL, "HgfsAccessInt failed getting attrib: %s (%d)\n",
            HGFS_VP_TO_FILENAME(vp), ret);
   }

   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp),
         HGFS_VP_TO_FILENAME(vp), ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMmapInt --
 *
 *      HgfsMmapInt is invoked invoked from HgfsVnopMmap to verify parameters
 *      and mark vnode as mmapped if necessary.
 *
 * Results:
 *      Zero on success or non-zero error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsMmapInt(struct vnode *vp,
            int accessMode)
{
   int ret;
   HgfsFile *fp;

   ASSERT(vp);
   fp = HGFS_VP_TO_FP(vp);

   ASSERT(fp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s,%d)\n", HGFS_VP_TO_FILENAME_LENGTH(vp),
         HGFS_VP_TO_FILENAME(vp), accessMode);

   /*
    *  If the directory is already opened then we are done.
    *  There is no different open modes for directories thus the handle is compatible.
    */
   os_rw_lock_lock_exclusive(fp->handleLock);

   ret = HgfsCheckAndReferenceHandle(vp, accessMode, OPENREQ_MMAP);
   os_rw_lock_unlock_exclusive(fp->handleLock);
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d).\n", HGFS_VP_TO_FILENAME_LENGTH(vp),
         HGFS_VP_TO_FILENAME(vp), ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMnomapInt --
 *
 *      HgfsMnomapInt is invoked invoked from HgfsVnopNomap to tear down memory
 *      mapping and dereference file handle.
 *
 * Results:
 *      Zero on success or non-zero error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsMnomapInt(struct vnode *vp)
{
   int ret = 0;
   HgfsHandle handleToClose;
   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);

   ASSERT(vp);

   DEBUG(VM_DEBUG_ENTRY, "Enter(%.*s)\n", HGFS_VP_TO_FILENAME_LENGTH(vp),
         HGFS_VP_TO_FILENAME(vp));

   /*
    * Check to see if we should close the file handle on the host, which happen when
    * the reference count of the current handle become 0.
    */
   if (HgfsReleaseOpenFileHandle(vp, OPENREQ_MMAP, &handleToClose) == 0) {
      ret = HgfsCloseServerFileHandle(sip, handleToClose);
   }
   DEBUG(VM_DEBUG_EXIT, "Exit(%.*s -> %d).\n", HGFS_VP_TO_FILENAME_LENGTH(vp),
         HGFS_VP_TO_FILENAME(vp), ret);
   return ret;
}
