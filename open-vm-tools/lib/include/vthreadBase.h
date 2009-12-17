/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * vthreadBase.h --
 *
 *	Subset of vthread defines that are used by libs that need to make
 *      vthread calls but don't actually do any vthreading. These libs
 *      can be linked against lib/nothread and will function correctly.
 */

#ifndef VTHREAD_BASE_H
#define VTHREAD_BASE_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if !defined VMM && !defined WIN32
#define VTHREAD_USE_PTHREAD 1
#endif


/*
 * Types
 */

typedef unsigned VThreadID;


/*
 * Constants
 *
 * VThread ID's are allocated this way:
 *    0 -- VMX main thread
 *    1 -- MKS
 *    2 -- UI
 *    3 -- other (main thread in simple apps)
 *    4..3+VTHREAD_MAX_VCPUS -- VMX VCPU threads
 *    4+VTHREAD_MAX_VCPUS.. -- additional dynamic threads
 */

#define VTHREAD_MAX_THREADS	96
#define VTHREAD_MAX_VCPUS	32

#define VTHREAD_INVALID_ID	(~0u)

#define VTHREAD_VMX_ID		0
#define VTHREAD_MKS_ID		1
#define VTHREAD_UI_ID		2
#define VTHREAD_OTHER_ID	3
#define VTHREAD_VCPU0_ID	4
#define VTHREAD_ALLOCSTART_ID	(VTHREAD_VCPU0_ID + VTHREAD_MAX_VCPUS)


const char *VThread_CurName(void);

#ifndef VMM
VThreadID VThread_CurID(void);
void VThread_Init(VThreadID tid, const char *name);
VThreadID VThread_InitThread(VThreadID tid, const char *name);
void NORETURN VThread_ExitThread(Bool clean);

#ifdef _WIN32
static INLINE Bool
VThread_IsInSignal(void)
{
   /* Win32 does not worry about async-signal-safety. */
   return FALSE;
}
#else
Bool VThread_IsInSignal(void);
#endif
#endif


#endif // VTHREAD_BASE_H
