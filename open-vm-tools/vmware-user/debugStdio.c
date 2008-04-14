/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * debug.c --
 *
 *    Platform specific debug routines
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#if defined(_WIN32) && defined(_MSC_VER)
#   include <windows.h>
#endif
#if defined(N_PLAT_NLM)
#   include "vmwtool.h"
#endif


#include "vmware.h"
#include "debug.h"
#include "util.h"
#include "str.h"
#include "fileIO.h"
#include "file.h"
#include "system.h"
#include "unicode.h"

static char debugFile[FILE_MAXPATH] = {0};
static Bool debugEnabled = FALSE;
static const char *debugPrefix = NULL;


/*
 *-----------------------------------------------------------------------------
 *
 * Debug_Set --
 *
 *    Enable/Disable debugging output
 *
 * Result
 *    None.
 *
 * Side-effects
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
Debug_Set(Bool enable,        // IN
          const char *prefix) // IN
{
   debugEnabled = enable;
   debugPrefix = prefix;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Debug_EnableToFile --
 *
 *    Enable debugging output to the given file. If backup is TRUE, will rename
 *    existing file to file.old and start logging to a new file. Only daemon
 *    should set backup flag then will do backup for each reboot.
 *
 * Result
 *    None.
 *
 * Side-effects
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
Debug_EnableToFile(const char *file,      // IN
                   Bool backup)           // IN
{
   if (backup && file && File_Exists(file)) {
      /* Back up existing log file. */
      char *bakFile = Str_Asprintf(NULL, "%s.old", file);
      if (bakFile &&
          !File_IsDirectory(bakFile) &&
          0 == File_UnlinkIfExists(bakFile)) {  // remove old back up file.
         File_Rename(file, bakFile);
      }
      free(bakFile);
   }
   if (file) {
      Str_Sprintf(debugFile, sizeof debugFile, "%s", file);
      debugEnabled = TRUE;
   } else {
      debugFile[0] = '\0';
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Debug_IsEnabled --
 *
 *    Is debugging output enabled?
 *
 * Result
 *    TRUE/FALSE
 *
 * Side-effects
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Debug_IsEnabled(void)
{
   return debugEnabled;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DebugToFile --
 *
 *    Print a string to the given file. This opens & closes the file
 *    handle each time it is called so it will significantly
 *    slow down the calling program. This is done so that the file
 *    can be opened & read while the program is running.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    DebugToFile is turned off if there was an error opening the file.
 *
 *-----------------------------------------------------------------------------
 */

static
void DebugToFile(const char *str) // IN
{
#ifndef _CONSOLE
   FileIOResult fr;
   FileIODescriptor *fd;
   size_t bytesWritten;
   Unicode timePrefix;
   const char *timePrefixUtf8;
   
   ASSERT(debugFile[0] != 0);
   
   fd = (FileIODescriptor *) malloc(sizeof(FileIODescriptor));
   ASSERT_NOT_IMPLEMENTED(fd);
   FileIO_Invalidate(fd);
   
   fr = FileIO_Open(fd, debugFile, FILEIO_OPEN_ACCESS_WRITE,
                    FILEIO_OPEN_CREATE);
   if (fr != FILEIO_SUCCESS) {
      Warning("---Error opening file '%s'.\n", debugFile);
      debugFile[0] = '\0';
      goto done;
   }

   /*
    * XXX: Writing the date/time prefix in UTF-8 and the rest of the string in
    * an unspecified encoding is rather broken, but it'll have to do until the
    * rest of the Tools are made internationalization-safe.
    */
   timePrefix = System_GetTimeAsString();
   if (timePrefix == NULL) {
      Warning("---Error getting formatted time string.\n");
      goto close;
   }
   timePrefixUtf8 = UTF8(timePrefix);
   ASSERT(timePrefixUtf8);

   FileIO_Seek(fd, 0, FILEIO_SEEK_END);
   fr = FileIO_Write(fd, timePrefixUtf8, strlen(timePrefixUtf8), &bytesWritten);
   fr = FileIO_Write(fd, str, strlen(str), &bytesWritten);
   Unicode_Free(timePrefix);
   if (fr != FILEIO_SUCCESS) {
      Warning("---Error writing to file '%s'.\n", debugFile);
   }

 close:
   FileIO_Close(fd);
   
 done:
   free(fd);
   return;
#endif // _CONSOLE
}


/*
 *-----------------------------------------------------------------------------
 *
 * Debug --
 *
 *    If debugging is enabled, output debug information
 *
 * Result
 *    None.
 *
 * Side-effects
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
Debug(char const *fmt, // IN: Format string
      ...)             // IN: Arguments
{
   va_list args;
   char *str;
   char *msg;

   if (debugEnabled == FALSE) {
      return;
   }

   va_start(args, fmt);
   msg = Str_Vasprintf(NULL, fmt, args);
   va_end(args);

   str = Str_Asprintf(NULL, "[%s]: %s", debugPrefix ? debugPrefix : "NULL", msg);
   free(msg);

#ifdef N_PLAT_NLM
   OutputToScreenWithAttribute(VMwareScreen, BOLD_RED, "%s", str);
#else
#ifdef _WIN32   
   OutputDebugString(str);
#endif
#if !defined(_WIN32) || defined(_CONSOLE)
   fprintf(stderr, str);
#endif
#endif
   if (debugFile[0] != '\0') {
      DebugToFile(str);
   }

   free(str);
}
