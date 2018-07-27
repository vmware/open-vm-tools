/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * @vmGuestDnDSrc.cc --
 *
 * The inherited implementation of common class GuestDnDSrc in VM side.
 */

#include "util.h"
#include "vmGuestDnDSrc.hh"

#include "file.h"
#include "str.h"


/**
 * Constructor.
 *
 * @param[in] mgr guest DnD manager
 */

VMGuestDnDSrc::VMGuestDnDSrc(GuestDnDMgr *mgr)
   : GuestDnDSrc(mgr)
{
}


/**
 * Creates a directory for file transfer. If the destination dir is provided,
 * we will attempt to copy files to that directory.
 *
 * @param[in] destDir the preferred destination directory
 *
 * @return the destination directory on success, an empty string on failure.
 */

const std::string&
VMGuestDnDSrc::SetupDestDir(const std::string &destDir)
{
   mStagingDir = "";

   if (!destDir.empty() && File_Exists(destDir.c_str())) {
      mStagingDir = destDir;
      const char *lastSep = Str_Strrchr(mStagingDir.c_str(), DIRSEPC);
      if (lastSep && lastSep[1] != '\0') {
         mStagingDir += DIRSEPS;
      }

      return mStagingDir;
   } else {
      char *newDir;

      newDir = DnD_CreateStagingDirectory();
      if (newDir != NULL) {
         mStagingDir = newDir;

         char *lastSep = Str_Strrchr(newDir, DIRSEPC);
         if (lastSep && lastSep[1] != '\0') {
            mStagingDir += DIRSEPS;
         }
         free(newDir);
         g_debug("%s: destination dir is: %s", __FUNCTION__, mStagingDir.c_str());

         return mStagingDir;
      } else {
         g_debug("%s: destination dir is not created", __FUNCTION__);
         return mStagingDir;
      }
   }
}


/**
 * Clean the staging files.
 *
 * @param[in] fileTransferResult the file transfer result
 */

void
VMGuestDnDSrc::CleanStagingFiles(bool fileTransferResult)
{
   if (!fileTransferResult && !mStagingDir.empty()) {
      /* Delete all files if host canceled the file transfer. */
      DnD_DeleteStagingFiles(mStagingDir.c_str(), FALSE);
      mStagingDir.clear();
   }
}
