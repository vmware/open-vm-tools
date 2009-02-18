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

/*
 * toolboxInt.c --
 *
 *    Internal, shared functions for the GTK toolbox.
 */

#include <errno.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include "conf.h"
#include "debug.h"
#include "guestApp.h"
#include "util.h"
#include "vmtools.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Toolbox_LoadToolsConf --
 *
 *    Load the Tools configuration file from the default location.
 *
 *    XXX: This function is temporary until the GTK toolbox is refactored
 *    to be able to use vmtoolslib.
 *
 * Results:
 *    The config object. If loading the data fails, returns an empty conf object.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

GKeyFile *
Toolbox_LoadToolsConf(void)
{
   gchar *path = VMTools_GetToolsConfFile();
   GKeyFile *config;

   config = VMTools_LoadConfig(path,
                               G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                               TRUE);

   if (config == NULL) {
      Debug("Unable to load config file.\n");
      config = g_key_file_new();
   }

   g_free(path);
   return config;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Toolbox_SaveToolsConf --
 *
 *    Saves the given config data to the default tools config file location.
 *
 *    XXX: This function is temporary until the GTK toolbox is refactored
 *    to be able to use vmtoolslib.
 *
 * Results:
 *    Whether saving was successful.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
Toolbox_SaveToolsConf(GKeyFile *config)   // IN
{
   gboolean ret = FALSE;
   gchar *path = NULL;
   GError *err = NULL;

   path = VMTools_GetToolsConfFile();
   ret = VMTools_WriteConfig(path, config, &err);

   if (!ret) {
      Warning("Error saving conf data: %s\n", err->message);
      g_clear_error(&err);
   }

   g_free(path);
   return ret;
}

