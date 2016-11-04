/*********************************************************
 * Copyright (C) 2005-2016 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * cpNameUtil.c
 *
 *    Common implementations of CPName utility functions.
 */


/* Some of the headers below cannot be included in driver code */
#if !defined __KERNEL__ && !defined _KERNEL && !defined KERNEL

#include "cpNameUtil.h"
#include "hgfsServerPolicy.h"
#include "hgfsVirtualDir.h"
#include "util.h"
#include "vm_assert.h"
#include "str.h"
#include "cpNameUtilInt.h"

#define WIN_DIRSEPC     '\\'
#define WIN_DIRSEPS     "\\"


/*
 *----------------------------------------------------------------------------
 *
 * CPNameUtil_Strrchr --
 *
 *    Performs strrchr(3) on a CPName path.
 *
 * Results:
 *    Pointer to last occurrence of searchChar in cpNameIn if found, NULL if
 *    not found.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

char *
CPNameUtil_Strrchr(char const *cpNameIn,       // IN: CPName path to search
                   size_t cpNameInSize,        // IN: Size of CPName path
                   char searchChar)            // IN: Character to search for
{
   ssize_t index;

   ASSERT(cpNameIn);
   ASSERT(cpNameInSize > 0);

   for (index = cpNameInSize - 1;
        cpNameIn[index] != searchChar && index >= 0;
        index--);

   return (index < 0) ? NULL : (char *)(cpNameIn + index);
}


/*
 *----------------------------------------------------------------------------
 *
 * CPNameUtil_LinuxConvertToRoot --
 *
 *    Performs CPName conversion and such that the result can be converted back
 *    to an absolute path (in the "root" share) by a Linux hgfs server.
 *
 *    Note that nameIn must contain an absolute path.
 *
 * Results:
 *    Size of the output buffer on success, negative value on error
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPNameUtil_LinuxConvertToRoot(char const *nameIn, // IN:  buf to convert
                              size_t bufOutSize,  // IN:  size of the output buffer
                              char *bufOut)       // OUT: output buffer
{
   const size_t shareNameSize = HGFS_STR_LEN(HGFS_SERVER_POLICY_ROOT_SHARE_NAME);

   int result;

   ASSERT(nameIn);
   ASSERT(bufOut);

   if (bufOutSize <= shareNameSize) {
      return -1;
   }

   /* Prepend the name of the "root" share directly in the output buffer */
   memcpy(bufOut, HGFS_SERVER_POLICY_ROOT_SHARE_NAME, shareNameSize);
   bufOut[shareNameSize] = '\0';

   result = CPName_LinuxConvertTo(nameIn, bufOutSize - shareNameSize - 1,
                                  bufOut + shareNameSize + 1);

   /* Return either the same error code or the correct size */
   return (result < 0) ? result : (int)(result + shareNameSize + 1);
}


