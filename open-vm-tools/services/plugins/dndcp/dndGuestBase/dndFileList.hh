/*********************************************************
 * Copyright (C) 2007-2017 VMware, Inc. All rights reserved.
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
 * dndFileList.hh
 *
 *      Translates filelists to different formats.
 */

#ifndef DND_FILELIST_HH
#define DND_FILELIST_HH

#include <string>
#include <vector>

#include "vm_basic_types.h"

extern "C" {
#include "dndClipboard.h"
}

#include "dynbuf.h"

class DnDFileList {
   public:
      DnDFileList();
      ~DnDFileList() {};

      void SetFileSize(uint64 fsize);
      uint64 GetFileSize() const;
      void AddFile(const std::string fullPath,
                   const std::string relPath);
      void AddFileUri(const std::string uriPath);
      void AddFiles(const std::vector<std::string> fullPathList,
                    const std::vector<std::string> relPathList);
      void AddFileAttributes(const CPFileAttributes& attributes);

      /* Copy paste/dndV2 V2 rpc */
      void SetRelPathsStr(const std::string inpath);

      /* DnDFileTransfer & V2 RPC */
      std::string GetRelPathsStr() const;
      std::string GetFullPathsStr(bool local) const;
      std::string GetUriPathsStr() const;

      /* UI Local clipboard */
      std::vector<std::string> GetRelPaths() const;
      std::vector<CPFileAttributes> GetFileAttributes() const;

      /* CPClipboard */
      bool ToCPClipboard(DynBuf *out, bool local) const;
      bool ToUriClipboard(DynBuf *out) const;
      bool AttributesToCPClipboard(DynBuf *out) const;
      bool FromCPClipboard(const void *buf, size_t len);
      bool AttributesFromCPClipboard(const void *buf, size_t len);

      void Clear();

   private:

      std::vector<std::string> mRelPaths;
      std::vector<std::string> mFullPaths;
      std::vector<std::string> mUriPaths;
      std::vector<CPFileAttributes> mAttributeList;
      std::string mFullPathsBinary;
      uint64 mFileSize;
};

#endif // DND_FILELIST_HH
