/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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

#include "vm_version.h"
#include "vix.h"

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
 * should be. There are for sanity checks and to ignore maliciously
 * large messages that may be part of an DoS attack. The may need to
 * be revised if large messages are added to the protocol. 
 */

#define VIX_COMMAND_MAX_SIZE           (16 * 1024 * 1024)  
#define VIX_COMMAND_MAX_REQUEST_SIZE   65536

/*
 * The types of credential we can pass with any request.
 */
#define VIX_USER_CREDENTIAL_NONE                      0
#define VIX_USER_CREDENTIAL_NAME_PASSWORD             1
#define VIX_USER_CREDENTIAL_ANONYMOUS                 2
#define VIX_USER_CREDENTIAL_ROOT                      3
#define VIX_USER_CREDENTIAL_NAME_PASSWORD_OBFUSCATED  4
#define VIX_USER_CREDENTIAL_CONSOLE_USER              5
#define VIX_USER_CREDENTIAL_HOST_CONFIG_SECRET        6
#define VIX_USER_CREDENTIAL_HOST_CONFIG_HASHED_SECRET 7
#define VIX_USER_CREDENTIAL_NAMED_INTERACTIVE_USER    8

#define VIX_SHARED_SECRET_CONFIG_USER_NAME          "__VMware_Vix_Shared_Secret_1__"


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
   VIX_COMMAND_GUEST_RETURNS_ENCODED_STRING  = 0x20,
   VIX_COMMAND_GUEST_RETURNS_PROPERTY_LIST   = 0x40,
   VIX_COMMAND_GUEST_RETURNS_BINARY          = 0x80,
   // We cannot add more constants here. This is stored in a uint8,
   // so it is full. Use requestFlags or responseFlags.
};


/*
 * These are the flags set in the request Flags field.
 */
enum {
   VIX_REQUESTMSG_ONLY_RELOAD_NETWORKS                = 0x01,
   VIX_REQUESTMSG_RETURN_ON_INITIATING_TOOLS_UPGRADE  = 0x02,
   VIX_REQUESTMSG_RUN_IN_ANY_VMX_STATE                = 0x04,
   VIX_REQUESTMSG_REQUIRES_INTERACTIVE_ENVIRONMENT    = 0x08,
};


/*
 * These are the flags set in responseFlags.
 */
enum VixResponseFlagsValues {
   VIX_RESPONSE_SOFT_POWER_OP       = 0x0001,
   VIX_RESPONSE_EXTENDED_RESULT_V1  = 0x0002,
   VIX_RESPONSE_TRUNCATED           = 0x0004,
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
typedef
#include "vmware_pack_begin.h"
struct VixMsgHeader {
   uint32   magic;
   uint16   messageVersion;

   uint32   totalMessageLength;
   uint32   headerLength;
   uint32   bodyLength;
   uint32   credentialLength;

   uint8    commonFlags;
}
#include "vmware_pack_end.h"
VixMsgHeader;


/*
 * These are the headers for a single request, response, or event.
 * In theory, either the VMX or the client may issue a request
 * to the other.  In practice, legacy foundry clients can only
 * accept response messages from the VMX, not requests.  Because of
 * this, an event message is a special kind of response message.
 */
typedef
#include "vmware_pack_begin.h"
struct VixCommandRequestHeader {
   VixMsgHeader      commonHeader;

   uint32            opCode;
   uint32            requestFlags;

   uint32            timeOut;

   uint64            cookie;
   uint32            clientHandleId; // for remote case

   uint32            userCredentialType;
}
#include "vmware_pack_end.h"
VixCommandRequestHeader;


typedef
#include "vmware_pack_begin.h"
struct VixCommandResponseHeader {
   VixMsgHeader   commonHeader;

   uint64         requestCookie;

   uint32         responseFlags;

   uint32         duration;

   uint32         error;
   uint32         additionalError;
   uint32         errorDataLength;
}
#include "vmware_pack_end.h"
VixCommandResponseHeader;


typedef
#include "vmware_pack_begin.h"
struct VixMsgEventHeader {
   VixCommandResponseHeader   responseHeader;

