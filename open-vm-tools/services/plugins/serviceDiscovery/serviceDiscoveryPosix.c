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
 * serviceDiscoveryPosix.c --
 *
 * Captures the information about services inside the guest
 * and writes it to Namespace DB.
 */

#ifndef __linux__
#   error This file should not be compiled.
#endif

#include "serviceDiscoveryInt.h"
#include "dynbuf.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>


/*
 *****************************************************************************
 * ReadFromHandle --
 *
 * Reads from handle and appends to provided buffer. Handle is closed inside the
 * function after read operation.
 *
 * @param[in] h             handle.
 * @param[out] out          output buffer.
 *
 *****************************************************************************
 */

static void
ReadFromHandle(gint h,
               DynBuf* out)
{
   FILE* f = fdopen(h, "r");

   if (f == NULL) {
      g_warning("%s: Failed to create file stream, errno=%d",
                __FUNCTION__, errno);
      if (close(h) != 0) {
         g_warning("%s: Failed to close file handle, errno=%d",
                   __FUNCTION__, errno);
      }
   } else {
      DepleteReadFromStream(f, out);
      if (fclose(f) != 0) {
         g_warning("%s: Failed to close file stream, errno=%d",
                   __FUNCTION__, errno);
      }
   }
}

/*
 *****************************************************************************
 * ExecuteScript --
 *
 * Spawns child process for script, reads child process stdout stream and
 * sends data to Namespace DB and/or host-side gdp daemon.
 *
 * @param[in] ctx             The application context
 * @param[in] key             Script name
 * @param[in] script          Script to be executed
 *
 * @retval TRUE  Successfully executed script and sent output to gdp daemon.
 * @retval FALSE Otherwise.
 *
 *****************************************************************************
 */

Bool
ExecuteScript(ToolsAppCtx *ctx,
              const char *key,
              const char *script)
{
   Bool status;
   gchar *command = g_strdup(script);
   gchar *cmd[] = { command, NULL };
   gint child_stdout = -1;
   gint child_stderr = -1;
   FILE *child_stdout_f;
   GError *p_error = NULL;
   DynBuf err;

   status = g_spawn_async_with_pipes(NULL, // const gchar *working_directory
                                     cmd, // gchar **argv
                                     NULL, // gchar **envp
                                     G_SPAWN_DEFAULT, // GSpawnFlags flags
                                     NULL, // GSpawnChildSetupFunc child_setup
                                     NULL, // gpointer user_data
                                     NULL, // GPid *child_pid
                                     NULL, // gint *standard_input
                                     &child_stdout, // gint *standard_output
                                     &child_stderr, // gint *standard_error
                                     &p_error); // GError **error
   if (!status) {
      if (p_error != NULL) {
         g_warning("%s: Error during script exec %s\n",
                   __FUNCTION__, p_error->message);
         g_clear_error(&p_error);
      } else {
         g_warning("%s: Command not run\n", __FUNCTION__);
      }

      /*
       * If an error occurs, child_pid, standard_input, standard_output,
       * and standard_error will not be filled with valid values.
       */
      g_free(command);
      return status;
   }

   g_debug("%s: Child process spawned for %s\n", __FUNCTION__, key);

   status = FALSE;

   child_stdout_f = fdopen(child_stdout, "r");
   if (child_stdout_f == NULL) {
      g_warning("%s: Failed to create file stream for child stdout, errno=%d",
             __FUNCTION__, errno);
      goto out;
   }

   status = SendScriptOutput(ctx, key, child_stdout_f);

   DynBuf_Init(&err);
   ReadFromHandle(child_stderr, &err);
   child_stderr = -1;
   if (DynBuf_GetSize(&err) != 0) {
      DynBuf_AppendString(&err, "");
      g_debug("%s: stderr=%s\n",
             __FUNCTION__, (const char *) DynBuf_Get(&err));
   }
   DynBuf_Destroy(&err);

out:
   if (child_stdout_f != NULL) {
      if (fclose(child_stdout_f) != 0) {
         g_warning("%s: Failed to close child stdout file stream, errno=%d",
                __FUNCTION__, errno);
      }
   } else if (close(child_stdout) != 0) {
      g_warning("%s: Failed to close child stdout handle, errno=%d",
             __FUNCTION__, errno);
   }
   if (child_stderr != -1 && close(child_stderr) != 0) {
      g_warning("%s: Failed to close child process stderr handle, errno=%d",
             __FUNCTION__, errno);
   }

   g_free(command);
   return status;
}
