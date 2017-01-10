/*********************************************************
 * Copyright (C) 2015-2016 VMware, Inc. All rights reserved.
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
 * hgfsPosix.c --
 *
 *    Provides a library for guest applications to convert local pathames to
 *    x-vmware-share:// style URIs
 */

#if !defined __linux__ && !defined __APPLE__ && !defined __FreeBSD__
#   error This file should not be compiled
#endif

#include "vmware.h"
#include "debug.h"
#include "str.h"
#include <glib.h>

#include "hgfsUri.h"
#include "hgfsHelper.h"

#include "util.h"
#include "unicode.h"
#include "hgfsEscape.h"

#include "ghIntegrationCommon.h"    // For GHI_HGFS_SHARE_URL_UTF8


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUri_ConvertFromPathToHgfsUri --
 *
 *    Test the UTF8 pathname to see if it's on an HGFS Share. If it is
 *    construct a UTF8 URI in the form of x-vmware-share://share_name/item.txt.
 *    If not, convert to a regular UTF8 URI string.
 *
 * Results:
 *    Filename as UTF8 URI string if success, NULL if failed.
 *
 * Side effects:
 *    Memory may be allocated for the returned string.
 *
 *-----------------------------------------------------------------------------
 */

char *
HgfsUri_ConvertFromPathToHgfsUri(const char *pathName,  // IN: path to convert
                                 Bool hgfsOnly)         // IN
{
   char *shareUri = NULL;
   Bool isHgfsName = FALSE;
   char *sharesDefaultRootPath = NULL;

   /* We can only operate on full paths. */
   if (pathName[0] != DIRSEPC) {
      return shareUri;
   }

   /* Retrieve the servername & share name in use. */
   if (!HgfsHlpr_QuerySharesDefaultRootPath(&sharesDefaultRootPath)) {
      Debug("%s: Unable to query shares default root path\n", __FUNCTION__);
      goto exit;
   }

   if (Unicode_StartsWith(pathName, sharesDefaultRootPath)) {
      char *relativeSharePath = NULL;
      char *escapedSharePath = NULL;
      UnicodeIndex relativePathStart = strlen(sharesDefaultRootPath);
      if (   strlen(pathName) > relativePathStart
          && pathName[relativePathStart] == DIRSEPC) {
         relativePathStart++;
      }
      relativeSharePath = Unicode_RemoveRange(pathName, 0, relativePathStart);
      HgfsEscape_Undo(relativeSharePath, strlen(relativeSharePath) + 1);
      escapedSharePath = g_uri_escape_string(relativeSharePath, "/", FALSE);
      shareUri = Unicode_Append(GHI_HGFS_SHARE_URL_UTF8, escapedSharePath);
      g_free(escapedSharePath);
      free(relativeSharePath);
      isHgfsName = TRUE;
   }

exit:
   if (!isHgfsName && !hgfsOnly) {
      /* Only convert non-hgfs file name if hgfsOnly is not set. */
      char *escapedPath = g_uri_escape_string(pathName, "/", FALSE);
      shareUri = Str_Asprintf(NULL,
                              "file://%s",
                               escapedPath);
      g_free(escapedPath);
   }
   HgfsHlpr_FreeSharesRootPath(sharesDefaultRootPath);
   return shareUri;
}
