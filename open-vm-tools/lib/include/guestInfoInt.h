/*********************************************************
 * Copyright (C) 2001 VMware, Inc. All rights reserved.
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
 * guestInfoInt.h --
 *
 *    Functions used to communicate guest information to the host.
 *
 */

#ifndef GUEST_INFO_INT_H
#define GUEST_INFO_INT_H


#include "guestInfo.h"

Bool GuestInfoGetFqdn(int outBufLen, char fqdn[]);
Bool GuestInfoGetNicInfo(GuestNicInfo *nicInfo);
void GuestInfoMemset(void * mem, int value, unsigned int size);
Bool GuestInfoGetDiskInfo(PGuestDiskInfo di);
Bool GuestInfoGetOSName(unsigned int outBufFullLen, unsigned int outBufLen,
                        char *osNameFull, char *osName);
int GuestInfo_GetSystemBitness(void);
Bool GuestInfo_PerfMon(struct GuestMemInfo *vmStats);

NicEntry *GuestInfoAddNicEntry(GuestNicInfo *nicInfo,
                               const char macAddress[MAC_ADDR_SIZE]);
VmIpAddressEntry *GuestInfoAddIpAddress(NicEntry *nicEntry,
                                        const char *ipAddr,
                                        const uint32 af_type);
void GuestInfoAddSubnetMask(VmIpAddressEntry *ipAddressEntry,
                            const uint32 subnetMaskBits);

NicEntry *GuestInfoFindMacAddress(GuestNicInfo *nicInfo, const char *macAddress);


#endif
