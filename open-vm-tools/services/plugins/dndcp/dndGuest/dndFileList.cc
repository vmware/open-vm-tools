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
 * dndFileList.cc
 *
 *      Handles filelists for CPClipboard.
 *      Relative paths are read/write to allow placing data on the clipboard,
 *      but full paths are write only. Full path parsing depends on g->h/h->g
 *      as well as dnd/fcp versions. Given that the host ui never needs to
 *      parse it, full paths are only stored in binary format for consumption
 *      by the vmx.
 *
 *      Local relative paths are expected to be encoded in Normalized UTF8 in
 *      local format.
 *
 *      XXX This file is almost identical to bora/apps/lib/cui/dndFileList.cc
 *      but right now we can not find a way to share the file.
 */

#if defined (_WIN32)
/*
 * When compile this file for Windows dnd plugin dll, there may be a conflict
 * between CRT and MFC libraries.  From
 * http://support.microsoft.com/default.aspx?scid=kb;en-us;q148652: The CRT
 * libraries use weak external linkage for the DllMain function. The MFC
 * libraries also contain this function. The function requires the MFC
 * libraries to be linked before the CRT library. The Afx.h include file
 * forces the correct order of the libraries.
 */
#include <afx.h>
#endif

#include "dndFileList.hh"

extern "C" {
   #include "dndClipboard.h"
   #include "cpNameUtil.h"
}

#include "vm_assert.h"
#include "file.h"

/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList --
 *
 *      Constructor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

