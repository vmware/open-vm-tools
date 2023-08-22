/*********************************************************
 * Copyright (c) 2003-2021, 2023 VMware, Inc. All rights reserved.
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
 * vixCommands.h --
 *
 * Defines used when Vix crosses various IPC boundaries.
 */

#ifndef _VIX_COMMANDS_H_
#define _VIX_COMMANDS_H_

#include "vixOpenSource.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * These describe the format of the message objects.
 * This will change when the client/vmx support different
 * structures for the message header. Hopefully, that won't
 * happen.
 */
#define VIX_COMMAND_MAGIC_WORD        0xd00d0001
#define VIX_COMMAND_MESSAGE_VERSION   5

/*
 * These give upper bounds for how big any VIX IPC meesage
 * should be. There are for confidence checks and to ignore maliciously
 * large messages that may be part of an DoS attack. The may need to
 * be revised if large messages are added to the protocol.
 */

#define VIX_COMMAND_MAX_SIZE           (16 * 1024 * 1024)
#define VIX_COMMAND_MAX_REQUEST_SIZE   65536

/*
 * We don't want to allow guest ops commands with input size too large.
 * Limit it to the max request size with enough room for the credentials.
 * Check bugs 824773, 926819 for more details.
 */
#define VIX_COMMAND_MAX_USER_INPUT_SIZE (VIX_COMMAND_MAX_REQUEST_SIZE - 5000)

/*
 * The types of credential we can pass with any request.
 */
#define VIX_USER_CREDENTIAL_NONE                             0
#define VIX_USER_CREDENTIAL_NAME_PASSWORD                    1
#define VIX_USER_CREDENTIAL_ANONYMOUS                        2
#define VIX_USER_CREDENTIAL_ROOT                             3
#define VIX_USER_CREDENTIAL_NAME_PASSWORD_OBFUSCATED         4
#define VIX_USER_CREDENTIAL_CONSOLE_USER                     5
#define VIX_USER_CREDENTIAL_HOST_CONFIG_SECRET               6
#define VIX_USER_CREDENTIAL_HOST_CONFIG_HASHED_SECRET        7
#define VIX_USER_CREDENTIAL_NAMED_INTERACTIVE_USER           8
#define VIX_USER_CREDENTIAL_TICKETED_SESSION                 9
#define VIX_USER_CREDENTIAL_SSPI                             10
#define VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN                11
#define VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN_HOST_VERIFIED  12

#define VIX_SHARED_SECRET_CONFIG_USER_NAME          "__VMware_Vix_Shared_Secret_1__"

/*
 * Base64 encoded value of string %HOST_VERIFIED_SIGNATURE%
 */
#define VIX_BASE64_ENCODED_HOST_VERIFIED_SIGNATURE_VALUE    "JUhPU1RfVkVSSUZJRURfU0lHTkFUVVJFJQA="


/*
 * This is the port for the server side remote Vix component
 */
#define VIX_SERVER_PORT          61525
#define VIX_TOOLS_SOCKET_PORT    61526

/*
 * These are the flags set in the commonFlags field.
 */
enum VixCommonCommandOptionValues {
   VIX_COMMAND_REQUEST                       = 0x01,
   VIX_COMMAND_REPORT_EVENT                  = 0x02,
   VIX_COMMAND_FORWARD_TO_GUEST              = 0x04,
   VIX_COMMAND_GUEST_RETURNS_STRING          = 0x08,
   VIX_COMMAND_GUEST_RETURNS_INTEGER_STRING  = 0x10,
   /* DEPRECATED VIX_COMMAND_GUEST_RETURNS_ENCODED_STRING  = 0x20, */
   VIX_COMMAND_GUEST_RETURNS_PROPERTY_LIST   = 0x40,
   VIX_COMMAND_GUEST_RETURNS_BINARY          = 0x80,
   // We cannot add more constants here. This is stored in a uint8,
   // so it is full. Use requestFlags or responseFlags.
};


/*
 * These are the flags set in the request Flags field.
 */
enum {
   VIX_REQUESTMSG_ONLY_RELOAD_NETWORKS                = 0x001,
   VIX_REQUESTMSG_RETURN_ON_INITIATING_TOOLS_UPGRADE  = 0x002,
   VIX_REQUESTMSG_RUN_IN_ANY_VMX_STATE                = 0x004,
   VIX_REQUESTMSG_REQUIRES_INTERACTIVE_ENVIRONMENT    = 0x008,
   VIX_REQUESTMSG_INCLUDES_AUTH_DATA_V1               = 0x010,
   VIX_REQUESTMSG_REQUIRES_VMDB_NOTIFICATION          = 0x020,
   VIX_REQUESTMSG_ESCAPE_XML_DATA                     = 0x040,
   VIX_REQUESTMSG_HAS_HASHED_SHARED_SECRET            = 0x080,
   VIX_REQUESTMSG_VIGOR_COMMAND                       = 0x100,
};


/*
 * These are the flags set in responseFlags.
 */
enum VixResponseFlagsValues {
   VIX_RESPONSE_SOFT_POWER_OP             = 0x0001,
   VIX_RESPONSE_EXTENDED_RESULT_V1        = 0x0002,
   VIX_RESPONSE_TRUNCATED                 = 0x0004,
   VIX_RESPONSE_FSR                       = 0x0008,
   VIX_RESPONSE_VMDB_NOTIFICATION_POSTED  = 0x0010,
   VIX_RESPONSE_VIGOR_COMMAND             = 0x0020,
};


/*
 * This is the header for one message, either a request or a
 * response, and sent either to or from the VMX.
 *
 * Every message has 3 regions:
 *
 *  -------------------------------------
 *  |   Header  |  Body  |  Credential  |
 *  -------------------------------------
 *
 * The credential and the body may either or both be empty.
 * The 3 regions always appear in this order. First the header, then a body 
 * if there is one, then a credential if there is one.
 * There should be no gaps between these regions. New regions are added
 * to the end. This means the lengths can also be used to compute
 * offsets to the regions.
 *
 * The length of the headers, the credential, and the body are all stored in
 * the common header. This should allow parsing code to receive complete
 * messages even if it does not understand them.
 *
 * Currently that the credential is only used for a Request. It is 
 * currently empty for a response.
 *
 */
#pragma pack(push, 1)
typedef struct VixMsgHeader {
   uint32   magic;
   uint16   messageVersion;

   uint32   totalMessageLength;
   uint32   headerLength;
   uint32   bodyLength;
   uint32   credentialLength;

   uint8    commonFlags;
} VixMsgHeader;
#pragma pack(pop)


/*
 * These are the headers for a single request, response, or event.
 * In theory, either the VMX or the client may issue a request
 * to the other.  In practice, legacy foundry clients can only
 * accept response messages from the VMX, not requests.  Because of
 * this, an event message is a special kind of response message.
 */
#pragma pack(push, 1)
typedef struct VixCommandRequestHeader {
   VixMsgHeader      commonHeader;

   uint32            opCode;
   uint32            requestFlags;

   uint32            timeOut;

   uint64            cookie;
   uint32            clientHandleId; // for remote case

   uint32            userCredentialType;
} VixCommandRequestHeader;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixCommandResponseHeader {
   VixMsgHeader   commonHeader;

   uint64         requestCookie;

   uint32         responseFlags;

   uint32         duration;

   uint32         error;
   uint32         additionalError;
   uint32         errorDataLength;
} VixCommandResponseHeader;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgEventHeader {
   VixCommandResponseHeader   responseHeader;

   int32                      eventType;
} VixMsgEventHeader;
#pragma pack(pop)


/*
 * A trivial request that is just a generic
 * response header (it has no body).
 */
#pragma pack(push, 1)
typedef struct VixMsgTrivialRequest {
   VixCommandRequestHeader   header;
} VixMsgTrivialRequest;
#pragma pack(pop)


/*
 * A trivial event that is just a generic
 * event header (it has no body).
 */

#pragma pack(push, 1)
typedef struct VixMsgTrivialEvent {
   VixMsgEventHeader          eventHeader;
} VixMsgTrivialEvent;
#pragma pack(pop)


/*
 * **********************************************************
 * This is a generic progress update from the VMX.
 * The VMX may send several of these before sending a final response 
 * message. These only report progress, they do not mean the job
 * has completed. These messages are identified by the
 * VIX_COMMAND_REPORT_EVENT flag in the commonFlags field and
 * VIX_EVENTTYPE_JOB_PROGRESS as the eventType.
 */

#pragma pack(push, 1)
typedef struct VixMsgProgressEvent {
   VixMsgEventHeader          eventHeader;

   int64                      workToDo;
   int64                      workDone;
} VixMsgProgressEvent;
#pragma pack(pop)


/*
 * This is an event sent from the VMX to all clients when some property changes.
 * It may be used for any runtime property.
 */
#pragma pack(push, 1)
typedef struct VixMsgPropertyChangedEvent {
   VixMsgEventHeader        eventHeader;
   int                      options;
   uint32                   propertyListSize;
} VixMsgPropertyChangedEvent;
#pragma pack(pop)



/*
 * **********************************************************
 * This is a userName and password pair.
 */
#pragma pack(push, 1)
typedef struct VixCommandNamePassword {
   uint32    nameLength;
   uint32    passwordLength;
} VixCommandNamePassword;
#pragma pack(pop)

/*
 * **********************************************************
 * This is a ticketed session for authentication.
 */
