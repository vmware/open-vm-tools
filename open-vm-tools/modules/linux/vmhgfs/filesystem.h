/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * filesystem.h --
 *
 * High-level filesystem operations for the filesystem portion of 
 * the vmhgfs driver.
 */

#ifndef _HGFS_DRIVER_FILESYSTEM_H_
#define _HGFS_DRIVER_FILESYSTEM_H_

#include "vm_basic_types.h"

/* Public functions (with respect to the entire module). */
Bool HgfsInitFileSystem(void);
Bool HgfsCleanupFileSystem(void);

#endif // _HGFS_DRIVER_FILESYSTEM_H_
