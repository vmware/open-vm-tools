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
 * @file cmdLine.c
 *
 *    Parses the daemon's command line arguments. Some commands may cause the
 *    process to exit.
 */

#include "toolsCoreInt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(G_PLATFORM_WIN32)
#  include <unistd.h>
#endif
#include <glib/gi18n.h>

#include "vm_assert.h"
#include "conf.h"
#include "str.h"
#include "vmcheck.h"
#include "vmtoolsd_version.h"
#include "vmware/tools/log.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/guestrpc.h"
#include "vm_version.h"

/**
 * Runs the given Tools RPC command, printing the result to the terminal and
 * exiting the application afterwards.
 *
 * @param[in]  option      Unused.
 * @param[in]  value       RPC command.
 * @param[in]  data        Unused.
 * @param[out] error       Unused.
 *
 * @return This function doesn't return.
 */

static gboolean
ToolsCoreRunCommand(const gchar *option,
                    const gchar *value,
                    gpointer data,
                    GError **error)
{
#if defined(_WIN32)
   VMTools_AttachConsole();
#endif
   if (VmCheck_IsVirtualWorld()) {
      char *result = NULL;
      Bool status = FALSE;

      status = RpcChannel_SendOne(&result, NULL, "%s", value);

      if (!status) {
         g_printerr("%s\n", result ? result : "NULL");
      } else {
         g_print("%s\n", result);
      }

      vm_free(result);
      exit(status ? 0 : 1);
   }
   g_printerr("%s\n",
              SU_(cmdline.rpcerror, "Unable to send command to VMware hypervisor."));
   exit(1);
}


#if defined(G_PLATFORM_WIN32)

/**
 * Function used to ignore command line arguments.
 *
 * @param[in]  option      Unused.
 * @param[in]  value       Unused.
 * @param[in]  data        Unused.
 * @param[out] error       Unused.
 *
 * @return TRUE
 */

static gboolean
ToolsCoreIgnoreArg(const gchar *option,
                   const gchar *value,
                   gpointer data,
                   GError **error)
{
   return TRUE;
}


/**
 * Signals a specific event in a running service instance. Since this function
 * doesn't know whether the service is running through the SCM, it first tries
 * to open a local event, and if that fails, tries a global event.
 *
 * @param[in]  svcname     Name of the service to be signaled.
 * @param[in]  evtFmt      Format string for the event name. It should expect
 *                         a single string parameter.
 *
 * @return Whether successfully signaled the running service.
 */

static gboolean
ToolsCoreSignalEvent(const gchar *svcname,
                     const wchar_t *evtFmt)
{
   gboolean ret = FALSE;
   gchar *msg;
   wchar_t *evt;
   HANDLE h = NULL;

   ASSERT(svcname != NULL);

   evt = Str_Aswprintf(NULL, evtFmt, L"Local", svcname);
   if (evt == NULL) {
      g_printerr("Out of memory!\n");
      goto exit;
   }

   h = OpenEvent(EVENT_MODIFY_STATE, FALSE, evt);
   if (h != NULL) {
      goto dispatch;
   }

   vm_free(evt);
   evt = Str_Aswprintf(NULL, evtFmt, L"Global", svcname);
   if (evt == NULL) {
      g_printerr("Out of memory!\n");
      goto exit;
   }

   h = OpenEvent(EVENT_MODIFY_STATE, FALSE, evt);
   if (h == NULL) {
      goto error;
   }

dispatch:
   if (!SetEvent(h)) {
      goto error;
   }

   ret = TRUE;
   goto exit;

error:
   msg = g_win32_error_message(GetLastError());
   g_printerr("Cannot open event: %s\n", msg);
   g_free(msg);

exit:
   vm_free(evt);
   CloseHandle(h);
   return ret;
}

#endif


