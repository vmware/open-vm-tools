/*********************************************************
 * Copyright (C) 2006-2013 VMware, Inc. All rights reserved.
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
 * vmciDriver.h --
 *
 *    VMCI host driver interface.
 */

#ifndef _VMCI_DRIVER_H_
#define _VMCI_DRIVER_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciContext.h"

/*
 * A few macros to encapsulate logging in common code. The macros
 * result in LOG/LOGThrottled on vmkernel and Log on hosted.
 */

#ifdef _WIN32
#  include "vmciLog.h"
#else // _WIN32
#if defined(VMKERNEL) && !defined(VMKERNEL_OFFSET_CHECKER)
   /*
    * LOGLEVEL_MODULE is defined in another header that gets included into the
    * dummy file used by the offsetchecker, which causes it to barf on the
    * redefinition.
    */
#  define LOGLEVEL_MODULE_LEN 0
#  define LOGLEVEL_MODULE VMCIVMK
#  include "log.h"
#  ifdef VMX86_LOG
#    define _VMCILOG(_args...) LOG(i, _args)
#    define VMCI_DEBUG_LOG(_level, _args) \
     do {                                 \
        int i = _level;                   \
        _VMCILOG _args ;                  \
     } while(FALSE)
#  else // VMX86_LOG
#    define VMCI_DEBUG_LOG(_level, _args)
#  endif // VMX86_LOG
#else // VMKERNEL
#  define VMCI_DEBUG_LEVEL 4
#  define VMCI_DEBUG_LOG(_level, _args) \
   do {                                 \
      if (_level < VMCI_DEBUG_LEVEL) {  \
         Log _args ;                    \
      }                                 \
   } while(FALSE)
#endif // VMKERNEL
#define VMCI_LOG(_args) Log _args
#define VMCI_WARNING(_args) Warning _args
#endif // _WIN32


int VMCI_SharedInit(void);
void VMCI_SharedCleanup(void);
int VMCI_HostInit(void);
void VMCI_HostCleanup(void);
VMCIId VMCI_GetContextID(void);
int VMCI_SendDatagram(VMCIDatagram *dg);

void VMCIUtil_Init(void);
void VMCIUtil_Exit(void);
Bool VMCI_CheckHostCapabilities(void);
void VMCI_ReadDatagramsFromPort(VMCIIoHandle ioHandle, VMCIIoPort dgInPort,
                                uint8 *dgInBuffer, size_t dgInBufferSize);
Bool VMCI_DeviceEnabled(void);
#if defined(_WIN64)
int VMCIDoSendDatagram(unsigned int dgSize, ULONG *dataPort, ULONG *resultPort,
                       VMCIDatagram *dg);
#endif // _WIN64


#endif // _VMCI_DRIVER_H_
