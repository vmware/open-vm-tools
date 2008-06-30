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
 * resolutionInt.h --
 *
 *    Internal header file for lib/resolution.
 */

#ifndef _LIB_RESOLUTIONINT_H_
#define _LIB_RESOLUTIONINT_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "resolution.h"


/*
 * Data types
 */


/*
 * Describes the size and offset of a display.  An array of these
 * structures describes the entire topology of the guest desktop
 *
 * NB: Cribbed from Win32 resolution code.  Refactor this.
 */
typedef struct {
   int x;
   int y;
   int width;
   int height;
} DisplayTopologyInfo;


/*
 * Global variables
 */

extern Bool gCanSetResolution;  // Set to TRUE if initialized backend supports
                                // Resolution_Set 
extern Bool gCanSetTopology;    // Set to TRUE if " " " Topology_Set


/*
 * Global functions
 */

/* Defined per backend. */
Bool ResolutionBackendInit(InitHandle handle);
Bool ResolutionSetResolution(uint32 width, uint32 height);
Bool ResolutionSetTopology(char const **result, size_t *resultLen,
                           unsigned int ndisplays, DisplayTopologyInfo displays[]);


#endif // ifndef _LIB_RESOLUTIONINT_H_
