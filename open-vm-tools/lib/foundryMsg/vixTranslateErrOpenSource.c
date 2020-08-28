/*********************************************************
 * Copyright (C) 2003-2017,2019 VMware, Inc. All rights reserved.
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
 * vixTranslateErrorOpenSource.c --
 *
 * Routines which translate between various other error code systems
 * into foundry errors.
 *
 * This is the minimal functions needed to build the tools for open source.
 * Most of the error translation functions are in foundryTranslateError.c,
 * which is NOT being released as open source. We do not want to include
 * any unnecessary error functions, since those use lots of different
 * error code definitions, and that would drag in a lot of headers from
 * bora/lib/public.
 */

#include "vmware.h"
#include "vixOpenSource.h"
#include <errno.h>
#include <string.h>

#ifdef _WIN32
#include "windowsu.h"
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Vix_TranslateGuestRegistryError --
 *
 *     Translate a guest windows registry error to a Foundry error.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
Vix_TranslateGuestRegistryError(int systemError) // IN
{
   VixError err = VIX_E_FAIL;
#ifdef _WIN32
   char *msg;

   switch (systemError) {
   case ERROR_INVALID_PARAMETER:
   case ERROR_FILE_NOT_FOUND:
      err = VIX_E_REG_KEY_INVALID;
      break;
   case ERROR_ACCESS_DENIED:
      err = VIX_E_GUEST_USER_PERMISSIONS;
      break;
   case ERROR_CHILD_MUST_BE_VOLATILE:
      err = VIX_E_REG_KEY_PARENT_VOLATILE;
      break;
   default:
      return Vix_TranslateSystemError(systemError);
   }
   msg = Win32U_FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                              NULL, systemError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              NULL);

   Log("Foundry operation failed with guest windows registry error: %s (%d), translated to %"FMT64"d\n",
       msg, systemError, err);
   free(msg);

#endif
   return err;
} // Vix_TranslateGuestRegistryError


