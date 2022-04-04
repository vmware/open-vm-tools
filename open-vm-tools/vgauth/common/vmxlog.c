/*********************************************************
 * Copyright (C) 2018-2022 VMware, Inc. All rights reserved.
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
 * @file vmxlog.c
 *
 * Simple guest->VMX RPC log support that assumes VMCI is available.
 *
 */

#include <glib.h>
#include "vmxrpc.h"
#include "vmxlog.h"

#define LOG_RPC_CMD  "log"
#define LOG_RPC_CMD_NEW  "guest.log.text"

static gboolean gDisableVMXLogging = TRUE;

/*
 * Error codes for SendLogString()
 */
#define VMX_RPC_OK       1                    // success
#define VMX_RPC_UNKNOWN  0                    // RPC disabled or not supported
#define VMX_RPC_ERROR   -1                    // failed to send RPC

#define VMXLOG_SERVICE_NAME      "[vgauthservice]"


/*
 ******************************************************************************
 * VMXLog_Init --                                                        */ /**
 *
 * Initializes the VMX log facility.
 *
 * Returns -1 on error, 1 on success.
 *
 ******************************************************************************
 */

int
VMXLog_Init(void)
{
   gDisableVMXLogging = FALSE;
   return VMXRPC_Init();
}


/*
 ******************************************************************************
 * VMXLog_Shutdown --                                                    */ /**
 *
 * Shuts down the VMX log facility.
 *
 ******************************************************************************
 */

void
VMXLog_Shutdown(void)
{
   gDisableVMXLogging = TRUE;
}


/*
 ******************************************************************************
 * SendLogString --                                                      */ /**
 *
 * Sends a Log message to the VMX.
 *
 * @param[in] cmd       The message to send.
 *
 * Returns VMX_RPC_ERROR on failure, VMX_RPC_OK on success, VMX_RPC_UNKNOWN
 * if RPC failed (doesn't exist or disabled).
 ******************************************************************************
 */

static int
SendLogString(const gchar *cmd)
{
   gchar *reply = NULL;
   int ret;
   int retVal = VMX_RPC_OK;

   ret = VMXRPC_SendRpc(cmd, FALSE, &reply);
   if (ret >= 0) {
      if (g_strcmp0(reply, "disabled") == 0 ||
          g_strcmp0(reply, "Unknown command") == 0) {
         g_warning("%s: RPC unknown or disabled\n", __FUNCTION__);
         retVal = VMX_RPC_UNKNOWN;
      }
   } else {
      g_warning("%s: failed to send RPC packet\n", __FUNCTION__);
      retVal = VMX_RPC_ERROR;
   }

   g_free(reply);
   return retVal;
}


/*
 ******************************************************************************
 * VMXLog_LogV --                                                        */ /**
 *
 * Logs to the VMX using va_list arguments.
 *
 * @param[in] level       Logging level (currently unused).
 * @param[in] fmt         The format message for the event.
 * @param[in] args        The arguments for @a fmt.
 *
 ******************************************************************************
 */

void
VMXLog_LogV(int level,
            const char *fmt,
            va_list args)
{
   gchar *msg = NULL;
   gchar *cmd = NULL;
   int ret;
   // static gboolean useNewRpc = TRUE;
   // XXX the new RPC can quietly no-op on virtual hw < 17
   // is this fixable somehow, or should we just give up
   // on the new RPC completely?
   static gboolean useNewRpc = FALSE;
   static gboolean rpcBroken = FALSE;

   /*
    * RPCs don't work -- not in a VM or no vmci -- so drop any messages.
    */
   if (gDisableVMXLogging || rpcBroken) {
      return;
   }

   msg = g_strdup_vprintf(fmt, args);
again:
   /*
    * Try the new logging RPC, fail over to the old
    *
    * Possible optimization -- every N minutes, retry the new RPC in
    * case its been enabled dynamically.
    */
   if (useNewRpc) {
      /* XXX TODO use the level */
      cmd = g_strdup_printf("%s " VMXLOG_SERVICE_NAME " %s", LOG_RPC_CMD_NEW, msg);
   } else {
      cmd = g_strdup_printf("%s " VMXLOG_SERVICE_NAME " %s", LOG_RPC_CMD, msg);
   }

   ret = SendLogString(cmd);
   g_free(cmd);
   cmd = NULL;
   if ((ret == VMX_RPC_UNKNOWN) && useNewRpc) {
      g_debug("%s: new RPC Failed, using old\n", __FUNCTION__);
      useNewRpc = FALSE;
      goto again;
   } else if (ret == VMX_RPC_ERROR) {
      rpcBroken = TRUE;
      g_debug("%s: Error sending RPC, assume they aren't supported\n",
              __FUNCTION__);
   }

   g_free(msg);
   msg = NULL;
}


/*
 ******************************************************************************
 * VMXLog_Log --                                                        */ /**
 *
 * Logs to the VMX.
 *
 * @param[in] level       Logging level (currently unused).
 * @param[in] fmt         The format message for the event.
 * @param[in] args        The arguments for @a fmt.
 *
 ******************************************************************************
 */

void
VMXLog_Log(int level,
           const char *fmt,
           ...)
{
   va_list args;

   va_start(args, fmt);
   VMXLog_LogV(level, fmt, args);
   va_end(args);
}