/*
 *----------------------------------------------------------------------------
 *
 * CPNameUtil_WindowsConvertToRoot --
 *
 *    Performs CPName conversion and appends necessary strings ("root" and
 *    "drive"|"unc") so that the result can be converted back to an absolute
 *    path (in the "root" share) by a Windows hgfs server.
 *
 *    Note that nameIn must contain an absolute path.
 *
 * Results:
 *    Size of the output buffer on success, negative value on error
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPNameUtil_WindowsConvertToRoot(char const *nameIn, // IN:  buf to convert
                                size_t bufOutSize,  // IN:  size of the output buffer
                                char *bufOut)       // OUT: output buffer
{
   const char partialName[] = HGFS_SERVER_POLICY_ROOT_SHARE_NAME;
   const size_t partialNameLen = HGFS_STR_LEN(HGFS_SERVER_POLICY_ROOT_SHARE_NAME);
   const char *partialNameSuffix = "";
   size_t partialNameSuffixLen;
   char *fullName;
   size_t fullNameLen;
   size_t nameLen;
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

   nameLen = strlen(nameIn);
   fullNameLen = partialNameLen + partialNameSuffixLen + nameLen;
   fullName = (char *)Util_SafeMalloc(fullNameLen + 1);

   memcpy(fullName, partialName, partialNameLen);

   memcpy(fullName + partialNameLen, partialNameSuffix, partialNameSuffixLen);
   if (nameIn[1] == ':') {
      /*
       * If the name is in format "<drive letter>:" strip out ':' from it
       * because the rest of the code assumes that driver letter in a 
       * platform independent name is represented by a single character without colon.
       */
      fullName[partialNameLen + partialNameSuffixLen] = nameIn[0];
      memcpy(fullName + partialNameLen + partialNameSuffixLen + 1, nameIn + 2, nameLen - 2);
      fullNameLen--;
   } else {
      memcpy(fullName + partialNameLen + partialNameSuffixLen, nameIn, nameLen);
   }
   fullName[fullNameLen] = '\0';

   /* CPName_ConvertTo strips out the ':' character */
   result = CPName_WindowsConvertTo(fullName, bufOutSize, bufOut);
   free(fullName);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPNameUtil_Utf8FormHostToUtf8FormC --
 *
 *    Convert CPname to form C (precomposed) which is used by the HGFS
 *    protocol from host preferred format. On Mac hosts the current format
 *    is unicode form D, so conversion is required, others the current
 *    format is the same.
 *
 *    Input/output name lengths include the nul-terminator so that the 
 *    conversion routine will include the final character when breaking
 *    up the CPName into it's individual components.
 *
 *
 * Results:
 *    TRUE if success result string is in form C format, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPNameUtil_Utf8FormHostToUtf8FormC(const char *cpNameToConvert,   // IN:
                                   size_t cpNameToConvertLen,     // IN: includes nul
                                   char **cpUtf8FormCName,        // OUT:
                                   size_t *cpUtf8FormCNameLen)    // OUT: includes nul
{
   return CPNameUtilConvertUtf8FormCAndD(cpNameToConvert,
                                         cpNameToConvertLen,
                                         TRUE,
                                         cpUtf8FormCName,
                                         cpUtf8FormCNameLen);
}


/*
 *----------------------------------------------------------------------------
 *
 * CPNameUtil_Utf8FormCToUtf8FormHost --
 *
 *    Convert from CP unicode form C (decomposed) name  used by the HGFS
 *    protocol to the host preferred format. On Mac OS is unicode form D 
 *    (precomposed), everyone else this stays as form C (precomposed).
 *
 *    Input/output name lengths include the nul-terminator so that the 
 *    conversion routine will include the final character when breaking
 *    up the CPName into it's individual components.
 *
 * Results:
 *    TRUE if success result string is in CP format, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPNameUtil_Utf8FormCToUtf8FormHost(const char *cpUtf8FormCName,   // IN:
                                   size_t cpUtf8FormCNameLen,     // IN: includes nul 
                                   char **cpConvertedName,        // OUT:
                                   size_t *cpConvertedNameLen)    // OUT: includes nul
{
   return CPNameUtilConvertUtf8FormCAndD(cpUtf8FormCName,
                                         cpUtf8FormCNameLen,
                                         FALSE,
                                         cpConvertedName,
                                         cpConvertedNameLen);
}


/*
 *----------------------------------------------------------------------------
 *
 * CPNameUitl_CharReplace --
 *
 *    A simple function to replace all oldChar's with newChar's in a binary
 *    buffer. This is used for either replacing NULL with local DIRSPEC to
 *    convert from relative cross-platform name to local relative name, or
 *    replacing local DIRSEPC with NULL to convert from local relative name
 *    to relative cross-platform file name 
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
CPNameUtil_CharReplace(char *buf,      // IN/OUT
                       size_t bufSize, // IN
                       char oldChar,   // IN
                       char newChar)   // IN

{
   size_t i;

   ASSERT(buf);

   for (i = 0; i < bufSize; i++) {
      if (buf[i] == oldChar) {
         buf[i] = newChar;
      }
   }
}


#endif /* __KERNEL__ */