/*
 *-----------------------------------------------------------------------------
 *
 * Vix_TranslateSystemError --
 *
 *     Translate a System error to a Foundry error.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

VixError
Vix_TranslateSystemError(int systemError) // IN
{
   VixError err;
#ifdef _WIN32
   char *msg;

   switch (systemError) {
   case ERROR_ACCESS_DENIED:
      err = VIX_E_FILE_ACCESS_ERROR;
      break;
   case ERROR_INVALID_NAME:
      err = VIX_E_FILE_NAME_INVALID;
      break;
   case ERROR_FILENAME_EXCED_RANGE:
      err = VIX_E_FILE_NAME_TOO_LONG;
      break;
   case ERROR_FILE_NOT_FOUND:
   case ERROR_PATH_NOT_FOUND:
   case ERROR_BAD_PATHNAME:
   case ERROR_DIRECTORY:
   case ERROR_BUFFER_OVERFLOW:
      err = VIX_E_FILE_NOT_FOUND;
      break;
   case ERROR_DIR_NOT_EMPTY:
      err = VIX_E_DIRECTORY_NOT_EMPTY;
      break;
   case ERROR_TOO_MANY_OPEN_FILES:
   case ERROR_NO_MORE_FILES:
   case ERROR_WRITE_PROTECT:
   case ERROR_WRITE_FAULT:
   case ERROR_READ_FAULT:
   case ERROR_SHARING_VIOLATION:
   case ERROR_SEEK:
   case ERROR_CANNOT_MAKE:
      Log("%s: system error = %d\n", __FUNCTION__,
          systemError);
      err = VIX_E_FILE_ERROR;
      break;
   case ERROR_HANDLE_DISK_FULL:
   case ERROR_DISK_FULL:
      err = VIX_E_DISK_FULL;
      break;
   case ERROR_FILE_EXISTS:
   case ERROR_ALREADY_EXISTS:
      err = VIX_E_FILE_ALREADY_EXISTS;
      break;
   case ERROR_BUSY:
   case ERROR_PATH_BUSY:
      err = VIX_E_OBJECT_IS_BUSY;
      break;
   case ERROR_INVALID_PARAMETER:
      err = VIX_E_INVALID_ARG;
      break;
   case ERROR_NOT_SUPPORTED:
      err = VIX_E_NOT_SUPPORTED;
      break;
   case ERROR_NO_DATA:
   case ERROR_INVALID_DATA:
      err = VIX_E_NOT_FOUND;
      break;
   case ERROR_NOT_ENOUGH_MEMORY:
      err = VIX_E_OUT_OF_MEMORY;
      break;
   default:
      err = VIX_E_FAIL;
   }
   msg = Win32U_FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                              NULL, systemError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              NULL);

   Log("Foundry operation failed with system error: %s (%d), translated to %"FMT64"d\n",
       msg, systemError, err);
   free(msg);

#else // linux, other *nix
   err = Vix_TranslateErrno(systemError);
#endif

   return err;
} // Vix_TranslateSystemError


/*
 *-----------------------------------------------------------------------------
 *
 * Vix_TranslateCOMError --
 *
 *     Translate a COM (Windows) error to a Foundry error.
 *
 * Results:
 *     VixError.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef _WIN32
VixError
Vix_TranslateCOMError(HRESULT hrError) // IN
{
   VixError err = VIX_E_FAIL;

   switch (hrError) {
   case E_ACCESSDENIED:
      err = VIX_E_FILE_ACCESS_ERROR;
      break;

   case STG_E_PATHNOTFOUND:
   case STG_E_FILENOTFOUND:
      err = VIX_E_FILE_NOT_FOUND;
      break;

   case STG_E_MEDIUMFULL:
      err = VIX_E_DISK_FULL;
      break;

   case STG_E_FILEALREADYEXISTS:
      err = VIX_E_FILE_ALREADY_EXISTS;
      break;

   case E_INVALIDARG:
   case E_POINTER:
      err = VIX_E_INVALID_ARG;
      break;

   case E_NOTIMPL:
   case E_NOINTERFACE:
      err = VIX_E_NOT_SUPPORTED;
      break;

   case E_OUTOFMEMORY:
      err = VIX_E_OUT_OF_MEMORY;
      break;

   case E_FAIL:
   default:
      err = VIX_E_FAIL;
   }

   return err;
} // Vix_TranslateCOMError
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Vix_TranslateCryptoError --
 *
 *     Translate a Crypto error to a Foundry error.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

VixError
Vix_TranslateCryptoError(CryptoError cryptoError) // IN
{
   if (CRYPTO_ERROR_SUCCESS == cryptoError) {
      return VIX_OK;
   } else if (CRYPTO_ERROR_OPERATION_FAILED == cryptoError) {
      return VIX_E_GUEST_USER_PERMISSIONS;
   } else if (CRYPTO_ERROR_UNKNOWN_ALGORITHM == cryptoError) {
      return VIX_E_CRYPTO_UNKNOWN_ALGORITHM;
   } else if (CRYPTO_ERROR_BAD_BUFFER_SIZE == cryptoError) {
      return VIX_E_CRYPTO_BAD_BUFFER_SIZE;
   } else if (CRYPTO_ERROR_INVALID_OPERATION == cryptoError) {
      return VIX_E_CRYPTO_INVALID_OPERATION;
   } else if (CRYPTO_ERROR_NOMEM == cryptoError) {
      return VIX_E_OUT_OF_MEMORY;
   } else if (CRYPTO_ERROR_NEED_PASSWORD == cryptoError) {
      return VIX_E_CRYPTO_NEED_PASSWORD;
   } else if (CRYPTO_ERROR_BAD_PASSWORD == cryptoError) {
      return VIX_E_CRYPTO_BAD_PASSWORD;
   } else if (CRYPTO_ERROR_IO_ERROR == cryptoError) {
      Log("%s: crypto error = %d\n", __FUNCTION__,
          (int)cryptoError);
      return VIX_E_FILE_ERROR;
   } else if (CRYPTO_ERROR_UNKNOWN_ERROR == cryptoError) {
      return VIX_E_FAIL;
   } else if (CRYPTO_ERROR_NAME_NOT_FOUND == cryptoError) {
      return VIX_E_CRYPTO_NOT_IN_DICTIONARY;
   } else if (CRYPTO_ERROR_NO_CRYPTO == cryptoError) {
      return VIX_E_CRYPTO_NO_CRYPTO;
   }

   return VIX_E_FAIL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Vix_TranslateErrno --
 *
 *     Translate a Posix errno to a Foundry error.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixError
Vix_TranslateErrno(int systemError)             // IN
{
   VixError err = VIX_E_FAIL;

   /*
    * Be careful while adding new error code translations. This function is
    * complied for both Windows and POSIX guests. Few errors i.e. ETIMEDOUT,
    * ENOBUFS are defined only for POSIX guests. When a new error code
    * translation is added, make sure you build a sandbox job and it is
    * successful.
    */

   switch (systemError) {
   case EPERM:
   case EACCES:
      err = VIX_E_FILE_ACCESS_ERROR;
      break;
   case EAGAIN:
   case EBUSY:
      err = VIX_E_OBJECT_IS_BUSY;
      break;
   case EEXIST:
      err = VIX_E_FILE_ALREADY_EXISTS;
      break;
   case EFBIG:
      err = VIX_E_FILE_TOO_BIG;
      break;
   case ENOTEMPTY:
      err = VIX_E_DIRECTORY_NOT_EMPTY;
      break;
   case ENOTDIR:
      err = VIX_E_NOT_A_DIRECTORY;
      break;
#ifndef _WIN32
   case ETIMEDOUT:
   case ENOBUFS:
#endif
   case EIO:
   case EMFILE:
   case ENFILE:
   case EMLINK:
   case EROFS:
      Log("%s: errno = %d\n", __FUNCTION__,
                              systemError);
      err = VIX_E_FILE_ERROR;
      break;
   case ENODEV:
   case ENOENT:
      err = VIX_E_FILE_NOT_FOUND;
      break;
   case ENOSPC:
      err = VIX_E_DISK_FULL;
      break;
   case EISDIR:
      err = VIX_E_NOT_A_FILE;
      break;
   case ESRCH:
      err = VIX_E_NO_SUCH_PROCESS;
      break;
   case ENAMETOOLONG:
      err = VIX_E_FILE_NAME_TOO_LONG;
      break;
#ifndef _WIN32
   case EMSGSIZE:
#endif
   case EINVAL:
      err = VIX_E_INVALID_ARG;
      break;
#ifndef _WIN32
   case ELOOP:
#endif
   case ENOMEM:
      err = VIX_E_OUT_OF_MEMORY;
      break;
   default:
      err = VIX_E_FAIL;
   }
   Log("Foundry operation failed with system error: %s (%d), translated to %"FMT64"d\n",
       strerror(systemError), systemError, err);

   return err;
} // Vix_TranslateErrno

