/*********************************************************
 * Copyright (c) 2004-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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


/*
 * Global shared buffer state used to back RPCI log calls made through
 * RpcVMX_Log and RpcVMX_LogV.
 */
static char gRpcVmxLogBackingBuffer[RPCVMX_DEFAULT_LOG_BUFSIZE] = "log ";
static RpcVMXLogBuffer gRpcVmxLog = { &gRpcVmxLogBackingBuffer[0],
                                      RPCVMX_DEFAULT_LOG_BUFSIZE,
                                      sizeof "log" };

static Bool RpcVMXBufferSetPrefix(char *logBackingBuffer,
                                  unsigned int logBackingBufferSizeBytes,
                                  const char *prefix,
                                  unsigned int *logOffsetOut);


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_LogSetPrefix
 *
 *      Allows callers to set a prefix to prepend to the log output, for calls
 *      to RpcVMX_Log and RpcVMX_LogV. If the prefix overflows the (static)
 *      prefix space available, it is rejected and the prefix is reset to
 *      nothing.  Each call to VMXLog_SetPrefix replaces the previously set
 *      prefix.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      All subsequent calls to RpcVMX_Log() and RpcVMX_LogV() will have the
 *      prefix string prepended.
 *
 *----------------------------------------------------------------------------
 */

void
RpcVMX_LogSetPrefix(const char *prefix)
{
   RpcVMXBufferSetPrefix(gRpcVmxLog.logBuf, gRpcVmxLog.logBufSizeBytes, prefix,
                         &gRpcVmxLog.logOffset);
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

   gRpcVmxLog.logBuf[gRpcVmxLog.logOffset] = '\0';
   return gRpcVmxLog.logBuf + sizeof "log";
}


/*
 *---------------------------------------------------------------------------
 *
 * RpcVMXBufferSetPrefix --
 *
 *    Internal helper function to set the prefix string for a log buffer.
 *
 *  Results:
 *    Returns TRUE iff the prefix was successfully set.
 *
 *    On success, writes the buffer index immediately following "log {PREFIX}"
 *    to *logOffsetOut.
 *
 * Side effects:
 *      All subsequent calls to the RpcVMX_Log* functions using the new
 *      backing buffer will have the prefix string prepended.
 *
 *---------------------------------------------------------------------------
 */

