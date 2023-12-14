/*********************************************************
 * Copyright (c) 2014-2021 VMware, Inc. All rights reserved.
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
 * nicInfoInt.h --
 *
 *    Functions used to communicate guest network information to the host.
 *
 */

#ifndef NICINFO_INT_H
#define NICINFO_INT_H

#include "nicInfo.h"

#if defined __FreeBSD__ || defined __sun__ || defined __APPLE__
#   include <sys/socket.h>      // struct sockaddr
#endif

Bool GuestInfoGetFqdn(int outBufLen, char fqdn[]);
Bool GuestInfoGetNicInfo(unsigned int maxIPv4Routes,   // IN
                         unsigned int maxIPv6Routes,   // IN
                         NicInfoV3 *nicInfo,           // OUT
                         Bool *maxNicsError);          // OUT

GuestNicV3 *GuestInfoAddNicEntry(NicInfoV3 *nicInfo,                    // IN/OUT
                                 const char macAddress[NICINFO_MAC_LEN], // IN
                                 DnsConfigInfo *dnsInfo,                // IN
                                 WinsConfigInfo *winsInfo,              // IN
                                 Bool *maxNicsError);                   // OUT

IpAddressEntry *GuestInfoAddIpAddress(GuestNicV3 *nic,                  // IN/OUT
                                      const struct sockaddr *sockAddr,  // IN
                                      InetAddressPrefixLength pfxLen,   // IN
                                      const IpAddressOrigin *origin,    // IN
                                      const IpAddressStatus *status);   // IN

char *GuestInfoGetPrimaryIP(void);

#if defined _WIN32
void GuestInfoDupTypedIpAddress(TypedIpAddress *srcIp,   // IN
                                TypedIpAddress *destIp);  // OUT
#endif // if defined _WIN32

#if defined __linux__ || defined _WIN32
Bool GuestInfoGetNicInfoIfIndex(NicInfoV3 *nicInfo,  // IN
                                int ifIndex,         // IN
                                int *nicIfIndex);    // OUT
#endif // if defined __linux__ || defined _WIN32
void GuestInfoSockaddrToTypedIpAddress(const struct sockaddr *sa,    // IN
                                       TypedIpAddress *typedIp);     // OUT

GuestNicV3 *
GuestInfoUtilFindNicByMac(const NicInfoV3 *nicInfo,
                          const char *macAddress);
#endif
