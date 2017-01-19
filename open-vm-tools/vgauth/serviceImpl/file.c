/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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
 * @file file.c --
 *
 *    Common file functions.  These are wrappers on glib calls
 *    that log errno.  Since glib uses the posix Win32 APIs, these
 *    should work everywhere.  Any OS-specific APIS are broken
 *    out into file{Posix,Win32}.c
 */

#include <errno.h>

#include "serviceInt.h"
#include "VGAuthLog.h"

/*
 ******************************************************************************
 * ServiceFileRenameFile --                                              */ /**
 *
 * Wrapper on g_rename() that logs errno details.
 *
 * @param[in]   srcName       The source file name.
 * @param[in]   dstName       The destination file name.
 *
 * @return 0 on success, -1 on error
 *
 ******************************************************************************
 */

int
ServiceFileRenameFile(const gchar *srcName,
                      const gchar *dstName)
{
   int ret;

   ret = g_rename(srcName, dstName);
   if (ret < 0) {
      Warning("%s: g_rename(%s, %s) failed (%d)\n", __FUNCTION__, srcName,
              dstName, errno);
   }

   return ret;
}


/*
 ******************************************************************************
 * ServiceFileUnlinkFile --                                              */ /**
 *
 * Wrapper on g_unlink() that logs errno details.
 *
 * @param[in]   fileName       The file name.
 *
 * @return 0 on success, -1 on error
 *
 ******************************************************************************
 */

int
ServiceFileUnlinkFile(const gchar *fileName)
{
   int ret;

   ret = g_unlink(fileName);
   if (ret < 0) {
      VGAUTH_LOG_ERR_POSIX("g_unlink(%s) failed", fileName);
   }

   return ret;
}

