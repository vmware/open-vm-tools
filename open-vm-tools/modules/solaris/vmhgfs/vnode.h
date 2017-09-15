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
 * vnode.h --
 *
 * Functions exported by vnode.c
 */

#ifndef __VNODE_H_
#define __VNODE_H_

#include "hgfsSolaris.h"

int HgfsSetVnodeOps(struct vnode *vp);
inline HgfsSuperInfo *HgfsGetSuperInfo(void);
void HgfsInitSuperInfo(struct vfs *vfsp);
void HgfsClearSuperInfo(void);
int HgfsMakeVnodeOps(void);
void HgfsFreeVnodeOps(void);

#endif
