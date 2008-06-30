/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * dnd.h --
 *
 *    Drag and Drop library
 *
 */

#ifndef _DND_H_
#define _DND_H_

#define INCLUDE_ALLOW_USERLEVEL

#ifdef _WIN32
#   include <windows.h>
#   include <shellapi.h>
#endif

#include "includeCheck.h"
#include "vm_basic_types.h"
#include "unicodeTypes.h"
#include "dynarray.h"

/* Error value returned when data contains illegal characters */
#define DND_ILLEGAL_CHARACTERS  "data contains illegal characters"
/*
 * Use the same maximum path length as Hgfs.
 * XXX: Move HGFS_PATH_MAX to some header file which is more public
 *      and use it here.
 */
#define DND_MAX_PATH        6144

#define DNDMSG_HEADERSIZE_V3 ((3 * sizeof (uint32)) + (1 * sizeof (uint8)))
/* Hard limits we never want to exceed */
/* The maximum size of a serializied DnDMsg. Close to 4M. */
#define DNDMSG_MAX_ARGSZ ((1 << 22) - DNDMSG_HEADERSIZE_V3)
/* The maximum number of arguments we can hold */
#define DNDMSG_MAX_ARGS 64

/* Strings used for formatting various types of data */
#define DND_URI_LIST_PRE     "file://"
#define DND_URI_LIST_PRE_KDE "file:"
#define DND_URI_LIST_POST    "\r\n"
#define DND_TEXT_PLAIN_PRE   ""
#define DND_TEXT_PLAIN_POST  ""
#define DND_STRING_PRE       ""
#define DND_STRING_POST      ""
#define FCP_GNOME_LIST_PRE   "file://"
#define FCP_GNOME_LIST_POST  "\n"

/* Guest detection window width and height. */
#define DRAG_DET_WINDOW_WIDTH 15

typedef enum
{
   CPFORMAT_UNKNOWN = 0,
   CPFORMAT_TEXT,       /* NUL terminated UTF-8. */
   CPFORMAT_FILELIST,
   CPFORMAT_RTF,
   CPFORMAT_MAX,
} DND_CPFORMAT;

enum DND_DROPEFFECT
{
   DROP_UNKNOWN = 1<<31,
   DROP_NONE = 0,
   DROP_COPY = 1<<0,
   DROP_MOVE = 1<<1,
   DROP_LINK = 1<<2,
};

/* Clipboard item. */
typedef struct CPClipItem {
   void *buf;
   uint32 size;
   Bool exists;
} CPClipItem;

/*
 * Cross platform clipboard. The native UI will convert host clipboard content
 * into cross platform clipboards.
 */
typedef struct {
   Bool changed;
   CPClipItem items[CPFORMAT_MAX - 1];
} CPClipboard;

/* Definitions for transport layer big buffer support (>= V3). */
typedef enum
{
   DND_TRANSPORT_PACKET_TYPE_UNKNOWN = 0,
   DND_TRANSPORT_PACKET_TYPE_SINGLE,
   DND_TRANSPORT_PACKET_TYPE_REQUEST,
   DND_TRANSPORT_PACKET_TYPE_PAYLOAD,
} DND_TRANSPORT_PACKET_TYPE;

typedef
#include "vmware_pack_begin.h"
struct DnDTransportPacketHeader {
   uint32 type;
   uint32 seqNum;
   uint32 totalSize;
   uint32 payloadSize;
   uint32 offset;
   uint8 payload[1];
}
#include "vmware_pack_end.h"
DnDTransportPacketHeader;

typedef struct DnDTransportBuffer {
   size_t seqNum;
   uint8 *buffer;
   size_t totalSize;
   size_t offset;
   VmTimeType lastUpdateTime;
} DnDTransportBuffer;

#define DND_TRANSPORT_PACKET_HEADER_SIZE      (5 * sizeof(uint32))
/* Close to 64k (maximum guestRpc message size). Leave some space for guestRpc header. */
#define DND_MAX_TRANSPORT_PACKET_SIZE         ((1 << 16) - 100)
#define DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE (DND_MAX_TRANSPORT_PACKET_SIZE - \
                                               DND_TRANSPORT_PACKET_HEADER_SIZE)
