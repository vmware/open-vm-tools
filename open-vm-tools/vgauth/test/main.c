/*********************************************************
 * Copyright (C) 2016-2017 VMware, Inc. All rights reserved.
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
 * @file main.c
 *
 *    The VGAuthService smoke test.
 *
 *    This does some very basic vgauth effort to verify that it
 *    properly validates SAML tokens.
 *
 *    This uses a built-in SAML token with a 1000 year lifetime,
 *    to avoid any issues with the XML security library on the signing
 *    side.
 *
 *    This test must be run as root on the same system as VGAuthService.
 *    This test should only be run in a test environment, since it
 *    will clear out any existing aliases.
 *
 *    Steps:
 *    - clear out any existing aliases
 *    - add an alias using the built-in cert
 *    - validate the SAML token
 *
 *    Possible reasons for failure:
 *    - VGAuthService wasn't started
 *    - VGAuthService failed to start up properly
 *       - unable to find support files (schemas)
 *       - unable to access various files/directories
 *       - parts of xmlsec1 missing (openssl crypto lib missing)
 *       - SAML verification failed to init (xmlsec1 build issues)
 *    - token fails to validate
 *       - this test was run after 12/18/3015
 *       - xmlsec1-config lies about how xmlsec1 was built
 *          some packages leave out -DXMLSEC_NO_SIZE_T,
 *          which can make some data structures a different size
 *          than in the library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <errno.h>
#endif
#include <glib.h>

#include "VGAuthBasicDefs.h"
#include "VGAuthAlias.h"
#include "VGAuthAuthentication.h"
#include "VGAuthCommon.h"
#include "VGAuthError.h"
#include "VGAuthLog.h"
#include "VGAuthUtil.h"

static gchar *appName;

#define ALIAS_USER_NAME    "root"
#define SUBJECT_NAME       "SmokeSubject"
#define COMMENT            "Smoke comment"

#define MAKE_PEM_FROM_BASE64(base64) \
   "-----BEGIN CERTIFICATE-----\n"   \
   base64                            \
   "-----END CERTIFICATE-----\n"
/*
 * Self-signed smoketest cert.
 *
 * Not Before: Aug 16 22:29:21 2016 GMT
 * Not After : Dec 18 22:29:21 3015 GMT
 *
 */
#define SMOKETEST_CERT_BASE64 \
"MIIDZTCCAk2gAwIBAgIJALuLD4JnajhkMA0GCSqGSIb3DQEBBQUAMEgxCzAJBgNV\n" \
"BAYTAlhYMRMwEQYDVQQIDApTbW9rZVN0YXRlMRIwEAYDVQQHDAlTbW9rZUNpdHkx\n" \
"EDAOBgNVBAoMB1Ntb2tlQ28wIBcNMTYwODE2MjIyOTIxWhgPMzAxNTEyMTgyMjI5\n" \
"MjFaMEgxCzAJBgNVBAYTAlhYMRMwEQYDVQQIDApTbW9rZVN0YXRlMRIwEAYDVQQH\n" \
"DAlTbW9rZUNpdHkxEDAOBgNVBAoMB1Ntb2tlQ28wggEiMA0GCSqGSIb3DQEBAQUA\n" \
"A4IBDwAwggEKAoIBAQDcRD+tNhOwxtEDDhnwQ94Qn+eEI4Nh6zXBP5CfnbMIHYo0\n" \
"1tzxLWOaJsN8/WoHy2cbeQkXGiGHpzuJIndhkL3XZpRdKTLIw95EVJkChYJi8ZUl\n" \
"LnaLIPsG1bpVOSuf+0qGcRyoItXBlvvYMZ5JAdUncHYnJ2NAbvqZVIH0sSafupzv\n" \
"w5txeQ7ufIcCzHYzSIFiX82CVMq/xuSQULVAZXoIfjNqMlwhYQn/EiSFb+y3kUa+\n" \
"xDzNWNyzv4H+7/6C+qz2KxTUbBEKT/lsuIVYVJ5R+1vZ2MnGkqsz8ELttXk0tAK+\n" \
"pfUAg7ugOhpF2rdvNOt4874Kkdj8a2It/JKqN3kBAgMBAAGjUDBOMB0GA1UdDgQW\n" \
"BBR9OuZuejgPVz64LWnhOfO1d6u0dTAfBgNVHSMEGDAWgBR9OuZuejgPVz64LWnh\n" \
"OfO1d6u0dTAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA4IBAQCZ91zS4zKZ\n" \
"uQv5rXn/zJtJ7d1pWnywh26n5bBlNQS3N7nAQPG5fvK2MB2rztE45Anq056YcYL7\n" \
"TTDDDPz9dGndThGyusHbzO/lV7UHCQUzMr0joItxrQoX7/4OPBMyARBLAE5wRa85\n" \
"uXm0D/Z6AAKJLz30yaQ+kBwTlIVhJFFhQv2zGZ3vB7CN0zNAZ/4s6lo+ejHj4Dhc\n" \
"PFsUDwWnqqp9iqZMX3vxp3BEuxUsSzXtuwBytvWcS/6i1LUl41obD4RNxZ3llQTN\n" \
"+uXVUFTTt0NgCbMJq5G8Nz4ziyjgxT94tB/AMwRmJzPSvew3vGMFhF7Fm0Z3Oxn5\n" \
"kWMiikdSCME8\n"


