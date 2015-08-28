/*********************************************************
 * Copyright (C) 2013 VMware, Inc. All rights reserved.
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
 * file.h --
 *
 * High-level filesystem operations for the filesystem portion of
 * the vmhgfs driver.
 */

#ifndef _HGFS_DRIVER_FILE_H_
#define _HGFS_DRIVER_FILE_H_

#define HGFS_FILE_OPEN_PERMS (HGFS_OPEN_VALID_SPECIAL_PERMS | \
			      HGFS_OPEN_VALID_OWNER_PERMS | \
			      HGFS_OPEN_VALID_GROUP_PERMS | \
			      HGFS_OPEN_VALID_OTHER_PERMS)

#define HGFS_FILE_OPEN_MASK (HGFS_OPEN_VALID_MODE | \
			     HGFS_OPEN_VALID_FLAGS | \
			     HGFS_OPEN_VALID_FILE_NAME | \
			     HGFS_OPEN_VALID_SERVER_LOCK)

#define HGFS_FILE_CREATE_MASK (HGFS_FILE_OPEN_MASK | \
			       HGFS_FILE_OPEN_PERMS)

/* Public functions (with respect to the entire module). */
int HgfsRelease(HgfsHandle handle);

#endif // _HGFS_DRIVER_FILE_H_
