/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * uuid.h --
 *
 *      UUID generation
 */

#ifndef _UUID_H_
#define _UUID_H_

#define INCLUDE_ALLOW_USERLEVEL 
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h" 

#define UUID_SIZE 16
#define UUID_STRSIZE (2*UUID_SIZE + 1)
#define	UUID_MAXLEN 48

typedef enum {
   UUID_WITH_PATH = 0,
   UUID_RANDOM,
   UUID_VPX_BIOS,
   UUID_VPX_INSTANCE,
   UUID_UNKNOWN
} UUIDStyle;

Bool UUID_ConvertToBin(uint8 dest_id[UUID_SIZE], const char *text);
char *UUID_ConvertToText(const uint8 id[UUID_SIZE]);

#define UUID_CREATE_WS4     0  /* the "original", WS4 and earlier scheme */
#define UUID_CREATE_WS5     1  /* the WS5 scheme */
#define UUID_CREATE_WS6     2  /* the WS6 scheme - "native" path */
#define UUID_CREATE_WS65    3  /* the WS65 scheme - UTF-8 path */
#define UUID_CREATE_CURRENT 3  /* the current scheme - always the latest */

char *UUID_Create(const char *configFileFullPath, int schemeControl);

char *UUID_CreateRandom(void);
char *UUID_CreateRandomVpxStyle(uint8 vpxdId, UUIDStyle);
Bool UUID_IsUUIDGeneratedByThatVpxd(const uint8 *id, int vpxdInstanceId);
char *UUID_PackText(const char *text, char *pack, int packLen);
char *UUID_ProperHostUUID(void);
char *UUID_GetHostUUID(void);
UUIDStyle UUID_GetStyle(const uint8 *id);
/* like UUID_GetHostUUID, except gets actual host UUID */
char *UUID_GetRealHostUUID(void);
#endif
