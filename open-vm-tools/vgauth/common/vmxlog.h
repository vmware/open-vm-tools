/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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

#ifndef _VMXLOG_H_
#define _VMXLOG_H_

/*
 * @file vmxlog.h
 *
 * Logging to the VMX (vmware.log)
 */

#include "VGAuthBasicDefs.h"

int VMXLog_Init(void);

void VMXLog_Shutdown(void);

void VMXLog_Log(int level, const char *fmt, ...) PRINTF_DECL(2, 3);

/*
 * * XXX Future-proofing -- currently unused.
 */
#define VMXLOG_LEVEL_INFO     1
#define VMXLOG_LEVEL_WARNING  2

void VMXLog_LogV(int level, const char *fmt, va_list args);

#endif // _VMXLOG_H_

