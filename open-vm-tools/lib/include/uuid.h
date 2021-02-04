/*********************************************************
 * Copyright (C) 1998-2018, 2020-2021 VMware, Inc. All rights reserved.
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

#if defined(__cplusplus)
extern "C" {
#endif

#define UUID_SIZE 16
#define UUID_STRSIZE (2*UUID_SIZE + 1)
#define UUID_MAXLEN 48

/*
 * ISO 11578 (now X.667 section 6.4) defines the canonical text format of a
 * UUID, which looks like this example:
 *
 *    "f81d4fae-7dec-11d0-a765-00a0c91e6bf6"
 *
 * It is always precisely 36 characters long (excluding terminating NUL).
 */
#define UUID_ISO_11578_LEN 36

typedef enum {
   UUID_WITH_PATH = 0,
   UUID_RANDOM,
   UUID_VPX_BIOS,
   UUID_VPX_INSTANCE,
   UUID_UNKNOWN
} UUIDStyle;

/* Scheme control */
#define UUID_CREATE_WS6      0  /* the WS6 scheme - "native" path */
#define UUID_CREATE_WS65     1  /* the WS65 scheme - UTF-8 path */
#define UUID_CREATE_ESXi2018 2  /* UTF-8 path, no host UUID for >= 2018 ESXi */
#define UUID_CREATE_CURRENT  2  /* the current scheme - always the latest */


/*
 * RFC 4122-compliant UUIDs and UEFI/Microsoft GUIDs are essentially
 * the same thing except for byte-ordering issues.  Although their
 * fields are named differently, the meanings are the same.  The only
 * important difference is that RFC 4122 recommends always storing and
 * transmitting binary UUIDs with the multi-byte fields in network
 * byte order (big-endian), while binary UEFI/Microsoft GUIDs are
 * typically stored and transmitted with the first three fields in x86
 * CPU native order (little-endian).  But both UUIDs and GUIDs use the
 * same canonical text format representation, in which the hex digits
 * are in big-endian order.  Details of each are below.
 */

/*
 * An RFC 4122-compliant UUID.
 *
 * See RFC 4122 section 4.1.2 (Layout and Byte Order). The RFC
 * recommends that multi-byte types be stored in big-endian (most
 * significant byte first) order.
 *
 * The UUID packed text string
 *    00112233-4455-6677-8899-AABBCCDDEEFF
 * represents
 *    timeLow = 0x00112233
 *    timeMid = 0x4455
 *    timeHiAndVersion = 0x6677
 *    clockSeqHiAndReserved = 0x88
 *    clockSeqLow = 0x99
 *    node[0] = 0xAA
 *    node[1] = 0xBB
 *    node[2] = 0xCC
 *    node[3] = 0xDD
 *    node[4] = 0xEE
 *    node[5] = 0xFF
 * and the structure is stored as the sequence of bytes
 *    00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF
 *
 * Confusingly, some applications use the field names from this
 * definition but store timeLow, timeMid, and timeHiAndVersion in
 * little-endian order as UEFI/Microsoft GUIDs do; for example, the
 * SMBIOS standard does so.  In that case, the structure is stored as
 * the sequence of bytes
 *    33 22 11 00 55 44 77 66 88 99 AA BB CC DD EE FF
 */

#pragma pack(push, 1)
typedef struct {
   uint32 timeLow;
   uint16 timeMid;
   uint16 timeHiAndVersion;
   uint8  clockSeqHiAndReserved;
   uint8  clockSeqLow;
   uint8  node[6];
} UUIDRFC4122;
#pragma pack(pop)


/*
 * An EFI/UEFI/Microsoft-compliant GUID.
 *
 * See MdeModulePkg/Universal/DevicePathDxe/DevicePathFromText.c::StrToGuid(),
 * BaseTools/Source/C/Common/ParseInf.c::StringToGuid(),
 * http://en.wikipedia.org/wiki/GUID . All multi-byte types are stored in CPU
 * (a.k.a. native) byte order.
 *
 * The GUID packed text string
 *    00112233-4455-6677-8899-AABBCCDDEEFF
 * represents
 *    data1 = 0x00112233
 *    data2 = 0x4455
 *    data3 = 0x6677
 *    data4[0] = 0x88
 *    data4[1] = 0x99
 *    data4[2] = 0xAA
 *    data4[3] = 0xBB
 *    data4[4] = 0xCC
 *    data4[5] = 0xDD
 *    data4[6] = 0xEE
 *    data4[7] = 0xFF
 * and the structure is stored as the sequence of bytes
 *       big-endian CPU: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF
 *    little-endian CPU: 33 22 11 00 55 44 77 66 88 99 AA BB CC DD EE FF
 */

#pragma pack(push, 1)
typedef struct {
   uint32 data1;
   uint16 data2;
   uint16 data3;
   uint8  data4[8];
} EFIGUID;
#pragma pack(pop)

Bool UUID_ConvertPackedToBin(EFIGUID *destID,
                             const char *text);

Bool UUID_ConvertPackedToUUIDRFC4122(UUIDRFC4122 *destID,
                                     const char *text);

Bool UUID_ConvertToBin(uint8 dest_id[UUID_SIZE],
                       const char *text);

char *UUID_ConvertToText(const uint8 id[UUID_SIZE]);

void UUID_ConvertToTextBuf(const uint8 id[UUID_SIZE],
                           char *buffer,
                           size_t len);

char *UUID_CreateLocation(const char *configFileFullPath,
                          int schemeControl);

char *UUID_CreateRandom(void);

Bool UUID_CreateRandomRFC4122V4(UUIDRFC4122 *id);

Bool UUID_CreateRandomEFI(EFIGUID *id);

char *UUID_CreateRandomVpxStyle(uint8 vpxdId,
                                UUIDStyle);

Bool UUID_IsUUIDGeneratedByThatVpxd(const uint8 *id,
                                    int vpxdInstanceId);

char *UUID_PackText(const char *text,
                    char *pack,
                    size_t packLen);

char *UUID_ProperHostUUID(void);

char *UUID_GetHostUUID(void);

UUIDStyle UUID_GetStyle(const uint8 *id);

Bool UUID_Equal(const uint8 id1[UUID_SIZE],
                const uint8 id2[UUID_SIZE]);

/* Like UUID_GetHostUUID, except gets actual host UUID */
char *UUID_GetRealHostUUID(void);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