#pragma pack(push, 1)
typedef struct VixCommandTicketedSession {
   uint32    ticketLength;
} VixCommandTicketedSession;
#pragma pack(pop)

/*
 * **********************************************************
 * This is a SSPI token for acquiring credentials
 */
#pragma pack(push, 1)
typedef struct VixCommandSSPI {
   uint32    tokenLength;
} VixCommandSSPI;
#pragma pack(pop)


/*
 * **********************************************************
 * This is a SAML bearer token with optional userName to specify
 * an IdProvider store.
 */
#pragma pack(push, 1)
typedef struct VixCommandSAMLToken {
   uint32    tokenLength;
   uint32    nameLength;
} VixCommandSAMLToken;
#pragma pack(pop)

/*
 * **********************************************************
 * Basic power op request. The response is just a generic
 * response header (it has no body).
 */
#pragma pack(push, 1)
typedef struct VixMsgPowerOpRequest {
   VixCommandRequestHeader   header;
   VixVMPowerOpOptions       powerOpOptions;
   /*
    * Starting in Workstation 7.0, a serialized property list buffer
    * can be appended here. This was originally used for augmenting
    * poweroff to support revert to snapshot upon poweroff functionality.
    */
} VixMsgPowerOpRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Get/Set Properties Request
 */

#pragma pack(push, 1)
typedef struct VixMsgGetVMStateResponse {
   VixCommandResponseHeader   header;
   uint32                     bufferSize;
   // This is followed by the buffer of serialized properties
} VixMsgGetVMStateResponse;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgSetVMStateRequest {
   VixCommandRequestHeader header;
   uint32                  bufferSize;
   // This is followed by the buffer of serialized properties
} VixMsgSetVMStateRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgAuthDataV1 {
   int64  nonce;
   uint32 sequenceNumber;
   uint8  hashValue[32];
} VixMsgAuthDataV1;
#pragma pack(pop)



/*
 * **********************************************************
 * Basic reload state request. The response is just a generic
 * response header (it has no body).
 */
#pragma pack(push, 1)
typedef struct VixMsgReloadVMStateRequest {
   VixCommandRequestHeader   header;
   // This is followed by an array of VixMsgConfigurationObjectType objects
} VixMsgReloadVMStateRequest;
#pragma pack(pop)


/*
 * This is a prefix to a configuration object. The current supported
 * types are defined below in the VixCommonConfigObjectType enum.
 * Following each object type struct is the specific object. Currently,
 * we support:
 * 
 *    VIX_NETWORK_SETTING_CONFIG   - VixMsgNICBandwidth
 */
#pragma pack(push, 1)
typedef struct VixMsgConfigurationObjectType {
   int32    configurationType;
   uint32   objectSize;
} VixMsgConfigurationObjectType;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgNICBandwidth {
   Bool        validNICNum;
   int32       nicNum;
   char        pvnGUID[64];

   uint32      totalBandwidth;
   uint32      maxSendBandwidth;
   uint32      maxReceiveBandwidth;

   uint32      packetLossPattern;
   uint32      packetLossRate;
   uint32      packetLossMinBurstDuration;
   uint32      packetLossMaxBurstDuration;

   uint32      minLatency;
   uint32      maxLatency;

   uint32      options;
} VixMsgNICBandwidth;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgLANSegmentConfiguration {
   VixMsgConfigurationObjectType   configHeader;
   VixMsgNICBandwidth              lanSegment;
} VixMsgLANSegmentConfiguration;
#pragma pack(pop)

/*
 * These are options to the bandwidth commands.
 */
enum VixMsgPacketLossType {
   // packetLossPattern values
   VIX_PACKETLOSS_RANDOM   = 1,
};

/*
 * These are the types of configuration objects we can send
 * to a VIX_COMMAND_RELOAD_VM command.
 */
enum VixMsgConfigObjectType {
   VIX_LAN_SEGMENT_SETTING_CONFIG   = 1,
};

/*
 * **********************************************************
 * Wait for tools request. The response is just a generic
 * response header (it has no body).
 */
#pragma pack(push, 1)
typedef struct VixMsgWaitForToolsRequest {
   VixCommandRequestHeader header;
   int32                   timeoutInSeconds;
   int32                   minVersion;
} VixMsgWaitForToolsRequest;
#pragma pack(pop)



/*
 * **********************************************************
 * Run a program on the guest.
 */
#pragma pack(push, 1)
typedef struct VixMsgRunProgramRequest {
   VixCommandRequestHeader header;

   int32                   runProgramOptions;
   uint32                  programNameLength;
   uint32                  commandLineArgsLength;
} VixMsgRunProgramRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgOldRunProgramResponse {
   VixCommandResponseHeader   header;

   int32                      exitCode;
   VmTimeType                 deltaTime;
} VixMsgOldRunProgramResponse;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgRunProgramResponse {
   VixCommandResponseHeader   header;

   int32                      exitCode;
   VmTimeType                 deltaTime;

   int64                      pid;
   uint32                     stdOutLength;
   uint32                     stdErrLength;
} VixMsgRunProgramResponse;
#pragma pack(pop)


/*
 * **********************************************************
 * Install VMware tools.
 */
#pragma pack(push, 1)
typedef struct VixMsgInstallToolsRequest {
   VixCommandRequestHeader header;

   int32                   installOptions;
   uint32                  commandLineArgsLength;
} VixMsgInstallToolsRequest;
#pragma pack(pop)



/*
 * **********************************************************
 * Send keystrokes to the guest.
 */

enum VixKeyStrokeCharType {
   VIX_KEYSTROKE_SCANCODE     = 1,
   VIX_KEYSTROKE_TEXT_CHAR    = 2,
};

enum VixKeyStrokeModifiers {
   VIX_KEYSTROKE_MODIFIER_KEY_DOWN          = 0x01,
   VIX_KEYSTROKE_MODIFIER_KEY_UP            = 0x02,
   VIX_KEYSTROKE_MODIFIER_CONTROL           = 0x04,
   VIX_KEYSTROKE_MODIFIER_SHIFT             = 0x08,
   VIX_KEYSTROKE_MODIFIER_ALT               = 0x10,
   VIX_KEYSTROKE_MODIFIER_KEY_DOWN_ONLY     = 0x80,
   VIX_KEYSTROKE_MODIFIER_KEY_UP_ONLY       = 0x100,
};


#pragma pack(push, 1)
typedef struct VixMsgKeyStroke {
   int32                   modifier;
   int32                   scanCode;
   int32                   duration;
   int32                   delayAfterKeyUp;
   int32                   repeat;
} VixMsgKeyStroke;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgSendKeyStrokesRequest {
   VixCommandRequestHeader header;

   int32                   keyStrokeType;
   int32                   options;
   int64                   targetPid;
   int32                   numKeyStrokes;
   uint32                  windowNameLength;
} VixMsgSendKeyStrokesRequest;
#pragma pack(pop)

/*
 * send a mouse event to the guest
 */

#pragma pack(push, 1)
typedef struct VixMsgSendMouseEventRequest {
   VixCommandRequestHeader header;

   int16                    x;
   int16                    y;
   int16                    buttons;
   int32                    options;
} VixMsgSendMouseEventRequest;
#pragma pack(pop)




/*
 * **********************************************************
 * Read or write the registry on the guest.
 */
#pragma pack(push, 1)
typedef struct VixMsgRegistryRequest {
   VixCommandRequestHeader header;

   uint32                  registryKeyLength;
   int32                   expectedRegistryKeyType;
   uint32                  dataToWriteSize;
} VixMsgRegistryRequest;
#pragma pack(pop)



/*
 * **********************************************************
 * Copy files between the host and the guest.
 */
#pragma pack(push, 1)
typedef struct VixCommandRenameFileRequest {
   VixCommandRequestHeader header;

   int32                   copyFileOptions;
   uint32                  oldPathNameLength;
   uint32                  newPathNameLength;
   uint32                  filePropertiesLength;
} VixCommandRenameFileRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixCommandRenameFileRequestEx {
   VixCommandRequestHeader header;

   int32                   copyFileOptions;
   uint32                  oldPathNameLength;
   uint32                  newPathNameLength;
   uint32                  filePropertiesLength;
   Bool                    overwrite;
} VixCommandRenameFileRequestEx;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixCommandHgfsSendPacket {
   VixCommandRequestHeader header;

   uint32                  hgfsPacketSize;
   int32                   timeout;
} VixCommandHgfsSendPacket;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgSetGuestFileAttributesRequest {
   VixCommandRequestHeader header;

   int32                  fileOptions;
   int64                  accessTime;
   int64                  modificationTime;
   int32                  ownerId;
   int32                  groupId;
   int32                  permissions;
   Bool                   hidden;
   Bool                   readOnly;
   uint32                 guestPathNameLength;
} VixMsgSetGuestFileAttributesRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Perform a simple operation (like delete or check for existence)
 * on a file or registry key on the guest.
 */
#pragma pack(push, 1)
typedef struct VixMsgSimpleFileRequest {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
} VixMsgSimpleFileRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgListDirectoryRequest {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
   int64                   offset;
} VixMsgListDirectoryRequest;
#pragma pack(pop)

enum VixListDirectoryOptions {
   VIX_LIST_DIRECTORY_USE_OFFSET = 0x01
};