const char *smoketestPEMCert = MAKE_PEM_FROM_BASE64(SMOKETEST_CERT_BASE64);

const char *smoketestBase64Cert = SMOKETEST_CERT_BASE64;

/*
 * Token
 */

static const gchar *token =
"<saml:Assertion xmlns:saml=\"urn:oasis:names:tc:SAML:2.0:assertion\" ID=\"_b07b804c-7c29-ea16-7300-4f3d6f7928ac\" IssueInstant=\"2004-12-05T09:22:05Z\" Version=\"2.0\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
"<saml:Issuer>https://sso.eng.vmware.com</saml:Issuer>\n"
"<ds:Signature xmlns:ds=\"http://www.w3.org/2000/09/xmldsig#\">\n"
"<ds:SignedInfo>\n"
"<ds:CanonicalizationMethod Algorithm=\"http://www.w3.org/2001/10/xml-exc-c14n#\"/>\n"
"<ds:SignatureMethod Algorithm=\"http://www.w3.org/2001/04/xmldsig-more#rsa-sha256\"/>\n"
"<ds:Reference URI=\"#_b07b804c-7c29-ea16-7300-4f3d6f7928ac\">\n"
"<ds:Transforms>\n"
"<ds:Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"/>\n"
"</ds:Transforms>\n"
"<ds:DigestMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#sha256\"/>\n"
"<ds:DigestValue>w0kRFhuuzMenlkrfZttAweUTHcyRsQtHRn2L01Rmsa4=</ds:DigestValue>\n"
"</ds:Reference>\n"
"</ds:SignedInfo>\n"
"<ds:SignatureValue>B54Qp2fO+YyMPK/6gYzCDigLZdOO3vEu8getiKB4a8s14ySoH6aQtq/RjgNSW8jr\n"
"yNox9NRxc8ipBXC/noF8UBw6sUPpxsifMabWdMb9XvoZKufdDDrYKxQ4LwGjKF9y\n"
"i2dO/Saw8kZ8CQKYvbNt0KkMqbQZNtDtM6AVAobWXuZioYyphQSJ6YZVwJnLh6wv\n"
"sI0DgBqjFI91pID4n4N4SZq+tr2u8wcepnSIcmFNZ+BVdy7TKnjqTnjaDCG0Y0Uk\n"
"P5wtWOAVpqTGMmTDpVwAtKfs089tDw/doGds+FIAXd6oR2eECo9j7SOm0i0V9pEn\n"
"/nIe1Di7JNVJfl9V+g/bfA==</ds:SignatureValue>\n"
"<ds:KeyInfo>\n"
"<ds:X509Data>\n"
"<ds:X509Certificate>MIIDZTCCAk2gAwIBAgIJALuLD4JnajhkMA0GCSqGSIb3DQEBBQUAMEgxCzAJBgNV\n"
"BAYTAlhYMRMwEQYDVQQIDApTbW9rZVN0YXRlMRIwEAYDVQQHDAlTbW9rZUNpdHkx\n"
"EDAOBgNVBAoMB1Ntb2tlQ28wIBcNMTYwODE2MjIyOTIxWhgPMzAxNTEyMTgyMjI5\n"
"MjFaMEgxCzAJBgNVBAYTAlhYMRMwEQYDVQQIDApTbW9rZVN0YXRlMRIwEAYDVQQH\n"
"DAlTbW9rZUNpdHkxEDAOBgNVBAoMB1Ntb2tlQ28wggEiMA0GCSqGSIb3DQEBAQUA\n"
"A4IBDwAwggEKAoIBAQDcRD+tNhOwxtEDDhnwQ94Qn+eEI4Nh6zXBP5CfnbMIHYo0\n"
"1tzxLWOaJsN8/WoHy2cbeQkXGiGHpzuJIndhkL3XZpRdKTLIw95EVJkChYJi8ZUl\n"
"LnaLIPsG1bpVOSuf+0qGcRyoItXBlvvYMZ5JAdUncHYnJ2NAbvqZVIH0sSafupzv\n"
"w5txeQ7ufIcCzHYzSIFiX82CVMq/xuSQULVAZXoIfjNqMlwhYQn/EiSFb+y3kUa+\n"
"xDzNWNyzv4H+7/6C+qz2KxTUbBEKT/lsuIVYVJ5R+1vZ2MnGkqsz8ELttXk0tAK+\n"
"pfUAg7ugOhpF2rdvNOt4874Kkdj8a2It/JKqN3kBAgMBAAGjUDBOMB0GA1UdDgQW\n"
"BBR9OuZuejgPVz64LWnhOfO1d6u0dTAfBgNVHSMEGDAWgBR9OuZuejgPVz64LWnh\n"
"OfO1d6u0dTAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA4IBAQCZ91zS4zKZ\n"
"uQv5rXn/zJtJ7d1pWnywh26n5bBlNQS3N7nAQPG5fvK2MB2rztE45Anq056YcYL7\n"
"TTDDDPz9dGndThGyusHbzO/lV7UHCQUzMr0joItxrQoX7/4OPBMyARBLAE5wRa85\n"
"uXm0D/Z6AAKJLz30yaQ+kBwTlIVhJFFhQv2zGZ3vB7CN0zNAZ/4s6lo+ejHj4Dhc\n"
"PFsUDwWnqqp9iqZMX3vxp3BEuxUsSzXtuwBytvWcS/6i1LUl41obD4RNxZ3llQTN\n"
"+uXVUFTTt0NgCbMJq5G8Nz4ziyjgxT94tB/AMwRmJzPSvew3vGMFhF7Fm0Z3Oxn5\n"
"kWMiikdSCME8\n"
"</ds:X509Certificate>\n"
"</ds:X509Data>\n"
"</ds:KeyInfo>\n"
"</ds:Signature><saml:Subject>\n"
"<saml:NameID Format=\"urn:oasis:names:tc:SAML:2.0:nameid-format:transient\">SmokeSubject</saml:NameID>\n"
"<saml:SubjectConfirmation Method=\"urn:oasis:names:tc:SAML:2.0:cm:bearer\">\n"
"<saml:SubjectConfirmationData NotOnOrAfter=\"2116-07-23T23:29:34.677406Z\"/>\n"
"</saml:SubjectConfirmation>\n"
"</saml:Subject>\n"
"<saml:Conditions NotBefore=\"2016-08-16T23:29:34.677402Z\" NotOnOrAfter=\"2116-07-23T23:29:34.677229Z\">\n"
"<saml:AudienceRestriction>\n"
"<saml:Audience>https://sp.example.com/SAML2</saml:Audience></saml:AudienceRestriction>\n"
"</saml:Conditions>\n"
"<saml:AuthnStatement AuthnInstant=\"2004-12-05T09:22:00Z\" SessionIndex=\"b07b804c-7c29-ea16-7300-4f3d6f7928ac\">\n"
"<saml:AuthnContext>\n"
"<saml:AuthnContextClassRef>urn:oasis:names:tc:SAML:2.0:ac:classes:PasswordProtectedTransport</saml:AuthnContextClassRef>\n"
"</saml:AuthnContext>\n"
"</saml:AuthnStatement>\n"
"<saml:AttributeStatement>\n"
"<saml:Attribute FriendlyName=\"eduPersonAffiliation\" Name=\"urn:oid:1.3.6.1.4.1.5923.1.1.1.1\" NameFormat=\"urn:oasis:names:tc:SAML:2.0:attrname-format:uri\" xmlns:x500=\"urn:oasis:names:tc:SAML:2.0:profiles:attribute:X500\" x500:Encoding=\"LDAP\">\n"
"<saml:AttributeValue xsi:type=\"xs:string\">member</saml:AttributeValue>\n"
"<saml:AttributeValue xsi:type=\"xs:string\">staff</saml:AttributeValue>\n"
"</saml:Attribute>\n"
"</saml:AttributeStatement>\n"
"</saml:Assertion>\n"
;


