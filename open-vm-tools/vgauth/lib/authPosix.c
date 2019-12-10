/*********************************************************
 * Copyright (C) 2011-2017,2019 VMware, Inc. All rights reserved.
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
 * @file authPosix.h
 *
 * Posix user authentication.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for access, crypt, etc.

/*
 * USE_PAM should be defined in the makefile, since it impacts
 * what system libraries (-ld, -lcrypt) will be needed.
 *
 * XXX non-PAM code doesn't enforce a delay after failure to
 * slow down a brute-force attack.
 */
#ifdef USE_PAM
#  include <security/pam_appl.h>
#  include <dlfcn.h>
#else
# ifdef __APPLE__
#  error - Apple not supported.
# else
#  include <shadow.h>
# endif
#endif

#include <pwd.h>

#if defined(sun)
#  include <crypt.h>
#endif

#include "VGAuthInt.h"


#ifdef USE_PAM
#if defined(sun)
#define CURRENT_PAM_LIBRARY	"libpam.so.1"
#else
#define CURRENT_PAM_LIBRARY	"libpam.so.0"
#endif

static typeof(&pam_start) dlpam_start;
static typeof(&pam_end) dlpam_end;
static typeof(&pam_authenticate) dlpam_authenticate;
static typeof(&pam_setcred) dlpam_setcred;
static typeof(&pam_acct_mgmt) dlpam_acct_mgmt;
static typeof(&pam_strerror) dlpam_strerror;
#if 0  /* These three functions are not used yet */
static typeof(&pam_open_session) dlpam_open_session;
static typeof(&pam_close_session) dlpam_close_session;
static typeof(&pam_chauthtok) dlpam_chauthtok;
#endif

static struct {
   void       **procaddr;
   const char  *procname;
} authPAMImported[] = {
#define IMPORT_SYMBOL(x) { (void **)&dl##x, #x }
   IMPORT_SYMBOL(pam_start),
   IMPORT_SYMBOL(pam_end),
   IMPORT_SYMBOL(pam_authenticate),
   IMPORT_SYMBOL(pam_setcred),
   IMPORT_SYMBOL(pam_acct_mgmt),
   IMPORT_SYMBOL(pam_strerror),
#undef IMPORT_SYMBOL
};

static void *authPamLibraryHandle = NULL;


/*
 ******************************************************************************
 * AuthLoadPAM --                                                        */ /**
 *
 * Loads and initializes PAM library.
 *
 * @return TRUE on success
 *
 ******************************************************************************
 */

static gboolean
AuthLoadPAM(void)
{
   void *pam_library;
   int i;

   if (authPamLibraryHandle) {
      return TRUE;
   }
   pam_library = dlopen(CURRENT_PAM_LIBRARY, RTLD_NOW | RTLD_GLOBAL);
   if (!pam_library) {
      /*
       * XXX do we even try to configure the pam libraries?
       * potential nightmare on all the possible guest OSes
       */

      Warning("System PAM libraries are unusable: %s\n", dlerror());

      return FALSE;
   }
   for (i = 0; i < sizeof(authPAMImported)/sizeof(*authPAMImported); i++) {
      void *symbol = dlsym(pam_library, authPAMImported[i].procname);

      if (!symbol) {
         Warning("PAM library does not contain required function: %s\n",
                 dlerror());
         dlclose(pam_library);
         return FALSE;
      }

      *(authPAMImported[i].procaddr) = symbol;
   }

   authPamLibraryHandle = pam_library;
   Log("PAM up and running.\n");

   return TRUE;
}

/*
 * Holds the username & password for the PAM_conversation callbacks.
 */
typedef struct PamData {
   const char *username;
   const char *password;
} PamData;


/*
 ******************************************************************************
 * PAM_conv --                                                           */ /**
 *
 * PAM conversation function.  This is passed to pam_start
 * and is used by pam to provide communication between the
 * application and loaded modules.  See pam_conv(3) for details.
 *
 * @param[in]  num_msg      Number of messages to process.
 * @param[in]  msg          The messages.
 * @param[out] resp         Responses to the messages.
 * @param[in]  appdata_ptr  Application data (username/password).
 *
 * @return PAM_SUCCESS on success, the appropriate PAM error on failure.
 *
 ******************************************************************************
 */