#pragma pack(push, 1)
typedef struct VixMsgListFilesRequest {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
   uint32                  patternLength;
   int32                   index;
   int32                   maxResults;
   uint64                  offset;
} VixMsgListFilesRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixCommandInitiateFileTransferToGuestRequest {
   VixCommandRequestHeader header;

   int32                   options;
   uint32                  guestPathNameLength;
   Bool                    overwrite;
} VixCommandInitiateFileTransferToGuestRequest;
#pragma pack(pop)


/*
 * This is used to reply to several operations, like testing whether
 * a file or registry key exists on the client.
 */
#pragma pack(push, 1)
typedef struct VixMsgCheckExistsResponse {
   VixCommandResponseHeader   header;
   Bool                       exists;
} VixMsgCheckExistsResponse;
#pragma pack(pop)


/*
 * **********************************************************
 * Perform a create file operation (like createDir or moveFile)
 * on a file in the guest. This lets you pass in things like the initial file
 * properties.
 */
#pragma pack(push, 1)
typedef struct VixMsgCreateFileRequest {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
   uint32                  filePropertiesLength;
} VixMsgCreateFileRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgCreateFileRequestEx {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
   uint32                  filePropertiesLength;
   Bool                    createParentDirectories;
} VixMsgCreateFileRequestEx;
#pragma pack(pop)


/*
 * **********************************************************
 * Hot extend a disk in a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotExtendDiskRequest {
   VixCommandRequestHeader header;
   int32                    hotDiskOptions;
   uint32                   typeLength;
   int32                    adapterNum;
   int32                    targetNum;
   uint64                   newNumSectors;
} VixMsgHotExtendDiskRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Hot plug CPU in a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotPlugCPURequest {
   VixCommandRequestHeader header;
   uint32                  newNumCPU;
} VixMsgHotPlugCPURequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Hot plug memory in a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotPlugMemoryRequest {
   VixCommandRequestHeader header;
   uint32                  newSizeMb;
} VixMsgHotPlugMemoryRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Hot add device in a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotAddDeviceRequest {
   VixCommandRequestHeader header;
   int32                   deviceType;
   uint32                  devicePropsBufferSize;
   int32                   backingType;
   uint32                  backingPropsBufferSize;
} VixMsgHotAddDeviceRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgHotAddDeviceResponse {
   VixCommandResponseHeader   header;
   int32                      adapterNum;
   int32                      targetNum;
} VixMsgHotAddDeviceResponse;
#pragma pack(pop)


/*
 * **********************************************************
 * Hot remove device in a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotRemoveDeviceRequest {
   VixCommandRequestHeader header;
   int32                   deviceType;
   uint32                  devicePropsBufferSize;
} VixMsgHotRemoveDeviceRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Change monitor type of a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotChangeMonitorTypeRequest {
   VixCommandRequestHeader header;
   int32                   monitorType;
} VixMsgHotChangeMonitorTypeRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Hot plug begin batch.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotPlugBeginBatchRequest {
   VixCommandRequestHeader header;
   int32                   flags;
} VixMsgHotPlugBeginBatchRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Hot plug commit batch.
 */
#pragma pack(push, 1)
typedef struct VixMsgHotPlugCommitBatchRequest {
   VixCommandRequestHeader header;
   int32                   status;
} VixMsgHotPlugCommitBatchRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Transfer connection.  Besides fields here you are supposed to
 * receive file descriptor OOB.
 */
#pragma pack(push, 1)
typedef struct VixMsgTransferConnectionRequest {
   VixCommandRequestHeader header;
   Bool                    isPrivileged;
   uint32                  cryptoLength;
   uint32                  fdLength;
/* uint8                   cryptoData[]; */
/* uint8                   fdData[]; */
} VixMsgTransferConnectionRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Pass data.  Besides fields here you may receive also
 * file descriptor.  Data is just command which was pending
 * on original connection already transferred via
 * TransferConnectionRequest.
 */
#pragma pack(push, 1)
typedef struct VixMsgTransferRequestRequest {
   VixCommandRequestHeader header;
   uint32                  dataLength;
   uint32                  fdLength;
/* uint8                   data[]; */
/* uint8                   fdData[]; */
} VixMsgTransferRequestRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Pass final data.  Besides fields here you may receive also
 * file descriptor.  Data is just what was already received
 * on the socket passed by TransferConnectionRequest.
 */
#pragma pack(push, 1)
typedef struct VixMsgTransferFinalDataRequest {
   VixCommandRequestHeader header;
   uint32                  dataLength;
   uint32                  fdLength;
/* uint8                   data[]; */
/* uint8                   fdData[]; */
} VixMsgTransferFinalDataRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Create a snapshot of a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgCreateSnapshotRequest {
   VixCommandRequestHeader    header;

   int32                      options;

   Bool                       powerOff;
   Bool                       saveDeviceState;

   uint32                     nameLength;
   uint32                     descriptionLength;
} VixMsgCreateSnapshotRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgCreateSnapshotResponse {
   VixCommandResponseHeader   header;
   int32                      snapshotUID;
} VixMsgCreateSnapshotResponse;
#pragma pack(pop)


/*
 * Several snapshot operations for a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgSnapshotRequest {
   VixCommandRequestHeader    header;

   int32                      options;
   int32                      snapshotId;
} VixMsgSnapshotRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgSnapshotUpdateEvent {
   VixMsgEventHeader          eventHeader;

   int32                      options;
   uint32                     propertyListLength;
   /*
    * This is followed by a serialized property list.
    */
} VixMsgSnapshotUpdateEvent;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgSnapshotMRURequest {
   VixCommandRequestHeader    header;

   int32                      snapshotId;
   int32                      maxMRU;
} VixMsgSnapshotMRURequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgSetSnapshotInfoRequest {
   VixCommandRequestHeader    header;

   int32                      snapshotId;
   int32                      clientFlags;
   int32                      numTierUIDs;

   uint32                     displayNameLength;
   uint32                     descriptionLength;
   uint32                     propertyListLength;
   uint32                     tierUIDListLength;

   /*
    * Followed by:
    *   displayName string
    *   description string
    *   serialized property list.
    */
} VixMsgSetSnapshotInfoRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgSetSnapshotInfoResponse {
   VixCommandResponseHeader    header;

   uint32                     propertyListLength;
} VixMsgSetSnapshotInfoResponse;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgRemoveBulkSnapshotRequest {
   VixCommandRequestHeader    header;

   int32                      options;
   int32                      numSnapshots;
   /*
    * This is followed by numSnapshots snapshotIDs.
    */
} VixMsgRemoveBulkSnapshotRequest;
#pragma pack(pop)

/*
 * Stop recording or playback of a snapshot event log.
 */
#pragma pack(push, 1)
typedef struct VixMsgVMSnapshotPauseRequest {
   VixCommandRequestHeader    header;

   int32                      options;
} VixMsgVMSnapshotPauseRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgDebuggerEvent {
   VixMsgEventHeader          eventHeader;

   int32                      blobLength;
   /*
    * This is followed by the blob buffer.
    */
} VixMsgDebuggerEvent;
#pragma pack(pop)


/*
 * **********************************************************
 * Shared folder operations.
 */
#pragma pack(push, 1)
typedef struct VixMsgSharedFolderRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   int32                     index;
   uint32                    shareNameLength;
   uint32                    hostPathNameLength;
} VixMsgSharedFolderRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgSharedFolderResponse {
   VixCommandResponseHeader      header;
   int32                         numSharedFolders;
} VixMsgSharedFolderResponse;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgGetSharedFolderInfoResponse {
   VixCommandResponseHeader   header;

   uint32                     shareNameLength;
   uint32                     hostPathNameLength;
   int32                      sharedFolderFlags;
} VixMsgGetSharedFolderInfoResponse;
#pragma pack(pop)


/*
 * Add or change a shared folder request.
 */
#pragma pack(push, 1)
typedef struct VixMsgSetSharedFolderRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   uint32                    shareNameLength;
   uint32                    hostPathNameLength;
} VixMsgSetSharedFolderRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Capture the screen of a VM
 */

#pragma pack(push, 1)
typedef struct VixMsgCaptureScreenRequest {
   VixCommandRequestHeader header;
   
   int32                   format;  // Identifies the requested data format.
   int32                    maxSize; // Max data response size in bytes
                                    //    (-1 is any size)

   int32                    captureScreenOptions;
} VixMsgCaptureScreenRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgCaptureScreenResponse {
   VixCommandResponseHeader header;
   
   int32                   format; // Format of the data in the response.
   uint32                  dataOffset; // Relative to the address of this struct.
} VixMsgCaptureScreenResponse;
#pragma pack(pop)

/*
 * **********************************************************
 * Run a script in the guest.
 */
#pragma pack(push, 1)
typedef struct VixMsgRunScriptRequest {
   VixCommandRequestHeader header;

   int32                   scriptOptions;

   uint32                  interpreterNameLength;
   uint32                  scriptLength;
   uint32                  propertiesLength;
} VixMsgRunScriptRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * An unsupported command. This is used to test future versions
 * of the API sending us commands we don't recognize.
 */
#pragma pack(push, 1)
typedef struct VixUnsupportedCommandRequest {
   VixCommandRequestHeader   header;
   char                      junk[2053];
} VixUnsupportedCommandRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Create a session key between the client and the VMX.
 */