/*
 ******************************************************************************
 * Usage --                                                              */ /**
 *
 * Usage message for smoke test
 *
 ******************************************************************************
 */

static void
Usage(void)
{
   fprintf(stderr, "Usage: %s\n", appName);
   exit(-1);
}


/*
 ******************************************************************************
 * Log --                                                                */ /**
 *
 * Error message logging function.
 *
 * @param[in]     logDomain   The glib logging domain, which is set by the
 *                            various glib components and vgauth itself.
 * @param[in]     logLevel    The severity of the message.
 * @param[in]     msg         The error message.
 * @param[in]     userData    Any userData specified in the call to
 *                            VGAuth_SetLogHandler() (unused)
 *
 ******************************************************************************
 */

static void
Log(const char *logDomain,
    int logLevel,
    const char *msg,
    void *userData)
{
   g_printerr("%s[%d]: %s", logDomain, logLevel, msg);
}


/*
 ******************************************************************************
 * CleanAliases --                                                       */ /**
 *
 * Clears out the alias store for the given user, and the map file.
 *
 * @param[in]  ctx        VGAuth context
 * @param[in]  userName   The user whose alias store is to be cleaned.
 *
 * @return A SAMl token on success, NULL failure.
 *
 ******************************************************************************
 */

static VGAuthError
CleanAliases(VGAuthContext *ctx,
             const gchar *userName)
{
   VGAuthError err;
   int num;
   int i;
   VGAuthMappedAlias *maList;
   VGAuthUserAlias *uaList;

   // clear out mapped aliaes
   err = VGAuth_QueryMappedAliases(ctx, 0, NULL, &num, &maList);
   if (err != VGAUTH_E_OK) {
      g_printerr("VGAuth_QueryMappedAliases() failed "VGAUTHERR_FMT64"\n",
                 err);
      return err;
   }

   for (i = 0; i < num; i++) {
      err = VGAuth_RemoveAliasByCert(ctx, maList[i].userName,
                                     maList[i].pemCert, 0, NULL);
      if (err != VGAUTH_E_OK) {
         g_printerr("VGAuth_RemoveAliasByCert() failed "VGAUTHERR_FMT64"\n",
                    err);
         return err;
      }
   }
   VGAuth_FreeMappedAliasList(num, maList);
   err = VGAuth_QueryMappedAliases(ctx, 0, NULL, &num, &maList);
   if (err != VGAUTH_E_OK || num != 0) {
         g_printerr("sitll have mapped aliases or "
                    "VGAuth_QueryMappedAliases() failed "VGAUTHERR_FMT64"\n",
                    err);
         return err;
   }

   // clear out user aliases
   err = VGAuth_QueryUserAliases(ctx, userName, 0, NULL,
                                 &num, &uaList);
   if (err != VGAUTH_E_OK) {
      g_printerr("VGAuth_QueryUserAliases() failed "VGAUTHERR_FMT64"\n",
                 err);
      return err;
   }
   for (i = 0; i < num; i++) {
      err = VGAuth_RemoveAliasByCert(ctx, userName,
                                     uaList[i].pemCert, 0, NULL);
      if (err != VGAUTH_E_OK) {
         g_printerr("VGAuth_RemoveAliasByCert() failed "VGAUTHERR_FMT64"\n",
                    err);
         return err;
      }
   }

   err = VGAuth_QueryUserAliases(ctx, userName, 0, NULL,
                                 &num, &uaList);
   if (err != VGAUTH_E_OK || num != 0) {
      g_printerr("aliases left or VGAuth_QueryUserAliases() failed "
                 VGAUTHERR_FMT64"\n",
                 err);
      return err;
   }

   return err;
}


