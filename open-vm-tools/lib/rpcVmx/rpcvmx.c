/*********************************************************
 * Copyright (c) 2004-2018,2019,2021,2023 VMware, Inc. All rights reserved.
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

#if defined(__KERNEL__) || defined(_KERNEL) || defined(KERNEL)
#   include "kernelStubs.h"
#else
#   include <stdio.h>
#   include <stdarg.h>
#   include <string.h>
#   include <stdlib.h>
#   include "str.h"
#endif

#include "guest_msg_def.h"
#include "message.h"
#include "rpcout.h"
#include "rpcvmx.h"


typedef struct {
   char logBuf[RPCVMX_MAX_LOG_LEN + sizeof "log"];
   unsigned int logOffset;
} RpcVMXState;

static RpcVMXState RpcVMX = { "log ", sizeof "log" };


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_LogSetPrefix
 *
 *      Allows callers to set a prefix to prepend to the log output. If the
 *      prefix overflows the (static) prefix space available, it is rejected
 *      and the prefix is reset to nothing.  Each call to VMXLog_SetPrefix
 *      replaces the previously set prefix.
 *
 * Results:
 *      TRUE if the prefix was accepted, FALSE otherwise.
 *
 * Side effects:
 *      All subsequent calls to RpcVMX_Log() will have the prefix string
 *      prepended.
 *
 *----------------------------------------------------------------------------
 */

void
RpcVMX_LogSetPrefix(const char *prefix)
{
   size_t prefixLen = strlen(prefix);

   if (prefixLen + sizeof "log" >= sizeof RpcVMX.logBuf - 1) {
      /*
       * Somebody passed a huge prefix. Don't do that!
       */
      RpcVMX.logOffset = sizeof "log";
      return;
   }
   Str_Strcpy(RpcVMX.logBuf + sizeof "log",
              prefix,
              sizeof RpcVMX.logBuf - sizeof "log");

   RpcVMX.logOffset = (unsigned int)(sizeof "log" + prefixLen);
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_LogGetPrefix --
 *
 *      Returns a read-only pointer to the currently set prefix string. The
 *      client should make a copy if it wants to e.g. save the previous
 *      prefix and then restore it.
 *
 * Results:
 *      Current prefix string, empty string if no prefix is set.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

const char *
RpcVMX_LogGetPrefix(const char *prefix)
{
   UNUSED_VARIABLE(prefix);

   RpcVMX.logBuf[RpcVMX.logOffset] = '\0';
   return RpcVMX.logBuf + sizeof "log";
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_Log --
 *
 *      Passes through to RpcVMX_LogV but takes arguments inline.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See RpcVMX_LogV.
 *
 *----------------------------------------------------------------------------
 */

void
RpcVMX_Log(const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   RpcVMX_LogV(fmt, args);
   va_end(args);
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_LogV --
 *
 *      Construct an output string using the provided format string and
 *      argument list, then send it to the VMX using the RPCI "log" command.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Sends the log command described above.
 *
 *----------------------------------------------------------------------------
 */

void
RpcVMX_LogV(const char *fmt, va_list args)
{
   int payloadLen;
   char receiveBuffer[16];

   payloadLen = Str_Vsnprintf(RpcVMX.logBuf + RpcVMX.logOffset,
                              sizeof RpcVMX.logBuf - RpcVMX.logOffset,
                              fmt, args);

   if (payloadLen < 1) {
      /*
       * Overflow. We need more space in the buffer. Just set the length to
       * the buffer size and send the (truncated) log message.
       */
      payloadLen = sizeof RpcVMX.logBuf - RpcVMX.logOffset;
   }

   /*
    * Use a pre-allocated receive buffer so that it's possible to
    * perform the log without needing to call malloc.  This makes
    * RpcVMX_LogV suitable to be used in Windows kernel interrupt
    * handlers.  (It also makes it faster.)  The log command only ever
    * returns two character strings "1 " on success and "0 " on
    * failure, so we don't need a sizeable buffer.
    */
   RpcOut_SendOneRawPreallocated(RpcVMX.logBuf,
                                 (size_t)RpcVMX.logOffset + payloadLen,
                                 receiveBuffer, sizeof receiveBuffer);
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_ConfigGetString --
 *
 *      Look up a config variable in the VMX's config file and return its
 *      value as a string. The caller should free the string using free() when
 *      done.
 *
 * Results:
 *      The value of the variable if it was set, or a copy of the default
 *      value string if the variable was not set.
 *
 * Side effects:
 *      Allocates memory which the caller must free().
 *
 *----------------------------------------------------------------------------
 */

char *
RpcVMX_ConfigGetString(const char *defval, const char *var)
{
   char *value;
   if (!RpcOut_sendOne(&value, NULL, "info-get guestinfo.%s", var)) {
      /* Get rid of the old value first. */
      free(value);
      value = NULL;

      if (defval) {
         /*
          * We have to dup the default, because of our contract: values we
          * return must always be freed by the caller.
          */
#if defined(_WIN32) && defined(USERLEVEL)
         value = _strdup(defval);
#else
         value = strdup(defval);
#endif
      }
   }

   return value;
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_ConfigGetBool --
 *
 *      Same as RpcVMX_ConfigGetString, but convert the value to a boolean
 *      and return it.
 *
 * Results:
 *      Value of config variable as a Bool, or the default value if the config
 *      variable was not set *or* could not be converted to a Bool.
 *
 * Side effects:
 *      Perturbs the heap.
 *
 *----------------------------------------------------------------------------
 */

Bool
RpcVMX_ConfigGetBool(Bool defval, const char *var)
{
   char *value = RpcVMX_ConfigGetString(NULL, var);
   Bool ret = defval;

   if (value) {
      if (!Str_Strcasecmp(value, "TRUE")) {
         ret = TRUE;
      } else if (!Str_Strcasecmp(value, "FALSE")) {
         ret = FALSE;
      }

      free(value);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_ConfigGetLong --
 *
 *      Same as RpcVMX_ConfigGetString, but convert the value to an integer.
 *      XXX We use atoi, so there's no error checking. If you care, use
 *          RpcVMX_ConfigGetString and do the conversion yourself.
 *
 * Results:
 *      The value of the config variable as a 32-bit integer if it was set,
 *      the default value if it was not set, and 0 if there was an error
 *      converting the value to an integer.
 *
 * Side effects:
 *      Perturbs the heap.
 *
 *----------------------------------------------------------------------------
 */

int32
RpcVMX_ConfigGetLong(int32 defval, const char *var)
{
   char *value = RpcVMX_ConfigGetString(NULL, var);
   int32 ret = defval;

   if (value) {
      ret = atoi(value);
      free(value);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_ReportDriverVersion --
 *
 *      Report driver name and driver version to vmx to store the key-value in
 *      GuestVars, and write a log in vmware.log using RpcVMX_Log.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
RpcVMX_ReportDriverVersion(const char *drivername, const char *versionString)
{
   char setVersionCmd[128];
   Str_Sprintf(setVersionCmd, sizeof(setVersionCmd),
               "info-set guestinfo.driver.%s.version %s",
               drivername, versionString);
   RpcOut_sendOne(NULL, NULL, setVersionCmd);
   RpcVMX_Log("Driver=%s, Version=%s", drivername, versionString);
}