DnDFileList::DnDFileList()
{
   mFileSize = 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::SetFileSize --
 *
 *      Sets the expected size of the files.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDFileList::SetFileSize(uint64 fsize)  // IN: The size of the files.
{
   mFileSize = fsize;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::GetFileSize --
 *
 *      Gets the size of the files if known.
 *
 * Results:
 *      0 if the size of the files is not known, otherwise the expected size of
 *      the files.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

uint64
DnDFileList::GetFileSize() const
{
   return mFileSize;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::AddFile --
 *
 *      Add the full path and the relative path to the file list. Both strings
 *      should be normalized UTF8.
 *      Will not add file if the file list was created from a clipboard buffer.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDFileList::AddFile(const std::string fullPath,        // IN: filename
                     const std::string relPath)         // IN: filename
{
   ASSERT(mFullPathsBinary.empty());

   if (!mFullPathsBinary.empty()) {
      return;
   }

   mRelPaths.push_back(relPath);
   mFullPaths.push_back(fullPath);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::AddFileUri --
 *
 *      Add the full uri UTF8 path the file list.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDFileList::AddFileUri(const std::string uriPath) // IN
{
   mUriPaths.push_back(uriPath);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::AddFiles --
 *
 *      Add the full path list and the relative path list. Both lists should be
 *      normalized UTF8.
 *
 *      Will not add lists if the file list was created from a clipboard buffer.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDFileList::AddFiles(const std::vector<std::string> fullPathList, // IN: filenames
                      const std::vector<std::string> relPathList)  // IN: filenames
{
   ASSERT(mFullPathsBinary.empty());

   if (!mFullPathsBinary.empty()) {
      return;
   }

   mRelPaths = relPathList;
   mFullPaths = fullPathList;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::AddFileAttributes --
 *
 *      Adds files attributes.
 *
 *      Will not add attributes if the file list was created from a clipboard
 *      buffer.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDFileList::AddFileAttributes(const CPFileAttributes& attributes)  // IN
{
   ASSERT(mFullPathsBinary.empty());

   if (!mFullPathsBinary.empty()) {
      return;
   }

   mAttributeList.push_back(attributes);
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::SetRelPathsStr --
 *
 *      Set the relative paths based on the serialized input string. Paths are
 *      in normalized utf-8 with local path separators. Exposed for dnd/cp
 *      version 2.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDFileList::SetRelPathsStr(const std::string inpath)   // IN:
{
   std::string::size_type mPos = 0;
   std::string curFile;
   std::string path;
   const char* cpath;

   if (inpath.empty()) {
      return;
   }

   if (inpath[inpath.size()-1] != '\0') {
      path = inpath + '\0';
   } else {
      path = inpath;
   }

   cpath = path.c_str();
   mRelPaths.clear();
   curFile = cpath;
   mPos = path.find('\0', mPos);

   while (mPos != std::string::npos) {
      mPos++;
      mRelPaths.push_back(curFile);
      curFile = cpath + mPos;
      mPos = path.find('\0', mPos);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::GetRelPaths --
 *
 *      Gets a vector containing the reletive paths of the files.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

std::vector<std::string>
DnDFileList::GetRelPaths() const
{
   return mRelPaths;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::GetFileAttributes --
 *
 *      Gets a vector containing file attributes.
 *
 * Results:
 *      File attributes.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

std::vector<CPFileAttributes>
DnDFileList::GetFileAttributes()
   const
{
   return mAttributeList;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::GetFullPathsStr --
 *
 *      Gets the serialized version of the full paths. If the file list was
 *      created from a CPClipboard, use the serialized input instead.
 *
 *      Local paths are serialized inot NUL separated pathes.
 *      CPName paths are converted from local to CPName and serialized into
 *      uint32,cpname pairs.
 *
 * Results:
 *      An std::string containing the serialized full paths. Empty string on
 *      error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

std::string
DnDFileList::GetFullPathsStr(bool local)        // IN: Use local format
   const
{
   std::string stringList;
   std::vector<std::string>::const_iterator i;

   if (mFullPathsBinary.empty() && !mFullPaths.empty()) {
      for (i = mFullPaths.begin(); i != mFullPaths.end(); ++i) {
         if (local) {
            stringList.append(i->c_str());
            stringList.push_back('\0');
         } else {
            char outPath[FILE_MAXPATH + 100];
            int32 outPathLen;

            outPathLen = CPNameUtil_ConvertToRoot(i->c_str(),
                                                  sizeof outPath,
                                                  outPath);
            if (outPathLen < 0) {
               continue;
            }

            stringList.append(reinterpret_cast<char *>(&outPathLen),
                              sizeof outPathLen);
            stringList.append(outPath, outPathLen);
         }
      }

      return stringList;
   } else if (!mFullPathsBinary.empty() && mFullPaths.empty()) {
      return mFullPathsBinary;
   } else {
      return "";
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::GetRelPathsStr --
 *
 *      Gets the serialized version of the relative paths. Primarily used for
 *      dnd/cp v2.
 *
 * Results:
 *      An std::string contatin the serialized full paths.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

std::string
DnDFileList::GetRelPathsStr()
   const
{
   std::string stringList("");
   std::vector<std::string>::const_iterator i;

   for (i = mRelPaths.begin(); i != mRelPaths.end(); ++i) {
      stringList.append(i->c_str());
      stringList.push_back('\0');
   }
   return stringList;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::GetUriPathsStr --
 *
 *      Gets the serialized version of the uri paths.
 *
 * Results:
 *      An std::string contatin the serialized uri paths.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

std::string
DnDFileList::GetUriPathsStr(void)
   const
{
   std::string stringList;
   std::vector<std::string>::const_iterator i;

   for (i = mUriPaths.begin(); i != mUriPaths.end(); i++) {
      stringList.append(i->c_str());
      stringList.push_back('\0');
   }
   return stringList;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::FromCPClipboard --
 *
 *      Loads the filelist from a buffer, typically from CPClipboard.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

bool
DnDFileList::FromCPClipboard(const void *buf,   // IN: Source buffer
                             size_t len)        // IN: Buffer length
{
   const CPFileList *flist;
   std::string relPaths;

   ASSERT(buf);
   ASSERT(len);
   if (!buf || !len) {
      return false;
   }

   flist = reinterpret_cast<const CPFileList *>(buf);
   relPaths.assign(
      reinterpret_cast<const char *>(flist->filelists), flist->relPathsLen);

   mRelPaths.clear();
   mFullPaths.clear();

   mFileSize = flist->fileSize;

   SetRelPathsStr(relPaths);
   mFullPathsBinary.assign(
      reinterpret_cast<const char *>(flist->filelists + flist->relPathsLen),
      flist->fulPathsLen);

   return true;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::AttributesFromCPClipboard --
 *
 *      Loads the attribute list from a buffer, typically from CPClipboard.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

bool
DnDFileList::AttributesFromCPClipboard(const void *buf, // IN: Source buffer
                                       size_t len)      // IN: Buffer length
{
   const CPAttributeList *alist;

   ASSERT(buf);
   ASSERT(len);
   if (!buf || !len) {
      return false;
   }

   alist = reinterpret_cast<const CPAttributeList *>(buf);

   mAttributeList.resize(alist->attributesLen);
   for (uint32 i = 0; i < alist->attributesLen; i++) {
      mAttributeList[i] = alist->attributeList[i];
   }

   return true;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::ToCPClipboard --
 *
 *      Serializes contents for CPClipboard in eithr CPFormat or local format.
 *
 * Results:
 *      false on error, true on success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

bool
DnDFileList::ToCPClipboard(DynBuf *out, // OUT: Initialized output buffer
                           bool local)  // IN: Use local format
   const
{
   std::string strListRel;
   std::string strListFul;
   CPFileList header;

   strListRel = GetRelPathsStr();
   strListFul = GetFullPathsStr(local);

   if (!out) {
      return false;
   }

   /* Check if the size is too big. */
   if (strListRel.size() > MAX_UINT32 ||
       strListFul.size() > MAX_UINT32) {
      return false;
   }

   header.fileSize = mFileSize;
   header.relPathsLen = strListRel.size();
   header.fulPathsLen = strListFul.size();

   DynBuf_Append(out, &header, CPFILELIST_HEADER_SIZE);
   DynBuf_Append(out, strListRel.c_str(), header.relPathsLen);
   DynBuf_Append(out, strListFul.c_str(), header.fulPathsLen);

   return true;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::ToCPClipboard --
 *
 *      Serializes uri paths for CPClipboard in URI Format.
 *
 * Results:
 *      false on error, true on success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

bool
DnDFileList::ToUriClipboard(DynBuf *out) // OUT: Initialized output buffer
   const
{
   std::string strListUri;
   UriFileList header;

   if (!out) {
      return false;
   }

   strListUri = GetUriPathsStr();

   /* Check if the size is too big. */
   if (strListUri.size() > MAX_UINT32) {
      return false;
   }

   header.fileSize = mFileSize;
   header.uriPathsLen = strListUri.size();

   DynBuf_Append(out, &header, URI_FILELIST_HEADER_SIZE);
   DynBuf_Append(out, strListUri.c_str(), header.uriPathsLen);

   return true;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::AttributesToCPClipboard --
 *
 *      Serializes attributes for CPClipboard in Attributes Format.
 *
 * Results:
 *      false on error, true on success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

bool
DnDFileList::AttributesToCPClipboard(DynBuf *out) // IN
   const
{
   CPAttributeList header;

   if (!out) {
      return false;
   }

   header.attributesLen = mAttributeList.size();

   DynBuf_Append(out, &header, URI_ATTRIBUTES_LIST_HEADER_SIZE);
   if (header.attributesLen > 0) {
      DynBuf_Append(out,
                    &mAttributeList[0],
                    header.attributesLen * sizeof(CPFileAttributes));
   }

   return true;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDFileList::Clear --
 *
 *      Clears the contents of the filelist.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
DnDFileList::Clear()
{
   mRelPaths.clear();
   mFullPaths.clear();
   mUriPaths.clear();
   mAttributeList.clear();
   mFullPathsBinary.clear();
   mFileSize = 0;
}

