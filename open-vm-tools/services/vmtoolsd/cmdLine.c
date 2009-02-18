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

#include "conf.h"
#include "rpcout.h"
#include "str.h"
#include "vmtools.h"
#include "vmtoolsd_version.h"


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
   char *result = NULL;
   Bool status = FALSE;

   status = RpcOut_sendOne(&result, NULL, "%s", value);

   if (!status) {
      g_printerr("%s\n", result ? result : "NULL");
   } else {
      g_print("%s\n", result);
   }

   vm_free(result);
   exit(status ? 0 : 1);
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
 * Asks a running instance of a service to stop running.
 *
 * @param[in]  svcname     Name of the service to be killed.
 *
 * @return Whether killing the service was successful.
 */

static gboolean
ToolsCoreKillService(const gchar *svcname)
{
   gboolean ret = FALSE;
   gchar *msg;
   wchar_t *evt;
   HANDLE h = NULL;

   g_assert(svcname != NULL);

   evt = Str_Aswprintf(NULL, QUIT_EVENT_NAME_FMT, svcname);
   if (evt == NULL) {
      g_printerr("Out of memory!\n");
      goto exit;
   }

   h = OpenEvent(EVENT_MODIFY_STATE, FALSE, evt);
   if (h == NULL) {
      goto error;
   }

   if (!SetEvent(h)) {
      goto error;
   }

   ret = TRUE;
   goto exit;

error:
   msg = g_win32_error_message(GetLastError());
   g_printerr("Cannot open quit event: %s\n", msg);
   g_free(msg);

exit:
   vm_free(evt);
   CloseHandle(h);
   return ret;
}

#endif


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
   gboolean ret = FALSE;
   gboolean version = FALSE;
#if defined(G_PLATFORM_WIN32)
   gboolean kill = FALSE;
#endif
   GOptionEntry clOptions[] = {
      { "name", 'n', 0, G_OPTION_ARG_STRING, &state->name,
         N_("Name of the service being started."), N_("svcname") },
      { "plugin-path", 'p', 0, G_OPTION_ARG_FILENAME, &state->pluginPath,
         N_("Path to the plugin directory."), N_("path") },
      { "cmd", '\0', 0, G_OPTION_ARG_CALLBACK, ToolsCoreRunCommand,
         N_("Sends an RPC command to the host and exits."), N_("command") },
#if defined(G_PLATFORM_WIN32)
      { "kill", 'k', 0, G_OPTION_ARG_NONE, &kill,
         N_("Stops a running instance of a tools service."), 0 },
      { "install", 'i', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, ToolsCoreIgnoreArg,
         N_("Installs the service with the Service Control Manager."), N_("args") },
      { "uninstall", 'u', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, ToolsCoreIgnoreArg,
         N_("Uninstalls the service from the Service Control Manager."), NULL },
      { "displayname", 'd', 0, G_OPTION_ARG_STRING, &state->displayName,
         N_("Service display name (only used with -i)."), N_("name") },
#else
      { "background", 'b', 0, G_OPTION_ARG_FILENAME, &state->pidFile,
         N_("Runs in the background and creates a pid file."), N_("pidfile") },
      { "blockFd", '\0', 0, G_OPTION_ARG_INT, &state->ctx.blockFD,
         N_("File descriptor for the VMware blocking fs."), N_("fd") },
#endif
      { "config", 'c', 0, G_OPTION_ARG_FILENAME, &state->configFile,
         N_("Uses the config file at the given path."), N_("path") },
      { "debug", 'g', 0, G_OPTION_ARG_FILENAME, &state->debugPlugin,
         N_("Runs in debug mode, using the given plugin."), N_("path") },
      { "log", 'l', 0, G_OPTION_ARG_NONE, &state->log,
         N_("Turns on logging. Overrides the config file."), NULL },
      { "version", 'v', 0, G_OPTION_ARG_NONE, &version,
         N_("Prints the daemon version and exits."), NULL },
      { NULL }
   };
   GError *error = NULL;
   GOptionContext *context = NULL;

#if !defined(G_PLATFORM_WIN32)
   state->ctx.blockFD = -1;
#endif

   context = g_option_context_new(NULL);
#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 12
   g_option_context_set_summary(context, N_("Runs the VMware Tools daemon."));
#endif
   g_option_context_add_main_entries(context, clOptions, NULL);

   if (!g_option_context_parse(context, &argc, &argv, &error)) {
      g_print("%s: %s\n", N_("Command line parsing failed"), error->message);
      goto exit;
   }

   if (version) {
      g_print("%s %s\n", _("VMware Tools daemon, version"), VMTOOLSD_VERSION_STRING);
      exit(0);
   }

   VMTools_EnableLogging(state->log);
   if (state->name == NULL) {
      state->name = VMTOOLS_GUEST_SERVICE;
      state->mainService = TRUE;
   } else {
      state->mainService = (strcmp(state->name, VMTOOLS_GUEST_SERVICE) == 0);
   }

#if defined(G_PLATFORM_WIN32)
   if (kill) {
      exit(ToolsCoreKillService(state->name) ? 0 : 1);
   }
#else
   /* If not running the "vmusr" service, ignore the blockFd parameter. */
   if (strcmp(state->name, VMTOOLS_USER_SERVICE) != 0) {
      if (state->ctx.blockFD >= 0) {
         close(state->ctx.blockFD);
      }
      state->ctx.blockFD = -1;
   }
#endif

   ret = TRUE;

exit:
   g_clear_error(&error);
   g_option_context_free(context);
   return ret;
}

