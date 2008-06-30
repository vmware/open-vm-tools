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
#include "guestrpc/nicinfo.h"

Bool GuestInfoGetFqdn(int outBufLen, char fqdn[]);
Bool GuestInfoGetNicInfo(GuestNicList *nicInfo);
void GuestInfoMemset(void * mem, int value, unsigned int size);
Bool GuestInfoGetDiskInfo(PGuestDiskInfo di);
Bool GuestInfoGetOSName(unsigned int outBufFullLen, unsigned int outBufLen,
                        char *osNameFull, char *osName);
Bool GuestInfo_PerfMon(struct GuestMemInfo *vmStats);

GuestNic *GuestInfoAddNicEntry(GuestNicList *nicInfo,
                               const char macAddress[NICINFO_MAC_LEN]);
VmIpAddress *GuestInfoAddIpAddress(GuestNic *nic,
                                   const char *ipAddr,
                                   const uint32 af_type);
void GuestInfoAddSubnetMask(VmIpAddress *ipAddressEntry,
                            const uint32 subnetMaskBits);

#endif
