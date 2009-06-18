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
 * fsutil.h --
 *
 * VFS helper functions that are shared between the FreeBSD and Mac OS
 * implementations of HGFS.
 */

#ifndef _HGFS_FSUTIL_H_
#define _HGFS_FSUTIL_H_

#include <sys/param.h>          // for everything
#include <sys/vnode.h>          // for struct vnode
#include <sys/mount.h>          // for struct mount
#include <sys/namei.h>          // for name lookup goodness
#include <sys/fcntl.h>          // for in-kernel file access flags (FREAD, etc)
#include <sys/stat.h>           // for file flag bitmasks (S_IRWXU, etc)
#include <sys/uio.h>            // for uiomove

#include "debug.h"
#include "hgfsUtil.h"
#include "hgfs_kernel.h"
#include "request.h"

/*
 * Macros
 */

/* Sets the values of request headers properly */
#define HGFS_INIT_REQUEST_HDR(header, req, _op)                \
         do {                                                   \
            header->id = HgfsKReq_GetId(req);           \
            header->op = _op;                           \
         } while(0)

/* Determine if this is the root vnode. */
#define HGFS_IS_ROOT_VNODE(sip, vp)                             \
         (sip->rootVnode == vp)

#define DIRSEPC '/'
#define DIRSEPS "/"
#define DIRSEPSLEN 1

#define HGFS_VA_MODE va_mode
#define HGFS_VA_UID va_uid
#define HGFS_VA_GID va_gid
#define HGFS_VA_TYPE va_type
#define HGFS_VA_NLINK va_nlink
#define HGFS_VA_FSID va_fsid
#define HGFS_VA_FILEID va_fileid

#define HGFS_VATTR_TYPE_RETURN(vap, val)                         \
        VATTR_RETURN(vap, va_type, val)

#define HGFS_VATTR_MODE_RETURN(vap, val)                         \
        VATTR_RETURN(vap, va_mode, val)

#define HGFS_VATTR_NLINK_RETURN(vap, val)                         \
        VATTR_RETURN(vap, va_nlink, val)

#define HGFS_VATTR_UID_RETURN(vap, val)                         \
        VATTR_RETURN(vap, va_uid, val)

#define HGFS_VATTR_GID_RETURN(vap, val)                         \
        VATTR_RETURN(vap, va_gid, val)

#define HGFS_VATTR_FSID_RETURN(vap, val)                         \
        VATTR_RETURN(vap, va_fsid, val)

#define HGFS_VATTR_FILEID_RETURN(vap, val)                         \
        VATTR_RETURN(vap, va_fileid, val)

#if defined __APPLE__
   #define HGFS_VA_DATA_SIZE va_data_size
   #define HGFS_VA_DATA_BYTES va_data_size
   #define HGFS_VA_ACCESS_TIME_SEC va_access_time
   #define HGFS_VA_ACCESS_TIME va_access_time
   #define HGFS_VA_MODIFY_TIME_SEC va_modify_time
   #define HGFS_VA_MODIFY_TIME va_modify_time
   #define HGFS_VA_CREATE_TIME_SEC va_create_time
   #define HGFS_VA_CREATE_TIME va_create_time
   #define HGFS_VA_CHANGE_TIME_SEC va_change_time
   #define HGFS_VA_CHANGE_TIME va_change_time
   #define HGFS_VA_BLOCK_SIZE va_iosize
   #define HGFS_VATTR_IS_ACTIVE(vap, attr)                      \
           VATTR_IS_ACTIVE(vap, attr)
   #define HGFS_VATTR_MODE_IS_ACTIVE(vap, mode)                 \
           VATTR_IS_ACTIVE(vap, mode)
   #define HGFS_VATTR_SIZE_IS_ACTIVE(vap, size)                 \
           VATTR_IS_ACTIVE(vap, size)
   #define HGFS_VATTR_BLOCKSIZE_RETURN(vap, val)                \
           VATTR_RETURN(vap, va_iosize, val)
   #define HGFS_VATTR_BYTES_RETURN(vap, val)
   #define HGFS_VATTR_SIZE_RETURN(vap, val)                     \
           VATTR_RETURN(vap, va_data_size, val)
   #define HGFS_VATTR_ACCESS_TIME_SET_SUPPORTED(vap)            \
           VATTR_SET_SUPPORTED(vap, va_access_time)
   #define HGFS_VATTR_MODIFY_TIME_SET_SUPPORTED(vap)            \
           VATTR_SET_SUPPORTED(vap, va_modify_time)
   #define HGFS_VATTR_CREATE_TIME_SET_SUPPORTED(vap)            \
           VATTR_SET_SUPPORTED(vap, va_create_time)
   #define HGFS_VATTR_CHANGE_TIME_SET_SUPPORTED(vap)            \
           VATTR_SET_SUPPORTED(vap, va_change_time)