   int32                      eventType;
}
#include "vmware_pack_end.h"
VixMsgEventHeader;


/*
 * A trivial request that is just a generic
 * response header (it has no body).
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgTrivialRequest {
   VixCommandRequestHeader   header;
}
#include "vmware_pack_end.h"
VixMsgTrivialRequest;


/*
 * A trivial event that is just a generic
 * event header (it has no body).
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgTrivialEvent {
   VixMsgEventHeader          eventHeader;
}
#include "vmware_pack_end.h"
VixMsgTrivialEvent;


/*
 * **********************************************************
 * This is a generic progress update from the VMX.
 * The VMX may send several of these before sending a final response 
 * message. These only report progress, they do not mean the job
 * has completed. These messages are identified by the
 * VIX_COMMAND_REPORT_EVENT flag in the commonFlags field and
 * VIX_EVENTTYPE_JOB_PROGRESS as the eventType.
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgProgressEvent {
   VixMsgEventHeader          eventHeader;

   int64                      workToDo;
   int64                      workDone;
} 
#include "vmware_pack_end.h"
VixMsgProgressEvent;


/*
 * This is an event sent from the VMX to all clients when some property changes.
 * It may be used for any runtime property.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgPropertyChangedEvent {
   VixMsgEventHeader        eventHeader;
   int                      options;
   uint32                   propertyListSize;
}
#include "vmware_pack_end.h"
VixMsgPropertyChangedEvent;



/*
 * **********************************************************
 * This is a userName and password pair.
 */
typedef
#include "vmware_pack_begin.h"
struct VixCommandNamePassword {
   uint32    nameLength;
   uint32    passwordLength;
}
#include "vmware_pack_end.h"
VixCommandNamePassword;


/*
 * **********************************************************
 * Open VM Command
 *
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgVMOpenRequest {
   VixCommandRequestHeader header;
   int32                   options; //options for VM_Open or CreateWorkingCopy
   uint32                  xmlPathNameSize;
   uint32                  vmxPathNameSize;
   //this is followed by the xml pathname and the vmx pathname
}
#include "vmware_pack_end.h"
VixMsgVMOpenRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgVMOpenResponse {
   VixCommandResponseHeader   header;
   int32                      vmPowerState;
   uint32                     vmxPathNameSize;
}
#include "vmware_pack_end.h"
VixMsgVMOpenResponse;



/*
 * **********************************************************
 * Basic power op request. The response is just a generic
 * response header (it has no body).
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgPowerOpRequest {
   VixCommandRequestHeader   header;
   VixVMPowerOpOptions       powerOpOptions;
}
#include "vmware_pack_end.h"
VixMsgPowerOpRequest;



/*
 * **********************************************************
 * Set NIC request. The response is just a generic
 * response header (it has no body).
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgNICBandwidth {
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
}
#include "vmware_pack_end.h"
VixMsgNICBandwidth;
   

typedef
#include "vmware_pack_begin.h"
struct VixMsgSetNICBandwidthRequest {
   VixCommandRequestHeader header;
   VixMsgNICBandwidth      nicSettings;
}
#include "vmware_pack_end.h"
VixMsgSetNICBandwidthRequest;



/*
 * **********************************************************
 * Get/Set Properties Request
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgGetVMStateResponse {
   VixCommandResponseHeader   header;
   uint32                     bufferSize;
   // This is followed by the buffer of serialized properties
}
#include "vmware_pack_end.h"
VixMsgGetVMStateResponse;


typedef
#include "vmware_pack_begin.h"
struct VixMsgSetVMStateRequest {
   VixCommandRequestHeader header;
   uint32                  bufferSize;
   // This is followed by the buffer of serialized properties
}
#include "vmware_pack_end.h"
VixMsgSetVMStateRequest;





/*
 * **********************************************************
 * Basic reload state request. The response is just a generic
 * response header (it has no body).
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgReloadVMStateRequest {
   VixCommandRequestHeader   header;
   // This is followed by an array of VixMsgConfigurationObjectType objects
}
#include "vmware_pack_end.h"
VixMsgReloadVMStateRequest;


/*
 * This is a prefix to a configuration object. The current supported
 * types are defined below in the VixCommonConfigObjectType enum.
 * Following each object type struct is the specific object. Currently,
 * we support:
 * 
 *    VIX_NETWORK_SETTING_CONFIG   - VixMsgNICBandwidth
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgConfigurationObjectType {
   int32    configurationType;
   uint32   objectSize;
}
#include "vmware_pack_end.h"
VixMsgConfigurationObjectType;


typedef
#include "vmware_pack_begin.h"
struct VixMsgLANSegmentConfiguration {
   VixMsgConfigurationObjectType   configHeader;
   VixMsgNICBandwidth              lanSegment;
}
#include "vmware_pack_end.h"
VixMsgLANSegmentConfiguration;

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
 * Commands related to Record|Replay State.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgRecordReplayStateCommandRequest {
   VixCommandRequestHeader   header;
   
   int32                     options;
   uint32                    propertyListBufferSize;
}
#include "vmware_pack_end.h"
VixMsgRecordReplayStateCommandRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgRecordReplayStateCommandResponse {
   VixCommandResponseHeader header;
   uint32   propertyListBufferSize;
}
#include "vmware_pack_end.h"
VixMsgRecordReplayStateCommandResponse;


/*
 * **********************************************************
 * Wait for tools request. The response is just a generic
 * response header (it has no body).
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgWaitForToolsRequest {
   VixCommandRequestHeader header;
   int32                   timeoutInSeconds;
   int32                   minVersion;
}
#include "vmware_pack_end.h"
VixMsgWaitForToolsRequest;



/*
 * **********************************************************
 * Run a program on the guest.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgRunProgramRequest {
   VixCommandRequestHeader header;

   int32                   runProgramOptions;
   uint32                  programNameLength;
   uint32                  commandLineArgsLength;
}
#include "vmware_pack_end.h"
VixMsgRunProgramRequest;


typedef
#include "vmware_pack_begin.h"
struct VixMsgOldRunProgramResponse {
   VixCommandResponseHeader   header;

   int32                      exitCode;
   VmTimeType                 deltaTime;
} 
#include "vmware_pack_end.h"
VixMsgOldRunProgramResponse;


typedef
#include "vmware_pack_begin.h"
struct VixMsgRunProgramResponse {
   VixCommandResponseHeader   header;

   int32                      exitCode;
   VmTimeType                 deltaTime;

   int64                      pid;
   uint32                     stdOutLength;
   uint32                     stdErrLength;
} 
#include "vmware_pack_end.h"
VixMsgRunProgramResponse;


/*
 * **********************************************************
 * Install VMware tools.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgInstallToolsRequest {
   VixCommandRequestHeader header;

   int32                   installOptions;
   uint32                  commandLineArgsLength;
}
#include "vmware_pack_end.h"
VixMsgInstallToolsRequest;



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
   VIX_KEYSTROKE_MODIFIER_CAPS_LOCK         = 0x20,
   VIX_KEYSTROKE_MODIFIER_NUM_LOCK          = 0x40,
   VIX_KEYSTROKE_MODIFIER_KEY_DOWN_ONLY     = 0x80,
   VIX_KEYSTROKE_MODIFIER_KEY_UP_ONLY       = 0x100,
};


typedef
#include "vmware_pack_begin.h"
struct VixMsgKeyStroke {
   int32                   modifier;
   int32                   scanCode;
   int32                   duration;
   int32                   delayAfterKeyUp;
   int32                   repeat;
} 
#include "vmware_pack_end.h"
VixMsgKeyStroke;


typedef
#include "vmware_pack_begin.h"
struct VixMsgSendKeyStrokesRequest {
   VixCommandRequestHeader header;

   int32                   keyStrokeType;
   int32                   options;
   int64                   targetPid;
   int32                   numKeyStrokes;
   uint32                  windowNameLength;
} 
#include "vmware_pack_end.h"
VixMsgSendKeyStrokesRequest;

/*
 * send a mouse event to the guest
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgSendMouseEventRequest {
   VixCommandRequestHeader header;

   int16                    x;
   int16                    y;
   int16                    buttons;
   int32                    options;
} 
#include "vmware_pack_end.h"
VixMsgSendMouseEventRequest;




/*
 * **********************************************************
 * Read or write the registry on the guest.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgRegistryRequest {
   VixCommandRequestHeader header;

   uint32                  registryKeyLength;
   int32                   expectedRegistryKeyType;
   uint32                  dataToWriteSize;
}
#include "vmware_pack_end.h"
VixMsgRegistryRequest;



/*
 * **********************************************************
 * Copy files between the host and the guest.
 */
typedef
#include "vmware_pack_begin.h"
struct VixCommandRenameFileRequest {
   VixCommandRequestHeader header;

