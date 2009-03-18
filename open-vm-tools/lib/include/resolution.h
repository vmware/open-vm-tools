/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * resolution.h --
 *
 *    Library for the guest resolution and topology fit/resize capabilities.
 *    This library handles its own RPC callbacks, and it's intended to more
 *    or less operate independently of the client application.
 *
 *    Client applications are expected to do (roughly) the following:
 *      // Init library.
 *      Resolution_Init();
 *
 *      // Register RpcIn callbacks.
 *      Resolution_InitBackdoor();
 *
 *      // Call this in response to getting a/ capreg message from the host.
 *      Resolution_RegisterCaps();
 *
 *      // Call this when you're finished with the library and wish to reclaim
 *      // resources.
 *      Resolution_Cleanup();
 *
 *    In response to a TCLO reset or to otherwise (temporarily) disable the
 *    library, one can also call Resolution_UnregisterCaps().  (This routine
 *    is also called implicitly by Resolution_Cleanup().)
 */

#ifndef _LIB_RESOLUTION_H_
#define _LIB_RESOLUTION_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vmware.h"
#include "rpcin.h"


/*
 * Data types
 */

/*
 * Argument to be passed to Resolution_Init().  Users of the X11 backend
 * should provide a Display *.  Not sure what users of Mac OS X will need
 * to provide, if anything.
 */
#if defined(RESOLUTION_X11)
#   include <X11/Xlib.h>
#   undef Bool
typedef Display *       InitHandle;

#elif defined(__APPLE__) || defined(RESOLUTION_WIN32)
typedef void *          InitHandle;
#else
#   error Unknown display backend
#endif

/*
 * Arguments to VMwareResolutionSet.exe
 */
#define RESOLUTION_SET_APP_NAME "VMwareResolutionSet.exe"
typedef enum {
   RESOLUTION_SET_NORESET   = 0,
   RESOLUTION_SET_RESET     = 1,
   RESOLUTION_SET_ARBITRARY = 2,
} ResolutionSetDisplayReset;


/*
 * Global functions
 */

Bool Resolution_Init(const char *tcloChannel, InitHandle handle);
void Resolution_Cleanup(void);

void Resolution_InitBackdoor(RpcIn *rpcIn);
void Resolution_CleanupBackdoor(void);

Bool Resolution_RegisterCaps(void);
Bool Resolution_UnregisterCaps(void);

#endif // ifndef _LIB_RESOLUTION_H_
