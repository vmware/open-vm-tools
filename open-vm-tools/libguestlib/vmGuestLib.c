/*********************************************************
 * Copyright (C) 2005-2016,2019 VMware, Inc. All rights reserved.
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
 * Implementation of the guestlib library.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vmware.h"
#include "vmGuestLib.h"
#include "vmGuestLibInt.h"
#include "str.h"
#include "vmware/tools/guestrpc.h"
#include "vmcheck.h"
#include "util.h"
#include "debug.h"
#include "strutil.h"
#include "guestlibV3.h"
#include "guestlibIoctl.h"
#include "dynxdr.h"
#include "xdrutil.h"
#include "ctype.h"

#define GUESTLIB_NAME "VMware Guest API"

/* 
 * These are client side data structures, separate from the wire data formats
 * (VMGuestLibDataV[23]).
 */

/* Layout of the buffer holding the variable length array of V3 statistics. */
typedef struct {
   GuestLibV3StatCount   numStats;
   GuestLibV3Stat        stats[0];
} VMGuestLibStatisticsV3;

/* Layout of the handle that holds information about the statistics. */
typedef struct {
   uint32 version;
   VMSessionId sessionId;

   /*
    * Statistics.
    *
    * dataSize is the size of the buffer pointed to by 'data'.
    * For v2 protocol: 
    *   - 'data' points to VMGuestLibDataV2 struct,
    * For v3 protocol:
    *   - 'data' points to VMGuestLibStatisticsV3 struct.
    */
   size_t dataSize;
   void *data;
} VMGuestLibHandleType;

#define HANDLE_VERSION(h)     (((VMGuestLibHandleType *)(h))->version)
#define HANDLE_SESSIONID(h)   (((VMGuestLibHandleType *)(h))->sessionId)
#define HANDLE_DATA(h)        (((VMGuestLibHandleType *)(h))->data)
#define HANDLE_DATASIZE(h)    (((VMGuestLibHandleType *)(h))->dataSize)

#define VMGUESTLIB_GETSTAT_V2(HANDLE, ERROR, OUTPTR, FIELDNAME)      \
   do {                                                              \
      VMGuestLibDataV2 *_dataV2 = HANDLE_DATA(HANDLE);               \
      ASSERT(HANDLE_VERSION(HANDLE) == 2);                           \
      if (!_dataV2->FIELDNAME.valid) {                               \
         (ERROR) = VMGUESTLIB_ERROR_NOT_AVAILABLE;                   \
         break;                                                      \
      }                                                              \
      *(OUTPTR) = _dataV2->FIELDNAME.value;                          \
      (ERROR) = VMGUESTLIB_ERROR_SUCCESS;                            \
   } while (0)

#define VMGUESTLIB_GETSTAT_V3(HANDLE, ERROR, OUTPTR, FIELDNAME, STATID)         \
   do {                                                                         \
      void *_data;                                                              \
      GuestLibV3Stat _stat;                                                     \
                                                                                \
      (ERROR) = VMGuestLibCheckArgs((HANDLE), (OUTPTR), &_data);                \
      if (VMGUESTLIB_ERROR_SUCCESS != (ERROR)) {                                \
         break;                                                                 \
      }                                                                         \
      ASSERT(HANDLE_VERSION(HANDLE) == 3);                                      \
      (ERROR) = VMGuestLibGetStatisticsV3((HANDLE), (STATID), &_stat);          \
      if ((ERROR) != VMGUESTLIB_ERROR_SUCCESS) {                                \
         break;                                                                 \
      }                                                                         \
      if (!_stat.GuestLibV3Stat_u.FIELDNAME.valid) {                            \
         (ERROR) = VMGUESTLIB_ERROR_NOT_AVAILABLE;                              \
         break;                                                                 \
      }                                                                         \
      ASSERT(_stat.d == (STATID));                                              \
      if (sizeof *(OUTPTR) < sizeof _stat.GuestLibV3Stat_u.FIELDNAME.value) {   \
         (ERROR) = VMGUESTLIB_ERROR_BUFFER_TOO_SMALL;                           \
         break;                                                                 \
      }                                                                         \
      *(OUTPTR) = _stat.GuestLibV3Stat_u.FIELDNAME.value;                       \
      (ERROR) = VMGUESTLIB_ERROR_SUCCESS;                                       \
   } while (0)

