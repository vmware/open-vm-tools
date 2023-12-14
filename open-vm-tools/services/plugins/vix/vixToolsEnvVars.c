/*********************************************************
 * Copyright (c) 2010-2018, 2021 VMware, Inc. All rights reserved.
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
 * vixToolsEnvVars.c --
 *
 *      Routines that encapsulate the complexity of dealing with
 *      environment variables when the process may be impersonating
 *      a user.
 */

#include <stdlib.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

#include "util.h"
#include "unicode.h"
#include "dynbuf.h"
#include "str.h"
#include "posix.h"
#include "vixToolsInt.h"


#ifndef _WIN32
extern char **environ;
#endif

struct VixToolsEnvIterator {
#ifdef _WIN32
   enum {
      VIX_TOOLS_ENV_TYPE_ENV_BLOCK = 1,
      VIX_TOOLS_ENV_TYPE_ENVIRON,
   } envType;
   union {
      /* Used when envType is VIX_TOOLS_ENV_TYPE_ENV_BLOCK. */
      struct {
         wchar_t *envBlock;     // Keep the original around to free.
         wchar_t *currEnvVar;
      } eb;
      /* Used when envType is VIX_TOOLS_ENV_TYPE_ENVIRON. */
      wchar_t **env;
   } data;
#else
   char **environ;
#endif
};


struct VixToolsUserEnvironment {
#ifdef _WIN32
   Bool impersonated;
   wchar_t *envBlock;      // Only used when impersonated == TRUE.
#else
   // The POSIX versions don't need any state currently.
#endif
};



/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsNewEnvIterator --
 *
 *      Create a new environment variable iterator for the user
 *      represented by 'userToken'.
 *      The resulting VixToolsEnvIterator must be freed using
 *      VixToolsDestroyEnvIterator.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsNewEnvIterator(void *userToken,                  // IN
#ifdef __FreeBSD__
                       char **envp,                      // IN
#endif
                       VixToolsEnvIterator **envItr)     // OUT
{
   VixError err = VIX_OK;
   VixToolsEnvIterator *it = Util_SafeMalloc(sizeof *it);

   if (NULL == envItr) {
      err = VIX_E_FAIL;
      goto quit;
   }

   *envItr = NULL;

#ifdef _WIN32
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      /*
       * The process is impersonating a user, so retrieve the user's
       * environment block instead of using the process's environment.
       */
      it->envType = VIX_TOOLS_ENV_TYPE_ENV_BLOCK;
      err = VixToolsGetEnvBlock(userToken, &it->data.eb.envBlock);
      if (VIX_FAILED(err)) {
         goto quit;
      }
      it->data.eb.currEnvVar = it->data.eb.envBlock;
   } else {
      /*
       * The action is being performed as the user running the process
       * so the process's environment is fine.
       * TODO: Is this totally equivilent to the behavior when impersonated?
       * Would fetching the environment block include changes to the user's
       * or system's environment made after the process is running?
       */
      it->envType = VIX_TOOLS_ENV_TYPE_ENVIRON;
      it->data.env = _wenviron;
   }
#elif defined(__APPLE__)
   it->environ = *_NSGetEnviron();
#elif defined(__FreeBSD__)
   it->environ = envp;
#else
   it->environ = environ;
#endif
   *envItr = it;
