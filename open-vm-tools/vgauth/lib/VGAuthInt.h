/*********************************************************
 * Copyright (C) 2011-2017 VMware, Inc. All rights reserved.
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
 * @file VGAuthInt.h
 *
 * Private functions and data types for client library.
 */

#ifndef _VGAUTHINT_H_
#define _VGAUTHINT_H_

#include "VGAuthBasicDefs.h"
#include "VGAuthCommon.h"
#include "VGAuthAuthentication.h"
#include "VGAuthAlias.h"
#include "audit.h"
#include "prefs.h"

#define VMW_TEXT_DOMAIN "VGAuthLib"
#include "i18n.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>

/*
 * Use this for any informational messages, eg "VGAuth initialized".
 */
#define Log   g_message

/*
 * Use this for any error reporting, such as unexpected failures from APIs
 * or bad input to VGAuth APIs.
 */
#define Warning   g_warning

/*
 * Use this for any debugging messages.
 */
#define Debug  g_debug


/*
 * Set this to be horribly inefficient but to be sure that nothing
 * is assuming it will get a full packet as sent by a single syscall
 * on the other end.
 */
#define  NETWORK_FORCE_TINY_PACKETS 0


/*
 * State of the client/service communication channel
 */
typedef struct VCAGComm {
   gboolean connected;
   unsigned int sequenceNumber;
   gchar *userName;           // the user we're runing as, used for
                              // setting up the comm pipe permissions
#ifdef UNITTEST
   gboolean fileTest;
   gboolean bufTest;

   FILE *testFp;

   char testBuffer[10240];
   gsize bufLen;
   gsize bufLoc;
#endif

#ifdef _WIN32
   HANDLE hPipe;
#else
   int sock;
#endif
   char *pipeName;
} VGAuthComm;

struct VGAuthUserHandle;

struct VGAuthContext {
   /*
    * Needed for pam(3) initialization.
    */
   char *applicationName;

   int numExtraParams;
   VGAuthExtraParams *extraParams;

#ifdef _WIN32
   /*
    * Used for authentication using SSPI, to track the SSPI challenge
    * and response handshakes that are in progress.
    */
   GHashTable *sspiHandshakes;
#endif

   /*
    *
    * Connection data for keystore service, etc
    */
   VGAuthComm comm;

   /*
    * Impersonation state.
    */
   gboolean isImpersonating;

   /*
    * Impersonated user.
    */
   VGAuthUserHandle *impersonatedUser;

   /*
    * XXX optimization -- keep a comm channel alive for superuser?
    *
    * An app that just does validation would probably just be connected
    * as root all the time anyways.  But it could be useful for something
    * that did both certstore work and validation.
    */

};


typedef enum {
   VGAUTH_HANDLE_FLAG_NONE             = 0x0,
   /* handle cannot be impersonated */
   VGAUTH_HANDLE_FLAG_CAN_IMPERSONATE  = 0x1,
   /* handle cannot be used by CreateTicket */
   VGAUTH_HANDLE_FLAG_CAN_CREATE_TICKET = 0x2,

   /* normal handle */
   VGAUTH_HANDLE_FLAG_NORMAL            =  (VGAUTH_HANDLE_FLAG_CAN_IMPERSONATE |
                                           VGAUTH_HANDLE_FLAG_CAN_CREATE_TICKET),
} VGAuthHandleFlag;


typedef struct AuthDetails {
   VGAuthUserHandleType type;
   union {
      struct {
         char *subject;
         VGAuthAliasInfo aliasInfo;
      } samlData;
   } val;
} AuthDetails;

struct VGAuthUserHandle {
   char *userName;
   VGAuthHandleFlag   flags;
   AuthDetails details;
#ifdef _WIN32
   HANDLE token;
   HANDLE hProfile;
#else
   uid_t uid;
#endif
   int refCount;
};


extern PrefHandle gPrefs;


void VGAuth_AuditEvent(VGAuthContext *ctx,
                       gboolean isSuccess,
                       const char *fmt, ...) PRINTF_DECL(3, 4);

gboolean VGAuth_IsRunningAsRoot(void);
gchar *VGAuth_GetCurrentUsername(void);


VGAuthError VGAuth_ConnectToServiceAsUser(VGAuthContext *ctx,
                                          const char *userName);
VGAuthError VGAuth_ConnectToServiceAsCurrentUser(VGAuthContext *ctx);
gboolean VGAuth_IsConnectedToServiceAsUser(VGAuthContext *ctx,
                                           const char *userName);
gboolean VGAuth_IsConnectedToServiceAsAnyUser(VGAuthContext *ctx);

VGAuthError VGAuth_InitConnection(VGAuthContext *ctx);
VGAuthError VGAuth_CloseConnection(VGAuthContext *ctx);

VGAuthError VGAuth_CommSendData(VGAuthContext *ctx,
                                gchar *request);

VGAuthError VGAuth_CommReadData(VGAuthContext *ctx,
                                gsize *len,
                                gchar **response);

VGAuthError VGAuth_SendConnectRequest(VGAuthContext *ctx);

VGAuthError VGAuth_SendSessionRequest(VGAuthContext *ctx,
                                      const char *userName,
                                      char **pipeName);           // OUT

VGAuthError VGAuth_SendCreateTicketRequest(VGAuthContext *ctx,
                                           VGAuthUserHandle *handle,
                                           char **ticket);               // OUT
