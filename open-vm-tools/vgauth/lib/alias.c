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
 * @file alias.c
 *
 * Alias APIs
 *
 * An Alias is a combination of a certificate and a list
 * of subject names with an optional comment.  These are used to
 * map SAML token users into guest users.
 */

#include "VGAuthAlias.h"
#include "VGAuthInt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "usercheck.h"


/*
 ******************************************************************************
 * VGAuthFreeSubjectContents --                                          */ /**
 *
 * @brief Frees any data inside a VGAuthSubject structure.
 *
 * Does not free the VGAuthSubject itself.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  subj       The VGAuthSubject contents to be freed.
 *
 ******************************************************************************
 */

static void
VGAuthFreeSubjectContents(VGAuthSubject *subj)
{
   if (NULL == subj) {
      return;
   }

   switch (subj->type) {
   case VGAUTH_SUBJECT_ANY:
      break;
   case VGAUTH_SUBJECT_NAMED:
      g_free(subj->val.name);
      break;
   }
}


/*
 ******************************************************************************
 * VGAuth_FreeAliasInfoContents --                                       */ /**
 *
 * @brief Frees any data inside a VGAuthAliasInfo structure.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  ai       The VGAuthAliasInfo to be clear.
 *
 ******************************************************************************
 */

void
VGAuth_FreeAliasInfoContents(VGAuthAliasInfo *ai)
{
   if (NULL == ai) {
      return;
   }

   VGAuthFreeSubjectContents(&(ai->subject));
   g_free(ai->comment);
}


/*
 ******************************************************************************
 * VGAuth_FreeAliasInfo --                                               */ /**
 *
 * @brief Frees a VGAuthAliasInfo structure.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  ai       The VGAuthAliasInfo to free.
 *
 ******************************************************************************
 */

void
VGAuth_FreeAliasInfo(VGAuthAliasInfo *ai)
{
   if (NULL == ai) {
      return;
   }

   VGAuth_FreeAliasInfoContents(ai);
   g_free(ai);
}


/*
 ******************************************************************************
 * VGAuth_CopyAliasInfo --                                               */ /**
 *
 * Copies a VGAuthAliasInfo structure.
 *
 * @param[in]  src       The src VGAuthAliasInfo.
 * @param[in]  dst       The dst VGAuthAliasInfo.
 *
 ******************************************************************************
 */

void
VGAuth_CopyAliasInfo(const VGAuthAliasInfo *src,
                     VGAuthAliasInfo *dst)
{
   if ((NULL == src) || (NULL == dst)) {
      ASSERT(0);
      return;
   }

   dst->subject.type = src->subject.type;
   dst->subject.val.name = g_strdup(src->subject.val.name);
   dst->comment = g_strdup(src->comment);
}


/*
 ******************************************************************************
 * VGAuth_FreeUserAliasList --                                           */ /**
 *
 * @brief Frees a list of VGAuthUserAlias structures.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  num      The length of the list.
 * @param[in]  uaList   The list to be freed.
 *
 ******************************************************************************
 */

void
VGAuth_FreeUserAliasList(int num,
                         VGAuthUserAlias *uaList)
{
   int i;
   int j;

   if (NULL == uaList) {
      return;
   }

   for (i = 0; i < num; i++) {
      for (j = 0; j < uaList[i].numInfos; j++) {
         VGAuth_FreeAliasInfoContents(&(uaList[i].infos[j]));
      }
      g_free(uaList[i].infos);
      g_free(uaList[i].pemCert);
   }
   g_free(uaList);
}


/*
 ******************************************************************************
 * VGAuth_FreeMappedAliasList --                                         */ /**
 *
 * @brief Frees a list of VGAuthMappedAlias structures.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  num      The length of the list.
 * @param[in]  maList   The list to be freed.
 *
 ******************************************************************************
 */

void
VGAuth_FreeMappedAliasList(int num,
                           VGAuthMappedAlias *maList)
{
   int i;
   int j;

   if (NULL == maList) {
      return;
   }

   for (i = 0; i < num; i++) {
      g_free(maList[i].pemCert);
      for (j = 0; j < maList[i].numSubjects; j++) {
         VGAuthFreeSubjectContents(&(maList[i].subjects[j]));
      }
      g_free(maList[i].subjects);
      g_free(maList[i].userName);
   }
   g_free(maList);
}


/*
 ******************************************************************************
 * VGAuthValidateSubject --                                              */ /**
 *
 * @brief Does a sanity check on a Subject paramter.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  subj       The VGAuthSubject to be checked.
 *
 ******************************************************************************
 */

