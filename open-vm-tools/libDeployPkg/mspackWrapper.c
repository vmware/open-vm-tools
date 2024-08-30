/*********************************************************
 * Copyright (C) 2006-2020 VMware, Inc. All rights reserved.
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
 * mspackWrapper.c --
 *
 *      Implementation of the mspack wrapper.
 */

#include "mspackWrapper.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mspack.h>
#include <stdarg.h>
#include <errno.h>
#include "str.h"

/*
 * Template functions
 */

static void DefaultLog(int logLevel, const char* fmtstr, ...);

/*
 * String explanation for the error codes.
 * They are arranged in the same order as the corresponding error codes.
 */

static const char*  LINUXCAB_STRERR[] = {
                                        "Success.",
                                        "Unknown Error.",
                                        "Error extractinf file from cabinet.",
                                        "Error creating decompressor.",
                                        "Error opening cabinet file.",
                                        "Error seeking. Check config.h & \
                                         library compilation architecture.",
                                        "Error tyring to read the cabinet header."
                                        };

/*
 * Statics
 */

static LogFunction sLog = DefaultLog;

// .....................................................................................

/**
 *
 * Default logging mechanism to be used. Print to screen.
 *
 * @param   [in]  level    Log level
 * @param   [in]  fmtstr   Format to print the variables in
 * @param   [in]  ...      Variables to be printed
 *
 **/
static void
DefaultLog(int logLevel, const char* fmtstr, ...)
{
   va_list args;
   va_start(args, fmtstr);
   printf(fmtstr, args);
   va_end(args);
}


// .....................................................................................

/**
 *
 * Set the logging function.
 *
 * @param   [in]  log   Logging function to be used.
 * @returns None
 *
 **/
void
MspackWrapper_SetLogger(LogFunction log)
{
   sLog = log;
}

//......................................................................................

/**
 *
 * Sets up the path for extracting file. For e.g. if the file is /a/b/c/d.abc
 * then it creates /a/b/c (skips if any of the directories along the path
 * exists)
 *
 * @param path  IN: Complete path of the file
 * @return
 *  On Success  LINUXCAB_SUCCESS
 *  On Error    LINUXCAB_ERROR
 *
 **/
unsigned int
SetupPath (char* path) {
   char* token;

   // walk through the path (it employs in string replacement)
   for (token = path; *token; ++token) {
      //MS-DOC to unix path conversion
      if (*token == '\\') {
         *token = '/';
      }

      // ignore first / or subsequent characters that are not /
      if (token  == path) continue;
      if (*token != '/') continue;

      /*
       * cut it off here for e.g. on first iteration /a/b/c/d.abc will have
       * token /a, on second /a/b etc
       */
      *token = 0;

#ifdef VMX86_DEBUG
      sLog(log_debug, "Creating directory %s ", path);
#endif

      if (mkdir(path, 0777) == -1) {
         struct stat stats;
         // ignore if the directory exists
         if (!((stat(path, &stats) == 0) && S_ISDIR(stats.st_mode))) {
            sLog(log_error, "Unable to create directory %s (%s)", path,
                 strerror(errno));
            return LINUXCAB_ERROR;
         }
      }

      // restore the token
      *token = '/';
    }

    return LINUXCAB_SUCCESS;
}

//......................................................................................

/**
 *
 * Extract one given file.
 *
 * @param deflator      IN: Pointer to the cabinet decompressor
 * @param file          IN: Pointer to file under decompression
 * @param destDirector  IN: Destination directory
 * @return
 *  On Success    LINUXCAB_SUCCESS
 *  On Failure    LINUXCAB_ERROR
 *
 **/
static unsigned int
ExtractFile (struct mscab_decompressor* deflator,
             struct mscabd_file* file,
             const char* destDirectory)
{
   size_t sz;

   // copy it into a string as SetupPath will do an in place text manipulation
   char fileName[strlen(file->filename)+1];
   strcpy(fileName, file->filename);

   // add the target directory to the file path
   sz = strlen(destDirectory)+ 1 + strlen(fileName)+ 1;
   {
      char outCabFile[sz];
      Str_Sprintf (outCabFile, sz,  "%s/%s", destDirectory, fileName);

      // set up the path if it does not exist
      if (SetupPath(outCabFile) != LINUXCAB_SUCCESS) {
         return LINUXCAB_ERROR;
      }

      #ifdef VMX86_DEBUG
       sLog(log_info, "Extracting %s .... ", outCabFile );
      #endif

      // Extract File
      if (deflator->extract(deflator,file,outCabFile) != MSPACK_ERR_OK) {
         return LINUXCAB_ERR_EXTRACT;
      }
   }

   return LINUXCAB_SUCCESS;
}

//.............................................................................

