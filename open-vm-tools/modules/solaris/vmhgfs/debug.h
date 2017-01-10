/*********************************************************
 * Copyright (C) 2004-2016 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/


/*
 * debug.h --
 *
 * Macros and includes for debugging.
 *
 */

#ifndef __DEBUG_H_
#define __DEBUG_H_

#ifdef _KERNEL

#include <sys/vnode.h>
#include <sys/vfs.h>

#include "hgfsSolaris.h"
#include "filesystem.h"

#endif

/*
 * Debugging
 */
#define HGFS_DEBUG	        (CE_NOTE)

#define VM_DEBUG_ALWAYS         (1)
#define VM_DEBUG_FAIL	        VM_DEBUG_ALWAYS
#define VM_DEBUG_NOTSUP         VM_DEBUG_ALWAYS
#define VM_DEBUG_ENTRY          (1 << 1)
#define VM_DEBUG_DONE	        (1 << 2)
#define VM_DEBUG_LOAD	        (1 << 3)
#define VM_DEBUG_INFO           (1 << 4)
#define VM_DEBUG_STRUCT         (1 << 5)
#define VM_DEBUG_LIST           (1 << 6)
#define VM_DEBUG_CHPOLL         (1 << 7)
#define VM_DEBUG_RARE           (1 << 8)
#define VM_DEBUG_COMM           (1 << 9)
#define VM_DEBUG_REQUEST        (1 << 10)
#define VM_DEBUG_LOG            (1 << 11)
#define VM_DEBUG_ATTR           (1 << 12)
#define VM_DEBUG_DEVENTRY       (1 << 13)
#define VM_DEBUG_DEVDONE        (1 << 14)
#define VM_DEBUG_SIG            (1 << 15)
#define VM_DEBUG_ERROR          (1 << 16)
#define VM_DEBUG_HSHTBL         (1 << 17)
#define VM_DEBUG_HANDLE         (1 << 18)
#define VM_DEBUG_STATE          (1 << 19)

#ifdef VM_DEBUGGING_ON
/*#define VM_DEBUG_LEV    (VM_DEBUG_ALWAYS | VM_DEBUG_ENTRY | VM_DEBUG_DONE |     \
                         VM_DEBUG_LOAD | VM_DEBUG_COMM |                        \
                         VM_DEBUG_LOG | VM_DEBUG_ATTR)
*/
/*#define VM_DEBUG_LEV    (VM_DEBUG_ALWAYS | VM_DEBUG_FAIL | VM_DEBUG_ERROR |	\
			 VM_DEBUG_COMM | VM_DEBUG_DONE)
*/
#define VM_DEBUG_LEV    (VM_DEBUG_ALWAYS | VM_DEBUG_FAIL)
#endif

#ifdef VM_DEBUG_LEV
#define DEBUG(type, args...)    \
             ((type & VM_DEBUG_LEV) ? (cmn_err(HGFS_DEBUG, args)) : 0)
#else
#define DEBUG(type, args...)
#endif


/*
 * Prototypes
 */

#ifdef _KERNEL

INLINE void HgfsDebugPrintVfssw(char *str, struct vfssw *vfsswp);
INLINE void HgfsDebugPrintVfs(char *str, struct vfs *vfsp);
INLINE void HgfsDebugPrintVnode(uint32 level, char *str,
                                struct vnode *vnodep, Bool printFileName);
INLINE void HgfsDebugPrintCred(char *str, struct cred *credp);
INLINE void HgfsDebugPrintMounta(char *str, struct mounta *mntp);
INLINE void HgfsDebugPrintVattr(const struct vattr *vap);
void HgfsDebugPrintReqList(DblLnkLst_Links *listAnchor);
void HgfsDebugPrintReq(const char *str, HgfsReq *req);
void HgfsDebugPrintReqPool(const char *str);

#endif

void Log(const char *fmt, ...);
void Debug(const char *fmt, ...);


#endif /* __DEBUG_H_ */
