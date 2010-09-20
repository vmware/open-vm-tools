/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
      wchar_t **environ;
   } data;
#else
   char **environ;
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
                       VixToolsEnvIterator **envItr)     // OUT
{
   VixError err = VIX_OK;
   VixToolsEnvIterator *it = Util_SafeMalloc(sizeof *it);

   if (NULL == envItr) {
      err = VIX_E_FAIL;
      goto abort;
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
         goto abort;
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
      it->data.environ = _wenviron;
   }
#elif defined(__APPLE__)
   it->environ = *_NSGetEnviron();
#elif defined(__FreeBSD__)
   /*
    * Looking at /build/toolchain/bsd32/freebsd-6.3/usr/include/stand.h,
    * environ is a pointer to a doubly linked list of structs. I guess they
    * just want to be different. Anyway, this is something that needs
    * work if we want to support FreeBSD.
    */
   err = VIX_E_NOT_SUPPORTED;
   goto abort;
#else
   it->environ = environ;
#endif
   *envItr = it;
abort:
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
      if (NULL == *envItr->data.environ) {
         envVar = NULL;
      } else {
         Unicode_AllocWithUTF16(*envItr->data.environ);
         envItr->data.environ++;
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
