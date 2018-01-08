/*********************************************************
 * Copyright (C) 2005-2017 VMware, Inc. All rights reserved.
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
 * cryptoError.h --
 *
 *      Error code for cryptographic infrastructure library.
 */

#ifndef VMWARE_CRYPTOERROR_H
#define VMWARE_CRYPTOERROR_H 1

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vmware.h"

typedef int CryptoError;

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * This set of errors should not be expanded beyond a maximum value of 15
 * without also updating the code for AIOMgr errors, which allots only 4 bits
 * for sub-error codes.
 *
 * Adding a lot of error codes to describe particular errors is a bad idea
 * anyhow, because it can be a security hole in itself; see, for example, the
 * SSL vulnerability described at <http://www.openssl.org/~bodo/tls-cbc.txt>.
 * It is best to distinguish only those types of errors that the caller can
 * legitimately use to figure out how to fix the problem and try again.
 */
#define CRYPTO_ERROR_SUCCESS            ((CryptoError) 0)
#define CRYPTO_ERROR_OPERATION_FAILED   ((CryptoError) 1)
#define CRYPTO_ERROR_UNKNOWN_ALGORITHM  ((CryptoError) 2)
#define CRYPTO_ERROR_BAD_BUFFER_SIZE    ((CryptoError) 3)
#define CRYPTO_ERROR_INVALID_OPERATION  ((CryptoError) 4)
#define CRYPTO_ERROR_NOMEM              ((CryptoError) 5)
#define CRYPTO_ERROR_NEED_PASSWORD      ((CryptoError) 6)
#define CRYPTO_ERROR_BAD_PASSWORD       ((CryptoError) 7)
#define CRYPTO_ERROR_IO_ERROR           ((CryptoError) 8)
#define CRYPTO_ERROR_UNKNOWN_ERROR      ((CryptoError) 9)
#define CRYPTO_ERROR_NAME_NOT_FOUND     ((CryptoError) 10)
#define CRYPTO_ERROR_NO_CRYPTO          ((CryptoError) 11)
#define CRYPTO_ERROR_LOCK_FAILURE       ((CryptoError) 12)

const char *
CryptoError_ToString(CryptoError error);

const char *
CryptoError_ToMsgString(CryptoError error);


static INLINE int
CryptoError_ToInteger(CryptoError error)
{
   return (int) error;
}

static INLINE CryptoError
CryptoError_FromInteger(int index)
{
   return (CryptoError) index;
}

static INLINE Bool
CryptoError_IsSuccess(CryptoError error)
{
   return (CRYPTO_ERROR_SUCCESS == error);
}

static INLINE Bool
CryptoError_IsFailure(CryptoError error)
{
   return (CRYPTO_ERROR_SUCCESS != error);
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* cryptoError.h */
