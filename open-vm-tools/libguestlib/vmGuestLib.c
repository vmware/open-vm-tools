/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * Implementation of the guestlib library.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vmware.h"
#include "vmGuestLib.h"
#include "vmGuestLibInt.h"
#include "str.h"
#include "rpcout.h"
#include "vmcheck.h"
#include "guestApp.h" // for ALLOW_TOOLS_IN_FOREIGN_VM

#define GUESTLIB_NAME "VMware Guest API"


// XXX For testing only. As a real "library", this should not be here.
#include "debug.h"



#if 0
/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLibDumpData --
 *
 *      Debugging routine to print a data struct.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
VMGuestLibDumpData(VMGuestLibDataV2 *data) // IN
{
   ASSERT(data);

   printf("version: %u\n"
          "sessionId: %"FMT64"u\n"
          "cpuReservationMHz: %u\n"
          "cpuLimitMHz: %u\n"
          "cpuShares: %u\n"
          "cpuUsedMs: %"FMT64"u\n"
          "hostMHz: %u\n"
          "memReservationMB: %u\n"
          "memLimitMB: %u\n"
          "memShares: %u\n"
          "memMappedMB: %u\n"
          "memActiveMB: %u\n"
          "memOverheadMB: %u\n"
          "memBalloonedMB: %u\n"
          "memSwappedMB: %u\n"
          "memSharedMB: %u\n"
          "memSharedSavedMB: %u\n"
          "memUsedMB: %u\n"
          "elapsedMs: %"FMT64"u\n"
          "resourcePoolPath: '%s'\n"
          "last character: '%c'\n",
          data->version, data->sessionId,
          data->cpuReservationMHz.value, data->cpuLimitMHz.value,
          data->cpuShares.value, data->cpuUsedMs.value,
          data->hostMHz.value, data->memReservationMB.value,
          data->memLimitMB.value, data->memShares.value,
          data->memMappedMB.value, data->memActiveMB.value,
          data->memOverheadMB.value, data->memBalloonedMB.value,
          data->memSwappedMB.value, data->memSharedMB.value, 
          data->memSharedSavedMB.value, data->memUsedMB.value,
          data->elapsedMs.value, data->resourcePoolPath.value,
          data->resourcePoolPath.value[sizeof data->resourcePoolPath.value - 2]);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetErrorText --
 *
 *      Get the English text explanation for a given GuestLib error code.
 *
 * Results:
 *      The error string for the given error code.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char const *
VMGuestLib_GetErrorText(VMGuestLibError error) // IN: Error code
{
   switch (error) {
   case VMGUESTLIB_ERROR_SUCCESS:
      return "No error";

   case VMGUESTLIB_ERROR_NOT_RUNNING_IN_VM:
      return GUESTLIB_NAME " is not running in a Virtual Machine";

   case VMGUESTLIB_ERROR_NOT_ENABLED:
      return GUESTLIB_NAME " is not enabled on the host";

   case VMGUESTLIB_ERROR_NOT_AVAILABLE:
      return "This value is not available on this host";

   case VMGUESTLIB_ERROR_NO_INFO:
      return "VMGuestLib_UpdateInfo() has not been called";

   case VMGUESTLIB_ERROR_MEMORY:
      return "There is not enough system memory";

   case VMGUESTLIB_ERROR_BUFFER_TOO_SMALL:
      return "The provided memory buffer is too small";

   case VMGUESTLIB_ERROR_INVALID_HANDLE:
      return "The provided handle is invalid";

   case VMGUESTLIB_ERROR_INVALID_ARG:
      return "One or more arguments were invalid";

   case VMGUESTLIB_ERROR_OTHER:
      return "Other error";

   default:
      ASSERT(FALSE); // We want to catch this case in debug builds.
      return "Other error";
   }

   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLibCheckArgs --
 *
 *      Helper function to factor out common argument checking code.
 *
 *      If VMGUESTLIB_ERROR_SUCCESS is returned, args are valid and *data
 *      now points to the valid struct.
 *
 * Results:
 *      Error indicating results
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VMGuestLibError
VMGuestLibCheckArgs(VMGuestLibHandle handle, // IN
                    void *outArg,            // IN
                    VMGuestLibDataV2 **data) // OUT
{
   ASSERT(data);

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_HANDLE;
   }

   if (NULL == outArg) {
      return VMGUESTLIB_ERROR_INVALID_ARG;
   }

   *data = (VMGuestLibDataV2 *)handle;

   if (0 == (*data)->sessionId) {
      return VMGUESTLIB_ERROR_NO_INFO;
   }

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_OpenHandle --
 *
 *      Obtain a handle for use with GuestLib. Allocates resources and
 *      returns a handle that should be released using
 *      VMGuestLib_CloseHandle().
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      Resources are allocated.
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_OpenHandle(VMGuestLibHandle *handle) // OUT
{
   VMGuestLibDataV2 *data;

#ifndef ALLOW_TOOLS_IN_FOREIGN_VM
   if (!VmCheck_IsVirtualWorld()) {
      Debug("VMGuestLib_OpenHandle: Not in a VM.\n");
      return VMGUESTLIB_ERROR_NOT_RUNNING_IN_VM;
   }
#endif

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_ARG;
   }

   data = calloc(1, sizeof *data);
   if (!data) {
      Debug("VMGuestLib_OpenHandle: Unable to allocate memory\n");
      return VMGUESTLIB_ERROR_MEMORY;
   }

   *handle = (VMGuestLibHandle)data;
   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_CloseHandle --
 *
 *      Release resources associated with a handle obtained from
 *      VMGuestLib_OpenHandle().
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_CloseHandle(VMGuestLibHandle handle) // IN
{
   VMGuestLibDataV2 *data;

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_HANDLE;
   }

   data = (VMGuestLibDataV2 *)handle;
   free(data);

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLibUpdateInfo --
 *
 *      Retrieve the bundle of stats over the backdoor.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VMGuestLibError
VMGuestLibUpdateInfo(VMGuestLibDataV2 *data) // IN
{
   char commandBuf[64];
   char *reply;
   size_t replyLen;
   VMGuestLibDataV2 *newData;

   /*
    * Construct command string with the command name and the version
    * of the data struct that we want.
    */
   Str_Sprintf(commandBuf, sizeof commandBuf, "%s %d",
               VMGUESTLIB_BACKDOOR_COMMAND_STRING,
               VMGUESTLIB_DATA_VERSION);

   /* Actually send the request. */
   if (!RpcOut_sendOne(&reply, &replyLen, commandBuf)) {
      Debug("Failed to retrieve info: %s\n", reply ? reply : "NULL");
      free(reply);
      return VMGUESTLIB_ERROR_NOT_ENABLED;
   }

   /* Sanity check the results. */
   if (replyLen < sizeof data->version) {
      Debug("Unable to retrieve version\n");
      return VMGUESTLIB_ERROR_OTHER;
   }
   newData = (VMGuestLibDataV2 *)reply;
   if (newData->version != VMGUESTLIB_DATA_VERSION) {
      Debug("Incorrect version returned\n");
      return VMGUESTLIB_ERROR_OTHER;
   }
   if (replyLen != sizeof *data) {
      Debug("Incorrect data size returned\n");
      return VMGUESTLIB_ERROR_OTHER;
   }

   /* Copy stats to struct, free buffer. */
   memcpy(data, newData, sizeof *data);
   free(reply);

   /* Make sure resourcePoolPath is NUL terminated. */
   data->resourcePoolPath.value[sizeof data->resourcePoolPath.value - 1] = '\0';

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_UpdateInfo --
 *
 *      Update VMGuestLib's internal info state.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      Previous stat values will be overwritten.
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_UpdateInfo(VMGuestLibHandle handle) // IN
{
   VMGuestLibError error;

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_HANDLE;
   }

   /*
    * We check for virtual world in VMGuestLib_OpenHandle, so we don't
    * need to do the test again here.
    */

   error = VMGuestLibUpdateInfo((VMGuestLibDataV2 *)handle);
   if (error != VMGUESTLIB_ERROR_SUCCESS) {
      Debug("VMGuestLibUpdateInfo failed: %d\n", error);
      return error;
   }

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetSessionId --
 *
 *      Retrieve the session ID for this virtual machine.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetSessionId(VMGuestLibHandle handle, // IN
                        VMSessionId *id)         // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, id, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   *id = data->sessionId;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetCpuReservationMHz --
 *
 *      Retrieve CPU min speed.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetCpuReservationMHz(VMGuestLibHandle handle,   // IN
                                uint32 *cpuReservationMHz) // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, cpuReservationMHz, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->cpuReservationMHz.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *cpuReservationMHz = data->cpuReservationMHz.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetCpuLimitMHz --
 *
 *      Retrieve CPU max speed.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetCpuLimitMHz(VMGuestLibHandle handle, // IN
                          uint32 *cpuLimitMHz)     // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, cpuLimitMHz, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->cpuLimitMHz.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *cpuLimitMHz = data->cpuLimitMHz.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetCpuShares --
 *
 *      Retrieve CPU shares.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetCpuShares(VMGuestLibHandle handle, // IN
                        uint32 *cpuShares)       // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, cpuShares, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->cpuShares.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *cpuShares = data->cpuShares.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetCpuUsedMs --
 *
 *      Retrieve CPU used time.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetCpuUsedMs(VMGuestLibHandle handle, // IN
                        uint64 *cpuUsedMs)       // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, cpuUsedMs, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->cpuUsedMs.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *cpuUsedMs = data->cpuUsedMs.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostProcessorSpeed --
 *
 *      Retrieve host processor speed. This can be used along with
 *      CpuUsedMs and elapsed time to estimate approximate effective
 *      VM CPU speed over a time interval.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetHostProcessorSpeed(VMGuestLibHandle handle, // IN
                                 uint32 *mhz)             // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, mhz, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->hostMHz.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *mhz = data->hostMHz.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VMGuestLib_GetMemReservationMB --
 *
 *      Retrieve min memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemReservationMB(VMGuestLibHandle handle,  // IN
                               uint32 *memReservationMB) // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memReservationMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memReservationMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memReservationMB = data->memReservationMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemLimitMB --
 *
 *      Retrieve max memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemLimitMB(VMGuestLibHandle handle, // IN
                         uint32 *memLimitMB)      // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memLimitMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memLimitMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memLimitMB = data->memLimitMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemShares --
 *
 *      Retrieve memory shares.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemShares(VMGuestLibHandle handle, // IN
                        uint32 *memShares)       // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memShares, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memShares.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memShares = data->memShares.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemMappedMB --
 *
 *      Retrieve mapped memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemMappedMB(VMGuestLibHandle handle,  // IN
                          uint32 *memMappedMB)      // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memMappedMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memMappedMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memMappedMB = data->memMappedMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemActiveMB --
 *
 *      Retrieve active memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemActiveMB(VMGuestLibHandle handle, // IN
                          uint32 *memActiveMB)     // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memActiveMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memActiveMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memActiveMB = data->memActiveMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemOverheadMB --
 *
 *      Retrieve overhead memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemOverheadMB(VMGuestLibHandle handle, // IN
                            uint32 *memOverheadMB)   // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memOverheadMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memOverheadMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memOverheadMB = data->memOverheadMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemBalloonedMB --
 *
 *      Retrieve ballooned memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemBalloonedMB(VMGuestLibHandle handle, // IN
                             uint32 *memBalloonedMB)  // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memBalloonedMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memBalloonedMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memBalloonedMB = data->memBalloonedMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemSwappedMB --
 *
 *      Retrieve swapped memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemSwappedMB(VMGuestLibHandle handle, // IN
                           uint32 *memSwappedMB)    // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memSwappedMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memSwappedMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memSwappedMB = data->memSwappedMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemSharedMB --
 *
 *      Retrieve shared memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemSharedMB(VMGuestLibHandle handle, // IN
                          uint32 *memSharedMB)     // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memSharedMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memSharedMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memSharedMB = data->memSharedMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemSharedSavedMB --
 *
 *      Retrieve shared saved memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemSharedSavedMB(VMGuestLibHandle handle,  // IN
                               uint32 *memSharedSavedMB) // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memSharedSavedMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memSharedSavedMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memSharedSavedMB = data->memSharedSavedMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemUsedMB --
 *
 *      Retrieve used memory.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetMemUsedMB(VMGuestLibHandle handle, // IN
                        uint32 *memUsedMB)       // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, memUsedMB, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->memUsedMB.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *memUsedMB = data->memUsedMB.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetElapsedMs --
 *
 *      Retrieve elapsed time on the host.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetElapsedMs(VMGuestLibHandle handle, // IN
                        uint64 *elapsedMs)       // OUT
{
   VMGuestLibDataV2 *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, elapsedMs, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   if (!data->elapsedMs.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   *elapsedMs = data->elapsedMs.value;

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetResourcePoolPath --
 *
 *      Retrieve Resource Pool Path.
 *
 *      pathBuffer is a pointer to a buffer that will receive the
 *      resource pool path string. bufferSize is a pointer to the
 *      size of the pathBuffer in bytes. If bufferSize is not large
 *      enough to accomodate the path and NUL terminator, then
 *      VMGUESTLIB_ERROR_BUFFER_TOO_SMALL is returned and bufferSize
 *      contains the amount of memory needed (in bytes).
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VMGuestLibError
VMGuestLib_GetResourcePoolPath(VMGuestLibHandle handle, // IN
                               size_t *bufferSize,      // IN/OUT
                               char *pathBuffer)        // OUT
{
   size_t size;
   VMGuestLibDataV2 *data;

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_HANDLE;
   }

   if (NULL == bufferSize || NULL == pathBuffer) {
      return VMGUESTLIB_ERROR_INVALID_ARG;
   }

   data = (VMGuestLibDataV2 *)handle;

   if (0 == data->sessionId) {
      return VMGUESTLIB_ERROR_NO_INFO;
   }

   if (!data->resourcePoolPath.valid) {
      return VMGUESTLIB_ERROR_NOT_AVAILABLE;
   }

   /*
    * Check buffer size. It's safe to use strlen because we squash the
    * final byte of resourcePoolPath in VMGuestLibUpdateInfo.
    */
   size = strlen(data->resourcePoolPath.value) + 1;
   if (*bufferSize < size) {
      *bufferSize = size;
      return VMGUESTLIB_ERROR_BUFFER_TOO_SMALL;
   }

   memcpy(pathBuffer, data->resourcePoolPath.value, *bufferSize);

   return VMGUESTLIB_ERROR_SUCCESS;
}
