/*********************************************************
 * Copyright (C) 2020-2021 VMware, Inc. All rights reserved.
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
 * toolboxcmd-globalconf.c --
 *
 *     Global Configuration operations for toolbox-cmd.
 */

#include "globalConfig.h"

#if !defined(GLOBALCONFIG_SUPPORTED)
#   error This file should not be compiled
#endif

#include "vm_product.h"
#include "vm_assert.h"
#include "vm_basic_defs.h"
#include "toolboxCmdInt.h"
#include "vmware/tools/i18n.h"

/*
 * GuestStore client library error messages
 *
 * Localization can be done in this way if needed:
 *
 * #define GUESTSTORE_LIB_ERR_ITEM(err, msgid, msg) MSGID(msgid) msg,
 *
 * call this to get localized error message:
 *
 * VMTools_GetString(VMW_TEXT_DOMAIN, guestStoreLibErrMsgs[errCode])
 */
#define GUESTSTORE_LIB_ERR_ITEM(err, msgid, msg) msg,
static const char * const guestStoreLibErrMsgs[] = {
GUESTSTORE_LIB_ERR_LIST
};
#undef GUESTSTORE_LIB_ERR_ITEM


/*
 *-----------------------------------------------------------------------------
 *
 * GlobalConfRefresh --
 *
 *      Trigger a new download of the global configuration from the GuestStore.
 *
 * Results:
 *      GSLIBERR_SUCCESS on success.
 *      GuestStoreClientError on error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static GuestStoreClientError
GlobalConfRefresh(GKeyFile *confDict)
{
   GuestStoreClientError downloadStatus;

   ASSERT(confDict != NULL);

   if (!GuestStoreClient_Init()) {
      g_critical("GuestStoreClient_Init failed.\n");
      downloadStatus = GSLIBERR_NOT_INITIALIZED;
   } else {
      downloadStatus = GlobalConfig_DownloadConfig(confDict);
      GuestStoreClient_DeInit();
   }

   return downloadStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GlobalConfStatus --
 *
 *      Handler for 'status', 'enable', 'disable' commands for globalconf.
 *      'status' will query and prints the status.
 *      'enable' will enable the module. tools.conf is updated.
 *      'disable' will disable the module. tools.conf is updated.
 *      If the tools 'vmsvc' service is running, then the service
 *      is stopped, config is updated and the service is started.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_TEMPFAIL, EX_SOFTWARE or EX_USAGE on error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */


static int
GlobalConfStatus(const char *command)   // IN: command specified by the user.
{
   GKeyFile *confDict = NULL;
   int ret = EXIT_SUCCESS;
   gboolean currentEnabledState;
   gboolean desiredEnabledState;

   ASSERT(command != NULL);

   VMTools_LoadConfig(NULL,
                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                      &confDict,
                      NULL);

   if (confDict == NULL) {
      confDict = g_key_file_new();
   }

   currentEnabledState = GlobalConfig_GetEnabled(confDict);

   if (toolbox_strcmp(command, "status") == 0) {
      ToolsCmd_Print(SU_(globalconf.status,
                         "The status of globalconf module is '%s'\n"),
                     currentEnabledState ?
                        SU_(option.enabled,
                            "Enabled") :
                        SU_(option.disabled,
                            "Disabled"));
      desiredEnabledState = currentEnabledState;
   } else if (toolbox_strcmp(command, "enable") == 0) {
      desiredEnabledState = TRUE;
   } else if (toolbox_strcmp(command, "disable") == 0) {
      desiredEnabledState = FALSE;
   } else {
      desiredEnabledState = currentEnabledState;
      ret = EX_USAGE;
   }

   if (currentEnabledState != desiredEnabledState) {
      GError *err = NULL;
      GlobalConfig_SetEnabled(desiredEnabledState, confDict);

      ToolsCmd_Print(SU_(globalconf.update_config,
                         "%s: Updating the Configuration.\n"),
                     command);
      if (!VMTools_WriteConfig(NULL, confDict, &err)) {
         g_warning("%s: Error writing config: %s.\n",
                   __FUNCTION__, err ? err->message : "");
         g_clear_error(&err);
         ret = EX_TEMPFAIL;
         goto error;
      }

      if (!desiredEnabledState) {
         if (GlobalConfig_DeleteConfig()) {
            g_debug("%s: Deleted the global configuration.\n", __FUNCTION__);
         } else {
            g_warning("%s: Failed to delete the global configuration.\n",
                      __FUNCTION__);
         }
      }
   }

error:
   g_key_file_free(confDict);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GlobalConf_Command --
 *
 *      Parse and handle globalconf command.
 *
 * Results:
 *      0 on success.
 *
 *      Error code from GuestStore client library or
 *      general process error exit code.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
GlobalConf_Command(char **argv,      // IN: Command line arguments
                   int argc,         // IN: Length of the command line arguments
                   gboolean quiet)   // IN
{
   int ret;

   if (toolbox_strcmp(argv[optind], "refresh") == 0) {
      GuestStoreClientError downloadStatus;
      GKeyFile *confDict = NULL;

      VMTools_LoadConfig(NULL,
                        G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                        &confDict,
                        NULL);

      if (confDict == NULL) {
         confDict = g_key_file_new();
      }

      ret = EX_SOFTWARE;

      if (!GlobalConfig_GetEnabled(confDict)) {
         ToolsCmd_PrintErr(SU_(globalconf.refresh.failed,
                               "'%s' failed, since globalconf module is"
                               " disabled.\n"),
                           argv[optind]);
         g_key_file_free(confDict);
         return ret;
      }

      downloadStatus = GlobalConfRefresh(confDict);
      g_key_file_free(confDict);

      if (downloadStatus == GSLIBERR_SUCCESS) {
         ToolsCmd_Print(SU_(result.succeeded,
                            "'%s' succeeded.\n"),
                        argv[optind]);
         ret = EXIT_SUCCESS;
      } else if (downloadStatus < GUESTSTORE_LIB_ERR_MAX) {
         ToolsCmd_PrintErr(SU_(gueststore.error.client_lib,
                               "'%s' failed, GuestStore client library "
                               "error: %s.\n"), argv[optind],
                           guestStoreLibErrMsgs[downloadStatus]);
      } else {
         ToolsCmd_PrintErr(SU_(result.error.failed,
                               "'%s' failed, check %s log for "
                               "more information.\n"),
                           argv[optind], argv[0]);
      }
   } else if (toolbox_strcmp(argv[optind], "status") == 0 ||
              toolbox_strcmp(argv[optind], "enable") == 0 ||
              toolbox_strcmp(argv[optind], "disable") == 0) {
      ret = GlobalConfStatus(argv[optind]);
      if (ret != EXIT_SUCCESS) {
         ToolsCmd_PrintErr(SU_(result.error.failed,
                               "'%s' failed, check %s log for "
                               "more information.\n"),
                           argv[optind], argv[0]);
      }
   } else {
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  argv[optind]);
      ret = EX_USAGE;
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GlobalConf_Help --
 *
 *      Prints the help for the globalconf command.
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
GlobalConf_Help(const char *progName, // IN: Name of the program from argv[0]
                const char *cmd)      // IN
{
   g_print(SU_(help.globalconf,
               "%s: Manage global configuration downloads "
               "from the GuestStore\n"
               "Usage: %s %s <subcommand>\n\n"
               "ESX guests only subcommands:\n"
               "   enable: "
               "Enable the global configuration module\n"
               "   disable: "
               "Disable the global configuration module\n"
               "   refresh: "
               "Trigger a new download of the global configuration "
               "from the GuestStore\n"
               "   status: "
               "Print the status of the global configuration module\n"),
           cmd, progName, cmd);
}
