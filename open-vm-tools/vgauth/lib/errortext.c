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

/*
 * @file errortext.c
 *
 * Error descriptions.
 */

#include "VGAuthCommon.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct VGAuthErrorCodeInfo {
   VGAuthError err;
   const char *name;
   const char *msg;
} VGAuthErrorCodeInfo;

#define _DEFINE_VGAUTH_ERR(err, str) {err, #err, str},

/*
 * This is the global table that maps error codes to human-readable
 * descriptions.
 *
 * Note, that the UI requires strings to never end with
 * a period. So, if a string contains several sentences, then
 * the last sentence does not end with a period. See Bug 52793.
 *
 * It would be nice to be able to combine this with the doxygen comments
 * in VGAuthError.h.  However, this can be spun as a feature, since the
 * doxygen commentary is intended for developers, while these are intended
 * for users.
 * See the differences in the VGAUTH_E_SYSTEM_ERROR descriptions as an example.
 */

const static VGAuthErrorCodeInfo vgauthErrorCodeInfoList[] =
{
   _DEFINE_VGAUTH_ERR(VGAUTH_E_OK, "The operation was successful")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_INVALID_ARGUMENT, "One of the parameters was invalid")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_INVALID_CERTIFICATE, "The certificate is not a well-formed x509 document")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_PERMISSION_DENIED, "Insufficient permissions")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_OUT_OF_MEMORY, "Out of memory")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_COMM, "Internal communication error between library and service")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_NOTIMPLEMENTED, "Not implemented")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_NOT_CONNECTED, "Not connected to the service")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_VERSION_MISMATCH, "Service/library version mismatch")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_SECURITY_VIOLATION, "Potential security violation detected")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_CERT_ALREADY_EXISTS, "The certificate already exists")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_AUTHENTICATION_DENIED, "Authentication denied")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_INVALID_TICKET, "Invalid ticket")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_MULTIPLE_MAPPINGS, "The certificate was found associated with more than one user, or a chain contained multiple matches against the mapping file")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_ALREADY_IMPERSONATING, "The context is already impersonating")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_NO_SUCH_USER, "User cannot be found")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_SERVICE_NOT_RUNNING, "Service not running")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_SYSTEM_ERRNO, "An OS-specific operation failed")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_SYSTEM_WINDOWS, "An OS-specific operation failed")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_TOO_MANY_CONNECTIONS,
                      "The user exceeded its max number of connections")
   _DEFINE_VGAUTH_ERR(VGAUTH_E_UNSUPPORTED, "The operation is not supported")


   /*
    * Add new error definitions above.
    *
    * VGAUTH_E_FAIL must appear last, to catch any unspecfied errors.
    */

   _DEFINE_VGAUTH_ERR(VGAUTH_E_FAIL, "Unknown error")
};


/*
 ******************************************************************************
 * VGAuthErrorInfo --                                                    */ /**
 *
 * Returns the full description of the error.
 *
 * @param[in]  err        The VGAuthError.
 *
 * @return A VGAuthErrorInfo of the error.
 *
 ******************************************************************************
 */

static const VGAuthErrorCodeInfo *
VGAuthGetErrorInfo(VGAuthError err)
{
   const VGAuthErrorCodeInfo *errorInfo;

   err = VGAUTH_ERROR_CODE(err);

   for (errorInfo = vgauthErrorCodeInfoList;
        errorInfo->err != err && errorInfo->err != VGAUTH_E_FAIL;
        ++errorInfo);

   return errorInfo;
}


/*
 ******************************************************************************
 * VGAuth_GetErrorText --                                                */ /**
 *
 * Returns explanatory text for an error code.  This returns a reference
 * to a static global string, do not free it.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  errCode    The VGAuthError code.
 * @param[in]  language   The language to use in RFC-1766 language code (currently unused).
 *
 * @return A description of @a errCode.
 *
 ******************************************************************************
 */

const char *
VGAuth_GetErrorText(VGAuthError errCode,
                    const char *language)
{
   const VGAuthErrorCodeInfo *errorInfo;

   errorInfo = VGAuthGetErrorInfo(errCode);

   /*
    * XXX Add error localization.
    */

   return errorInfo->msg;
}


/*
 ******************************************************************************
 * VGAuth_GetErrorName --                                                */ /**
 *
 * Returns the name of the error code.  This returns a reference
 * to a static global string, do not free it.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  errCode   The VGAuthError code.
 *
 * @return The name of @a errCode.
 *
 ******************************************************************************
 */

const char *
VGAuth_GetErrorName(VGAuthError errCode)
{
   return VGAuthGetErrorInfo(errCode)->name;
}
