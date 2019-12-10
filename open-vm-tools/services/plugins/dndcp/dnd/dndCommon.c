/*********************************************************
 * Copyright (C) 2005-2019 VMware, Inc. All rights reserved.
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
 * dndCommon.c --
 *
 *     Implementation of bora/lib/public/dnd.h functions that are common to
 *     Linux and Windows platforms
 */


#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include "vmware.h"
#include "codeset.h"
#include "dndInt.h"
#include "dnd.h"
#include "dndClipboard.h"
#include "file.h"
#include "str.h"
#include "random.h"
#include "util.h"
#include "cpNameUtil.h"
#include "hgfsEscape.h"
#include "hgfsServerPolicy.h"
#include "hgfsVirtualDir.h"
#include "unicodeOperations.h"
#include "hostinfo.h"

#define LOGLEVEL_MODULE dnd
#include "loglevel_user.h"

#define WIN_DIRSEPC     '\\'
#define WIN_DIRSEPS     "\\"


#ifndef DND_IS_XDG
static const char *DnDCreateRootStagingDirectory(void);

/*
 *-----------------------------------------------------------------------------
 *
 * DnD_CreateStagingDirectory --
 *
 *    Generate a unique staging directory name, create the directory, and
 *    return the name. The caller is responsible for freeing the returned
 *    string.
 *
 *    Our staging directory structure is comprised of a "root" staging
 *    directory that itself contains multiple staging directories that are
 *    intended to be used on a per-DnD and per-user basis.  That is, each DnD
 *    by a particular user will have its own staging directory within the root.
 *    Sometimes these directories are emptied after the DnD (either because it
 *    was cancelled or the destination application told us to), and we resuse
 *    any empty directories that we can.  This function will return a directory
 *    to be reused if possible and fall back on creating a new one if
 *    necessary.
 *
 * Results:
 *    A string containing the newly created name, or NULL on failure.
 *
 * Side effects:
 *    A directory is created
 *
 *-----------------------------------------------------------------------------
 */

char *
DnD_CreateStagingDirectory(void)
{
   const char *root;
   char **stagingDirList;
   int numStagingDirs;
   int i;
   char *ret = NULL;
   Bool found = FALSE;

   /*
    * Make sure the root staging directory is created with the correct
    * permissions.
    */
   root = DnDCreateRootStagingDirectory();
   if (!root) {
      return NULL;
   }

   /* Look for an existing, empty staging directory */
   numStagingDirs = File_ListDirectory(root, &stagingDirList);
   if (numStagingDirs < 0) {
      goto exit;
   }

   for (i = 0; i < numStagingDirs; i++) {
      if (!found) {
         char *stagingDir;

         stagingDir = Unicode_Append(root, stagingDirList[i]);

         if (File_IsEmptyDirectory(stagingDir) &&
             DnDStagingDirectoryUsable(stagingDir)) {
               ret = Unicode_Append(stagingDir, DIRSEPS);
               /*
                * We can use this directory.  Make sure to continue to loop
                * so we don't leak the remaining stagindDirList[i]s.
                */
               found = TRUE;
         }

         free(stagingDir);
      }
   }

   Util_FreeStringList(stagingDirList, numStagingDirs);

   /* Only create a directory if we didn't find one above. */
   if (!found) {
      rqContext *context;

      context = Random_QuickSeed((unsigned)time(NULL));

      for (i = 0; i < 10; i++) {
         char *temp;

         /* Each staging directory is given a random name. */
         free(ret);
         temp = Unicode_Format("%08x%c", Random_Quick(context), DIRSEPC);
         VERIFY(temp);
         ret = Unicode_Append(root, temp);
         free(temp);

         if (File_CreateDirectory(ret) &&
             DnDSetPermissionsOnStagingDir(ret)) {
            found = TRUE;
            break;
         }
      }

      free(context);
   }

exit:
   if (!found && ret != NULL) {
      free(ret);
      ret = NULL;
   }

   return ret;
}
#endif // ifndef DND_IS_XDG


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_AppendPrefixToStagingDir --
 *
 *    Append prefix to a DnD staging directory
 *
 * Results:
 *    Return new DnD staging directory for success, NULL otherwise.
 *
 * Side effects:
 *    Caller must free the retrun string with free
 *
 *-----------------------------------------------------------------------------
 */
