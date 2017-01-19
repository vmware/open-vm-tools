/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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
 * @file VGAuthError.h
 *
 * Enums common to both client library and service.
 *
 * @addtogroup vgauth_error VGAuth Error Codes
 * @{
 *
 */
#ifndef _VGAUTHERROR_H_
#define _VGAUTHERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <BaseTsd.h>

typedef UINT32 vga_uint32;
typedef unsigned __int64 vga_uint64;
#else
#include <stdint.h>

typedef uint32_t vga_uint32;
typedef long long unsigned int vga_uint64;
#endif

/**
 * Returns the base VGAuthError.
 */
#define VGAUTH_ERROR_CODE(err) ((err) & 0xFFFFFFFF)

/**
 * Use to test the return value from a VGAuth API for success.
 */
#define VGAUTH_SUCCEDED(err)   (VGAUTH_E_OK == (err))

/**
 * Use to test the return value from a VGAuth API for failure.
 */
#define VGAUTH_FAILED(err)     (VGAUTH_E_OK != (err))

/*
 * Printf format specifiers for VGAuthError.
 * Use them like this:
 *    printf("VGAUTHERR_FMT64X"\n", err);
 */

#ifdef _MSC_VER
   #define VGAUTHERR_FMT64X     "0x%I64x"
   #define VGAUTHERR_FMT64      "%I64u"
#elif defined __APPLE__
   /* Mac OS hosts use the same formatters for 32- and 64-bit. */
   #define VGAUTHERR_FMT64X "0x%llx"
   #define VGAUTHERR_FMT64  "%llu"
#elif __GNUC__
   #if defined(sun) || defined(__FreeBSD__)
      #define VGAUTHERR_FMT64X    "0x%llx"
      #define VGAUTHERR_FMT64     "%llu"
   #else
      #define VGAUTHERR_FMT64X    "0x%Lx"
      #define VGAUTHERR_FMT64     "%Lu"
   #endif
#else
   #error - Need compiler define for FMT64 and FMTSZ
/* Doxygen input */

/**
 * Format specifier for VGAuth errors as a hexadecimal value.
 * @n
 * Example usage:
 * @code
 *    printf("Error in hex: "VGAUTHERR_FMT64X"\n", err);
 * @endcode
 */
#define VGAUTHERR_FMT64X

/**
 * Format specifier for VGAuth errors as a decimal value.
 * @n
 * Example usage:
 * @code
 *    printf("Error: "VGAUTHERR_FMT64"\n", err);
 * @endcode
 */
#define VGAUTHERR_FMT64

#endif

/**
 * This is an expanded view of a VGAuthError.
 * Every VGAuthError is a 64 bit unsigned int, so it can fit into this
 * struct.
 *
 * The extraError is optional.  It is guarenteed to be 0 when the error
 * is @b VGAUTH_E_OK, so any program that checks (@b VGAUTH_E_OK == err) or
 * (@b VGAUTH_E_OK != err) will always work.
 *
 * extraError may be set in certain error conditions when extra info may
 * be of use to the caller.
 *
 * The basic error field is a VGAuth error value, and it's the lsb
 * of the new struct. This means that a 64-bit error can be assigned
 * to an enum constant, like an integer. For example, err = @b VGAUTH_E_FAIL;
 * works.  This just leaves the flags and extraError fields as 0.
 */
typedef
#ifdef _MSC_VER
#   pragma pack(push, 1)
#elif __GNUC__
#else
#   error Compiler packing...
#endif
struct VGAuthErrorFields {
   vga_uint32   error;
   vga_uint32   extraError;
}
#ifdef _MSC_VER
#   pragma pack(pop)
#elif __GNUC__
__attribute__((__packed__))
#else
#   error Compiler packing...
#endif
VGAuthErrorFields;

/**
 * Extra error accessor macro.  Use to get additional error info from
 * #VGAUTH_E_SYSTEM_ERRNO or #VGAUTH_E_SYSTEM_WINDOWS.
 */
#define VGAUTH_ERROR_EXTRA_ERROR(err) ((VGAuthErrorFields *) &err)->extraError

/**
 * Error codes.
 */

typedef vga_uint64 VGAuthError;
enum {
   /** No error. */
   VGAUTH_E_OK                         = 0,

   /** Unspecified failure. */
   VGAUTH_E_FAIL                       = 1,
   /** Invalid argument passed to API. */
   VGAUTH_E_INVALID_ARGUMENT           = 2,
   /** Invalid certficate. */
   VGAUTH_E_INVALID_CERTIFICATE        = 3,
   /** Permission denied. */
   VGAUTH_E_PERMISSION_DENIED          = 4,
   /** Out of memory for operation. */
   VGAUTH_E_OUT_OF_MEMORY              = 5,
   /** Internal communication error between client and service. */
   VGAUTH_E_COMM                       = 6,
   /** Not implemented. */
   VGAUTH_E_NOTIMPLEMENTED             = 7,
   /** Not connected to service. */
   VGAUTH_E_NOT_CONNECTED              = 8,
   /** Version mismatch. */
   VGAUTH_E_VERSION_MISMATCH           = 9,
   /** Security violation. */
   VGAUTH_E_SECURITY_VIOLATION         = 10,
   /** The certficate already exists. */
   VGAUTH_E_CERT_ALREADY_EXISTS        = 11,
   /** Authentication denied. */
   VGAUTH_E_AUTHENTICATION_DENIED      = 12,
   /** Ticket is invalid. */
   VGAUTH_E_INVALID_TICKET             = 13,
   /**
    * The cert was found associated with more than one user, or a chain
    * contained multiple matches againast the mappings file.
    */
   VGAUTH_E_MULTIPLE_MAPPINGS          = 14,
   /** The context is already impersonating. */
   VGAUTH_E_ALREADY_IMPERSONATING      = 15,
   /** User does not exist. */
   VGAUTH_E_NO_SUCH_USER               = 16,
   /** Operation failed because service does not appear to be running. */
   VGAUTH_E_SERVICE_NOT_RUNNING        = 17,
   /**
    * Failed to process an OS-specific Posix API operation;
    * use #VGAUTH_ERROR_EXTRA_ERROR for the OS specific Posix errno.
    */
   VGAUTH_E_SYSTEM_ERRNO               = 18,
   /**
    * Failed to process an OS-specific Win32 API operation;
    * use #VGAUTH_ERROR_EXTRA_ERROR for the OS specific Windows errno.
    */
   VGAUTH_E_SYSTEM_WINDOWS             = 19,
   /** Maximum number of connections is reached */
   VGAUTH_E_TOO_MANY_CONNECTIONS       = 20,
   /** Operation not supported. */
   VGAUTH_E_UNSUPPORTED                = 21,
};


/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif   // _VGAUTHERROR_H_
