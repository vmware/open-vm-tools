/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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

/**
 * @file vmGuestAppMonitorLib.c
 *
 * @brief  This file contains the functions used by an application monitoring
 *         agent to communicate to HA via VMX that the application(s) that it
 *         is monitoring is alive. The general flow is as follows:
 *
 *  @code
 *  VMGuestAppMonitor_Enable();
 *
 *  -- Call at least every 30 seconds
 *  VMGuestAppMonitor_MarkActive();
 *
 *  -- When finished monitoring
 *  VMGuestAppMonitor_Disable();
 *  @endcode
 *
 *  To signal an application failure, simply do not call
 *  VMGuestAppMonitor_MarkActive().
 *
 *  @endcode
 *
 *  @addtogroup VMGuestAppMonitor
 *  @{
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vmware.h"
#include "str.h"
#include "util.h"
#include "vmGuestAppMonitorLib.h"
#include "vmGuestAppMonitorLibInt.h"
#include "rpcout.h"
#include "vmcheck.h"
#include "debug.h"
#include "vmguestappmonitorlib_version.h"
#include "vm_version.h"
#include "embed_version.h"
#include "vmware/tools/guestrpc.h"

VM_EMBED_VERSION(VMGUESTAPPMONITORLIB_VERSION_STRING);

/**
 * @cond INTERNAL
 * @{
 */

static VMGuestAppMonitorLibError
RunGuestAppMonitorCmd(const char *cmd);

static VMGuestAppMonitorLibError
RunGuestAppMonitorCmdWithResult(const char *cmd,
                                char **result);

static Bool
CreateSecRpcChannel();

static void
DestroySecRpcChannel();

static void
DestroyChannel();

#if defined(VMX86_DEBUG) && defined(linux)
static void
LogChannelType(const char *filePath, const char *chanType);
#endif

static RpcChannel *gChan = NULL;

static Bool isHeartbeatingOnly = FALSE;

/**
 * @}
 * @endcond
 */

/*
 ******************************************************************************
 * VMGuestAppMonitor_Enable --
 ******************************************************************************
 */

VMGuestAppMonitorLibError
VMGuestAppMonitor_Enable(void)
{
   VMGuestAppMonitorLibError rc;
   isHeartbeatingOnly = TRUE;
   rc = RunGuestAppMonitorCmd(VMGUESTAPPMONITOR_BD_CMD_ENABLE);
   return rc;
}


/*
 ******************************************************************************
 * VMGuestAppMonitor_Disable --
 ******************************************************************************
 */

VMGuestAppMonitorLibError
VMGuestAppMonitor_Disable(void)
{
   VMGuestAppMonitorLibError rc;
   rc = RunGuestAppMonitorCmd(VMGUESTAPPMONITOR_BD_CMD_DISABLE);
   if (rc == VMGUESTAPPMONITORLIB_ERROR_SUCCESS) {
      DestroySecRpcChannel();
      gChan = NULL;
      Debug("Destroyed the secure rpc channel.\n");
      isHeartbeatingOnly = FALSE;
   }
   return rc;
}


/*
 ******************************************************************************
 * VMGuestAppMonitor_IsEnabled --
 ******************************************************************************
 */

int
VMGuestAppMonitor_IsEnabled(void)
{
   VMGuestAppMonitorLibError rc;
   char *status = NULL;

   rc = RunGuestAppMonitorCmdWithResult(VMGUESTAPPMONITOR_BD_CMD_IS_ENABLED,
                                        &status);

   if (rc == VMGUESTAPPMONITORLIB_ERROR_SUCCESS && status != NULL) {
      Bool isEnabled = Str_Strncmp(status, "true", sizeof("true")) == 0;

      free(status);
      return isEnabled;
   } else {
      return FALSE;
   }
}


/*
 ******************************************************************************
 * VMGuestAppMonitor_MarkActive --
 ******************************************************************************
 */

VMGuestAppMonitorLibError
VMGuestAppMonitor_MarkActive(void)
{
   return RunGuestAppMonitorCmd(VMGUESTAPPMONITOR_BD_CMD_MARK_ACTIVE);
}


/*
 ******************************************************************************
 * VMGuestAppMonitor_GetAppStatus --
 ******************************************************************************
 */

