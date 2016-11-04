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
 * cpNameUtilLinux.c
 *
 *    Linux implementation of CPName utility functions.  These are not included
 *    in the main CPName API since they may require other calls on the
 *    resulting CPName to be from the same OS-type.
 */

/* Some of the headers in cpNameUtil.c cannot be included in driver code */
#if !defined __KERNEL__ && !defined _KERNEL && !defined KERNEL

#include "cpNameUtil.h"
#include "cpNameUtilInt.h"

#if defined __APPLE__
#include "codeset.h"
#endif /* defined __APPLE__ */
#include "util.h"

/*
 *----------------------------------------------------------------------------
 *
 * CPNameUtil_ConvertToRoot --
 *
 *    Pass through function that calls Linux version of _ConvertToRoot().
 *
 *    Performs CPName conversion and such that the result can be converted back
 *    to an absolute path (in the "root" share) by a Linux hgfs server.
 *
 *    Note that nameIn must contain an absolute path.
 *
 * Results:
 *    Size of the output buffer on success, negative value on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPNameUtil_ConvertToRoot(char const *nameIn, // IN:  buf to convert
                         size_t bufOutSize,  // IN:  size of the output buffer
                         char *bufOut)       // OUT: output buffer
{
   return CPNameUtil_LinuxConvertToRoot(nameIn, bufOutSize, bufOut);
}


/*
 *----------------------------------------------------------------------------
 *
 * CPNameUtilConvertUtf8FormCAndD --
 *
 *    Helper conversion routine to convert between a CP format name 
 *    in unicode form C (precomposed) format which is used by the HGFS
 *    protocol requests and the unicode form D (decomposed) format,
 *    which is used on Mac OS host (everyone else uses form C).
 *
 * Results:
 *    TRUE if success result string is converted, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
CPNameUtilConvertUtf8FormCAndD(const char *cpNameToConvert,   // IN:
                               size_t cpNameToConvertLen,     // IN: includes nul 
                               Bool convertToFormC,           // IN:
                               char **cpConvertedName,        // OUT:
                               size_t *cpConvertedNameLen)    // OUT: includes nul
{
   Bool status = TRUE;
#if defined __APPLE__
   const char *begin;
   const char *end;
   const char *next;
   char *newData = NULL;
   char *convertedName = NULL;
   size_t convertedNameLen;
   uint32 newDataLen = 0;
   int len;

   ASSERT(cpNameToConvert);
   ASSERT(cpConvertedName);
   ASSERT(cpConvertedNameLen);

   /* 
    * Get first component. We bypass the higher level CPName_GetComponent
    * function so we'll have more control over the illegal characters.
    */
   begin = cpNameToConvert;
   end = cpNameToConvert + cpNameToConvertLen - 1;
   /* Get the length of this component, and a pointer to the next. */
   while ((len = CPName_GetComponent(begin, end, &next)) != 0) {
      uint32 origNewDataLen = newDataLen;

      if (len < 0) {
         status = FALSE;
         goto exit;
      }

      if (convertToFormC) {
         status = CodeSet_Utf8FormDToUtf8FormC(begin, 
                                               len, 
                                               &convertedName, 
                                               &convertedNameLen);
      } else {
         status = CodeSet_Utf8FormCToUtf8FormD(begin, 
                                               len, 
                                               &convertedName, 
                                               &convertedNameLen);
      }

      if (!status) {
         goto exit;
      }

      /*
       * Append this component to our list: allocate one more for NUL.
       */
      newDataLen += convertedNameLen + 1;
      newData = (char *)Util_SafeRealloc(newData, newDataLen);

      memcpy(newData + origNewDataLen, convertedName, convertedNameLen);
      newData[newDataLen - 1] = '\0';

      free(convertedName);
      convertedName = NULL;
      begin = next;
   }

   *cpConvertedName = newData;
   /* Including nul terminator */
   *cpConvertedNameLen = newDataLen;
exit:
   if (!status) {
      if (newData != NULL) {
         free(newData);
      }
   }
#else /* defined __APPLE__ */
   /* No conversion required return a copy of what is received. */
   *cpConvertedName = Util_SafeCalloc(1, cpNameToConvertLen);
   memcpy(*cpConvertedName, cpNameToConvert, cpNameToConvertLen);
   *cpConvertedNameLen = cpNameToConvertLen;
#endif /* defined __APPLE__ */
   return status;
}

#endif /* __KERNEL__ */
