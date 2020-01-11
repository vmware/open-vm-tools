/*********************************************************
 * Copyright (C) 2011-2019 VMware, Inc. All rights reserved.
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
 * @file impersonate.c
 *
 * Impersonation APIs
 */

#include "VGAuthInt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#endif

/*
 ******************************************************************************
 * VGAuth_IsRunningAsRoot --                                             */ /**
 *
 * Checks if the user is running as root/system.
 *
 * @return TRUE if the user is running as root.
 *
 ******************************************************************************
 */

gboolean
VGAuth_IsRunningAsRoot(void)
{
   gboolean isRoot = FALSE;
#ifndef _WIN32
   uid_t uid;

   uid = getuid();

   if (0 == uid) {
      isRoot = TRUE;
   }
#endif

   return isRoot;
}


/*
 ******************************************************************************
 * VGAuth_CreateHandleForUsername --                                     */ /**
 *
 * Creates a new VGAuthUserHandle associated with @a userName.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  userName   The user.
 * @param[in]  token      The access token on Windows. The ownership is passed
 *                        to the returned UserHandle object if successful.
 *                        The parameter is ignored on other platforms.
 * @param[out] handle     The new handle.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_CreateHandleForUsername(VGAuthContext *ctx,
                               const char *userName,
                               const VGAuthUserHandleType type,
                               HANDLE token,
                               VGAuthUserHandle **handle)
{
   VGAuthError err = VGAUTH_E_OK;
   VGAuthUserHandle *newHandle;

   if (!g_utf8_validate(userName, -1, NULL)) {
      Warning("%s: invalid username\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   newHandle = g_malloc0(sizeof(VGAuthUserHandle));

   newHandle->userName = g_strdup(userName);
   newHandle->details.type = type;
   newHandle->flags = VGAUTH_HANDLE_FLAG_NONE;

   switch (type) {
   case VGAUTH_AUTH_TYPE_NAMEPASSWORD:
   case VGAUTH_AUTH_TYPE_SSPI:
   case VGAUTH_AUTH_TYPE_SAML:
      newHandle->flags = VGAUTH_HANDLE_FLAG_NORMAL;
      break;
   case VGAUTH_AUTH_TYPE_SAML_INFO_ONLY:
      break;
   default:
      Warning("%s: trying to create handle with unsupported type %d\n",
              __FUNCTION__, type);
   }

#ifdef _WIN32
   newHandle->token = token;
   newHandle->hProfile = INVALID_HANDLE_VALUE;
#endif

   newHandle->refCount = 1;
   *handle = newHandle;

   Debug("%s: Created handle %p\n", __FUNCTION__, newHandle);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_SetUserHandleSamlInfo --                                       */ /**
 *
 * Sets the SAML data associated with a userhandle.
 *
 * @param[in]  ctx         The VGAuthContext.
 * @param[in]  handle      The handle being updated.
 * @param[in]  samlSubject The SAML subject to be associated with the handle.
 * @param[in]  ai          The AliasInfo to be associated with the handle.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SetUserHandleSamlInfo(VGAuthContext *ctx,
                             VGAuthUserHandle *handle,
                             const char *samlSubject,
                             VGAuthAliasInfo *ai)
{
   ASSERT((handle->details.type == VGAUTH_AUTH_TYPE_SAML)
          || (handle->details.type == VGAUTH_AUTH_TYPE_SAML_INFO_ONLY));

   handle->details.val.samlData.subject = g_strdup(samlSubject);
   VGAuth_CopyAliasInfo(ai, &(handle->details.val.samlData.aliasInfo));

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_UserHandleUsername --                                          */ /**
 *
 * @brief Returns the user associated with @a handle.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  handle     The handle.
 * @param[out] userName   The user. Must be freed by the caller using
 *                        VGAuth_FreeBuffer().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_UserHandleUsername(VGAuthContext *ctx,
                          VGAuthUserHandle *handle,
                          char **userName)                       // OUT
{
   if ((NULL == ctx) || (NULL == handle) || (NULL == userName)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   *userName = g_strdup(handle->userName);

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_UserHandleType --                                              */ /**
 *
 * @brief Returns the type of @a handle.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  handle     The handle.
 *
 * @return The handle type or VGAUTH_AUTH_TYPE_UNKNOWN on error.
 *
 ******************************************************************************
 */

VGAuthUserHandleType
VGAuth_UserHandleType(VGAuthContext *ctx,
                      VGAuthUserHandle *handle)
{
   if ((NULL == ctx) || (NULL == handle)) {
      Warning("%s: Invalid arguments\n", __FUNCTION__);
      return VGAUTH_AUTH_TYPE_UNKNOWN;
   }

   return handle->details.type;
}