   int32                   copyFileOptions;
   uint32                  oldPathNameLength;
   uint32                  newPathNameLength;
   uint32                  filePropertiesLength;
}
#include "vmware_pack_end.h"
VixCommandRenameFileRequest;

typedef
#include "vmware_pack_begin.h"
struct VixCommandHgfsSendPacket {
   VixCommandRequestHeader header;

   uint32                  hgfsPacketSize;
   int32                   timeout;
}
#include "vmware_pack_end.h"
VixCommandHgfsSendPacket;


/*
 * **********************************************************
 * Perform a simple operation (like delete or check for existence)
 * on a file or registry key on the guest.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgSimpleFileRequest {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
}
#include "vmware_pack_end.h"
VixMsgSimpleFileRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgListDirectoryRequest {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
   int64                   offset;
}
#include "vmware_pack_end.h"
VixMsgListDirectoryRequest;

enum VixListDirectoryOptions {
   VIX_LIST_DIRECTORY_USE_OFFSET = 0x01
};

/*
 * This is used to reply to several operations, like testing whether
 * a file or registry key exists on the client.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgCheckExistsResponse {
   VixCommandResponseHeader   header;
   Bool                       exists;
}
#include "vmware_pack_end.h"
VixMsgCheckExistsResponse;


/*
 * **********************************************************
 * Perform a create file operation (like createDir or moveFile)
 * on a file in the guest. This lets you pass in things like the initial file
 * properties.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgCreateFileRequest {
   VixCommandRequestHeader header;

   int32                   fileOptions;
   uint32                  guestPathNameLength;
   uint32                  filePropertiesLength;
}
#include "vmware_pack_end.h"
VixMsgCreateFileRequest;


/*
 * **********************************************************
 * Hot add and remove a disk in a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgHotDiskRequest {
   VixCommandRequestHeader header;
   int32                    hotDiskOptions;
   uint32                   adapterTypeLength;
   uint32                   typeLength;
   uint32                   nameLength;
   uint32                   modeLength;
   uint32                   deviceTypeLength;
   int32                    adapterNum;
   int32                    targetNum;
}
#include "vmware_pack_end.h"
VixMsgHotDiskRequest;


/*
 * **********************************************************
 * Hot extend a disk in a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgHotExtendDiskRequest {
   VixCommandRequestHeader header;
   int32                    hotDiskOptions;
   uint32                   typeLength;
   int32                    adapterNum;
   int32                    targetNum;
   uint64                   newNumSectors;
}
#include "vmware_pack_end.h"
VixMsgHotExtendDiskRequest;


/*
 * **********************************************************
 * Hot plug CPU in a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgHotPlugCPURequest {
   VixCommandRequestHeader header;
   uint32                  newNumCPU;
}
#include "vmware_pack_end.h"
VixMsgHotPlugCPURequest;


/*
 * **********************************************************
 * Hot plug memory in a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgHotPlugMemoryRequest {
   VixCommandRequestHeader header;
   uint32                  newSizeMb;
}
#include "vmware_pack_end.h"
VixMsgHotPlugMemoryRequest;


/*
 * **********************************************************
 * Hot add device in a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgHotAddDeviceRequest {
   VixCommandRequestHeader header;
   int32                   deviceType;
   uint32                  devicePropsBufferSize;
   int32                   backingType;
   uint32                  backingPropsBufferSize;
}
#include "vmware_pack_end.h"
VixMsgHotAddDeviceRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgHotAddDeviceResponse {
   VixCommandResponseHeader   header;
   int32                      adapterNum;
   int32                      targetNum;
}
#include "vmware_pack_end.h"
VixMsgHotAddDeviceResponse;


/*
 * **********************************************************
 * Hot remove device in a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgHotRemoveDeviceRequest {
   VixCommandRequestHeader header;
   int32                   deviceType;
   uint32                  devicePropsBufferSize;
}
#include "vmware_pack_end.h"
VixMsgHotRemoveDeviceRequest;


/*
 * **********************************************************
 * Create a snapshot of a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgCreateSnapshotRequest {
   VixCommandRequestHeader    header;

   int32                      options;

   Bool                       powerOff;
   Bool                       saveDeviceState;

   uint32                     nameLength;
   uint32                     descriptionLength;
}
#include "vmware_pack_end.h"
VixMsgCreateSnapshotRequest;


typedef
#include "vmware_pack_begin.h"
struct VixMsgCreateSnapshotResponse {
   VixCommandResponseHeader   header;
   int32                      snapshotUID;
}
#include "vmware_pack_end.h"
VixMsgCreateSnapshotResponse;


/*
 * Several snapshot operations for a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgSnapshotRequest {
   VixCommandRequestHeader    header;

   int32                      options;
   int32                      snapshotId;
}
#include "vmware_pack_end.h"
VixMsgSnapshotRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgSnapshotUpdateEvent {
   VixMsgEventHeader          eventHeader;

   int32                      options;
   uint32                     propertyListLength;
   /*
    * This is followed by a serialized property list.
    */
}
#include "vmware_pack_end.h"
VixMsgSnapshotUpdateEvent;

typedef
#include "vmware_pack_begin.h"
struct VixMsgSnapshotMRURequest {
   VixCommandRequestHeader    header;

   int32                      snapshotId;
   int32                      maxMRU;
}
#include "vmware_pack_end.h"
VixMsgSnapshotMRURequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgSetSnapshotInfoRequest {
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
}
#include "vmware_pack_end.h"
VixMsgSetSnapshotInfoRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgSetSnapshotInfoResponse {
   VixCommandResponseHeader    header;

   uint32                     propertyListLength;
}
#include "vmware_pack_end.h"
VixMsgSetSnapshotInfoResponse;


/*
 * Fork a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgVMForkRequest {
   VixCommandRequestHeader    header;

   int32                      options;
   Bool                       disconnectRemovable;

   uint32                     cfgFileNameLen;
   uint32                     displayNameLen;
}
#include "vmware_pack_end.h"
VixMsgVMForkRequest;


typedef
#include "vmware_pack_begin.h"
struct VixMsgVMForkResponse {
   VixCommandResponseHeader  header;
}
#include "vmware_pack_end.h"
VixMsgVMForkResponse;


/*
 * Stop recording or playback of a snapshot event log.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgVMSnapshotLoggingRequest {
   VixCommandRequestHeader    header;

   int32                      options;
}
#include "vmware_pack_end.h"
VixMsgVMSnapshotLoggingRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgRecordReplayEvent {
   VixMsgEventHeader          eventHeader;

   int32                      newRecordState;
   int32                      reason;
} 
#include "vmware_pack_end.h"
VixMsgRecordReplayEvent;

typedef
#include "vmware_pack_begin.h"
struct VixMsgTimeMarkerEncounteredEvent {
   VixMsgEventHeader        eventHeader;      
   uint32                   propertyListSize;
}
#include "vmware_pack_end.h"
VixMsgTimeMarkerEncounteredEvent;

typedef
#include "vmware_pack_begin.h"
struct VixMsgDebuggerEvent {
   VixMsgEventHeader          eventHeader;

   int32                      blobLength;
   /*
    * This is followed by the blob buffer.
    */
}
#include "vmware_pack_end.h"
VixMsgDebuggerEvent;

