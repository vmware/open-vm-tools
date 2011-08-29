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

/**
 * @file toolsRpc.c
 *
 *    Functions related to the GuestRPC channel provided by the service.
 *    Provides the interface for the service to bring up the RPC channel,
 *    and handlers for the RPC messages which are handled by the service
 *    itself.
 */

#include <stdlib.h>
#include <string.h>
#include "vm_assert.h"
#include "conf.h"
#include "str.h"
#include "strutil.h"
#include "toolsCoreInt.h"
#include "vm_tools_version.h"
#include "vmware/tools/utils.h"


/**
 * Take action after an RPC channel reset.
 *
 * @param[in]  chan     The RPC channel.
 * @param[in]  success  Whether reset was successful.
 * @param[in]  _state   The service state.
 */

static void
ToolsCoreCheckReset(struct RpcChannel *chan,
                    gboolean success,
                    gpointer _state)
{
   ToolsServiceState *state = _state;

   ASSERT(state != NULL);

   if (success) {
      const gchar *app;
      gchar *msg;

      app = ToolsCore_GetTcloName(state);
      if (app == NULL) {
         app = state->name;
      }

      msg = g_strdup_printf("vmx.capability.unified_loop %s", app);
      if (!RpcChannel_Send(state->ctx.rpc, msg, strlen(msg) + 1, NULL, NULL)) {
         g_warning("VMX doesn't support the Tools unified loop.\n"
                   "Some functionality (like setting options) may not work.\n");
      }
      g_free(msg);

      /*
       * Log the Tools build number to the VMX log file. We don't really care
       * if sending the message fails.
       */
      msg = g_strdup_printf("log %s: Version: %s", app, BUILD_NUMBER);
      RpcChannel_Send(state->ctx.rpc, msg, strlen(msg) + 1, NULL, NULL);
      g_free(msg);

      g_signal_emit_by_name(state->ctx.serviceObj,
                            TOOLS_CORE_SIG_RESET,
                            &state->ctx);
   } else {
      VMTOOLSAPP_ERROR(&state->ctx, EXIT_FAILURE);
   }
}


/**
 * Checks all loaded plugins for their capabilities, and sends the data to the
 * host. The code will try to send all capabilities, just logging errors as
 * they occur.
 *
 * @param[in]  data     The RPC data.
 *
 * @return TRUE.
 */

