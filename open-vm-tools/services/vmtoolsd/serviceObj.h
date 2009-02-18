/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

#ifndef _SERVICEOBJ_H_
#define _SERVICEOBJ_H_

/**
 * @file serviceObj.h
 *
 * Interface of the "core service" object. This interface is not really
 * public, just the type itself, so that plugins can provide their own
 * signals for communicating with other plugins in the same process. For
 * this reason, it doesn't provide all the GObject boilerplate macros.
 */

#include <glib-object.h>
#include "vmtoolsApp.h"

#define TOOLSCORE_TYPE_SERVICE   ToolsCore_Service_get_type()

typedef struct ToolsCoreService {
   GObject        parent;
} ToolsCoreService;

typedef struct ToolsCoreServiceClass {
   GObjectClass   parentClass;
} ToolsCoreServiceClass;

GType
ToolsCore_Service_get_type(void);

#endif /* _SERVICEOBJ_H_ */

