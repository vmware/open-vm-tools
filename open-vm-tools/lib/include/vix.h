/*********************************************************
 * Copyright (C) 2004-2020 VMware, Inc. All rights reserved.
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
 * This is the C interface to the VIX API.
 * This is platform-independent.
 */

#ifndef _VIX_H_
#define _VIX_H_

#ifdef __cplusplus
extern "C" {
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Basic Types --
 *
 *-----------------------------------------------------------------------------
 */

#include "vm_basic_types.h"


typedef int VixHandle;
enum {
   VIX_INVALID_HANDLE   = 0,
};

/*
 * These are the types of handles.
 */
typedef int VixHandleType;
enum {
   VIX_HANDLETYPE_NONE                 = 0,
   VIX_HANDLETYPE_HOST                 = 2,
   VIX_HANDLETYPE_VM                   = 3,
   VIX_HANDLETYPE_NETWORK              = 5,
   VIX_HANDLETYPE_JOB                  = 6,
   VIX_HANDLETYPE_SNAPSHOT             = 7,
   VIX_HANDLETYPE_PROPERTY_LIST        = 9,
   VIX_HANDLETYPE_METADATA_CONTAINER   = 11
};

/*
 * The "//{{ Begin VIX_ERROR  }}" and "//{{ End VIX_ERROR }}" lines are
 * to bracket the error code definitions that will be copied over
 * to vixDiskLib.h during build time. If you modify these two lines, please
 * make sure you also change bora/lib/distribute/vixDiskLib.h and
 * bora/support/scripts/replaceVixErrors.py
 */

// {{ Begin VIX_ERROR }}
/*
 * An error is a 64-bit value. If there is no error, then the value is
 * set to VIX_OK. If there is an error, then the least significant bits
 * will be set to one of the integer error codes defined below. The more
 * significant bits may or may not be set to various values, depending on 
 * the errors.
 */
typedef uint64 VixError;
#define VIX_ERROR_CODE(err)   ((err) & 0xFFFF)
#define VIX_SUCCEEDED(err)    (VIX_OK == (err))
#define VIX_FAILED(err)       (VIX_OK != (err))

/*
 * The error codes are returned by all public VIX routines.
 */
enum {
   VIX_OK                                       = 0,

   /* General errors */
   VIX_E_FAIL                                   = 1,
   VIX_E_OUT_OF_MEMORY                          = 2,
   VIX_E_INVALID_ARG                            = 3,
   VIX_E_FILE_NOT_FOUND                         = 4,
   VIX_E_OBJECT_IS_BUSY                         = 5,
   VIX_E_NOT_SUPPORTED                          = 6,
   VIX_E_FILE_ERROR                             = 7,
   VIX_E_DISK_FULL                              = 8,
   VIX_E_INCORRECT_FILE_TYPE                    = 9,
   VIX_E_CANCELLED                              = 10,
   VIX_E_FILE_READ_ONLY                         = 11,
   VIX_E_FILE_ALREADY_EXISTS                    = 12,
   VIX_E_FILE_ACCESS_ERROR                      = 13,
   VIX_E_REQUIRES_LARGE_FILES                   = 14,
   VIX_E_FILE_ALREADY_LOCKED                    = 15,
   VIX_E_VMDB                                   = 16,
   VIX_E_NOT_SUPPORTED_ON_REMOTE_OBJECT         = 20,
   VIX_E_FILE_TOO_BIG                           = 21,
   VIX_E_FILE_NAME_INVALID                      = 22,
   VIX_E_ALREADY_EXISTS                         = 23,
   VIX_E_BUFFER_TOOSMALL                        = 24,
   VIX_E_OBJECT_NOT_FOUND                       = 25,
   VIX_E_HOST_NOT_CONNECTED                     = 26,
   VIX_E_INVALID_UTF8_STRING                    = 27,
   VIX_E_OPERATION_ALREADY_IN_PROGRESS          = 31,
   VIX_E_UNFINISHED_JOB                         = 29,
   VIX_E_NEED_KEY                               = 30,
   VIX_E_LICENSE                                = 32,
   VIX_E_VM_HOST_DISCONNECTED                   = 34,
   VIX_E_AUTHENTICATION_FAIL                    = 35,
   VIX_E_HOST_CONNECTION_LOST                   = 36,
   VIX_E_DUPLICATE_NAME                         = 41,
   VIX_E_ARGUMENT_TOO_BIG                       = 44,

   /* Handle Errors */
   VIX_E_INVALID_HANDLE                         = 1000,
   VIX_E_NOT_SUPPORTED_ON_HANDLE_TYPE           = 1001,
   VIX_E_TOO_MANY_HANDLES                       = 1002,

   /* XML errors */
   VIX_E_NOT_FOUND                              = 2000,
   VIX_E_TYPE_MISMATCH                          = 2001,
   VIX_E_INVALID_XML                            = 2002,

   /* VM Control Errors */
   VIX_E_TIMEOUT_WAITING_FOR_TOOLS              = 3000,
   VIX_E_UNRECOGNIZED_COMMAND                   = 3001,
   VIX_E_OP_NOT_SUPPORTED_ON_GUEST              = 3003,
   VIX_E_PROGRAM_NOT_STARTED                    = 3004,
   VIX_E_CANNOT_START_READ_ONLY_VM              = 3005,
   VIX_E_VM_NOT_RUNNING                         = 3006,
   VIX_E_VM_IS_RUNNING                          = 3007,
   VIX_E_CANNOT_CONNECT_TO_VM                   = 3008,
   VIX_E_POWEROP_SCRIPTS_NOT_AVAILABLE          = 3009,
   VIX_E_NO_GUEST_OS_INSTALLED                  = 3010,
   VIX_E_VM_INSUFFICIENT_HOST_MEMORY            = 3011,
   VIX_E_SUSPEND_ERROR                          = 3012,
   VIX_E_VM_NOT_ENOUGH_CPUS                     = 3013,
   VIX_E_HOST_USER_PERMISSIONS                  = 3014,
   VIX_E_GUEST_USER_PERMISSIONS                 = 3015,
   VIX_E_TOOLS_NOT_RUNNING                      = 3016,
   VIX_E_GUEST_OPERATIONS_PROHIBITED            = 3017,
   VIX_E_ANON_GUEST_OPERATIONS_PROHIBITED       = 3018,
   VIX_E_ROOT_GUEST_OPERATIONS_PROHIBITED       = 3019,
   VIX_E_MISSING_ANON_GUEST_ACCOUNT             = 3023,
   VIX_E_CANNOT_AUTHENTICATE_WITH_GUEST         = 3024,
   VIX_E_UNRECOGNIZED_COMMAND_IN_GUEST          = 3025,
   VIX_E_CONSOLE_GUEST_OPERATIONS_PROHIBITED    = 3026,
   VIX_E_MUST_BE_CONSOLE_USER                   = 3027,
   VIX_E_VMX_MSG_DIALOG_AND_NO_UI               = 3028,
   /* VIX_E_NOT_ALLOWED_DURING_VM_RECORDING        = 3029, Removed in version 1.11 */
   /* VIX_E_NOT_ALLOWED_DURING_VM_REPLAY           = 3030, Removed in version 1.11 */
   VIX_E_OPERATION_NOT_ALLOWED_FOR_LOGIN_TYPE   = 3031,
   VIX_E_LOGIN_TYPE_NOT_SUPPORTED               = 3032,
   VIX_E_EMPTY_PASSWORD_NOT_ALLOWED_IN_GUEST    = 3033,
   VIX_E_INTERACTIVE_SESSION_NOT_PRESENT        = 3034,
   VIX_E_INTERACTIVE_SESSION_USER_MISMATCH      = 3035,
   /* VIX_E_UNABLE_TO_REPLAY_VM                    = 3039, Removed in version 1.11 */
   VIX_E_CANNOT_POWER_ON_VM                     = 3041,
   VIX_E_NO_DISPLAY_SERVER                      = 3043,
   /* VIX_E_VM_NOT_RECORDING                       = 3044, Removed in version 1.11 */
   /* VIX_E_VM_NOT_REPLAYING                       = 3045, Removed in version 1.11 */
   VIX_E_TOO_MANY_LOGONS                        = 3046,
   VIX_E_INVALID_AUTHENTICATION_SESSION         = 3047,

   /* VM Errors */
   VIX_E_VM_NOT_FOUND                           = 4000,
   VIX_E_NOT_SUPPORTED_FOR_VM_VERSION           = 4001,
   VIX_E_CANNOT_READ_VM_CONFIG                  = 4002,
   VIX_E_TEMPLATE_VM                            = 4003,
   VIX_E_VM_ALREADY_LOADED                      = 4004,
   VIX_E_VM_ALREADY_UP_TO_DATE                  = 4006,
   VIX_E_VM_UNSUPPORTED_GUEST                   = 4011,

   /* Property Errors */
   VIX_E_UNRECOGNIZED_PROPERTY                  = 6000,
   VIX_E_INVALID_PROPERTY_VALUE                 = 6001,
   VIX_E_READ_ONLY_PROPERTY                     = 6002,
   VIX_E_MISSING_REQUIRED_PROPERTY              = 6003,
   VIX_E_INVALID_SERIALIZED_DATA                = 6004,
   VIX_E_PROPERTY_TYPE_MISMATCH                 = 6005,

   /* Completion Errors */
   VIX_E_BAD_VM_INDEX                           = 8000,

   /* Message errors */
   VIX_E_INVALID_MESSAGE_HEADER                 = 10000,
   VIX_E_INVALID_MESSAGE_BODY                   = 10001,

   /* Snapshot errors */
   VIX_E_SNAPSHOT_INVAL                         = 13000,
   VIX_E_SNAPSHOT_DUMPER                        = 13001,
   VIX_E_SNAPSHOT_DISKLIB                       = 13002,
   VIX_E_SNAPSHOT_NOTFOUND                      = 13003,
   VIX_E_SNAPSHOT_EXISTS                        = 13004,
   VIX_E_SNAPSHOT_VERSION                       = 13005,
   VIX_E_SNAPSHOT_NOPERM                        = 13006,
   VIX_E_SNAPSHOT_CONFIG                        = 13007,
   VIX_E_SNAPSHOT_NOCHANGE                      = 13008,
   VIX_E_SNAPSHOT_CHECKPOINT                    = 13009,
   VIX_E_SNAPSHOT_LOCKED                        = 13010,
   VIX_E_SNAPSHOT_INCONSISTENT                  = 13011,
   VIX_E_SNAPSHOT_NAMETOOLONG                   = 13012,
   VIX_E_SNAPSHOT_VIXFILE                       = 13013,
   VIX_E_SNAPSHOT_DISKLOCKED                    = 13014,
   VIX_E_SNAPSHOT_DUPLICATEDDISK                = 13015,
   VIX_E_SNAPSHOT_INDEPENDENTDISK               = 13016,
   VIX_E_SNAPSHOT_NONUNIQUE_NAME                = 13017,
   VIX_E_SNAPSHOT_MEMORY_ON_INDEPENDENT_DISK    = 13018,
   VIX_E_SNAPSHOT_MAXSNAPSHOTS                  = 13019,
   VIX_E_SNAPSHOT_MIN_FREE_SPACE                = 13020,
   VIX_E_SNAPSHOT_HIERARCHY_TOODEEP             = 13021,
   // DEPRECRATED VIX_E_SNAPSHOT_RRSUSPEND                     = 13022,
   VIX_E_SNAPSHOT_NOT_REVERTABLE                = 13024,

   /* Host Errors */
   VIX_E_HOST_DISK_INVALID_VALUE                = 14003,
   VIX_E_HOST_DISK_SECTORSIZE                   = 14004,
   VIX_E_HOST_FILE_ERROR_EOF                    = 14005,
   VIX_E_HOST_NETBLKDEV_HANDSHAKE               = 14006,
   VIX_E_HOST_SOCKET_CREATION_ERROR             = 14007,
   VIX_E_HOST_SERVER_NOT_FOUND                  = 14008,
   VIX_E_HOST_NETWORK_CONN_REFUSED              = 14009,
   VIX_E_HOST_TCP_SOCKET_ERROR                  = 14010,
   VIX_E_HOST_TCP_CONN_LOST                     = 14011,
   VIX_E_HOST_NBD_HASHFILE_VOLUME               = 14012,
   VIX_E_HOST_NBD_HASHFILE_INIT                 = 14013,
   VIX_E_HOST_SERVER_SHUTDOWN                   = 14014,
   VIX_E_HOST_SERVER_NOT_AVAILABLE              = 14015,
   
   /* Disklib errors */
   VIX_E_DISK_INVAL                             = 16000,
   VIX_E_DISK_NOINIT                            = 16001,
   VIX_E_DISK_NOIO                              = 16002,
   VIX_E_DISK_PARTIALCHAIN                      = 16003,
   VIX_E_DISK_NEEDSREPAIR                       = 16006,
   VIX_E_DISK_OUTOFRANGE                        = 16007,
   VIX_E_DISK_CID_MISMATCH                      = 16008,
   VIX_E_DISK_CANTSHRINK                        = 16009,
   VIX_E_DISK_PARTMISMATCH                      = 16010,
   VIX_E_DISK_UNSUPPORTEDDISKVERSION            = 16011,
   VIX_E_DISK_OPENPARENT                        = 16012,
   VIX_E_DISK_NOTSUPPORTED                      = 16013,
   VIX_E_DISK_NEEDKEY                           = 16014,
   VIX_E_DISK_NOKEYOVERRIDE                     = 16015,
   VIX_E_DISK_NOTENCRYPTED                      = 16016,
   VIX_E_DISK_NOKEY                             = 16017,
   VIX_E_DISK_INVALIDPARTITIONTABLE             = 16018,
   VIX_E_DISK_NOTNORMAL                         = 16019,
   VIX_E_DISK_NOTENCDESC                        = 16020,
   VIX_E_DISK_NEEDVMFS                          = 16022,
   VIX_E_DISK_RAWTOOBIG                         = 16024,
   VIX_E_DISK_TOOMANYOPENFILES                  = 16027,
   VIX_E_DISK_TOOMANYREDO                       = 16028,
   VIX_E_DISK_RAWTOOSMALL                       = 16029,
   VIX_E_DISK_INVALIDCHAIN                      = 16030,
   VIX_E_DISK_KEY_NOTFOUND                      = 16052, // metadata key is not found
   VIX_E_DISK_SUBSYSTEM_INIT_FAIL               = 16053,
   VIX_E_DISK_INVALID_CONNECTION                = 16054,
   VIX_E_DISK_ENCODING                          = 16061,
   VIX_E_DISK_CANTREPAIR                        = 16062,
   VIX_E_DISK_INVALIDDISK                       = 16063,
   VIX_E_DISK_NOLICENSE                         = 16064,
   VIX_E_DISK_NODEVICE                          = 16065,
   VIX_E_DISK_UNSUPPORTEDDEVICE                 = 16066,
   VIX_E_DISK_CAPACITY_MISMATCH                 = 16067,
   VIX_E_DISK_PARENT_NOTALLOWED                 = 16068,
   VIX_E_DISK_ATTACH_ROOTLINK                   = 16069,

   /* Crypto Library Errors */
   VIX_E_CRYPTO_UNKNOWN_ALGORITHM               = 17000,
   VIX_E_CRYPTO_BAD_BUFFER_SIZE                 = 17001,
   VIX_E_CRYPTO_INVALID_OPERATION               = 17002,
   VIX_E_CRYPTO_RANDOM_DEVICE                   = 17003,
   VIX_E_CRYPTO_NEED_PASSWORD                   = 17004,
   VIX_E_CRYPTO_BAD_PASSWORD                    = 17005,
   VIX_E_CRYPTO_NOT_IN_DICTIONARY               = 17006,
   VIX_E_CRYPTO_NO_CRYPTO                       = 17007,
   VIX_E_CRYPTO_ERROR                           = 17008,
   VIX_E_CRYPTO_BAD_FORMAT                      = 17009,
   VIX_E_CRYPTO_LOCKED                          = 17010,
   VIX_E_CRYPTO_EMPTY                           = 17011,
   VIX_E_CRYPTO_KEYSAFE_LOCATOR                 = 17012,

   /* Remoting Errors. */
   VIX_E_CANNOT_CONNECT_TO_HOST                 = 18000,
   VIX_E_NOT_FOR_REMOTE_HOST                    = 18001,
   VIX_E_INVALID_HOSTNAME_SPECIFICATION         = 18002,
    
   /* Screen Capture Errors. */
   VIX_E_SCREEN_CAPTURE_ERROR                   = 19000,
   VIX_E_SCREEN_CAPTURE_BAD_FORMAT              = 19001,
   VIX_E_SCREEN_CAPTURE_COMPRESSION_FAIL        = 19002,
   VIX_E_SCREEN_CAPTURE_LARGE_DATA              = 19003,

   /* Guest Errors */
   VIX_E_GUEST_VOLUMES_NOT_FROZEN               = 20000,
   VIX_E_NOT_A_FILE                             = 20001,
   VIX_E_NOT_A_DIRECTORY                        = 20002,
   VIX_E_NO_SUCH_PROCESS                        = 20003,
   VIX_E_FILE_NAME_TOO_LONG                     = 20004,
   VIX_E_OPERATION_DISABLED                     = 20005,

   /* Tools install errors */
   VIX_E_TOOLS_INSTALL_NO_IMAGE                 = 21000,
   VIX_E_TOOLS_INSTALL_IMAGE_INACCESIBLE        = 21001,
   VIX_E_TOOLS_INSTALL_NO_DEVICE                = 21002,
   VIX_E_TOOLS_INSTALL_DEVICE_NOT_CONNECTED     = 21003,
   VIX_E_TOOLS_INSTALL_CANCELLED                = 21004,
   VIX_E_TOOLS_INSTALL_INIT_FAILED              = 21005,
   VIX_E_TOOLS_INSTALL_AUTO_NOT_SUPPORTED       = 21006,
   VIX_E_TOOLS_INSTALL_GUEST_NOT_READY          = 21007,
   VIX_E_TOOLS_INSTALL_SIG_CHECK_FAILED         = 21008,
   VIX_E_TOOLS_INSTALL_ERROR                    = 21009,
   VIX_E_TOOLS_INSTALL_ALREADY_UP_TO_DATE       = 21010,
   VIX_E_TOOLS_INSTALL_IN_PROGRESS              = 21011,
   VIX_E_TOOLS_INSTALL_IMAGE_COPY_FAILED        = 21012,

   /* Wrapper Errors */
   VIX_E_WRAPPER_WORKSTATION_NOT_INSTALLED      = 22001,
   VIX_E_WRAPPER_VERSION_NOT_FOUND              = 22002,
   VIX_E_WRAPPER_SERVICEPROVIDER_NOT_FOUND      = 22003,
   VIX_E_WRAPPER_PLAYER_NOT_INSTALLED           = 22004,
   VIX_E_WRAPPER_RUNTIME_NOT_INSTALLED          = 22005,
   VIX_E_WRAPPER_MULTIPLE_SERVICEPROVIDERS      = 22006,

   /* FuseMnt errors*/
   VIX_E_MNTAPI_MOUNTPT_NOT_FOUND               = 24000,
   VIX_E_MNTAPI_MOUNTPT_IN_USE                  = 24001,
   VIX_E_MNTAPI_DISK_NOT_FOUND                  = 24002,
   VIX_E_MNTAPI_DISK_NOT_MOUNTED                = 24003,
   VIX_E_MNTAPI_DISK_IS_MOUNTED                 = 24004,
   VIX_E_MNTAPI_DISK_NOT_SAFE                   = 24005,
   VIX_E_MNTAPI_DISK_CANT_OPEN                  = 24006,
   VIX_E_MNTAPI_CANT_READ_PARTS                 = 24007,
   VIX_E_MNTAPI_UMOUNT_APP_NOT_FOUND            = 24008,
   VIX_E_MNTAPI_UMOUNT                          = 24009,
   VIX_E_MNTAPI_NO_MOUNTABLE_PARTITONS          = 24010,
   VIX_E_MNTAPI_PARTITION_RANGE                 = 24011,
   VIX_E_MNTAPI_PERM                            = 24012,
   VIX_E_MNTAPI_DICT                            = 24013,
   VIX_E_MNTAPI_DICT_LOCKED                     = 24014,
   VIX_E_MNTAPI_OPEN_HANDLES                    = 24015,
   VIX_E_MNTAPI_CANT_MAKE_VAR_DIR               = 24016,
   VIX_E_MNTAPI_NO_ROOT                         = 24017,
   VIX_E_MNTAPI_LOOP_FAILED                     = 24018,
   VIX_E_MNTAPI_DAEMON                          = 24019,
   VIX_E_MNTAPI_INTERNAL                        = 24020,
   VIX_E_MNTAPI_SYSTEM                          = 24021,
   VIX_E_MNTAPI_NO_CONNECTION_DETAILS           = 24022,
   /* FuseMnt errors: Do not exceed 24299 */

   /* VixMntapi errors*/
   VIX_E_MNTAPI_INCOMPATIBLE_VERSION            = 24300,
   VIX_E_MNTAPI_OS_ERROR                        = 24301,
   VIX_E_MNTAPI_DRIVE_LETTER_IN_USE             = 24302,
   VIX_E_MNTAPI_DRIVE_LETTER_ALREADY_ASSIGNED   = 24303,
   VIX_E_MNTAPI_VOLUME_NOT_MOUNTED              = 24304,
   VIX_E_MNTAPI_VOLUME_ALREADY_MOUNTED          = 24305,
   VIX_E_MNTAPI_FORMAT_FAILURE                  = 24306,
   VIX_E_MNTAPI_NO_DRIVER                       = 24307,
   VIX_E_MNTAPI_ALREADY_OPENED                  = 24308,
   VIX_E_MNTAPI_ITEM_NOT_FOUND                  = 24309,
   VIX_E_MNTAPI_UNSUPPROTED_BOOT_LOADER         = 24310,
   VIX_E_MNTAPI_UNSUPPROTED_OS                  = 24311,
   VIX_E_MNTAPI_CODECONVERSION                  = 24312,
   VIX_E_MNTAPI_REGWRITE_ERROR                  = 24313,
   VIX_E_MNTAPI_UNSUPPORTED_FT_VOLUME           = 24314,
   VIX_E_MNTAPI_PARTITION_NOT_FOUND             = 24315,
   VIX_E_MNTAPI_PUTFILE_ERROR                   = 24316,
   VIX_E_MNTAPI_GETFILE_ERROR                   = 24317,
   VIX_E_MNTAPI_REG_NOT_OPENED                  = 24318,
   VIX_E_MNTAPI_REGDELKEY_ERROR                 = 24319,
   VIX_E_MNTAPI_CREATE_PARTITIONTABLE_ERROR     = 24320,
   VIX_E_MNTAPI_OPEN_FAILURE                    = 24321,
   VIX_E_MNTAPI_VOLUME_NOT_WRITABLE             = 24322,

   /* Success on operation that completes asynchronously */
   VIX_ASYNC                                    = 25000,

   /* Async errors */
   VIX_E_ASYNC_MIXEDMODE_UNSUPPORTED            = 26000,

   /* Network Errors */
   VIX_E_NET_HTTP_UNSUPPORTED_PROTOCOL     = 30001,
   VIX_E_NET_HTTP_URL_MALFORMAT            = 30003,
   VIX_E_NET_HTTP_COULDNT_RESOLVE_PROXY    = 30005,
   VIX_E_NET_HTTP_COULDNT_RESOLVE_HOST     = 30006,
   VIX_E_NET_HTTP_COULDNT_CONNECT          = 30007,
   VIX_E_NET_HTTP_HTTP_RETURNED_ERROR      = 30022,
   VIX_E_NET_HTTP_OPERATION_TIMEDOUT       = 30028,
   VIX_E_NET_HTTP_SSL_CONNECT_ERROR        = 30035,
   VIX_E_NET_HTTP_TOO_MANY_REDIRECTS       = 30047,
   VIX_E_NET_HTTP_TRANSFER                 = 30200,
   VIX_E_NET_HTTP_SSL_SECURITY             = 30201,
   VIX_E_NET_HTTP_GENERIC                  = 30202,
};

// {{ End VIX_ERROR }}

const char *Vix_GetErrorText(VixError err, const char *locale);


/*
 *-----------------------------------------------------------------------------
 *
 * VIX Handles --
 *
 * These are common functions that apply to handles of several types. 
 *-----------------------------------------------------------------------------
 */

/* 
 * VIX Property Type
 */

typedef int VixPropertyType;
enum {
   VIX_PROPERTYTYPE_ANY             = 0,
   VIX_PROPERTYTYPE_INTEGER         = 1,
   VIX_PROPERTYTYPE_STRING          = 2,
   VIX_PROPERTYTYPE_BOOL            = 3,
   VIX_PROPERTYTYPE_HANDLE          = 4,
   VIX_PROPERTYTYPE_INT64           = 5,
   VIX_PROPERTYTYPE_BLOB            = 6
};

/*
 * VIX Property ID's
 */

typedef int VixPropertyID;
enum {
   VIX_PROPERTY_NONE                                  = 0,

   /* Properties used by several handle types. */
   VIX_PROPERTY_META_DATA_CONTAINER                   = 2,

   /* VIX_HANDLETYPE_HOST properties */
   VIX_PROPERTY_HOST_HOSTTYPE                         = 50,
   VIX_PROPERTY_HOST_API_VERSION                      = 51,
   VIX_PROPERTY_HOST_SOFTWARE_VERSION                 = 52,

   /* VIX_HANDLETYPE_VM properties */
   VIX_PROPERTY_VM_NUM_VCPUS                          = 101,
   VIX_PROPERTY_VM_VMX_PATHNAME                       = 103, 
   VIX_PROPERTY_VM_VMTEAM_PATHNAME                    = 105, 
   VIX_PROPERTY_VM_MEMORY_SIZE                        = 106,
   VIX_PROPERTY_VM_READ_ONLY                          = 107,
   VIX_PROPERTY_VM_NAME                               = 108,
   VIX_PROPERTY_VM_GUESTOS                            = 109,
   VIX_PROPERTY_VM_IN_VMTEAM                          = 128,
   VIX_PROPERTY_VM_POWER_STATE                        = 129,
   VIX_PROPERTY_VM_TOOLS_STATE                        = 152,
   VIX_PROPERTY_VM_IS_RUNNING                         = 196,
   VIX_PROPERTY_VM_SUPPORTED_FEATURES                 = 197,
   /* VIX_PROPERTY_VM_IS_RECORDING                       = 236, Removed in version 1.11 */
   /* VIX_PROPERTY_VM_IS_REPLAYING                       = 237, Removed in version 1.11 */
   VIX_PROPERTY_VM_SSL_ERROR                          = 293,

   /* Result properties; these are returned by various procedures */
   VIX_PROPERTY_JOB_RESULT_ERROR_CODE                 = 3000,
   VIX_PROPERTY_JOB_RESULT_VM_IN_GROUP                = 3001,
   VIX_PROPERTY_JOB_RESULT_USER_MESSAGE               = 3002,
   VIX_PROPERTY_JOB_RESULT_EXIT_CODE                  = 3004,
   VIX_PROPERTY_JOB_RESULT_COMMAND_OUTPUT             = 3005,
   VIX_PROPERTY_JOB_RESULT_HANDLE                     = 3010,
   VIX_PROPERTY_JOB_RESULT_GUEST_OBJECT_EXISTS        = 3011,
   VIX_PROPERTY_JOB_RESULT_GUEST_PROGRAM_ELAPSED_TIME = 3017,
   VIX_PROPERTY_JOB_RESULT_GUEST_PROGRAM_EXIT_CODE    = 3018,
   VIX_PROPERTY_JOB_RESULT_ITEM_NAME                  = 3035,
   VIX_PROPERTY_JOB_RESULT_FOUND_ITEM_DESCRIPTION     = 3036,
   VIX_PROPERTY_JOB_RESULT_SHARED_FOLDER_COUNT        = 3046,
   VIX_PROPERTY_JOB_RESULT_SHARED_FOLDER_HOST         = 3048,
   VIX_PROPERTY_JOB_RESULT_SHARED_FOLDER_FLAGS        = 3049,
   VIX_PROPERTY_JOB_RESULT_PROCESS_ID                 = 3051,
   VIX_PROPERTY_JOB_RESULT_PROCESS_OWNER              = 3052,
   VIX_PROPERTY_JOB_RESULT_PROCESS_COMMAND            = 3053,
   VIX_PROPERTY_JOB_RESULT_FILE_FLAGS                 = 3054,
   VIX_PROPERTY_JOB_RESULT_PROCESS_START_TIME         = 3055,
   VIX_PROPERTY_JOB_RESULT_VM_VARIABLE_STRING         = 3056,
   VIX_PROPERTY_JOB_RESULT_PROCESS_BEING_DEBUGGED     = 3057,
   VIX_PROPERTY_JOB_RESULT_SCREEN_IMAGE_SIZE          = 3058,
   VIX_PROPERTY_JOB_RESULT_SCREEN_IMAGE_DATA          = 3059,
   VIX_PROPERTY_JOB_RESULT_FILE_SIZE                  = 3061,
   VIX_PROPERTY_JOB_RESULT_FILE_MOD_TIME              = 3062,
   VIX_PROPERTY_JOB_RESULT_EXTRA_ERROR_INFO           = 3084,

   /* Event properties; these are sent in the moreEventInfo for some events. */
   VIX_PROPERTY_FOUND_ITEM_LOCATION                   = 4010,

   /* VIX_HANDLETYPE_SNAPSHOT properties */
   VIX_PROPERTY_SNAPSHOT_DISPLAYNAME                  = 4200,   
   VIX_PROPERTY_SNAPSHOT_DESCRIPTION                  = 4201,
   VIX_PROPERTY_SNAPSHOT_POWERSTATE                   = 4205,
   /* VIX_PROPERTY_SNAPSHOT_IS_REPLAYABLE                = 4207, Removed in version 1.11 */

   VIX_PROPERTY_GUEST_SHAREDFOLDERS_SHARES_PATH       = 4525,

   /* Virtual machine encryption properties */
   VIX_PROPERTY_VM_ENCRYPTION_PASSWORD                = 7001,
};

/*
 * These are events that may be signalled by calling a procedure
 * of type VixEventProc.
 */

typedef int VixEventType;
enum {
   VIX_EVENTTYPE_JOB_COMPLETED          = 2,
   VIX_EVENTTYPE_JOB_PROGRESS           = 3,
   VIX_EVENTTYPE_FIND_ITEM              = 8,
   VIX_EVENTTYPE_CALLBACK_SIGNALLED     = 2,  // Deprecated - Use VIX_EVENTTYPE_JOB_COMPLETED instead.
};


/*
 * These are the property flags for each file.
 */

enum {
   VIX_FILE_ATTRIBUTES_DIRECTORY       = 0x0001,
   VIX_FILE_ATTRIBUTES_SYMLINK         = 0x0002,       
};


/*
 * Procedures of this type are called when an event happens on a handle.
 */

typedef void VixEventProc(VixHandle handle,
                          VixEventType eventType,
                          VixHandle moreEventInfo,
                          void *clientData);

/*
 * Handle Property functions
 */

void Vix_ReleaseHandle(VixHandle handle);

void Vix_AddRefHandle(VixHandle handle);

VixHandleType Vix_GetHandleType(VixHandle handle);

VixError Vix_GetProperties(VixHandle handle, 
                           VixPropertyID firstPropertyID, ...);

VixError Vix_GetPropertyType(VixHandle handle, VixPropertyID propertyID, 
                             VixPropertyType *propertyType);

void Vix_FreeBuffer(void *p);


/*
 *-----------------------------------------------------------------------------
 *
 * VIX Host --
 *
 *-----------------------------------------------------------------------------
 */

typedef int VixHostOptions;
enum {
   /*
    * The following option was removed in version 1.11.
      VIX_HOSTOPTION_USE_EVENT_PUMP        = 0x0008,
    */
   VIX_HOSTOPTION_VERIFY_SSL_CERT       = 0x4000,
};

typedef int VixServiceProvider;
enum {
   VIX_SERVICEPROVIDER_DEFAULT                   = 1,
   VIX_SERVICEPROVIDER_VMWARE_SERVER             = 2,
   VIX_SERVICEPROVIDER_VMWARE_WORKSTATION        = 3,
   VIX_SERVICEPROVIDER_VMWARE_PLAYER             = 4,
   VIX_SERVICEPROVIDER_VMWARE_VI_SERVER          = 10,
   VIX_SERVICEPROVIDER_VMWARE_WORKSTATION_SHARED = 11,
};

/*
 * VIX_API_VERSION tells VixHost_Connect to use the latest API version 
 * that is available for the product specified in the VixServiceProvider 
 * parameter.
 */
enum {
   VIX_API_VERSION      = -1
};

VixHandle VixHost_Connect(int apiVersion,
                          VixServiceProvider hostType,
                          const char *hostName,
                          int hostPort,
                          const char *userName,
                          const char *password,
                          VixHostOptions options,
                          VixHandle propertyListHandle,
                          VixEventProc *callbackProc,
                          void *clientData);
 
void VixHost_Disconnect(VixHandle hostHandle);

/*
 * VM Registration
 */

VixHandle VixHost_RegisterVM(VixHandle hostHandle,
                             const char *vmxFilePath,
                             VixEventProc *callbackProc,
                             void *clientData);

VixHandle VixHost_UnregisterVM(VixHandle hostHandle,
                               const char *vmxFilePath,
                               VixEventProc *callbackProc,
                               void *clientData);

/*
 * VM Search
 */

typedef int VixFindItemType;
enum {
   VIX_FIND_RUNNING_VMS         = 1,
   VIX_FIND_REGISTERED_VMS      = 4,
};

VixHandle VixHost_FindItems(VixHandle hostHandle,
                            VixFindItemType searchType,
                            VixHandle searchCriteria,
                            int32 timeout,
                            VixEventProc *callbackProc,
                            void *clientData);


/*
 * VixHost_OpenVM() supercedes VixVM_Open() since it allows for
 * the passing of option flags and extra data in the form of a
 * property list.
 * It is recommended to use VixHost_OpenVM() instead of VixVM_Open().
 */

typedef int VixVMOpenOptions;
enum {
   VIX_VMOPEN_NORMAL  = 0x0,
};

VixHandle VixHost_OpenVM(VixHandle hostHandle,
                         const char *vmxFilePathName,
                         VixVMOpenOptions options,
                         VixHandle propertyListHandle,
                         VixEventProc *callbackProc,
                         void *clientData);


/*
 * Following functions were removed in version 1.11.
 *
   typedef int VixPumpEventsOptions;
   enum {
    VIX_PUMPEVENTOPTION_NONE = 0,
   };
   void Vix_PumpEvents(VixHandle hostHandle, VixPumpEventsOptions options);

   VixHandle VixVM_OpenUrlInGuest(VixHandle vmHandle,
                                  const char *url,
                                  int windowState,
                                  VixHandle propertyListHandle,
                                  VixEventProc *callbackProc,
                                  void *clientData);
 */

/*
 *-----------------------------------------------------------------------------
 *
 * PropertyList --
 *
 *-----------------------------------------------------------------------------
 */

#ifndef VIX_HIDE_FROM_JAVA
VixError VixPropertyList_AllocPropertyList(VixHandle hostHandle,
                                           VixHandle *resultHandle, 
                                           int firstPropertyID,
                                           ...);
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VIX VM --
 *
 * This describes the persistent configuration state of a single VM. The 
 * VM may or may not be running.
 *
 *-----------------------------------------------------------------------------
 */

VixHandle VixVM_Open(VixHandle hostHandle,
                     const char *vmxFilePathName,
                     VixEventProc *callbackProc,
                     void *clientData);

typedef int VixVMPowerOpOptions;
enum {
   VIX_VMPOWEROP_NORMAL                      = 0,
   VIX_VMPOWEROP_FROM_GUEST                  = 0x0004,
   VIX_VMPOWEROP_SUPPRESS_SNAPSHOT_POWERON   = 0x0080,
   VIX_VMPOWEROP_LAUNCH_GUI                  = 0x0200,
   VIX_VMPOWEROP_START_VM_PAUSED             = 0x1000,
};

/*
 * Power operations 
 */

VixHandle VixVM_PowerOn(VixHandle vmHandle,
                        VixVMPowerOpOptions powerOnOptions,
                        VixHandle propertyListHandle,
                        VixEventProc *callbackProc,
                        void *clientData);

VixHandle VixVM_PowerOff(VixHandle vmHandle,
                         VixVMPowerOpOptions powerOffOptions,
                         VixEventProc *callbackProc,
                         void *clientData);

VixHandle VixVM_Reset(VixHandle vmHandle,
                      VixVMPowerOpOptions resetOptions,
                      VixEventProc *callbackProc,
                      void *clientData);

VixHandle VixVM_Suspend(VixHandle vmHandle,
                        VixVMPowerOpOptions suspendOptions,
                        VixEventProc *callbackProc,
                        void *clientData);

VixHandle VixVM_Pause(VixHandle vmHandle,
                      int options,
                      VixHandle propertyList,
                      VixEventProc *callbackProc,
                      void *clientData);

VixHandle VixVM_Unpause(VixHandle vmHandle,
                        int options,
                        VixHandle propertyList,
                        VixEventProc *callbackProc,
                        void *clientData);

typedef int VixVMDeleteOptions;
enum {
   VIX_VMDELETE_DISK_FILES     = 0x0002,
};

VixHandle VixVM_Delete(VixHandle vmHandle,
                       VixVMDeleteOptions deleteOptions,
                       VixEventProc *callbackProc,
                       void *clientData);

/*
 * This is the state of an individual VM.  These values are bitwise flags.
 * The actual value returned for may be a bitwise OR of one more of these
 * flags, along with other reserved values not documented here.
 */

typedef int VixPowerState;
enum {
      VIX_POWERSTATE_POWERING_OFF    = 0x0001,
      VIX_POWERSTATE_POWERED_OFF     = 0x0002,
      VIX_POWERSTATE_POWERING_ON     = 0x0004,
      VIX_POWERSTATE_POWERED_ON      = 0x0008,
      VIX_POWERSTATE_SUSPENDING      = 0x0010,
      VIX_POWERSTATE_SUSPENDED       = 0x0020,
      VIX_POWERSTATE_TOOLS_RUNNING   = 0x0040,
      VIX_POWERSTATE_RESETTING       = 0x0080,
      VIX_POWERSTATE_BLOCKED_ON_MSG  = 0x0100,
      VIX_POWERSTATE_PAUSED          = 0x0200,
      VIX_POWERSTATE_RESUMING        = 0x0800,
};

typedef int VixToolsState;
enum {
      VIX_TOOLSSTATE_UNKNOWN           = 0x0001,
      VIX_TOOLSSTATE_RUNNING           = 0x0002,
      VIX_TOOLSSTATE_NOT_INSTALLED     = 0x0004,
};


/*
 * These flags describe optional functions supported by different
 * types of VM.
 */
enum {
      VIX_VM_SUPPORT_SHARED_FOLDERS       = 0x0001,
      VIX_VM_SUPPORT_MULTIPLE_SNAPSHOTS   = 0x0002,
      VIX_VM_SUPPORT_TOOLS_INSTALL        = 0x0004,
      VIX_VM_SUPPORT_HARDWARE_UPGRADE     = 0x0008,
};

/*
 * VIX_ADMINISTRATOR_USER_NAME and VIX_CONSOLE_USER_NAME are no longer
 * supported. If your code includes references to these constants please
 * update your code to use a valid guest username and password when calling 
 * VixVM_LoginInGuest(). 
 */

//#define VIX_ADMINISTRATOR_USER_NAME    "__VMware_Vix_Guest_User_Admin__"
//#define VIX_CONSOLE_USER_NAME          "__VMware_Vix_Guest_Console_User__"

/*
 * Guest operations
 */

VixHandle VixVM_WaitForToolsInGuest(VixHandle vmHandle,
                                    int timeoutInSeconds,
                                    VixEventProc *callbackProc,
                                    void *clientData);

/*
 * VixVM_LoginInGuest option flags.
 */
enum {
   VIX_LOGIN_IN_GUEST_REQUIRE_INTERACTIVE_ENVIRONMENT      = 0x08,                     
};

VixHandle VixVM_LoginInGuest(VixHandle vmHandle,
                             const char *userName,
                             const char *password,
                             int options,
                             VixEventProc *callbackProc,
                             void *clientData);

VixHandle VixVM_LogoutFromGuest(VixHandle vmHandle,
                                VixEventProc *callbackProc,
                                void *clientData);


/* 
 * Guest Process functions 
 */

typedef int VixRunProgramOptions;
enum {
   VIX_RUNPROGRAM_RETURN_IMMEDIATELY   = 0x0001,
   VIX_RUNPROGRAM_ACTIVATE_WINDOW      = 0x0002,
};

VixHandle VixVM_RunProgramInGuest(VixHandle vmHandle,
                                  const char *guestProgramName,
                                  const char *commandLineArgs,
                                  VixRunProgramOptions options,
                                  VixHandle propertyListHandle,
                                  VixEventProc *callbackProc,
                                  void *clientData);

VixHandle VixVM_ListProcessesInGuest(VixHandle vmHandle,
                                     int options,
                                     VixEventProc *callbackProc,
                                     void *clientData);

VixHandle VixVM_KillProcessInGuest(VixHandle vmHandle,
                                   uint64 pid,
                                   int options,
                                   VixEventProc *callbackProc,
                                   void *clientData);

VixHandle VixVM_RunScriptInGuest(VixHandle vmHandle,
                                 const char *interpreter,
                                 const char *scriptText,
                                 VixRunProgramOptions options,
                                 VixHandle propertyListHandle,
                                 VixEventProc *callbackProc,
                                 void *clientData);

/* 
 * Guest File functions 
 */

VixHandle VixVM_CopyFileFromHostToGuest(VixHandle vmHandle,
                                        const char *hostPathName,
                                        const char *guestPathName,
                                        int options,
                                        VixHandle propertyListHandle,
                                        VixEventProc *callbackProc,
                                        void *clientData);

VixHandle VixVM_CopyFileFromGuestToHost(VixHandle vmHandle,
                                        const char *guestPathName,
                                        const char *hostPathName,
                                        int options,
                                        VixHandle propertyListHandle,
                                        VixEventProc *callbackProc,
                                        void *clientData);

VixHandle VixVM_DeleteFileInGuest(VixHandle vmHandle,
                                  const char *guestPathName,
                                  VixEventProc *callbackProc,
                                  void *clientData);

VixHandle VixVM_FileExistsInGuest(VixHandle vmHandle,
                                  const char *guestPathName,
                                  VixEventProc *callbackProc,
                                  void *clientData);

VixHandle VixVM_RenameFileInGuest(VixHandle vmHandle,
                                  const char *oldName,
                                  const char *newName,
                                  int options,
                                  VixHandle propertyListHandle,
                                  VixEventProc *callbackProc,
                                  void *clientData);

VixHandle VixVM_CreateTempFileInGuest(VixHandle vmHandle,
                                      int options,
                                      VixHandle propertyListHandle,
                                      VixEventProc *callbackProc,
                                      void *clientData);

VixHandle VixVM_GetFileInfoInGuest(VixHandle vmHandle,
                                   const char *pathName,
                                   VixEventProc *callbackProc,
                                   void *clientData);


/* 
 * Guest Directory functions 
 */

VixHandle VixVM_ListDirectoryInGuest(VixHandle vmHandle,
                                     const char *pathName,
                                     int options,
                                     VixEventProc *callbackProc,
                                     void *clientData);

VixHandle VixVM_CreateDirectoryInGuest(VixHandle vmHandle,
                                       const char *pathName,
                                       VixHandle propertyListHandle,
                                       VixEventProc *callbackProc,
                                       void *clientData);

VixHandle VixVM_DeleteDirectoryInGuest(VixHandle vmHandle,
                                       const char *pathName,
                                       int options,
                                       VixEventProc *callbackProc,
                                       void *clientData);

VixHandle VixVM_DirectoryExistsInGuest(VixHandle vmHandle,
                                       const char *pathName,
                                       VixEventProc *callbackProc,
                                       void *clientData);

/*
 * Guest Variable Functions
 */
enum {
   VIX_VM_GUEST_VARIABLE            = 1,
   VIX_VM_CONFIG_RUNTIME_ONLY       = 2,
   VIX_GUEST_ENVIRONMENT_VARIABLE   = 3,
};     

VixHandle VixVM_ReadVariable(VixHandle vmHandle,
                             int variableType,
                             const char *name,
                             int options,
                             VixEventProc *callbackProc,
                             void *clientData);

VixHandle VixVM_WriteVariable(VixHandle vmHandle,
                              int variableType,
                              const char *valueName,
                              const char *value,
                              int options,
                              VixEventProc *callbackProc,
                              void *clientData);


/* 
 * Snapshot functions that operate on a VM
 */

VixError VixVM_GetNumRootSnapshots(VixHandle vmHandle,
                                   int *result);

VixError VixVM_GetRootSnapshot(VixHandle vmHandle,
                               int index,
                               VixHandle *snapshotHandle);

VixError VixVM_GetCurrentSnapshot(VixHandle vmHandle, 
                                  VixHandle *snapshotHandle);

VixError VixVM_GetNamedSnapshot(VixHandle vmHandle, 
                                const char *name,
                                VixHandle *snapshotHandle);

typedef int VixRemoveSnapshotOptions;
enum {
   VIX_SNAPSHOT_REMOVE_CHILDREN    = 0x0001,
};

VixHandle VixVM_RemoveSnapshot(VixHandle vmHandle, 
                               VixHandle snapshotHandle,
                               VixRemoveSnapshotOptions options,
                               VixEventProc *callbackProc,
                               void *clientData);

VixHandle VixVM_RevertToSnapshot(VixHandle vmHandle,
                                 VixHandle snapshotHandle,
                                 VixVMPowerOpOptions options,
                                 VixHandle propertyListHandle,
                                 VixEventProc *callbackProc,
                                 void *clientData);

typedef int VixCreateSnapshotOptions;
enum {
   VIX_SNAPSHOT_INCLUDE_MEMORY     = 0x0002,
};

VixHandle VixVM_CreateSnapshot(VixHandle vmHandle,
                               const char *name,
                               const char *description,
                               VixCreateSnapshotOptions options,
                               VixHandle propertyListHandle,
                               VixEventProc *callbackProc,
                               void *clientData);


/*
 * Shared Folders Functions
 */

/*
 * These are the flags describing each shared folder.
 */
typedef int VixMsgSharedFolderOptions;
enum  {
   VIX_SHAREDFOLDER_WRITE_ACCESS     = 0x04,
};

VixHandle VixVM_EnableSharedFolders(VixHandle vmHandle,
                                    Bool enabled,      
                                    int options,       
                                    VixEventProc *callbackProc,
                                    void *clientData); 

VixHandle VixVM_GetNumSharedFolders(VixHandle vmHandle,
                                    VixEventProc *callbackProc,
                                    void *clientData);

VixHandle VixVM_GetSharedFolderState(VixHandle vmHandle,
                                     int index,
                                     VixEventProc *callbackProc,
                                     void *clientData);

VixHandle VixVM_SetSharedFolderState(VixHandle vmHandle,
                                     const char *shareName,
                                     const char *hostPathName,
                                     VixMsgSharedFolderOptions flags,
                                     VixEventProc *callbackProc,
                                     void *clientData);

VixHandle VixVM_AddSharedFolder(VixHandle vmHandle,
                                const char *shareName,
                                const char *hostPathName,
                                VixMsgSharedFolderOptions flags,
                                VixEventProc *callbackProc,
                                void *clientData);

VixHandle VixVM_RemoveSharedFolder(VixHandle vmHandle,
                                   const char *shareName,
                                   int flags,
                                   VixEventProc *callbackProc,
                                   void *clientData);


/*
 * Screen Capture
 */

#ifndef VIX_HIDE_FROM_JAVA
enum {
   VIX_CAPTURESCREENFORMAT_PNG            = 0x01,
   VIX_CAPTURESCREENFORMAT_PNG_NOCOMPRESS = 0x02,
};

VixHandle VixVM_CaptureScreenImage(VixHandle vmHandle, 
                                   int captureType,
                                   VixHandle additionalProperties,
                                   VixEventProc *callbackProc,
                                   void *clientdata);
#endif   // VIX_HIDE_FROM_JAVA



/*
 * VM Cloning --
 */

typedef int VixCloneType;
enum {
   VIX_CLONETYPE_FULL       = 0,
   VIX_CLONETYPE_LINKED     = 1,
};

VixHandle VixVM_Clone(VixHandle vmHandle,
                      VixHandle snapshotHandle,
                      VixCloneType cloneType,
                      const char *destConfigPathName,
                      int options,
                      VixHandle propertyListHandle,
                      VixEventProc *callbackProc,
                      void *clientData);




/*
 * Misc Functions
 */

VixHandle VixVM_UpgradeVirtualHardware(VixHandle vmHandle,
                                       int options,
                                       VixEventProc *callbackProc,
                                       void *clientData);

enum {
   VIX_INSTALLTOOLS_MOUNT_TOOLS_INSTALLER = 0x00,
   VIX_INSTALLTOOLS_AUTO_UPGRADE          = 0x01,
   VIX_INSTALLTOOLS_RETURN_IMMEDIATELY    = 0x02
};

VixHandle VixVM_InstallTools(VixHandle vmHandle,
                             int options,
                             const char *commandLineArgs,
                             VixEventProc *callbackProc,
                             void *clientData);


/*
 *-----------------------------------------------------------------------------
 *
 * VIX Job --
 *
 *-----------------------------------------------------------------------------
 */

/* 
 * Synchronization functions 
 * (used to detect when an asynch operation completes). 
 */

VixError VixJob_Wait(VixHandle jobHandle, 
                     VixPropertyID firstPropertyID, 
                     ...);

VixError VixJob_CheckCompletion(VixHandle jobHandle, 
                                Bool *complete);


/* 
 * Accessor functions 
 * (used to get results of a completed asynch operation). 
 */

VixError VixJob_GetError(VixHandle jobHandle);

int VixJob_GetNumProperties(VixHandle jobHandle,
                            int resultPropertyID);

VixError VixJob_GetNthProperties(VixHandle jobHandle,
                                 int index,
                                 int propertyID,
                                 ...);



/*
 *-----------------------------------------------------------------------------
 *
 * VIX Snapshot --
 *
 *-----------------------------------------------------------------------------
 */


VixError VixSnapshot_GetNumChildren(VixHandle parentSnapshotHandle, 
                                    int *numChildSnapshots);

VixError VixSnapshot_GetChild(VixHandle parentSnapshotHandle,
                              int index,
                              VixHandle *childSnapshotHandle);

VixError VixSnapshot_GetParent(VixHandle snapshotHandle,
                               VixHandle *parentSnapshotHandle);






#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _VIX_H_ */