#define DND_MAX_TRANSPORT_LATENCY_TIME        3 * 1000000 /* 3 seconds. */

#ifdef _WIN32
/*
 * Windows-specific functions
 */
EXTERN uint32 DnD_GetClipboardFormatFromName(ConstUnicode pFormatName);
EXTERN Unicode DnD_GetClipboardFormatName(UINT cf);
EXTERN HGLOBAL  DnD_CopyStringToGlobal(ConstUnicode str);
EXTERN HGLOBAL DnD_CopyDWORDToGlobal(DWORD *pDWORD);
EXTERN HGLOBAL DnD_CreateHDrop(ConstUnicode path, ConstUnicode fileList);
EXTERN HGLOBAL DnD_CreateHDropForGuest(ConstUnicode path,
                                       ConstUnicode fileList);
EXTERN size_t DnD_CPStringToLocalString(ConstUnicode bufIn,
                                        utf16_t **bufOut);
EXTERN size_t DnD_LocalStringToCPString(utf16_t *bufIn,
                                        char **bufOut);
EXTERN Bool DnD_SetCPClipboardFromLocalText(CPClipboard *clip,
                                            utf16_t *bufIn);
EXTERN Bool DnD_SetCPClipboardFromLocalRtf(CPClipboard *clip,
                                           char *bufIn);
EXTERN Bool DnD_FakeMouseEvent(DWORD flag);
EXTERN Bool DnD_FakeMouseState(DWORD key, Bool isDown);
EXTERN Bool DnD_FakeEscapeKey(void);
EXTERN Bool DnD_DeleteLocalDirectory(ConstUnicode localDir);
EXTERN Bool DnD_SetClipboard(UINT format, char *buffer, int len);
EXTERN Bool DnD_GetFileList(HDROP hDrop,
                            char **remoteFiles,
                            int *remoteLength,
                            char **localFiles,
                            int *localLength,
                            uint64 *totalSize);

#else
/*
 * Posix-specific functions
 */
EXTERN char *DnD_UriListGetNextFile(char const *uriList,
                                    size_t *index,
                                    size_t *length);
#endif

/*
 * Shared functions
 */
ConstUnicode DnD_GetFileRoot(void);
char *DnD_CreateStagingDirectory(void);
Bool DnD_DeleteStagingFiles(ConstUnicode fileList, Bool onReboot);
Bool DnD_DataContainsIllegalCharacters(const char *data,
                                       const size_t dataSize);
Bool DnD_PrependFileRoot(ConstUnicode fileRoot, char **src, size_t *srcSize);
int DnD_LegacyConvertToCPName(const char *nameIn,
                              size_t bufOutSize,
                              char *bufOut);
Bool DnD_CPNameListToDynBufArray(char *fileList,
                                 size_t listSize,
                                 DynBufArray *dynBufArray);
Unicode DnD_GetLastDirName(const char *str);

/* vmblock support functions. */
int DnD_InitializeBlocking(void);
Bool DnD_UninitializeBlocking(int blockFd);
Bool DnD_AddBlock(int blockFd, const char *blockPath);
Bool DnD_RemoveBlock(int blockFd, const char *blockedPath);

/* Transport layer big buffer support functions. */
void DnD_TransportBufInit(DnDTransportBuffer *buf,
                          uint8 *msg,
                          size_t msgSize,
                          uint32 seqNum);
void DnD_TransportBufReset(DnDTransportBuffer *buf);
size_t DnD_TransportBufGetPacket(DnDTransportBuffer *buf,
                                 DnDTransportPacketHeader **packet);
Bool DnD_TransportBufAppendPacket(DnDTransportBuffer *buf,
                                  DnDTransportPacketHeader *packet,
                                  size_t packetSize);
size_t DnD_TransportMsgToPacket(uint8 *msg,
                                size_t msgSize,
                                uint32 seqNum,
                                DnDTransportPacketHeader **packet);
size_t DnD_TransportReqPacket(DnDTransportBuffer *buf,
                              DnDTransportPacketHeader **packet);

#endif // _DND_H_