typedef
#include "vmware_pack_begin.h"
struct VixMsgGetRecordReplayInfoResponse {
   VixCommandResponseHeader header;
   uint32                   propertyListSize;
}
#include "vmware_pack_end.h"
VixMsgGetRecordReplayInfoResponse;

typedef
#include "vmware_pack_begin.h"
struct VixMsgVMSnapshotSetReplaySpeedRequest {
   VixCommandRequestHeader    header;

   int32                      options;
   int32                      timeType;
   int64                      timeValue;
}
#include "vmware_pack_end.h"
VixMsgVMSnapshotSetReplaySpeedRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgVMAddTimeMarkerRequest {
   VixCommandRequestHeader    header;
   uint32                     options;
   uint32                     action;
   uint32                     propertyListSize;
}
#include "vmware_pack_end.h"
VixMsgVMAddTimeMarkerRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgVMGetTimeMarkerRequest {
   VixCommandRequestHeader    header;
   uint32                     options;
   uint32                     whence;
   uint32                     index;
   uint32                     propertyListSize;
}
#include "vmware_pack_end.h"
VixMsgVMGetTimeMarkerRequest;


/*
 * Fault Tolerance Automation
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgFaultToleranceControlRequest {
   VixCommandRequestHeader    requestHeader;

   int32                      command;
   char                       uuid[48];
   uint32                     vmxPathLen;
   char                       vmxFilePath[1];
} 
#include "vmware_pack_end.h"
VixMsgFaultToleranceControlRequest;



/*
 * **********************************************************
 * Shared folder operations.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgSharedFolderRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   int32                     index;
   uint32                    shareNameLength;
   uint32                    hostPathNameLength;
}
#include "vmware_pack_end.h"
VixMsgSharedFolderRequest;


typedef
#include "vmware_pack_begin.h"
struct VixMsgSharedFolderResponse {
   VixCommandResponseHeader      header;
   int32                         numSharedFolders;
}
#include "vmware_pack_end.h"
VixMsgSharedFolderResponse;


typedef
#include "vmware_pack_begin.h"
struct VixMsgGetSharedFolderInfoResponse {
   VixCommandResponseHeader   header;

   uint32                     shareNameLength;
   uint32                     hostPathNameLength;
   int32                      sharedFolderFlags;
}
#include "vmware_pack_end.h"
VixMsgGetSharedFolderInfoResponse;


/*
 * Add or change a shared folder request.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgSetSharedFolderRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   uint32                    shareNameLength;
   uint32                    hostPathNameLength;
}
#include "vmware_pack_end.h"
VixMsgSetSharedFolderRequest;



/*
 * **********************************************************
 * Get properties of a disk.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgGetDiskPropertiesRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   uint32                    diskPathNameLength;
}
#include "vmware_pack_end.h"
VixMsgGetDiskPropertiesRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgGetDiskPropertiesResponse {
   VixCommandResponseHeader  header;

   int64                     capacity;
   int64                     spaceUsed;
   int32                     diskLibDiskType;
   uint32                    physicalPathLength;
}
#include "vmware_pack_end.h"
VixMsgGetDiskPropertiesResponse;

/*
 * **********************************************************
 * Open a URL in the guest.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgOpenUrlRequest {
   VixCommandRequestHeader  header;

   int32                    windowState;
   uint64                   urlLength;
}
#include "vmware_pack_end.h"
VixMsgOpenUrlRequest;


/*
 * **********************************************************
 * Capture the screen of a VM
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgCaptureScreenRequest {
   VixCommandRequestHeader header;
   
   int32                   format;  // Identifies the requested data format.
   int32                    maxSize; // Max data response size in bytes
                                    //    (-1 is any size)

   int32                    captureScreenOptions;
}
#include "vmware_pack_end.h"
VixMsgCaptureScreenRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgCaptureScreenResponse {
   VixCommandResponseHeader header;
   
   int32                   format; // Format of the data in the response.
   uint32                  dataOffset; // Relative to the address of this struct.
}
#include "vmware_pack_end.h"
VixMsgCaptureScreenResponse;

/*
 * **********************************************************
 * Run a script in the guest.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgRunScriptRequest {
   VixCommandRequestHeader header;

   int32                   scriptOptions;

   uint32                  interpreterNameLength;
   uint32                  scriptLength;
   uint32                  propertiesLength;
}
#include "vmware_pack_end.h"
VixMsgRunScriptRequest;


/*
 * **********************************************************
 * An unsupported command. This is used to test future versions
 * of the API sending us commands we don't recognize.
 */
typedef
#include "vmware_pack_begin.h"
struct VixUnsupportedCommandRequest {
   VixCommandRequestHeader   header;
   char                      junk[2053];
}
#include "vmware_pack_end.h"
VixUnsupportedCommandRequest;


/*
 * **********************************************************
 * Create a session key between the client and the VMX.
 */
typedef
#include "vmware_pack_begin.h"
struct VixCommandMakeSessionKeyRequest {
   VixCommandRequestHeader   header;

   int32                     keyOptions;
   int32                     timeout;
   uint32                    responseKeyLength;
   int32                     responseKeyCypherType;
   int32                     cypherType;
}
#include "vmware_pack_end.h"
VixCommandMakeSessionKeyRequest;


typedef
#include "vmware_pack_begin.h"
struct VixCommandMakeSessionKeyResponse {
   VixCommandResponseHeader     header;

   int32                        keyOptions;
   int32                        timeout;
   uint32                       keyLength;
   int32                        cypherType;
}
#include "vmware_pack_end.h"
VixCommandMakeSessionKeyResponse;


enum {
   VIX_CYPHERTYPE_NONE        = 0,
   VIX_CYPHERTYPE_DEFAULT     = 1,
};


