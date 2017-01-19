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
 * @file VGAuthIdProvider.h
 *
 * Client library identity provider management API definitions.
 *
 * @addtogroup vgauth_id VGAuth Identity Management
 * @{
 *
 */
#ifndef _VGAUTHIDPROVIDER_H_
#define _VGAUTHIDPROVIDER_H_

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
 * entry in the IdProvider store is found using that Subject, then
 * if the special ANY subject exists in the identity store, that entry
 * will be used.
 */
typedef struct VGAuthSubject {
   VGAuthSubjectType type; /**< The subject type. */
   union {
      char *name;          /**< The subject name, if type VGAUTH_SUBJECT_NAMED */
   } val;
} VGAuthSubject;


/**
 * VGAuthSubjectInfo combines a subject and a comment.  Each subject can
 * have its own comment describing its use.
 */
typedef struct VGAuthSubjectInfo {
   VGAuthSubject subject; /**< The subject. */
   char *comment;      /**< User-supplied data to describe the subject. */
} VGAuthSubjectInfo;


/**
 * Describes all subjects that are associated with a certficate.
 */
typedef struct VGAuthIdProvider {
   char *pemCert;                   /**< The provider's certficate in PEM format.  */
   int numInfos;
   VGAuthSubjectInfo *infos;        /**< The SubjectInfos associated with the
                                      certificate. */
} VGAuthIdProvider;


/**
 * Describes an entry in the IdProvider mapping file.
 */
typedef struct VGAuthMappedIdentity {
   char *pemCert;          /**< The provider's certificate in PEM format. */
   int numSubjects;        /**< The number of Subjects associated with
                             the mapping file entry. */
   VGAuthSubject *subjects;  /**< The Subjects  associated with
                               the mapping file entry. */

   char *userName;        /**< The username associated with the  mapping file entry. */
} VGAuthMappedIdentity;


/*
 * Adds the given VGAuthSubjectInfo associated with the pemCert
 * to the identity store of @a userName.
 *
 * If addMapping is TRUE, also create an entry in the mapping file from
 * the cert and subject to @a userName.
 */
VGAuthError VGAuth_AddSubject(VGAuthContext *ctx,
                              const char *userName,
                              VGAuthBool addMapping,
                              const char *pemCert,
                              VGAuthSubjectInfo *si,
                              int numExtraParams,
                              const VGAuthExtraParams *extraParams);

/*
 * Removes the VGAuthSubjectInfo from the @a userName identity provider for @a
 * subject, also removing any associated mapping entry.
 */
VGAuthError VGAuth_RemoveSubject(VGAuthContext *ctx,
                                 const char *userName,
                                 const char *pemCert,
                                 VGAuthSubject *subject,
                                 int numExtraParams,
                                 const VGAuthExtraParams *extraParams);
/*
 * Removes all VGAuthIdentities with the matching PEM certificate from
 * the @a userName identity store for all subjects, also removing any associated
 * mapping entry.
 */
VGAuthError VGAuth_RemoveCert(VGAuthContext *ctx,
                              const char *userName,
                              const char *pemCert,
                              int numExtraParams,
                              const VGAuthExtraParams *extraParams);

/*
 * Lists all the VGAuthIdProvider from the identity store belonging
 * to @a userName.
 */
VGAuthError VGAuth_QueryIdProviders(VGAuthContext *ctx,
                                    const char *userName,
                                    int numExtraParams,
                                    const VGAuthExtraParams *extraParams,
                                    int *num,                     // OUT
                                    VGAuthIdProvider **idList);   // OUT

/*
 * Lists all the certificates/subject pairs in the mapping file and their
 * associated user.
 */
VGAuthError VGAuth_QueryMappedIdentities(VGAuthContext *ctx,
                                         int numExtraParams,
                                         const VGAuthExtraParams *extraParams,
                                         int *num,                       // OUT
                                         VGAuthMappedIdentity **miList); // OUT

/*
 * Data structure cleanup functions.
 */

/*
 * Frees an array of VGAuthIdProvider.
 */
void VGAuth_FreeIdProviderList(int num, VGAuthIdProvider *idList);

/*
 * Frees a VGAuthVGAuthSubjectInfo and contents.
 */
void VGAuth_FreeSubjectInfo(VGAuthSubjectInfo *si);

/*
 * Frees an array of VGAuthMappedIdentity.
 */
void VGAuth_FreeMappedIdentityList(int num, VGAuthMappedIdentity *miList);

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif   // _VGAUTHIDPROVIDER_H_

