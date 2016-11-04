/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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
 * nicInfo.h --
 *
 *    Functions used to communicate guest networking information to the host.
 *
 */

#ifndef NICINFO_H
#define NICINFO_H

#include <glib.h>

#include "guestInfo.h"

Bool GuestInfo_GetFqdn(int outBufLen, char fqdn[]);
Bool GuestInfo_GetNicInfo(NicInfoV3 **nicInfo);
void GuestInfo_FreeNicInfo(NicInfoV3 *nicInfo);
char *GuestInfo_GetPrimaryIP(void);

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

#endif
