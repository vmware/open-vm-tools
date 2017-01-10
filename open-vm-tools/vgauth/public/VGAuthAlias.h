/*********************************************************
 * Copyright (C) 2012-2016 VMware, Inc. All rights reserved.
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
 * @file VGAuthAlias.h
 *
 * Client library alias management API definitions.
 *
 * @addtogroup vgauth_alias VGAuth Alias Management
 * @{
 *
 */
#ifndef _VGAUTHALIAS_H_
#define _VGAUTHALIAS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "VGAuthCommon.h"

/**
 * The types of subjects.  Any is a special case.
 */
typedef enum {
   /** The Subject field in a SAML token must match in the token verification process. */
   VGAUTH_SUBJECT_NAMED,

   /** Any Subject field in a SAML token can be matched in the token verification process. */
   VGAUTH_SUBJECT_ANY,
} VGAuthSubjectType;


/**
 * VGAuthSubject is either ANY or NAMED, in which case it contains
 * the Subject name.
 *
 * When authenticating a SAML token, the Subject is used to determine
 * what guest user the SAML token can be authenticated as.  If no
 * entry in the Alias store is found using that Subject, then
 * if the special ANY subject exists in the alias store, that entry
 * will be used.
 */
typedef struct VGAuthSubject {
   VGAuthSubjectType type; /**< The subject type. */
   union {
      char *name;          /**< The subject name, if type VGAUTH_SUBJECT_NAMED */
   } val;
} VGAuthSubject;


/**
 * VGAuthAliasInfo combines a subject and a comment.  Each alias can
 * have its own comment describing its use.
 */
typedef struct VGAuthAliasInfo {
   VGAuthSubject subject; /**< The subject. */
   char *comment;         /**< User-supplied data to describe the subject. */
} VGAuthAliasInfo;


/**
 * Describes all subjects that are associated with a certficate.
 */
typedef struct VGAuthUserAlias {
   char *pemCert;                   /**< The certficate in PEM format.  */
   int numInfos;
   VGAuthAliasInfo *infos;          /**< The AliasInfos associated with the
                                      certificate. */
} VGAuthUserAlias;


/**
 * Describes an entry in the Alias mapping file.
 */
typedef struct VGAuthMappedAlias {
   char *pemCert;          /**< The certificate in PEM format. */
   int numSubjects;        /**< The number of Subjects associated with
                             the mapping file entry. */
   VGAuthSubject *subjects;  /**< The Subjects  associated with
                               the mapping file entry. */

   char *userName;        /**< The username associated with the  mapping file entry. */
} VGAuthMappedAlias;


/*
 * Adds the given VGAuthAliasInfo associated with the pemCert
 * to the alias store of @a userName.
 *
 * If addMapping is TRUE, also create an entry in the mapping file from
 * the cert and subject to @a userName.
 */
VGAuthError VGAuth_AddAlias(VGAuthContext *ctx,
                            const char *userName,
                            VGAuthBool addMapping,
                            const char *pemCert,
                            VGAuthAliasInfo *si,
                            int numExtraParams,
                            const VGAuthExtraParams *extraParams);

/*
 * Removes the VGAuthAliasInfo from the @a userName alias store for @a
 * subject, also removing any associated mapping entry.
 */
VGAuthError VGAuth_RemoveAlias(VGAuthContext *ctx,
                               const char *userName,
                               const char *pemCert,
                               VGAuthSubject *subject,
                               int numExtraParams,
                               const VGAuthExtraParams *extraParams);
/*
 * Removes all VGAuthUserAliases with the matching PEM certificate from
 * the @a userName alias store for all subjects, also removing any associated
 * mapping entry.
 */
VGAuthError VGAuth_RemoveAliasByCert(VGAuthContext *ctx,
                                     const char *userName,
                                     const char *pemCert,
                                     int numExtraParams,
                                     const VGAuthExtraParams *extraParams);

/*
 * Lists all the VGAuthUserAlias from the alias store belonging
 * to @a userName.
 */
VGAuthError VGAuth_QueryUserAliases(VGAuthContext *ctx,
                                    const char *userName,
                                    int numExtraParams,
                                    const VGAuthExtraParams *extraParams,
                                    int *num,                     // OUT
                                    VGAuthUserAlias **uaList);    // OUT

/*
 * Lists all the certificates/subject pairs in the mapping file and their
 * associated user.
 */
VGAuthError VGAuth_QueryMappedAliases(VGAuthContext *ctx,
                                      int numExtraParams,
                                      const VGAuthExtraParams *extraParams,
                                      int *num,                       // OUT
                                      VGAuthMappedAlias **maList);    // OUT

/*
 * Data structure cleanup functions.
 */

/*
 * Frees an array of VGAuthAlias.
 */
void VGAuth_FreeUserAliasList(int num, VGAuthUserAlias *uaList);

/*
 * Frees a VGAuthAliasInfo and contents.
 */
void VGAuth_FreeAliasInfo(VGAuthAliasInfo *ai);

/*
 * Frees an array of VGAuthMappedAlias.
 */
void VGAuth_FreeMappedAliasList(int num, VGAuthMappedAlias *maList);

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif   // _VGAUTHALIAS_H_

