/*********************************************************
 * Copyright (C) 2013-2018 VMware, Inc. All rights reserved.
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
 * hgfsServerParameters.h --
 *
 *	This defines the HGFS protocol message packet functions
 *	for creating requests and extracting data from replies.
 */

#ifndef _HGFS_SERVER_PARAMETERS_H_
#define _HGFS_SERVER_PARAMETERS_H_

#include "hgfsServer.h"    // for HgfsPacket type
#include "hgfsProto.h"     // for the HGFS protocol request, reply and types
#include "hgfsUtil.h"      // for HgfsInternalStatus
#include "hgfsServerInt.h" // for HgfsSessionInfo


/*
 * Global functions
 */


HgfsInternalStatus
HgfsUnpackPacketParams(const void *packet,      // IN: HGFS packet
                       size_t packetSize,       // IN: request packet size
                       Bool *sessionEnabled,    // OUT: session enabled request
                       uint64 *sessionId,       // OUT: session Id
                       uint32 *requestId,       // OUT: unique request id
                       HgfsOp *opcode,          // OUT: request opcode
                       size_t *payloadSize,     // OUT: size of the opcode request
                       const void **payload);    // OUT: pointer to the opcode request

Bool
HgfsPackReplyHeader(HgfsInternalStatus status,    // IN: reply status
                    uint32 payloadSize,           // IN: size of the reply payload
                    Bool sessionEnabledHeader,    // IN: session enabled header
                    uint64 sessionId,             // IN: session id
                    uint32 requestId,             // IN: request id
                    HgfsOp op,                    // IN: request type
                    uint32 hdrFlags,              // IN: header flags
                    size_t hdrPacketSize,         // IN: header packet size
                    void *hdrPacket);             // OUT: outgoing packet header

Bool
HgfsUnpackOpenRequest(const void *packet,          // IN: incoming packet
                      size_t packetSize,           // IN: size of packet
                      HgfsOp op,                   // IN: request type
                      HgfsFileOpenInfo *openInfo); // IN/OUT: open info struct

Bool
HgfsPackOpenReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                  const void *packetHeader,     // IN: packet header
                  HgfsFileOpenInfo *openInfo,   // IN: open info struct
                  size_t *payloadSize,          // OUT: outgoing packet size
                  HgfsSessionInfo *session);    // IN: Session Info

Bool
HgfsUnpackGetattrRequest(const void *packetHeader,   // IN: packet header
                         size_t packetSize,          // IN: request packet size
                         HgfsOp op,                  // IN: request type
                         HgfsFileAttrInfo *attrInfo, // IN/OUT: unpacked attr struct
                         HgfsAttrHint *hints,        // OUT: getattr hints
                         const char **cpName,        // OUT: cpName
                         size_t *cpNameSize,         // OUT: cpName size
                         HgfsHandle *file,           // OUT: file handle
                         uint32 *caseFlags);         // OUT: case-sensitivity flags

Bool
HgfsUnpackDeleteRequest(const void *packet,         // IN: request packet
                        size_t packetSize,          // IN: request packet size
                        HgfsOp  op,                 // IN: requested operation
                        const char **cpName,        // OUT: cpName
                        size_t *cpNameSize,         // OUT: cpName size
                        HgfsDeleteHint *hints,      // OUT: delete hints
                        HgfsHandle *file,           // OUT: file handle
                        uint32 *caseFlags);         // OUT: case-sensitivity flags

Bool
HgfsPackDeleteReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                    const void *packetHeader,   // IN: packet header
                    HgfsOp op,                  // IN: requested operation
                    size_t *payloadSize,        // OUT: size of HGFS packet
                    HgfsSessionInfo *session);  // IN: Session Info

Bool
HgfsUnpackRenameRequest(const void *packet,         // IN: request packet
                        size_t packetSize,          // IN: request packet size
                        HgfsOp op,                  // IN: requested operation
                        const char **cpOldName,     // OUT: rename src
                        size_t *cpOldNameLen,       // OUT: rename src size
                        const char **cpNewName,     // OUT: rename dst
                        size_t *cpNewNameLen,       // OUT: rename dst size
                        HgfsRenameHint *hints,      // OUT: rename hints
                        HgfsHandle *srcFile,        // OUT: src file handle
                        HgfsHandle *targetFile,     // OUT: target file handle
                        uint32 *oldCaseFlags,       // OUT: old case-sensitivity flags
                        uint32 *newCaseFlags);      // OUT: new case-sensitivity flags