#pragma pack(push, 1)
typedef struct VixCommandMakeSessionKeyRequest {
   VixCommandRequestHeader   header;

   int32                     keyOptions;
   int32                     timeout;
   uint32                    responseKeyLength;
   int32                     responseKeyCypherType;
   int32                     cypherType;
} VixCommandMakeSessionKeyRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixCommandMakeSessionKeyResponse {
   VixCommandResponseHeader     header;

   int32                        keyOptions;
   int32                        timeout;
   uint32                       keyLength;
   int32                        cypherType;
} VixCommandMakeSessionKeyResponse;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixCommandGenerateNonceResponse {
   VixCommandResponseHeader     header;

   int64                        nonce;
} VixCommandGenerateNonceResponse;
#pragma pack(pop)


enum {
   VIX_CYPHERTYPE_NONE        = 0,
   VIX_CYPHERTYPE_DEFAULT     = 1,
};


/*
 * **********************************************************
 * Force a guest process to quit.
 */

#pragma pack(push, 1)
typedef struct VixCommandKillProcessRequest {
   VixCommandRequestHeader    header;

   uint64                     pid;
   uint32                     options;
} VixCommandKillProcessRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Read and write variables like guest variables and config values.
 */
#pragma pack(push, 1)
typedef struct VixMsgReadVariableRequest {
   VixCommandRequestHeader header;

   int32                   variableType;
   int32                   options;
   uint32                  nameLength;
} VixMsgReadVariableRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgReadVariableResponse {
   VixCommandResponseHeader   header;

   int32                      valueType;
   int32                      valueProperties;
   uint32                     valueLength;
} VixMsgReadVariableResponse;
#pragma pack(pop)


/*
 * Several snapshot operations for a running VM.
 */
#pragma pack(push, 1)
typedef struct VixMsgWriteVariableRequest {
   VixCommandRequestHeader header;

   int32                   variableType;
   int32                   options;

   uint32                  nameLength;
   uint32                  valueLength;
} VixMsgWriteVariableRequest;
#pragma pack(pop)



/*
 * **********************************************************
 * Perform a create file operation (like createDir or moveFile)
 * on a file in the guest. This lets you pass in things like the initial file
 * properties.
 */
#pragma pack(push, 1)
typedef struct VixMsgCreateTempFileRequest {
   VixCommandRequestHeader header;

   int32                   options;
   uint32                  propertyNameLength;
   uint32                  filePrefixLength;
   uint32                  fileSuffixLength;
} VixMsgCreateTempFileRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgCreateTempFileRequestEx {
   VixCommandRequestHeader header;

   int32                   options;
   uint32                  filePrefixLength;
   uint32                  fileSuffixLength;
   uint32                  directoryPathLength;
   uint32                  propertyListLength;
} VixMsgCreateTempFileRequestEx;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
   uint32                  filePropertiesLength;
   Bool                    recursive;
} VixMsgDeleteDirectoryRequest;
#pragma pack(pop)

/*
 * **********************************************************
 * Connect/Disconnect device request. The response is just a generic
 * response header (it has no body).
 */
#pragma pack(push, 1)
typedef struct VixMsgConnectDeviceRequest {
   VixCommandRequestHeader    header;
   int32                      options;
   Bool                       connected;
   uint32                     nameLength;
} VixMsgConnectDeviceRequest;
#pragma pack(pop)

/*
 * **********************************************************
 * Get the state of a virtual device.
 */
#pragma pack(push, 1)
typedef struct VixMsgGetDeviceStateRequest {
   VixCommandRequestHeader header;

   int32                   options;
   uint32                  nameLength;
} VixMsgGetDeviceStateRequest;
#pragma pack(pop)


/*
 * This is used to reply to IsDeviceConnected operations.
 */
#pragma pack(push, 1)
typedef struct VixMsgGetDeviceStateResponse {
   VixCommandResponseHeader   header;
   Bool                       connected;
   int32                      stateFlags;
   // Maybe capacity and percent allocated?
} VixMsgGetDeviceStateResponse;
#pragma pack(pop)


/*
 * **********************************************************
 * Enable/disable all shared folders on this VM. The response
 * is just a generic response header (it has no body).
 */
#pragma pack(push, 1)
typedef struct VixMsgEnableSharedFoldersRequest {
   VixCommandRequestHeader   header;
   Bool                      enabled;
   int32                     sharedFolderOptions;
} VixMsgEnableSharedFoldersRequest;
#pragma pack(pop)


/*
 * **********************************************************
 * Mount volumes in the guest.
 */

enum VixMountOptions {
   VIX_MOUNT_ALL              = 0x0001,
   VIX_MOUNT_REMOUNT_FIRST    = 0x0002,
};


#pragma pack(push, 1)
typedef struct VixMsgMountHGFSRequest {
   VixCommandRequestHeader header;

   int32                   mountOptions;
   int32                   mountType;

   /* The str path list has the form "host1\0dest1\0host2\0dest2\0host3\0dest3\0\0" */
   uint32                  pathListLength;
} VixMsgMountHGFSRequest;
#pragma pack(pop)


/*
 * Get guest networking config
 */
#pragma pack(push, 1)
typedef struct VixMsgGetGuestNetworkingConfigRequest {
   VixCommandRequestHeader   header;

   int32                     options;
} VixMsgGetGuestNetworkingConfigRequest;
#pragma pack(pop)


/*
 * Set guest networking config
 */
#pragma pack(push, 1)
typedef struct VixMsgSetGuestNetworkingConfigRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   uint32                    bufferSize;
} VixMsgSetGuestNetworkingConfigRequest;
#pragma pack(pop)


/*
 * Query VMX performance data
 */
#pragma pack(push, 1)
typedef struct VixMsgGetPerformanceDataRequest {
   VixCommandRequestHeader   header;

   // unused for now, but left for future expansion in case we
   // get such a large list that we want to pass the desired properties.
   int32                     options;
   uint32                    sizeOfPropertyList;
   // This is followed by the buffer of properties we wish to fetch
} VixMsgGetPerformanceDataRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgGetPerformanceDataResponse {
   VixCommandResponseHeader   header;
   uint32                     bufferSize;
   // This is followed by the buffer of serialized properties
} VixMsgGetPerformanceDataResponse;
#pragma pack(pop)


/*
 * Run a program in guest with (VI version with more args)
 */
#pragma pack(push, 1)
typedef struct VixMsgStartProgramRequest {
   VixCommandRequestHeader   header;

   Bool   startMinimized;
   uint32 programPathLength;
   uint32 argumentsLength;
   uint32 workingDirLength;
   uint32 numEnvVars;
   uint32 envVarLength;

   // This is followed by the buffer of the args
} VixMsgStartProgramRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgListProcessesExRequest {
   VixCommandRequestHeader   header;

   // if we need to make multiple trips, this is the key used to identify
   // the result being processed
   uint32 key;

   // if we need to make multiple trips, this is the offset in the reply
   // from which to send the next chunk
   uint32 offset;
   uint32 numPids;

   // This is followed by the list of uint64s
} VixMsgListProcessesExRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgReadEnvironmentVariablesRequest {
   VixCommandRequestHeader   header;

   uint32 numNames;
   uint32 namesLength;

   // This is followed by the list of NUL-terminated names
} VixMsgReadEnvironmentVariablesRequest;
#pragma pack(pop)

/* IdProvider support */

#pragma pack(push, 1)
typedef struct VixMsgAddAliasRequest {
   VixCommandRequestHeader header;

   uint32                  options;

   uint32                  userNameLen;
   uint32                  pemCertLen;
   Bool                    addMapping;

   int32                   subjectType;    // one of VixGuestAuthSubjectType
   uint32                  subjectNameLen;
   uint32                  aliasCommentLen;

   /* Followed by the NUL-terminated string arguments. */
   /* char[]               userName; */
   /* char[]               pemCert; */
   /* char[]               subjectName; */
   /* char[]               aliasComment; */
} VixMsgAddAuthAliasRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgRemoveAuthAliasRequest {
   VixCommandRequestHeader header;

   uint32                  options;

   uint32                  userNameLen;
   uint32                  pemCertLen;

   // special case for RemoveAliasByCert:
   // if subjectType is NONE, then all aliases will be removed.
   int32                   subjectType;    // one of VixGuestAuthSubjectType
   uint32                  subjectNameLen;

   /* Followed by the NUL-terminated string arguments. */
   /* char[]               userName; */
   /* char[]               pemCert; */
   /* char[]               subjectName; */
} VixMsgRemoveAuthAliasRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgListAuthAliasesRequest {
   VixCommandRequestHeader header;

   uint32                  options;

   uint32                  userNameLen;

   /* char[]               userName; */
} VixMsgListAuthAliasesRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgListMappedAliasesRequest {
   VixCommandRequestHeader header;

   uint32                  options;
} VixMsgListMappedAliasesRequest;
#pragma pack(pop)

/*
 * Windows Registry Management Support.
 */