/*
 ******************************************************************************
 * VGAuth_UserHandleSamlData --                                          */ /**
 *
 * @brief Returns the user associated with @a handle.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  ctx                     The VGAuthContext.
 * @param[in]  handle                  The handle.
 * @param[out] samlTokenSubject        The SAML subject in the SAML token
 *                                     used to create the userHandle.
 *                                     Must be freed by the caller using
 *                                     VGAuth_FreeBuffer().
 * @param[out] matchedAliasInfo        The VGAuthAliasInfo used to validate
 *                                     the SAML token used in the userHandle.
 *                                     Must be freed by calling
 *                                     VGAuth_FreeAliasInfo()
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_UserHandleSamlData(VGAuthContext *ctx,
                          VGAuthUserHandle *handle,
                          char **samlTokenSubject,
                          VGAuthAliasInfo **matchedAliasInfo)
{
   VGAuthAliasInfo *ai;

   if ((NULL == ctx) || (NULL == handle) || (NULL == samlTokenSubject) ||
       (NULL == matchedAliasInfo)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if ((handle->details.type != VGAUTH_AUTH_TYPE_SAML) &&
       (handle->details.type != VGAUTH_AUTH_TYPE_SAML_INFO_ONLY)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   ai = g_malloc0(sizeof(VGAuthAliasInfo));

   VGAuth_CopyAliasInfo(&(handle->details.val.samlData.aliasInfo), ai);

   *samlTokenSubject = g_strdup(handle->details.val.samlData.subject);
   *matchedAliasInfo = ai;

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_UserHandleFree --                                              */ /**
 *
 * @brief Frees a #VGAuthUserHandle.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  handle     The handle to be freed.  No-op if NULL.
 *
 ******************************************************************************
 */

void
VGAuth_UserHandleFree(VGAuthUserHandle *handle)
{
   if (NULL == handle) {
      return;
   }

   ASSERT(handle->refCount > 0);
   if (handle->refCount <= 0) {
      Warning("%s: invalid user handle reference count %d\n",
              __FUNCTION__, handle->refCount);
      return;
   }

   handle->refCount--;

   if (handle->refCount > 0) {
      return;
   }

   WIN32_ONLY(CloseHandle(handle->token));

   g_free(handle->userName);

   if (handle->details.type == VGAUTH_AUTH_TYPE_SAML ||
       handle->details.type == VGAUTH_AUTH_TYPE_SAML_INFO_ONLY) {
      g_free(handle->details.val.samlData.subject);
      VGAuth_FreeAliasInfoContents(&(handle->details.val.samlData.aliasInfo));
   }

   Debug("%s: Freeing handle %p\n", __FUNCTION__, handle);

   g_free(handle);
}


/*
 ******************************************************************************
 * VGAuth_Impersonate --                                                 */ /**
 *
 * @brief Starts impersonating the user represented by @a handle.
 *
 * Note that this will change the entire process on Linux to the
 * user represented by the #VGAuthUserHandle (so it must be called by root).
 *
 * The effective uid/gid, @b $HOME, @b $USER and @b $SHELL are changed;
 * however, no @b $SHELL startup files are run, so you cannot assume that
 * other environment variables have been changed.
 *
 * Calls to the API cannot be nested; call VGAuth_EndImpersonation()
 * before another call to VGAuth_Impersonate() is made.
 *
 * @remark Must be called by superuser.
 *         One @a extraParams is supported for Windows:
 *         VGAUTH_PARAM_LOAD_USER_PROFILE, which must have the value
 *         VGAUTH_PARAM_VALUE_TRUE or VGAUTH_PARAM_VALUE_FALSE.
 *         If set true, load user profile before impersonation.
 *
 * @param[in]  ctx             The VGAuthContext.
 * @param[in]  handle          The handle representing the user to be
 *                             impersonated.
 * @param[in]  numExtraParams  The number of elements in extraParams.
 * @param[in]  extraParams     Any optional, additional paramaters to the
 *                             function. Currently none are supported, so
 *                             this must be NULL.
 *
 * @retval VGAUTH_E_ALREADY_IMPERSONATING If the context is already impersonating.
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_Impersonate(VGAuthContext *ctx,
                   VGAuthUserHandle *handle,
                   int numExtraParams,
                   const VGAuthExtraParams *extraParams)
{
   VGAuthError err;
   gboolean loadUserProfile;

   if ((NULL == ctx) || (NULL == handle)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!(handle->flags & VGAUTH_HANDLE_FLAG_CAN_IMPERSONATE)) {
      Warning("%s: called on handle that doesn't not support operation \n",
              __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuthGetBoolExtraParam(numExtraParams, extraParams,
                                 VGAUTH_PARAM_LOAD_USER_PROFILE,
                                 FALSE,
                                 &loadUserProfile);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   if (ctx->isImpersonating) {
      return VGAUTH_E_ALREADY_IMPERSONATING;
   }

   err = VGAuthImpersonateImpl(ctx,
                               handle,
                               loadUserProfile);
   if (VGAUTH_E_OK == err) {
      ctx->isImpersonating = TRUE;
      handle->refCount++;
      ctx->impersonatedUser = handle;
   }

   return err;
}


/*
 ******************************************************************************
 * VGAuth_EndImpersonation --                                            */ /**
 *
 * @brief Ends the current impersonation.
 *
 * Restores the process to superUser, and resets @b $USER, @b $HOME and @b $SHELL.
 *
 * @remark Must be called by superuser.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success or no-op, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_EndImpersonation(VGAuthContext *ctx)
{
   VGAuthError err;

   if (NULL == ctx) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!ctx->isImpersonating) {
      Debug("%s: not currently impersonating; ignoring\n", __FUNCTION__);
      return VGAUTH_E_OK;
   }

   err = VGAuthEndImpersonationImpl(ctx);
   if (VGAUTH_E_OK == err) {
      ctx->isImpersonating = FALSE;
      VGAuth_UserHandleFree(ctx->impersonatedUser);
      ctx->impersonatedUser = NULL;
   }

   return err;
}