/*
 * **********************************************************
 * Kill a guest process.
 */

typedef
#include "vmware_pack_begin.h"
struct VixCommandKillProcessRequest {
   VixCommandRequestHeader    header;

   uint64                     pid;
   uint32                     options;
}
#include "vmware_pack_end.h"
VixCommandKillProcessRequest;


/*
 * **********************************************************
 * Read and write variables like guest variables and config values.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgReadVariableRequest {
   VixCommandRequestHeader header;

   int32                   variableType;
   int32                   options;
   uint32                  nameLength;
}
#include "vmware_pack_end.h"
VixMsgReadVariableRequest;


typedef
#include "vmware_pack_begin.h"
struct VixMsgReadVariableResponse {
   VixCommandResponseHeader   header;

   int32                      valueType;
   int32                      valueProperties;
   uint32                     valueLength;
}
#include "vmware_pack_end.h"
VixMsgReadVariableResponse;


/*
 * Several snapshot operations for a running VM.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgWriteVariableRequest {
   VixCommandRequestHeader header;

   int32                   variableType;
   int32                   options;

   uint32                  nameLength;
   uint32                  valueLength;
}
#include "vmware_pack_end.h"
VixMsgWriteVariableRequest;



/*
 * **********************************************************
 * Perform a create file operation (like createDir or moveFile)
 * on a file in the guest. This lets you pass in things like the initial file
 * properties.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgCreateTempFileRequest {
   VixCommandRequestHeader header;

   int32                   options;
   uint32                  propertyNameLength;
   uint32                  filePrefixLength;
   uint32                  fileSuffixLength;
}
#include "vmware_pack_end.h"
VixMsgCreateTempFileRequest;


/*
 * **********************************************************
 * Connect/Disconnect device request. The response is just a generic
 * response header (it has no body).
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgConnectDeviceRequest {
   VixCommandRequestHeader    header;
   int32                      options;
   Bool                       connected;
   uint32                     nameLength;
}
#include "vmware_pack_end.h"
VixMsgConnectDeviceRequest;
/*
 * **********************************************************
 * Get the list of VProbes exported by a given VM. The request is
 * parameterless, and so generic.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgGetVProbesResponse {
   VixCommandResponseHeader header;
   int32                    numEntries;
}
#include "vmware_pack_end.h"
VixMsgGetVProbesResponse;

/*
 * **********************************************************
 * Load a vprobe script into a given VM. The request is a string
 * to be loaded. The response is a collection of warning and error
 * strings; zero errors indicates success.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgVProbeLoadRequest {
   VixCommandResponseHeader header;
   char   string[1];             /* variable length */
} 
#include "vmware_pack_end.h"
VixMsgVProbeLoadRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgVProbeLoadResponse {
   VixCommandResponseHeader header;
   uint32 numWarnings;
   uint32 numErrors;
   char   warningsAndErrors[1]; /* variable length */
}
#include "vmware_pack_end.h"
VixMsgVProbeLoadResponse;

/*
 * **********************************************************
 * Get the state of a virtual device.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgGetDeviceStateRequest {
   VixCommandRequestHeader header;

   int32                   options;
   uint32                  nameLength;
}
#include "vmware_pack_end.h"
VixMsgGetDeviceStateRequest;


/*
 * This is used to reply to IsDeviceConnected operations.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgGetDeviceStateResponse {
   VixCommandResponseHeader   header;
   Bool                       connected;
   int32                      stateFlags;
   // Maybe capacity and percent allocated?
}
#include "vmware_pack_end.h"
VixMsgGetDeviceStateResponse;


/*
 * **********************************************************
 * Enable/disable all shared folders on this VM. The response
 * is just a generic response header (it has no body).
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgEnableSharedFoldersRequest {
   VixCommandRequestHeader   header;
   Bool                      enabled;
   int32                     sharedFolderOptions;
}
#include "vmware_pack_end.h"
VixMsgEnableSharedFoldersRequest;


/*
 * **********************************************************
 * Mount volumes in the guest.
 */

enum VixMountOptions {
   VIX_MOUNT_ALL              = 0x0001,
   VIX_MOUNT_REMOUNT_FIRST    = 0x0002,
};


typedef
#include "vmware_pack_begin.h"
struct VixMsgMountHGFSRequest {
   VixCommandRequestHeader header;

   int32                   mountOptions;
   int32                   mountType;

   /* The str path list has the form "host1\0dest1\0host2\0dest2\0host3\0dest3\0\0" */
   uint32                  pathListLength;
}
#include "vmware_pack_end.h"
VixMsgMountHGFSRequest;


/*
 * **********************************************************
 * Wait for the VM to be in a specific state.
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgWaitForState {
   VixCommandRequestHeader header;
   int32 state;
   int32 options;
}
#include "vmware_pack_end.h"
VixMsgWaitForState;


/*
 * **********************************************************
 * Get the state of all USB devices.
 */


/*
 * This is used to reply to VixMsgGetUSBDeviceListRequest operations.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgGetUSBDeviceListResponse {
   VixCommandResponseHeader   header;

   int32                      numRunningDevices;
   uint32                     runningDeviceListLength;

   uint32                     patternListLength;
}
#include "vmware_pack_end.h"
VixMsgGetUSBDeviceListResponse;


/*
 * Get guest networking config
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgGetGuestNetworkingConfigRequest {
   VixCommandRequestHeader   header;

   int32                     options;
}
#include "vmware_pack_end.h"
VixMsgGetGuestNetworkingConfigRequest;


/*
 * Set guest networking config
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgSetGuestNetworkingConfigRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   uint32                    bufferSize;
}
#include "vmware_pack_end.h"
VixMsgSetGuestNetworkingConfigRequest;


/*
 * Query VMX performance data
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgGetPerformanceDataRequest {
   VixCommandRequestHeader   header;

   // unused for now, but left for future expansion in case we
   // get such a large list that we want to pass the desired properties.
   int32                     options;
   uint32                    sizeOfPropertyList;
   // This is followed by the buffer of properties we wish to fetch
}
#include "vmware_pack_end.h"
VixMsgGetPerformanceDataRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgGetPerformanceDataResponse {
   VixCommandResponseHeader   header;
   uint32                     bufferSize;
   // This is followed by the buffer of serialized properties
}
#include "vmware_pack_end.h"
VixMsgGetPerformanceDataResponse;


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

typedef
#include "vmware_pack_begin.h"
struct VixMsgSampleCommandRequest {
   VixCommandRequestHeader header;

   int32                   intArg;
   uint32                  strArgLength;
}
#include "vmware_pack_end.h"
VixMsgSampleCommandRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgSampleCommandResponse {
   VixCommandResponseHeader   header;

   int32                      intResult;
   uint32                     strResultLength;
} 
#include "vmware_pack_end.h"
VixMsgSampleCommandResponse;

// End of "HOWTO: Adding a new Vix Command. Step 3."



/*
 * **********************************************************
 * Report and manage the state of a Msg_Post dialogs.
 */

