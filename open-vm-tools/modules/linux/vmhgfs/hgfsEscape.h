/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * hgfsEscape.h --
 *
 *    Escape and unescape filenames that are not legal on a particular
 *    platform.
 *
 */

#ifndef __HGFS_ESCAPE_H__
#define __HGFS_ESCAPE_H__

#if defined(WIN32)
int HgfsEscape_GetSizeW(wchar_t const *bufIn, // IN
                        uint32 sizeIn);       // IN (chars)
int HgfsEscape_DoW(wchar_t const *bufIn, // IN
		   uint32 sizeIn,        // IN (chars)
		   uint32 sizeBufOut,    // IN (bytes)
		   wchar_t *bufOut);     // OUT

int HgfsEscape_Undo(char *bufIn,    // IN
                    uint32 sizeIn); // IN

int HgfsEscape_UndoW(wchar_t *bufIn,   // IN
                     uint32 sizeIn);   // IN
#else
int HgfsEscape_Do(char const *bufIn,   // IN
		  uint32 sizeIn,       // IN (chars)
		  uint32 sizeBufOut,   // IN (bytes)
		  char *bufOut);       // OUT

int HgfsEscape_Undo(char *bufIn,    // IN
		    uint32 sizeIn); // IN
#endif // defined(WIN32)
#endif // __HGFS_ESCAPE_H__
