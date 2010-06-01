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
#include "vm_app.h"


/*
 * Data types
 */


/*
 * Describes internal state of the resolution library.  I.e., tracks whether
 * a capability is supported, enabled, etc.
 */
typedef struct {
   Bool initialized;                    // TRUE if successfully initialized.
   Bool canSetResolution;               // TRUE if back-end supports Resolution_Set.
   Bool canSetTopology;                 // TRUE if back-end supports DisplayTopology_Set.
   Bool cbResolutionRegistered;         // TRUE if Resolution_Set CB registered.
   Bool cbTopologyRegistered;           // TRUE if DisplayTopology_Set CB registered.
   RpcIn *rpcIn;                        // Points to RpcIn channel.

   char tcloChannel[MAX(sizeof TOOLS_DAEMON_NAME,
                        sizeof TOOLS_DND_NAME)];
                                        // Names the TCLO channel which the library will
                                        //    use for registering capabilities.
} ResolutionInfoType;


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

extern ResolutionInfoType resolutionInfo;


/*
 * Global functions
 */

/* Defined per back-end. */
Bool ResolutionBackendInit(InitHandle handle);
void ResolutionBackendCleanup(void);
Bool ResolutionSetResolution(uint32 width, uint32 height);
Bool ResolutionSetTopology(unsigned int ndisplays, DisplayTopologyInfo displays[]);


#endif // ifndef _LIB_RESOLUTIONINT_H_
