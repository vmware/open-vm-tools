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
 * ghiX11icon.h --
 *
 *	Declares GHIX11Icon type and utility functions.
 */

#ifndef _GHIX11ICON_H_
#define _GHIX11ICON_H_

#include <list>
#include <glib.h>
#include "ghIntegrationInt.h"

struct GHIBinaryIconInfo;

Bool GHIX11IconGetIconsForDesktopFile(const char* desktopFile,
                                      std::list<GHIBinaryIconInfo>& iconList);
Bool GHIX11IconGetIconsByName(const char* iconName,
                              std::list<GHIBinaryIconInfo>& iconList);


#endif // ifndef _GHIX11ICON_H_
