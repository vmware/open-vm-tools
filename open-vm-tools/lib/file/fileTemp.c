/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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

#if defined(_WIN32)
#include <windows.h>
#endif

#include "vmware.h"
#include "log.h"
#include "file.h"
#include "util.h"
#include "unicodeOperations.h"


/*
 *----------------------------------------------------------------------
 *
 * File_MakeSafeTemp --
 *
 *      Exactly the same as File_MakeTemp except uses a safe directory
 *      as the default temporary directory.
 *
 * Results:
 *      Open file descriptor or -1
 *
 * Side effects:
 *      Creates a file if successful.
 *----------------------------------------------------------------------
 */

int
File_MakeSafeTemp(ConstUnicode tag,  // IN (OPT):
                  Unicode *presult)  // OUT:
{
   int fd = -1;
   Unicode dir = NULL;
   Unicode fileName = NULL;

   *presult = NULL;

   if (tag && File_IsFullPath(tag)) {
      File_GetPathName(tag, &dir, &fileName);
   } else {
      dir = File_GetSafeTmpDir(TRUE);
      fileName = Unicode_Duplicate(tag ? tag : "vmware");
   }

   fd = File_MakeTempEx(dir, fileName, presult);

   Unicode_Free(dir);
   Unicode_Free(fileName);

   return fd;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_DoesVolumeSupportAcls --
 *
 *    Determines if the volume that the pathname resides on supports
 *    ACLs.
 *
 * Results:
 *    TRUE   it does
 *    FALSE  it doesn't
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_DoesVolumeSupportAcls(ConstUnicode path)  // IN:
{
   Bool succeeded = FALSE;

#if defined(_WIN32)
   Bool res;
   Unicode vol, vol2;
   const utf16_t *vol2W;
   DWORD fsFlags;

   ASSERT(path);

   File_SplitName(path, &vol, NULL, NULL);
   vol2 = Unicode_Append(vol, DIRSEPS);

   vol2W = UNICODE_GET_UTF16(vol2);
   res = GetVolumeInformationW(vol2W, NULL, 0, NULL, NULL, &fsFlags, NULL, 0);
   UNICODE_RELEASE_UTF16(vol2W);

   if (res) {
      if ((fsFlags & FS_PERSISTENT_ACLS) == 0) {
         goto exit;
      }
   } else {
      Log("%s: GetVolumeInformation failed: %d\n", __FUNCTION__,
          GetLastError());
      goto exit;
   }

   succeeded = TRUE;

  exit:
   Unicode_Free(vol);
   Unicode_Free(vol2);
#endif

   return succeeded;
}