Bool
HgfsPackRenameReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                    const void *packetHeader,   // IN: packet header
                    HgfsOp op,                  // IN: requested operation
                    size_t *payloadSize,        // OUT: size of packet
                    HgfsSessionInfo *session);  // IN: Session Info

Bool
HgfsPackGetattrReply(HgfsPacket *packet,          // IN/OUT: Hgfs packet
                     const void *packetHeader,    // IN: packet header
                     HgfsFileAttrInfo *attr,      // IN: attr stucture
                     const char *utf8TargetName,  // IN: optional target name
                     uint32 utf8TargetNameLen,    // IN: file name length
                     size_t *payloadSize,         // OUT: size of HGFS packet
                     HgfsSessionInfo *session);   // IN: Session Info
Bool
HgfsPackSymlinkCreateReply(HgfsPacket *packet,        // IN/OUT: Hgfs packet
                           const void *packetHeader,  // IN: packet header
                           HgfsOp op,                 // IN: request type
                           size_t *payloadSize,       // OUT: size of HGFS packet
                           HgfsSessionInfo *session); // IN: Session Info
Bool
HgfsUnpackSearchReadRequest(const void *packet,           // IN: request packet
                            size_t packetSize,            // IN: packet size
                            HgfsOp op,                    // IN: requested operation
                            HgfsSearchReadInfo *info,     // OUT: search info
                            size_t *baseReplySize,        // OUT: op base reply size
                            size_t *inlineReplyDataSize,  // OUT: size of inline reply data
                            HgfsHandle *hgfsSearchHandle);// OUT: hgfs search handle
Bool
HgfsPackSearchReadReplyRecord(HgfsOp requestType,           // IN: search read request
                              HgfsSearchReadEntry *entry,   // IN: entry info
                              size_t maxRecordSize,         // IN: max size in bytes for record
                              void *lastSearchReadRecord,   // IN/OUT: last packed entry
                              void *currentSearchReadRecord,// OUT: currrent entry to pack
                              size_t *replyRecordSize);     // OUT: size of packet
Bool
HgfsPackSearchReadReplyHeader(HgfsSearchReadInfo *info,    // IN: request info
                              size_t *payloadSize);        // OUT: size of packet

Bool
HgfsUnpackSetattrRequest(const void *packet,            // IN: request packet
                         size_t packetSize,             // IN: request packet size
                         HgfsOp op,                     // IN: requested operation
                         HgfsFileAttrInfo *attr,        // IN/OUT: getattr info
                         HgfsAttrHint *hints,           // OUT: setattr hints
                         const char **cpName,           // OUT: cpName
                         size_t *cpNameSize,            // OUT: cpName size
                         HgfsHandle *file,              // OUT: server file ID
                         uint32 *caseFlags);            // OUT: case-sensitivity flags

Bool
HgfsPackSetattrReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                     const void *packetHeader,   // IN: packet header
                     HgfsOp op,                  // IN: request type
                     size_t *payloadSize,        // OUT: size of packet
                     HgfsSessionInfo *session);  // IN: Session Info

Bool
HgfsUnpackCreateDirRequest(const void *packet,       // IN: HGFS packet
                           size_t packetSize,        // IN: size of packet
                           HgfsOp op,                // IN: requested operation
                           HgfsCreateDirInfo *info); // IN/OUT: info struct

Bool
HgfsPackCreateDirReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                       const void *packetHeader,   // IN: packet header
                       HgfsOp op,                  // IN: request type
                       size_t *payloadSize,        // OUT: size of packet
                       HgfsSessionInfo *session);  // IN: Session Info

Bool
HgfsPackQueryVolumeReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                         const void *packetHeader,  // IN: packet header
                         HgfsOp op,                 // IN: request type
                         uint64 freeBytes,          // IN: volume free space
                         uint64 totalBytes,         // IN: volume capacity
                         size_t *payloadSize,       // OUT: size of packet
                         HgfsSessionInfo *session); // IN: Session Info
