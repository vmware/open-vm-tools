/*********************************************************
 * Copyright (C) 2005-2019 VMware, Inc. All rights reserved.
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
#include "includeCheck.h"

#ifdef _WIN32
#   include <windows.h>
#   include <shellapi.h>
#endif

#include "vm_basic_types.h"
#include "unicodeTypes.h"
#include "dynarray.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Error value returned when data contains illegal characters */
#define DND_ILLEGAL_CHARACTERS  "data contains illegal characters"
/*
 * Use the same maximum path length as Hgfs.
 * XXX: Move HGFS_PATH_MAX to some header file which is more public
 *      and use it here.
 */
#define DND_MAX_PATH        6144

#define DNDMSG_HEADERSIZE_V3 ((3 * sizeof (uint32)) + (1 * sizeof (uint8)))
/*
 * Hard limits we never want to exceed. The maximum size of a serializied
 * DnDMsg. Close to 4M for Workstion/Fusion, 4G for Horzion.
 */
#ifdef VMX86_HORIZON_VIEW
#define DNDMSG_MAX_ARGSZ (0xffffffff - DNDMSG_HEADERSIZE_V3)
#else
#define DNDMSG_MAX_ARGSZ ((1 << 22) - DNDMSG_HEADERSIZE_V3)
#endif
/* The maximum number of arguments we can hold */
#define DNDMSG_MAX_ARGS 64

/* Linux only defines. Should be in separate dndLinux.h */
/* Strings used for formatting various types of data */
#define DND_URI_LIST_PRE     "file://"
#define DND_URI_LIST_PRE_KDE "file:"
#define DND_URI_NON_FILE_SCHEMES {"ssh", "sftp", "smb", "dav", "davs", "ftp",  NULL}
#define DND_URI_LIST_POST    "\r\n"
#define DND_TEXT_PLAIN_PRE   ""
#define DND_TEXT_PLAIN_POST  ""
#define DND_STRING_PRE       ""
#define DND_STRING_POST      ""
#define FCP_GNOME_LIST_PRE   "file://"
#define FCP_GNOME_LIST_POST  "\n"

/* FCP target used in gnome. */
#define FCP_TARGET_NAME_GNOME_COPIED_FILES   "x-special/gnome-copied-files"
#define FCP_TARGET_INFO_GNOME_COPIED_FILES   0
/* FCP target used in KDE. */
#define FCP_TARGET_NAME_URI_LIST             "text/uri-list"
#define FCP_TARGET_INFO_URI_LIST             1
/* FCP target used for nautilus 3.30 or later. */
#define FCP_TARGET_NAME_NAUTILUS_FILES       "UTF8_STRING"
#define FCP_TARGET_MIME_NAUTILUS_FILES       "x-special/nautilus-clipboard"
#define FCP_TARGET_INFO_NAUTILUS_FILES       2
/* Number of FCP targets. */
#define NR_FCP_TARGETS                       3

#define VMWARE_TARGET                        "vmware-target"

#define FCP_COPY_DELAY                       1000000  // 1 second
#define TARGET_NAME_TIMESTAMP                "TIMESTAMP"
#define TARGET_NAME_STRING                   "STRING"
#define TARGET_NAME_TEXT_PLAIN               "text/plain"
#define TARGET_NAME_UTF8_STRING              "UTF8_STRING"
#define TARGET_NAME_COMPOUND_TEXT            "COMPOUND_TEXT"
#define TARGET_NAME_APPLICATION_RTF          "application/rtf"
#define TARGET_NAME_TEXT_RICHTEXT            "text/richtext"
#define TARGET_NAME_TEXT_RTF                 "text/rtf"

#define DRAG_TARGET_NAME_URI_LIST  "text/uri-list"
#define DRAG_LEAVE_TIMEOUT         500

/* Guest detection window width and height. */
#define DRAG_DET_WINDOW_WIDTH 31

/* Clipboard image size limit. */
#define CLIPBOARD_IMAGE_MAX_WIDTH  4000
#define CLIPBOARD_IMAGE_MAX_HEIGHT 4000

typedef enum
{
   CPFORMAT_UNKNOWN = 0,
   CPFORMAT_TEXT,       /* NUL terminated UTF-8. */
   CPFORMAT_FILELIST,
   CPFORMAT_RTF,
   CPFORMAT_FILELIST_URI,
   CPFORMAT_FILECONTENTS,
   CPFORMAT_IMG_PNG,
   CPFORMAT_FILEATTRIBUTES,
   CPFORMAT_BIFF12,
   CPFORMAT_ART_GVML_CLIPFORMAT,
   CPFORMAT_HTML_FORMAT,
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
   Bool isInitialized;
   uint32 maxSize;
   CPClipItem items[CPFORMAT_MAX - 1];
} CPClipboard;

#if !defined(SWIG)

typedef enum {
   DND_FILE_TRANSFER_NOT_STARTED = 0,
   DND_FILE_TRANSFER_IN_PROGRESS,
   DND_FILE_TRANSFER_FINISHED,
} DND_FILE_TRANSFER_STATUS;

/*
 * Comment out the following for SWIG. We don't currently need to use any of
 * these data structures or call any of these functions from test scripts, and
 * we would have to link in extra libraries if so. Only DnD V3 transport layer
 * will call these functions. At some later time, may want to refactor this
 * file to separate CPClipboard definitions from all these transport-related
 * stuff (it is just the CPClipboard code that test scripts need).
 */

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
#ifdef VMX86_HORIZON_VIEW
/*
 * For Horizon DnD, expand the message size to almost 16M, which provides
 * better DnD Performance on text/rich text/image etc. dragging and dropping
 * per current performance tuning.
 */
