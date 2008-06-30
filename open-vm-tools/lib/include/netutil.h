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

#include "vm_basic_types.h"

#if !defined(N_PLAT_NLM)
#  include "guestInfo.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <iphlpapi.h>
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

#endif

#ifdef N_PLAT_NLM
/* Monitoring IP changes */
void NetUtil_MonitorIPStart(void);
void NetUtil_MonitorIPStop(void);
#endif

#endif
