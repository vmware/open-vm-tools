/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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
 * netutil.h --
 *
 *    Utility network functions.
 *
 */


#ifndef __NETUTIL_H__
#define __NETUTIL_H__

#ifdef _WIN32
#   include <windows.h>
#   include <ws2tcpip.h>
#   include "vmware/iphlpapi_packed.h"
#   include <ipmib.h>
#   include <netioapi.h>
#else
#   include <arpa/inet.h>
#endif

#include "vm_basic_types.h"
#include "guestInfo.h"

/*
 * Interface types as assigned by IANA.
 * See http://www.iana.org/assignments/ianaiftype-mib for more details.
 */

typedef enum {
   IANA_IFTYPE_OTHER            = 1,
   IANA_IFTYPE_ETHERNETCSMACD   = 6,
} IanaIfType;


/*
 * Modified from iptypes.h...
 */
char *NetUtil_GetPrimaryIP(void);

GuestNic *NetUtil_GetPrimaryNic(void);

#ifdef _WIN32

/*
 * Brute-force this. Trying to do it "properly" with
 * WINVER/NTDDI_VERSION checks/manips in a way that compiles for both
 * VC8 and VC9 didn't work.
 */
#ifndef FIXED_INFO
typedef  FIXED_INFO_W2KSP1 FIXED_INFO;
typedef  FIXED_INFO_W2KSP1 *PFIXED_INFO;
#endif

DWORD NetUtil_LoadIpHlpApiDll(void);
DWORD NetUtil_FreeIpHlpApiDll(void);

/* Wrappers for functions in iphlpapi.dll */
PFIXED_INFO NetUtil_GetNetworkParams(void);
PIP_ADAPTER_INFO NetUtil_GetAdaptersInfo(void);
ULONG NetUtil_GetAdaptersAddresses(ULONG Family,
                                   ULONG Flags,
                                   PVOID rsvd,
                                   PIP_ADAPTER_ADDRESSES adap_addresses,
                                   PULONG SizePointer);

PMIB_IPFORWARDTABLE NetUtilWin32_GetIpForwardTable(void);
PMIB_IPFORWARD_TABLE2 NetUtilWin32_GetIpForwardTable2(void);
void NetUtilWin32_FreeMibTable(PMIB_IPFORWARD_TABLE2);
#endif

#ifdef WIN32
int NetUtil_InetPToN(int af, const char *src, void *dst);
const char *NetUtil_InetNToP(int af, const void *src, char *dst,
                             socklen_t size);
#else // ifdef WIN32
#   define NetUtil_InetPToN     inet_pton
#   define NetUtil_InetNToP     inet_ntop
#endif

#if defined(__linux__)
#   ifdef DUMMY_NETUTIL
/*
 * Dummy interface table to enable other tools'/libraries' unit tests.
 */
typedef struct {
   int           ifIndex;
   const char   *ifName;
} NetUtilIfTableEntry;


/*
 * {-1, NULL}-terminated array of NetUtilIfTableEntry pointers.
 *
 * (Test) applications wishing to use the dummy NetUtil_GetIf{Index,Name}
 * functions must define this variable somewhere.  It allows said apps
 * to work with a priori knowledge of interface name <=> index mappings
 * returned by said APIs.
 */
EXTERN NetUtilIfTableEntry netUtilIfTable[];

int   NetUtil_GetIfIndex(const char *ifName);
char *NetUtil_GetIfName(int ifIndex);
#   endif // ifdef DUMMY_NETUTIL
#endif // if defined(__linux__)

size_t NetUtil_GetHardwareAddress(int ifIndex,         // IN
                                  char *hwAddr,        // OUT
                                  size_t hwAddrSize,   // IN
                                  IanaIfType *ifType); // OUT

#endif // ifndef _NETUTIL_H_