static VGAuthError
VGAuthValidateSubject(VGAuthSubject *subj)
{
   if (NULL == subj) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if ((VGAUTH_SUBJECT_ANY != subj->type) &&
       (VGAUTH_SUBJECT_NAMED != subj->type)) {
      Warning("%s: invalid Subject type %d\n", __FUNCTION__, subj->type);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if ((VGAUTH_SUBJECT_NAMED == subj->type) &&
       (NULL == subj->val.name || !g_utf8_validate(subj->val.name, -1, NULL))) {
      Warning("%s: invalid Subject name\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_AddAlias --                                                    */ /**
 *
 * @brief Adds a certificate and AliasInfo to @a userName's alias store,
 * also adding a mapped link if requested.
 *
 * Any extraneous whitespace is removed from the beginning and end of the PEM
 * string before it is stored.
 *
 * If the pemCert already exists, the @a ai is added to any existing
 * @a AliasInfos.  If both exist, the operation is a no-op.
 *
 * @remark Must be called by superuser, or the owner of the alias store.
 *
 * @param[in]  ctx             The VGAuthContext.
 * @param[in]  userName        The name of the user with whom to associate the
 *                             certificate.
 * @param[in]  addMapping      If set, an entry in the mapping file is also
 *                             created.
 * @param[in]  pemCert         The certificate being added.
 * @param[in]  ai              The AliasInfo to be associated with this cert.
 * @param[in]  numExtraParams  The number of elements in extraParams.
 * @param[in]  extraParams     Any optional, additional paramaters to the
 *                             function. Currently none are supported, so
 *                             this must be NULL.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @retval VGAUTH_E_OK If called with the same arguments more than once
 *         (the operation is treated as a no-op).
 * @retval VGAUTH_E_SERVICE_NOT_RUNNING If the service cannot be contacted.
 * @retval VGAUTH_E_PERMISSION_DENIED If not called by superuser or the
 *         owner of the alias store.
 * @retval VGAUTH_E_NO_SUCH_USER If @a userName cannot be looked up.
 * @retval VGAUTH_E_MULTIPLE_MAPPINGS If @a addMapping is TRUE and the
 *         certificate and subject already exists in the mapping file
 *         associated with a different user.
 * @retval VGAUTH_E_INVALID_CERTIFICATE If @a cert is not a
 *         well-formed PEM x509 certificate.
 * @retval VGAUTH_ERROR_SET_SYSTEM_ERRNO If a syscall fails.  Use
 *         VGAUTH_ERROR_EXTRA_ERROR on the return value to get the errno.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_AddAlias(VGAuthContext *ctx,
                const char *userName,
                VGAuthBool addMapping,
                const char *pemCert,
                VGAuthAliasInfo *ai,
                int numExtraParams,
                const VGAuthExtraParams *extraParams)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == userName) || (NULL == pemCert) ||
       (NULL == ai) || (NULL == ai->comment)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(userName, -1, NULL)) {
      Warning("%s: invalid username\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!Usercheck_UsernameIsLegal(userName)) {
      Warning("%s: username contains illegal chars\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   /*
    * This is safe to do for Add only -- we need to handle the deleted-user
    * case for Remove and Query, but Add can't work since we can't
    * put proper security on the aliasStore file.
    */
   if (!UsercheckUserExists(userName)) {
      Warning("%s: username does not exist\n", __FUNCTION__);
      return VGAUTH_E_NO_SUCH_USER;
   }

   if (!g_utf8_validate(pemCert, -1, NULL)) {
      Warning("%s: invalid pemCert\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(ai->comment, -1, NULL)) {
      Warning("%s: invalid comment\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateSubject(&(ai->subject));
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuth_SendAddAliasRequest(ctx,
                                    userName,
                                    addMapping ? TRUE : FALSE,
                                    pemCert,
                                    ai);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_RemoveAlias --                                                 */ /**
 *
 * Removes @a subject from the alias store belonging
 * to @a userName.
 *
 * @remark Must be called by superuser, or the owner of the alias store.
 * @remark @a userName need not be valid in the OS; this allows removal of
 *         data belonging to a deleted user.
 *
 * @param[in]  ctx             The VGAuthContext.
 * @param[in]  userName        The name of the user from whose store to remove
 *                             the certificate.
 * @param[in]  pemCert         The certificate of the alias provider being
 *                             removed, in PEM format.
 * @param[in]  subject         The subject to be removed, in PEM format.
 * @param[in]  numExtraParams  The number of elements in extraParams.
 * @param[in]  extraParams     Any optional, additional paramaters to the
 *                             function. Currently none are supported, so
 *                             this must be NULL.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument or if the specified
 *         alias does not exist in the alias store.
 * @retval VGAUTH_E_SERVICE_NOT_RUNNING If the service cannot be contacted.
 * @retval VGAUTH_E_PERMISSION_DENIED If not called by superuser or the
 *         owner of the alias store.
 * @retval VGAUTH_ERROR_SET_SYSTEM_ERRNO If a syscall fails.  Use
 *         VGAUTH_ERROR_EXTRA_ERROR on the return value to get the errno.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_RemoveAlias(VGAuthContext *ctx,
                   const char *userName,
                   const char *pemCert,
                   VGAuthSubject *subject,
                   int numExtraParams,
                   const VGAuthExtraParams *extraParams)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == userName) || (NULL == pemCert) ||
       (NULL == subject)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(userName, -1, NULL)) {
      Warning("%s: invalid username\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!Usercheck_UsernameIsLegal(userName)) {
      Warning("%s: username contains illegal chars\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(pemCert, -1, NULL)) {
      Warning("%s: invalid PEM certificate\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateSubject(subject);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuth_SendRemoveAliasRequest(ctx,
                                       userName,
                                       pemCert,
                                       subject);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_RemoveAliasByCert --                                           */ /**
 *
 * Removes a cert and all associated subjects from the store belonging
 * to @a userName.
 *
 * @remark Must be called by superuser, or the owner of the alias store.
 * @remark @a userName need not be valid in the OS; this allows removal of
 *         data belonging to a deleted user.
 *
 * @param[in]  ctx             The VGAuthContext.
 * @param[in]  userName        The name of the user from whose store to remove
 *                             the certificate.
 * @param[in]  pemCert         The certificate to be removed, in PEM format.
 * @param[in]  numExtraParams  The number of elements in extraParams.
 * @param[in]  extraParams     Any optional, additional paramaters to the
 *                             function. Currently none are supported, so
 *                             this must be NULL.
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument or if @a pemCert
 *         does not exist in the alias store.
 * @retval VGAUTH_E_SERVICE_NOT_RUNNING If the service cannot be contacted.
 * @retval VGAUTH_E_PERMISSION_DENIED If not called by superuser or the
 *         owner of the alias store.
 * @retval VGAUTH_ERROR_SET_SYSTEM_ERRNO If a syscall fails.  Use
 *         VGAUTH_ERROR_EXTRA_ERROR on the return value to get the errno.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_RemoveAliasByCert(VGAuthContext *ctx,
                         const char *userName,
                         const char *pemCert,
                         int numExtraParams,
                         const VGAuthExtraParams *extraParams)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == userName) || (NULL == pemCert)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(userName, -1, NULL)) {
      Warning("%s: invalid username\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!Usercheck_UsernameIsLegal(userName)) {
      Warning("%s: username contains illegal chars\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(pemCert, -1, NULL)) {
      Warning("%s: invalid PEM certificate\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   /*
    * Re-use the wire message with just no Subject.
    */
   err = VGAuth_SendRemoveAliasRequest(ctx,
                                       userName,
                                       pemCert,
                                       NULL);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_QueryUserAliases --                                            */ /**
 *
 * @brief Returns the list of VGAuthUserAlias associated with the
 * alias store owned by @a userName.
 *
 * @remark Must be called by superuser, or the owner of the alias store.
 * @remark @a userName need not be valid in the OS; this allows queries of
 *         data belonging to a deleted user.
 *
 * @param[in]  ctx             The VGAuthContext.
 * @param[in]  userName        The name of the user whose alias store should
 *                             be queried.
 * @param[in]  numExtraParams  The number of elements in extraParams.
 * @param[in]  extraParams     Any optional, additional paramaters to the
 *                             function. Currently none are supported, so
 *                             this must be NULL.
 * @param[out] num             The number of aliases being returned.
 * @param[out] uaList          The VGAuthUserAlias being returned.
 *                             Free this list with VGAuth_FreeUserAliasList().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @retval VGAUTH_E_PERMISSION_DENIED If not called by superuser or the
 *         owner of the alias store.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_QueryUserAliases(VGAuthContext *ctx,
                        const char *userName,
                        int numExtraParams,
                        const VGAuthExtraParams *extraParams,
                        int *num,
                        VGAuthUserAlias **uaList)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == userName) || (NULL == num) ||
       (NULL == uaList)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!g_utf8_validate(userName, -1, NULL)) {
      Warning("%s: invalid username\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   if (!Usercheck_UsernameIsLegal(userName)) {
      Warning("%s: username contains illegal chars\n", __FUNCTION__);
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuth_SendQueryUserAliasesRequest(ctx,
                                            userName,
                                            num,
                                            uaList);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_QueryMappedAliases --                                          */ /**
 *
 * @brief Returns all the certificate/subject pairs and their associated user
 * from the mapping file.
 *
 * @remark Can be called by any user.
 *
 * @param[in]  ctx             The VGAuthContext.
 * @param[in]  numExtraParams  The number of elements in extraParams.
 * @param[in]  extraParams     Any optional, additional paramaters to the
 *                             function. Currently none are supported, so
 *                             this must be NULL.
 * @param[out] num             The number of aliases being returned.
 * @param[out] maList          The list of VGAuthMappedAlias.
 *                             Free this with VGAuth_FreeMappedAliasList().
 *
 * @retval VGAUTH_E_INVALID_ARGUMENT For a bad argument.
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_QueryMappedAliases(VGAuthContext *ctx,
                          int numExtraParams,
                          const VGAuthExtraParams *extraParams,
                          int *num,
                          VGAuthMappedAlias **maList)
{
   VGAuthError err;

   if ((NULL == ctx) || (NULL == num) || (NULL == maList)) {
      return VGAUTH_E_INVALID_ARGUMENT;
   }

   err = VGAuthValidateExtraParams(numExtraParams, extraParams);
   if (VGAUTH_E_OK != err) {
      return err;
   }

   err = VGAuth_SendQueryMappedAliasesRequest(ctx,
                                              num,
                                              maList);

   return err;
}
