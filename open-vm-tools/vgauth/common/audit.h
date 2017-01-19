/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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

#ifndef _AUDIT_H_
#define _AUDIT_H_

/*
 * @file audit.h
 *
 * Auditing support.
 */

#include "VGAuthBasicDefs.h"
#include <glib.h>

void Audit_Init(const char *appName, gboolean logSuccess);

void Audit_Shutdown(void);

void Audit_Event(gboolean isSuccess, const char *fmt, ...) PRINTF_DECL(2, 3);

void Audit_EventV(gboolean isSuccess, const char *fmt, va_list args);

#endif // _AUDIT_H_
