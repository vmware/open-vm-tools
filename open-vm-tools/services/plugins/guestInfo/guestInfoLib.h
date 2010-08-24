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

Bool
GuestInfo_GetFqdn(int outBufLen,
                  char fqdn[]);

Bool
GuestInfo_GetNicInfo(NicInfoV3 **nicInfo);

void
GuestInfo_FreeNicInfo(NicInfoV3 *nicInfo);

GuestDiskInfo *
GuestInfo_GetDiskInfo(void);

void
GuestInfo_FreeDiskInfo(GuestDiskInfo *di);

Bool
GuestInfo_GetOSName(unsigned int outBufFullLen,
                    unsigned int outBufLen,
                    char *osNameFull,
                    char *osName);

/*
 * Comparison routines -- handy for caching, unit testing.
 */

Bool
GuestInfo_IsEqual_DhcpConfigInfo(const DhcpConfigInfo *a,
                                 const DhcpConfigInfo *b);

Bool
GuestInfo_IsEqual_DnsConfigInfo(const DnsConfigInfo *a,
                                const DnsConfigInfo *b);

Bool
GuestInfo_IsEqual_DnsHostname(const DnsHostname *a,
                              const DnsHostname *b);

Bool
GuestInfo_IsEqual_InetCidrRouteEntry(const InetCidrRouteEntry *a,
                                     const InetCidrRouteEntry *b,
                                     const NicInfoV3 *aInfo,
                                     const NicInfoV3 *bInfo);

Bool
GuestInfo_IsEqual_IpAddressEntry(const IpAddressEntry *a,
                                 const IpAddressEntry *b);

Bool
GuestInfo_IsEqual_NicInfoV3(const NicInfoV3 *a,
                            const NicInfoV3 *b);

Bool
GuestInfo_IsEqual_TypedIpAddress(const TypedIpAddress *a,
                                 const TypedIpAddress *b);

Bool
GuestInfo_IsEqual_WinsConfigInfo(const WinsConfigInfo *a,
                                 const WinsConfigInfo *b);

/*
 * Misc utilities.
 */

GuestNicV3 *
GuestInfo_Util_FindNicByMac(const NicInfoV3 *nicInfo,
                            const char *macAddress);

#endif /* _GUESTINFOLIB_H_ */