quit:
   if (VIX_FAILED(err)) {
      free(it);
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetNextEnvVar --
 *
 *      Get the next envariable variable pair in the form NAME=VALUE.
 *
 * Results:
 *      A heap-allocated UTF-8 string, or NULL when the iterator has
 *      reached the end.
 *
 * Side effects:
 *      Advances the iterator.
 *
 *-----------------------------------------------------------------------------
 */

char *
VixToolsGetNextEnvVar(VixToolsEnvIterator *envItr)    // IN
{
   char *envVar;

   if (NULL == envItr) {
      return NULL;
   }

#ifdef _WIN32
   if (VIX_TOOLS_ENV_TYPE_ENV_BLOCK == envItr->envType) {
      if (L'\0' == envItr->data.eb.currEnvVar[0]) {
         envVar = NULL;
      } else {
         envVar = Unicode_AllocWithUTF16(envItr->data.eb.currEnvVar);
         while(*envItr->data.eb.currEnvVar++);
      }
   } else if (VIX_TOOLS_ENV_TYPE_ENVIRON == envItr->envType) {
      if (NULL == *envItr->data.env) {
         envVar = NULL;
      } else {
         envVar = Unicode_AllocWithUTF16(*envItr->data.env);
         envItr->data.env++;
      }
   } else {
      /* Is someone using uninitialized memory? */
      NOT_IMPLEMENTED();
   }
#else
   if (NULL == *envItr->environ) {
      envVar = NULL;
   } else {
      envVar = Unicode_Alloc(*envItr->environ, STRING_ENCODING_DEFAULT);
      envItr->environ++;
   }
#endif
   return envVar;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsDestroyEnvIterator --
 *
 *      Free()s any memory associated with the VixToolsEnvIterator.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
VixToolsDestroyEnvIterator(VixToolsEnvIterator *envItr)   // IN
{
   if (NULL != envItr) {
#ifdef _WIN32
      if (VIX_TOOLS_ENV_TYPE_ENV_BLOCK == envItr->envType) {
         if (NULL != envItr->data.eb.envBlock) {
            VixToolsDestroyEnvironmentBlock(envItr->data.eb.envBlock);
         }
      }
#endif
      free(envItr);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsNewUserEnvironment --
 *
 *      Create a new UserEnvironment that can be used to query for
 *      environment variables.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsNewUserEnvironment(void *userToken,                   // IN
                           VixToolsUserEnvironment **env)     // OUT
{
   VixError err = VIX_OK;
   VixToolsUserEnvironment *myEnv = Util_SafeMalloc(sizeof *myEnv);

   if (NULL == env) {
      err = VIX_E_FAIL;
      goto quit;
   }

   *env = NULL;

#ifdef _WIN32
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      myEnv->impersonated = TRUE;
      err = VixToolsGetEnvBlock(userToken, &myEnv->envBlock);
      if (VIX_FAILED(err)) {
         goto quit;
      }
   } else {
      myEnv->impersonated = FALSE;
      /* We will just read from the process's environment. */
   }
#endif

   *env = myEnv;

quit:
   if (VIX_FAILED(err)) {
      free(myEnv);
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetEnvFromUserEnvironment --
 *
 *      Looks up the environment variable given by 'name' in the provided
 *      user environment.
 *
 * Results:
 *      A heap-allocated UTF-8 string, or NULL if the environment variable
 *      is not found.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
VixToolsGetEnvFromUserEnvironment(const VixToolsUserEnvironment *env,  // IN
                                  const char *name)                    // IN
{
   char *envVar;

   if (NULL == env) {
      return NULL;
   }

#ifdef _WIN32
   if (env->impersonated) {
      envVar = VixToolsGetEnvVarFromEnvBlock(env->envBlock, name);
   } else {
      envVar = Util_SafeStrdup(Posix_Getenv(name));
   }
#else
   envVar = Util_SafeStrdup(Posix_Getenv(name));
#endif

   return envVar;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsDestroyUserEnvironment --
 *
 *      Releases any resources used by the VixToolsUserEnvironment.
 *      The VixToolsUserEnvironment must not be used after calling
 *      this function.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
VixToolsDestroyUserEnvironment(VixToolsUserEnvironment *env)   // IN
{
   if (NULL != env) {
#ifdef _WIN32
      if (NULL != env->envBlock) {
         if (env->impersonated) {
            VixToolsDestroyEnvironmentBlock(env->envBlock);
         }
      }
#endif
      free(env);
   }
}


#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsEnvironToEnvBlock --
 *
 *      Converts a NULL terminated array of UTF-8 environment variables in
 *      the form NAME=VALUE to an Win32 environment block, which is a single
 *      contiguous array containing UTF-16 environment variables in the same
 *      form, each separated by a UTF-16 null character, followed by two
 *      training null characters.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsEnvironToEnvBlock(char const * const *env,        // IN: UTF-8
                          wchar_t **envBlock)             // OUT
{
   VixError err;
   DynBuf buf;
   Bool res;
   static const wchar_t nullTerm[] = { L'\0', L'\0' };

   DynBuf_Init(&buf);

   if ((NULL == env) || (NULL == envBlock)) {
      err = VIX_E_FAIL;
      goto quit;
   }

   *envBlock = NULL;

   while (NULL != *env) {
      wchar_t *envVar = Unicode_GetAllocUTF16(*env);

      res = DynBuf_Append(&buf, envVar,
                          (wcslen(envVar) + 1) * sizeof(*envVar));
      free(envVar);
      if (!res) {
         err = VIX_E_OUT_OF_MEMORY;
         goto quit;
      }
      env++;
   }

   /*
    * Append two null characters at the end. This adds an extra (third) null
    * if there was at least one environment variable (since there already
    * is one after the last string) but we need both if there were no
    * environment variables in the input array. I'll waste two bytes to
    * keep the code a little simpler.
    */
   res = DynBuf_Append(&buf, nullTerm, sizeof nullTerm);
   if (!res) {
      err = VIX_E_OUT_OF_MEMORY;
      goto quit;
   }

   *envBlock = DynBuf_Detach(&buf);
   err = VIX_OK;

quit:
   DynBuf_Destroy(&buf);

   return err;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsValidateEnviron --
 *
 *      Ensures that the NULL terminated array of strings contains
 *      properly formated environment variables.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsValidateEnviron(char const * const *env)   // IN
{
   if (NULL == env) {
      return VIX_E_FAIL;
   }

   while (NULL != *env) {
      /*
       * Each string should contain at least one '=', to delineate between
       * the name and the value.
       */
      if (NULL == Str_Strchr(*env, '=')) {
         return VIX_E_INVALID_ARG;
      }
      env++;
   }

   return VIX_OK;
}


#ifdef VMX86_DEVEL
#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsEnvironToEnvBlockEmptyEnviron --
 *
 *      Tests VixToolsEnvironToEnvBlock() with an empty environment: an
 *      char ** pointing to a single NULL pointer.
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsEnvironToEnvBlockEmptyEnviron(void)
{
   const char *environ1[] = { NULL };
   wchar_t *envBlock;
   VixError err;

   err = VixToolsEnvironToEnvBlock(environ1, &envBlock);
   ASSERT(VIX_OK == err);

   ASSERT((L'\0' == envBlock[0]) && (L'\0' == envBlock[1]));
   free(envBlock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsEnvironToEnvBlockTwoGood --
 *
 *      Tests VixToolsEnvironToEnvBlock() with an environment containing
 *      two valid entries.
 *
 * Results:
 *      Passes or ASSERTs
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsEnvironToEnvBlockTwoGood(void)
{
   const char *environ1[] = { "foo=bar", "env=block", NULL };
   wchar_t *envBlock, *currPos;
   VixError err;

   err = VixToolsEnvironToEnvBlock(environ1, &envBlock);
   ASSERT(VIX_OK == err);

   currPos = envBlock;
   ASSERT(wcscmp(currPos, L"foo=bar") == 0);
   currPos += wcslen(L"foo=bar") + 1;
   ASSERT(wcscmp(currPos, L"env=block") == 0);
   free(envBlock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsEnvironToEnvBlock --
 *
 *      Runs unit tests for VixToolsEnvironToEnvBlock().
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsEnvironToEnvBlock(void)
{
   TestVixToolsEnvironToEnvBlockEmptyEnviron();
   TestVixToolsEnvironToEnvBlockTwoGood();
}
#endif // #ifdef _WIN32


/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsValidateEnvironEmptyEnviron --
 *
 *      Tests VixToolsEnvironToEnvBlock() with an empty environment.
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsValidateEnvironEmptyEnviron(void)
{
   const char *environ1[] = { NULL };
   VixError err;

   err = VixToolsValidateEnviron(environ1);
   ASSERT(VIX_OK == err);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsValidateEnvironTwoGoodVars --
 *
 *      Tests VixToolsEnvironToEnvBlock() with an environment containing
 *      two valid environment variables.
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsValidateEnvironTwoGoodVars(void)
{
   const char *environ1[] = { "foo=bar", "vix=api", NULL };
   VixError err;

   err = VixToolsValidateEnviron(environ1);
   ASSERT(VIX_OK == err);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsValidateEnvironOneBad --
 *
 *      Tests VixToolsEnvironToEnvBlock() with an environment containing
 *      one invalid environment variable.
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsValidateEnvironOneBad(void)
{
   const char *environ1[] = { "noequals", NULL };
   VixError err;

   err = VixToolsValidateEnviron(environ1);
   ASSERT(VIX_E_INVALID_ARG == err);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsValidateEnvironSecondBad --
 *
 *      Tests VixToolsEnvironToEnvBlock() with an environment containing
 *      one valid environment variable followed by one invalid environment
 *      variable.
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsValidateEnvironSecondBad(void)
{
   const char *environ1[] = { "foo=bar", "noequals", NULL };
   VixError err;

   err = VixToolsValidateEnviron(environ1);
   ASSERT(VIX_E_INVALID_ARG == err);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TestVixToolsValidateEnviron --
 *
 *      Run unit tests for VixToolsValidateEnviron().
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TestVixToolsValidateEnviron(void)
{
   TestVixToolsValidateEnvironEmptyEnviron();
   TestVixToolsValidateEnvironTwoGoodVars();
   TestVixToolsValidateEnvironOneBad();
   TestVixToolsValidateEnvironSecondBad();
}


/*
 *-----------------------------------------------------------------------------
 *
 * TextVixToolsEnvVars --
 *
 *      Run unit tests for functions in this file.
 *
 * Results:
 *      Passes or ASSERTs.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
TestVixToolsEnvVars(void)
{
#ifdef _WIN32
   TestVixToolsEnvironToEnvBlock();
#endif
   TestVixToolsValidateEnviron();
}
#endif // #ifdef VMX86_DEVEL
