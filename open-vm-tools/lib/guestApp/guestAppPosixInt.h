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
 * guestAppPosixInt.h --
 *
 *	Declarations specific to the POSIX flavor of lib/guestapp.
 */

#ifndef _GUESTAPPPOSIXINT_H_
#define _GUESTAPPPOSIXINT_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vmware.h"


/*
 * Environment used when launching applications.
 */
extern const char **guestAppSpawnEnviron;


/*
 * Global functions
 */

#ifdef GUESTAPP_HAS_X11
extern Bool GuestAppX11OpenUrl(const char *url, Bool maximize);
#endif



#endif // ifndef _GUESTAPPPOSIXINT_H_
