/*********************************************************
 * Copyright (c) 2011-2017,2023 VMware, Inc. All rights reserved.
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
 * @file auth.c
 *
 * Authentication APIs
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "VGAuthInt.h"
#include "VGAuthLog.h"
#include "VGAuthUtil.h"
#include "usercheck.h"


/*
 ******************************************************************************
 * VGAuthInitAuthentication --                                           */ /**
 *
 * Initializes any resources needed for authentication.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuthInitAuthentication(VGAuthContext *ctx)
{
   return VGAuthInitAuthenticationPlatform(ctx);
}


/*
 ******************************************************************************
 * VGAuthShutdownAuthentication --                                       */ /**
 *
 * Releases any resources used for authentication.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 ******************************************************************************
 */

void
VGAuthShutdownAuthentication(VGAuthContext *ctx)
{
   VGAuthShutdownAuthenticationPlatform(ctx);
}


/*
 ******************************************************************************
 * VGAuth_CreateTicket --                                                */ /**
 *
 * @brief Creates a new ticket associated with the user represented by
 * @a handle.
 *
 * @remark On a non-Windows OS, the function must be called by root
 *         or the user associated with @a handle.
 *         On Windows, the function must be called by the local system account
 *         or an account in the administrators group or the user associated
 *         with @a handle.
 *
 * @param[in]  ctx            The VGAuthContext.
 * @param[in]  handle         The handle representing the user.
 * @param[in]  numExtraParams The number of elements in extraParams.
 * @param[in]  extraParams    Any optional, additional paramaters to the
 *                            function. Currently none are supported, so this
 *                            must be NULL.
 * @param[out] newTicket      The resulting ticket. Must be freed by the caller
 *                            using VGAuth_FreeBuffer().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @retval VGAUTH_E_SERVICE_NOT_RUNNING If the service cannot be contacted.
 * @retval VGAUTH_E_PERMISSION_DENIED If not called by superuser or the
 *    the user associated with @a handle.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_CreateTicket(VGAuthContext *ctx,
                    VGAuthUserHandle *handle,
                    int numExtraParams,
                    const VGAuthExtraParams *extraParams,
                    char **newTicket)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == handle) || (NULL == newTicket)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!(handle->flags & VGAUTH_HANDLE_FLAG_CAN_CREATE_TICKET)) {
      Warning("%s: called on handle that doesn't not support operation \n",
              __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }
   err = VGAuth_SendCreateTicketRequest(ctx, handle, newTicket);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_ValidateTicket --                                              */ /**
 *
 * @brief Validates @a ticket and returns a handle associated with it.
 *
 * @remark On a non-Windows OS, the function must be called by root.
 *         On Windows, the function must be called by the local system account
 *         or an account in the administrators group.
 *
 * @param[in]  ctx            The VGAuthContext.
 * @param[in]  ticket         The ticket to be validated.
 * @param[in]  numExtraParams The number of elements in extraParams.
 * @param[in]  extraParams    Any optional, additional paramaters to the
 *                            function. Currently none are supported, so this
 *                            must be NULL.
 * @param[out] handle         The resulting handle representing the user
 *                            associated with the ticket.
 *                            Must be freed with VGAuth_UserHandleFree().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @retval VGAUTH_E_INVALID_TICKET If @a ticket is not valid.
 * @retval VGAUTH_E_SERVICE_NOT_RUNNING If the service cannot be contacted.
 * @retval VGAUTH_E_PERMISSION_DENIED If not called by superuser.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_ValidateTicket(VGAuthContext *ctx,
                      const char *ticket,
                      int numExtraParams,
                      const VGAuthExtraParams *extraParams,
                      VGAuthUserHandle **handle)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == ticket) || (NULL == handle)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(ticket, -1, NULL)) {
      Warning("%s: invalid ticket\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuth_SendValidateTicketRequest(ctx, ticket, handle);
   if (err != VGAUTH_E_OK) {
      goto done;
   }

done:

   return err;
}


/*
 ******************************************************************************
 * VGAuth_RevokeTicket --                                                */ /**
 *
 * @brief Revokes @a ticket.
 *
 * If the ticket does not exist or the calling user does not own it,
 * this operation is a no-op and returns VGAUTH_E_OK.
 *
 * @remark On a non-Windows OS, the function must be called by root
 *         or the owner of the ticket.
 *         On Windows, the function must be called by the local system account
 *         or an account in the administrators group
 *         or the owner of the ticket.
 *
 * @param[in]  ctx            The VGAuthContext.
 * @param[in]  ticket         The ticket to be revoked.
 * @param[in]  numExtraParams The number of elements in extraParams.
 * @param[in]  extraParams    Any optional, additional paramaters to the
 *                            function. Currently none are supported, so this
 *                            must be NULL.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @retval VGAUTH_E_SERVICE_NOT_RUNNING If the service cannot be contacted.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_RevokeTicket(VGAuthContext *ctx,
                    const char *ticket,
                    int numExtraParams,
                    const VGAuthExtraParams *extraParams)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == ticket)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(ticket, -1, NULL)) {
      Warning("%s: invalid ticket\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuth_SendRevokeTicketRequest(ctx, ticket);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_GenerateSSPIChallenge --                                       */ /**
 *
 * @brief Generates a challenge for an SSPI authentication.
 *
 * Takes an initial request to begin an SSPI negotiation and generates a
 * challenge used to complete the negotiation. This uses the "Negotate"
 * security package to perform the authentication. The client first calls
 * AcquireCredentialsHandle(). The client then calls InitializeSecurityContext().
 * The resulting partially-formed context is passed to this function.
 *
 * For more information, see the MSDN documentation on SSPI.
 *
 * @remark On Windows, the function must be called by the local system account
 *         or an account from the administrators group.
 *
 * @param[in]  ctx            The VGAuthContext.
 * @param[in]  requestLen     The size of the request in bytes.
 * @param[in]  request        The SSPI request.
 * @param[in]  numExtraParams The number of elements in extraParams.
 * @param[in]  extraParams    Any optional, additional paramaters to the
 *                            function. Currently none are supported, so this
 *                            must be NULL.
 * @param[out] id             An identifier to use when validating the
 *                            response.
 * @param[out] challengeLen   The length of the challenge in bytes.
 * @param[out] challenge      The SSPI challenge to send to the client. Must be
 *                            freed with VGAuth_FreeBuffer().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a NULL argument.
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_GenerateSSPIChallenge(VGAuthContext *ctx,
                             size_t requestLen,
                             const unsigned char *request,
                             int numExtraParams,
                             const VGAuthExtraParams *extraParams,
                             unsigned int *id,
                             size_t *challengeLen,
                             unsigned char **challenge)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == request) || (NULL == challengeLen) ||
       (NULL == challenge) || (NULL == id)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   return VGAuthGenerateSSPIChallengeImpl(ctx, requestLen, request, id,
                                          challengeLen, challenge);
}


/*
 ******************************************************************************
 * VGAuth_ValidateSSPIResponse --                                        */ /**
 *
 * @brief Validates an SSPI response.
 *
 * The client should use InitializeSecurityContext() to generate the response
 * from the challenge returned by VGAuth_GenerateSSPIChallenge().
 * The response must be received within a per-system configurable timeout,
 * or the challenge will be discarded, causing @a id to no longer be valid.
 *
 * For more information, see the MSDN documentation on SSPI.
 *
 * @remark On Windows, the function must be called by the local system account
 *         or an account from the administrators group.
 *
 * @param[in]  ctx            The VGAuthContext. Must be the same VGAuthContext
 *                            as was passed to the corresponding call to
 *                            VGAuth_GenerateSSPIChallenge().
 * @param[in]  id             The identifier returned by
 *                            VGAuth_GenerateSSPIChallenge() when the challenge
 *                            was generated.
 * @param[in]  responseLen    The length of the response in bytes.
 * @param[in]  response       The response to be validated.
 * @param[in]  numExtraParams The number of elements in extraParams.
 * @param[in]  extraParams    Any optional, additional paramaters to the
 *                            function. Currently none are supported, so this
 *                            must be NULL.
 * @param[out] handle         The resulting handle representing the user.
 *                            Must be freed with VGAuth_UserHandleFree().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a NULL argument.
 * @retval VGAUTH_E_AUTHENTICATION_DENIED If the response fails validation.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_ValidateSSPIResponse(VGAuthContext *ctx,
                            unsigned int id,
                            size_t responseLen,
                            const unsigned char *response,
                            int numExtraParams,
                            const VGAuthExtraParams *extraParams,
                            VGAuthUserHandle **handle)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == response) || (NULL == handle)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   return VGAuthValdiateSSPIResponseImpl(ctx, id, responseLen, response,
                                         handle);
}


/*
 ******************************************************************************
 * VGAuth_ValidateSamlBearerToken --                                     */ /**
 *
 * @brief Authenticate using a SAML bearer token.
 *
 * Takes a SAML bearer token and determines whether that token is valid
 * and whether the principal specified in the "Subject" field is trusted
 * given the current identity provider store for the requested user.
 *
 * The token is valid if:
 * <ol>
 *    <li>it is well formed and conforms the SAML assertion schema,</li>
 *    <li>any conditions specified in the assertion, including any
 *        "NotBefore" or "NotOnOrAfter" information, must be true,</li>
 *    <li>the Subject element contains a SubjectConfirmation element and the
 *        SubjectConfirmation method is "bearer," and</li>
 *    <li>the assertion is correctly signed by a certificate contained within
 *        the assertion.</li>
 * </ol>
 *
 * The principal is trusted if
 * <ol>
 *    <li>the issuer of the token has certificate where a chain of trust can
 *        be established to an identity provider certificate in the local
 *        user's identity provider store, and</li>
 *    <li>the subject named in the token is on the list of trusted principals
 *        associated with the matching identity provider certificate or that
 *        identity provider certificate allows any principal to be
 *        authenticated.</li>
 * </ol>
 *
 * @remark Supported @a extraParams:
 *         VGAUTH_PARAM_VALIDATE_INFO_ONLY, which must have the value
 *         VGAUTH_PARAM_VALUE_TRUE or VGAUTH_PARAM_VALUE_FALSE.
 *         If set, SAML token validation is done, but the returned
 *         @a handle cannot be used for impersonation or ticket
 *         creation.
 *
 *         VGAUTH_PARAM_SAML_HOST_VERIFIED, which must have the value
 *         VGAUTH_PARAM_VALUE_TRUE or VGAUTH_PARAM_VALUE_FALSE.
 *         If set, the SAML token has been verified by the host
 *         and this service will skip that step when validating.
 *
 * @param[in]  ctx            The VGAuthContext.
 * @param[in]  samlToken      The SAML token to be validated.
 * @param[in]  userName       The user to authenticate as. Optional.
 *                            If the user is not specified, the mapped
 *                            identities files will be used to determine
 *                            which user to authenticate as, based on the
 *                            token issuer's certificiate and the token's
 *                            subject.
 * @param[in]  numExtraParams The number of elements in extraParams.
 * @param[in]  extraParams    Any optional, additional paramaters to the
 *                            function.
 * @param[out] handle         The resulting handle representing the user
 *                            associated with the username.
 *                            Must be freed with VGAuth_UserHandleFree().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @retval VGAUTH_E_SERVICE_NOT_RUNNING If the service cannot be contacted.
 * @retval VGAUTH_E_PERMISSION_DENIED If not called by superuser.
 * @retval VGAUTH_E_AUTHENTICATION_DENIED If the token is not validate or the
 *                                        principal is not trusted.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */


