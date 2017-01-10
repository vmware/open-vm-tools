/*********************************************************
 * Copyright (C) 2007-2014 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/


/*
 * driverLog.h --
 *
 *      Logging functions for Linux kernel modules.
 */

#ifndef __DRIVERLOG_H__
#define __DRIVERLOG_H__

/*
 * The definitions of Warning(), Log(), and Panic() come from vm_assert.h for
 * consistency.
 */
#include "vm_assert.h"

void DriverLog_Init(const char *prefix);

#endif /* __DRIVERLOG_H__ */