#pragma pack(push, 1)
typedef struct VixMsgCreateRegKeyRequest {
   VixCommandRequestHeader header;

   uint32 options;

   uint32 pathLength;
   uint32 wowBitness;
   Bool isVolatile;
   uint32 classTypeLength;

   /*
    * Followed by NUL-terminated string arguments.
    * char[] path;
    * char[] classType;
    */
} VixMsgCreateRegKeyRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgListRegKeysRequest {
   VixCommandRequestHeader header;

   uint32 options;

   /*
    * If we need multiple roundtrips, this is the index
    * used to identify the result being processed.
    */
   uint32 index;

   /*
    * If we need multiple roundtrips, this is the offset
    * in the reply from which to send the next chunk.
    */
   uint32 offset;

   uint32 pathLength;
   uint32 wowBitness;
   Bool recursive;
   uint32 matchPatternLength;

   /*
    * Followed by NUL-terminated string arguments.
    * char[] path;
    * char[] matchPattern;
    */
} VixMsgListRegKeysRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgDeleteRegKeyRequest {
   VixCommandRequestHeader header;

   uint32 options;

   uint32 pathLength;
   uint32 wowBitness;
   Bool recursive;

   /*
    * Followed by NUL-terminated string arguments.
    * char[] path;
    */
} VixMsgDeleteRegKeyRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgSetRegValueRequest {
   VixCommandRequestHeader header;

   uint32 options;

   uint32 pathLength;
   uint32 wowBitness;
   uint32 nameLength;
   uint32 dataBlobType;
   uint32 dataBlobLength;

   /*
    * Followed by NUL-terminated string arguments.
    * char[] path;
    * char[] name;
    *
    * Followed by a data blob of specified length
    * containing information of specified type.
    * void *dataBlob;
    */
} VixMsgSetRegValueRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgListRegValuesRequest {
   VixCommandRequestHeader header;

   uint32 options;

   /*
    * If we need multiple roundtrips, this is the index
    * used to identify the result being processed.
    */
   uint32 index;

   /*
    * If we need multiple roundtrips, this is the offset
    * in the reply from which to send the next chunk.
    */
   uint32 offset;

   uint32 pathLength;
   uint32 wowBitness;
   Bool expandStrings;
   uint32 matchPatternLength;

   /*
    * Followed by NUL-terminated string arguments.
    * char[] path;
    * char[] matchPattern;
    */
} VixMsgListRegValuesRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgDeleteRegValueRequest {
   VixCommandRequestHeader header;

   uint32 options;

   uint32 pathLength;
   uint32 wowBitness;
   uint32 nameLength;

   /*
    * Followed by NUL-terminated string arguments.
    * char[] path;
    * char[] name;
    */
} VixMsgDeleteRegValueRequest;
#pragma pack(pop)


/*
 * HOWTO: Adding a new Vix Command. Step 3.
 *
 * Add a new struct to pass over the control socket into the VMX.
 * You only need to do this if your command is manipulating a running
 * VM, but that is a common situation. If your command only manipulates
 * non-running VMs, then you can skip this.
 *
 * This particular command passes strings as both a param and a
 * result. This is the most general case, because it means that both
 * the request and response have a variable-length string on the end.
 * You can make a simpler request or response if it only passes integers
 * and so is fixed size.
 */

/*
 * **********************************************************
 * Sample Command.
 */

#pragma pack(push, 1)
typedef struct VixMsgSampleCommandRequest {
   VixCommandRequestHeader header;

   int32                   intArg;
   uint32                  strArgLength;
} VixMsgSampleCommandRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgSampleCommandResponse {
   VixCommandResponseHeader   header;

   int32                      intResult;
   uint32                     strResultLength;
} VixMsgSampleCommandResponse;
#pragma pack(pop)

// End of "HOWTO: Adding a new Vix Command. Step 3."


/*
 * **********************************************************
 *  Debugger related commands.
 */

#pragma pack(push, 1)
typedef struct VixMsgAttachDebuggerRequest {
   VixCommandRequestHeader   header;
   
   int32                     options;
   uint32                    propertyListBufferSize;
} VixMsgAttachDebuggerRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgAttachDebuggerResponse {
   VixCommandResponseHeader header;
   uint32   propertyListBufferSize;
} VixMsgAttachDebuggerResponse;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgIssueDebuggerCommandRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   uint32                    propertyListBufferSize;
   uint32                    debuggerBlobBufferSize;
} VixMsgIssueDebuggerCommandRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgIssueDebuggerCommandResponse {
   VixCommandResponseHeader header;
   uint32   propertyListBufferSize;
   uint32   debuggerBlobBufferSize;
} VixMsgIssueDebuggerCommandResponse;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgDetachDebuggerRequest {
   VixCommandRequestHeader   header;
  
   int32                     options;
   uint32                    propertyListBufferSize;
} VixMsgDetachDebuggerRequest;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct VixMsgDetachDebuggerResponse {
   VixCommandResponseHeader header;
   uint32   propertyListBufferSize;
} VixMsgDetachDebuggerResponse;
#pragma pack(pop)

/*
 * **********************************************************
 * VM Pause state change event format
 */

#pragma pack(push, 1)
typedef struct VixMsgPauseStateChangedEvent {
   VixMsgEventHeader          eventHeader;
   Bool                       paused;
} VixMsgPauseStateChangedEvent;
#pragma pack(pop)


/*
 * **********************************************************
 * Wait for a user action, such as a user logging into the guest.
 */

/*
 * Vix_WaitForUserActionInGuest Request
 * VIX_COMMAND_WAIT_FOR_USER_ACTION_IN_GUEST
 */

#pragma pack(push, 1)
typedef struct VixMsgWaitForUserActionRequest {
   VixCommandRequestHeader    header;

   int32                      userType;
   int32                      userAction;

   int32                      timeoutInSeconds;
   int32                      options;

   uint32                     userNameLength;
   uint32                     propertyBufferSize;

   // This is followed by:
   //    userName
   //    buffer of serialized properties
} VixMsgWaitForUserActionRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct VixMsgWaitForUserActionResponse {
   VixCommandRequestHeader    header;

   Bool                       actionHappened;

   uint32                     bufferSize;
   // This is followed by the buffer of serialized properties
} VixMsgWaitForUserActionResponse;
#pragma pack(pop)


/*
 * **********************************************************
 * List filesystems
 */

#pragma pack(push, 1)
typedef struct VixCommandListFileSystemsRequest {
   VixCommandRequestHeader    header;

   uint32                     options;
   uint32                     propertyListSize;
} VixCommandListFileSystemsRequest;
#pragma pack(pop)

/*
 * **********************************************************
 * Acquire Credentials.
 */

#pragma pack(push, 1)
typedef struct VixCommandAcquireCredentialsRequest {
   VixCommandRequestHeader    header;

   int64                      sessionID;
} VixCommandAcquireCredentialsRequest;
#pragma pack(pop)

/*
 * **********************************************************
 * A simple request packet that contains an options field and a
 * property list.
 */

#pragma pack(push, 1)
typedef struct VixCommandGenericRequest {
   VixCommandRequestHeader    header;

   uint32                     options;
   uint32                     propertyListSize;
   // This is followed by the buffer of serialized properties
} VixCommandGenericRequest;
#pragma pack(pop)

/*
 * The security classifications for async op types/op code. Each op code
 * is given a security category, and the VMX uses that category to determine
 * whether a client is allowed to perform the given command.
 */
typedef enum VixCommandSecurityCategory {
   
   /* The default for unknown commands */
   VIX_COMMAND_CATEGORY_UNKNOWN,
   
   /*
    * A command that should be executed in the guest OS by the VIX Tools.
    * component. These are allowed for all connection types.
    */
   VIX_COMMAND_CATEGORY_ALWAYS_ALLOWED,

   /*
    * A command that only allowed by privileged connections; in the VI
    * world this is means that only Hostd is allowed to perform these
    * commands.
    */
   VIX_COMMAND_CATEGORY_PRIVILEGED,

   /*
    * A command that may or may not be privileged. Usually, extra inspection
    * of the payload is required to make the determination. This should be
    * used sparingly, since must always be accompanied by "deep packet
    * inspection" code in the VMX (mainDispatch.c).
    */
   VIX_COMMAND_CATEGORY_MIXED,
} VixCommandSecurityCategory;

/*
 * This is the list of all Vix commands
 *
 * Be really careful with these. These values are passed over the socket
 * between clients and the VMX process. One client may connect to newer or
 * older versions of the VMX, so we cannot ever change or recycle values if
 * if we add or remove command ids. This is why the values are explicitly 
 * assigned, and there may be gaps in the numeric sequence as some commands 
 * are no longer supported.
 */
typedef int VixAsyncOpType;
enum {
   VIX_COMMAND_UNKNOWN                          = -1,

