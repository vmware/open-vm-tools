/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * vmciProcess.c --
 *
 *     VMCI Process code for guest driver.
 */

#ifdef __linux__
#  include "driver-config.h"

#  define EXPORT_SYMTAB

#  include <linux/module.h>
#  include "compat_kernel.h"
#  include "compat_pci.h"
#endif // __linux__

#include "vmciInt.h"
#include "vmci_defs.h"
#include "vmci_kernel_if.h"
#include "vmciProcess.h"
#include "vmciDatagram.h"
#include "vmci_infrastructure.h"
#include "circList.h"
#include "vmciUtil.h"
#include "vmciGuestKernelAPI.h"
#include "vmciCommonInt.h"

static ListItem *processList = NULL;
static VMCILock processLock;

/*
 *----------------------------------------------------------------------
 *
 * VMCIProcess_Init --
 *
 *      General init code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIProcess_Init(void)
{
   VMCI_InitLock(&processLock, "VMCIProcessListLock", VMCI_LOCK_RANK_HIGH);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIProcess_Exit --
 *
 *      General init code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIProcess_Exit(void)
{
   VMCI_CleanupLock(&processLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIProcess_CheckHostCapabilities --
 *
 *      Verify that the host supports the hypercalls we need. If it does not,
 *      try to find fallback hypercalls and use those instead.
 *
 * Results:
 *      TRUE if required hypercalls (or fallback hypercalls) are
 *      supported by the host, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCIProcess_CheckHostCapabilities(void)
{
   /* VMCIProcess does not require any hypercalls. */
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIProcess_Create --
 *
 *      Creates a new VMCI process.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIProcess_Create(VMCIProcess **outProcess) // IN
{
   VMCIProcess *process;
   VMCILockFlags flags;

   process = VMCI_AllocKernelMem(sizeof *process, VMCI_MEMORY_NONPAGED);
   if (process == NULL) {
      return VMCI_ERROR_NO_MEM;
   }

   process->pid = (VMCIId)(uintptr_t)process >> 1;

   VMCI_GrabLock(&processLock, &flags);
   LIST_QUEUE(&process->listItem, &processList);
   VMCI_ReleaseLock(&processLock, flags);

   *outProcess = process;
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIProcess_Destroy --
 *
 *      Destroys a VMCI process.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIProcess_Destroy(VMCIProcess *process)
{
   VMCILockFlags flags;

   /* Dequeue process. */
   VMCI_GrabLock(&processLock, &flags);
   LIST_DEL(&process->listItem, &processList);
   VMCI_ReleaseLock(&processLock, flags);

   VMCI_FreeKernelMem(process, sizeof *process);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIProcess_Get --
 *
 *      Get the process corresponding to the pid.
 *
 * Results:
 *      VMCI process on success, NULL otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCIProcess *
VMCIProcess_Get(VMCIId processID)  // IN
{
   VMCIProcess *process = NULL;  
   ListItem *next;
   VMCILockFlags flags;

   VMCI_GrabLock(&processLock, &flags);
   if (processList == NULL) {
      goto out;
   }

   LIST_SCAN(next, processList) {
      process = LIST_CONTAINER(next, VMCIProcess, listItem);
      if (process->pid == processID) {
         break;
      }
   }

out:
   VMCI_ReleaseLock(&processLock, flags);
   return (process && process->pid == processID) ? process : NULL;
}
