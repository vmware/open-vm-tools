/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * toolboxcmd-stat.c --
 *
 *     Various stat operations for toolbox-cmd
 */

#include <time.h>
#include "toolboxCmdInt.h"


/*
 * Local Functions
 */

static int OpenHandle(VMGuestLibHandle *glHandle,VMGuestLibError *glError);


/*
 *-----------------------------------------------------------------------------
 *
 * OpenHandle  --
 *
 *      Opens a VMGuestLibHandle.
 *
 * Results:
 *      0 on success.
 *      EX_UNAVAILABLE on failure to open handle.
 *      EX_TEMPFAIL on failure to update info.
 *
 * Side effects:
 *      Prints to stderr and exits on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
OpenHandle(VMGuestLibHandle *glHandle, // OUT: The guestlib handle
           VMGuestLibError *glError)   // OUT: The errors when opening the handle
{
   *glError = VMGuestLib_OpenHandle(glHandle);
   if (*glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "OpenHandle failed: %s\n", VMGuestLib_GetErrorText(*glError));
      return EX_UNAVAILABLE;
   }
   *glError = VMGuestLib_UpdateInfo(*glHandle);
   if (*glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "UpdateInfo failed: %s\n", VMGuestLib_GetErrorText(*glError));
      return EX_TEMPFAIL;
   }
   return 0;  // We don't return EXIT_SUCCESSS to indicate that this is not
              // an exit code

}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_ProcessorSpeed  --
 *
 *      Gets the Processor Speed.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_ProcessorSpeed(void)
{
   uint32 speed;
   Backdoor_proto bp;
   bp.in.cx.halfs.low = BDOOR_CMD_GETMHZ;
   Backdoor(&bp);
   speed = bp.out.ax.word;
   if (speed < 0) {
      fprintf(stderr, "Unable to get processor speed\n");
      return EX_TEMPFAIL;
   }
   printf("%u MHz\n", speed);
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_MemorySize  --
 *
 *      Gets the virtual machine's memory in MBs.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_MemorySize(void)
{
   uint32 memsize;
   Backdoor_proto bp;
   bp.in.cx.halfs.low = BDOOR_CMD_GETMEMSIZE;
   Backdoor(&bp);
   memsize = bp.out.ax.word;
   if (memsize < 0) {
      fprintf(stderr, "Unable to get memory size\n");
      return EX_TEMPFAIL;
   }
   printf("%u MB\n", memsize);
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_HostTime  --
 *
 *      Gets the host machine's time.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_HostTime(void)
{
   int64 hostSecs;
   int64 hostUsecs;
   time_t sec;
   char buf[256];
   Backdoor_proto bp;

   bp.in.cx.halfs.low = BDOOR_CMD_GETTIMEFULL;
   Backdoor(&bp);
   if (bp.out.ax.word == BDOOR_MAGIC) {
      hostSecs = ((uint64)bp.out.si.word << 32) | bp.out.dx.word;
   } else {
      /* Falling back to older command. */
      bp.in.cx.halfs.low = BDOOR_CMD_GETTIME;
      Backdoor(&bp);
      hostSecs = bp.out.ax.word;
   }
   hostUsecs = bp.out.bx.word;

   if (hostSecs <= 0) {
      fprintf(stderr, "Unable to get host time\n");
      return EX_TEMPFAIL;
   }
   
   sec = hostSecs + (hostUsecs / 1000000);
   if (strftime(buf, sizeof buf, "%d %b %Y %H:%M:%S", localtime(&sec)) == 0) {
      fprintf(stderr, "Unable to format host time\n");
      return EX_TEMPFAIL;
   }
   printf("%s\n", buf);
   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_GetSessionID --
 *
 *      Gets the Session ID for the virtual machine
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_GetSessionID(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint64 session;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetSessionId(glHandle, &session);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "Failed to get session ID: %s\n",
              VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("0x%"FMT64"x\n", session);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_GetMemoryBallooned  --
 *
 *      Retrieves memory ballooned.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_GetMemoryBallooned(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 memBallooned;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemBalloonedMB(glHandle, &memBallooned);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "Failed to get CPU Limit: %s\n", VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MHz\n", memBallooned);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_GetMemoryReservation  --
 *
 *      Retrieves min memory.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_GetMemoryReservation(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32  memReservation;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemReservationMB(glHandle, &memReservation);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "Failed to get CPU Limit: %s\n", VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MB\n", memReservation);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_GetMemorySwapped  --
 *
 *      Retrieves swapped memory.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *      If OpenHandle fails the program exits.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_GetMemorySwapped(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 memSwapped;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemSwappedMB(glHandle, &memSwapped);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "Failed to get CPU Limit: %s\n", VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MB\n", memSwapped);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_GetMemoryLimit --
 *
 *      Retrieves max memory.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_GetMemoryLimit(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 memLimit;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetMemLimitMB(glHandle, &memLimit);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "Failed to get CPU Limit: %s\n", VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MB\n", memLimit);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_GetCpuReservation  --
 *
 *      Retrieves cpu min speed.
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_GetCpuReservation(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 cpuReservation;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetCpuReservationMHz(glHandle, &cpuReservation);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "Failed to get CPU Limit: %s\n", VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MHz\n", cpuReservation);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Stat_GetCpuLimit  --
 *
 *      Retrieves cpu max speed .
 *      Works only if the host is ESX.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Prints to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

int
Stat_GetCpuLimit(void)
{
   int exitStatus = EXIT_SUCCESS;
   uint32 cpuLimit;
   VMGuestLibHandle glHandle;
   VMGuestLibError glError;

   exitStatus = OpenHandle(&glHandle, &glError);
   if (exitStatus) {
      return exitStatus;
   }
   glError = VMGuestLib_GetCpuLimitMHz(glHandle, &cpuLimit);
   if (glError != VMGUESTLIB_ERROR_SUCCESS) {
      fprintf(stderr, "Failed to get CPU Limit: %s\n", VMGuestLib_GetErrorText(glError));
      exitStatus = EX_TEMPFAIL;
   } else {
      printf("%u MHz\n", cpuLimit);
   }
   VMGuestLib_CloseHandle(glHandle);
   return exitStatus;
}