#if defined(sun)
static int
PAM_conv(int num_msg,
         struct pam_message **msg,
         struct pam_response **resp,
         void *appdata_ptr)
#else
static int
PAM_conv(int num_msg,
         const struct pam_message **msg,
         struct pam_response **resp,
         void *appdata_ptr)
#endif
{
   PamData *pd = (PamData *) appdata_ptr;
   int count;
   struct pam_response *reply = calloc(num_msg, sizeof(struct pam_response));

   if (!reply) {
      return PAM_CONV_ERR;
   }

   for (count = 0; count < num_msg; count++) {
      switch (msg[count]->msg_style) {
      case PAM_PROMPT_ECHO_ON:
         reply[count].resp_retcode = PAM_SUCCESS;
         reply[count].resp = pd->username ? strdup(pd->username) : NULL;
         /* PAM frees resp */
         break;
      case PAM_PROMPT_ECHO_OFF:
         reply[count].resp_retcode = PAM_SUCCESS;
         reply[count].resp = pd->password ? strdup(pd->password) : NULL;
         /* PAM frees resp */
         break;
      case PAM_TEXT_INFO:
         reply[count].resp_retcode = PAM_SUCCESS;
         reply[count].resp = NULL;
         /* ignore it... */
         break;
      case PAM_ERROR_MSG:
         reply[count].resp_retcode = PAM_SUCCESS;
         reply[count].resp = NULL;
         /* Must be an error of some sort... */
         break;
      default:
         while (--count >= 0) {
            free(reply[count].resp);
         }
         free(reply);

         return PAM_CONV_ERR;
      }
   }

   *resp = reply;

   return PAM_SUCCESS;
}

static struct pam_conv PAM_conversation = {
    &PAM_conv,
    NULL
};
#endif /* USE_PAM */


/*
 ******************************************************************************
 * VGAuthValidateUsernamePasswordImpl --                                 */ /**
 *
 * Validates a username/password.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  userName   The username to be validated.
 * @param[in]  password   The password to be validated.
 * @param[out] handle     The resulting handle representing the user
 *                        associated with the username.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuthValidateUsernamePasswordImpl(VGAuthContext *ctx,
                                   const char *userName,
                                   const char *password,
                                   VGAuthUserHandle **handle)
{
#ifdef USE_PAM
   pam_handle_t *pamh;
   int pam_error;
   PamData pd;
   VGAuthError err;
#else
   struct passwd *pwd;
#endif

#ifdef USE_PAM
   if (!AuthLoadPAM()) {
      return VGAUTH_E_FAIL;
   }

   pd.username = userName;
   pd.password = password;
   PAM_conversation.appdata_ptr = &pd;
   pam_error = dlpam_start(ctx->applicationName,
                           userName,
                           &PAM_conversation, &pamh);
   if (pam_error != PAM_SUCCESS) {
      Warning("Failed to start PAM (error: %d).\n", pam_error);
      return VGAUTH_E_FAIL;
   }

   pam_error = dlpam_authenticate(pamh, 0);
   if (pam_error == PAM_SUCCESS) {
      pam_error = dlpam_acct_mgmt(pamh, 0);
      if (pam_error == PAM_SUCCESS) {
         pam_error = dlpam_setcred(pamh, PAM_ESTABLISH_CRED);
      }
   }
   dlpam_end(pamh, pam_error);
   if (pam_error != PAM_SUCCESS) {
      switch (pam_error) {
         /*
          * Most PAM errors get mapped to VGAUTH_E_AUTHENTICATION_DENIED,
          * but some are mapped into VGAUTH_E_FAIL.
          */
         case PAM_OPEN_ERR:
         case PAM_SYMBOL_ERR:
         case PAM_SERVICE_ERR:
         case PAM_SYSTEM_ERR:
         case PAM_BUF_ERR:
         case PAM_NO_MODULE_DATA:
         case PAM_CONV_ERR:
         case PAM_ABORT:
#ifndef sun   /* The following error codes are undefined on Solaris. */
         case PAM_BAD_ITEM:
         case PAM_CONV_AGAIN:
         case PAM_INCOMPLETE:
#endif
            err = VGAUTH_E_FAIL;
            break;

         default:
            err = VGAUTH_E_AUTHENTICATION_DENIED;
            break;

      }
      Warning("PAM error: %s (%d), mapped to VGAuth error "VGAUTHERR_FMT64"\n",
              dlpam_strerror(pamh, pam_error), pam_error, err);
      return err;
   }

#else /* !USE_PAM */

   setpwent(); //XXX can kill?
   pwd = getpwnam(userName);
   endpwent(); //XXX can kill?

   if (!pwd) {
      // No such user
      return VGAUTH_E_AUTHENTICATION_DENIED;
   }

   if (*pwd->pw_passwd != '\0') {
      const char *passwd = pwd->pw_passwd;
      const char *crypt_pw;

      // looks like a shadow password, so use it instead
      if (strcmp(passwd, "x") == 0) {
         struct spwd *sp = getspnam(userName);
         if (sp) {
            passwd = sp->sp_pwdp;
         }
      }

      crypt_pw = crypt(password, passwd);
      if (!crypt_pw || (strcmp(crypt_pw, passwd) != 0)) {
         // Incorrect password
         return VGAUTH_E_AUTHENTICATION_DENIED;
      }

      // Clear out crypt()'s internal state, too.
      crypt("glurp", passwd);
   }
#endif /* !USE_PAM */

   return VGAuth_CreateHandleForUsername(ctx, userName,
                                         VGAUTH_AUTH_TYPE_NAMEPASSWORD,
                                         NULL, handle);
}


/*
 ******************************************************************************
 * VGAuthInitAuthenticationPlatform --                                   */ /**
 *
 * Initializes any POSIX-specific authentication resources.
 *
 * @param[in]  ctx        The VGAuthContext to initialize.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuthInitAuthenticationPlatform(VGAuthContext *ctx)
{
   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuthShutdownAuthenticationPlatform --                                   */ /**
 *
 * Cleans up any POSIX-specific authentication resources.
 *
 * @param[in]  ctx        The VGAuthContext to shutdown.
 *
 ******************************************************************************
 */

void
VGAuthShutdownAuthenticationPlatform(VGAuthContext *ctx)
{
}


/*
 ******************************************************************************
 * VGAuthGenerateSSPIChallengeImpl --                                    */ /**
 *
 * Not supported.
 *
 * @param[in]  ctx            The VGAuthContext.
 * @param[in]  requestLen     The size of the request in bytes.
 * @param[in]  request        The SSPI request.
 * @param[out] id             An identifier to use when validating the response.
 * @param[out] challengeLen   The length of the challenge in bytes.
 * @param[out] challenge      The SSPI challenge to send to the client.
 *
 * @return VGAUTH_E_UNSUPPORTED
 *
 ******************************************************************************
 */

VGAuthError
VGAuthGenerateSSPIChallengeImpl(VGAuthContext *ctx,
                                size_t requestLen,
                                const unsigned char *request,
                                unsigned int *id,
                                size_t *challengeLen,
                                unsigned char **challenge)
{
   return VGAUTH_E_UNSUPPORTED;
}


/*
 ******************************************************************************
 * VGAuthValdiateSSPIResponseImpl --                                     */ /**
 *
 * Not supported.
 *
 * @param[in]  ctx           The VGAuthContext.
 * @param[in]  id            Used to identify which SSPI challenge
 *                           this response correspends to.
 * @param[in]  responseLen   The length of the response in bytes.
 * @param[in]  response      The SSPI response.
 * @param[out] userHandle    The resulting handle representing the user
 *                           associated with the username.
 *
 * @return VGAUTH_E_UNSUPPORTED
 *
 ******************************************************************************
 */

VGAuthError
VGAuthValdiateSSPIResponseImpl(VGAuthContext *ctx,
                               unsigned int id,
                               size_t responseLen,
                               const unsigned char *response,
                               VGAuthUserHandle **userHandle)
{
   return VGAUTH_E_UNSUPPORTED;
}
