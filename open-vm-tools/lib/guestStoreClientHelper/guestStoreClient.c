/*********************************************************
 * Copyright (c) 2020-2021 VMware, Inc. All rights reserved.
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
 * guestStoreClient.c --
 *
 *     Wrapper functions to load/unload and get content from GuestStore.
 */

#define G_LOG_DOMAIN          "guestStoreClient"

#include <glib-object.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

#include "vm_assert.h"
#include "vm_basic_defs.h"
#include "guestStoreClient.h"


#if defined(_WIN32)
#define GUESTSTORE_CLIENTLIB_DLL   WSTR("guestStoreClient.dll")
#else
#define GUESTSTORE_CLIENTLIB_DLL   "libguestStoreClient.so.0"
#endif

/*
 * Track whether the library has been initialized or not.
 */
static gboolean gsClientInit = FALSE;

/*
 * Module handle of GuestStore client library.
 */
#if defined(_WIN32)
static HMODULE gsClientLibModule = NULL;
#else
static void *gsClientLibModule = NULL;
#endif


/*
 * Function pointer types for GuestStore client library exports.
 */
typedef GuestStoreLibError (*GuestStoreLibInit)(void);
typedef GuestStoreLibError (*GuestStoreLibDeInit)(void);
typedef GuestStoreLibError (*GuestStoreLibGetContent)(const char* contentPath,
                                                      const char* outputPath,
                                                      GuestStore_Logger logger,
                                                      GuestStore_Panic panic,
                                                      GuestStore_GetContentCallback getContentCb,
                                                      void* clientData);

/*
 * Function pointer definitions for GuestStore client library exports.
 */

static GuestStoreLibInit       GuestStoreLib_Init;
static GuestStoreLibDeInit     GuestStoreLib_DeInit;
static GuestStoreLibGetContent GuestStoreLib_GetContent;

/*
 * Macro to get the export function address from GuestStore client library.
 */
#if defined(_WIN32)