/*
 * This is used to report a MsgPost is opening.
 * This is the non-localized version. It passes the original format string
 * and a list of vararg-style parameters.
 *
 * See VixMsgOpenMsgPostEventLocalized for the localized version.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgOpenMsgPostEvent {
   VixMsgEventHeader          eventHeader;

   int32                      dialogType;
   uint64                     dialogCookie;
   int32                      dialogOptions;

   int32                      severity;
   int32                      defaultAnswer;
   int32                      percent;
   int32                      cancelButton;
   int32                      hintOptions;

   uint32                     localeStrLength;
   int32                      numMessages;
   int32                      numButtons;

   /*
    * Followed by:
    *       A locale string (a null-terminated string)
    *       A list of messages, each is stored in one VixMsgDialogStr.
    *       A list of button strings (each is a null-terminated string.
    */
}
#include "vmware_pack_end.h"
VixMsgOpenMsgPostEvent;

/*
 * These are the flags set in the dialogOptions field.
 */
enum VixMsgPostStateValues {
   VIX_COMMAND_DIALOG_OPTIONS_VMX_IS_BLOCKED       = 0x01,
};


/*
 * This is one string in the message. It corresponds to a
 * single Msg_List object.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgDialogStr {
   uint32         idLength;
   uint32         formatLength;
   int32          numArgs;
   /*
    * Followed by:
    *       The ID string (with NULL terminator)
    *       The format string (with NULL terminator)
    *       A list of arguments, each is one VixMsgDialogStrArg.
    */
}
#include "vmware_pack_end.h"
VixMsgDialogStr;


/*
 * This is one argument to the message. It corresponds to a
 * single MsgFmt_Arg object.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgDialogStrArg {
   int32          argType;
   uint32         argSize;
   /*
    * Followed by the actual argument data.
    */
}
#include "vmware_pack_end.h"
VixMsgDialogStrArg;


/*
 * This is used to report a MsgPost is closing.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgCloseUIDialogEvent {
   VixMsgEventHeader          eventHeader;

   uint64                     dialogCookie;
   int32                      numMessages;

   /*
    * Followed by:
    *       A list of strings, each is one MsgPost Id.
    */
}
#include "vmware_pack_end.h"
VixMsgCloseUIDialogEvent;


/*
 * Answer a Msg_Post post/hint/question
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgAnswerMsgPost {
   VixCommandRequestHeader   header;

   uint64                    dialogCookie;

   int32                     options;

   int32                     answer;
   void                      *progressState;

   uint32                    idStrSize;
   uint32                    propertyListBufferSize;

   /*
    * Followed by:
    *       msgIdStr.
    *       The serialized properties
    */
}
#include "vmware_pack_end.h"
VixMsgAnswerMsgPost;



typedef
#include "vmware_pack_begin.h"
struct VixMsgSetLocaleRequest {
   VixCommandRequestHeader header;

   int32                   localeOptions;
   
   uint32                  localeStrLength;
   char                    localeStr[1];
   // This is followed by the country code string.
}
#include "vmware_pack_end.h"
VixMsgSetLocaleRequest;



/*
 * This is used to report progress.
 */
typedef
#include "vmware_pack_begin.h"
struct VixMsgLazyProgressEvent {
   VixMsgEventHeader          eventHeader;

   uint64                     dialogCookie;
   int32                      percent;
}
#include "vmware_pack_end.h"
VixMsgLazyProgressEvent;



/*
 * **********************************************************
 *  Debugger related commands.
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgAttachDebuggerRequest {
   VixCommandRequestHeader   header;
   
   int32                     options;
   uint32                    propertyListBufferSize;
}  
#include "vmware_pack_end.h"
VixMsgAttachDebuggerRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgAttachDebuggerResponse {
   VixCommandResponseHeader header;
   uint32   propertyListBufferSize;
} 
#include "vmware_pack_end.h"
VixMsgAttachDebuggerResponse;

typedef
#include "vmware_pack_begin.h"
struct VixMsgIssueDebuggerCommandRequest {
   VixCommandRequestHeader   header;

   int32                     options;
   uint32                    propertyListBufferSize;
   uint32                    debuggerBlobBufferSize;
}
#include "vmware_pack_end.h"
VixMsgIssueDebuggerCommandRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgIssueDebuggerCommandResponse {
   VixCommandResponseHeader header;
   uint32   propertyListBufferSize;
   uint32   debuggerBlobBufferSize;
}
#include "vmware_pack_end.h"
VixMsgIssueDebuggerCommandResponse;

typedef
#include "vmware_pack_begin.h"
struct VixMsgDetachDebuggerRequest {
   VixCommandRequestHeader   header;
  
   int32                     options;
   uint32                    propertyListBufferSize;
} 
#include "vmware_pack_end.h"
VixMsgDetachDebuggerRequest;

typedef
#include "vmware_pack_begin.h"
struct VixMsgDetachDebuggerResponse {
   VixCommandResponseHeader header;
   uint32   propertyListBufferSize;
}
#include "vmware_pack_end.h"
VixMsgDetachDebuggerResponse;

/*
 * **********************************************************
 * VM Pause state change event format
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgPauseStateChangedEvent {
   VixMsgEventHeader          eventHeader;
   Bool                       paused;
}
#include "vmware_pack_end.h"
VixMsgPauseStateChangedEvent;


/*
 * **********************************************************
 * Wait for a user action, such as a user logging into the guest.
 */

/*
 * Vix_WaitForUserActionInGuest Request
 * VIX_COMMAND_WAIT_FOR_USER_ACTION_IN_GUEST
 */

typedef
#include "vmware_pack_begin.h"
struct VixMsgWaitForUserActionRequest {
   VixCommandResponseHeader   header;

   int32                      userType;
   int32                      userAction;

   int32                      timeoutInSeconds;
   int32                      options;

   uint32                     userNameLength;
   uint32                     propertyBufferSize;

   // This is followed by:
   //    userName
   //    buffer of serialized properties
}
#include "vmware_pack_end.h"
VixMsgWaitForUserActionRequest;


