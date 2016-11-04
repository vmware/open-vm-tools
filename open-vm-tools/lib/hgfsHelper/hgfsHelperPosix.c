/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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
 * hgfsHelperPosix.c --
 *
 *    Provides a posix helper library for guest applications to access
 *    the HGFS file system.
 *
 */

#if !defined __linux__ && !defined __FreeBSD__ && !defined sun && !defined __APPLE__
#   error This file should not be compiled
#endif

#include "vmware.h"
#include "debug.h"

#include "hgfsHelper.h"

#if defined __linux__
#define HGFSHLPR_DEFAULT_MOUNT_PATH      "/mnt/hgfs"
#elif defined sun
#define HGFSHLPR_DEFAULT_MOUNT_PATH      "/hgfs"
#elif defined __APPLE__
#define HGFSHLPR_DEFAULT_MOUNT_PATH      "/Volumes/VMware Shared Folders"
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHlpr_QuerySharesDefaultRootPath --
 *
 *      Queries the driver for its share's root paths.
 *      Currently only one is expected to be supported
 *      and returned, although later versions may not.
 *      E.g. "/mnt/hgfs" is the root path to
 *      the HGFS shares.
 *
 * Results:
 *      TRUE always.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHlpr_QuerySharesDefaultRootPath(char **hgfsRootPath)
{
#if defined __FreeBSD__
   return FALSE;
#else
   ASSERT(hgfsRootPath != NULL);

   *hgfsRootPath = Unicode_AllocWithUTF8(HGFSHLPR_DEFAULT_MOUNT_PATH);

   Debug("%s: HGFS shares root path name \"%s\"\n",
         __FUNCTION__, *hgfsRootPath);

   return TRUE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHlpr_FreeSharesRootPath --
 *
 *      Frees the share's root paths previously returned
 *      to the caller from the HgfsHlpr_QuerySharesRootPath.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsHlpr_FreeSharesRootPath(char *hgfsRootPath)
{
   free(hgfsRootPath);
}
