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
 * @file mainPosix.c
 *
 *    Service entry point for the POSIX version of the tools daemon.
 */


#include "toolsCoreInt.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include "file.h"
#include "hostinfo.h"
#include "unicode.h"
#include "util.h"
#include "vmtools.h"

#if !defined(__APPLE__)
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
   VMTools_ResetLogging(TRUE);
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

gboolean
ToolsCoreSigHandler(const siginfo_t *info,
                    gpointer data)
{
   g_main_loop_quit((GMainLoop *)data);
   return FALSE;
}


/**
 * Handles a USR1 signal; logs the current service state.
 *
 * @param[in]  info     Unused.
 * @param[in]  data     Unused.
 *
 * @return TRUE
 */

gboolean
ToolsCoreSigUsrHandler(const siginfo_t *info,
                       gpointer data)
{
   ToolsCore_DumpState(&gState);
   return TRUE;
}


/**
 * Tools daemon entry function.
 *
 * @param[in] argc   Argument count.
 * @param[in] argv   Argument array.
 *
 * @return 0 on successful execution, error code otherwise.
 */

int
main(int argc,
     char *argv[])
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
         if (abs == NULL) {
            char *cwd = File_Cwd(NULL);
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
         if (strcmp(argv[i], "--background") == 0 ||
             strcmp(argv[i], "-b") == 0) {
            memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof *argv);
            argv[argc - 2] = NULL;
            break;
         }
      }

      if (!Hostinfo_Daemonize(argv[0],
                              argv,
                              HOSTINFO_DAEMONIZE_DEFAULT,
                              gState.pidFile, NULL, 0)) {
         goto exit;
      }
      return 0;
   }

   if (!ToolsCore_Setup(&gState)) {
      goto exit;
   }

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

   src = VMTools_NewSignalSource(SIGTERM);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src,
                            ToolsCoreSigHandler, gState.ctx.mainLoop, NULL);
   g_source_unref(src);

   src = VMTools_NewSignalSource(SIGUSR1);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src, ToolsCoreSigUsrHandler, NULL, NULL);
   g_source_unref(src);

   ret = ToolsCore_Run(&gState);

   ToolsCore_Cleanup(&gState);

   if (gState.pidFile != NULL) {
      g_unlink(gState.pidFile);
   }
exit:
   return ret;
}

