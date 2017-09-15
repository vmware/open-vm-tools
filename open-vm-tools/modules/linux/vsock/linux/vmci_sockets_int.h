/*********************************************************
 * Copyright (C) 2009,2014 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmci_sockets_int.h --
 *
 *    vSockets private constants and types.
 *
 *    This file is internal only, we do not ship the kernel interface yet.
 *    You need to include this file *before* vmci_sockets.h in your kernel
 *    module.
 */

#ifndef _VMCI_SOCKETS_INT_H_
#define _VMCI_SOCKETS_INT_H_

#if defined __cplusplus
extern "C" {
#endif


#if defined(_WIN32)
#  if defined(NT_INCLUDED)
#     if (_WIN32_WINNT < 0x0600)
         /*
          * WinSockKernel is targetted at Vista and later.  We want to allow
          * drivers built from W2K onwards to work with the interface, so we
          * need to define some missing types before we bring in the WSK header.
          */
         typedef unsigned short u_short;
#        include <windef.h>
#        include <ws2def.h>
         typedef WSACMSGHDR CMSGHDR, *PCMSGHDR;
#     endif // (_WIN32_WINNT < 0x0600)
#     include <wsk.h>
      NTSTATUS VMCISock_WskRegister(PWSK_CLIENT_NPI wskClientNpi,
                                    PWSK_REGISTRATION wskRegistration);
      NTSTATUS VMCISock_WskDeregister(PWSK_REGISTRATION wskRegistration);
      NTSTATUS VMCISock_WskCaptureProviderNPI(PWSK_REGISTRATION wskRegistration,
                                              ULONG waitTimeout,
                                              PWSK_PROVIDER_NPI wskProviderNpi);
      NTSTATUS VMCISock_WskReleaseProviderNPI(PWSK_REGISTRATION wskRegistration);
      NTSTATUS VMCISock_WskGetAFValue(PWSK_CLIENT wskClient, PIRP irp);
      NTSTATUS VMCISock_WskGetLocalCID(PWSK_CLIENT wskClient, PIRP irp);
#  endif // NT_INCLUDED
#endif // _WIN32


#if defined __cplusplus
} // extern "C"
#endif

#endif // _VMCI_SOCKETS_INT_H_

