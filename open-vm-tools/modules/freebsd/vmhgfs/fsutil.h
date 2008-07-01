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
#define HGFS_INIT_REQUEST_HDR(request, req, _op)                \
         do {                                                   \
            request->header.id = HgfsKReq_GetId(req);           \
            request->header.op = _op;                           \
         } while(0)

/* Determine if this is the root vnode. */
#define HGFS_IS_ROOT_VNODE(sip, vp)                             \
         (sip->rootVnode == vp)

#define DIRSEPC '/'
#define DIRSEPS "/"
#define DIRSEPSLEN 1

/*
 *  Types
 */

/*
 * Hgfs permissions are similar to Unix permissions in that they both include
 * bits for read vs. write vs. execute permissions.  However, Hgfs is only
 * concerned with file owners, meaning no "group" or "other" bits, so we need to
 * translate between Hgfs and Unix permissions with a simple bitshift.  The
 * shift value corresponds to omitting the "group" and "other" bits.
 */
#define HGFS_ATTR_MODE_SHIFT    6

/* Solaris times support nsecs, so only use these functions directly */
#define HGFS_SET_TIME(unixtm, nttime)                   \
   HgfsConvertFromNtTimeNsec(&unixtm, nttime)
#define HGFS_GET_TIME(unixtm)                           \
   HgfsConvertTimeSpecToNtTime(&unixtm)

/* Utility functions */

int HgfsSubmitRequest(HgfsSuperInfo *sip, HgfsKReqHandle req);
int HgfsValidateReply(HgfsKReqHandle req, uint32_t minSize);
int HgfsEscapeBuffer(char const *bufIn, uint32 sizeIn,
                            uint32 sizeBufOut, char *bufOut);
int HgfsUnescapeBuffer(char *bufIn, uint32 sizeIn);
int HgfsGetOpenMode(uint32 flags);
int HgfsGetOpenFlags(uint32 flags);
int HgfsMakeFullName(const char *path, uint32_t pathLen, const char *file,
		     size_t fileLen, char *outBuf, ssize_t bufSize);
void HgfsAttrToBSD(struct vnode *vp, const HgfsAttrV2 *hgfsAttrV2, HGFS_VNODE_ATTR *vap);
Bool HgfsSetattrCopy(HGFS_VNODE_ATTR *vap, HgfsAttr *hgfsAttr, HgfsAttrChanges *update);
int HgfsStatusToBSD(HgfsStatus hgfsStatus);
Bool HgfsAttemptToCreateShare(const char *path, int flag);
int HgfsNameFromWireEncoding(const char *bufIn, uint32 bufInSize, char *bufOut, uint32 bufOutSize);
int HgfsNameToWireEncoding(const char *bufIn, uint32 bufInSize, char *bufOut, uint32 bufOutSize);


#endif // _HGFS_FSUTIL_H_