#define DND_MAX_TRANSPORT_PACKET_SIZE         ((1 << 24) - 100)
#else
/* Close to 64k (maximum guestRpc message size). Leave some space for guestRpc header. */
#define DND_MAX_TRANSPORT_PACKET_SIZE         ((1 << 16) - 100)
#endif

#define DND_MAX_TRANSPORT_PACKET_PAYLOAD_SIZE (DND_MAX_TRANSPORT_PACKET_SIZE - \
                                               DND_TRANSPORT_PACKET_HEADER_SIZE)
#define DND_MAX_TRANSPORT_LATENCY_TIME        3 * 1000000 /* 3 seconds. */

/*
 * Structure to access methods of currently used blocking mechanism.
 */
typedef struct DnDBlockControl {
   int fd;
   const char *blockRoot;
   Bool (*AddBlock)(int blockFd, const char *blockPath);
   Bool (*RemoveBlock)(int blockFd, const char *blockedPath);
} DnDBlockControl;

#ifdef _WIN32
#ifdef METRO
DECLARE_HANDLE(HDROP);
#endif
/*
 * Windows-specific functions
 */
char *DnD_GetClipboardFormatName(UINT cf);
HGLOBAL DnD_CopyStringToGlobal(const char *str);
HGLOBAL DnD_CopyDWORDToGlobal(DWORD *pDWORD);
HGLOBAL DnD_CreateHDrop(const char *path, const char *fileList);
HGLOBAL DnD_CreateHDropForGuest(const char *path,
                                const char *fileList);
size_t DnD_CPStringToLocalString(const char *bufIn,
                                 utf16_t **bufOut);
size_t DnD_LocalStringToCPString(utf16_t *bufIn,
                                 char **bufOut);
Bool DnD_SetCPClipboardFromLocalText(CPClipboard *clip,
                                     utf16_t *bufIn);
Bool DnD_SetCPClipboardAndTruncateLocalText(CPClipboard *clip,
                                            utf16_t *bufIn);
Bool DnD_SetCPClipboardFromLocalRtf(CPClipboard *clip,
                                    char *bufIn);
Bool DnD_SetCPClipboardFromSpecifiedFormat(CPClipboard *clip,
                                           const DND_CPFORMAT fmt,
                                           char *bufIn,
                                           unsigned int len);
Bool DnD_SetCPClipboardFromBMPInfo(CPClipboard *clip,
                                   const LPBITMAPINFOHEADER bmi,
                                   DND_CPFORMAT fmt);
Bool DnD_SetCPClipboardFromHBITMAP(CPClipboard *clip,
                                   HBITMAP hBitmap,
                                   DND_CPFORMAT fmt);
Bool DnD_PNGToLocalFormat(const unsigned char *pngData,
                          unsigned int pngDataLen,
                          int pngReadFlags,
                          DynBuf *bmpData,
                          HBITMAP *hBitmap);
Bool DnD_FakeMouseEvent(DWORD flag);
Bool DnD_FakeMouseState(DWORD key, Bool isDown);
Bool DnD_FakeEscapeKey(void);
Bool DnD_DeleteLocalDirectory(const char *localDir);
Bool DnD_SetClipboard(UINT format, char *buffer, int len);
Bool DnD_GetFileList(HDROP hDrop,
                     char **remoteFiles,
                     int *remoteLength,
                     char **localFiles,
                     int *localLength,
                     uint64 *totalSize);

#else
/*
 * Posix-specific functions
 */

char *DnD_UriListGetNextFile(char const *uriList,
                             size_t *index,
                             size_t *length);
Bool DnD_UriIsNonFileSchemes(char const *uri);
#endif

/*
 * Shared functions
 */
const char *DnD_GetFileRoot(void);
char *DnD_CreateStagingDirectory(void);
char *DnD_AppendPrefixToStagingDir(const char *oldName, const char *newName);
Bool DnD_DeleteStagingFiles(const char *stagingDir, Bool onReboot);
Bool DnD_RemoveTempDirs(const char *dndTempDir, const char *prefix);
int DnD_LegacyConvertToCPName(const char *nameIn,
                              size_t bufOutSize,
                              char *bufOut);
Bool DnD_CPNameListToDynBufArray(char *fileList,
                                 size_t listSize,
                                 DynBufArray *dynBufArray);
char *DnD_GetLastDirName(const char *str);

void DnD_SetCPClipboardAndTruncateText(CPClipboard *clip,
                                       char *destBuf,
                                       size_t len);

/* vmblock support functions. */
Bool DnD_InitializeBlocking(DnDBlockControl *blkCtrl);
Bool DnD_UninitializeBlocking(DnDBlockControl *blkCtrl);
Bool DnD_CompleteBlockInitialization(int fd, DnDBlockControl *blkCtrl);

static INLINE Bool
DnD_BlockIsReady(DnDBlockControl *blkCtrl)   // IN: blocking control structure
{
   if (blkCtrl->fd >= 0) {
      ASSERT(blkCtrl->AddBlock && blkCtrl->RemoveBlock);
      return TRUE;
   }
   return FALSE;
}

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
#endif // !SWIG

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _DND_H_