typedef
#include "vmware_pack_begin.h"
struct VixMsgWaitForUserActionResponse {
   VixCommandRequestHeader    header;

   Bool                       actionHappened;

   uint32                     bufferSize;
   // This is followed by the buffer of serialized properties
}
#include "vmware_pack_end.h"
VixMsgWaitForUserActionResponse;




/*
 * These are values we use to discover hosts and guests through SLPv2.
 */
#define VIX_SLPV2_SERVICE_NAME_TOOLS_SERVICE       "VMware_Vix_Tools"
#define VIX_SLPV2_PROPERTY_IP_ADDR                 "IP"
#define VIX_SLPV2_PROPERTY_MAC_ADDR                "Mac"



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
   VIX_COMMAND_GET_PROPERTY                     = 5,
   VIX_COMMAND_SET_PROPERTY                     = 6,
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
   VIX_COMMAND_CREATE_DISK                      = 29,
   VIX_COMMAND_CREATE_FLOPPY                    = 30,
   VIX_COMMAND_RELOAD_VM                        = 31,
   VIX_COMMAND_DELETE_VM                        = 32,
   VIX_COMMAND_SYNCDRIVER_FREEZE                = 33,
   VIX_COMMAND_SYNCDRIVER_THAW                  = 34,
   VIX_COMMAND_HOT_ADD_DISK                     = 35,
   VIX_COMMAND_HOT_REMOVE_DISK                  = 36,
   VIX_COMMAND_SET_GUEST_PRINTER                = 37,
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
   VIX_COMMAND_GET_DISK_PROPERTIES              = 48,
   VIX_COMMAND_OPEN_URL                         = 49,
   VIX_COMMAND_GET_HANDLE_STATE                 = 50,
   VIX_COMMAND_SET_HANDLE_STATE                 = 51,
   VIX_COMMAND_CREATE_WORKING_COPY              = 55, // DELETE this when we switch remote foundry to VIM
   VIX_COMMAND_DISCARD_WORKING_COPY             = 56, // DELETE this when we switch remote foundry to VIM
   VIX_COMMAND_SAVE_WORKING_COPY                = 57, // DELETE this when we switch remote foundry to VIM
   VIX_COMMAND_CAPTURE_SCREEN                   = 58,
   VIX_COMMAND_GET_VMDB_VALUES                  = 59,
   VIX_COMMAND_SET_VMDB_VALUES                  = 60,
   VIX_COMMAND_READ_XML_FILE                    = 61,
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
   VIX_VM_FORK_COMMAND                          = 86,
   VIX_COMMAND_LOGOUT_IN_GUEST                  = 87,
   VIX_COMMAND_READ_VARIABLE                    = 88,
   VIX_COMMAND_WRITE_VARIABLE                   = 89,
   VIX_COMMAND_CONNECT_DEVICE                   = 92,
   VIX_COMMAND_IS_DEVICE_CONNECTED              = 93,
   VIX_COMMAND_GET_FILE_INFO                    = 94,
   VIX_COMMAND_SET_FILE_INFO                    = 95,
   VIX_COMMAND_MOUSE_EVENTS                     = 96,
   VIX_COMMAND_OPEN_TEAM                        = 97,
   VIX_COMMAND_FIND_HOST_DEVICES                = 98,
   VIX_COMMAND_ANSWER_MESSAGE                   = 99,
   VIX_COMMAND_ENABLE_SHARED_FOLDERS            = 100,
   VIX_COMMAND_MOUNT_HGFS_FOLDERS               = 101,
   VIX_COMMAND_HOT_EXTEND_DISK                  = 102,

   VIX_COMMAND_GET_VPROBES_VERSION              = 104,
   VIX_COMMAND_GET_VPROBES                      = 105,
   VIX_COMMAND_VPROBE_GET_GLOBALS               = 106,
   VIX_COMMAND_VPROBE_LOAD                      = 107,
   VIX_COMMAND_VPROBE_RESET                     = 108,

   VIX_COMMAND_LIST_USB_DEVICES                 = 109,
   VIX_COMMAND_CONNECT_HOST                     = 110,
   VIX_COMMAND_WAIT_FOR_OPPORTUNE_MOMENT        = 111,

   VIX_COMMAND_CREATE_LINKED_CLONE              = 112,

   VIX_COMMAND_STOP_SNAPSHOT_LOG_RECORDING      = 113,
   VIX_COMMAND_STOP_SNAPSHOT_LOG_PLAYBACK       = 114,
   /*
    * HOWTO: Adding a new Vix Command. Step 2.
    *
    * Add a new ID for your new function prototype here. BE CAREFUL. The
    * OFFICIAL list of id's is in the bfg-main tree, in bora/lib/public/vixCommands.h.
    * When people add new command id's in different tree, they may collide and use
    * the same ID values. This can merge without conflicts, and cause runtime bugs.
    */
   VIX_COMMAND_SAMPLE_COMMAND                   = 115,

   VIX_COMMAND_GET_GUEST_NETWORKING_CONFIG      = 116,
   VIX_COMMAND_SET_GUEST_NETWORKING_CONFIG      = 117,

   VIX_COMMAND_FAULT_TOLERANCE_REGISTER         = 118,
   VIX_COMMAND_FAULT_TOLERANCE_UNREGISTER       = 119,
   VIX_COMMAND_FAULT_TOLERANCE_CONTROL          = 120,
   VIX_COMMAND_FAULT_TOLERANCE_QUERY_SECONDARY  = 121,

   VIX_COMMAND_VM_PAUSE                         = 122,
   VIX_COMMAND_VM_UNPAUSE                       = 123,
   VIX_COMMAND_GET_SNAPSHOT_LOG_INFO            = 124,
   VIX_COMMAND_SET_REPLAY_SPEED                 = 125,

   VIX_COMMAND_ANSWER_USER_MESSAGE              = 126,
   VIX_COMMAND_SET_CLIENT_LOCALE                = 127,

   VIX_COMMAND_GET_PERFORMANCE_DATA             = 128,

   VIX_COMMAND_REFRESH_RUNTIME_PROPERTIES       = 129,

   VIX_COMMAND_GET_SNAPSHOT_SCREENSHOT          = 130,
   VIX_COMMAND_ADD_TIMEMARKER                   = 131,

   VIX_COMMAND_WAIT_FOR_USER_ACTION_IN_GUEST    = 132,
   VIX_COMMAND_VMDB_END_TRANSACTION             = 133,
   VIX_COMMAND_VMDB_SET                         = 134,

   VIX_COMMAND_CHANGE_VIRTUAL_HARDWARE          = 135,

   VIX_COMMAND_HOT_PLUG_CPU                     = 136,
   VIX_COMMAND_HOT_PLUG_MEMORY                  = 137,
   VIX_COMMAND_HOT_ADD_DEVICE                   = 138,
   VIX_COMMAND_HOT_REMOVE_DEVICE                = 139,

   VIX_COMMAND_DEBUGGER_ATTACH                  = 140,
   VIX_COMMAND_DEBUGGER_DETACH                  = 141,
   VIX_COMMAND_DEBUGGER_SEND_COMMAND            = 142,

   VIX_COMMAND_GET_RECORD_STATE                 = 143,
   VIX_COMMAND_SET_RECORD_STATE                 = 144,
   VIX_COMMAND_REMOVE_RECORD_STATE              = 145,
   VIX_COMMAND_GET_REPLAY_STATE                 = 146,
   VIX_COMMAND_SET_REPLAY_STATE                 = 147,
   VIX_COMMAND_REMOVE_REPLAY_STATE              = 148,

   VIX_COMMAND_CANCEL_USER_PROGRESS_MESSAGE     = 150,
   
   VIX_COMMAND_GET_VMX_DEVICE_STATE             = 151,

   VIX_COMMAND_GET_NUM_TIMEMARKERS              = 152,
   VIX_COMMAND_GET_TIMEMARKER                   = 153,
   VIX_COMMAND_REMOVE_TIMEMARKER                = 154,

   VIX_COMMAND_SET_SNAPSHOT_INFO                = 155,
   VIX_COMMAND_SNAPSHOT_SET_MRU                 = 156,

   VIX_COMMAND_LAST_NORMAL_COMMAND              = 157,

   VIX_TEST_UNSUPPORTED_TOOLS_OPCODE_COMMAND    = 998,
   VIX_TEST_UNSUPPORTED_VMX_OPCODE_COMMAND      = 999,
};

   

