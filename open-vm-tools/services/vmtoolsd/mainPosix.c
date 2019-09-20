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
 * @file mainPosix.c
 *
 *    Service entry point for the POSIX version of the tools daemon.
 */


#include "toolsCoreInt.h"
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include "file.h"
#include "guestApp.h"
#include "hostinfo.h"
#include "system.h"
#include "unicode.h"
#include "util.h"
#include "vmware/tools/log.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/utils.h"

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

static ToolsServiceState gState = { NULL, };


/**
 * Reloads the service configuration - including forcing rotation of log
 * files by reinitializing the logging subsystem.
 *
 * @param[in]  info     Unused.
 * @param[in]  data     Service state.
 *
 * @return TRUE
 */

static gboolean
ToolsCoreSigHUPCb(const siginfo_t *info,
                  gpointer data)
{
   ToolsCore_ReloadConfig(data, TRUE);
   return TRUE;
}


/**
 * Handles a signal that would terminate the process. Asks the main loop
 * to exit nicely.
 *
 * @param[in]  info     Unused.
 * @param[in]  data     Pointer to the main loop to be stopped.
 *
 * @return FALSE
 */

static gboolean
ToolsCoreSigHandler(const siginfo_t *info,
                    gpointer data)
{
   g_main_loop_quit((GMainLoop *)data);
   return FALSE;
}


/**
 * Handles a USR1 signal; logs the current service state.
 * Also shutdown rpc connection so we can do tools upgrade.
 *
 * @param[in]  info     Unused.
 * @param[in]  data     Unused.
 *
 * @return TRUE
 */

static gboolean
ToolsCoreSigUsrHandler(const siginfo_t *info,
                       gpointer data)
{
   ToolsCore_DumpState(&gState);

   if (TOOLS_IS_USER_SERVICE(&gState.ctx)) {
      g_info("Shutting down guestrpc on signal USR1 ...\n");
      g_signal_emit_by_name(gState.ctx.serviceObj,
                            TOOLS_CORE_SIG_NO_RPC,
                            &gState.ctx);
      RpcChannel_Destroy(gState.ctx.rpc);
      gState.ctx.rpc = NULL;
   }

   return TRUE;
}


/**
 * Perform (optional) work before or after running the main loop.
 *
 * @param[in]  state    Service state.
 * @param[in]  before   TRUE if before running the main loop, FALSE if after.
 */

static void
ToolsCoreWorkAroundLoop(ToolsServiceState *state,
                        gboolean before)
{
#ifdef __APPLE__
   if (state->mainService) {
      char *libDir = GuestApp_GetInstallPath();
      char *argv[] = {
         NULL,
         before ? "--startInternal" : "--stopInternal",
         NULL,
      };

      if (!libDir) {
         g_error("Failed to retrieve libDir.\n");
      }

      argv[0] = g_strdup_printf("%s/services.sh", libDir);
      free(libDir);
      if (!argv[0]) {
         g_error("Failed to construct argv[0].\n");
      }

      g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
      free(argv[0]);
   }
#endif
}


/**
 * Tools daemon entry function.
 *
 * @param[in] argc   Argument count.
 * @param[in] argv   Argument array.
 * @param[in] envp   User environment.
 *
 * @return 0 on successful execution, error code otherwise.
 */

int
main(int argc,
     char *argv[],
     const char *envp[])
{
   int i;
   int ret = EXIT_FAILURE;
   char **argvCopy;
   GSource *src;

   Unicode_Init(argc, &argv, NULL);

   /*
    * ToolsCore_ParseCommandLine() uses g_option_context_parse(), which modifies
    * argv. We don't want that to happen, so we make a copy of the array and
    * use that as the argument instead.
    */
   argvCopy = g_malloc(argc * sizeof *argvCopy);
   for (i = 0; i < argc; i++) {
      argvCopy[i] = argv[i];
   }

   setlocale(LC_ALL, "");

   VMTools_UseVmxGuestLog(VMTOOLS_APP_NAME);
   VMTools_ConfigLogging(G_LOG_DOMAIN, NULL, TRUE, FALSE);
   VMTools_SetupVmxGuestLog(FALSE, NULL, NULL);

   VMTools_BindTextDomain(VMW_TEXT_DOMAIN, NULL, NULL);

   if (!ToolsCore_ParseCommandLine(&gState, argc, argvCopy)) {
      g_free(argvCopy);
      goto exit;
   }
   g_free(argvCopy);
   argvCopy = NULL;

   if (gState.pidFile != NULL) {
      /*
       * If argv[0] is not an absolute path, make it so; all other path
       * arguments should have been given as absolute paths if '--background'
       * was used, or things may not work as expected.
       */
      if (!g_path_is_absolute(argv[0])) {
         gchar *abs = g_find_program_in_path(argv[0]);
         if (abs == NULL || strcmp(abs, argv[0]) == 0) {
            char *cwd = File_Cwd(NULL);
            g_free(abs);
            abs = g_strdup_printf("%s%c%s", cwd, DIRSEPC, argv[0]);
            vm_free(cwd);
         }
         argv[0] = abs;
      }

      /*
       * Need to remove --background from the command line or we'll get
       * into an infinite loop. ToolsCore_ParseCommandLine() already
       * validated that "-b" has an argument, so it's safe to assume the
       * data is there.
       */
      for (i = 1; i < argc; i++) {
         size_t count = 0;
         if (strcmp(argv[i], "--background") == 0 ||
             strcmp(argv[i], "-b") == 0) {
            count = 2;
         } else if (g_str_has_prefix(argv[i], "--background=")) {
            count = 1;
         }
         if (count) {
            memmove(argv + i, argv + i + count, (argc - i - count) * sizeof *argv);
            argv[argc - count] = NULL;
            break;
         }
      }

      if (!Hostinfo_Daemonize(argv[0],
                              argv,
                              HOSTINFO_DAEMONIZE_LOCKPID,
                              gState.pidFile, NULL, 0)) {
         goto exit;
      }
      return 0;
   }

   ToolsCore_Setup(&gState);

   src = VMTools_NewSignalSource(SIGHUP);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src,
                            ToolsCoreSigHUPCb, &gState, NULL);
   g_source_unref(src);

   src = VMTools_NewSignalSource(SIGINT);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src,
                            ToolsCoreSigHandler, gState.ctx.mainLoop, NULL);
   g_source_unref(src);

   src = VMTools_NewSignalSource(SIGQUIT);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src,
                            ToolsCoreSigHandler, gState.ctx.mainLoop, NULL);
   g_source_unref(src);

   /* On Mac OS, launchd uses SIGTERM. */
   src = VMTools_NewSignalSource(SIGTERM);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src,
                            ToolsCoreSigHandler, gState.ctx.mainLoop, NULL);
   g_source_unref(src);

   src = VMTools_NewSignalSource(SIGUSR1);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src, ToolsCoreSigUsrHandler, NULL, NULL);
   g_source_unref(src);

   /* Ignore SIGUSR2 by default. */
   signal(SIGUSR2, SIG_IGN);

   signal(SIGPIPE, SIG_IGN);

   /*
    * Save the original environment so that we can safely spawn other
    * applications (since we may have to modify the original environment
    * to launch vmtoolsd successfully).
    */
   gState.ctx.envp = System_GetNativeEnviron(envp);

   ToolsCoreWorkAroundLoop(&gState, TRUE);
   ret = ToolsCore_Run(&gState);
   ToolsCoreWorkAroundLoop(&gState, FALSE);

   if (gState.pidFile != NULL) {
      g_unlink(gState.pidFile);
   }
exit:
   return ret;
}