/*
 ******************************************************************************
 * AddAlias --                                                           */ /**
 *
 * Generates a SAMl token with a given Subject.
 *
 * @param[in]  ctx        VGAuth context
 * @param[in]  cert       The certificate for the alias.
 * @param[in]  user       The user whose alias store is to be updated.
 * @param[in]  subject    The Subject for the alias.
 * @param[in]  comment    The Comment for the alias.
 *
 * @return A SAMl token on success, NULL failure.
 *
 ******************************************************************************
 */

static VGAuthError
AddAlias(VGAuthContext *ctx,
         const gchar *cert,
         const gchar *user,
         const gchar *subject,
         const gchar *comment)
{
   VGAuthError err;
   VGAuthAliasInfo ai;

   ai.subject.type = VGAUTH_SUBJECT_NAMED;
   ai.subject.val.name = (gchar *)subject;
   ai.comment = (gchar *)comment;
   err = VGAuth_AddAlias(ctx, user, FALSE, cert, &ai, 0, NULL);
   if (err != VGAUTH_E_OK) {
      g_printerr("VGAuth_AddAlias() failed "VGAUTHERR_FMT64"\n",
                 err);
      return err;
   }

   return err;
}


/*
 ******************************************************************************
 * ValidateToken --                                                      */ /**
 *
 * Validates a SAMl token.
 *
 * @param[in]  ctx        VGAuth context
 * @param[in]  userName   The user associated with the token.
 * @param[in]  token      A SAML token.
 *
 * @return VGAUTH_E_OK on success, an error on failure.
 *
 ******************************************************************************
 */

