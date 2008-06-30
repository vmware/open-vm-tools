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

#elif defined(__APPLE__)
typedef void *          InitHandle;
#else
#   error Unknown display backend
#endif


/*
 * Global functions
 */

Bool Resolution_Init(const char *tcloChannel, InitHandle handle);
Bool Resolution_Register(RpcIn *rpcIn);
Bool Resolution_RegisterCapability(void);
Bool Resolution_UnregisterCapability(void);

#endif // ifndef _LIB_RESOLUTION_H_
