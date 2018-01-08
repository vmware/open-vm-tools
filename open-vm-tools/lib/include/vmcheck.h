/*********************************************************
 * Copyright (C) 2006-2017 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


/*
 * vmcheck.h --
 *
 *    Utility functions for discovering our virtualization status.
 */


#ifndef __VMCHECK_H__
#   define __VMCHECK_H__

#include "vm_basic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

Bool
VmCheck_GetVersion(uint32 *version, // OUT
                   uint32 *type);   // OUT

Bool
VmCheck_IsVirtualWorld(void);

#ifdef __cplusplus
}
#endif

#endif /* __VMCHECK_H__ */