char *
DnD_AppendPrefixToStagingDir(const char *stagingDir, // IN:
                             const char *prefix)     // IN:
{
   const char *dndRoot = NULL;
   char *newDir = NULL;

   dndRoot = DnD_GetFileRoot();
   if (Unicode_Find(stagingDir, dndRoot) == UNICODE_INDEX_NOT_FOUND) {
      // incorrect staging directory
      Log("%s: Not find root = %s\n", __FUNCTION__, dndRoot);
      return NULL;
   }

   newDir = Unicode_Insert(stagingDir, Unicode_LengthInCodePoints(dndRoot), prefix);
   if (!File_Move(stagingDir, newDir, NULL)) {
      free(newDir);
      newDir = NULL;
   }
   return newDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_DeleteStagingFiles --
 *
 *    Attempts to delete all files in the staging directory. This does not
 *    delete the directory itself.
 *
 * Results:
 *    TRUE if all files were deleted. FALSE if there was an error.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DnD_DeleteStagingFiles(const char *stagingDir,  // IN:
                       Bool onReboot)           // IN:
{
   Bool ret = TRUE;

   ASSERT(stagingDir);

   if (!File_Exists(stagingDir)) {
      /* The stagingDir is already gone. */
      return TRUE;
   }

   if (!File_IsDirectory(stagingDir)) {
      return FALSE;
   }

   if (onReboot) {
      if (File_UnlinkDelayed(stagingDir)) {
         ret = FALSE;
      }
   } else {
      int i;
      int numFiles;
      char *base;
      char **fileList = NULL;

      /* get list of files in current directory */
      numFiles = File_ListDirectory(stagingDir, &fileList);

      if (numFiles == -1) {
         return FALSE;
      } else if (numFiles == 0) {
         return TRUE;
      }

      /* delete everything in the directory */
      base = Unicode_Append(stagingDir, DIRSEPS);

      for (i = 0; i < numFiles; i++) {
         char *curPath;

         curPath = Unicode_Append(base, fileList[i]);

         if (File_IsDirectory(curPath)) {
            if (!File_DeleteDirectoryTree(curPath)) {
               ret = FALSE;
            }
         } else {
            if (File_Unlink(curPath) == -1) {
               ret = FALSE;
            }
         }

         free(curPath);
      }

      free(base);
      Util_FreeStringList(fileList, numFiles);
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_RemoveTempDirs --
 *
 *    Remove all directories with the specific prefix in the staging directory.
 *
 * Results:
 *    TRUE if the specific directories were deleted. FALSE if there was an error.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DnD_RemoveTempDirs(const char *dndTempDir,  // IN:
                   const char *prefix)      // IN:
{
   Bool ret = TRUE;
   int i = 0;
   int numFiles = 0;
   char *base = NULL;
   char **fileList = NULL;

   ASSERT(dndTempDir);

   if (!File_Exists(dndTempDir)) {
      /* The dndTempDir doesn't exist. */
      return TRUE;
   }

   if (!File_IsDirectory(dndTempDir)) {
      return FALSE;
   }

   /* get list of files in current directory */
   numFiles = File_ListDirectory(dndTempDir, &fileList);
   if (numFiles == -1) {
      return FALSE;
   } else if (numFiles == 0) {
      return TRUE;
   }

   base = Unicode_Append(dndTempDir, DIRSEPS);
   for (i = 0; i < numFiles; i++) {
      char *curPath = Unicode_Append(base, fileList[i]);
      if (File_IsDirectory(curPath) &&
          (UNICODE_INDEX_NOT_FOUND != Unicode_Find(curPath, prefix))) {
         if (!File_DeleteDirectoryTree(curPath)) {
            ret = FALSE;
         }
      }
      free(curPath);
   }
   free(base);
   Util_FreeStringList(fileList, numFiles);
   return ret;
}


#ifndef DND_IS_XDG
/*
 *----------------------------------------------------------------------------
 *
 * DnDCreateRootStagingDirectory --
 *
 *    Checks if the root staging directory exists with the correct permissions,
 *    or creates it if necessary.
 *
 * Results:
 *    The path of the root directory on success, NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static const char *
DnDCreateRootStagingDirectory(void)
{
   const char *root;

   /*
    * DnD_GetFileRoot() gives us a pointer to a static string, so there's no
    * need to free anything.
    */
   root = DnD_GetFileRoot();
   if (!root) {
      return NULL;
   }

   if (File_Exists(root)) {
      if (!DnDRootDirUsable(root)) {
         /*
          * The directory already exists and its permissions are wrong.
          */
         Log("%s: The root dir is not usable.\n", __FUNCTION__);
         return NULL;
      }
   } else {
      if (!File_CreateDirectory(root) ||
          !DnDSetPermissionsOnRootDir(root)) {
         /* We couldn't create the directory or set the permissions. */
         return NULL;
      }
   }

   return root;
}
#endif // ifndef DND_IS_XDG


/*
 *----------------------------------------------------------------------------
 *
 * DnD_LegacyConvertToCPName --
 *
 *    Converts paths received from older tools that do not send data in CPName
 *    format across the backdoor.  Older tools send paths in Windows format so
 *    this implementation must always convert from Windows path to CPName path,
 *    regardless of the platform we are running on.
 *
 *    The logic here and in the called functions appears to be UTF8-safe.
 *
 * Results:
 *    On success, returns the number of bytes used in the cross-platform name,
 *    NOT including the final terminating NUL character.  On failure, returns
 *    a negative error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
DnD_LegacyConvertToCPName(const char *nameIn,   // IN:  Buffer to convert
                          size_t bufOutSize,    // IN:  Size of output buffer
                          char *bufOut)         // OUT: Output buffer
{
   const char partialName[] = HGFS_SERVER_POLICY_ROOT_SHARE_NAME;
   const size_t partialNameLen = HGFS_STR_LEN(HGFS_SERVER_POLICY_ROOT_SHARE_NAME);
   const char *partialNameSuffix = "";
   size_t partialNameSuffixLen;
   char *fullName;
   size_t fullNameSize;
   size_t nameSize;
   int result;

   ASSERT(nameIn);
   ASSERT(bufOut);

   /*
    * Create the full name. Note that Str_Asprintf should not be
    * used here as it uses FormatMessages which interprets 'data', a UTF-8
    * string, as a string in the current locale giving wrong results.
    */

   /*
    * Is this file path a UNC path?
    */
   if (nameIn[0] == WIN_DIRSEPC && nameIn[1] == WIN_DIRSEPC) {
      partialNameSuffix    = WIN_DIRSEPS HGFS_UNC_DIR_NAME WIN_DIRSEPS;
      partialNameSuffixLen = HGFS_STR_LEN(WIN_DIRSEPS) +
                             HGFS_STR_LEN(HGFS_UNC_DIR_NAME) +
                             HGFS_STR_LEN(WIN_DIRSEPS);
   } else {
      partialNameSuffix    = WIN_DIRSEPS HGFS_DRIVE_DIR_NAME WIN_DIRSEPS;
      partialNameSuffixLen = HGFS_STR_LEN(WIN_DIRSEPS) +
                             HGFS_STR_LEN(HGFS_DRIVE_DIR_NAME) +
                             HGFS_STR_LEN(WIN_DIRSEPS);
   }

   /* Skip any path separators at the beginning of the input string */
   while (*nameIn == WIN_DIRSEPC) {
      nameIn++;
   }

   nameSize = strlen(nameIn);
   fullNameSize = partialNameLen + partialNameSuffixLen + nameSize;
   fullName = Util_SafeMalloc(fullNameSize + 1);

   memcpy(fullName, partialName, partialNameLen);
   memcpy(fullName + partialNameLen, partialNameSuffix, partialNameSuffixLen);
   memcpy(fullName + partialNameLen + partialNameSuffixLen, nameIn, nameSize);
   fullName[fullNameSize] = '\0';

   LOG(4, ("%s: generated name is \"%s\"\n", __FUNCTION__, fullName));

   /*
    * CPName_ConvertTo implementation is performed here without calling any
    * CPName_ functions.  This is safer since those functions might change, but
    * the legacy behavior we are special casing here will not.
    */

   {
      char const *winNameIn = fullName;
      char const *origOut = bufOut;
      char const *endOut = bufOut + bufOutSize;
      char const pathSep = WIN_DIRSEPC;
      char *ignores = ":";

      /* Skip any path separators at the beginning of the input string */
      while (*winNameIn == pathSep) {
         winNameIn++;
      }

      /*
       * Copy the string to the output buf, converting all path separators into
       * '\0' and ignoring the specified characters.
       */

      for (; *winNameIn != '\0' && bufOut < endOut; winNameIn++) {
         if (ignores) {
            char *currIgnore = ignores;
            Bool ignore = FALSE;

            while (*currIgnore != '\0') {
               if (*winNameIn == *currIgnore) {
                  ignore = TRUE;
                  break;
               }
               currIgnore++;
            }

            if (!ignore) {
               *bufOut = (*winNameIn == pathSep) ? '\0' : *winNameIn;
               bufOut++;
            }
         } else {
            *bufOut = (*winNameIn == pathSep) ? '\0' : *winNameIn;
            bufOut++;
         }
      }

      /*
       * NUL terminate. XXX This should go away.
       *
       * When we get rid of NUL termination here, this test should
       * also change to "if (*winNameIn != '\0')".
       */

      if (bufOut == endOut) {
         result = -1;
         goto out;
      }
      *bufOut = '\0';

      /* Path name size should not require more than 4 bytes. */
      ASSERT((bufOut - origOut) <= 0xFFFFFFFF);

      /* If there were any trailing path separators, dont count them [krishnan] */
      result = (int)(bufOut - origOut);
      while ((result >= 1) && (origOut[result - 1] == 0)) {
         result--;
      }

      /*
       * Make exception and call CPName_Print() here, since it's only for
       * logging
       */

      LOG(4, ("%s: CPName is \"%s\"\n", __FUNCTION__, 
              CPName_Print(origOut, result)));
   }

out:
   free(fullName);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_CPNameListToDynBufArray --
 *
 *    Export CPName file list from binary buffer to DynBufArray.
 *
 * Results:
 *    TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *    Memory may allocated for DynBufArray if success.
 *
 *-----------------------------------------------------------------------------
 */

Bool
DnD_CPNameListToDynBufArray(char *fileList,           // IN: CPName format
                            size_t listSize,          // IN
                            DynBufArray *dynBufArray) // OUT
{
   DynBuf buf;
   BufRead r;
   int32 pathLen;
   size_t count;
   size_t i;

   ASSERT(fileList);
   r.pos = fileList;
   r.unreadLen = listSize;

   DynBufArray_Init(dynBufArray, 0);

   while (r.unreadLen > 0) {
      DynBuf_Init(&buf);
      if (!DnDReadBuffer(&r, &pathLen, sizeof pathLen) ||
          (pathLen > r.unreadLen) ||
          !DynBuf_Append(&buf, r.pos, pathLen)) {
         goto error;
      }

      if (!DnDSlideBuffer(&r, pathLen)) {
         goto error;
      }

      if (!DynBufArray_Push(dynBufArray, buf)) {
         goto error;
      }
   }
   return TRUE;

error:
   DynBuf_Destroy(&buf);

   count = DynBufArray_Count(dynBufArray);
   for (i = 0; i < count; i++) {
      DynBuf *b = DynArray_AddressOf(dynBufArray, i);
      DynBuf_Destroy(b);
   }
   DynBufArray_SetCount(dynBufArray, 0);
   DynBufArray_Destroy(dynBufArray);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_GetLastDirName --
 *
 *    Try to get last directory name from a full path name.
 *
 * Results:
 *    The allocated UTF8 string, or NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

char *
DnD_GetLastDirName(const char *str) // IN
{
   size_t end = strlen(str);
   size_t start;
   size_t res = 0;

   if (end != 0 && DIRSEPC == str[end - 1]) {
      end--;
   }

   if (end == 0) {
      return 0;
   }

   start = end;

   while (start && DIRSEPC != str[start - 1]) {
      start--;
   }

   /* There should be at lease 1 DIRSEPC before end. */
   if (start == 0) {
      return 0;
   }

   res = end - start;
   return Unicode_AllocWithLength(str + start, res, STRING_ENCODING_UTF8);
}

/*
 *-----------------------------------------------------------------------------
 *
 * DnD_SetCPClipboardAndTruncateText --
 *
 *    Truncate the text if the size exceeds the maximum size and then put it
 *    into clip.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    The destBuf should be released by the caller where the destBuf allocated.
 *
 *-----------------------------------------------------------------------------
 */

void
DnD_SetCPClipboardAndTruncateText(CPClipboard *clip, // IN/OUT
                                  char *destBuf,     // IN
                                  size_t len)        // IN
{
   size_t bytesLeft = clip->maxSize - CPClipboard_GetTotalSize(clip) - 1;

   if (bytesLeft < 2 || len == 1) {
      /*
       * Less than 2 bytes left ( 1 byte needed for ending NULL ) or
       * input buffer only contains ending NULL
       */
      return;
   }
   // Truncate if the length is greater than max allowed.
   if (len > bytesLeft) {
      size_t boundaryPoint =
         CodeSet_Utf8FindCodePointBoundary(destBuf, bytesLeft - 1);
      destBuf[boundaryPoint] = '\0';
      ASSERT(Unicode_IsBufferValid(destBuf, -1, STRING_ENCODING_UTF8));
      Log("%s: Truncating text from %" FMTSZ "d chars to %" FMTSZ "d chars.\n",
          __FUNCTION__, len - 1, boundaryPoint);
      len = boundaryPoint + 1;
   }

   CPClipboard_SetItem(clip, CPFORMAT_TEXT, destBuf, len);
   Log("%s: retrieved text (%" FMTSZ "d bytes) from clipboard.\n",
      __FUNCTION__, len);
}


/* Transport layer big buffer support functions. */

/*
 *-----------------------------------------------------------------------------
 *
 * DnD_TransportBufInit --
 *
 *    Initialize transport layer buffer with DnD message.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Buffer memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

void
DnD_TransportBufInit(DnDTransportBuffer *buf, // OUT
                     uint8 *msg,              // IN
                     size_t msgSize,          // IN
                     uint32 seqNum)           // IN
{
   ASSERT(buf);
   ASSERT(msgSize <= DNDMSG_MAX_ARGSZ);

   free(buf->buffer);
   buf->buffer = Util_SafeMalloc(msgSize);
   memcpy(buf->buffer, msg, msgSize);
   buf->seqNum = seqNum;
   buf->totalSize = msgSize;
   buf->offset = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_TransportBufReset --
 *
 *    Reset transport layer buffer.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
DnD_TransportBufReset(DnDTransportBuffer *buf) // IN/OUT
{
   ASSERT(buf);

   free(buf->buffer);
   buf->buffer = NULL;

   buf->seqNum = 0;
   buf->totalSize = 0;
   buf->offset = 0;
   buf->lastUpdateTime = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_TransportBufGetPacket --
 *
 *    Get a transport layer packet from transport layer buffer.
 *
 * Results:
 *    Transport layer packet size, or 0 if failed.
 *
 * Side effects:
 *    Memory may be allocated for packet.
 *
 *-----------------------------------------------------------------------------
 */

size_t
DnD_TransportBufGetPacket(DnDTransportBuffer *buf,           // IN/OUT
                          DnDTransportPacketHeader **packet) // OUT
{
   size_t payloadSize;

   ASSERT(buf);

   if (buf->totalSize < buf->offset) {
      return 0;
   }

   if ((buf->totalSize - buf->offset) > DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE) {
      payloadSize = DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE;
   } else {
      payloadSize = buf->totalSize - buf->offset;
   }

   *packet = Util_SafeMalloc(payloadSize + DND_TRANSPORT_PACKET_HEADER_SIZE);
   (*packet)->type = DND_TRANSPORT_PACKET_TYPE_PAYLOAD;
   (*packet)->seqNum = buf->seqNum;
   (*packet)->totalSize = buf->totalSize;
   (*packet)->payloadSize = payloadSize;
   (*packet)->offset = buf->offset;

   memcpy((*packet)->payload,
          buf->buffer + buf->offset,
          payloadSize);
   buf->offset += payloadSize;

   /* This time is used for timeout purpose. */
   buf->lastUpdateTime = Hostinfo_SystemTimerUS();

   return payloadSize + DND_TRANSPORT_PACKET_HEADER_SIZE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_TransportBufAppendPacket --
 *
 *    Put a received packet into transport layer buffer.
 *    This function should be called after validate the packet and packetSize!
 *    See: RpcV3Util::OnRecvPacket()
 *
 * Results:
 *    TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *    Memory may be allocated for transport layer buffer.
 *
 *-----------------------------------------------------------------------------
 */

Bool
DnD_TransportBufAppendPacket(DnDTransportBuffer *buf,          // IN/OUT
                             DnDTransportPacketHeader *packet, // IN
                             size_t packetSize)                // IN
{
   ASSERT(buf);

   /*
    * If seqNum does not match, it means either this is the first packet, or there
    * is a timeout in another side. Reset the buffer in all cases.
    */
   if (buf->seqNum != packet->seqNum) {
      DnD_TransportBufReset(buf);
   }

   if (!buf->buffer) {
      ASSERT(!packet->offset);
      if (packet->offset) {
         goto error;
      }
      buf->buffer = Util_SafeMalloc(packet->totalSize);
      buf->totalSize = packet->totalSize;
      buf->seqNum = packet->seqNum;
      buf->offset = 0;
   }

   if (buf->offset != packet->offset) {
      goto error;
   }

   memcpy(buf->buffer + buf->offset,
          packet->payload,
          packet->payloadSize);
   buf->offset += packet->payloadSize;
   return TRUE;

error:
   DnD_TransportBufReset(buf);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_TransportMsgToPacket --
 *
 *    Get a packet from small size message.
 *
 * Results:
 *    Transport layer packet size, or 0 if failed.
 *
 * Side effects:
 *    Memory may be allocated for packet.
 *
 *-----------------------------------------------------------------------------
 */

size_t
DnD_TransportMsgToPacket(uint8 *msg,                        // IN
                         size_t msgSize,                    // IN
                         uint32 seqNum,                     // IN
                         DnDTransportPacketHeader **packet) // OUT
{
   size_t packetSize;

   ASSERT(msgSize > 0 && msgSize <= DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE);
   ASSERT(msg);
   ASSERT(packet);

   if (msgSize <=0 ||
       msgSize > DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE ||
       !msg || !packet) {
      return 0;
   }

   packetSize = msgSize + DND_TRANSPORT_PACKET_HEADER_SIZE;

   *packet = Util_SafeMalloc(packetSize);

   (*packet)->type = DND_TRANSPORT_PACKET_TYPE_SINGLE;
   (*packet)->seqNum = seqNum;
   (*packet)->totalSize = msgSize;
   (*packet)->payloadSize = msgSize;
   (*packet)->offset = 0;

   memcpy((*packet)->payload, msg, msgSize);

   return packetSize;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_TransportReqPacket --
 *
 *    Generate a request packet with empty payload. After got a payload, receive
 *    side should send a DND_TRANSPORT_PACKET_TYPE_REQUEST packet to ask for
 *    next payload packet.
 *
 * Results:
 *    Transport layer packet size.
 *
 * Side effects:
 *    Memory is allocated for packet.
 *
 *-----------------------------------------------------------------------------
 */

size_t
DnD_TransportReqPacket(DnDTransportBuffer *buf,           // IN
                       DnDTransportPacketHeader **packet) // OUT
{
   *packet = Util_SafeMalloc(DND_TRANSPORT_PACKET_HEADER_SIZE);

   (*packet)->type = DND_TRANSPORT_PACKET_TYPE_REQUEST;
   (*packet)->seqNum = buf->seqNum;
   (*packet)->totalSize = buf->totalSize;
   (*packet)->payloadSize = 0;
   (*packet)->offset = buf->offset;
   return DND_TRANSPORT_PACKET_HEADER_SIZE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDReadBuffer --
 *
 *      Copies len bytes of data from b to out. Subsequent calls to this
 *      function will copy data from the last unread point.
 *
 * Results:
 *      TRUE when data is successfully copies to out, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDReadBuffer(BufRead *b,       // IN/OUT: buffer to read from
              void *out,        // OUT: the output buffer
              size_t len)       // IN: the amount to read
{
   ASSERT(b);
   ASSERT(out);

   if (len > b->unreadLen) {
      return FALSE;
   }

   memcpy(out, b->pos, len);
   if (!DnDSlideBuffer(b, len)) {
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDSlideBuffer --
 *
 *      Ignore len bytes of data in b. Subsequent calls to DnDReadBuffer will
 *      copy data from the last point.
 *
 * Results:
 *      TRUE when pos is successfully changed, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDSlideBuffer(BufRead *b, // IN/OUT: buffer to read from
               size_t len) // IN: the amount to read
{
   ASSERT(b);

   if (len > b->unreadLen) {
      return FALSE;
   }

   b->pos += len;
   b->unreadLen -= len;

   return TRUE;
}

