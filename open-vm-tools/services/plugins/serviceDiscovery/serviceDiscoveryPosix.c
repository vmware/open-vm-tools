/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
      for (;;) {
         size_t readBytes;
         char buf[SERVICE_DISCOVERY_VALUE_MAX_SIZE];
         readBytes = fread(buf, 1, sizeof(buf), f);
         g_debug("%s: readBytes = %" G_GSSIZE_FORMAT "\n", __FUNCTION__,
                 readBytes);

         if (readBytes > 0) {
            DynBuf_Append(out, buf, readBytes);
         }

         if (readBytes < sizeof(buf)) {
            break;
         }
      }
      if (fclose(f) != 0) {
         g_warning("%s: Failed to close file stream, errno=%d",
                   __FUNCTION__, errno);
      }
   }
}


/*
 *****************************************************************************
 * PublishScriptOutputToNamespaceDB --
 *
 * Spawns child process for script, reads stdout from pipe, writes
 * generated chunks to Namespace DB.
 *
 * Chunk count will be written to Namespace DB using received
 * "key" (ie. get-listening-process-info).
 *
 * Chunks will be written to Namespace DB with keys constructed by "key-[i]"
 * template (ie. get-listening-process-info-1, get-listening-process-info-2)
 *
 * @param[in] ctx             Application context.
 * @param[in] key             Key used for chunk count
 * @param[in] script          Script to be executed
 *
 * @retval TRUE  Script execution and Namespace DB write over RPC succeeded.
 * @retval FALSE Either of script execution or Namespace DB write failed.
 *
 *****************************************************************************
 */

Bool
PublishScriptOutputToNamespaceDB(ToolsAppCtx *ctx,
                                 const char *key,
                                 const char *script)
{
   Bool status = FALSE;
   GPid pid;
   gchar *command = g_strdup(script);
   gchar *cmd[] = { command, NULL };
   gint child_stdout = -1;
   gint child_stderr = -1;
   FILE* child_stdout_f;
   GError *p_error = NULL;
   DynBuf err;
   int i = 0;

   status = g_spawn_async_with_pipes(NULL, cmd, NULL, G_SPAWN_DEFAULT, NULL,
                                     NULL, &pid, NULL, &child_stdout, &child_stderr,
                                     &p_error);
   if (!status) {
      if (p_error != NULL) {
         g_warning("%s: Error during script exec %s\n", __FUNCTION__,
                   p_error->message);
         g_error_free(p_error);
      } else {
         g_warning("%s: Command not run\n", __FUNCTION__);
      }
      g_free(command);
      return status;
   }

   g_debug("%s: Child process spawned for %s\n", __FUNCTION__, key);

   child_stdout_f = fdopen(child_stdout, "r");
   if (child_stdout_f == NULL) {
      g_warning("%s: Failed to create file stream for child stdout, errno=%d",
                __FUNCTION__, errno);
      status = FALSE;
      goto out;
   }

   for (;;) {
      char buf[SERVICE_DISCOVERY_VALUE_MAX_SIZE];
      size_t readBytes = fread(buf, 1, sizeof(buf), child_stdout_f);

      g_debug("%s: readBytes = %" G_GSSIZE_FORMAT " status = %s\n",
              __FUNCTION__, readBytes, status ? "TRUE" : "FALSE");
      // At first iteration we are sure that status is true
      if (status && readBytes > 0) {
         gchar* msg = g_strdup_printf("%s-%d", key, ++i);
         status = WriteData(ctx, msg, buf, readBytes);
         if (!status) {
             g_warning("%s: Failed to store data\n", __FUNCTION__);
         }
         g_free(msg);
      }

      if (readBytes < sizeof(buf)) {
         if (!status) {
            g_warning("%s: Data read finished but failed to store\n", __FUNCTION__);
         }
         break;
      }
   }

   if (status) {
      gchar *chunkCount = g_strdup_printf("%d", i);
      status = WriteData(ctx, key, chunkCount, strlen(chunkCount));
      if (status) {
         g_debug("%s: Written key %s chunks %s\n", __FUNCTION__, key, chunkCount);
      }
      g_free(chunkCount);
   }

   DynBuf_Init(&err);
   ReadFromHandle(child_stderr, &err);
   child_stderr = -1;
   if (DynBuf_GetSize(&err) != 0) {
      DynBuf_AppendString(&err, "");
      g_debug("%s: stderr=%s\n", __FUNCTION__, (const char *) DynBuf_Get(&err));
   }
   DynBuf_Destroy(&err);
out:
   g_free(command);
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
   return status;
}