/*
 * These are the command names that are passed through VMDB. These correspond
 * to the TestCommandType values.
 */
#define VIX_VMDBCOMMAND_SET_GUEST_PRINTER       "SetGuestPrinter"
#define VIX_VMDBCOMMAND_OPEN_URL                "OpenUrl"


/*
 * These are the command names that are passed through the backdoor from the
 * VMX to the tools.
 */
#define VIX_BACKDOOR_COMMAND_VERSION               "Vix_1_"
#define VIX_BACKDOORCOMMAND_RUN_PROGRAM            VIX_BACKDOOR_COMMAND_VERSION"Run_Program"
#define VIX_BACKDOORCOMMAND_SET_GUEST_PRINTER      VIX_BACKDOOR_COMMAND_VERSION"Printer_Set"
#define VIX_BACKDOORCOMMAND_SYNCDRIVER_FREEZE      VIX_BACKDOOR_COMMAND_VERSION"SyncDriver_Freeze"
#define VIX_BACKDOORCOMMAND_SYNCDRIVER_THAW        VIX_BACKDOOR_COMMAND_VERSION"SyncDriver_Thaw"
#define VIX_BACKDOORCOMMAND_OPEN_URL               VIX_BACKDOOR_COMMAND_VERSION"Open_Url"
#define VIX_BACKDOORCOMMAND_GET_PROPERTIES         VIX_BACKDOOR_COMMAND_VERSION"Get_ToolsProperties"
#define VIX_BACKDOORCOMMAND_CHECK_USER_ACCOUNT     VIX_BACKDOOR_COMMAND_VERSION"Check_User_Account"
#define VIX_BACKDOORCOMMAND_SEND_HGFS_PACKET       VIX_BACKDOOR_COMMAND_VERSION"Send_Hgfs_Packet"
#define VIX_BACKDOORCOMMAND_UNRECOGNIZED_COMMAND   VIX_BACKDOOR_COMMAND_VERSION"Unrecognized_Command"
#define VIX_BACKDOORCOMMAND_COMMAND                VIX_BACKDOOR_COMMAND_VERSION"Relayed_Command"
#define VIX_BACKDOORCOMMAND_MOUNT_VOLUME_LIST      VIX_BACKDOOR_COMMAND_VERSION"Mount_Volumes"


/*
 * This is the set of features that may be supported by different
 * versions of the VMX or Vix Tools.
 */
enum VixToolsFeatures {
   VIX_TOOLSFEATURE_SUPPORT_GET_HANDLE_STATE   = 0x0001,
   VIX_TOOLSFEATURE_SUPPORT_OPEN_URL           = 0x0002,
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
#define VIX_BACKDOORCOMMAND_PROXY_MESSAGE       "VIX_Proxy_Msg"


#define VIX_HOST_FOR_THIS_GUEST_OS "self"
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
struct VixCommandRequestHeader *VixMsg_AllocRequestMsg(size_t msgHeaderAndBodyLength,
                                                       int opCode,
                                                       uint64 cookie,
                                                       int credentialType,
                                                       const char *userNamePassword);

struct VixCommandResponseHeader *VixMsg_AllocResponseMsg(struct VixCommandRequestHeader *requestHeader,
                                                         VixError error,
                                                         uint32 additionalError,
                                                         size_t responseBodyLength,
                                                         void *responseBody,
                                                         size_t *responseMsgLength);

void VixMsg_InitResponseMsg(struct VixCommandResponseHeader *responseHeader,
                            struct VixCommandRequestHeader *requestHeader,
                            VixError error,
                            uint32 additionalError,
                            size_t totalMessageLength);

VixError VixMsg_ValidateMessage(void *vMsg, size_t msgLength);

VixError VixMsg_ValidateRequestMsg(void *vMsg, size_t msgLength);

VixError VixMsg_ValidateResponseMsg(void *vMsg, size_t msgLength);

VixError VixMsg_ParseWriteVariableRequest(VixMsgWriteVariableRequest *msg,
                                          char **valueName,
                                          char **value);

char *VixMsg_ObfuscateNamePassword(const char *userName,
                                   const char *password);

Bool VixMsg_DeObfuscateNamePassword(const char *packagedName,
                                    char **userNameResult,
                                    char **passwordResult);

char *VixMsg_EncodeString(const char *str);

char *VixMsg_DecodeString(const char *str);
#endif   // VIX_HIDE_FROM_JAVA




#endif // _VIX_COMMANDS_H_