   VIX_COMMAND_VM_POWERON                       = 0,
   VIX_COMMAND_VM_POWEROFF                      = 1,
   VIX_COMMAND_VM_RESET                         = 2,
   VIX_COMMAND_VM_SUSPEND                       = 3,
   VIX_COMMAND_RUN_PROGRAM                      = 4,
   /* DEPRECATED VIX_COMMAND_GET_PROPERTY                     = 5, */
   /* DEPRECATED VIX_COMMAND_SET_PROPERTY                     = 6, */
   VIX_COMMAND_KEYSTROKES                       = 7,
   VIX_COMMAND_READ_REGISTRY                    = 8,
   VIX_COMMAND_WRITE_REGISTRY                   = 10,
   VIX_COMMAND_COPY_FILE_FROM_GUEST_TO_HOST     = 12,
   VIX_COMMAND_COPY_FILE_FROM_HOST_TO_GUEST     = 13,
   VIX_COMMAND_CREATE_SNAPSHOT                  = 14,
   VIX_COMMAND_REMOVE_SNAPSHOT                  = 15,
   VIX_COMMAND_REVERT_TO_SNAPSHOT               = 16,
   VIX_COMMAND_VM_CLONE                         = 17,
   VIX_COMMAND_DELETE_GUEST_FILE                = 18,
   VIX_COMMAND_GUEST_FILE_EXISTS                = 19,
   VIX_COMMAND_FIND_VM                          = 20,
   VIX_COMMAND_CALL_PROCEDURE                   = 21,
   VIX_COMMAND_REGISTRY_KEY_EXISTS              = 22,
   VIX_COMMAND_WIN32_WINDOW_MESSAGE             = 23,
   VIX_COMMAND_CONSOLIDATE_SNAPSHOTS            = 24,
   VIX_COMMAND_INSTALL_TOOLS                    = 25,
   VIX_COMMAND_CANCEL_INSTALL_TOOLS             = 26,
   VIX_COMMAND_UPGRADE_VIRTUAL_HARDWARE         = 27,
   VIX_COMMAND_SET_NIC_BANDWIDTH                = 28,
   /* DEPRECATED VIX_COMMAND_CREATE_DISK                      = 29, */
   /* DEPRECATED VIX_COMMAND_CREATE_FLOPPY                    = 30, */
   VIX_COMMAND_RELOAD_VM                        = 31,
   VIX_COMMAND_DELETE_VM                        = 32,
   /* DEPRECATED VIX_COMMAND_SYNCDRIVER_FREEZE                = 33, */
   /* DEPRECATED VIX_COMMAND_SYNCDRIVER_THAW                  = 34, */
   /* DEPRECATED VIX_COMMAND_HOT_ADD_DISK                     = 35, */
   /* DEPRECATED VIX_COMMAND_HOT_REMOVE_DISK                  = 36, */
   /* DEPRECATED VIX_COMMAND_SET_GUEST_PRINTER                = 37, */
   VIX_COMMAND_WAIT_FOR_TOOLS                   = 38,
   VIX_COMMAND_CREATE_RUNNING_VM_SNAPSHOT       = 39,
   VIX_COMMAND_CONSOLIDATE_RUNNING_VM_SNAPSHOT  = 40,
   VIX_COMMAND_GET_NUM_SHARED_FOLDERS           = 41,
   VIX_COMMAND_GET_SHARED_FOLDER_STATE          = 42,
   VIX_COMMAND_EDIT_SHARED_FOLDER_STATE         = 43,
   VIX_COMMAND_REMOVE_SHARED_FOLDER             = 44,
   VIX_COMMAND_ADD_SHARED_FOLDER                = 45,
   VIX_COMMAND_RUN_SCRIPT_IN_GUEST              = 46,
   VIX_COMMAND_OPEN_VM                          = 47,
   /* DEPRECATED VIX_COMMAND_GET_DISK_PROPERTIES              = 48, */
   /* DEPRECATED VIX_COMMAND_OPEN_URL                         = 49, */
   VIX_COMMAND_GET_HANDLE_STATE                 = 50,
   /* DEPRECATED VIX_COMMAND_SET_HANDLE_STATE                 = 51, */
   VIX_COMMAND_CREATE_WORKING_COPY              = 55, // DELETE this when we switch remote foundry to VIM
   VIX_COMMAND_DISCARD_WORKING_COPY             = 56, // DELETE this when we switch remote foundry to VIM
   VIX_COMMAND_SAVE_WORKING_COPY                = 57, // DELETE this when we switch remote foundry to VIM
   VIX_COMMAND_CAPTURE_SCREEN                   = 58,
   /* DEPRECATED VIX_COMMAND_GET_VMDB_VALUES                  = 59, */
   /* DEPRECATED VIX_COMMAND_SET_VMDB_VALUES                  = 60, */
   /* DEPRECATED VIX_COMMAND_READ_XML_FILE                    = 61, */
   VIX_COMMAND_GET_TOOLS_STATE                  = 62,
   VIX_COMMAND_CHANGE_SCREEN_RESOLUTION         = 69,
   VIX_COMMAND_DIRECTORY_EXISTS                 = 70,
   VIX_COMMAND_DELETE_GUEST_REGISTRY_KEY        = 71,
   VIX_COMMAND_DELETE_GUEST_DIRECTORY           = 72,
   VIX_COMMAND_DELETE_GUEST_EMPTY_DIRECTORY     = 73,
   VIX_COMMAND_CREATE_TEMPORARY_FILE            = 74,
   VIX_COMMAND_LIST_PROCESSES                   = 75,
   VIX_COMMAND_MOVE_GUEST_FILE                  = 76,
   VIX_COMMAND_CREATE_DIRECTORY                 = 77,
   VIX_COMMAND_CHECK_USER_ACCOUNT               = 78,
   VIX_COMMAND_LIST_DIRECTORY                   = 79,
   VIX_COMMAND_REGISTER_VM                      = 80,
   VIX_COMMAND_UNREGISTER_VM                    = 81,
   VIX_CREATE_SESSION_KEY_COMMAND               = 83,
   VMXI_HGFS_SEND_PACKET_COMMAND                = 84,
   VIX_COMMAND_KILL_PROCESS                     = 85,
   /* DEPRECATED VIX_VM_FORK_COMMAND                          = 86, */
   VIX_COMMAND_LOGOUT_IN_GUEST                  = 87,
   VIX_COMMAND_READ_VARIABLE                    = 88,
   VIX_COMMAND_WRITE_VARIABLE                   = 89,
   VIX_COMMAND_CONNECT_DEVICE                   = 92,
   VIX_COMMAND_IS_DEVICE_CONNECTED              = 93,
   VIX_COMMAND_GET_FILE_INFO                    = 94,
   VIX_COMMAND_SET_FILE_INFO                    = 95,
   VIX_COMMAND_MOUSE_EVENTS                     = 96,
   VIX_COMMAND_OPEN_TEAM                        = 97,
   /* DEPRECATED VIX_COMMAND_FIND_HOST_DEVICES                = 98, */
   VIX_COMMAND_ANSWER_MESSAGE                   = 99,
   VIX_COMMAND_ENABLE_SHARED_FOLDERS            = 100,
   VIX_COMMAND_MOUNT_HGFS_FOLDERS               = 101,
   VIX_COMMAND_HOT_EXTEND_DISK                  = 102,

   /* DEPRECATED VIX_COMMAND_GET_VPROBES_VERSION              = 104, */
   /* DEPRECATED VIX_COMMAND_GET_VPROBES                      = 105, */
   /* DEPRECATED VIX_COMMAND_VPROBE_GET_GLOBALS               = 106, */
   /* DEPRECATED VIX_COMMAND_VPROBE_LOAD                      = 107, */
   /* DEPRECATED VIX_COMMAND_VPROBE_RESET                     = 108, */

   /* DEPRECATED VIX_COMMAND_LIST_USB_DEVICES                 = 109, */
   VIX_COMMAND_CONNECT_HOST                     = 110,

   VIX_COMMAND_CREATE_LINKED_CLONE              = 112,

   /* DEPRECATED VIX_COMMAND_STOP_SNAPSHOT_LOG_RECORDING      = 113, */
   /* DEPRECATED VIX_COMMAND_STOP_SNAPSHOT_LOG_PLAYBACK       = 114, */


   VIX_COMMAND_SAMPLE_COMMAND                   = 115,

   VIX_COMMAND_GET_GUEST_NETWORKING_CONFIG      = 116,
   VIX_COMMAND_SET_GUEST_NETWORKING_CONFIG      = 117,

   /* DEPRECATED VIX_COMMAND_FAULT_TOLERANCE_REGISTER         = 118, */
   /* DEPRECATED VIX_COMMAND_FAULT_TOLERANCE_UNREGISTER       = 119, */
   /* DEPRECATED VIX_COMMAND_FAULT_TOLERANCE_CONTROL          = 120, */
   /* DEPRECATED VIX_COMMAND_FAULT_TOLERANCE_QUERY_SECONDARY  = 121, */

   VIX_COMMAND_VM_PAUSE                         = 122,
   VIX_COMMAND_VM_UNPAUSE                       = 123,
   /* DEPRECATED VIX_COMMAND_GET_SNAPSHOT_LOG_INFO            = 124, */
   /* DEPRECATED VIX_COMMAND_SET_REPLAY_SPEED                 = 125, */

   /* DEPRECATED VIX_COMMAND_ANSWER_USER_MESSAGE              = 126, */
   /* DEPRECATED VIX_COMMAND_SET_CLIENT_LOCALE                = 127, */

   VIX_COMMAND_GET_PERFORMANCE_DATA             = 128,

   /* DEPRECATED VIX_COMMAND_REFRESH_RUNTIME_PROPERTIES       = 129, */

   VIX_COMMAND_GET_SNAPSHOT_SCREENSHOT          = 130,
   /* DEPRECATED VIX_COMMAND_ADD_TIMEMARKER                   = 131, */

   VIX_COMMAND_WAIT_FOR_USER_ACTION_IN_GUEST    = 132,
   /* DEPRECATED VIX_COMMAND_VMDB_END_TRANSACTION             = 133, */
   /* DEPRECATED VIX_COMMAND_VMDB_SET                         = 134, */

   VIX_COMMAND_CHANGE_VIRTUAL_HARDWARE          = 135,

   VIX_COMMAND_HOT_PLUG_CPU                     = 136,
   VIX_COMMAND_HOT_PLUG_MEMORY                  = 137,
   VIX_COMMAND_HOT_ADD_DEVICE                   = 138,
   VIX_COMMAND_HOT_REMOVE_DEVICE                = 139,

