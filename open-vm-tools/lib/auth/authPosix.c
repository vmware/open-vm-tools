/*********************************************************
 * Copyright (C) 2003-2019 VMware, Inc. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // for access, crypt, etc.
#if !defined USE_PAM && !defined __APPLE__
#include <shadow.h>
#endif

#include "vmware.h"
#include "vm_product.h"
#include "codeset.h"
#include "posix.h"
#include "auth.h"
#include "str.h"
#include "log.h"

#ifdef USE_PAM
#   include "file.h"
#   include "config.h"
#   include "localconfig.h"
#   include <security/pam_appl.h>
#   include <dlfcn.h>
#endif

#if defined(HAVE_CONFIG_H) || defined(sun)
#  include <crypt.h>
#endif

#define LOGLEVEL_MODULE auth
#include "loglevel_user.h"

typedef struct {
   struct passwd  pwd;      /* must be first member */
   size_t         bufSize;
   uint8          buf[];
} AuthTokenInternal;

#ifdef USE_PAM
#if defined(sun)
#define CURRENT_PAM_LIBRARY	"libpam.so.1"
#elif defined(__FreeBSD__)
#define CURRENT_PAM_LIBRARY	"libpam.so"
#elif defined(__APPLE__)
#define CURRENT_PAM_LIBRARY	"libpam.dylib"
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
 *----------------------------------------------------------------------
 *
 * AuthLoadPAM --
 *
 *      Attempt to load and initialize PAM library.
 *
 * Results:
 *      FALSE if load and/or initialization failed.
 *      TRUE  if initialization succeeded.
 *
 * Side effects:
 *      libpam loaded.  We never unload - some libpam modules use
 *      syslog() function, and glibc does not survive when arguments
 *      specified to openlog() are freeed from memory.
 *
 *----------------------------------------------------------------------
 */

