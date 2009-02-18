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
#include "system.h"
#include "unicode.h"
#include "vmtools.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

static ToolsServiceState gState = { NULL, };

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
   int ret = EXIT_FAILURE;
   GSource *src;

   Unicode_Init(argc, &argv, NULL);

   if (!ToolsCore_ParseCommandLine(&gState, argc, argv)) {
      goto exit;
   }

   if (!ToolsCore_Setup(&gState)) {
      goto exit;
   }

   src = VMTools_NewSignalSource(SIGHUP);
   VMTOOLSAPP_ATTACH_SOURCE(&gState.ctx, src,
                            ToolsCoreSigHandler, gState.ctx.mainLoop, NULL);
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

   if (gState.pidFile != NULL &&
       !System_Daemon(FALSE, FALSE, gState.pidFile)) {
      goto exit;
   }

   ret = ToolsCore_Run(&gState);

   ToolsCore_Cleanup(&gState);

   if (gState.pidFile != NULL) {
      g_unlink(gState.pidFile);
   }
exit:
   return ret;
}