static gboolean
ToolsCoreRpcCapReg(RpcInData *data)
{
   char *confPath = GuestApp_GetConfPath();
   gchar *msg;
   GArray *pcaps = NULL;
   ToolsServiceState *state = data->clientData;

   g_signal_emit_by_name(state->ctx.serviceObj,
                         TOOLS_CORE_SIG_CAPABILITIES,
                         &state->ctx,
                         TRUE,
                         &pcaps);

   if (pcaps != NULL) {
      ToolsCore_SetCapabilities(state->ctx.rpc, pcaps, TRUE);
      g_array_free(pcaps, TRUE);
   }

   /* Tell the host the location of the conf directory. */
   msg = g_strdup_printf("tools.capability.guest_conf_directory %s", confPath);
   if (!RpcChannel_Send(state->ctx.rpc, msg, strlen(msg) + 1, NULL, NULL)) {
      g_debug("Unable to register guest conf directory capability.\n");
   }
   g_free(msg);
   msg = NULL;

   /* Send the tools version to the VMX. */
   if (state->mainService) {
      uint32 version;
      char *result = NULL;
      size_t resultLen;
      gchar *toolsVersion;

#if defined(OPEN_VM_TOOLS)
      version = TOOLS_VERSION_UNMANAGED;
#else
      gboolean disableVersion;

      disableVersion = g_key_file_get_boolean(state->ctx.config,
                                              "vmtools",
                                              CONFNAME_DISABLETOOLSVERSION,
                                              NULL);
      version = disableVersion ? TOOLS_VERSION_UNMANAGED : TOOLS_VERSION_CURRENT;
#endif

      toolsVersion = g_strdup_printf("tools.set.version %u", version);

      if (!RpcChannel_Send(state->ctx.rpc, toolsVersion, strlen(toolsVersion) + 1,
                           &result, &resultLen)) {
         g_debug("Error setting tools version: %s.\n", result);
      }
      vm_free(result);
      g_free(toolsVersion);
   }

   state->capsRegistered = TRUE;
   free(confPath);
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/**
 * Handles a "set option" RPC. Calls the plugins which have registered interest
 * in the option being set.
 *
 * @param[in]  data     The RPC data.
 *
 * @return Whether the option was successfully processed.
 */

static gboolean
ToolsCoreRpcSetOption(RpcInData *data)
{

   gboolean retVal = FALSE;
   char *option;
   char *value;
   unsigned int index = 0;
   ToolsServiceState *state = data->clientData;

   /* Parse the option & value string. */
   option = StrUtil_GetNextToken(&index, data->args, " ");
   /* Ignore leading space before value. */
   index++;
   value = StrUtil_GetNextToken(&index, data->args, "");

   if (option != NULL && value != NULL && strlen(value) != 0) {

      g_debug("Setting option '%s' to '%s'.\n", option, value);
      g_signal_emit_by_name(state->ctx.serviceObj,
                            TOOLS_CORE_SIG_SET_OPTION,
                            &state->ctx,
                            option,
                            value,
                            &retVal);
   }

   vm_free(option);
   vm_free(value);

   RPCIN_SETRETVALS(data, retVal ? "" : "Unknown or invalid option", retVal);

   return retVal;
}


/**
 * Initializes the RPC channel. Currently this instantiates an RpcIn loop.
 * This function should only be called once.
 *
 * @param[in]  state    The service state.
 *
 * @return TRUE on success.
 */

gboolean
ToolsCore_InitRpc(ToolsServiceState *state)
{
   static RpcChannelCallback rpcs[] = {
      { "Capabilities_Register", ToolsCoreRpcCapReg, NULL, NULL, NULL, 0 },
      { "Set_Option", ToolsCoreRpcSetOption, NULL, NULL, NULL, 0 },
   };

   size_t i;
   const gchar *app;
   GMainContext *mainCtx = g_main_loop_get_context(state->ctx.mainLoop);

   ASSERT(state->ctx.rpc == NULL);

   if (state->debugPlugin != NULL) {
      app = "debug";
      state->ctx.rpc = state->debugData->newDebugChannel(&state->ctx,
                                                         state->debugData);
   } else {
      /*
       * Currently we try to bring up an RpcIn channel, which will only run
       * inside a Virtual Machine. Some plugins may still want to launch and at
       * least begin even in not in a VM (typically because the installation is dual
       * purposed between a VM and Bootcamp) - plugins may wish to undo some state
       * if not in a VM.
       *
       * XXX: this should be relaxed when we try to bring up a VMCI or TCP channel.
       */
      if (!state->ctx.isVMware) {
         g_warning("The %s service needs to run inside a virtual machine.\n",
                   state->name);
         state->ctx.rpc = NULL;
      } else {
         state->ctx.rpc = BackdoorChannel_New();
      }
      app = ToolsCore_GetTcloName(state);
      if (app == NULL) {
         g_warning("Trying to start RPC channel for invalid %s container.", state->name);
         return FALSE;
      }
   }

   if (state->ctx.rpc) {
      RpcChannel_Setup(state->ctx.rpc,
                       app,
                       mainCtx,
                       &state->ctx,
                       ToolsCoreCheckReset,
                       state);

      /* Register the "built in" RPCs. */
      for (i = 0; i < ARRAYSIZE(rpcs); i++) {
         RpcChannelCallback *rpc = &rpcs[i];
         rpc->clientData = state;
         RpcChannel_RegisterCallback(state->ctx.rpc, rpc);
      }
   }

   return TRUE;
}


/**
 * Sends a list of capabilities to the host.
 *
 * @param[in]  chan     The RPC channel.
 * @param[in]  caps     The list of capabilities.
 * @param[in]  set      TRUE is setting capabilities (otherwise they're set to 0).
 */

void
ToolsCore_SetCapabilities(RpcChannel *chan,
                          GArray *caps,
                          gboolean set)
{
   char *result;
   size_t resultLen;
   guint i;
   gchar *newcaps = NULL;

   for (i = 0; i < caps->len; i++) {
      gchar *tmp;
      ToolsAppCapability *cap =  &g_array_index(caps, ToolsAppCapability, i);
      switch (cap->type) {
      case TOOLS_CAP_OLD:
         result = NULL;
         tmp = g_strdup_printf("tools.capability.%s %u",
                               cap->name,
                               set ? cap->value : 0);
         if (!RpcChannel_Send(chan, tmp, strlen(tmp) + 1, &result, &resultLen)) {
            g_warning("Error sending capability %s: %s\n", cap->name, result);
         }
         vm_free(result);
         g_free(tmp);
         break;

      case TOOLS_CAP_OLD_NOVAL:
         /*
          * This is kind of weird, because of the way the VMX treats RPCs and
          * what is expected of these capabilities without arguments. For a
          * few details, see the comments in RpcOut_sendOne() (rpcout.c).
          * Basically, for the VMX handlers not to complain, we need to send the
          * RPC with the empty space at the end, and not consider the NULL
          * character when counting the bytes.
          */
         if (set) {
            tmp = g_strdup_printf("tools.capability.%s ", cap->name);
            if (!RpcChannel_Send(chan, tmp, strlen(tmp), &result, &resultLen)) {
               g_warning("Error sending capability %s: %s\n", cap->name, result);
            }
            vm_free(result);
            g_free(tmp);
         }
         break;

      case TOOLS_CAP_NEW:
         if (newcaps == NULL) {
            newcaps = g_strdup(GUEST_CAP_FEATURES);
         }
         tmp = g_strdup_printf("%s %d=%u",
                               newcaps,
                               cap->index,
                               set ? cap->value : 0);
         g_free(newcaps);
         newcaps = tmp;
         break;

      default:
         g_error("Invalid capability type: %d\n", cap->type);
      }
   }

   if (newcaps != NULL) {
      result = NULL;
      if (!RpcChannel_Send(chan, newcaps, strlen(newcaps) + 1, &result, &resultLen)) {
         g_warning("Error sending new-style capabilities: %s\n", result);
      }
      vm_free(result);
      g_free(newcaps);
   }
}