#define GET_GUESTSTORELIB_FUNC_ADDR(funcname)                              \
   do {                                                                    \
      (FARPROC) XXCONC(GuestStoreLib_,funcname) = GetProcAddress(gsClientLibModule,  \
         "GuestStore_" #funcname);                                         \
      if (XXCONC(GuestStoreLib_,funcname) == NULL) {                       \
         error = GetLastError();                                           \
         g_critical("GetProcAddress failed for \'%s\': error=%u.\n",       \
                    "GuestStore_" #funcname, error);                       \
         return FALSE;                                                     \
      }                                                                    \
   } while (0)

#else

#define GET_GUESTSTORELIB_FUNC_ADDR(funcname)                        \
   do {                                                              \
      dlerror();                                                     \
      *(void **)(&XXCONC(GuestStoreLib_,funcname)) = dlsym(gsClientLibModule,  \
         "GuestStore_" #funcname);                                   \
      if ((dlErrStr = dlerror()) != NULL) {                          \
         g_critical("dlsym failed for \'%s\': %s\n",                 \
                    "GuestStore_" #funcname, dlErrStr);              \
         return FALSE;                                               \
      }                                                              \
   } while (0)

#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreGetLibExportFunctions --
 *
 *      Get the export function addresses from GuestStore client library.
 *
 * Results:
 *      TRUE on success.
 *      FALSE on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestStoreGetLibExportFunctions(void)
{

#if defined(_WIN32)
   DWORD error;

   g_debug("Entering %s.\n", __FUNCTION__);

   gsClientLibModule = LoadLibraryW(GUESTSTORE_CLIENTLIB_DLL);
   if (gsClientLibModule == NULL) {
      error = GetLastError();
      g_critical("%s: LoadLibrary failed: error=%u.\n", __FUNCTION__, error);
      return FALSE;
   }
#else
   char const *dlErrStr;

   g_debug("Entering %s.\n", __FUNCTION__);

   gsClientLibModule = dlopen(GUESTSTORE_CLIENTLIB_DLL, RTLD_NOW);
   if (gsClientLibModule == NULL) {
      dlErrStr = dlerror();
      g_critical("%s: dlopen failed: %s\n", __FUNCTION__, dlErrStr);
      return FALSE;
   }
#endif

   GET_GUESTSTORELIB_FUNC_ADDR(Init);        // For GuestStore_Init
   GET_GUESTSTORELIB_FUNC_ADDR(GetContent);  // For GuestStore_GetContent
   GET_GUESTSTORELIB_FUNC_ADDR(DeInit);      // For GuestStore_DeInit

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreClientLogger --
 *
 *      Log messages from GuestStore client library.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestStoreClientLogger(GuestStoreLibLogLevel level,  // IN
                       const char *message,          // IN
                       void *clientData)             // IN
{
   switch (level) {
      case GSLIBLOGLEVEL_ERROR:
         g_critical("%s: Error: %s\n", __FUNCTION__, message);
         break;

      case GSLIBLOGLEVEL_WARNING:
         g_warning("%s: Warning: %s\n", __FUNCTION__, message);
         break;

      case GSLIBLOGLEVEL_INFO:
         g_info("%s: Info: %s\n", __FUNCTION__, message);
         break;

      case GSLIBLOGLEVEL_DEBUG:
         g_debug("%s: Debug: %s\n", __FUNCTION__, message);
         break;

      default:
         ASSERT(0);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreClientPanic --
 *
 *      Panic handler for GuestStore client library.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Process crashes?
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestStoreClientPanic(const char *message,  // IN
                      void *clientData)     // IN
{
   g_critical("%s: %s\n", __FUNCTION__, message);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreClient_Init --
 *
 *      Initialize the guest store client library access
 *
 * Results:
 *      TRUE on success., FALSE otherwise
 *
 *      Error code from GuestStore client library or
 *      general process error exit code.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
GuestStoreClient_Init(void)
{
   GuestStoreLibError libErr;

   g_debug("Entering %s.\n", __FUNCTION__);

   if (!GuestStoreGetLibExportFunctions()) {
      goto exit;
   }

   libErr = GuestStoreLib_Init();
   if (libErr != GSLIBERR_SUCCESS) {
      g_critical("%s: GuestStoreLib_Init failed: error=%d.\n",
                 __FUNCTION__, libErr);
      goto exit;
   }
   gsClientInit = TRUE;

exit:
   g_debug("%s: Exit -> %d.\n", __FUNCTION__, gsClientInit);
   return gsClientInit;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreClient_DeInit --
 *
 *      Deinitialize the guest store client library access
 *
 * Results:
 *      TRUE on success, FALSE otherwise
 *
 *      Error code from GuestStore client library or
 *      general process error exit code.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
GuestStoreClient_DeInit(void)
{
   GuestStoreLibError libErr;

   g_debug("Entering %s.\n", __FUNCTION__);

   if (!gsClientInit) {
      return gsClientInit;
   }

   libErr = GuestStoreLib_DeInit();
   if (libErr != GSLIBERR_SUCCESS) {
      g_critical("%s: GuestStore_DeInit failed: error=%d.\n",
                 __FUNCTION__, libErr);
   }

   if (gsClientLibModule != NULL) {
#if defined(_WIN32)
      if (!FreeLibrary(gsClientLibModule)) {
         g_critical("%s: FreeLibrary failed: error=%d.\n",
                    __FUNCTION__, GetLastError());
      }
#else
      if (dlclose(gsClientLibModule) != 0) {
         g_critical("%s: dlclose failed with error: %s\n",
                    __FUNCTION__, dlerror());
      }
#endif

      gsClientLibModule = NULL;
   }

   g_debug("Exiting %s.\n", __FUNCTION__);

   gsClientInit = FALSE;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestStoreClient_GetContent --
 *
 *      Handle and parse gueststore command.
 *
 * Results:
 *      Error code from GuestStore client library or
 *      general process error exit code.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GuestStoreClientError
GuestStoreClient_GetContent(const char *contentPath,                     // IN: content file path
                            const char *outputPath,                      // IN: output file path
                            GuestStoreClient_GetContentCb getContentCb,  // IN: OPTIONAL callback
                            void *clientCbData)                          // IN: OPTIONAL callback data
{
   g_debug("Entering %s.\n", __FUNCTION__);

   if (!gsClientInit) {
      return GSLIBERR_NOT_INITIALIZED;
   }

   return GuestStoreLib_GetContent(contentPath, outputPath,
                                   GuestStoreClientLogger,
                                   GuestStoreClientPanic,
                                   getContentCb, clientCbData);
}
