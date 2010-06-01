/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * stubs.h --
 *
 */


#ifndef __STUBS_H__
#define __STUBS_H__

#if defined(linux) && !defined(vmblock_fuse)
# include "driver-config.h"
# include "compat_version.h"
#endif

void Panic(const char *fmt, ...);

#endif /* __STUBS_H__ */
