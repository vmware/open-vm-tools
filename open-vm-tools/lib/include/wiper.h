/*********************************************************
 * Copyright (C) 2004-2019 VMware, Inc. All rights reserved.
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
 * wiper.h --
 *
 *      Library for wiping a virtual disk.
 *
 */

#ifndef _WIPER_H_
# define _WIPER_H_

#if defined(_WIN32) && defined(_MSC_VER)
#include <windows.h>
#endif

#include "vm_basic_types.h"
#include "dbllnklst.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define TOOLS_WIPE_CANCEL "Wipe cancelled by user.\n"

typedef enum {
   PARTITION_UNSUPPORTED = 0,
   PARTITION_EXT2,
   PARTITION_EXT3,
   PARTITION_REISERFS,
   PARTITION_NTFS,
   PARTITION_FAT,
   PARTITION_UFS,
   PARTITION_PCFS,
   PARTITION_EXT4,
   PARTITION_HFS,
   PARTITION_ZFS,
   PARTITION_XFS,
   PARTITION_BTRFS,
} WiperPartition_Type;

/* Max size of a path */
#define NATIVE_MAX_PATH 256
#define MAX_WIPER_FILE_SIZE (2 << 30)   /* The maximum wiper file size in bytes */

typedef struct WiperPartition {
   unsigned char mountPoint[NATIVE_MAX_PATH];

   /* Type of the partition */
   WiperPartition_Type type;

   /* Filesystem name - testing */
   const char *fsName;

   /* Filesystem type (name) */
   const char *fsType;

   /*
    * Clients should specifically set this flag to TRUE to enable free space
    * reclamation using unmaps.
    */
   Bool attemptUnmaps;

   /*
    * NULL if type is not PARTITION_UNSUPPORTED, otherwise describes
    * why the partition can not be wiped.
    */
   const char *comment;

#if defined(_WIN32)
   /* Private flags used by the Win32 implementation */
   DWORD flags;
#endif

   DblLnkLst_Links link;
} WiperPartition;

typedef struct WiperPartition_List {
   DblLnkLst_Links link;
} WiperPartition_List;

typedef struct WiperInitData {
#if defined(_WIN32)
   HINSTANCE resourceModule;
#endif
} WiperInitData;

Bool Wiper_Init(WiperInitData *clientData);
Bool WiperPartition_Open(WiperPartition_List *pl, Bool shrinkableOnly);
void WiperPartition_Close(WiperPartition_List *pl);

WiperPartition *WiperSinglePartition_Allocate(void);
WiperPartition *WiperSinglePartition_Open(const char *mntpt, Bool shrinkableOnly);
void WiperSinglePartition_Close(WiperPartition *);
Bool Wiper_IsWipeSupported(const WiperPartition *);

unsigned char *WiperSinglePartition_GetSpace(const WiperPartition *p,
                                             uint64 *avail,
                                             uint64 *free,
                                             uint64 *total);

/* External definition of the wiper state */
struct Wiper_State;
typedef struct Wiper_State Wiper_State;

Wiper_State *Wiper_Start(const WiperPartition *p, unsigned int maxWiperFileSize);

unsigned char *Wiper_Next(Wiper_State **s, unsigned int *progress);
unsigned char *Wiper_Cancel(Wiper_State **s);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _WIPER_H_ */
