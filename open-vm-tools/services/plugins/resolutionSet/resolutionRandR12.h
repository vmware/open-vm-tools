/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * resolutionRandR12.h --
 *
 *      This header file exports the minimalistic RandR12 utilities
 *      interface.
 */

#ifndef _RESOLUTIONRANDR12_H_
#define _RESOLUTIONRANDR12_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#ifndef NO_MULTIMON

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xmd.h>
#include <X11/extensions/panoramiXproto.h>

#define RR12_OUTPUT_FORMAT "Virtual%u"

/*
 * Global functions
 */

extern Bool
RandR12_SetTopology(Display *dpy, int screen, Window rootWin,
                    unsigned int ndisplays, xXineramaScreenInfo *displays,
                    unsigned int width, unsigned int height);

#endif // ifndef NO_MULTIMON
#endif // ifndef _RESOLUTIONRANDR12_H_
