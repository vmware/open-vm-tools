/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
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

int HgfsEscape_GetSize(char const *bufIn, // IN
                       uint32 sizeIn);    // IN
int HgfsEscape_Do(char const *bufIn, // IN
                  uint32 sizeIn,     // IN
                  uint32 sizeBufOut, // IN
                  char *bufOut);     // OUT

int HgfsEscape_Undo(char *bufIn,    // IN
                    uint32 sizeIn); // IN

#endif // __HGFS_ESCAPE_H__
