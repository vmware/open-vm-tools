/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>
#endif

#include "vm_basic_types.h"

#if !defined(N_PLAT_NLM)
#  include "guestInfo.h"
#endif


/*
 * Modified from iptypes.h...
 */
#if (NTDDI_VERSION < NTDDI_WIN2KSP1)
typedef FIXED_INFO_W2KSP1 FIXED_INFO;
typedef FIXED_INFO_W2KSP1 *PFIXED_INFO;
#endif

char *NetUtil_GetPrimaryIP(void);

#if !defined(N_PLAT_NLM)
GuestNic *NetUtil_GetPrimaryNic(void);
#endif

#ifdef _WIN32
DWORD NetUtil_LoadIpHlpApiDll(void);
DWORD NetUtil_FreeIpHlpApiDll(void);
Bool NetUtil_ReleaseRenewIP(Bool release);

/* Wrappers for functions in iphlpapi.dll */
PFIXED_INFO NetUtil_GetNetworkParams(void);
PIP_ADAPTER_INFO NetUtil_GetAdaptersInfo(void);
ULONG NetUtil_GetAdaptersAddresses(ULONG Family,
                                   ULONG Flags,
                                   PVOID rsvd,
                                   PIP_ADAPTER_ADDRESSES adap_addresses,
                                   PULONG SizePointer);

#endif

#ifdef N_PLAT_NLM
/* Monitoring IP changes */
void NetUtil_MonitorIPStart(void);
void NetUtil_MonitorIPStop(void);
#endif

#if defined(linux)
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
#   endif // ifdef DUMMY_NETUTIL

int   NetUtil_GetIfIndex(const char *ifName);
char *NetUtil_GetIfName(int ifIndex);

#endif // if defined(linux)

#endif
