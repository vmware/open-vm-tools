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
 * @file VGAuthCommon.h
 *
 * Core APIs for VGAuth.
 *
 * @addtogroup vgauth_init VGAuth Initialization and Error Handling
 * @{
 *
 */
#ifndef _VGAUTHCOMMON_H_
#define _VGAUTHCOMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "VGAuthError.h"


/**
 * VGAuth boolean
 */
typedef int VGAuthBool;
enum {
   /** False */
   VGAUTH_FALSE = 0,
   /** True */
   VGAUTH_TRUE = 1,
};


/**
 * Opaque handle to a VGAuth context, which contains context state and data.
 */
typedef struct VGAuthContext VGAuthContext;

/* Utility APIs */


/**
 * VGAuth extra parameters. Used to pass additional information to the
 * VGAuth library when making an API call.
 */
typedef struct VGAuthExtraParams {
   char *name;                      /**< The parameter's name. */
   char *value;                     /**< The parameters's value.  */
} VGAuthExtraParams;

#define VGAUTH_PARAM_VALUE_TRUE  "true"
#define VGAUTH_PARAM_VALUE_FALSE "false"

/*
 * Initalizes library, and specifies any configuration information.
 */
VGAuthError VGAuth_Init(const char *applicationName,
                        int numExtraParams,
                        const VGAuthExtraParams *extraParams,
                        VGAuthContext **ctx);

/*
 * Cleans up a context and any associated data.
 */
VGAuthError VGAuth_Shutdown(VGAuthContext *ctx);


/*
 * Provides any OS specific support that may be required.
 */
VGAuthError VGAuth_InstallClient(VGAuthContext *ctx,
                                 int numExtraParams,
                                 const VGAuthExtraParams *extraParams);


/*
 * Removes any OS specific support that may be required.
 */
VGAuthError VGAuth_UninstallClient(VGAuthContext *ctx,
                                   int numExtraParams,
                                   const VGAuthExtraParams *extraParams);


/**
 * The callback function used by #VGAuth_SetLogHandler().
 */
typedef void VGAuthLogFunc(const char *logDomain,
                           int logLevel,
                           const char *msg,
                           void *userData);

/*
 * Sets the log handler.  All VGAuth and glib errors, warnings and debug
 * messages will go through logFunc.
 */
VGAuthError VGAuth_SetLogHandler(VGAuthLogFunc logFunc,
                                 void *userData,
                                 int numExtraParams,
                                 const VGAuthExtraParams *extraParams);

void VGAuth_FreeBuffer(void *buffer);

const char *VGAuth_GetErrorName(VGAuthError err);
const char *VGAuth_GetErrorText(VGAuthError err,
                                const char *language);

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif   // _VGAUTHCOMMON_H_
