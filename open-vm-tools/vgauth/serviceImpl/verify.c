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
 * @file verify.c
 *
 * Code to handle certficate verification.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "serviceInt.h"
#include "certverify.h"
#include "vmxlog.h"

/*
 ******************************************************************************
 * ServiceInitVerify --                                                 */ /**
 *
 * Inits the verification code.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceInitVerify(void)
{

   CertVerify_Init();
   return SAML_Init();
}


/*
 ******************************************************************************
 * ServiceVerifyAndCheckTrustCertChainForSubject --                      */ /**
 *
 * @brief Checks the alias store for a username and subject to be sure certs are
 * trusted, and then verifies a certificate chain.
 *
 * Validates a certificate chain.  Verifies that all certs are properly
 * signed, in the proper date range, etc.  It is assumed that the first
 * element in the chain is the leaf cert being validated, with the rest
 * of the chain being certs that support that validation.  If userName
 * is not set, it uses the mapping file and the root cert of the chain
 * to find the user, and userNameOut is filled in with that user.
 *
 * #param[in]     numCerts      The number of certs in the chain.
 * @param[in]     pemCertChain  The chain of certificates to verify.
 * @param[in]     userName      The owner of the alias store to use to fetch any
 *                              extra certificates required for verification.
 * @param[in]     subj          The SAML subject to match.
 * @param[out]    userNameOut   If userName was NULL, this returns the
 *                              user from the mapping file associated with
 *                              the certs, otherwise points to a
 *                              heap-allocated copy of userName.
 * @param[out]    verifyAi      The ServiceAliasInfo associated with the
 *                              alias that was matched for @subj.
 *                              Should be freed with ServiceAliasFreeAliasInfo().
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceVerifyAndCheckTrustCertChainForSubject(int numCerts,
                                              const char **pemCertChain,
                                              const char *userName,
                                              ServiceSubject *subj,
                                              char **userNameOut,
                                              ServiceAliasInfo **verifyAi)
{
   VGAuthError err;
   int numMapped = 0;
   ServiceMappedAlias *maList = NULL;
   int numStoreCerts = 0;
   ServiceAlias *aList = NULL;
   int matchIdIdx = -1;
   int matchSiIdx = -1;
   ServiceAliasInfo *ai;
   char **trustedCerts = NULL;
   int numTrusted = 0;
   char **untrustedCerts = NULL;
   int numUntrusted = 0;
   char *queryUserName = NULL;
   char *leafCert = NULL;
   gboolean foundTrusted;
   int i;
   int j;
   int k;

   *userNameOut = NULL;
   *verifyAi = NULL;

   ASSERT(subj);
   ASSERT(numCerts > 0);

   /*
    * Dump the token cert chain for debugging purposes.
    */
   if (gVerboseLogging) {
      gchar *chainx509;

      for (i = 0; i < numCerts; i++) {
         chainx509 = CertVerify_CertToX509String(pemCertChain[i]);
         Debug("%s: Token chain cert #%d:\n%s", __FUNCTION__, i, chainx509);
         g_free(chainx509);
      }
   }

   /*
    * If we have no userName, look through the mapping file for a match
    * from the cert chain.
    */
   if (NULL == userName || *userName == '\0') {
      err = ServiceAliasQueryMappedAliases(&numMapped, &maList);

      if (VGAUTH_E_OK != err) {
         goto done;
      }
      if (0 == numMapped) {
         /*
          * No username, no mapped certs, no chance.
          */
         Warning("%s: no mapping entries or specified userName\n",
                 __FUNCTION__);
         VMXLog_Log(VMXLOG_LEVEL_WARNING,
                    "%s: no mapping entries or specified userName\n",
                    __FUNCTION__);
         err = VGAUTH_E_AUTHENTICATION_DENIED;
         goto done;
      }

      /*
       * Search for a match in the mapped store.
       */
      for (i = 0; i < numCerts; i++) {
         for (j = 0; j < numMapped; j++) {
            if (ServiceComparePEMCerts(pemCertChain[i], maList[j].pemCert)) {
               /*
                * Make sure we don't have multiple matches with different users.
                * Two possible scenarios that can trigger this:
                * - the mapping file could be inconsistent
                * - the chain coming in could have more than one cert that
                *   exists in the mapping file, belonging to different users
                */
               if ((NULL != queryUserName) &&
                   g_strcmp0(queryUserName, maList[j].userName) != 0) {
                  Warning("%s: found more than one user in map file chain\n",
                          __FUNCTION__);
                  VMXLog_Log(VMXLOG_LEVEL_WARNING,
                             "%s: found more than one user in map file chain\n",
                          __FUNCTION__);
                  err = VGAUTH_E_MULTIPLE_MAPPINGS;
                  goto done;
               }

               for (k = 0; k < maList[j].num; k++) {
                  if ((maList[j].subjects[k].type == SUBJECT_TYPE_ANY) ||
                      ServiceAliasIsSubjectEqual(subj->type,
                                                 maList[j].subjects[k].type,
                                                 subj->name,
                                                 maList[j].subjects[k].name)) {
                     queryUserName = g_strdup(maList[j].userName);
                     break;
                  }
               }

            }
         }
      }
      /*
       * Subject went unmatched, so fail.
       */
      if (NULL == queryUserName) {
         Warning("%s: no matching cert and subject found in mapping file\n",
                 __FUNCTION__);
         VMXLog_Log(VMXLOG_LEVEL_WARNING,
                    "%s: no matching cert and subject found in mapping file\n",
                    __FUNCTION__);
         err = VGAUTH_E_AUTHENTICATION_DENIED;
         goto done;
      }
   } else {
      queryUserName = g_strdup(userName);
   }

   /*
    * Make sure the user exists -- Query supports deleted users
    * to allow for cleanup.
    */
   if (!UsercheckUserExists(queryUserName)) {
      Warning("%s: User '%s' doesn't exist\n", __FUNCTION__, queryUserName);
      VMXLog_Log(VMXLOG_LEVEL_WARNING,
                 "%s: User doesn't exist\n", __FUNCTION__);
      err = VGAUTH_E_AUTHENTICATION_DENIED;
      goto done;
   }

   err = ServiceAliasQueryAliases(queryUserName, &numStoreCerts, &aList);
   if (VGAUTH_E_OK != err) {
      goto done;
   }

   /*
    * Dump the store cert chain for debugging purposes.
    */
   if (gVerboseLogging) {
      gchar *storex509;

      Debug("%s: %d certs in store for user %s\n",  __FUNCTION__,
            numStoreCerts, queryUserName);
      for (i = 0; i < numStoreCerts; i++) {
         storex509 = CertVerify_CertToX509String(aList[i].pemCert);
         Debug("%s: Store chain cert #%d:\n%s", __FUNCTION__, i, storex509);
         g_free(storex509);
      }
   }


   /*
    * Split the incoming chain into trusted and untrusted certs
    */
   for (i = 0; i < numCerts; i++) {
      int foundAnyIdx;
      int foundSubjectIdx;

      foundTrusted = FALSE;
      for (j = 0; j < numStoreCerts; j++) {
         if (ServiceComparePEMCerts(pemCertChain[i], aList[j].pemCert)) {
            /*
             * Remember the root cert, so we can return its AliasInfo
             * if all checks out.
             */
            matchIdIdx = j;
            foundAnyIdx = -1;
            foundSubjectIdx = -1;

            for (k = 0; k < aList[j].num; k++) {
               if (aList[j].infos[k].type == SUBJECT_TYPE_ANY) {
                  foundAnyIdx = k;
               } else if (ServiceAliasIsSubjectEqual(subj->type,
                                                     aList[j].infos[k].type,
                                                     subj->name,
                                                     aList[j].infos[k].name)) {
                  foundSubjectIdx = k;
               }
            }
            if ((foundSubjectIdx >= 0) || (foundAnyIdx >= 0)) {
               numTrusted++;
               trustedCerts = g_realloc_n(trustedCerts,
                                          numTrusted, sizeof(*trustedCerts));
               trustedCerts[numTrusted - 1] = g_strdup(pemCertChain[i]);
               foundTrusted = TRUE;
               /*
                * Remember the matching ai, so we can return its comment
                * if all checks out.  Note that a specific subject match takes
                * precendence over an ANY match.
                */
               matchSiIdx = (foundSubjectIdx >= 0) ?
                  foundSubjectIdx : foundAnyIdx;
            }
         }
      }
      if (!foundTrusted) {
         numUntrusted++;
         untrustedCerts = g_realloc_n(untrustedCerts,
                                      numUntrusted, sizeof(*untrustedCerts));
         untrustedCerts[numUntrusted - 1] = g_strdup(pemCertChain[i]);
      }
   }

   /*
    * Make sure we have at least one trusted cert.
    */
   if (numTrusted == 0) {
      err = VGAUTH_E_AUTHENTICATION_DENIED;
      Warning("%s: No trusted certs in chain\n", __FUNCTION__);
      VMXLog_Log(VMXLOG_LEVEL_WARNING,
                 "%s: No trusted certs in chain\n", __FUNCTION__);
      goto done;
   }

   /*
    * Pull out the leaf -- it should be either the first trusted
    * or untrusted cert
    */
   if (g_strcmp0(pemCertChain[0], trustedCerts[0]) == 0) {
      numTrusted--;
      leafCert = trustedCerts[0];
      memmove(trustedCerts, &(trustedCerts[1]), sizeof(*trustedCerts) * numTrusted);
   } else if (g_strcmp0(pemCertChain[0], untrustedCerts[0]) == 0) {
      numUntrusted--;
      leafCert = untrustedCerts[0];
      memmove(untrustedCerts, &(untrustedCerts[1]), sizeof(*untrustedCerts) * numUntrusted);
   } else {
      ASSERT(0);
   }

   err = CertVerify_CertChain(leafCert,
                              numUntrusted,
                              (const char **) untrustedCerts,
                              numTrusted,
                              (const char **) trustedCerts);
   if (VGAUTH_E_OK != err) {
      VMXLog_Log(VMXLOG_LEVEL_WARNING,
                 "%s: cert chain validation failed\n", __FUNCTION__);
      goto done;
   }

   Debug("%s: cert chain successfully validated", __FUNCTION__);

   /*
    * Save off AliasInfo.
    *
    * XXX unclear on what should be done here if we have multiple
    * trusted certs in the alias store.  For now, use the root-most
    * (last found).
    */
   ai = g_malloc0(sizeof(ServiceAliasInfo));
   ASSERT(matchIdIdx >= 0 && matchSiIdx >= 0);
   ai->type = aList[matchIdIdx].infos[matchSiIdx].type;
   ai->name = g_strdup(aList[matchIdIdx].infos[matchSiIdx].name);
   ai->comment = g_strdup(aList[matchIdIdx].infos[matchSiIdx].comment);
   *verifyAi = ai;
   *userNameOut = queryUserName;
   queryUserName = NULL;

done:
   ServiceAliasFreeMappedAliasList(numMapped, maList);

   ServiceAliasFreeAliasList(numStoreCerts, aList);

   for (i = 0; i < numTrusted; i++) {
      g_free(trustedCerts[i]);
   }
   g_free(trustedCerts);

   for (i = 0; i < numUntrusted; i++) {
      g_free(untrustedCerts[i]);
   }
   g_free(untrustedCerts);

   g_free(leafCert);
   g_free(queryUserName);

   return err;
}
