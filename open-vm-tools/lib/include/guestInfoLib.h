/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

#ifndef _GUESTINFOLIB_H_
#define _GUESTINFOLIB_H_

/**
 * @file guestInfoLib.h
 *
 * Declarations of functions implemented in the guestInfo library.
 */

#include "vm_basic_types.h"

uint64
GuestInfo_GetAvailableDiskSpace(char *pathName);

#endif /* _GUESTINFOLIB_H_ */

