/*********************************************************
 * Copyright (c) 2011-2019,2023 VMware, Inc. All rights reserved.
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
 *
 * @file VGAuthAuthentication.h
 *
 * Client library authentication and impersonation API defintions.
 *
 * @addtogroup vgauth_auth VGAuth Authentication and Impersonation
 * @{
 *
 */
#ifndef _VGAUTHAUTHENTICATION_H_
#define _VGAUTHAUTHENTICATION_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Include stddef.h to get the definition of size_t. */
#include <stddef.h>
#include "VGAuthAlias.h"

#ifdef _WIN32
#if !defined(WIN32_LEAN_AND_MEAN) && !defined(VGAUTH_NO_WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "VGAuthCommon.h"

/* Ticket APIs */

/**
 * Opaque handle to data describing a user.
 */
typedef struct VGAuthUserHandle VGAuthUserHandle;

/*
 * The possible types of VGAuthUserHandles.
 */
typedef enum {
   /** Unknown type */
   VGAUTH_AUTH_TYPE_UNKNOWN,
   /** The userHandle was created using namePassword validation. */
   VGAUTH_AUTH_TYPE_NAMEPASSWORD,
   /** The userHandle was created using SSPI validation. */
   VGAUTH_AUTH_TYPE_SSPI,
   /** The userHandle was created using SAML validation. */
   VGAUTH_AUTH_TYPE_SAML,
   /** The userHandle was created using SAML validation, but is not
    * valid for impersonation or ticket creation. */
   VGAUTH_AUTH_TYPE_SAML_INFO_ONLY,
} VGAuthUserHandleType;


/* VGAuthUserHandle APIs */
/*
 * Simple accessor for the user associated with a VGAuthUserHandle.
 */
VGAuthError VGAuth_UserHandleUsername(VGAuthContext *ctx,
                                      VGAuthUserHandle *handle,
                                      char **userName);                // OUT

#ifdef _WIN32
/*
 * Special accessor functions for Windows to set and get user and
 * profile handles.
 */

VGAuthError VGAuth_UserHandleAccessToken(VGAuthContext *ctx,
                                         VGAuthUserHandle *handle,
                                         HANDLE *authToken);

VGAuthError VGAuth_UserHandleGetUserProfile(VGAuthContext *ctx,
                                            VGAuthUserHandle *handle,
                                            HANDLE *hProfile);

VGAuthError VGAuth_UserHandleSetUserProfile(VGAuthContext *ctx,
                                            VGAuthUserHandle *handle,
                                            HANDLE hProfile);
#endif


/*
 * Returns the type of the UserHandle;  VGAUTH_AUTH_TYPE_UNKNOWN
 * on error.
 */
VGAuthUserHandleType VGAuth_UserHandleType(VGAuthContext *ctx,
                                           VGAuthUserHandle *handle);

VGAuthError VGAuth_UserHandleSamlData(VGAuthContext *ctx,
                                      VGAuthUserHandle *handle,
                                      char **samlTokenSubject,   // OUT
                                      VGAuthAliasInfo **matchedAliasInfo); // OUT



/*
 * Releases a VGAuthUserHandle.
 */
void VGAuth_UserHandleFree(VGAuthUserHandle *handle);


/*
 * Asks for a new ticket to be created associated with 'handle'.
 */
VGAuthError VGAuth_CreateTicket(VGAuthContext *ctx,
                                VGAuthUserHandle *handle,
                                int numExtraParams,
                                const VGAuthExtraParams *extraParams,
                                char **newTicket);      // OUT

/*
 * Returns the VGAuthUserHandle associated with the ticket.
 */
VGAuthError VGAuth_ValidateTicket(VGAuthContext *ctx,
                                  const char *ticket,
                                  int numExtraParams,
                                  const VGAuthExtraParams *extraParams,
                                  VGAuthUserHandle **handle);         // OUT


/*
 * Revokes a ticket.
 */
VGAuthError VGAuth_RevokeTicket(VGAuthContext *ctx,
                                const char *ticket,
                                int numExtraParams,
                                const VGAuthExtraParams *extraParams);



/* Name/Password authentication APIs */

/*
 * If the password is valid for userName, returns a VGAuthUserHandle.
 */
VGAuthError VGAuth_ValidateUsernamePassword(VGAuthContext *ctx,
                                            const char *userName,
                                            const char *password,
                                            int numExtraParams,
                                            const VGAuthExtraParams *extraParams,
                                            VGAuthUserHandle **handle); // OUT




/* SSPI Authentication APIs */

/*
 * Generates an SSPI challenge text.
 */
VGAuthError VGAuth_GenerateSSPIChallenge(VGAuthContext *ctx,
                                         size_t sspiRequestLen,
                                         const unsigned char *sspiRequest,
                                         int numExtraParams,
                                         const VGAuthExtraParams *extraParams,
                                         unsigned int *id,              // OUT
                                         size_t *challengeLen,          // OUT
                                         unsigned char **challenge);    // OUT

/*
 * Validates an SSPI response.
 */
VGAuthError VGAuth_ValidateSSPIResponse(VGAuthContext *ctx,
                                        unsigned int id,
                                        size_t responseLen,
                                        const unsigned char *response,
                                        int numExtraParams,
                                        const VGAuthExtraParams *extraParams,
                                        VGAuthUserHandle **handle);     // OUT



/* Alias Authentication APIs */

/*
 * SAML token validation.
 */

#define  VGAUTH_PARAM_VALIDATE_INFO_ONLY  "validateInfoOnly"

#define  VGAUTH_PARAM_SAML_HOST_VERIFIED "hostVerified"

VGAuthError VGAuth_ValidateSamlBearerToken(VGAuthContext *ctx,
                                           const char *samlToken,
                                           const char *userName,
                                           int numExtraParams,
                                           const VGAuthExtraParams *extraParams,
                                           VGAuthUserHandle **handle);  // OUT

/* Impersonation APIs */

#define  VGAUTH_PARAM_LOAD_USER_PROFILE  "loadUserProfile"

/*
 * Start impersonating the user described by VGAuthUserHandle.
 */
VGAuthError VGAuth_Impersonate(VGAuthContext *ctx,
                               VGAuthUserHandle *handle,
                               int numExtraParams,
                               const VGAuthExtraParams *extraParams);

/*
 * Ends the impersonation, restores the process to root.
 */
VGAuthError VGAuth_EndImpersonation(VGAuthContext *ctx);

/** @} */


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif   // _VGAUTHAUTHENTICATION_H_