static Bool
AuthLoadPAM(void)
{
   void *pam_library;
   int i;

   if (authPamLibraryHandle) {
      return TRUE;
   }
   pam_library = Posix_Dlopen(CURRENT_PAM_LIBRARY, RTLD_LAZY | RTLD_GLOBAL);
   if (!pam_library) {
#if defined(VMX86_TOOLS)
      /*
       * XXX do we even try to configure the pam libraries?
       * potential nightmare on all the possible guest OSes
       */

      Log("System PAM libraries are unusable: %s\n", dlerror());

      return FALSE;
#else
      char *liblocation;
      char *libdir;

      libdir = LocalConfig_GetPathName(DEFAULT_LIBDIRECTORY, CONFIG_VMWAREDIR);
      if (!libdir) {
         Log("System PAM library unusable and bundled one not found.\n");

         return FALSE;
      }
      liblocation = Str_SafeAsprintf(NULL, "%s/lib/%s/%s", libdir,
                                     CURRENT_PAM_LIBRARY, CURRENT_PAM_LIBRARY);
      free(libdir);

      pam_library = Posix_Dlopen(liblocation, RTLD_LAZY | RTLD_GLOBAL);
      if (!pam_library) {
         Log("Neither system nor bundled (%s) PAM libraries usable: %s\n",
             liblocation, dlerror());
         free(liblocation);

         return FALSE;
      }
      free(liblocation);
#endif
   }
   for (i = 0; i < ARRAYSIZE(authPAMImported); i++) {
      void *symbol = dlsym(pam_library, authPAMImported[i].procname);

      if (!symbol) {
         Log("PAM library does not contain required function: %s\n",
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


static const char *PAM_username;
static const char *PAM_password;

#if defined(sun)
static int PAM_conv (int num_msg,                     // IN:
		     struct pam_message **msg,        // IN:
		     struct pam_response **resp,      // OUT:
		     void *appdata_ptr)               // IN:
#else
static int PAM_conv (int num_msg,                     // IN:
		     const struct pam_message **msg,  // IN:
		     struct pam_response **resp,      // OUT:
		     void *appdata_ptr)               // IN:
#endif
{
   int count;
   struct pam_response *reply = calloc(num_msg, sizeof *reply);

   if (!reply) {
      return PAM_CONV_ERR;
   }

   for (count = 0; count < num_msg; count++) {
      switch (msg[count]->msg_style) {
      case PAM_PROMPT_ECHO_ON:
         reply[count].resp_retcode = PAM_SUCCESS;
         reply[count].resp = PAM_username ? strdup(PAM_username) : NULL;
         /* PAM frees resp */
         break;
      case PAM_PROMPT_ECHO_OFF:
         reply[count].resp_retcode = PAM_SUCCESS;
         reply[count].resp = PAM_password ? strdup(PAM_password) : NULL;
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
 *----------------------------------------------------------------------
 *
 * AuthAllocateToken --
 *
 *      Allocates an AuthTokenInternal structure, plus helper buffer
 *      large enough for the Posix_Get*_r calls.
 *
 * Side effects:
 *      None.
 *
 * Results:
 *      An AuthTokenInternal pointer. Free with Auth_CloseToken.
 *
 *----------------------------------------------------------------------
 */

static AuthTokenInternal *
AuthAllocateToken(void)
{
   long bufSize;
   AuthTokenInternal *ati;

   /*
    * We need to get the maximum size buffer needed by getpwuid_r from
    * sysconf. Multiply by 4 to compensate for the conversion to UTF-8
    * by the Posix_Get*_r() wrappers.
    */

   errno = 0;
   bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
   if ((errno != 0) || (bufSize <= 0)) {
      bufSize = 16 * 1024;  // Unlimited; pick something reasonable
   }

   bufSize *= 4;

   ati = Util_SafeMalloc(sizeof *ati + (size_t) bufSize);
   ati->bufSize = bufSize;

   return ati;
}


/*
 *----------------------------------------------------------------------
 *
 * Auth_GetPwnam --
 *
 *      Wrapper aroung Posix_Getpwnam_r.
 *
 * Side effects:
 *      None.
 *
 * Results:
 *      An AuthToken. Free with Auth_CloseToken.
 *
 *----------------------------------------------------------------------
 */

AuthToken
Auth_GetPwnam(const char *user)  // IN
{
   AuthTokenInternal *ati;
   int res;
   struct passwd *ppwd;

   ASSERT(user);

   ati = AuthAllocateToken();
   res = Posix_Getpwnam_r(user, &ati->pwd, ati->buf, ati->bufSize, &ppwd);

   if ((0 != res) || (ppwd == NULL)) {
      Auth_CloseToken((AuthToken) ati);
      return NULL;
   }

   ASSERT(ppwd == &ati->pwd);

   return (AuthToken) ati;
}


/*
 *----------------------------------------------------------------------
 *
 * Auth_AuthenticateSelf --
 *
 *      Authenticate as the current user.
 *
 * Side effects:
 *      None.
 *
 * Results:
 *      An AuthToken. Free with Auth_CloseToken.
 *
 *----------------------------------------------------------------------
 */

AuthToken
Auth_AuthenticateSelf(void)  // IN
{
   AuthTokenInternal *ati;
   int res;
   struct passwd *ppwd;

   ati = AuthAllocateToken();
   res = Posix_Getpwuid_r(getuid(), &ati->pwd, ati->buf, ati->bufSize, &ppwd);

   if ((0 != res) || (ppwd == NULL)) {
      Auth_CloseToken((AuthToken) ati);
      return NULL;
   }

   ASSERT(ppwd == &ati->pwd);

   return (AuthToken) ati;
}


/*
 *----------------------------------------------------------------------
 *
 * Auth_AuthenticateUserPAM --
 *
 *      Accept username/password, and service and verfiy it with PAM
 *
 * Side effects:
 *      None.
 *
 * Results:
 *
 *      The vmauthToken for the authenticated user, or NULL if
 *      authentication failed.
 *
 *----------------------------------------------------------------------
 */

AuthToken
Auth_AuthenticateUserPAM(const char *user,     // IN:
                         const char *pass,     // IN:
                         const char *service)  // IN:
{
#ifndef USE_PAM
   return NULL;
#else
   pam_handle_t *pamh;
   int pam_error;

   Bool success = FALSE;
   AuthTokenInternal *ati = NULL;

   ASSERT(service);

   if (!CodeSet_Validate(user, strlen(user), "UTF-8")) {
      Log("User not in UTF-8\n");
      goto exit;
   }
   if (!CodeSet_Validate(pass, strlen(pass), "UTF-8")) {
      Log("Password not in UTF-8\n");
      goto exit;
   }

   if (!AuthLoadPAM()) {
      goto exit;
   }

   /*
    * XXX PAM can blow away our syslog level settings so we need
    * to call Log_InitEx() again before doing any more Log()s
    */

#define PAM_BAIL if (pam_error != PAM_SUCCESS) { \
                  Log_Error("%s:%d: PAM failure - %s (%d)\n", \
                            __FUNCTION__, __LINE__, \
                            dlpam_strerror(pamh, pam_error), pam_error); \
                  dlpam_end(pamh, pam_error); \
                  goto exit; \
                 }
   PAM_username = user;
   PAM_password = pass;


   pam_error = dlpam_start(service, PAM_username, &PAM_conversation,
                           &pamh);

   if (pam_error != PAM_SUCCESS) {
      Log("Failed to start PAM (error = %d).\n", pam_error);
      goto exit;
   }

   pam_error = dlpam_authenticate(pamh, 0);
   PAM_BAIL;
   pam_error = dlpam_acct_mgmt(pamh, 0);
   PAM_BAIL;
   pam_error = dlpam_setcred(pamh, PAM_ESTABLISH_CRED);
   PAM_BAIL;
   dlpam_end(pamh, PAM_SUCCESS);

#undef PAM_BAIL

   /* If this point is reached, the user has been authenticated. */
   ati = (AuthTokenInternal *) Auth_GetPwnam(user);
   success = TRUE;

exit:
   if (success) {
      return (AuthToken) ati;
   } else {
      Auth_CloseToken((AuthToken) ati);
      return NULL;
   }

#endif // USE_PAM
}


/*
 *----------------------------------------------------------------------
 *
 * Auth_AuthenticateUser --
 *
 *      Accept username/password And verfiy it
 *
 * Side effects:
 *      None.
 *
 * Results:
 *
 *      The vmauthToken for the authenticated user, or NULL if
 *      authentication failed.
 *
 *----------------------------------------------------------------------
 */

AuthToken
Auth_AuthenticateUser(const char *user,  // IN:
                      const char *pass)  // IN:
{

#ifdef USE_PAM

#if defined(VMX86_TOOLS)
   return Auth_AuthenticateUserPAM(user, pass, "vmtoolsd");
#else
   return Auth_AuthenticateUserPAM(user, pass, "vmware-authd");
#endif

#else /* !USE_PAM */
   Bool success = FALSE;
   AuthTokenInternal *ati = NULL;

   if (!CodeSet_Validate(user, strlen(user), "UTF-8")) {
      Log("User not in UTF-8\n");
      goto exit;
   }
   if (!CodeSet_Validate(pass, strlen(pass), "UTF-8")) {
      Log("Password not in UTF-8\n");
      goto exit;
   }

   /* All of the following issues are dealt with in the PAM configuration
      file, so put all authentication/priviledge checks before the
      corresponding #endif below. */

   ati = (AuthTokenInternal *) Auth_GetPwnam(user);

   if (ati == NULL) {
      goto exit;
   }

   if (*ati->pwd.pw_passwd != '\0') {
      const char *pw = ati->pwd.pw_passwd;
      const char *namep;

#if !defined __APPLE__
      // support shadow passwords:
      if (strcmp(pw, "x") == 0) {
         struct spwd *sp = getspnam(user);

         if (sp) {
            pw = sp->sp_pwdp;
         }
      }
#endif

      namep = crypt(pass, pw);
      if (namep == NULL || strcmp(namep, pw) != 0) {
         // Incorrect password
         goto exit;
      }

      // Clear out crypt()'s internal state, too.
      crypt("glurp", pw);
   }

   success = TRUE;

exit:
   if (success) {
      return (AuthToken) ati;
   } else {
      Auth_CloseToken((AuthToken) ati);
      return NULL;
   }
#endif /* !USE_PAM */
}


/*
 *----------------------------------------------------------------------
 *
 * Auth_CloseToken --
 *
 *      Free the token allocated in Auth_AuthenticateUser.
 *
 * Side effects:
 *      None
 *
 * Results:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
Auth_CloseToken(AuthToken token)  // IN (OPT):
{
   free((void *) token);
}