Bool
HgfsUnpackQueryVolumeRequest(const void *packet,     // IN: HGFS packet
                             size_t packetSize,      // IN: request packet size
                             HgfsOp op,              // IN: request type
                             Bool *useHandle,        // OUT: use handle
                             const char **fileName,  // OUT: file name
                             size_t *fileNameLength, // OUT: file name length
                             uint32 *caseFlags,      // OUT: case sensitivity
                             HgfsHandle *file);      // OUT: Handle to the volume
Bool
HgfsUnpackSymlinkCreateRequest(const void *packet,        // IN: request packet
                               size_t packetSize,         // IN: request packet size
                               HgfsOp op,                 // IN: request type
                               Bool *srcUseHandle,        // OUT: use source handle
                               const char **srcFileName,  // OUT: source file name
                               size_t *srcFileNameLength, // OUT: source file name length
                               uint32 *srcCaseFlags,      // OUT: source case sensitivity
                               HgfsHandle *srcFile,       // OUT: source file handle
                               Bool *tgUseHandle,         // OUT: use target handle
                               const char **tgFileName,   // OUT: target file name
                               size_t *tgFileNameLength,  // OUT: target file name length
                               uint32 *tgCaseFlags,       // OUT: target case sensitivity
                               HgfsHandle *tgFile);        // OUT: target file handle
Bool
HgfsUnpackWriteWin32StreamRequest(const void *packet,   // IN: HGFS packet
                                  size_t packetSize,    // IN: size of packet
                                  HgfsOp op,            // IN: request type
                                  HgfsHandle *file,     // OUT: file to write to
                                  const char **payload, // OUT: data to write
                                  size_t *requiredSize, // OUT: size of data
                                  Bool *doSecurity);    // OUT: restore sec.str.
Bool
HgfsUnpackCreateSessionRequest(const void *packetHeader,     // IN: request packet
                               size_t packetSize,            // IN: size of packet
                               HgfsOp op,                    // IN: request type
                               HgfsCreateSessionInfo *info); // IN/OUT: info struct
Bool
HgfsPackWriteWin32StreamReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                              const void *packetHeader,   // IN: packet headert
                              HgfsOp op,                  // IN: request type
                              uint32 actualSize,          // IN: amount written
                              size_t *payloadSize,        // OUT: size of packet
                              HgfsSessionInfo *session);  // IN:Session Info

Bool
HgfsUnpackCloseRequest(const void *packet,    // IN: request packet
                       size_t packetSize,     // IN: request packet size
                       HgfsOp op,             // IN: request type
                       HgfsHandle *file);     // OUT: Handle to close
Bool
HgfsUnpackSearchOpenRequest(const void *packet,      // IN: HGFS packet
                            size_t packetSize,       // IN: request packet size
                            HgfsOp op,               // IN: request type
                            const char **dirName,    // OUT: directory name
                            size_t *dirNameLength,   // OUT: name length
                            uint32 *caseFlags);      // OUT: case flags
Bool
HgfsPackCloseReply(HgfsPacket *packet,          // IN/OUT: Hgfs Packet
                   const void *packetHeader,    // IN: packet header
                   HgfsOp op,                   // IN: request type
                   size_t *payloadSize,         // OUT: size of packet
                   HgfsSessionInfo *session);   // IN: Session Info

Bool
HgfsUnpackSearchCloseRequest(const void *packet,    // IN: request packet
                             size_t packetSize,     // IN: request packet size
                             HgfsOp op,             // IN: request type
                             HgfsHandle *file);     // OUT: Handle to close
Bool
HgfsPackSearchOpenReply(HgfsPacket *packet,          // IN/OUT: Hgfs Packet
                        const void *packetHeader,    // IN: packet header
                        HgfsOp op,                   // IN: request type
                        HgfsHandle search,           // IN: search handle
                        size_t *packetSize,          // OUT: size of packet
                        HgfsSessionInfo *session);   // IN: Session Info
