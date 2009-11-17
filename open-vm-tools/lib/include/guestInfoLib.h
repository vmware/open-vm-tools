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
#include "guestInfo.h"

uint64
GuestInfo_GetAvailableDiskSpace(char *pathName);

Bool
GuestInfo_GetFqdn(int outBufLen,
                  char fqdn[]);

Bool
GuestInfo_GetNicInfo(NicInfoV3 **nicInfo);

void
GuestInfo_FreeNicInfo(NicInfoV3 *nicInfo);

Bool
GuestInfo_GetDiskInfo(PGuestDiskInfo di);

Bool
GuestInfo_GetOSName(unsigned int outBufFullLen,
                    unsigned int outBufLen,
                    char *osNameFull,
                    char *osName);

int
GuestInfo_GetSystemBitness(void);

#endif /* _GUESTINFOLIB_H_ */