char *
VMGuestAppMonitor_GetAppStatus(void)
{
   VMGuestAppMonitorLibError rc;
   char *status = NULL;

   rc = RunGuestAppMonitorCmdWithResult(VMGUESTAPPMONITOR_BD_CMD_GET_APP_STATUS,
                                        &status);

   return status;
}

/*
 ******************************************************************************
 * VMGuestAppMonitor_PostAppState --
 ******************************************************************************
 */
VMGuestAppMonitorLibError
VMGuestAppMonitor_PostAppState(const char *state)
{
   char *cmd = Str_Asprintf(NULL, "%s %s",
                            VMGUESTAPPMONITOR_BD_CMD_POST_APP_STATE,
                            state);
   VMGuestAppMonitorLibError rc;

   rc = RunGuestAppMonitorCmd(cmd);
   free(cmd);

   return rc;
}

/*
 ******************************************************************************
 * VMGuestAppMonitor_Free --                                            */ /**
 ******************************************************************************
 */

void
VMGuestAppMonitor_Free(char *str)
{
   if (str != NULL) {
      free(str);
   }
}

/*
 ******************************************************************************
 * DestroyChannel --                                             */ /**
 *
 * Destroy the channel if conditions are met.
 * Do not destroy the channel otherwise.
 *
 ******************************************************************************
 */
static void
DestroyChannel()
{
   Bool isBackdoorChannel;
  /*
   * Destroying the channel here if a client is using anything other
   * than markActive. Channels using markActive need to be persisted
   * since we want to prevent any false VM resets triggered by a client
   * not being able to post heartbeats because it was not able to
   * acquire a channel. We also close and destroy all backdoor channels
   * irrespective of whether they are being used for markActive or not.
   */
   isBackdoorChannel = (RpcChannel_GetType(gChan) == RPCCHANNEL_TYPE_BKDOOR);
   Debug("isBackdoorChannel is set to %d.\n", isBackdoorChannel);
   Debug("isHeartbeatingOnly is set to %d.\n", isHeartbeatingOnly);
   if(!isHeartbeatingOnly || isBackdoorChannel) {
      DestroySecRpcChannel();
      gChan = NULL;
      Debug("Destroyed the secure rpc channel.\n");
   }
}

/*
 ******************************************************************************
 * CreateSecRpcChannel --                                             */ /**
 *
 * Create a new secure RpcChannel if not already created.
 *
 * @return  TRUE if successful or FALSE otherwise.
 *
 ******************************************************************************
 */
static Bool
CreateSecRpcChannel()
{
   Bool start;
   if(gChan == NULL) {
      Debug("VMGuestAppMonitor: Creating a new Secure Rpc channel.\n");
      gChan = RpcChannel_New();
      start = RpcChannel_Start(gChan);
      /*
       * Logging the channel type so we can verify the same via the
       * VMAppmon unit tests. Changing this log message/file path
       * will break the unit tests.
       */
      #if defined(VMX86_DEBUG) && defined(linux)
      LogChannelType("/tmp/chanType.txt", (RpcChannel_GetType(gChan) == RPCCHANNEL_TYPE_BKDOOR ? "BACKDOOR" : "VSOCK"));
      #endif
      return start;
   }
   else {
      Debug("VMGuestAppMonitor: Secure Rpc channel already present.\n");
      return TRUE;
   }
}

/*
 ******************************************************************************
 * DestroySecRpcChannel --                                               */ /**
 *
 * Destroy a secure RpcChannel only if it has been created.
 *
 ******************************************************************************
 */
static void
DestroySecRpcChannel()
{
   if(gChan != NULL) {
      RpcChannel_Destroy(gChan);
   }
}

#if defined(VMX86_DEBUG) && defined(linux)
/*
 ******************************************************************************
 * LogChannelType --                                               */ /**
 *
 * Open a file on linux and log the channel type.
 *
 ******************************************************************************
 */
static void
LogChannelType(const char *filePath, const char *chanType)
{
   FILE *fp = fopen(filePath, "wb");
   if (fp != NULL)
   {
      fputs(chanType, fp);
      fclose(fp);
   }
}
#endif

/*
 ******************************************************************************
 * RunGuestAppMonitorCmd --                                             */ /**
 *
 * Execute a Guest App Monitoring RPC.
 *
 * @param[in]  cmd     The RPC command.
 *
 * @return  VMGUESTAPPMONITORLIB_ERROR_SUCCESS if successful or
 *          the error code otherwise.
 *
 ******************************************************************************
 */
