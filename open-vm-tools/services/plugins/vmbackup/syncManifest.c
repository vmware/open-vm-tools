/*********************************************************
 * Copyright (C) 2017-2019 VMware, Inc. All rights reserved.
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
 * syncManifest.c --
 *
 * Implements a simple xml-based backup manifest file for quiesced snapshots
 * on Linux.
 */

#include "vmBackupInt.h"
#include "syncDriver.h"
#include "syncManifest.h"
#include "vm_tools_version.h"
#include "vmware/tools/log.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/*
 * Name and format of the manifest file.
 */
static const char syncManifestName[] = "quiesce_manifest.xml";
static const char syncManifestFmt[] = {
   "<quiesceManifest>\n"
   "   <productVersion>%d</productVersion>\n"  /* version of tools */
   "   <providerName>%s</providerName>\n"      /* name of backend provider */
   "</quiesceManifest>\n"
};

/*
 * tools.conf switch to enable manifest generation
 */
static const char syncManifestSwitch[] = "enableXmlManifest";


/*
 *-----------------------------------------------------------------------------
 *
 * SyncNewManifest --
 *
 *    Create a new SyncManifest structure.
 *
 * Results:
 *    Pointer to structure on success, otherwise NULL.
 *
 *-----------------------------------------------------------------------------
 */

SyncManifest *
SyncNewManifest(VmBackupState *state,          // IN
                SyncDriverHandle handle)       // IN
{
   Bool providerQuiesces;
   const char *providerName;
   SyncManifest *manifest;

   if (!VMTools_ConfigGetBoolean(state->ctx->config, "vmbackup",
                                 syncManifestSwitch, TRUE)) {
      g_debug("No backup manifest - %s is false\n", syncManifestSwitch);
      return NULL;
   }

   if (!state->generateManifests) {
      g_debug("No backup manifest requested\n");
      return NULL;
   }

   SyncDriver_GetAttr(handle, &providerName, &providerQuiesces);
   if (!providerQuiesces) {
      g_debug("No backup manifest needed since using "
              "non-quiescing backend.\n");
      return NULL;
   }

   manifest = g_new0(SyncManifest, 1);
   manifest->path = g_strdup_printf("%s/%s", state->configDir,
                                    syncManifestName);
   manifest->providerName = g_strdup(providerName);
   return manifest;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncManifestRelease --
 *
 *    Free a manifest structure.
 *
 * Results:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
SyncManifestRelease(SyncManifest *manifest)    // IN
{
   if (manifest != NULL) {
      g_free(manifest->path);
      g_free(manifest->providerName);
      g_free(manifest);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncManifestSend --
 *
 *    Generate manifest file and send it to vmx.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    On success, writes out the backup manifest file for a quiesced
 *    snapshot and sends the file's path to vmx.
 *
 *-----------------------------------------------------------------------------
 */

Bool
SyncManifestSend(SyncManifest *manifest)       // IN
{
   FILE *f;
   int ret;

   unlink(manifest->path);
   f = fopen(manifest->path, "w");
   if (f == NULL) {
      g_warning("Error opening backup manifest file %s\n",
                manifest->path);
      return FALSE;
   }

   ret = fprintf(f, syncManifestFmt, TOOLS_VERSION_CURRENT,
                 manifest->providerName);
   fclose(f);
   if (ret < 0) {
      g_warning("Error writing backup manifest file %s: %d %s\n",
                manifest->path, errno, strerror(errno));
      return FALSE;
   }

   if (!VmBackup_SendEventNoAbort(VMBACKUP_EVENT_GENERIC_MANIFEST,
                                  VMBACKUP_SUCCESS, manifest->path)) {
      /* VmBackup_SendEventNoAbort logs the error */
      g_info("Non-fatal error occurred while sending %s, continuing "
             "with the operation", VMBACKUP_EVENT_GENERIC_MANIFEST);
      return FALSE;
   }

   g_debug("Backup manifest was sent successfully.\n");
   return TRUE;
}
