/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * copyPasteCompat.h --
 *
 *    copyPaste header file.
 *
 */

#if !defined _COPYPASTE_COMPAT_H
#define _COPYPASTE_COMPAT_H

#include "vmware.h"

/* Functions to be compatible with old text copy/paste directly based on backdoor cmd. */
Bool CopyPaste_RequestSelection(void);
Bool CopyPaste_GetBackdoorSelections(void);
Bool CopyPaste_IsRpcCPSupported(void);
void CopyPaste_Init(void);
void CopyPaste_SetVersion(int version);

int32
CopyPaste_GetHostSelectionLen(void);

void
CopyPaste_GetHostSelection(unsigned int size, // IN
                           char *data);       // OUT

void
CopyPaste_SetSelLength(uint32 length); // IN

void
CopyPaste_SetNextPiece(uint32 data); // IN

#endif