Bool
HgfsPackSearchCloseReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                         const void *packetHeader,   // IN: packet header
                         HgfsOp op,                  // IN: request type
                         size_t *packetSize,         // OUT: size of packet
                         HgfsSessionInfo *session);  // IN: Session Info
Bool
HgfsPackWriteReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                   const void *packetHeader,     // IN: packet header
                   HgfsOp op,                    // IN: request type
                   uint32 actualSize,            // IN: number of bytes that were written
                   size_t *payloadSize,          // OUT: size of packet
                   HgfsSessionInfo *session);    // IN: Session info
Bool
HgfsUnpackReadRequest(const void *packet,     // IN: HGFS request
                      size_t packetSize,      // IN: request packet size
                      HgfsOp  op,             // IN: request type
                      HgfsHandle *file,       // OUT: Handle to close
                      uint64 *offset,         // OUT: offset to read from
                      uint32 *length);        // OUT: length of data to read
Bool
HgfsUnpackWriteRequest(const void *writeRequest,// IN: HGFS write request params
                       size_t writeRequestSize, // IN: write request params size
                       HgfsOp writeOp,          // IN: request version
                       HgfsHandle *file,        // OUT: Handle to write to
                       uint64 *offset,          // OUT: offset to write to
                       uint32 *length,          // OUT: length of data to write
                       HgfsWriteFlags *flags,   // OUT: write flags
                       const void **data);      // OUT: data to be written
Bool
HgfsPackCreateSessionReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                           const void *packetHeader,  // IN: packet header
                           size_t *payloadSize,       // OUT: size of packet
                           HgfsSessionInfo *session); // IN: Session Info
Bool
HgfsPackDestroySessionReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                            const void *packetHeader,  // IN: packet header
                            size_t *payloadSize,       // OUT: size of packet
                            HgfsSessionInfo *session); // IN: Session Info
void
HgfsServerGetDefaultCapabilities(HgfsOpCapability *capabilities, // OUT:
                                 uint32 *numberOfCapabilities);  // OUT:
Bool
HgfsUnpackSetWatchRequest(const void *packet,      // IN: HGFS packet
                          size_t packetSize,       // IN: request packet size
                          HgfsOp op,               // IN: requested operation
                          Bool *useHandle,         // OUT: handle or cpName
                          const char **cpName,     // OUT: cpName
                          size_t *cpNameSize,      // OUT: cpName size
                          uint32 *flags,           // OUT: flags for the new watch
                          uint32 *events,          // OUT: event filter
                          HgfsHandle *dir,         // OUT: direrctory handle
                          uint32 *caseFlags);      // OUT: case-sensitivity flags
Bool
HgfsPackSetWatchReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                      const void *packetHeader,     // IN: packet header
                      HgfsOp     op,                // IN: operation code
                      HgfsSubscriberHandle watchId, // IN: new watch id
                      size_t *payloadSize,          // OUT: size of packet
                      HgfsSessionInfo *session);    // IN: Session info
Bool
HgfsUnpackRemoveWatchRequest(const void *packet,             // IN: HGFS packet
                             size_t packetSize,              // IN: packet size
                             HgfsOp op,                      // IN: operation code
                             HgfsSubscriberHandle *watchId); // OUT: watch Id
Bool
HgfsPackRemoveWatchReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                         const void *packetHeader,     // IN: packet header
                         HgfsOp     op,                // IN: operation code
                         size_t *payloadSize,          // OUT: size of packet
                         HgfsSessionInfo *session);    // IN: Session info
size_t
HgfsPackCalculateNotificationSize(char const *shareName, // IN: shared folder name
                                  char *fileName);       // IN: file name
Bool
HgfsPackChangeNotificationRequest(void *packet,                    // IN/OUT: Hgfs Packet
                                  HgfsSubscriberHandle subscriber, // IN: watch
                                  char const *shareName,           // IN: share name
                                  char *fileName,                  // IN: file name
                                  uint32 mask,                     // IN: event mask
                                  uint32 notifyFlags,              // IN: notify flags
                                  HgfsSessionInfo *session,        // IN: session
                                  size_t *bufferSize);             // IN/OUT: packet size


#endif // ifndef _HGFS_SERVER_PARAMETERS_H_