/**
 * Error hook called when command line parsing fails. On Win32, make sure we
 * have a terminal where to show the error message.
 *
 * @param[in] context    Unused.
 * @param[in] group      Unused.
 * @param[in] data       Unused.
 * @param[in] error      Unused.
 */

static void
ToolsCoreCmdLineError(GOptionContext *context,
                      GOptionGroup *group,
                      gpointer data,
                      GError **error)
{
#if defined(_WIN32)
   VMTools_AttachConsole();
#endif
}


/**
 * Parses the command line. For a list of available options, look at the source
 * below, where the option array is declared.
 *
 * @param[out] state    Parsed options will be placed in this struct.
 * @param[in]  argc     Argument count.
 * @param[in]  argv     Argument array.
 *
 * @return TRUE on success.
 */

gboolean
ToolsCore_ParseCommandLine(ToolsServiceState *state,
                           int argc,
                           char *argv[])
{
   int i = 0;
   char *cmdStr = NULL;
   gboolean ret = FALSE;
   gboolean version = FALSE;
#if defined(G_PLATFORM_WIN32)
   gboolean dumpState = FALSE;
   gboolean kill = FALSE;
#endif
   gboolean unused;
   GOptionEntry clOptions[] = {
      { "name", 'n', 0, G_OPTION_ARG_STRING, &state->name,
         SU_(cmdline.name, "Name of the service being started."),
         SU_(cmdline.name.argument, "svcname") },
      { "common-path", '\0', 0, G_OPTION_ARG_FILENAME, &state->commonPath,
         SU_(cmdline.commonpath, "Path to the common plugin directory."),
         SU_(cmdline.path, "path") },
      { "plugin-path", 'p', 0, G_OPTION_ARG_FILENAME, &state->pluginPath,
         SU_(cmdline.pluginpath, "Path to the plugin directory."),
         SU_(cmdline.path, "path") },
      { "cmd", '\0', 0, G_OPTION_ARG_CALLBACK, ToolsCoreRunCommand,
         SU_(cmdline.rpc, "Sends an RPC command to the host and exits."),
         SU_(cmdline.rpc.command, "command") },
#if defined(G_PLATFORM_WIN32)
      { "dump-state", 's', 0, G_OPTION_ARG_NONE, &dumpState,
         SU_(cmdline.state, "Dumps the internal state of a running service instance to the logs."),
         NULL },
      { "kill", 'k', 0, G_OPTION_ARG_NONE, &kill,
         SU_(cmdline.kill, "Stops a running instance of a tools service."),
         NULL },
      { "install", 'i', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, ToolsCoreIgnoreArg,
         SU_(cmdline.install, "Installs the service with the Service Control Manager."),
         SU_(cmdline.install.args, "args") },
      { "uninstall", 'u', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, ToolsCoreIgnoreArg,
         SU_(cmdline.uninstall, "Uninstalls the service from the Service Control Manager."),
         NULL },
      { "displayname", 'd', 0, G_OPTION_ARG_STRING, &state->displayName,
         SU_(cmdline.displayname, "Service display name (only used with -i)."),
         SU_(cmdline.displayname.argument, "name") },
#else
      { "background", 'b', 0, G_OPTION_ARG_FILENAME, &state->pidFile,
         SU_(cmdline.background, "Runs in the background and creates a pid file."),
         SU_(cmdline.background.pidfile, "pidfile") },
      { "blockFd", '\0', 0, G_OPTION_ARG_INT, &state->ctx.blockFD,
         SU_(cmdline.blockfd, "File descriptor for the VMware blocking fs."),
         SU_(cmdline.blockfd.fd, "fd") },
      { "uinputFd", '\0', 0, G_OPTION_ARG_INT, &state->ctx.uinputFD,
         SU_(cmdline.uinputfd, "File descriptor for the uinput device."),
         SU_(cmdline.uinputfd.fd, "fd") },
#endif
      { "config", 'c', 0, G_OPTION_ARG_FILENAME, &state->configFile,
         SU_(cmdline.config, "Uses the config file at the given path."),
         SU_(cmdline.path, "path") },
      { "debug", 'g', 0, G_OPTION_ARG_FILENAME, &state->debugPlugin,
         SU_(cmdline.debug, "Runs in debug mode, using the given plugin."),
         SU_(cmdline.path, "path") },
      { "log", 'l', 0, G_OPTION_ARG_NONE, &unused,
         SU_(cmdline.log, "Ignored, kept for backwards compatibility."),
         NULL },
      { "version", 'v', 0, G_OPTION_ARG_NONE, &version,
         SU_(cmdline.version, "Prints the daemon version and exits."),
         NULL },
      { NULL }
   };
   GError *error = NULL;
   GOptionContext *context = NULL;

#if !defined(G_PLATFORM_WIN32)
   state->ctx.blockFD = -1;
   state->ctx.uinputFD = -1;
#endif

   /*
    * Form the commandline for debug log before calling
    * g_option_context_parse(), because it modifies argv.
    */
   cmdStr = Str_SafeAsprintf(NULL, "%s", argv[0]);
   for (i = 1; i < argc; i++) {
      char *prefix = cmdStr;
      cmdStr = Str_SafeAsprintf(NULL, "%s %s", prefix, argv[i]);
      free(prefix);
      /*
       * NOTE: We can't log the cmdStr here, we can
       * only log it after logging gets configured.
       * Logging it before ToolsCore_ReloadConfig call
       * will not generate proper logs.
       */
   }

   context = g_option_context_new(NULL);
   g_option_context_set_summary(context, N_("Runs the VMware Tools daemon."));
   g_option_context_add_main_entries(context, clOptions, NULL);
   g_option_group_set_error_hook(g_option_context_get_main_group(context),
                                 ToolsCoreCmdLineError);

   if (!g_option_context_parse(context, &argc, &argv, &error)) {
      g_printerr("%s: %s\n", N_("Command line parsing failed"), error->message);
      goto exit;
   }

   if (version) {
      g_print("%s %s (%s)\n", _("VMware Tools daemon, version"),
              VMTOOLSD_VERSION_STRING, BUILD_NUMBER);
      exit(0);
   }

   if (state->name == NULL) {
      state->name = VMTOOLS_GUEST_SERVICE;
      state->mainService = TRUE;
   } else {
      if (strcmp(state->name, VMTOOLS_USER_SERVICE) != 0 &&
          strcmp(state->name, VMTOOLS_GUEST_SERVICE) != 0) {
         g_printerr("%s is an invalid service name.\n", state->name);
         goto exit;
      }
      state->mainService = TOOLS_IS_MAIN_SERVICE(state);
   }

   /* Configure logging system. */
   ToolsCore_ReloadConfig(state, TRUE);

   /* Log the commandline for debugging purposes. */
   g_info("CmdLine: \"%s\"\n", cmdStr);

#if defined(G_PLATFORM_WIN32)
   if (kill) {
      exit(ToolsCoreSignalEvent(state->name, QUIT_EVENT_NAME_FMT) ? 0 : 1);
   }
   if (dumpState) {
      exit(ToolsCoreSignalEvent(state->name, DUMP_STATE_EVENT_NAME_FMT) ? 0 : 1);
   }
#else
   /* If not running the "vmusr" service, ignore the blockFd and uinputFd parameter. */
   if (!TOOLS_IS_USER_SERVICE(state)) {
      if (state->ctx.blockFD >= 0) {
         close(state->ctx.blockFD);
      }
      state->ctx.blockFD = -1;

      if (state->ctx.uinputFD >= 0) {
         close(state->ctx.uinputFD);
      }
      state->ctx.uinputFD = -1;
   }
#endif

   ret = TRUE;

exit:
   free(cmdStr);
   g_clear_error(&error);
   g_option_context_free(context);
   return ret;
}

