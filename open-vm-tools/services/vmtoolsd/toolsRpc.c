/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "conf.h"
#include "str.h"
#include "strutil.h"
#include "toolsCoreInt.h"
#include "vmtoolsd_version.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/log.h"
#include "vm_version.h"
#if defined(__linux__)
#include "vmci_sockets.h"
#endif

/**
 * Take action after an RPC channel reset.
 *
 * @param[in]  chan     The RPC channel.
 * @param[in]  success  Whether reset was successful.
 * @param[in]  _state   The service state.
 */

static void
ToolsCoreCheckReset(RpcChannel *chan,
                    gboolean success,
                    gpointer _state)
{
   ToolsServiceState *state = _state;
   static gboolean version_sent = FALSE;

   ASSERT(state != NULL);
   ASSERT(chan == state->ctx.rpc);

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

      if (!version_sent) {
         /*
          * Log the Tools version to the VMX log file. We don't really care
          * if sending the message fails.
          */
         msg = g_strdup_printf("log %s: Version: %s (%s)",
                               app, VMTOOLSD_VERSION_STRING, BUILD_NUMBER);
         RpcChannel_Send(state->ctx.rpc, msg, strlen(msg) + 1, NULL, NULL);
         g_free(msg);
         /* send message only once to prevent log spewing: */
         version_sent = TRUE;
      }

      g_signal_emit_by_name(state->ctx.serviceObj,
                            TOOLS_CORE_SIG_RESET,
                            &state->ctx);
#if defined(__linux__)
      if (state->mainService) {
         /*
          * Release the existing vSocket family.
          */
         ToolsCore_ReleaseVsockFamily(state);
         ToolsCore_InitVsockFamily(state);
      }
#endif
   } else {
      VMTOOLSAPP_ERROR(&state->ctx, EXIT_FAILURE);
   }
}


#if !defined(_WIN32)
/**
 * ToolsCoreAppChannelFail --
 *
 * Call-back function for RpcChannel to report that the RPC channel for the
 * toolbox-dnd (vmusr) application cannot be acquired. This would signify
 * that the channel is currently in use by another vmusr process.
 *
 * @param[in]  _state   The service state.
 */

