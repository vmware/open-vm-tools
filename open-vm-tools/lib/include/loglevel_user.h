/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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

#ifndef _LOGLEVEL_USER_H_
#define _LOGLEVEL_USER_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#define LOGLEVEL_EXTENSION user
#include "loglevel_defs.h"

#include "loglevel_userVars.h"

LOGLEVEL_EXTENSION_DECLARE(LOGLEVEL_USER);

#endif /* _LOGLEVEL_USER_H_ */