/**
 *
 * Helper function for ExpandAllFilesInCab below.
 * Expands all files in the cabinet into the specified directory. Also returns
 * the command that is specified in the VMware defined header.
 *
 * @param cabFileName      IN:   Cabinet file name
 * @param destDirectory    IN:   Destination directory to uncab
 *
 * @return
 *  On success          LINUXCAB_SUCCESS
 *  On Error            LINUXCAB_ERROR, LINUXCAB_ERR_OPEN, LINUXCAB_ERR_DECOMPRESS,
 *                      LINUXCAB_EXTRACT, LINUXCAB_ERR_HEADER
 **/
static unsigned int
ExpandAllFilesInCabInt (const char* cabFileName,
                        const char* destDirectory)
{
   // set the state as success
   int returnState = LINUXCAB_SUCCESS;
   struct mscabd_cabinet* cab;
   struct mscabd_cabinet* cabToClose;

   // Create decompressor instance
   // Note : I am using NULL for reading file operations meaning requesting it
   // to use the default routines. It may be a good idea to write that up in the
   // future
   struct mscab_decompressor* deflator = mspack_create_cab_decompressor (NULL);

   // deflator error ?
   if (!deflator) {
      return LINUXCAB_ERR_DECOMPRESSOR;
   }

   // Search for the specified file
   cab = deflator->search (deflator, (char*)cabFileName);
   cabToClose = cab;

   // was the file found ?
   if (!cab) {
      return LINUXCAB_ERR_OPEN;
   }

   /*
    * Extract file by file
    * NOTE: open call cannot be used as cab can span multiple files. Hence
    * search call has to be used.
    */

   // Iterate through the cabinets.
   while(cab) {
      // Get all files in the cab
      struct mscabd_file* file = cab->files;

      // iterate through the files
      while(file) {
         returnState = ExtractFile(deflator, file, destDirectory);

         // error extracting ?
         if (returnState != LINUXCAB_SUCCESS) {
            break;
         }

         file = file->next;
      }

      // Break - if error
      if (returnState != LINUXCAB_SUCCESS) {
         break;
      }

#ifdef VMX86_DEBUG
      sLog(log_debug, "flag = %i ", cab->flags);
#endif

      // move to next cab file - in case of multi file cabs
      cab = cab->next;
   }

   deflator->close (deflator, cabToClose);

   // close & destroy instance - clean up
   mspack_destroy_cab_decompressor(deflator);

#ifdef VMX86_DEBUG
   sLog(log_info, "Done extracting files. ");
#endif

   return returnState;
}

//.............................................................................

/**
 *
 * Expands all files in the cabinet into the specified directory. Also returns
 * the command that is specified in the VMware defined header.
 * The mspack library uses and honors the current umask when setting the
 * file permission of the extracted files. Here we set the umask in a way
 * that protects them against world access.
 *
 * @param cabFileName      IN:   Cabinet file name
 * @param destDirectory    IN:   Destination directory to uncab
 *
 * @return
 *  On success          LINUXCAB_SUCCESS
 *  On Error            LINUXCAB_ERROR, LINUXCAB_ERR_OPEN,
 *                      LINUXCAB_ERR_DECOMPRESS,
 *                      LINUXCAB_EXTRACT, LINUXCAB_ERR_HEADER
 **/
unsigned int
ExpandAllFilesInCab (const char* cabFileName,
                     const char* destDirectory)
{
   unsigned int rc;
   mode_t oldMask;

   /* Turn off the world access permssion for all expanded files */
   oldMask = umask(0027);

   rc = ExpandAllFilesInCabInt(cabFileName, destDirectory);

   umask(oldMask);

   return rc;
}

//.............................................................................

/**
 *
 * Does a self check on the library parameters to make sure that the library
 * compilation is compatible with the client compilation. This is funny scenario
 * and is put in to support different flavours of UNIX operating systems.
 * Essentially the library checks of off_t size.
 *
 * @param         None
 * @return
 *  On Success    LINUXCAB_SUCCESS
 *  On Error      LINUXCAB_ERR_SEEK, LINUXCAB_ERROR
 *
 **/
unsigned int
SelfTestMspack(void)
{
   int error;
   MSPACK_SYS_SELFTEST(error);

   // test failed ?
   if (error) {
      if (error == MSPACK_ERR_SEEK) {
         /* Library has been compiled for a different bit version of
          * than the program that uses it. The other explanation
          * is inappropriate .h inclusion, can be solved using configure.h.
          * This is the most common issue, hence addressed specifically
          */
         return LINUXCAB_ERR_SEEK;
      } else {
         // Not a common error.
         return LINUXCAB_ERROR;
      }
   }

   // Perfectly compatible
   return LINUXCAB_SUCCESS;
}

//...........................................................................

/**
 *
 * Get a string error message for the given error code.
 *
 * @param   error  IN:  Error  number
 * @return  error as a string message
 *
 **/
const char*
GetLinuxCabErrorMsg ( const unsigned int error )
{
   return LINUXCAB_STRERR[error];
}


