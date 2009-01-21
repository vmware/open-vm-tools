/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 *      EXTERNs needed by structures in module.c
 */

#ifndef __MODULE_H_
#define __MODULE_H_

#include "filesystem.h"

/*
 * Device driver EXTERNs
 */

/* Functions for the cb_ops structure (see driver.c) */
EXTERN int HgfsDevOpen(dev_t *devp, int flag, int otyp, cred_t *credp);
EXTERN int HgfsDevClose(dev_t dev, int flag, int otyp, cred_t *credp);
EXTERN int HgfsDevRead(dev_t dev, uio_t *uiop, cred_t *credp);
EXTERN int HgfsDevWrite(dev_t dev, uio_t *uiop, cred_t *credp);
EXTERN int HgfsDevChpoll(dev_t dev, short int events, int anyyet,
                         short int *reventsp, struct pollhead **phpp);

/* Functions for the dev_ops structure (see driver.c) */
EXTERN int HgfsDevGetinfo(dev_info_t *dip, ddi_info_cmd_t infocmd,
                          void *arg, void **result);
EXTERN int HgfsDevAttach(dev_info_t *dip, ddi_attach_cmd_t cmd);
EXTERN int HgfsDevDetach(dev_info_t *dip, ddi_detach_cmd_t cmd);

/* Struct needed in struct modldrv */
EXTERN struct mod_ops mod_driverops;


/*
 * Filesystem EXTERNs
 */

/* Filesystem initialization routine (see filesystem.c) */
#if HGFS_VFS_VERSION == 2
EXTERN int HgfsInit(struct vfssw *vfsswp, int fstype);
#else
EXTERN int HgfsInit(int, char *);
#endif

/* Functions for the vfsops structure (see filesystem.c) */
EXTERN int HgfsMount(struct vfs *vfsp, struct vnode *vnodep,
                 struct mounta *mntp, struct cred *credp);
EXTERN int HgfsUnmount(struct vfs *vfsp, int mflag, struct cred *credp);
EXTERN int HgfsRoot(struct vfs *vfsp, struct vnode **vnodepp);
EXTERN int HgfsStatvfs(struct vfs *vfsp, struct statvfs64 *stats);
EXTERN int HgfsSync(struct vfs* vfsp, short flags, struct cred *credp);
EXTERN int HgfsVget(struct vfs *vfsp, struct vnode **vnodepp, struct fid *fidp);
EXTERN int HgfsMountroot(struct vfs *vfsp, enum whymountroot reason);
EXTERN int HgfsReserved(struct vfs *vfsp, struct vnode **vnodepp, char * /* ?? */);
EXTERN void HgfsFreevfs(struct vfs *vfsp);

/* Struct needed in struct modlfs */
EXTERN struct mod_ops mod_fsops;


#endif /* __MODULE_H_ */
