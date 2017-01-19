/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * @file xioError.c
 *
 * Handles responding to X11 I/O errors.
 */

/* Include first.  Sets G_LOG_DOMAIN. */
#include "desktopEventsInt.h"

#include <sys/types.h>
#include <X11/Xlib.h>

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <unistd.h>

#include "vmware/tools/desktopevents.h"


static int gParentPid;
static ToolsAppCtx *gCtx;
static XIOErrorHandler gOrigHandler;


/*
 ******************************************************************************
 * DEXIOErrorHandler --                                                 */ /**
 *
 * Handler for all X I/O errors. Xlib documentation says we should not
 * return when handling I/O errors.
 *
 * @param[in] dpy    Unused.
 *
 * @return 1 (but doesn't really return).
 *
 ******************************************************************************
 */

static int
DEXIOErrorHandler(Display *dpy)
{
   pid_t my_pid = getpid();

   /*
    * ProcMgr_ExecAsync() needs to fork off a child to handle watching the
    * process being run.  When it dies, it will come through here, so we don't
    * want to let it shut down the RPC channel.
    */
   if (my_pid == gParentPid) {
      g_debug("%s", __func__);

      /*
       * Inform clients capable of/interested in quick'n'dirty cleanup upon an
       * X I/O error.
       */
      g_message("Emitting %s due to X I/O error.\n", TOOLS_CORE_SIG_XIOERROR);
      g_signal_emit_by_name(gCtx->serviceObj, TOOLS_CORE_SIG_XIOERROR, gCtx);

      /*
       * XXX: the really correct thing to do here would be to properly stop all
       * plugins so that capabilities are unset and all other "clean shutdown"
       * tasks are performed. Unfortunately two things currently prevent that:
       *
       * . we can't rely on g_main_loop_quit() because we can't return from this
       *   function (well, we can, but Xlib will exit() before vmtoolsd is able
       *   to clean up things), so the main loop will never regain control off
       *   the app.
       *
       * . we can't access the internal vmtoolsd functions that cleanly shuts
       *   down plugins.
       *
       * So, right now, let's stick with just stopping the RPC channel so that
       * the host is notified the application is gone. This may cause temporary
       * issues with clients that only look at capabilities and not at the
       * status of vmusr.
       */
      if (gCtx->rpc != NULL) {
         RpcChannel_Stop(gCtx->rpc);
      }
      exit(EXIT_FAILURE);
   } else {
      /*
       * _exit is used here so that any atexit() registered routines don't
       * interfere with any resources shared with the parent.
       */
      g_debug("%s hit from forked() child", __func__);
      _exit(EXIT_FAILURE);
   }

   return 1;
}


/*
 ******************************************************************************
 * XIOError_Init --                                                     */ /**
 *
 * Sets up an X11 I/O error callback that stops the daemon.
 *
 * @param[in] ctx       Application context.
 * @param[in] pdata     Registration data.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

gboolean
XIOError_Init(ToolsAppCtx *ctx,
              ToolsPluginData *pdata)
{
   gCtx = ctx;
   gParentPid = getpid();
   gOrigHandler = XSetIOErrorHandler(DEXIOErrorHandler);

   g_signal_new(TOOLS_CORE_SIG_XIOERROR,
                G_OBJECT_TYPE(ctx->serviceObj),
                0,      // GSignalFlags
                0,      // class offset
                NULL,   // accumulator
                NULL,   // accu_data
                g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE,
                1,
                G_TYPE_POINTER);

   return TRUE;
}


/*
 ******************************************************************************
 * XIOError_Shutdown --                                                 */ /**
 *
 * Shutdown function, restores the original X I/O error handler.
 *
 * @param[in] ctx   Application context.
 * @param[in] pdata Plugin data (unused).
 *
 ******************************************************************************
 */

void
XIOError_Shutdown(ToolsAppCtx *ctx,
                  ToolsPluginData *pdata)
{
   XSetIOErrorHandler(gOrigHandler);
   gCtx = NULL;
   gOrigHandler = NULL;
}