static Bool
RpcVMXBufferSetPrefix(char *logBackingBuffer,                  // OUT
                      unsigned int logBackingBufferSizeBytes,  // IN
                      const char *prefix,                      // IN
                      unsigned int *logOffsetOut)              // OUT
{
   size_t prefixLen;

   if (logBackingBuffer == NULL || prefix == NULL || logOffsetOut == NULL) {
      return FALSE;
   }

   *logOffsetOut = 0;

   prefixLen = strlen(prefix);

   if (prefixLen + sizeof "log" >= logBackingBufferSizeBytes - 1) {
      return FALSE;
   }

   Str_Strcpy(logBackingBuffer + sizeof "log", prefix,
              logBackingBufferSizeBytes - sizeof "log");

   *logOffsetOut = (unsigned int)(sizeof "log" + prefixLen);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_InitLogBackingBuffer --
 *
 *      Initialize the given log buffer struct with the given caller-allocated
 *      backing buffer and prefix string.
 *
 * Results:
 *      Returns TRUE if the provided RpcVMXBuffer was initialized successfully,
 *      or FALSE if initialization failed and the RpcVMXBuffer should not be
 *      used.
 *
 * Side effects:
 *      All subsequent calls to the RpcVMX_Log* functions using the new
 *      backing buffer will have the prefix string prepended.
 *
 *----------------------------------------------------------------------------
 */

Bool
RpcVMX_InitLogBackingBuffer(RpcVMXLogBuffer *bufferOut,       // OUT
                            char *logBuf,                     // IN
                            unsigned int logBufSizeBytes,     // IN
                            const char *prefix)               // IN
{
   unsigned int prefixLogOffset = 0;

   if (bufferOut == NULL || logBuf == NULL || prefix == NULL ||
       logBufSizeBytes < sizeof "log ") {
      return FALSE;
   }

   bufferOut->logBuf = logBuf;
   bufferOut->logBufSizeBytes = logBufSizeBytes;

   memset(bufferOut->logBuf, 0, bufferOut->logBufSizeBytes);

   /*
    * Copy in the RPCI command prefix "log ".
    */
   Str_Strcpy(bufferOut->logBuf, "log ",
              logBufSizeBytes - sizeof "log ");
   bufferOut->logOffset = sizeof "log";

   /*
    * Copy in the provided logging prefix after the initial "log ".
    */
   if (RpcVMXBufferSetPrefix(bufferOut->logBuf, bufferOut->logBufSizeBytes,
                             prefix, &prefixLogOffset)) {
      bufferOut->logOffset = prefixLogOffset;
      return TRUE;
   }

   return FALSE;
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
RpcVMX_LogV(const char *fmt,
            va_list args)
{
   int payloadLen;
   char receiveBuffer[16];

   payloadLen = Str_Vsnprintf(gRpcVmxLog.logBuf + gRpcVmxLog.logOffset,
                              gRpcVmxLog.logBufSizeBytes - gRpcVmxLog.logOffset,
                              fmt, args);

   if (payloadLen < 1) {
      /*
       * Overflow. We need more space in the buffer. Just set the length to
       * the buffer size and send the (truncated) log message.
       */
      payloadLen = gRpcVmxLog.logBufSizeBytes - gRpcVmxLog.logOffset;
   }

   /*
    * Use a pre-allocated receive buffer so that it's possible to
    * perform the log without needing to call malloc.  This makes
    * RpcVMX_LogV suitable to be used in Windows kernel interrupt
    * handlers.  (It also makes it faster.)  The log command only ever
    * returns two character strings "1 " on success and "0 " on
    * failure, so we don't need a sizeable buffer.
    */
   RpcOut_SendOneRawPreallocated(gRpcVmxLog.logBuf,
                                 (size_t)gRpcVmxLog.logOffset + payloadLen,
                                 receiveBuffer, sizeof receiveBuffer);
}


/*
 *----------------------------------------------------------------------------
 *
 * RpcVMX_LogVWithBuffer --
 *
 *      Construct an output string using the provided format string and
 *      argument list, then send it to the VMX using the RPCI "log" command.
 *
 *      Uses the caller-provided buffer to back the log, rather than the
 *      global gRpcVmxLog.
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
RpcVMX_LogVWithBuffer(RpcVMXLogBuffer *rpcBuffer,                 // IN/OUT
                      const char *fmt,                            // IN
                      va_list args)                               // IN
{
   int payloadLen;
   char receiveBuffer[16];

   if (rpcBuffer == NULL || fmt == NULL) {
      return;
   }

   if (rpcBuffer->logOffset >= rpcBuffer->logBufSizeBytes) {
      /*
       * The RpcVMXLogBuffer is not valid, because the prefix is taking up the
       * entire buffer.  Since we can't log any actual message, silently fail.
       */
      return;
   }

   payloadLen = Str_Vsnprintf(rpcBuffer->logBuf + rpcBuffer->logOffset,
                              rpcBuffer->logBufSizeBytes - rpcBuffer->logOffset,
                              fmt, args);

   if (payloadLen < 1) {
      /*
       * Overflow. We need more space in the buffer. Just set the length to
       * the buffer size and send the (truncated) log message.
       */
      payloadLen = rpcBuffer->logBufSizeBytes - rpcBuffer->logOffset;
   }

   /*
    * Use a pre-allocated receive buffer so that it's possible to
    * perform the log without needing to call malloc.  This makes
    * RpcVMX_LogVWithBuffer suitable to be used in Windows kernel interrupt
    * handlers.  (It also makes it faster.)  The log command only ever
    * returns two character strings "1 " on success and "0 " on
    * failure, so we don't need a sizeable buffer.
    */
   RpcOut_SendOneRawPreallocated(rpcBuffer->logBuf,
                                 (size_t)rpcBuffer->logOffset + payloadLen,
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
RpcVMX_ConfigGetString(const char *defval,
                       const char *var)
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
RpcVMX_ConfigGetBool(Bool defval,
                     const char *var)
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
RpcVMX_ConfigGetLong(int32 defval,
                     const char *var)
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
RpcVMX_ReportDriverVersion(const char *drivername,
                           const char *versionString)
{
   char setVersionCmd[128];
   Str_Sprintf(setVersionCmd, sizeof(setVersionCmd),
               "info-set guestinfo.driver.%s.version %s",
               drivername, versionString);
   RpcOut_sendOne(NULL, NULL, setVersionCmd);
   RpcVMX_Log("Driver=%s, Version=%s", drivername, versionString);
}