#elif defined __FreeBSD__
   #define HGFS_VA_DATA_SIZE va_size
   #define HGFS_VA_DATA_BYTES va_bytes
   #define HGFS_VA_ACCESS_TIME_SEC va_atime.tv_sec
   #define HGFS_VA_ACCESS_TIME va_atime
   #define HGFS_VA_MODIFY_TIME_SEC va_mtime.tv_sec
   #define HGFS_VA_MODIFY_TIME va_mtime
   #define HGFS_VA_CHANGE_TIME_SEC va_ctime.tv_sec
   #define HGFS_VA_CHANGE_TIME va_ctime
   #define HGFS_VA_CREATE_TIME_SEC va_birthtime.tv_sec
   #define HGFS_VA_CREATE_TIME va_birthtime
   #define HGFS_VA_BLOCK_SIZE va_blocksize
   #define HGFS_VATTR_IS_ACTIVE(vap, attr)                      \
           (vap->attr != VNOVAL)
   #define HGFS_VATTR_MODE_IS_ACTIVE(vap, mode)                 \
           (vap->mode != (mode_t)VNOVAL)
   #define HGFS_VATTR_SIZE_IS_ACTIVE(vap, size)                 \
           (vap->size != (u_quad_t)VNOVAL)
   #define VATTR_SET_SUPPORTED(vap, time) 
   #define HGFS_VATTR_BLOCKSIZE_RETURN(vap, val)                \
           VATTR_RETURN(vap, va_blocksize, val)
   #define HGFS_VATTR_BYTES_RETURN(vap, val)                    \
           VATTR_RETURN(vap, va_bytes, val)
   #define HGFS_VATTR_SIZE_RETURN(vap, val)                     \
           VATTR_RETURN(vap, va_size, val)

   /* NULL macros */
   #define HGFS_VATTR_ACCESS_TIME_SET_SUPPORTED(vap)
   #define HGFS_VATTR_MODIFY_TIME_SET_SUPPORTED(vap)
   #define HGFS_VATTR_CREATE_TIME_SET_SUPPORTED(vap)
   #define HGFS_VATTR_CHANGE_TIME_SET_SUPPORTED(vap)
#endif

/*
 *  Types
 */

/*
 * Hgfs permissions are similar to Unix permissions in that they both include
 * bits for read vs. write vs. execute permissions. Since permissions of
 * owner, groups and others are passed as individual components by Hgfs, 
 * we need simple bit shift operations for translation between Hgfs and Unix
 * permissions.
 */

#define HGFS_ATTR_SPECIAL_PERM_SHIFT 9
#define HGFS_ATTR_OWNER_PERM_SHIFT 6
#define HGFS_ATTR_GROUP_PERM_SHIFT 3

/* Solaris times support nsecs, so only use these functions directly */
#define HGFS_SET_TIME(unixtm, nttime)                   \
   HgfsConvertFromNtTimeNsec(&unixtm, nttime)
#define HGFS_GET_TIME(unixtm)                           \
   HgfsConvertTimeSpecToNtTime(&unixtm)

/* Utility functions */

int HgfsSubmitRequest(HgfsSuperInfo *sip, HgfsKReqHandle req);
int HgfsGetStatus(HgfsKReqHandle req, uint32_t minSize);
int HgfsEscapeBuffer(char const *bufIn, uint32 sizeIn,
                            uint32 sizeBufOut, char *bufOut);
int HgfsUnescapeBuffer(char *bufIn, uint32 sizeIn);
int HgfsGetOpenMode(uint32 flags);
int HgfsGetOpenFlags(uint32 flags);
int HgfsMakeFullName(const char *path, uint32_t pathLen, const char *file,
		     size_t fileLen, char *outBuf, ssize_t bufSize);
void HgfsAttrToBSD(struct vnode *vp, const HgfsAttrV2 *hgfsAttrV2, HgfsVnodeAttr *vap);
Bool HgfsSetattrCopy(HgfsVnodeAttr *vap, HgfsAttrV2 *hgfsAttrV2, HgfsAttrHint *hints);
int HgfsStatusToBSD(HgfsStatus hgfsStatus);
Bool HgfsAttemptToCreateShare(const char *path, int flag);
int HgfsNameFromWireEncoding(const char *bufIn, uint32 bufInSize, char *bufOut, uint32 bufOutSize);
int HgfsNameToWireEncoding(const char *bufIn, uint32 bufInSize, char *bufOut, uint32 bufOutSize);


#endif // _HGFS_FSUTIL_H_