   /* DEPRECATED VIX_COMMAND_DEBUGGER_ATTACH                  = 140, */
   /* DEPRECATED VIX_COMMAND_DEBUGGER_DETACH                  = 141, */
   /* DEPRECATED VIX_COMMAND_DEBUGGER_SEND_COMMAND            = 142, */

   /* DEPRECATED VIX_COMMAND_GET_RECORD_STATE                 = 143, */
   /* DEPRECATED VIX_COMMAND_SET_RECORD_STATE                 = 144, */
   /* DEPRECATED VIX_COMMAND_REMOVE_RECORD_STATE              = 145, */
   /* DEPRECATED VIX_COMMAND_GET_REPLAY_STATE                 = 146, */
   /* DEPRECATED VIX_COMMAND_SET_REPLAY_STATE                 = 147, */
   /* DEPRECATED VIX_COMMAND_REMOVE_REPLAY_STATE              = 148, */

   /* DEPRECATED VIX_COMMAND_CANCEL_USER_PROGRESS_MESSAGE     = 150, */
   
   VIX_COMMAND_GET_VMX_DEVICE_STATE             = 151,

   /* DEPRECATED VIX_COMMAND_GET_NUM_TIMEMARKERS              = 152, */
   /* DEPRECATED VIX_COMMAND_GET_TIMEMARKER                   = 153, */
   /* DEPRECATED VIX_COMMAND_REMOVE_TIMEMARKER                = 154, */

   VIX_COMMAND_SET_SNAPSHOT_INFO                = 155,
   VIX_COMMAND_SNAPSHOT_SET_MRU                 = 156,

   VIX_COMMAND_LOGOUT_HOST                      = 157,

   VIX_COMMAND_HOT_PLUG_BEGIN_BATCH             = 158,
   VIX_COMMAND_HOT_PLUG_COMMIT_BATCH            = 159,

   VIX_COMMAND_TRANSFER_CONNECTION              = 160,
   VIX_COMMAND_TRANSFER_REQUEST                 = 161,
   VIX_COMMAND_TRANSFER_FINAL_DATA              = 162,

   /* DEPRECATED VIX_COMMAND_ADD_ROLLING_SNAPSHOT_TIER        = 163,    */
   /* DEPRECATED VIX_COMMAND_REMOVE_ROLLING_SNAPSHOT_TIER     = 164,    */
   /* DEPRECATED VIX_COMMAND_LIST_ROLLING_SNAPSHOT_TIER       = 165,    */

   /* DEPRECATED VIX_COMMAND_ADD_ROLLING_SNAPSHOT_TIER_VMX    = 166,    */
   /* DEPRECATED VIX_COMMAND_REMOVE_ROLLING_SNAPSHOT_TIER_VMX = 167,    */
   /* DEPRECATED VIX_COMMAND_LIST_ROLLING_SNAPSHOT_TIER_VMX   = 168,    */

   VIX_COMMAND_LIST_FILESYSTEMS                 = 169,

   VIX_COMMAND_CHANGE_DISPLAY_TOPOLOGY          = 170,

   VIX_COMMAND_SUSPEND_AND_RESUME               = 171,

   VIX_COMMAND_REMOVE_BULK_SNAPSHOT             = 172,

   VIX_COMMAND_COPY_FILE_FROM_READER_TO_GUEST   = 173,

   VIX_COMMAND_GENERATE_NONCE                   = 174,

   VIX_COMMAND_CHANGE_DISPLAY_TOPOLOGY_MODES    = 175,

   VIX_COMMAND_QUERY_CHILDREN                   = 176,

   VIX_COMMAND_LIST_FILES                       = 177,

   VIX_COMMAND_CREATE_DIRECTORY_EX              = 178,

   VIX_COMMAND_MOVE_GUEST_FILE_EX               = 179,

   VIX_COMMAND_MOVE_GUEST_DIRECTORY             = 180,

   VIX_COMMAND_CREATE_TEMPORARY_FILE_EX         = 181,

   VIX_COMMAND_CREATE_TEMPORARY_DIRECTORY       = 182,

   VIX_COMMAND_SET_GUEST_FILE_ATTRIBUTES        = 183,

   VIX_COMMAND_COPY_FILE_FROM_GUEST_TO_READER   = 184,

   VIX_COMMAND_START_PROGRAM                    = 185,

   VIX_COMMAND_LIST_PROCESSES_EX                = 186,

   VIX_COMMAND_READ_ENV_VARIABLES               = 187,

   VIX_COMMAND_INITIATE_FILE_TRANSFER_FROM_GUEST   = 188,
   VIX_COMMAND_INITIATE_FILE_TRANSFER_TO_GUEST     = 189,

   VIX_COMMAND_ACQUIRE_CREDENTIALS              = 190,
   VIX_COMMAND_RELEASE_CREDENTIALS              = 191,
   VIX_COMMAND_VALIDATE_CREDENTIALS             = 192,
   VIX_COMMAND_TERMINATE_PROCESS                = 193,
   VIX_COMMAND_DELETE_GUEST_FILE_EX             = 194,
   VIX_COMMAND_DELETE_GUEST_DIRECTORY_EX        = 195,
   VIX_COMMAND_HOT_CHANGE_MONITOR_TYPE          = 196,

   VIX_COMMAND_ADD_AUTH_ALIAS                   = 197,
   VIX_COMMAND_REMOVE_AUTH_ALIAS                = 198,
   VIX_COMMAND_LIST_AUTH_PROVIDER_ALIASES       = 199,
   VIX_COMMAND_LIST_AUTH_MAPPED_ALIASES         = 200,

   VIX_COMMAND_CREATE_REGISTRY_KEY              = 201,
   VIX_COMMAND_LIST_REGISTRY_KEYS               = 202,
   VIX_COMMAND_DELETE_REGISTRY_KEY              = 203,
   VIX_COMMAND_SET_REGISTRY_VALUE               = 204,
   VIX_COMMAND_LIST_REGISTRY_VALUES             = 205,
   VIX_COMMAND_DELETE_REGISTRY_VALUE            = 206,

   VIX_COMMAND_REMOVE_AUTH_ALIAS_BY_CERT        = 207,

   /*
    * HOWTO: Adding a new Vix Command. Step 2a.
    *
    * Add a new ID for your new function prototype here. BE CAREFUL. The
    * OFFICIAL list of id's is in the bfg-main tree, in bora/lib/public/vixCommands.h.
    * When people add new command id's in different tree, they may collide and use
    * the same ID values. This can merge without conflicts, and cause runtime bugs.
    * Once a new command is added here, a command info field needs to be added
    * in bora/lib/foundryMsg/foundryMsg.c as well.
    */
   VIX_COMMAND_LAST_NORMAL_COMMAND              = 208,

   VIX_TEST_UNSUPPORTED_TOOLS_OPCODE_COMMAND    = 998,
   VIX_TEST_UNSUPPORTED_VMX_OPCODE_COMMAND      = 999,
};


/*
 * These are the command names that are passed through the backdoor from the
 * VMX to the tools.
 */
#define VIX_BACKDOOR_COMMAND_VERSION               "Vix_1_"
#define VIX_BACKDOORCOMMAND_RUN_PROGRAM            VIX_BACKDOOR_COMMAND_VERSION"Run_Program"
#define VIX_BACKDOORCOMMAND_SYNCDRIVER_FREEZE      VIX_BACKDOOR_COMMAND_VERSION"SyncDriver_Freeze"
#define VIX_BACKDOORCOMMAND_SYNCDRIVER_THAW        VIX_BACKDOOR_COMMAND_VERSION"SyncDriver_Thaw"
#define VIX_BACKDOORCOMMAND_GET_PROPERTIES         VIX_BACKDOOR_COMMAND_VERSION"Get_ToolsProperties"
#define VIX_BACKDOORCOMMAND_UNRECOGNIZED_COMMAND   VIX_BACKDOOR_COMMAND_VERSION"Unrecognized_Command"
#define VIX_BACKDOORCOMMAND_COMMAND                VIX_BACKDOOR_COMMAND_VERSION"Relayed_Command"
#define VIX_BACKDOORCOMMAND_MOUNT_VOLUME_LIST      VIX_BACKDOOR_COMMAND_VERSION"Mount_Volumes"


/*
 * This is the set of features that may be supported by different
 * versions of the VMX or Vix Tools.
 */
enum VixToolsFeatures {
   VIX_TOOLSFEATURE_SUPPORT_GET_HANDLE_STATE   = 0x0001,
   /* VIX_TOOLSFEATURE_SUPPORT_OPEN_URL           = 0x0002, Removed in version 1.11*/
};


enum  {
   VIX_TOOLS_READ_FILE_ACCESS    = 0x01,
   VIX_TOOLS_WRITE_FILE_ACCESS   = 0x02,
};


/*
 * These are the command names that are passed through the backdoor from the tools
 * to the VMX.
 */
#define VIX_BACKDOORCOMMAND_RUN_PROGRAM_DONE    "Run_Program_Done"


#define VIX_FEATURE_UNKNOWN_VALUE "Unknown"


/*
 * VIX_COMMAND_RUN_PROGRAM returns 2 integer values as an array. These
 * are the indexes
 * TODO: Delete this enum
 */
