/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * module.h --
 *
 *      Defnitions for entire vmblock module.
 */

#ifndef __MODULE_H_
#define __MODULE_H_

#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>
#include <sys/param.h>

#if SOL11
#include <sys/vfs_opreg.h>      /* fs_operation_def_t, ... */
#endif

#include "vm_basic_types.h"

/*
 * Macros
 */
#define VMBLOCK_FS_NAME     "vmblock"
#define VMBLOCK_VFS_FLAGS   0
#define VMBLOCK_VFSSW_FLAGS 0
#define VFSPTOMIP(vfsp)     ((VMBlockMountInfo *)(vfsp)->vfs_data)
#define VPTOMIP(vp)         VFSPTOMIP((vp)->v_vfsp)
#define VPTOVIP(vp)         ((VMBlockVnodeInfo *)(vp)->v_data)

/*
 * Debug logging
 */
#define VMBLOCK_DEBUG CE_WARN
#define VMBLOCK_ERROR CE_WARN
#define VMBLOCK_ENTRY_LOGLEVEL 7
#undef ASSERT
#ifdef VMX86_DEVEL
# define Debug(level, fmt, args...)    level > LOGLEVEL ?                       \
                                          0 :                                   \
                                          cmn_err(VMBLOCK_DEBUG, fmt, ##args)
# define LOG(level, fmt, args...)      Debug(level, fmt, ##args)
# define ASSERT(expr)                  (expr) ?                                 \
                                          0 :                                   \
                                          cmn_err(CE_PANIC, "ASSERT: %s:%d\n",  \
                                                  __FILE__, __LINE__)
#else
# define Debug(level, fmt, args...)
# define LOG(level, fmt, args...)
# define ASSERT(expr)
#endif
#define Warning(fmt, args...)  cmn_err(VMBLOCK_ERROR, fmt, ##args)

#if defined(SOL9)
# define OS_VFS_VERSION    2
#elif defined(SOL10)
# define OS_VFS_VERSION    3
#elif defined(SOL11)
# define OS_VFS_VERSION    5
#else
# error "Unknown Solaris version, can't set OS_VFS_VERSION"
#endif



#if OS_VFS_VERSION <= 3
# define VMBLOCK_VOP(vopName, vopFn, vmblkFn) { vopName, vmblkFn }
#else
# define VMBLOCK_VOP(vopName, vopFn, vmblkFn) { vopName, { .vopFn = vmblkFn } }
#endif

/*
 * Types
 */
typedef struct VMBlockMountInfo {
   struct vnode *root;
   struct vnode *redirectVnode;
   struct pathname redirectPath;
} VMBlockMountInfo;

typedef struct VMBlockVnodeInfo {
   struct vnode *realVnode;
   char name[MAXNAMELEN];
   size_t nameLen;
} VMBlockVnodeInfo;


/*
 * Externs
 */

/* Filesystem initialization routine (see vnops.c) */
EXTERN int VMBlockInit(int, char *);

/* Needed in struct modlfs */
EXTERN struct mod_ops mod_fsops;

EXTERN const fs_operation_def_t vnodeOpsArr[];
EXTERN vnodeops_t *vmblockVnodeOps;
EXTERN int vmblockType;

EXTERN int LOGLEVEL;

#endif /* __MODULE_H_ */