static VGAuthError
ValidateToken(VGAuthContext *ctx,
              const gchar *userName,
              const gchar *token)
{
   VGAuthError err;
   VGAuthExtraParams extraParams[1];
   VGAuthUserHandle *userHandle = NULL;
   VGAuthAliasInfo *retAi = NULL;
   char *retSamlSubject = NULL;
   char *retUserName = NULL;

   /*
    * Use info-only -- its all we need.
    */
   extraParams[0].name = VGAUTH_PARAM_VALIDATE_INFO_ONLY;
   extraParams[0].value = VGAUTH_PARAM_VALUE_TRUE;
   err = VGAuth_ValidateSamlBearerToken(ctx,
                                        token,
                                        userName,
                                        1,
                                        extraParams,
                                        &userHandle);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to validate token");
      goto done;
   }

   err = VGAuth_UserHandleUsername(ctx, userHandle, &retUserName);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to get username off handle");
      goto done;
   }
   err = VGAuth_UserHandleSamlData(ctx,
                                   userHandle,
                                   &retSamlSubject,
                                   &retAi);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to get SAML subject data off handle");
      goto done;
   }
   printf("Token details: user: %s (expected %s) subject: %s (expected %s)\n",
          retUserName, ALIAS_USER_NAME, retSamlSubject, SUBJECT_NAME);

done:
   VGAuth_UserHandleFree(userHandle);
   g_free(retUserName);
   g_free(retSamlSubject);
   VGAuth_FreeAliasInfo(retAi);
   return err;
}


/*
 ******************************************************************************
 * main --                                                               */ /**
 *
 * Main entry point.
 *
 * @param[in]  argc        Number of command line arguments.
 * @param[in]  argv        The command line arguments.
 *
 * @return 0 if the operation ran successfully, -1 if there was an error during
 *         execution.
 *
 ******************************************************************************
 */

int
main(int argc,
     char *argv[])
{
   VGAuthError err;
   VGAuthContext *ctx;

   appName = g_path_get_basename(argv[0]);
   if (argc != 1) {
      Usage();
   }

   VGAuth_SetLogHandler(Log, NULL, 0, NULL);

   err = VGAuth_Init(appName, 0, NULL, &ctx);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to init VGAuth");
      return -1;
   }

   // make sure we start with a clean slate
   err = CleanAliases(ctx, ALIAS_USER_NAME);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to clean alias store");
      return -1;
   }

   err = AddAlias(ctx, smoketestPEMCert,
                  ALIAS_USER_NAME, SUBJECT_NAME, COMMENT);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to add alias");
      return -1;
   }

   err = ValidateToken(ctx, ALIAS_USER_NAME, token);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to validate SAML token");
      return -1;
   }

   printf("PASSED!\n");

   // make sure we end with a clean slate
   err = CleanAliases(ctx, ALIAS_USER_NAME);
   if (VGAUTH_E_OK != err) {
      g_printerr("Failed to clean alias store");
      return -1;
   }

   VGAuth_Shutdown(ctx);
   g_free(appName);
   return 0;
}