enum VixRunProgramResultValues {
   VIX_COMMAND_RUN_PROGRAM_ELAPSED_TIME_RESULT   = 0,
   VIX_COMMAND_RUN_PROGRAM_EXIT_CODE_RESULT      = 1,
};

/* These are the values of Vix objects. */
#define VIX_VM_OBJECT_TYPE                        "VixVM"

/* VM enumeration */
#ifdef _WIN32
#define VIX_WINDOWSREGISTRY_VMWARE_KEY             "Software\\" COMPANY_NAME
#define VIX_WINDOWSREGISTRY_RUNNING_VM_LIST        "Running VM List"
#define VIX_WINDOWSREGISTRY_VMWARE_KEY_RUNNING_VM_LIST VIX_WINDOWSREGISTRY_VMWARE_KEY "\\" VIX_WINDOWSREGISTRY_RUNNING_VM_LIST
#endif


/*
 * This is used to denote that the contents of a VIX XML-like response
 * string has been escaped. Old Tools did not escape the contents.
 * This tag is only used for existing commands that did not originally perform
 * escaping. Any new command must always escape any strings passed in XML.
 * See ListProcessesInGuest as an example.
 * The protocol works as follows:
 * 1) A client library that internally knows how to handle escaped XML opts in
 *    by including the VIX_REQUESTMSG_ESCAPE_XML_DATA in relevent requests.
 * 2) Tools that understands the VIX_REQUESTMSG_ESCAPE_XML_DATA flag sees that
 *    it is set in the request, and then escapes all string data within the
 *    XML response. To indicate to the client that it has understood the
 *    request, it include the VIX_XML_ESCAPED_TAG in the response (at the
 *    begining of the response).
 * 3) When the client library receives the response, it searches for the
 *    VIX_XML_ESCAPED_TAG. If it is present, it then unescapes all string
 *    data in the response. If the tag is not present, the client library
 *    assumes that the Tools did not understand VIX_REQUESTMSG_ESCAPE_XML_DATA
 *    and that the string data is not escaped.
 * The following characters are escaped: '<', '>', and '%'.
 * For new commands (starting with those released in M/N for the vSphere
 * guest ops project), the escaping is exactly the same, but the
 * VIX_REQUESTMSG_ESCAPE_XML_DATA flag and the VIX_XML_ESCAPED_TAG are not
 * used, since both ends expect escaping.
 */
#define VIX_XML_ESCAPED_TAG   "<escaped/>"

#define VIX_XML_ESCAPE_CHARACTER '%'


/*
 *-----------------------------------------------------------------------------
 *
 * VixMsg --
 *
 * These are the formatting and parsing utilities provided by the VixMsg
 * library.
 *
 *-----------------------------------------------------------------------------
 */

#ifndef VIX_HIDE_FROM_JAVA
struct VixCommandRequestHeader *
VixMsg_AllocRequestMsg(size_t msgHeaderAndBodyLength,
                       int opCode,
                       uint64 cookie,
                       int credentialType,
                       const char *userNamePassword);

struct VixCommandResponseHeader *
VixMsg_AllocResponseMsg(const struct VixCommandRequestHeader *requestHeader,
                        VixError error,
                        uint32 additionalError,
                        size_t responseBodyLength,
                        const void *responseBody,
                        size_t *responseMsgLength);

void VixMsg_InitResponseMsg(struct VixCommandResponseHeader *responseHeader,
                            const struct VixCommandRequestHeader *requestHeader,
                            VixError error,
                            uint32 additionalError,
                            size_t totalMessageLength);

VixError VixMsg_ValidateMessage(const void *vMsg, size_t msgLength);

VixError VixMsg_ValidateRequestMsg(const void *vMsg, size_t msgLength);

VixError VixMsg_ValidateResponseMsg(const void *vMsg, size_t msgLength);

VixError VixMsg_ParseWriteVariableRequest(VixMsgWriteVariableRequest *msg,
                                          char **valueName,
                                          char **value);

VixError VixMsg_ObfuscateNamePassword(const char *userName,
                                      const char *password,
                                      char **result);

VixError VixMsg_DeObfuscateNamePassword(const char *packagedName,
                                        char **userNameResult,
                                        char **passwordResult);

VixError VixMsg_EncodeString(const char *str, char **result);

VixError VixMsg_DecodeString(const char *str, char **result);

Bool VixMsg_ValidateCommandInfoTable(void);

const char *VixAsyncOp_GetDebugStrForOpCode(int opCode);

VixCommandSecurityCategory VixMsg_GetCommandSecurityCategory(int opCode);

/*
 * Vix private internal properties shared between the Vix client
 * and the VMX.
 */

enum {
   VIX_PROPERTY_VM_POWER_OFF_TO_SNAPSHOT_UID       = 5102,
};

VixError VixMsg_AllocGenericRequestMsg(int opCode,
                                       uint64 cookie,
                                       int credentialType,
                                       const char *userNamePassword,
                                       int options,
                                       VixPropertyListImpl *propertyList,
                                       VixCommandGenericRequest **request);

VixError VixMsg_ParseGenericRequestMsg(const VixCommandGenericRequest *request,
                                       int *options,
                                       VixPropertyListImpl *propertyList);

VixError
VixMsg_ParseSimpleResponseWithString(const VixCommandResponseHeader *response,
                                     const char **result);

void *VixMsg_MallocClientData(size_t size);
void *VixMsg_ReallocClientData(void *ptr, size_t size);
char *VixMsg_StrdupClientData(const char *s, Bool *allocateFailed);

/*
 * Parser state used by VMAutomationMsgParser* group of functions.
 */
typedef struct {
   const char *currentPtr;
   const char *endPtr;
} VMAutomationMsgParser;

/* Keep the original type name around all the old code can stay the same. */
typedef VMAutomationMsgParser VMAutomationRequestParser;


#define VMAutomationRequestParserInit VMAutomationMsgParserInitRequest
#define VMAutomationMsgParserInitRequest(state, msg, fixedLength) \
   __VMAutomationMsgParserInitRequest(__FUNCTION__, __LINE__, state, msg, fixedLength)
VixError
__VMAutomationMsgParserInitRequest(const char *caller,
                                   unsigned int line,
                                   VMAutomationMsgParser *state,
                                   const struct VixCommandRequestHeader *msg,
                                   size_t fixedLength);

#define VMAutomationMsgParserInitResponse(state, msg, fixedLength) \
   __VMAutomationMsgParserInitResponse(__FUNCTION__, __LINE__, state, msg, fixedLength)
VixError
__VMAutomationMsgParserInitResponse(const char *caller,
                                    unsigned int line,
                                    VMAutomationMsgParser *state,
                                    const struct VixCommandResponseHeader *msg,
                                    size_t fixedLength);

#define VMAutomationRequestParserGetRemainingData \
   VMAutomationMsgParserGetRemainingData
const void *
VMAutomationMsgParserGetRemainingData(VMAutomationMsgParser *state,
                                      size_t *length);

#define VMAutomationRequestParserGetData VMAutomationMsgParserGetData
#define VMAutomationMsgParserGetData(state, length, result) \
   __VMAutomationMsgParserGetData(__FUNCTION__, __LINE__,               \
                                  state, length, (const char **)result)
VixError __VMAutomationMsgParserGetData(const char *caller,
                                        unsigned int line,
                                        VMAutomationMsgParser *state,
                                        size_t length,
                                        const char **result);

#define VMAutomationRequestParserGetOptionalString \
   VMAutomationMsgParserGetOptionalString
#define VMAutomationMsgParserGetOptionalString(state, length, result) \
   __VMAutomationMsgParserGetOptionalString(__FUNCTION__, __LINE__,     \
                                            state, length, result)
VixError __VMAutomationMsgParserGetOptionalString(const char *caller,
                                                  unsigned int line,
                                                  VMAutomationMsgParser *state,
                                                  size_t length,
                                                  const char **result);

#define VMAutomationRequestParserGetOptionalStrings \
   VMAutomationMsgParserGetOptionalStrings
#define VMAutomationMsgParserGetOptionalStrings(state, count, length,     \
           result)                                                            \
   __VMAutomationMsgParserGetOptionalStrings(__FUNCTION__, __LINE__,      \
                                             state, count, length, result)
VixError __VMAutomationMsgParserGetOptionalStrings
   (const char *caller,
    unsigned int line,
    VMAutomationMsgParser *state,
    uint32 count,
    size_t length,
    const char **result);

#define VMAutomationRequestParserGetString VMAutomationMsgParserGetString
#define VMAutomationMsgParserGetString(state, length, result) \
   __VMAutomationMsgParserGetString(__FUNCTION__, __LINE__,             \
                                    state, length, result)
VixError __VMAutomationMsgParserGetString(const char *caller,
                                          unsigned int line,
                                          VMAutomationMsgParser *state,
                                          size_t length,
                                          const char **result);

#define VMAutomationRequestParserGetPropertyList \
   VMAutomationMsgParserGetPropertyList
#define VMAutomationMsgParserGetPropertyList(state, length, propList) \
   __VMAutomationMsgParserGetPropertyList(__FUNCTION__, __LINE__,       \
                                          state, length, propList)
VixError
__VMAutomationMsgParserGetPropertyList(const char *caller,
                                       unsigned int line,
                                       VMAutomationMsgParser *state,
                                       size_t length,
                                       VixPropertyListImpl *propList);

#endif   // VIX_HIDE_FROM_JAVA

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _VIX_COMMANDS_H_