VGAuthError
VGAuth_ValidateSamlBearerToken(VGAuthContext *ctx,
                               const char *samlToken,
                               const char *userName,
                               int numExtraParams,
                               const VGAuthExtraParams *extraParams,
                               VGAuthUserHandle **handle)
{
   VGAuthError err;
   VGAuthUserHandle *newHandle = NULL;
   gboolean validateOnly;
   gboolean hostVerified;

   /*
    * arg check
    */
   if ((NULL == ctx) || (NULL == samlToken) || (NULL == handle)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   *handle = NULL;

   if (!g_utf8_validate(samlToken, -1, NULL)) {
      Warning("%s: SAML token is not valid UTF-8.\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if ((NULL != userName) && !g_utf8_validate(userName, -1, NULL)) {
      Warning("%s: Username is not valid UTF-8.\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if ((NULL != userName) && !Usercheck_UsernameIsLegal(userName)) {
      Warning("Username '%s' contains invalid characters\n", userName);
      return VGAUTH_E_INVALID_ARGUMENT;
   }


   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuthGetBoolExtraParam(numExtraParams, extraParams,
                                 VGAUTH_PARAM_VALIDATE_INFO_ONLY,
                                 FALSE,
                                 &validateOnly);
   if (VGAUTH_E_OK != err) {
      return err;
   }
   err = VGAuthGetBoolExtraParam(numExtraParams, extraParams,
                                 VGAUTH_PARAM_SAML_HOST_VERIFIED,
                                 FALSE,
                                 &hostVerified);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuth_SendValidateSamlBearerTokenRequest(ctx,
                                                   validateOnly,
                                                   hostVerified,
                                                   samlToken,
                                                   userName,
                                                   &newHandle);
   if (VGAUTH_E_OK != err) {
      goto done;
   }

   *handle = newHandle;

done:

   return err;
}


/*
 ******************************************************************************
 * VGAuth_ValidateUsernamePassword --                                    */ /**
 *
 * @brief Validates a username/password, and returns a handle associated with
 * that user.
 *
 * Note that on Windows an empty password will not be accepted unless Group
 * Policy has been changed to accept it. See Microsoft knowledge base article
 * number 303846 for more information.
 *
 * @remark On a non-Windows OS, the function must be called by root.
 *         On Windows, the function must be called by the local system account
 *         or an account in the administrators group.
 *
 * @param[in]  ctx            The VGAuthContext.
 * @param[in]  userName       The username to be validated.
 * @param[in]  password       The password to be validated.
 * @param[in]  numExtraParams The number of elements in extraParams.
 * @param[in]  extraParams    Any optional, additional paramaters to the
 *                            function. Currently none are supported, so this
 *                            must be NULL.
 * @param[out] handle         The resulting handle representing the user
 *                            associated with @a userName.
 *                            Must be freed with VGAuth_UserHandleFree().
 *
 * @retval VGAUTH_E_AUTHENTICATION_DENIED If @a userName cannot be looked up,
 *     or @a password is not correct for @a userName.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_ValidateUsernamePassword(VGAuthContext *ctx,
                                const char *userName,
                                const char *password,
                                int numExtraParams,
                                const VGAuthExtraParams *extraParams,
                                VGAuthUserHandle **handle)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == userName) || (NULL == password) || (NULL == handle)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(userName, -1, NULL)) {
      Warning("User not in UTF-8\n");
      return VGAUTH_E_INVALID_ARGUMENT;
   }
   if (!g_utf8_validate(password, -1, NULL)) {
      Warning("Password not in UTF-8\n");
      return VGAUTH_E_INVALID_ARGUMENT;
   }
   if (userName[0] == '\0') {
      Warning("Empty Username\n");
      return VGAUTH_E_INVALID_ARGUMENT;
   }
   if (!Usercheck_UsernameIsLegal(userName)) {
      Warning("Username '%s' contains invalid characters\n", userName);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuthValidateUsernamePasswordImpl(ctx,
                                            userName,
                                            password,
                                            handle);

   if (VGAUTH_E_OK == err) {
      VGAuth_AuditEvent(ctx,
                        TRUE,
                        SU_(auth.password.valid,
                            "Username and password successfully validated for '%s'"),
                        userName);
   } else {
      VGAuth_AuditEvent(ctx,
                        FALSE,
                        SU_(auth.password.invalid,
                            "Username and password mismatch for '%s'"),
                        userName);
   }

   return err;
}
