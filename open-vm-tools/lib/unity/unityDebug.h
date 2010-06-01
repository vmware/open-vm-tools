/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * unityDebug.h --
 *
 *    Debugging functions for unity window manager intergration.
 *    On in VMX86_DEVEL builds only.
 */

#ifndef _UNITY_DEBUG_H_
#define _UNITY_DEBUG_H_

#include "unityWindowTracker.h"

#ifdef  __cplusplus
extern "C" {
#endif // __cplusplus

#if defined(VMX86_DEVEL)
void UnityDebug_Init(UnityWindowTracker *tracker);
void UnityDebug_OnUpdate(void);
#else
#  define UnityDebug_Init(tracker)
#  define UnityDebug_OnUpdate()
#endif

#ifdef __cplusplus
};
#endif // __cplusplus

#endif

