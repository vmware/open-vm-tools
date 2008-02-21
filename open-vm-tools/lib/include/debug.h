/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * debug.h --
 *
 *    Platform specific debug routines
 *
 */


#ifndef __DEBUG_H__
#   define __DEBUG_H__

#   include "vm_basic_types.h"

void Debug(char const *fmt, ...) PRINTF_DECL(1, 2);
void Debug_Set(Bool enable, const char *prefix);
Bool Debug_IsEnabled(void);
void Debug_EnableToFile(const char *file, Bool backup);
Bool Debug_EnableIfFile(const char *enableFile, const char *toFileFile);


#endif /* __DEBUG_H__ */
