/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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


typedef enum {
   PARTITION_EXT2,
   PARTITION_EXT3,
   PARTITION_REISERFS,
   PARTITION_NTFS,
   PARTITION_FAT,
   PARTITION_UFS,
   PARTITION_PCFS,
} WiperPartition_Type;

/* Max size of a path */
#define NATIVE_MAX_PATH 256

typedef struct WiperPartition {
   unsigned char mountPoint[NATIVE_MAX_PATH];

   /* 
    * Empty if type is set, otherwise describes why the partition can not be
    * wiped
    */
   char *comment;

   /* Type of the partition we know how to wipe */
   WiperPartition_Type type;

#if defined(_WIN32)
   /* Private flags used by the Win32 implementation */
   DWORD flags;
#endif

#if defined(N_PLAT_NLM)
   unsigned long volumeNumber;
#endif
} WiperPartition;

typedef struct WiperPartition_List {
   WiperPartition *partitions;
   unsigned int size;
} WiperPartition_List;

typedef struct WiperInitData {
#if defined(_WIN32)
   HINSTANCE resourceModule;
#endif
} WiperInitData;

Bool Wiper_Init(void *clientData);
WiperPartition_List *WiperPartition_Open(void);
void WiperPartition_Close(WiperPartition_List *pl);

WiperPartition *SingleWiperPartition_Open(const char *mntpt);
void SingleWiperPartition_Close(WiperPartition *pl);

unsigned char *WiperSinglePartition_GetSpace(const WiperPartition *p,  
                              uint64 *free,       
                              uint64 *total);      

/* External definition of the wiper state */
struct Wiper_State;
typedef struct Wiper_State Wiper_State;

Wiper_State *Wiper_Start(const WiperPartition *p, unsigned int maxWiperFileSize);

unsigned char *Wiper_Next(Wiper_State **s, unsigned int *progress);
unsigned char *Wiper_Cancel(Wiper_State **s);

#endif /* _WIPER_H_ */
