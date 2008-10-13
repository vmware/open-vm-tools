/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * vmci_sockets_kernel.h --
 *
 *    VMCI sockets kernel public constants and types.
 */

#ifndef _VMCI_SOCKETS_KERNEL_H_
#define _VMCI_SOCKETS_KERNEL_H_


#include "vmci_sockets.h"


#if defined(_WIN32)
#  if defined(WINNT_DDK)
   typedef WSACMSGHDR CMSGHDR, *PCMSGHDR;
#  include <wsk.h>
   NTSTATUS VMCISock_WskRegister(PWSK_CLIENT_NPI wskClientNpi,
                                 PWSK_REGISTRATION wskRegistration);
   NTSTATUS VMCISock_WskDeregister(PWSK_REGISTRATION wskRegistration);
   NTSTATUS VMCISock_WskCaptureProviderNPI(PWSK_REGISTRATION wskRegistration,
                                           ULONG waitTimeout,
                                           PWSK_PROVIDER_NPI wskProviderNpi);
   NTSTATUS VMCISock_WskReleaseProviderNPI(PWSK_REGISTRATION wskRegistration);
#  endif // WINNT_DDK
#endif // _WIN32


#endif // _VMCI_SOCKETS_KERNEL_H_

