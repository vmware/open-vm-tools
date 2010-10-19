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

#ifndef _UNITYINT_H_
#define _UNITYINT_H_

/**
 * @file unityInt.h
 *
 *    Internal function prototypes.
 *
 * @addtogroup vmtools_plugins
 * @{
 */

#include "unityWindowTracker.h"
#include "unity.h"
#include "unityPlatform.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*
 * Singleton object for tracking the state of the service.
 */
typedef struct UnityState {
   UnityWindowTracker tracker;
   Bool forceEnable;
   Bool isEnabled;
   uint32 currentOptions;                       // Last feature mask received via 'set.options'
   UnityVirtualDesktopArray virtDesktopArray;   // Virtual desktop configuration
   UnityHostCallbacks hostCallbacks;   // Callbacks to the host for unity updates
   UnityPlatform *up; // Platform-specific state
   gpointer serviceObj; // 'Plugin' Host service object - used for signaling Unity state
} UnityState;

extern UnityState unity;

#ifdef __cplusplus
};
#endif // __cplusplus

#endif // _UNITYINT_H_