VGAuthError VGAuth_SendValidateTicketRequest(VGAuthContext *ctx,
                                             const char *ticket,
                                             VGAuthUserHandle **handle); // OUT
VGAuthError VGAuth_SendRevokeTicketRequest(VGAuthContext *ctx,
                                           const char *ticket);

VGAuthError VGAuth_SendAddAliasRequest(VGAuthContext *ctx,
                                         const char *userName,
                                         gboolean addMappedLink,
                                         const char *pemCert,
                                         VGAuthAliasInfo *si);

VGAuthError VGAuth_SendRemoveAliasRequest(VGAuthContext *ctx,
                                          const char *userName,
                                          const char *pemCert,
                                          VGAuthSubject *subj);

VGAuthError VGAuth_SendQueryUserAliasesRequest(VGAuthContext *ctx,
                                               const char *userName,
                                               int *num,             // OUT
                                               VGAuthUserAlias **uaList);// OUT

VGAuthError VGAuth_SendQueryMappedAliasesRequest(VGAuthContext *ctx,
                                                 int *num,            // OUT
                                                 VGAuthMappedAlias **maList); // OUT

VGAuthError VGAuth_SendValidateSamlBearerTokenRequest(VGAuthContext *ctx,
                                                      gboolean validateOnly,
                                                      const char *samlToken,
                                                      const char *userName,
                                                      VGAuthUserHandle **userHandle);

VGAuthError VGAuth_CreateHandleForUsername(VGAuthContext *ctx,
                                           const char *userName,
                                           VGAuthUserHandleType type,
                                           HANDLE token,
                                           VGAuthUserHandle **handle);   // OUT

VGAuthError VGAuth_SetUserHandleSamlInfo(VGAuthContext *ctx,
                                         VGAuthUserHandle *handle,
                                         const char *samlSubject,
                                         VGAuthAliasInfo *si);

VGAuthError VGAuthImpersonateImpl(VGAuthContext *ctx,
                                  VGAuthUserHandle *handle,
                                  gboolean loadUserProfile);

VGAuthError VGAuthEndImpersonationImpl(VGAuthContext *ctx);

VGAuthError VGAuth_NetworkConnect(VGAuthContext *ctx);

gboolean VGAuth_NetworkValidatePublicPipeOwner(VGAuthContext *ctx);

VGAuthError VGAuth_NetworkWriteBytes(VGAuthContext *ctx,
                                     gsize len,
                                     gchar *buffer);

VGAuthError VGAuth_NetworkReadBytes(VGAuthContext *ctx,
                                    gsize *len,
                                    gchar **buffer);


VGAuthError VGAuthValidateUsernamePasswordImpl(VGAuthContext *ctx,
                                               const char *userName,
                                               const char *password,
                                               VGAuthUserHandle **handle);

#ifdef UNITTEST
VGAuthError VGAuthComm_SetTestBufferInput(VGAuthContext *ctx,
                                          const char *buffer);

VGAuthError VGAuthComm_SetTestFileInput(VGAuthContext *ctx,
                                        const char *filename);

void VGAuth_UnitTestReplies(VGAuthContext *ctx);
#endif

#ifdef _WIN32
VGAuthError VGAuth_MakeToken(VGAuthContext *ctx, const char *userName,
                             VGAuthUserHandleType type,
                             VGAuthUserHandle **handle);
#endif

VGAuthError VGAuthInitAuthentication(VGAuthContext *ctx);
VGAuthError VGAuthInitAuthenticationPlatform(VGAuthContext *ctx);

void VGAuthShutdownAuthentication(VGAuthContext *ctx);
void VGAuthShutdownAuthenticationPlatform(VGAuthContext *ctx);

VGAuthError VGAuthGenerateSSPIChallengeImpl(VGAuthContext *ctx,
                                            size_t sspiRequestLen,
                                            const unsigned char *sspiRequest,
                                            unsigned int *id,
                                            size_t *challengeLen,
                                            unsigned char **challenge);

VGAuthError VGAuthValdiateSSPIResponseImpl(VGAuthContext *ctx,
                                           unsigned int id,
                                           size_t responseLen,
                                           const unsigned char *response,
                                           VGAuthUserHandle **userHandle);

#define VGAuthValidateExtraParams(numEP, ep)      \
   VGAuthValidateExtraParamsImpl(__FUNCTION__, (numEP), ep)

VGAuthError VGAuthValidateExtraParamsImpl(const char *funcName,
                                       int numExtraParams,
                                       const VGAuthExtraParams *params);

#define VGAuthGetBoolExtraParam(numEP, ep, name, defValue, value)      \
   VGAuthGetBoolExtraParamImpl(__FUNCTION__, (numEP), ep,              \
                               name, defValue, (value))

VGAuthError VGAuthGetBoolExtraParamImpl(const char *funcName,
                                        int numExtraParams,
                                        const VGAuthExtraParams *params,
                                        const char *paramName,
                                        gboolean defValue,
                                        gboolean *paramValue);

void VGAuth_FreeAliasInfoContents(VGAuthAliasInfo *si);
void VGAuth_CopyAliasInfo(const VGAuthAliasInfo *src,
                          VGAuthAliasInfo *dst);

#endif   // _VGAUTHINT_H_