static void
ToolsCoreAppChannelFail(UNUSED_PARAM(gpointer _state))
{
   char *cmdGrepVmusrTools;
#if !defined(__APPLE__)
   ToolsServiceState *state = _state;
#endif
#if defined(__linux__) || defined(__FreeBSD__) || defined(sun)
   static const char  *vmusrGrepExpr = "'vmtoolsd.*vmusr'";
#if defined(sun)
   static const char *psCmd = "ps -aef";
#else
   static const char *psCmd = "ps ax";     /* using BSD syntax */
#endif
#else  /* Mac OS */
   static const char  *vmusrGrepExpr = "'vmware-tools-daemon.*vmusr'";
   static const char *psCmd = "ps -ex";
#endif

   cmdGrepVmusrTools = Str_Asprintf(NULL, "%s | egrep %s | egrep -v 'grep|%d'",
                                    psCmd, vmusrGrepExpr, (int) getpid());

   /*
    * Check if there is another vmtoolsd vmusr process running on the
    * system and log the appropriate warning message before terminating
    * this vmusr process.
    */
   if (system(cmdGrepVmusrTools) == 0) {
      g_warning("Exiting the vmusr process. Another vmusr process is "
                "currently running.\n");
   } else {
      g_warning("Exiting the vmusr process; unable to acquire the channel.\n");
   }
   free(cmdGrepVmusrTools);

#if !defined(__APPLE__)
   if (g_main_loop_is_running(state->ctx.mainLoop)) {
      g_warning("Calling g_main_loop_quit() to terminate the process.\n");
      g_main_loop_quit(state->ctx.mainLoop);
   } else {
      g_warning("Exiting the process.\n");
      exit(1);
   }
#else  /* Mac OS */
   /*
    * On Mac OS X, always exit with non-zero status. This is a signal to
    * launchd that the vmusr process had a "permanent" failure and should
    * not be automatically restarted for this user session.
    */
   g_warning("Exiting the process.\n");
   exit(1);
#endif
}
#endif


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
      g_warning("Unable to register guest conf directory capability.\n");
   }
   g_free(msg);
   msg = NULL;

   /* Send the tools version to the VMX. */
   if (state->mainService) {
      uint32 version;
      uint32 type = TOOLS_TYPE_UNKNOWN;
      char *result = NULL;
      size_t resultLen;
      gchar *toolsVersion;
      gboolean hideVersion = g_key_file_get_boolean(state->ctx.config,
                                                    "vmtools",
                                                    CONFNAME_HIDETOOLSVERSION,
                                                    NULL);

#if defined(_WIN32)
      type = TOOLS_TYPE_MSI;
#else
#if defined(OPEN_VM_TOOLS)
      type = TOOLS_TYPE_OVT;
#else
      {
         static int is_osp = -1;

         if (is_osp == -1) {
            is_osp = (access("/usr/lib/vmware-tools/dsp", F_OK) == 0);
         }
         type = is_osp ? TOOLS_TYPE_OSP : TOOLS_TYPE_TARBALL;
      }
#endif
#endif

      version = hideVersion ? TOOLS_VERSION_UNMANAGED : TOOLS_VERSION_CURRENT;

      /*
       * First try "tools.set.versiontype", if that fails because host is too
       * old, fall back to "tools.set.version."
       */
      toolsVersion = g_strdup_printf("tools.set.versiontype %u %u", version, type);

      if (!RpcChannel_Send(state->ctx.rpc, toolsVersion, strlen(toolsVersion) + 1,
                           &result, &resultLen)) {
         GError *gerror = NULL;
         gboolean disableVersion;
         vm_free(result);
         g_free(toolsVersion);

         /*
          * Fall back to old behavior for OSPs and OVT so that tools will be
          * reported as guest managed.
          */
         disableVersion = g_key_file_get_boolean(state->ctx.config,
                                                          "vmtools",
                                                          CONFNAME_DISABLETOOLSVERSION,
                                                          &gerror);

         /* By default disableVersion is FALSE, except for open-vm-tools */
         if (type == TOOLS_TYPE_OVT && gerror != NULL) {
            g_debug("gerror->code = %d when checking for %s\n", gerror->code, CONFNAME_DISABLETOOLSVERSION);
            g_clear_error(&gerror);
            disableVersion = TRUE;
         }

         version = disableVersion ? TOOLS_VERSION_UNMANAGED : TOOLS_VERSION_CURRENT;
         toolsVersion = g_strdup_printf("tools.set.version %u", version);

         if (!RpcChannel_Send(state->ctx.rpc, toolsVersion, strlen(toolsVersion) + 1,
                              &result, &resultLen)) {
            g_warning("Error setting tools version: %s.\n", result);
         }
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
         g_info("The %s service needs to run inside a virtual machine.\n",
                state->name);
         state->ctx.rpc = NULL;
      } else {
         state->ctx.rpc = RpcChannel_New();
      }
      app = ToolsCore_GetTcloName(state);
      if (app == NULL) {
         g_warning("Trying to start RPC channel for invalid %s container.", state->name);
         return FALSE;
      }
   }

   if (state->ctx.rpc) {

      /*
       * Default tools RpcChannel setup: No channel error threshold limit and
       *                                 no notification callback function.
       */
      RpcChannelFailureCb failureCb = NULL;
      guint errorLimit = 0;

#if !defined(_WIN32)

      /* For the *nix user service app. */
      if (TOOLS_IS_USER_SERVICE(state)) {
         failureCb = ToolsCoreAppChannelFail;
         errorLimit = ToolsCore_GetVmusrLimit(state);
      }
#endif

      RpcChannel_Setup(state->ctx.rpc,
                       app,
                       mainCtx,
                       &state->ctx,
                       ToolsCoreCheckReset,
                       state,
                       failureCb,
                       errorLimit);

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


#if defined(__linux__)
/**
 * Initializes the vSocket address family and sticks a reference
 * to it in the service state.
 *
 * @param[in]  state    The service state.
 */

void
ToolsCore_InitVsockFamily(ToolsServiceState *state)
{
   int vsockDev = -1;
   int vsockFamily = -1;

   ASSERT(state);

   state->vsockDev = -1;
   state->vsockFamily = -1;

   if (!state->ctx.rpc) {
      /*
       * Nothing more to do when there is no RPC channel.
       */
      g_debug("No RPC channel; skipping reference to vSocket family.\n");
      return;
   }

   switch (RpcChannel_GetType(state->ctx.rpc)) {
   case RPCCHANNEL_TYPE_INACTIVE:
   case RPCCHANNEL_TYPE_PRIV_VSOCK:
   case RPCCHANNEL_TYPE_UNPRIV_VSOCK:
      return;
   case RPCCHANNEL_TYPE_BKDOOR:
      vsockFamily = VMCISock_GetAFValueFd(&vsockDev);
      if (vsockFamily == -1) {
         /*
          * vSocket driver may not be loaded, log and continue.
          */
         g_warning("Couldn't get vSocket family.\n");
      } else if (vsockDev >= 0) {
         g_debug("Saving reference to vSocket device=%d, family=%d\n",
                 vsockDev, vsockFamily);
         state->vsockFamily = vsockFamily;
         state->vsockDev = vsockDev;
      }
      return;
   default:
      NOT_IMPLEMENTED();
   }
}


/**
 * Releases the reference to vSocket address family.
 *
 * @param[in]  state    The service state.
 *
 * @return TRUE on success.
 */

void
ToolsCore_ReleaseVsockFamily(ToolsServiceState *state)
{
   ASSERT(state);

   /*
    * vSocket device is not opened in case of new kernels.
    * Therefore, we release it only if it was opened.
    */
   if (state->vsockFamily >= 0 && state->vsockDev >= 0) {
      g_debug("Releasing reference to vSocket device=%d, family=%d\n",
              state->vsockDev, state->vsockFamily);
      VMCISock_ReleaseAFValueFd(state->vsockDev);
      state->vsockDev = -1;
      state->vsockFamily = -1;
   }
}
#endif