static VMGuestAppMonitorLibError
RunGuestAppMonitorCmd(const char *cmd)
{
   char *reply = NULL;
   size_t replyLen;
   VMGuestAppMonitorLibError rc;

   ASSERT(cmd != NULL);

   if (!VmCheck_IsVirtualWorld()) {
      Debug("VMGuestAppMonitor: Not running in a VM.\n");
      return VMGUESTAPPMONITORLIB_ERROR_NOT_RUNNING_IN_VM;
   }

   //Init channel here if not already init
   if (!CreateSecRpcChannel()) {
      Debug("Error starting the Rpc Channel\n");
      return VMGUESTAPPMONITORLIB_ERROR_NOT_ENABLED;
   }
   Debug("VMGuestAppMonitor: Sending via secure Rpc channel.\n");
   if (!RpcChannel_Send(gChan, cmd, strlen(cmd), &reply, &replyLen)) {
      Debug("Failed to run %s command: %s\n", cmd, reply ? reply : "NULL");

      if (Str_Strncmp(reply, "Unknown command",
                      sizeof "Unknown command") == 0) {
         /* Host does not support application monitoring */
         rc = VMGUESTAPPMONITORLIB_ERROR_NOT_SUPPORTED;
      } else {
         rc = VMGUESTAPPMONITORLIB_ERROR_OTHER;
      }
   } else {
      if (Str_Strncmp(reply, VMGUESTAPPMONITOR_BD_RC_OK,
                      sizeof VMGUESTAPPMONITOR_BD_RC_OK) == 0) {
         rc = VMGUESTAPPMONITORLIB_ERROR_SUCCESS;
      } else {
         rc = VMGUESTAPPMONITORLIB_ERROR_OTHER;
      }
   }

   if (reply) {
      free(reply);
   }

   DestroyChannel();

   return rc;
}


/*
 ******************************************************************************
 * RunGuestAppMonitorCmdWithResult --                                   */ /**
 *
 * Execute a Guest App Monitoring RPC and return the command's result.
 *
 * @param[in]   cmd     The RPC command.
 * @param[out]  result  The result returned by the command or NULL if an
 *                      error occurred..
 *
 * @return  VMGUESTAPPMONITORLIB_ERROR_SUCCESS if successful or
 *          the error code otherwise.
 *
 ******************************************************************************
 */

static VMGuestAppMonitorLibError
RunGuestAppMonitorCmdWithResult(const char *cmd,
                                char **result)
{
   char *reply = NULL;
   size_t replyLen;
   VMGuestAppMonitorLibError rc;

   ASSERT(cmd != NULL);
   ASSERT(result != NULL);

   if (!VmCheck_IsVirtualWorld()) {
      Debug("VMGuestAppMonitor: Not running in a VM.\n");
      return VMGUESTAPPMONITORLIB_ERROR_NOT_RUNNING_IN_VM;
   }

   //Init channel here if not already init
   if (!CreateSecRpcChannel()) {
      Debug("Error starting the Rpc Channel\n");
      return VMGUESTAPPMONITORLIB_ERROR_NOT_ENABLED;
   }
   Debug("VMGuestAppMonitor: Sending via secure Rpc channel.\n");
   if (!RpcChannel_Send(gChan, cmd, strlen(cmd), &reply, &replyLen)) {
      Debug("Failed to run %s command: %s\n", cmd,  reply ? reply : "NULL");

      if (Str_Strncmp(reply, "Unknown command",
                      sizeof "Unknown command") == 0) {
         /* Host does not support application monitoring */
         rc = VMGUESTAPPMONITORLIB_ERROR_NOT_SUPPORTED;
      } else {
         rc = VMGUESTAPPMONITORLIB_ERROR_OTHER;
      }
   } else {
      Debug("Ran %s command, Reply is %s\n", cmd, reply ? reply : "NULL");
      rc = VMGUESTAPPMONITORLIB_ERROR_SUCCESS;
      if (replyLen > 0) {
         *result = Util_SafeMalloc(replyLen + 1);
         Str_Strcpy(*result, reply, replyLen + 1);
      } else {
         *result = NULL;
      }
   }

   if (reply) {
      free(reply);
   }

   DestroyChannel();

   return rc;
}


/**
 * @}
 */