/* Handle extraction of integral type statistics. */
#define VMGUESTLIB_GETFN_BODY(HANDLE, ERROR, OUTPTR, FIELDNAME, STATID)         \
   do {                                                                         \
      void *_data;                                                              \
                                                                                \
      (ERROR) = VMGuestLibCheckArgs((HANDLE), (OUTPTR), &_data);                \
      if (VMGUESTLIB_ERROR_SUCCESS != (ERROR)) {                                \
         break;                                                                 \
      }                                                                         \
      if (HANDLE_VERSION(HANDLE) == 2) {                                        \
         VMGUESTLIB_GETSTAT_V2(HANDLE, ERROR, OUTPTR, FIELDNAME);               \
      } else if (HANDLE_VERSION(HANDLE) == 3) {                                 \
         VMGUESTLIB_GETSTAT_V3(HANDLE, ERROR, OUTPTR, FIELDNAME, STATID);       \
      }                                                                         \
   } while (0)


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

   case VMGUESTLIB_ERROR_UNSUPPORTED_VERSION:
      return "Host does not support this request.";

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
                    void **data)             // OUT
{
   ASSERT(data);

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_HANDLE;
   }

   if (NULL == outArg) {
      return VMGUESTLIB_ERROR_INVALID_ARG;
   }

   *data = HANDLE_DATA(handle);

   if (0 == HANDLE_SESSIONID(handle)) {
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
   VMGuestLibHandleType *data;

   if (!VmCheck_IsVirtualWorld()) {
      Debug("VMGuestLib_OpenHandle: Not in a VM.\n");
      return VMGUESTLIB_ERROR_NOT_RUNNING_IN_VM;
   }

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_ARG;
   }

   data = Util_SafeCalloc(1, sizeof *data);
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
 *      VMGuestLib_OpenHandle(). Handle is invalid after return
 *      from this call.
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
   void *data;

   if (NULL == handle) {
      return VMGUESTLIB_ERROR_INVALID_HANDLE;
   }

   data = HANDLE_DATA(handle);
   if (data != NULL &&
       HANDLE_SESSIONID(handle) != 0 &&
       HANDLE_VERSION(handle) == 3) {
      VMGuestLibStatisticsV3 *v3stats = data;
      GuestLibV3StatCount count;

      for (count = 0; count < v3stats->numStats; count++) {
         VMX_XDR_FREE(xdr_GuestLibV3Stat, &v3stats->stats[count]);
      }
   }
   free(data);

   /* Be paranoid. */
   HANDLE_DATA(handle) = NULL;
   free(handle);

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLibUpdateInfo --
 *
 *      Retrieve the bundle of stats over the backdoor and update the pointer to
 *      the Guestlib info in the handle.
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
VMGuestLibUpdateInfo(VMGuestLibHandle handle) // IN
{
   char *reply = NULL;
   size_t replyLen;
   VMGuestLibError ret = VMGUESTLIB_ERROR_INVALID_ARG;
   uint32 hostVersion = HANDLE_VERSION(handle);

   /* 
    * Starting with the highest supported protocol (major) version, negotiate
    * down to the highest host supported version. Host supports minimum version
    * 2.
    */
   if (hostVersion == 0) {
      hostVersion = VMGUESTLIB_DATA_VERSION;
   }

   do {
      char commandBuf[64];
      unsigned int index = 0;

      /* Free the last reply when retrying. */
      free(reply);
      reply = NULL;

      /*
       * Construct command string with the command name and the version
       * of the data struct that we want.
       */
      Str_Sprintf(commandBuf, sizeof commandBuf, "%s %d",
                  VMGUESTLIB_BACKDOOR_COMMAND_STRING,
                  hostVersion);

      /* Send the request. */
      if (RpcChannel_SendOne(&reply, &replyLen, commandBuf)) {
         VMGuestLibDataV2 *v2reply = (VMGuestLibDataV2 *)reply;
         VMSessionId sessionId = HANDLE_SESSIONID(handle);

         ASSERT(hostVersion == v2reply->hdr.version);

         if (sessionId != 0 && sessionId != v2reply->hdr.sessionId) {
            /* Renegotiate protocol if sessionId changed. */
            hostVersion = VMGUESTLIB_DATA_VERSION;
            HANDLE_SESSIONID(handle) = 0;
            continue;
         }
         ret = VMGUESTLIB_ERROR_SUCCESS;
         break;
      }

      /* 
       * Host is older and doesn't support the requested protocol version.
       * Request the highest version the host supports.
       */
      Debug("Failed to retrieve info: %s\n", reply ? reply : "NULL");

      if (hostVersion == 2 ||
          Str_Strncmp(reply, "Unknown command", sizeof "Unknown command") == 0) {
         /* 
          * Host does not support this feature. Older (v2) host would return
          * "Unsupported version" if it doesn't recognize the requested version.
          *
          * XXX: Maybe use another error code for this case where the host
          * product doesn't support this feature?
          */
         ret = VMGUESTLIB_ERROR_NOT_ENABLED;
         break;
      } else if (hostVersion == 3) {
         /*
          * Host supports v2 at a minimum. If request for v3 fails, then just use
          * v2, since v2 host does not send the highest supported version in the
          * reply.
          */
         hostVersion = 2;
         HANDLE_SESSIONID(handle) = 0;
         continue;
      } else if (!StrUtil_GetNextUintToken(&hostVersion, &index, reply, ":")) {
         /*
          * v3 and onwards, the host returns the highest major version it supports,
          * if the requested version is not supported. So parse out the host
          * version from the reply and return error if it didn't.
          */
         Debug("Bad reply received from host.\n");
         ret = VMGUESTLIB_ERROR_OTHER;
         break;
      }
      ASSERT(hostVersion < VMGUESTLIB_DATA_VERSION);
   } while (ret != VMGUESTLIB_ERROR_SUCCESS);

   if (ret != VMGUESTLIB_ERROR_SUCCESS) {
      goto done;
   }

   /* Sanity check the results. */
   if (replyLen < sizeof hostVersion) {
      Debug("Unable to retrieve version\n");
      ret = VMGUESTLIB_ERROR_OTHER;
      goto done;
   }

   if (hostVersion == 2) {
      VMGuestLibDataV2 *v2reply = (VMGuestLibDataV2 *)reply;
      size_t dataSize = sizeof *v2reply;

      /* More sanity checks. */
      if (v2reply->hdr.version != hostVersion) {
         Debug("Incorrect data version returned\n");
         ret = VMGUESTLIB_ERROR_OTHER;
         goto done;
      }
      if (replyLen != dataSize) {
         Debug("Incorrect data size returned\n");
         ret = VMGUESTLIB_ERROR_OTHER;
         goto done;
      }

      /* Store the reply in the handle. */
      HANDLE_VERSION(handle) = v2reply->hdr.version;
      HANDLE_SESSIONID(handle) = v2reply->hdr.sessionId;
      if (HANDLE_DATASIZE(handle) < dataSize) {
         /* [Re]alloc if the local handle buffer is not big enough. */
         free(HANDLE_DATA(handle));
         HANDLE_DATA(handle) = Util_SafeCalloc(1, dataSize);
         HANDLE_DATASIZE(handle) = dataSize;
      }
      memcpy(HANDLE_DATA(handle), reply, replyLen);

      /* Make sure resourcePoolPath is NUL terminated. */
      v2reply = HANDLE_DATA(handle);
      v2reply->resourcePoolPath.value[sizeof v2reply->resourcePoolPath.value - 1] = '\0';
      ret = VMGUESTLIB_ERROR_SUCCESS;
   } else if (hostVersion == 3) {
      VMGuestLibDataV3 *v3reply = (VMGuestLibDataV3 *)reply;
      size_t dataSize;
      XDR xdrs;
      GuestLibV3StatCount count;
      VMGuestLibStatisticsV3 *v3stats;

      /* More sanity checks. */
      if (v3reply->hdr.version != hostVersion) {
         Debug("Incorrect data version returned\n");
         ret = VMGUESTLIB_ERROR_OTHER;
         goto done;
      }
      if (replyLen < sizeof *v3reply) {
         Debug("Incorrect data size returned\n");
         ret = VMGUESTLIB_ERROR_OTHER;
         goto done;
      }

      /* 0. Copy the reply version and sessionId to the handle. */
      HANDLE_VERSION(handle) = v3reply->hdr.version;
      HANDLE_SESSIONID(handle) = v3reply->hdr.sessionId;

      /* 
       * 1. Retrieve the length of the statistics array from the XDR encoded
       * part of the reply.
       */
      xdrmem_create(&xdrs, v3reply->data, v3reply->dataSize, XDR_DECODE);

      if (!xdr_GuestLibV3StatCount(&xdrs, &count)) {
         xdr_destroy(&xdrs);
         goto done;
      }
      if (count >= GUESTLIB_MAX_STATISTIC_ID) {
         /* 
          * Host has more than we can process. So process only what this side
          * can.
          */
         count = GUESTLIB_MAX_STATISTIC_ID - 1;
      }

      /* 2. [Re]alloc if the local handle buffer is not big enough. */
      dataSize = sizeof *v3stats + (count * sizeof (GuestLibV3Stat));
      if (HANDLE_DATASIZE(handle) < dataSize) {
         free(HANDLE_DATA(handle));
         HANDLE_DATA(handle) = Util_SafeCalloc(1, dataSize);
         HANDLE_DATASIZE(handle) = dataSize;
      }

      /* 3. Unmarshal the array of statistics. */
      v3stats = HANDLE_DATA(handle);
      v3stats->numStats = count;
      for (count = 0; count < v3stats->numStats; count++) {
         GuestLibV3TypeIds statId = count + 1;

         /* Unmarshal the statistic. */
         if (!xdr_GuestLibV3Stat(&xdrs, &v3stats->stats[count])) {
            break;
         }

         /* Host sends all the V3 statistics it supports, in order. */
         if (v3stats->stats[count].d != statId) {
            break;
         }
      }
      if (count >= v3stats->numStats) {
         ret = VMGUESTLIB_ERROR_SUCCESS;
      } else {
         /* 
          * Error while unmarshalling. Deep-free already unmarshalled
          * statistics and invalidate the data in the handle.
          */
         GuestLibV3StatCount c;

         for (c = 0; c < count; c++) {
            VMX_XDR_FREE(xdr_GuestLibV3Stat, &v3stats->stats[c]);
         }
         HANDLE_SESSIONID(handle) = 0;
      }

      /* 4. Free resources. */
      xdr_destroy(&xdrs);
   } else {
      /*
       * Host should never reply with a higher version protocol than requested.
       */
      ret = VMGUESTLIB_ERROR_OTHER;
   }

done:
   free(reply);
   return ret;
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

   error = VMGuestLibUpdateInfo(handle);
   if (error != VMGUESTLIB_ERROR_SUCCESS) {
      Debug("VMGuestLibUpdateInfo failed: %d\n", error);
      HANDLE_SESSIONID(handle) = 0;
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
   void *data;
   VMGuestLibError error;

   error = VMGuestLibCheckArgs(handle, id, &data);
   if (VMGUESTLIB_ERROR_SUCCESS != error) {
      return error;
   }

   *id = HANDLE_SESSIONID(handle);

   return VMGUESTLIB_ERROR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLibGetStatisticsV3 --
 *
 *      Accessor helper to retrieve the requested V3 statistic.
 *
 * Results:
 *      VMGuestLibError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VMGuestLibError
VMGuestLibGetStatisticsV3(VMGuestLibHandle handle,   // IN
                          GuestLibV3TypeIds statId,  // IN
                          GuestLibV3Stat *outStat)   // OUT
{
   VMGuestLibStatisticsV3 *stats = HANDLE_DATA(handle);
   uint32 statIdx = statId - 1;

   /* 
    * Check that the requested statistic is supported by the host. V3 host sends
    * all the available statistics, in order. So any statistics that were added
    * to this version of guestlib, but unsupported by the host, would not be
    * sent by the host.
    */
   if (statIdx >= stats->numStats) {
      return VMGUESTLIB_ERROR_UNSUPPORTED_VERSION;
   }

   memcpy(outStat, &stats->stats[statIdx], sizeof *outStat);
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         cpuReservationMHz, cpuReservationMHz,
                         GUESTLIB_CPU_RESERVATION_MHZ);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         cpuLimitMHz, cpuLimitMHz,
                         GUESTLIB_CPU_LIMIT_MHZ);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         cpuShares, cpuShares,
                         GUESTLIB_CPU_SHARES);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         cpuUsedMs, cpuUsedMs,
                         GUESTLIB_CPU_USED_MS);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         mhz, hostMHz,
                         GUESTLIB_HOST_MHZ);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memReservationMB, memReservationMB,
                         GUESTLIB_MEM_RESERVATION_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memLimitMB, memLimitMB,
                         GUESTLIB_MEM_LIMIT_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memShares, memShares,
                         GUESTLIB_MEM_SHARES);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memMappedMB, memMappedMB,
                         GUESTLIB_MEM_MAPPED_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memActiveMB, memActiveMB,
                         GUESTLIB_MEM_ACTIVE_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memOverheadMB, memOverheadMB,
                         GUESTLIB_MEM_OVERHEAD_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memBalloonedMB, memBalloonedMB,
                         GUESTLIB_MEM_BALLOONED_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                                memSwappedMB, memSwappedMB,
                                GUESTLIB_MEM_SWAPPED_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memSharedMB, memSharedMB,
                         GUESTLIB_MEM_SHARED_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memSharedSavedMB, memSharedSavedMB,
                         GUESTLIB_MEM_SHARED_SAVED_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         memUsedMB, memUsedMB,
                         GUESTLIB_MEM_USED_MB);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETFN_BODY(handle, error,
                         elapsedMs, elapsedMs,
                         GUESTLIB_ELAPSED_MS);
   return error;
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
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;

   do {
      size_t size;
      if (NULL == handle) {
         error = VMGUESTLIB_ERROR_INVALID_HANDLE;
         break;
      }

      if (NULL == bufferSize || NULL == pathBuffer) {
         error = VMGUESTLIB_ERROR_INVALID_ARG;
         break;
      }

      if (0 == HANDLE_SESSIONID(handle)) {
         error = VMGUESTLIB_ERROR_NO_INFO;
         break;
      }

      if (HANDLE_VERSION(handle) == 2) {
         VMGuestLibDataV2 *dataV2;

         dataV2 = (VMGuestLibDataV2 *)HANDLE_DATA(handle);

         if (!dataV2->resourcePoolPath.valid) {
            error = VMGUESTLIB_ERROR_NOT_AVAILABLE;
            break;
         }

         /*
          * Check buffer size. It's safe to use strlen because we squash the
          * final byte of resourcePoolPath in VMGuestLibUpdateInfo.
          */
         size = strlen(dataV2->resourcePoolPath.value) + 1;
         if (*bufferSize < size) {
            *bufferSize = size;
            error = VMGUESTLIB_ERROR_BUFFER_TOO_SMALL;
            break;
         }

         memcpy(pathBuffer, dataV2->resourcePoolPath.value, size);
         error = VMGUESTLIB_ERROR_SUCCESS;
      } else if (HANDLE_VERSION(handle) == 3) {
         VMGuestLibStatisticsV3 *stats = HANDLE_DATA(handle);
         GuestLibV3Stat *stat = &stats->stats[GUESTLIB_RESOURCE_POOL_PATH - 1];

         if (!stat->GuestLibV3Stat_u.resourcePoolPath.valid) {
            error = VMGUESTLIB_ERROR_NOT_AVAILABLE;
            break;
         }

         size = strlen(stat->GuestLibV3Stat_u.resourcePoolPath.value) + 1;
         if (*bufferSize < size) {
            *bufferSize = size;
            error = VMGUESTLIB_ERROR_BUFFER_TOO_SMALL;
            break;
         }
         memcpy(pathBuffer, stat->GuestLibV3Stat_u.resourcePoolPath.value, size);
         error = VMGUESTLIB_ERROR_SUCCESS;
      }
   } while (0);

   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetCpuStolenMs --
 *
 *      Retrieve CPU stolen.
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
VMGuestLib_GetCpuStolenMs(VMGuestLibHandle handle, // IN
                          uint64 *cpuStolenMs)     // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         cpuStolenMs, cpuStolenMs,
                         GUESTLIB_CPU_STOLEN_MS);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemTargetSizeMB --
 *
 *      Retrieve Memory Target Size.
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
VMGuestLib_GetMemTargetSizeMB(VMGuestLibHandle handle, // IN
                              uint64 *memTargetSizeMB) // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memTargetSizeMB, memTargetSizeMB,
                         GUESTLIB_MEM_TARGET_SIZE_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostNumCpuCores --
 *
 *      Number of physical CPU cores on the host machine.
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
VMGuestLib_GetHostNumCpuCores(VMGuestLibHandle handle, // IN
                              uint32 *hostNumCpuCores) // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostNumCpuCores, hostCpuNumCores,
                         GUESTLIB_HOST_CPU_NUM_CORES);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostCpuUsedMs --
 *
 *      Total CPU time used by host.
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
VMGuestLib_GetHostCpuUsedMs(VMGuestLibHandle handle,  // IN
                            uint64 *hostCpuUsedMs)    // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostCpuUsedMs, hostCpuUsedMs,
                         GUESTLIB_HOST_CPU_USED_MS);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemSwappedMB --
 *
 *      Total swapped out memory on the host.
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
VMGuestLib_GetHostMemSwappedMB(VMGuestLibHandle handle,  // IN
                               uint64 *hostMemSwappedMB) // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemSwappedMB, hostMemSwappedMB,
                         GUESTLIB_HOST_MEM_SWAPPED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemSharedMB --
 *
 *      Total COW (Copy-On-Write) memory on host.
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
VMGuestLib_GetHostMemSharedMB(VMGuestLibHandle handle, // IN
                              uint64 *hostMemSharedMB) // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemSharedMB, hostMemSharedMB,
                         GUESTLIB_HOST_MEM_SHARED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemUsedMB --
 *
 *      Total consumed memory on host.
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
VMGuestLib_GetHostMemUsedMB(VMGuestLibHandle handle,  // IN
                            uint64 *hostMemUsedMB)    // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemUsedMB, hostMemUsedMB,
                         GUESTLIB_HOST_MEM_USED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemPhysMB --
 *
 *      Total memory available to host OS kernel.
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
VMGuestLib_GetHostMemPhysMB(VMGuestLibHandle handle,  // IN
                            uint64 *hostMemPhysMB)    // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemPhysMB, hostMemPhysMB,
                         GUESTLIB_HOST_MEM_PHYS_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemPhysFreeMB --
 *
 *      Total phsyical memory free on host.
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
VMGuestLib_GetHostMemPhysFreeMB(VMGuestLibHandle handle,    // IN
                                uint64 *hostMemPhysFreeMB)  // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemPhysFreeMB, hostMemPhysFreeMB,
                         GUESTLIB_HOST_MEM_PHYS_FREE_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemUsedMB --
 *
 *      Total host kernel memory overhead.
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
VMGuestLib_GetHostMemKernOvhdMB(VMGuestLibHandle handle,     // IN
                                uint64 *hostMemKernOvhdMB)   // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemKernOvhdMB, hostMemKernOvhdMB,
                         GUESTLIB_HOST_MEM_KERN_OVHD_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemMappedMB --
 *
 *      Total mapped memory on host.
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
VMGuestLib_GetHostMemMappedMB(VMGuestLibHandle handle,  // IN
                              uint64 *hostMemMappedMB)  // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemMappedMB, hostMemMappedMB,
                         GUESTLIB_HOST_MEM_MAPPED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetHostMemUnmappedMB --
 *
 *      Total unmapped memory on host.
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
VMGuestLib_GetHostMemUnmappedMB(VMGuestLibHandle handle,    // IN
                                uint64 *hostMemUnmappedMB)  // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         hostMemUnmappedMB, hostMemUnmappedMB,
                         GUESTLIB_HOST_MEM_UNMAPPED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemZippedMB --
 *
 *      Total zipped VM memory
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
VMGuestLib_GetMemZippedMB(VMGuestLibHandle handle,    // IN
                          uint32 *memZippedMB)        // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memZippedMB, memZippedMB,
                         GUESTLIB_MEM_ZIPPED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemZipSavedMB --
 *
 *      Total memopry saved by zipping VM memory
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
VMGuestLib_GetMemZipSavedMB(VMGuestLibHandle handle,    // IN
                            uint32 *memZipSavedMB)      // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memZipSavedMB, memZipSavedMB,
                         GUESTLIB_MEM_ZIPSAVED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemLLSwappedMB --
 *
 *      VM memory swapped to SSD
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
VMGuestLib_GetMemLLSwappedMB(VMGuestLibHandle handle,    // IN
                             uint32 *memLLSwappedMB)     // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memLLSwappedMB, memLLSwappedMB,
                         GUESTLIB_MEM_LLSWAPPED_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemSwapTargetMB --
 *
 *      VM memory swap target
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
VMGuestLib_GetMemSwapTargetMB(VMGuestLibHandle handle,    // IN
                              uint32 *memSwapTargetMB)    // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memSwapTargetMB, memSwapTargetMB,
                         GUESTLIB_MEM_SWAP_TARGET_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemBalloonTargetMB --
 *
 *      VM memory balloon target size
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
VMGuestLib_GetMemBalloonTargetMB(VMGuestLibHandle handle,    // IN
                                 uint32 *memBalloonTargetMB) // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memBalloonTargetMB, memBalloonTargetMB,
                         GUESTLIB_MEM_BALLOON_TARGET_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemBalloonMaxMB --
 *
 *      VM memory balloon limit
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
VMGuestLib_GetMemBalloonMaxMB(VMGuestLibHandle handle,    // IN
                              uint32 *memBalloonMaxMB)    // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memBalloonMaxMB, memBalloonMaxMB,
                         GUESTLIB_MEM_BALLOON_MAX_MB);
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_GetMemShares64 --
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
VMGuestLib_GetMemShares64(VMGuestLibHandle handle, // IN
                          uint64 *memShares64)     // OUT
{
   VMGuestLibError error = VMGUESTLIB_ERROR_OTHER;
   VMGUESTLIB_GETSTAT_V3(handle, error,
                         memShares64, memShares64,
                         GUESTLIB_MEM_SHARES_64);
   if (VMGUESTLIB_ERROR_UNSUPPORTED_VERSION == error) {
      /*
       * GUESTLIB_MEM_SHARES_64 is available only from ESX 7.0.
       * If the host is older, then return the value of GUESTLIB_MEM_SHARES.
       */
      uint32 memShares = 0;
      if (VMGUESTLIB_ERROR_SUCCESS !=
         VMGuestLib_GetMemShares(handle, &memShares)) {
         return error;
      } else {
         *memShares64 = memShares;
         return VMGUESTLIB_ERROR_SUCCESS;
      }
   }
   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLibIoctl --
 *
 *      Marshal and invoke the guestlib ioctl.
 *
 * Results:
 *      TRUE on success, FALSE otherwise (reply contains error detail, if
 *      provided)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VMGuestLibIoctl(const GuestLibIoctlParam *param,
                char **reply,
                size_t *replySize)
{
   XDR xdrs;
   Bool ret;
   static const char *request = VMGUESTLIB_IOCTL_COMMAND_STRING " ";

   if (param == NULL || param->d >= GUESTLIB_IOCTL_MAX) {
      return FALSE;
   }
   if (NULL == DynXdr_Create(&xdrs)) {
      return FALSE;
   }
   if (!DynXdr_AppendRaw(&xdrs, request, strlen(request)) ||
       !xdr_GuestLibIoctlParam(&xdrs, (GuestLibIoctlParam *)param)) {
      DynXdr_Destroy(&xdrs, TRUE);
      return FALSE;
   }
   ret = RpcChannel_SendOneRaw(DynXdr_Get(&xdrs), xdr_getpos(&xdrs),
                               reply, replySize);
   DynXdr_Destroy(&xdrs, TRUE);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_AtomicUpdateCookie --
 *
 *      Atomically update a cookie on the host.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMGuestLib_AtomicUpdateCookie(const char *src,    // IN
                              const char *dst,    // IN
                              char **reply,       // OUT
                              size_t *replySize)  // OUT
{
   GuestLibIoctlParam param;

   ASSERT(src != NULL);
   ASSERT(dst != NULL);

   param.d = GUESTLIB_IOCTL_ATOMIC_UPDATE_COOKIE;
   param.GuestLibIoctlParam_u.atomicUpdateCookie.src = (char *)src;
   param.GuestLibIoctlParam_u.atomicUpdateCookie.dst = (char *)dst;
   return VMGuestLibIoctl(&param, reply, replySize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_StatGet --
 *
 *      Fetches semi-structured stat information from the hypervisor.
 *      Pass to VMGuestLib_StatFree() to release.
 *
 *      Encodings: 'json', 'text', 'xml', 'yaml'
 *      Stats: pass NULL or "" to receive a list of available stats
 *      Some stats are two words, e.g. "vscsi scsi0:0"
 *
 *      NOTE: stats, their meaning and availability may change from release
 *      to release as the underlying implementation evolves.
 *      This information is intended for troubleshooting only!
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
VMGuestLib_StatGet(const char *encoding,  // IN
                   const char *stat,      // IN
                   char **reply,          // OUT
                   size_t *replySize)     // OUT
{
   char commandBuf[256];
   int bufRet;
   Bool ret;

   if (encoding == NULL) {
      return VMGUESTLIB_ERROR_INVALID_ARG;
   }
   if (stat == NULL) {
      stat = "";  // Dummy value
   }

   /*
    * Construct command string with the command name and the name
    * of the stat that we want.
    */
   bufRet = Str_Snprintf(commandBuf, sizeof commandBuf, "%s %s %s",
                         VMGUESTLIB_STATDATA_COMMAND_STRING, encoding, stat);
   if (bufRet == -1 || bufRet >= sizeof commandBuf) {
      return VMGUESTLIB_ERROR_BUFFER_TOO_SMALL;
   }

   ret = RpcChannel_SendOneRaw(commandBuf, strlen(commandBuf),
                               reply, replySize);

   return ret ? VMGUESTLIB_ERROR_SUCCESS : VMGUESTLIB_ERROR_NOT_AVAILABLE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * VMGuestLib_StatFree --
 *
 *      Frees a stat fetched by VMGuestLib_StatGet()
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
VMGuestLib_StatFree(char *reply,       // IN
                    size_t replySize)  // IN
{
   free(reply);
}
