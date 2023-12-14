/*********************************************************
 * Copyright (C) 1998-2021 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/


/*
 * hgfsProto.h --
 *
 * Header file for data types and message formats used in the
 * Host/Guest File System (hgfs) protocol.
 */


#ifndef _HGFS_PROTO_H_
# define _HGFS_PROTO_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "hgfs.h"

/*
 * Handle used by the server to identify files and searches. Used
 * by the driver to match server replies with pending requests.
 */

typedef uint32 HgfsHandle;
#define HGFS_INVALID_HANDLE         ((HgfsHandle)~((HgfsHandle)0))

/*
 * Opcodes for server operations.
 *
 * Changing the ordering of this enum will break the protocol; new ops
 * should be added at the end (but before HGFS_OP_MAX).
 */

typedef enum {
   HGFS_OP_OPEN,               /* Open file */
   HGFS_OP_READ,               /* Read from file */
   HGFS_OP_WRITE,              /* Write to file */
   HGFS_OP_CLOSE,              /* Close file */
   HGFS_OP_SEARCH_OPEN,        /* Start new search */
   HGFS_OP_SEARCH_READ,        /* Get next search response */
   HGFS_OP_SEARCH_CLOSE,       /* End a search */
   HGFS_OP_GETATTR,            /* Get file attributes */
   HGFS_OP_SETATTR,            /* Set file attributes */
   HGFS_OP_CREATE_DIR,         /* Create new directory */
   HGFS_OP_DELETE_FILE,        /* Delete a file */
   HGFS_OP_DELETE_DIR,         /* Delete a directory */
   HGFS_OP_RENAME,             /* Rename a file or directory */
   HGFS_OP_QUERY_VOLUME_INFO,  /* Query volume information */

   /*
    * The following operations are only available in version 2 of the hgfs
    * protocol. The corresponding version 1 opcodes above are deprecated.
    */

   HGFS_OP_OPEN_V2,            /* Open file */
   HGFS_OP_GETATTR_V2,         /* Get file attributes */
   HGFS_OP_SETATTR_V2,         /* Set file attributes */
   HGFS_OP_SEARCH_READ_V2,     /* Get next search response */
   HGFS_OP_CREATE_SYMLINK,     /* Create a symlink */
   HGFS_OP_SERVER_LOCK_CHANGE, /* Change the oplock on a file */
   HGFS_OP_CREATE_DIR_V2,      /* Create a directory */
   HGFS_OP_DELETE_FILE_V2,     /* Delete a file */
   HGFS_OP_DELETE_DIR_V2,      /* Delete a directory */
   HGFS_OP_RENAME_V2,          /* Rename a file or directory */

   /*
    * Operations for version 3, deprecating version 2 operations.
    */

   HGFS_OP_OPEN_V3,               /* Open file */
   HGFS_OP_READ_V3,               /* Read from file */
   HGFS_OP_WRITE_V3,              /* Write to file */
   HGFS_OP_CLOSE_V3,              /* Close file */
   HGFS_OP_SEARCH_OPEN_V3,        /* Start new search */
   HGFS_OP_SEARCH_READ_V3,        /* Read V3 directory entries */
   HGFS_OP_SEARCH_CLOSE_V3,       /* End a search */
   HGFS_OP_GETATTR_V3,            /* Get file attributes */
   HGFS_OP_SETATTR_V3,            /* Set file attributes */
   HGFS_OP_CREATE_DIR_V3,         /* Create new directory */
   HGFS_OP_DELETE_FILE_V3,        /* Delete a file */
   HGFS_OP_DELETE_DIR_V3,         /* Delete a directory */
   HGFS_OP_RENAME_V3,             /* Rename a file or directory */
   HGFS_OP_QUERY_VOLUME_INFO_V3,  /* Query volume information */
   HGFS_OP_CREATE_SYMLINK_V3,     /* Create a symlink */
   HGFS_OP_SERVER_LOCK_CHANGE_V3, /* Change the oplock on a file */
   HGFS_OP_WRITE_WIN32_STREAM_V3, /* Write WIN32_STREAM_ID format data to file */
   /*
    * Operations for version 4, deprecating version 3 operations.
    */

   HGFS_OP_CREATE_SESSION_V4,     /* Create a session and return host capabilities. */
   HGFS_OP_DESTROY_SESSION_V4,    /* Destroy/close session. */
   HGFS_OP_READ_FAST_V4,          /* Read */
   HGFS_OP_WRITE_FAST_V4,         /* Write */
   HGFS_OP_SET_WATCH_V4,          /* Start monitoring directory changes. */
   HGFS_OP_REMOVE_WATCH_V4,       /* Stop monitoring directory changes. */
   HGFS_OP_NOTIFY_V4,             /* Notification for a directory change event. */
   HGFS_OP_SEARCH_READ_V4,        /* Read V4 directory entries. */
   HGFS_OP_OPEN_V4,               /* Open file */
   HGFS_OP_ENUMERATE_STREAMS_V4,  /* Enumerate alternative named streams for a file. */
   HGFS_OP_GETATTR_V4,            /* Get file attributes */
   HGFS_OP_SETATTR_V4,            /* Set file attributes */
   HGFS_OP_DELETE_V4,             /* Delete a file or a directory */
   HGFS_OP_LINKMOVE_V4,           /* Rename/move/create hard link. */
   HGFS_OP_FSCTL_V4,              /* Sending FS control requests. */
   HGFS_OP_ACCESS_CHECK_V4,       /* Access check. */
   HGFS_OP_FSYNC_V4,              /* Flush all cached data to the disk. */
   HGFS_OP_QUERY_VOLUME_INFO_V4,  /* Query volume information. */
   HGFS_OP_OPLOCK_ACQUIRE_V4,     /* Acquire OPLOCK. */
   HGFS_OP_OPLOCK_BREAK_V4,       /* Break or downgrade OPLOCK. */
   HGFS_OP_LOCK_BYTE_RANGE_V4,    /* Acquire byte range lock. */
   HGFS_OP_UNLOCK_BYTE_RANGE_V4,  /* Release byte range lock. */
   HGFS_OP_QUERY_EAS_V4,          /* Query extended attributes. */
   HGFS_OP_SET_EAS_V4,            /* Add or modify extended attributes. */

   HGFS_OP_MAX,                   /* Dummy op, must be last in enum */
   HGFS_OP_NEW_HEADER = 0xff,     /* Header op, must be unique, distinguishes packet headers. */
} HgfsOp;


/*
 * If we get to where the OP table has grown such that we hit the invalid opcode to
 * distinguish between header structures in the packet, then we must ensure that there
 * is no valid HGFS opcode with that same value.
 * The following assert is designed to force anyone who adds new opcodes which cause the
 * above condition to occur to verify the opcode values and then can remove this check.
 */
MY_ASSERTS(hgfsOpValuesAsserts,
   ASSERT_ON_COMPILE(HGFS_OP_MAX < HGFS_OP_NEW_HEADER);
)

/* HGFS protocol versions. */

typedef enum {
   HGFS_PROTOCOL_VERSION_NONE,
   HGFS_PROTOCOL_VERSION_1,
   HGFS_PROTOCOL_VERSION_2,
   HGFS_PROTOCOL_VERSION_3,
   HGFS_PROTOCOL_VERSION_4,
} HgfsProtocolVersion;

/* XXX: Needs change when VMCI is supported. */
#define HGFS_REQ_PAYLOAD_SIZE_V3(hgfsReq) (sizeof *hgfsReq + sizeof(HgfsRequest))
#define HGFS_REP_PAYLOAD_SIZE_V3(hgfsRep) (sizeof *hgfsRep + sizeof(HgfsReply))

/* XXX: Needs change when VMCI is supported. */
#define HGFS_REQ_GET_PAYLOAD_V3(hgfsReq) ((char *)(hgfsReq) + sizeof(HgfsRequest))
#define HGFS_REP_GET_PAYLOAD_V3(hgfsRep) ((char *)(hgfsRep) + sizeof(HgfsReply))

 /*
 * Open flags.
 *
 * Changing the order of this enum will break stuff.  Do not add any flags to
 * this enum: it has been frozen and all new flags should be added to
 * HgfsOpenMode.  This was done because HgfsOpenMode could still be converted
 * to a bitmask (so that it's easier to add flags to) whereas this enum was
 * already too large.
 */

typedef enum {             //  File doesn't exist   File exists
   HGFS_OPEN,              //  error
   HGFS_OPEN_EMPTY,        //  error               size = 0
   HGFS_OPEN_CREATE,       //  create
   HGFS_OPEN_CREATE_SAFE,  //  create              error
   HGFS_OPEN_CREATE_EMPTY, //  create              size = 0
} HgfsOpenFlags;


/*
 * Write flags.
 */

typedef uint8 HgfsWriteFlags;

#define HGFS_WRITE_APPEND 1


/*
 * Permissions bits.
 *
 * These are intentionally similar to Unix permissions bits, and we
 * convert to/from Unix permissions using simple shift operations, so
 * don't change these or you will break things.
 */

typedef uint8 HgfsPermissions;

#define HGFS_PERM_READ  4
#define HGFS_PERM_WRITE 2
#define HGFS_PERM_EXEC  1

/*
 * Access mode bits.
 *
 * Different operating systems have different set of file access mode.
 * Here are constants that are rich enough to describe all access modes in an OS
 * independent way.
 */

typedef uint32 HgfsAccessMode;
/*
 * Generic access rights control coarse grain access for the file.
 * A particular generic rigth can be expanded into different set of specific rights
 * on different OS.
 */

/*
 * HGFS_MODE_GENERIC_READ means ability to read file data and read various file
 * attributes and properties.
 */
#define HGFS_MODE_GENERIC_READ        (1 << 0)
/*
 * HGFS_MODE_GENERIC_WRITE means ability to write file data and updaate various file
 * attributes and properties.
 */
#define HGFS_MODE_GENERIC_WRITE       (1 << 1)
/*
 * HGFS_MODE_GENERIC_EXECUE means ability to execute file. For network redirectors
 * ability to execute usualy implies ability to read data; for local file systems
 * HGFS_MODE_GENERIC_EXECUTE does not imply ability to read data.
 */
#define HGFS_MODE_GENERIC_EXECUTE     (1 << 2)

/* Specific rights define fine grain access modes. */
#define HGFS_MODE_READ_DATA           (1 << 3)  // Ability to read file data
#define HGFS_MODE_WRITE_DATA          (1 << 4)  // Ability to writge file data
#define HGFS_MODE_APPEND_DATA         (1 << 5)  // Appending data to the end of file
#define HGFS_MODE_DELETE              (1 << 6)  // Ability to delete the file
#define HGFS_MODE_TRAVERSE_DIRECTORY  (1 << 7)  // Ability to access files in a directory
#define HGFS_MODE_LIST_DIRECTORY      (1 << 8)  // Ability to list file names
#define HGFS_MODE_ADD_SUBDIRECTORY    (1 << 9)  // Ability to create a new subdirectory
#define HGFS_MODE_ADD_FILE            (1 << 10) // Ability to create a new file
#define HGFS_MODE_DELETE_CHILD        (1 << 11) // Ability to delete file/subdirectory
#define HGFS_MODE_READ_ATTRIBUTES     (1 << 12) // Ability to read attributes
#define HGFS_MODE_WRITE_ATTRIBUTES    (1 << 13) // Ability to write attributes
#define HGFS_MODE_READ_EXTATTRIBUTES  (1 << 14) // Ability to read extended attributes
#define HGFS_MODE_WRITE_EXTATTRIBUTES (1 << 15) // Ability to write extended attributes
#define HGFS_MODE_READ_SECURITY       (1 << 16) // Ability to read permissions/ACLs/owner
#define HGFS_MODE_WRITE_SECURITY      (1 << 17) // Ability to change permissions/ACLs
#define HGFS_MODE_TAKE_OWNERSHIP      (1 << 18) // Ability to change file owner/group

/*
 * Server-side locking (oplocks and leases).
 *
 * The client can ask the server to acquire opportunistic locking/leasing
 * from the host FS on its behalf. This is communicated as part of an open request.
 *
 * HGFS_LOCK_OPPORTUNISTIC means that the client trusts the server
 * to decide what kind of locking to request from the host FS.
 * All other values tell the server explicitly the type of lock to
 * request.
 *
 * The server will attempt to acquire the desired lock and will notify the client
 * which type of lock was acquired as part of the reply to the open request.
 * Note that HGFS_LOCK_OPPORTUNISTIC should not be specified as the type of
 * lock acquired by the server, since HGFS_LOCK_OPPORTUNISTIC is not an
 * actual lock.
 */

typedef enum {
   HGFS_LOCK_NONE,
   HGFS_LOCK_OPPORTUNISTIC,
   HGFS_LOCK_EXCLUSIVE,
   HGFS_LOCK_SHARED,
   HGFS_LOCK_BATCH,
   HGFS_LOCK_LEASE,
} HgfsLockType;


/*
 * Flags to indicate in a setattr request which fields should be
 * updated. Deprecated.
 */

typedef uint8 HgfsAttrChanges;

#define HGFS_ATTR_SIZE                  (1 << 0)
#define HGFS_ATTR_CREATE_TIME           (1 << 1)
#define HGFS_ATTR_ACCESS_TIME           (1 << 2)
#define HGFS_ATTR_WRITE_TIME            (1 << 3)
#define HGFS_ATTR_CHANGE_TIME           (1 << 4)
#define HGFS_ATTR_PERMISSIONS           (1 << 5)
#define HGFS_ATTR_ACCESS_TIME_SET       (1 << 6)
#define HGFS_ATTR_WRITE_TIME_SET        (1 << 7)


/*
 * Hints to indicate in a getattr or setattr which attributes
 * are valid for the request.
 * For setattr only, attributes should be set by host even if
 * no valid values are specified by the guest.
 */

typedef uint64 HgfsAttrHint;

#define HGFS_ATTR_HINT_SET_ACCESS_TIME   (1 << 0)
#define HGFS_ATTR_HINT_SET_WRITE_TIME    (1 << 1)
#define HGFS_ATTR_HINT_USE_FILE_DESC     (1 << 2)

/*
 * Hint to determine using a name or a handle to determine
 * what to delete.
 */

typedef uint64 HgfsDeleteHint;

#define HGFS_DELETE_HINT_USE_FILE_DESC   (1 << 0)

/*
 * Hint to determine using a name or a handle to determine
 * what to renames.
 */

typedef uint64 HgfsRenameHint;

#define HGFS_RENAME_HINT_USE_SRCFILE_DESC       (1 << 0)
#define HGFS_RENAME_HINT_USE_TARGETFILE_DESC    (1 << 1)
#define HGFS_RENAME_HINT_NO_REPLACE_EXISTING    (1 << 2)
#define HGFS_RENAME_HINT_NO_COPY_ALLOWED        (1 << 3)

/*
 * File attributes.
 *
 * The four time fields below are in Windows NT format, which is in
 * units of 100ns since Jan 1, 1601, UTC.
 */

/*
 * Version 1 attributes. Deprecated.
 * Version 2 should be using HgfsAttrV2.
 */

#pragma pack(push, 1)
typedef struct HgfsAttr {
   HgfsFileType type;            /* File type */
   uint64 size;                  /* File size (in bytes) */
   uint64 creationTime;          /* Creation time. Ignored by POSIX */
   uint64 accessTime;            /* Time of last access */
   uint64 writeTime;             /* Time of last write */
   uint64 attrChangeTime;        /* Time file attributess were last
                                  * changed. Ignored by Windows */
   HgfsPermissions permissions;  /* Permissions bits */
} HgfsAttr;
#pragma pack(pop)


/* Various flags and Windows attributes. */

typedef uint64 HgfsAttrFlags;

#define HGFS_ATTR_HIDDEN      (1 << 0)
#define HGFS_ATTR_SYSTEM      (1 << 1)
#define HGFS_ATTR_ARCHIVE     (1 << 2)
#define HGFS_ATTR_HIDDEN_FORCED (1 << 3)
#define HGFS_ATTR_REPARSE_POINT (1 << 4)

/* V4 additional definitions for hgfsAttrFlags. */
#define HGFS_ATTR_COMPRESSED            (1 << 5)
#define HGFS_ATTR_ENCRYPTED             (1 << 6)
#define HGFS_ATTR_OFFLINE               (1 << 7)
#define HGFS_ATTR_READONLY              (1 << 8)
#define HGFS_ATTR_SPARSE                (1 << 9)
#define HGFS_ATTR_TEMPORARY             (1 << 10)

#define HGFS_ATTR_SEQUENTIAL_ONLY       (1 << 11)

/*
 * Specifies which open request fields contain
 * valid values.
 */

typedef uint64 HgfsOpenValid;

#define HGFS_OPEN_VALID_NONE              0
#define HGFS_OPEN_VALID_MODE              (1 << 0)
#define HGFS_OPEN_VALID_FLAGS             (1 << 1)
#define HGFS_OPEN_VALID_SPECIAL_PERMS     (1 << 2)
#define HGFS_OPEN_VALID_OWNER_PERMS       (1 << 3)
#define HGFS_OPEN_VALID_GROUP_PERMS       (1 << 4)
#define HGFS_OPEN_VALID_OTHER_PERMS       (1 << 5)
#define HGFS_OPEN_VALID_FILE_ATTR         (1 << 6)
#define HGFS_OPEN_VALID_ALLOCATION_SIZE   (1 << 7)
#define HGFS_OPEN_VALID_DESIRED_ACCESS    (1 << 8)
#define HGFS_OPEN_VALID_SHARE_ACCESS      (1 << 9)
#define HGFS_OPEN_VALID_SERVER_LOCK       (1 << 10)
#define HGFS_OPEN_VALID_FILE_NAME         (1 << 11)

/* V4 additional open mask flags. */
#define HGFS_OPEN_VALID_EA                      (1 << 12)
#define HGFS_OPEN_VALID_ACL                     (1 << 13)
#define HGFS_OPEN_VALID_STREAM_NAME             (1 << 14)

/*
 * Specifies which attribute fields contain
 * valid values.
 */

typedef uint64 HgfsAttrValid;

#define HGFS_ATTR_VALID_NONE              0
#define HGFS_ATTR_VALID_TYPE              (1 << 0)
#define HGFS_ATTR_VALID_SIZE              (1 << 1)
#define HGFS_ATTR_VALID_CREATE_TIME       (1 << 2)
#define HGFS_ATTR_VALID_ACCESS_TIME       (1 << 3)
#define HGFS_ATTR_VALID_WRITE_TIME        (1 << 4)
#define HGFS_ATTR_VALID_CHANGE_TIME       (1 << 5)
#define HGFS_ATTR_VALID_SPECIAL_PERMS     (1 << 6)
#define HGFS_ATTR_VALID_OWNER_PERMS       (1 << 7)
#define HGFS_ATTR_VALID_GROUP_PERMS       (1 << 8)
#define HGFS_ATTR_VALID_OTHER_PERMS       (1 << 9)
#define HGFS_ATTR_VALID_FLAGS             (1 << 10)
#define HGFS_ATTR_VALID_ALLOCATION_SIZE   (1 << 11)
#define HGFS_ATTR_VALID_USERID            (1 << 12)
#define HGFS_ATTR_VALID_GROUPID           (1 << 13)
#define HGFS_ATTR_VALID_FILEID            (1 << 14)
#define HGFS_ATTR_VALID_VOLID             (1 << 15)
/*
 * Add our file and volume identifiers.
 * NOTE: On Windows hosts, the file identifier is not guaranteed to be valid
 *       particularly with FAT. A defrag operation could cause it to change.
 *       Therefore, to not confuse older clients, and non-Windows
 *       clients we have added a separate flag.
 *       The Windows client will check for both flags for the
 *       file ID, and return the information to the guest application.
 *       However, it will use the ID internally, when it has an open
 *       handle on the server.
 *       Non-Windows clients need the file ID to be always guaranteed,
 *       which is to say, that the ID remains constant over the course of the
 *       file's lifetime, and will use the HGFS_ATTR_VALID_FILEID flag
 *       only to determine if the ID is valid.
 */
#define HGFS_ATTR_VALID_NON_STATIC_FILEID (1 << 16)
/*
 * File permissions that are in effect for the user which runs HGFS server.
 * Client needs to know effective permissions in order to implement access(2).
 * Client can't derive it from group/owner/other permissions because of two resaons:
 * 1. It does not know user/group id of the user which runs HGFS server
 * 2. Effective permissions account for additional restrictions that may be imposed
 *    by host file system, for example by ACL.
 */
#define HGFS_ATTR_VALID_EFFECTIVE_PERMS   (1 << 17)
#define HGFS_ATTR_VALID_EXTEND_ATTR_SIZE  (1 << 18)
#define HGFS_ATTR_VALID_REPARSE_POINT     (1 << 19)
#define HGFS_ATTR_VALID_SHORT_NAME        (1 << 20)


/*
 * Specifies which create dir request fields contain
 * valid values.
 */

typedef uint64 HgfsCreateDirValid;

#define HGFS_CREATE_DIR_VALID_NONE              0
#define HGFS_CREATE_DIR_VALID_SPECIAL_PERMS     (1 << 0)
#define HGFS_CREATE_DIR_VALID_OWNER_PERMS       (1 << 1)
#define HGFS_CREATE_DIR_VALID_GROUP_PERMS       (1 << 2)
#define HGFS_CREATE_DIR_VALID_OTHER_PERMS       (1 << 3)
#define HGFS_CREATE_DIR_VALID_FILE_NAME         (1 << 4)
#define HGFS_CREATE_DIR_VALID_FILE_ATTR         (1 << 5)

/*
 *  Version 2 of HgfsAttr
 */

#pragma pack(push, 1)
typedef struct HgfsAttrV2 {
   HgfsAttrValid mask;           /* A bit mask to determine valid attribute fields */
   HgfsFileType type;            /* File type */
   uint64 size;                  /* File size (in bytes) */
   uint64 creationTime;          /* Creation time. Ignored by POSIX */
   uint64 accessTime;            /* Time of last access */
   uint64 writeTime;             /* Time of last write */
   uint64 attrChangeTime;        /* Time file attributes were last
                                  * changed. Ignored by Windows */
   HgfsPermissions specialPerms; /* Special permissions bits (suid, etc.).
                                  * Ignored by Windows */
   HgfsPermissions ownerPerms;   /* Owner permissions bits */
   HgfsPermissions groupPerms;   /* Group permissions bits. Ignored by
                                  * Windows */
   HgfsPermissions otherPerms;   /* Other permissions bits. Ignored by
                                  * Windows */
   HgfsAttrFlags flags;          /* Various flags and Windows 'attributes' */
   uint64 allocationSize;        /* Actual size of file on disk */
   uint32 userId;                /* User identifier, ignored by Windows */
   uint32 groupId;               /* group identifier, ignored by Windows */
   uint64 hostFileId;            /* File Id of the file on host: inode_t on Linux */
   uint32 volumeId;              /* volume identifier, non-zero is valid. */
   uint32 effectivePerms;        /* Permissions in effect for the user on the host. */
   uint64 reserved2;             /* Reserved for future use */
} HgfsAttrV2;
#pragma pack(pop)


/*
 * Cross-platform filename representation
 *
 * Cross-platform (CP) names are represented by a string with each
 * path component separated by NULs, and terminated with a final NUL,
 * but with no leading path separator.
 *
 * For example, the representations of a POSIX and Windows name
 * are as follows, with "0" meaning NUL.
 *
 * Original name             Cross-platform name
 * -----------------------------------------------------
 * "/home/bac/temp"    ->    "home0bac0temp0"
 * "C:\temp\file.txt"  ->    "C0temp0file.txt0"
 *
 * Note that as in the example above, Windows should strip the colon
 * off of drive letters as part of the conversion. Aside from that,
 * all characters in each path component should be left unescaped and
 * unmodified. Each OS is responsible for escaping any characters that
 * are not legal in its filenames when converting FROM the CP name
 * format, and unescaping them when converting TO the CP name format.
 *
 * In some requests (OPEN, GETATTR, SETATTR, DELETE, CREATE_DIR) the
 * CP name is used to represent a particular file, but it is also used
 * to represent a search pattern for looking up files using
 * SEARCH_OPEN.
 *
 * In the current HGFS server implementation, each request has a minimum packet
 * size that must be met for it to be considered valid. This minimum is simply
 * the sizeof the particular request, which includes the solitary byte from the
 * HgfsFileName struct. For these particular requests, clients add an extra
 * byte to their payload size, without that byte being present anywhere.
 *
 * It isn't clear that this behavior is correct, but the end result is that
 * neither end malfunctions, as an extra byte gets sent by the client and is
 * ignored by the server. Unfortunately, it cannot be easily fixed. The
 * server's minimum packet size can be changed, but the client should continue
 * to send an extra byte, otherwise older servers with a slightly longer
 * minimum packet size may consider the new client's packets to be too short.
 *
 * UTF-8 representation
 * --------------------
 * XXX: It is expected that file names in the HGFS protocol will be a valid UTF-8
 * encoding.
 * See RFC 3629 (http://tools.ietf.org/html/rfc3629)
 *
 * Unicode Format
 * --------------
 * HGFS protocol requests that contain file names as in the structure below,
 * should contain unicode normal form C (precomposed see explanation below)
 * characters therefore hosts such as Mac OS which
 * use HFS+ and unicode form D should convert names before
 * processing or sending HGFS requests.
 *
 * Precomposed (normal form C) versus Decomposed (normal form D)
 * -------------------------------------------------------------
 * Certain Unicode characters can be encoded in more than one way.
 * For example, an (A acute) can be encoded either precomposed,
 * as U+00C1 (LATIN CAPITAL LETTER A WITH ACUTE), or decomposed,
 * as U+0041 U+0301 (LATIN CAPITAL LETTER A followed by a COMBINING ACUTE ACCENT).
 * Precomposed characters are more common in the Windows world,
 * whereas decomposed characters are more common on the Mac.
 *
 * See UAX 15 (http://unicode.org/reports/tr15/)
 */

#pragma pack(push, 1)
typedef struct HgfsFileName {
   uint32 length; /* Does NOT include terminating NUL */
   char name[1];
} HgfsFileName;
#pragma pack(pop)


/*
 * Windows hosts only: the server may return the DOS 8 dot 3 format
 * name as part of the directory entry.
 */
#pragma pack(push, 1)
typedef struct HgfsShortFileName {
   uint32 length;            /* Does NOT include terminating NUL */
   char name[12 * 4];        /* UTF8 max char size is 4 bytes. */
} HgfsShortFileName;
#pragma pack(pop)

/*
 * Case-sensitiviy flags are only used when any lookup is
 * involved on the server side.
 */

typedef enum {
   HGFS_FILE_NAME_DEFAULT_CASE,
   HGFS_FILE_NAME_CASE_SENSITIVE,
   HGFS_FILE_NAME_CASE_INSENSITIVE,
} HgfsCaseType;


/*
 * HgfsFileNameV3 - new header to incorporate case-sensitivity flags along with
 * Hgfs file handle.
 */

#pragma pack(push, 1)
typedef struct HgfsFileNameV3 {
   uint32 length;           /* Does NOT include terminating NUL */
   uint32 flags;            /* Flags described below. */
   HgfsCaseType caseType;   /* Case-sensitivity type. */
   HgfsHandle fid;
   char name[1];
} HgfsFileNameV3;
#pragma pack(pop)


/*
 * HgfsFileNameV3 flags. Case-sensitiviy flags are only used when any lookup is
 * involved on the server side.
 */
#define HGFS_FILE_NAME_USE_FILE_DESC     (1 << 0)  /* Case type ignored if set. */


/*
 * Request/reply structs. These are the first members of all
 * operation request and reply messages, respectively.
 */

#pragma pack(push, 1)
typedef struct HgfsRequest {
   HgfsHandle id;        /* Opaque request ID used by the requestor */
   HgfsOp op;
} HgfsRequest;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReply {
   HgfsHandle id;        /* Opaque request ID used by the requestor */
   HgfsStatus status;
} HgfsReply;
#pragma pack(pop)


/*
 * Messages for our file operations.
 */

/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestOpen {
   HgfsRequest header;
   HgfsOpenMode mode;            /* Which type of access is requested */
   HgfsOpenFlags flags;          /* Which flags to open the file with */
   HgfsPermissions permissions;  /* Which permissions to *create* a new file with */
   HgfsFileName fileName;
} HgfsRequestOpen;
#pragma pack(pop)


/* Version 2 of HgfsRequestOpen */

#pragma pack(push, 1)
typedef struct HgfsRequestOpenV2 {
   HgfsRequest header;
   HgfsOpenValid mask;           /* Bitmask that specified which fields are valid. */
   HgfsOpenMode mode;            /* Which type of access requested. See desiredAccess */
   HgfsOpenFlags flags;          /* Which flags to open the file with */
   HgfsPermissions specialPerms; /* Desired 'special' permissions for file creation */
   HgfsPermissions ownerPerms;   /* Desired 'owner' permissions for file creation */
   HgfsPermissions groupPerms;   /* Desired 'group' permissions for file creation */
   HgfsPermissions otherPerms;   /* Desired 'other' permissions for file creation */
   HgfsAttrFlags attr;           /* Attributes, if any, for file creation */
   uint64 allocationSize;        /* How much space to pre-allocate during creation */
   uint32 desiredAccess;         /* Extended support for windows access modes */
   uint32 shareAccess;           /* Windows only, share access modes */
   HgfsLockType desiredLock;     /* The type of lock desired by the client */
   uint64 reserved1;             /* Reserved for future use */
   uint64 reserved2;             /* Reserved for future use */
   HgfsFileName fileName;
} HgfsRequestOpenV2;
#pragma pack(pop)


/* Version 3 of HgfsRequestOpen */

#pragma pack(push, 1)
typedef struct HgfsRequestOpenV3 {
   HgfsOpenValid mask;           /* Bitmask that specified which fields are valid. */
   HgfsOpenMode mode;            /* Which type of access requested. See desiredAccess */
   HgfsOpenFlags flags;          /* Which flags to open the file with */
   HgfsPermissions specialPerms; /* Desired 'special' permissions for file creation */
   HgfsPermissions ownerPerms;   /* Desired 'owner' permissions for file creation */
   HgfsPermissions groupPerms;   /* Desired 'group' permissions for file creation */
   HgfsPermissions otherPerms;   /* Desired 'other' permissions for file creation */
   HgfsAttrFlags attr;           /* Attributes, if any, for file creation */
   uint64 allocationSize;        /* How much space to pre-allocate during creation */
   uint32 desiredAccess;         /* Extended support for windows access modes */
   uint32 shareAccess;           /* Windows only, share access modes */
   HgfsLockType desiredLock;     /* The type of lock desired by the client */
   uint64 reserved1;             /* Reserved for future use */
   uint64 reserved2;             /* Reserved for future use */
   HgfsFileNameV3 fileName;
} HgfsRequestOpenV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplyOpen {
   HgfsReply header;
   HgfsHandle file;      /* Opaque file ID used by the server */
} HgfsReplyOpen;
#pragma pack(pop)


/* Version 2 of HgfsReplyOpen */

#pragma pack(push, 1)
typedef struct HgfsReplyOpenV2 {
   HgfsReply header;
   HgfsHandle file;                  /* Opaque file ID used by the server */
   HgfsLockType acquiredLock;        /* The type of lock acquired by the server */
} HgfsReplyOpenV2;
#pragma pack(pop)


/* Version 3 of HgfsReplyOpen */


/*
 * The HGFS open V3 can acquire locks and reserve disk space when requested.
 * However, current versions of the server don't implement the locking or allocation of
 * disk space on a create. These results flags indicate to the client if the server
 * implements handling those fields and so the clients can respond accordingly.
 */
typedef uint32 HgfsReplyOpenFlags;

#define HGFS_OPEN_REPLY_ALLOC_DISK_SPACE      (1 << 0)
#define HGFS_OPEN_REPLY_LOCKED_FILE           (1 << 1)

#pragma pack(push, 1)
typedef struct HgfsReplyOpenV3 {
   HgfsHandle file;                  /* Opaque file ID used by the server */
   HgfsLockType acquiredLock;        /* The type of lock acquired by the server */
   HgfsReplyOpenFlags flags;         /* Opened file flags */
   uint32 reserved;                  /* Reserved for future use */
} HgfsReplyOpenV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestRead {
   HgfsRequest header;
   HgfsHandle file;      /* Opaque file ID used by the server */
   uint64 offset;
   uint32 requiredSize;
} HgfsRequestRead;
#pragma pack(pop)

/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplyRead {
   HgfsReply header;
   uint32 actualSize;
   char payload[1];
} HgfsReplyRead;
#pragma pack(pop)


/*
 * Version 3 of HgfsRequestRead.
 * Server must support HGFS_LARGE_PACKET_MAX to implement this op.
 */

#pragma pack(push, 1)
typedef struct HgfsRequestReadV3 {
   HgfsHandle file;      /* Opaque file ID used by the server */
   uint64 offset;
   uint32 requiredSize;
   uint64 reserved;      /* Reserved for future use */
} HgfsRequestReadV3;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyReadV3 {
   uint32 actualSize;
   uint64 reserved;      /* Reserved for future use */
   char payload[1];
} HgfsReplyReadV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestWrite {
   HgfsRequest header;
   HgfsHandle file;      /* Opaque file ID used by the server */
   HgfsWriteFlags flags;
   uint64 offset;
   uint32 requiredSize;
   char payload[1];
} HgfsRequestWrite;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplyWrite {
   HgfsReply header;
   uint32 actualSize;
} HgfsReplyWrite;
#pragma pack(pop)

/*
 * Version 3 of HgfsRequestWrite.
 * Server must support HGFS_LARGE_PACKET_MAX to implement this op.
 */

#pragma pack(push, 1)
typedef struct HgfsRequestWriteV3 {
   HgfsHandle file;      /* Opaque file ID used by the server */
   HgfsWriteFlags flags;
   uint64 offset;
   uint32 requiredSize;
   uint64 reserved;      /* Reserved for future use */
   char payload[1];
} HgfsRequestWriteV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyWriteV3 {
   uint32 actualSize;
   uint64 reserved;      /* Reserved for future use */
} HgfsReplyWriteV3;
#pragma pack(pop)

/* Stream write flags */
typedef enum {
   HGFS_WIN32_STREAM_IGNORE_SECURITY = (1<<0),
} HgfsWin32StreamFlags;

/*
 * HgfsRequestWriteWin32Stream.
 * Server must support HGFS_LARGE_PACKET_MAX to implement this op.
 */

#pragma pack(push, 1)
typedef struct HgfsRequestWriteWin32StreamV3 {
   HgfsHandle file;      /* Opaque file ID used by the server */
   HgfsWin32StreamFlags flags;
   uint32 reserved1;
   uint32 requiredSize;
   uint64 reserved2;     /* Reserved for future use */
   char payload[1];
} HgfsRequestWriteWin32StreamV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyWriteWin32StreamV3 {
   uint32 actualSize;
   uint64 reserved;      /* Reserved for future use */
} HgfsReplyWriteWin32StreamV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestClose {
   HgfsRequest header;
   HgfsHandle file;      /* Opaque file ID used by the server */
} HgfsRequestClose;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplyClose {
   HgfsReply header;
} HgfsReplyClose;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestCloseV3 {
   HgfsHandle file;      /* Opaque file ID used by the server */
   uint64 reserved;      /* Reserved for future use */
} HgfsRequestCloseV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyCloseV3 {
   uint64 reserved;
} HgfsReplyCloseV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestSearchOpen {
   HgfsRequest header;
   HgfsFileName dirName;
} HgfsRequestSearchOpen;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestSearchOpenV3 {
   uint64 reserved;      /* Reserved for future use */
   HgfsFileNameV3 dirName;
} HgfsRequestSearchOpenV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplySearchOpen {
   HgfsReply header;
   HgfsHandle search;    /* Opaque search ID used by the server */
} HgfsReplySearchOpen;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplySearchOpenV3 {
   HgfsHandle search;    /* Opaque search ID used by the server */
   uint64 reserved;      /* Reserved for future use */
} HgfsReplySearchOpenV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestSearchRead {
   HgfsRequest header;
   HgfsHandle search;    /* Opaque search ID used by the server */
   uint32 offset;        /* The first result is offset 0 */
} HgfsRequestSearchRead;
#pragma pack(pop)


/* Version 2 of HgfsRequestSearchRead */

#pragma pack(push, 1)
typedef struct HgfsRequestSearchReadV2 {
   HgfsRequest header;
   HgfsHandle search;    /* Opaque search ID used by the server */
   uint32 offset;        /* The first result is offset 0 */
} HgfsRequestSearchReadV2;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsRequestSearchReadV3 {
   HgfsHandle search;    /* Opaque search ID used by the server */
   uint32 offset;        /* The first result is offset 0 */
   uint32 flags;         /* Reserved for reading multiple directory entries. */
   uint64 reserved;      /* Reserved for future use */
} HgfsRequestSearchReadV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplySearchRead {
   HgfsReply header;
   HgfsAttr attr;
   HgfsFileName fileName;
   /* fileName.length = 0 means "no entry at this offset" */
} HgfsReplySearchRead;
#pragma pack(pop)


/* Version 2 of HgfsReplySearchRead */

#pragma pack(push, 1)
typedef struct HgfsReplySearchReadV2 {
   HgfsReply header;
   HgfsAttrV2 attr;

   /*
    * fileName.length = 0 means "no entry at this offset"
    * If the file is a symlink (as specified in attr)
    * this name is the name of the symlink, not the target.
    */
   HgfsFileName fileName;
} HgfsReplySearchReadV2;
#pragma pack(pop)


/* Directory entry structure. */

typedef struct HgfsDirEntry {
   uint32 nextEntry;
   HgfsAttrV2 attr;

   /*
    * fileName.length = 0 means "no entry at this offset"
    * If the file is a symlink (as specified in attr)
    * this name is the name of the symlink, not the target.
    */
   HgfsFileNameV3 fileName;
} HgfsDirEntry;

#pragma pack(push, 1)
typedef struct HgfsReplySearchReadV3 {
   uint64 count;         /* Number of directory entries. */
   uint64 reserved;      /* Reserved for future use. */
   char payload[1];      /* Directory entries. */
} HgfsReplySearchReadV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestSearchClose {
   HgfsRequest header;
   HgfsHandle search;    /* Opaque search ID used by the server */
} HgfsRequestSearchClose;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplySearchClose {
   HgfsReply header;
} HgfsReplySearchClose;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestSearchCloseV3 {
   HgfsHandle search;    /* Opaque search ID used by the server */
   uint64 reserved;      /* Reserved for future use */
} HgfsRequestSearchCloseV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplySearchCloseV3 {
   uint64 reserved;      /* Reserved for future use */
} HgfsReplySearchCloseV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestGetattr {
   HgfsRequest header;
   HgfsFileName fileName;
} HgfsRequestGetattr;
#pragma pack(pop)


/* Version 2 of HgfsRequestGetattr */

#pragma pack(push, 1)
typedef struct HgfsRequestGetattrV2 {
   HgfsRequest header;
   HgfsAttrHint hints;     /* Flags for file handle valid. */
   HgfsHandle file;        /* Opaque file ID used by the server. */
   HgfsFileName fileName;  /* Filename used when file handle invalid. */
} HgfsRequestGetattrV2;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestGetattrV3 {
   HgfsAttrHint hints;       /* Flags for file handle valid. */
   uint64 reserved;          /* Reserved for future use */
   HgfsFileNameV3 fileName;  /* Filename used when file handle invalid. */
} HgfsRequestGetattrV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplyGetattr {
   HgfsReply header;
   HgfsAttr attr;
} HgfsReplyGetattr;
#pragma pack(pop)


/* Version 2 of HgfsReplyGetattr */

#pragma pack(push, 1)
typedef struct HgfsReplyGetattrV2 {
   HgfsReply header;
   HgfsAttrV2 attr;

   /*
    * If the file is a symlink, as specified in attr.type, then this is
    * the target for the symlink. If the file is not a symlink, this should
    * be ignored.
    *
    * This filename is in "CPNameLite" format. See CPNameLite.c for details.
    */
   HgfsFileName symlinkTarget;
} HgfsReplyGetattrV2;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyGetattrV3 {
   HgfsAttrV2 attr;

   /*
    * If the file is a symlink, as specified in attr.type, then this is
    * the target for the symlink. If the file is not a symlink, this should
    * be ignored.
    *
    * This filename is in "CPNameLite" format. See CPNameLite.c for details.
    */
   uint64 reserved;          /* Reserved for future use */
   HgfsFileNameV3 symlinkTarget;
} HgfsReplyGetattrV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestSetattr {
   HgfsRequest header;
   HgfsAttrChanges update;  /* Which fields need to be updated */
   HgfsAttr attr;
   HgfsFileName fileName;
} HgfsRequestSetattr;
#pragma pack(pop)


/* Version 2 of HgfsRequestSetattr */

#pragma pack(push, 1)
typedef struct HgfsRequestSetattrV2 {
   HgfsRequest header;
   HgfsAttrHint hints;
   HgfsAttrV2 attr;
   HgfsHandle file;        /* Opaque file ID used by the server. */
   HgfsFileName fileName;  /* Filename used when file handle invalid. */
} HgfsRequestSetattrV2;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestSetattrV3 {
   HgfsAttrHint hints;
   HgfsAttrV2 attr;
   uint64 reserved;          /* Reserved for future use */
   HgfsFileNameV3 fileName;  /* Filename used when file handle invalid. */
} HgfsRequestSetattrV3;
#pragma pack(pop)

/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplySetattr {
   HgfsReply header;
} HgfsReplySetattr;
#pragma pack(pop)


/* Version 2 of HgfsReplySetattr */

#pragma pack(push, 1)
typedef struct HgfsReplySetattrV2 {
   HgfsReply header;
} HgfsReplySetattrV2;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplySetattrV3 {
   uint64 reserved;          /* Reserved for future use */
} HgfsReplySetattrV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestCreateDir {
   HgfsRequest header;
   HgfsPermissions permissions;
   HgfsFileName fileName;
} HgfsRequestCreateDir;
#pragma pack(pop)


/* Version 2 of HgfsRequestCreateDir */

#pragma pack(push, 1)
typedef struct HgfsRequestCreateDirV2 {
   HgfsRequest header;
   HgfsCreateDirValid mask;
   HgfsPermissions specialPerms;
   HgfsPermissions ownerPerms;
   HgfsPermissions groupPerms;
   HgfsPermissions otherPerms;
   HgfsFileName fileName;
} HgfsRequestCreateDirV2;
#pragma pack(pop)


/* Version 3 of HgfsRequestCreateDir */

#pragma pack(push, 1)
typedef struct HgfsRequestCreateDirV3 {
   HgfsCreateDirValid mask;
   HgfsPermissions specialPerms;
   HgfsPermissions ownerPerms;
   HgfsPermissions groupPerms;
   HgfsPermissions otherPerms;
   HgfsAttrFlags fileAttr;
   HgfsFileNameV3 fileName;
} HgfsRequestCreateDirV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplyCreateDir {
   HgfsReply header;
} HgfsReplyCreateDir;
#pragma pack(pop)


/* Version 2 of HgfsReplyCreateDir */

#pragma pack(push, 1)
typedef struct HgfsReplyCreateDirV2 {
   HgfsReply header;
} HgfsReplyCreateDirV2;
#pragma pack(pop)


/* Version 3 of HgfsReplyCreateDir */

#pragma pack(push, 1)
typedef struct HgfsReplyCreateDirV3 {
   uint64 reserved;              /* Reserved for future use */
} HgfsReplyCreateDirV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsRequestDelete {
   HgfsRequest header;
   HgfsFileName fileName;
} HgfsRequestDelete;
#pragma pack(pop)


/* Version 2 of HgfsRequestDelete */

#pragma pack(push, 1)
typedef struct HgfsRequestDeleteV2 {
   HgfsRequest header;
   HgfsDeleteHint hints;
   HgfsHandle file;        /* Opaque file ID used by the server. */
   HgfsFileName fileName;  /* Name used if the file is HGFS_HANDLE_INVALID */
} HgfsRequestDeleteV2;
#pragma pack(pop)


/* Version 3 of HgfsRequestDelete */

#pragma pack(push, 1)
typedef struct HgfsRequestDeleteV3 {
   HgfsDeleteHint hints;
   uint64 reserved;              /* Reserved for future use */
   HgfsFileNameV3 fileName;      /* Name used if the file is HGFS_HANDLE_INVALID */
} HgfsRequestDeleteV3;
#pragma pack(pop)


/* Deprecated */

#pragma pack(push, 1)
typedef struct HgfsReplyDelete {
   HgfsReply header;
} HgfsReplyDelete;
#pragma pack(pop)

/* Version 2 of HgfsReplyDelete */

#pragma pack(push, 1)
typedef struct HgfsReplyDeleteV2 {
   HgfsReply header;
} HgfsReplyDeleteV2;
#pragma pack(pop)


/* Version 2 of HgfsReplyDelete */

#pragma pack(push, 1)
typedef struct HgfsReplyDeleteV3 {
   uint64 reserved;              /* Reserved for future use */
} HgfsReplyDeleteV3;
#pragma pack(pop)


/*
 * The size of the HgfsFileName struct is variable depending on the
 * length of the name, so you can't use request->newName to get the
 * actual address of the new name, because where it starts is
 * dependant on how long the oldName is. To get the address of
 * newName, use this:
 *
 *          &oldName + sizeof(HgfsFileName) + oldName.length
 */

#pragma pack(push, 1)
typedef struct HgfsRequestRename {
   HgfsRequest header;
   HgfsFileName oldName;
   HgfsFileName newName;
} HgfsRequestRename;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyRename {
   HgfsReply header;
} HgfsReplyRename;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestRenameV2 {
   HgfsRequest header;
   HgfsRenameHint hints;
   HgfsHandle srcFile;           /* Opaque file ID to "old name" used by the server. */
   HgfsHandle targetFile;        /* Opaque file ID to "old name" used by the server. */
   HgfsFileName oldName;
   HgfsFileName newName;
} HgfsRequestRenameV2;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyRenameV2 {
   HgfsReply header;
} HgfsReplyRenameV2;
#pragma pack(pop)


/* HgfsRequestRename and HgfsReplyRename for v3. */

#pragma pack(push, 1)
typedef struct HgfsRequestRenameV3 {
   HgfsRenameHint hints;
   uint64 reserved;              /* Reserved for future use */
   HgfsFileNameV3 oldName;
   HgfsFileNameV3 newName;
} HgfsRequestRenameV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyRenameV3 {
   uint64 reserved;              /* Reserved for future use */
} HgfsReplyRenameV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestQueryVolume {
   HgfsRequest header;
   HgfsFileName fileName;
} HgfsRequestQueryVolume;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyQueryVolume {
   HgfsReply header;
   uint64 freeBytes;
   uint64 totalBytes;
} HgfsReplyQueryVolume;
#pragma pack(pop)


/* HgfsRequestQueryVolume and HgfsReplyQueryVolume for v3. */

#pragma pack(push, 1)
typedef struct HgfsRequestQueryVolumeV3 {
   uint64 reserved;              /* Reserved for future use */
   HgfsFileNameV3 fileName;
} HgfsRequestQueryVolumeV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyQueryVolumeV3 {
   uint64 freeBytes;
   uint64 totalBytes;
   uint64 reserved;              /* Reserved for future use */
} HgfsReplyQueryVolumeV3;
#pragma pack(pop)



/* New operations for Version 2 */

#pragma pack(push, 1)
typedef struct HgfsRequestServerLockChange {
   HgfsRequest header;
   HgfsHandle file;
   HgfsLockType newServerLock;
} HgfsRequestServerLockChange;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplyServerLockChange {
   HgfsReply header;
   HgfsLockType serverLock;
} HgfsReplyServerLockChange;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsRequestSymlinkCreate {
   HgfsRequest header;
   HgfsFileName symlinkName;

   /* This filename is in "CPNameLite" format. See CPNameLite.c for details. */
   HgfsFileName targetName;
} HgfsRequestSymlinkCreate;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplySymlinkCreate {
   HgfsReply header;
} HgfsReplySymlinkCreate;
#pragma pack(pop)


/* HgfsRequestSymlinkCreate and HgfsReplySymlinkCreate for v3. */

#pragma pack(push, 1)
typedef struct HgfsRequestSymlinkCreateV3 {
   uint64 reserved;              /* Reserved for future use */
   HgfsFileNameV3 symlinkName;

   /* This filename is in "CPNameLite" format. See CPNameLite.c for details. */
   HgfsFileNameV3 targetName;
} HgfsRequestSymlinkCreateV3;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsReplySymlinkCreateV3 {
   uint64 reserved;              /* Reserved for future use */
} HgfsReplySymlinkCreateV3;
#pragma pack(pop)

/* HGFS protocol version 4 definitions. */
#define HGFS_HEADER_VERSION_1                 1
#define HGFS_HEADER_VERSION                   HGFS_HEADER_VERSION_1

/*
 * Flags to indicate the type of packet following the header and
 * the overall state of the operation.
 */

#define HGFS_PACKET_FLAG_REQUEST              (1 << 0)       // Request packet
#define HGFS_PACKET_FLAG_REPLY                (1 << 1)       // Reply packet
#define HGFS_PACKET_FLAG_INFO_EXTERROR        (1 << 2)       // Info has ext error
#define HGFS_PACKET_FLAG_VALID_FLAGS          (0x7)          // Mask for valid values

#pragma pack(push, 1)
typedef struct HgfsHeader {
   uint8 version;       /* Header version. */
   uint8 reserved1[3];  /* Reserved for future use. */
   HgfsOp dummy;        /* Needed to distinguish between older and newer header. */
   uint32 packetSize;   /* Size of the packet, including the header size. */
   uint32 headerSize;   /* Size of the Hgfs header. */
   uint32 requestId;    /* Request ID. */
   HgfsOp op;           /* Operation. */
   uint32 status;       /* Return value. */
   uint32 flags;        /* Flags. See above. */
   uint32 information;  /* Generic field, used e.g. for native error code. */
   uint64 sessionId;    /* Session ID. */
   uint64 reserved;     /* Reserved for future use. */
} HgfsHeader;
#pragma pack(pop)

typedef uint32 HgfsOpCapFlags;

/*
 * The operation capability flags.
 *
 * These flags apply to all operations and occupy the least significant
 * 16 bits of the HgfsOpCapFlags type.
 */

/*
 * HGFS_OP_CAPFLAG_NOT_SUPPORTED
 * If no flags are set then the capability is not supported by the host.
 */
#define HGFS_OP_CAPFLAG_NOT_SUPPORTED           0

/*
 * HGFS_OP_CAPFLAG_IS_SUPPORTED
 * Set for each request that is supported by a host or client.
 * To be set for an Hgfs session both host and client must have the capability.
 */
#define HGFS_OP_CAPFLAG_IS_SUPPORTED            (1 << 0)

/*
 * HGFS_OP_CAPFLAG_ASYNCHRONOUS
 * Set for each request that can be handled asynchronously by a host or client.
 * By default all operations are handled synchronously but if this flag is set
 * by a client and a host then the operation can be handled in an asynchronous manner too.
 */
#define HGFS_OP_CAPFLAG_ASYNCHRONOUS            (1 << 1)

/*
 * The operation specific capability flags.
 *
 * These flags apply only to the operation given by the name and occupy the
 * most significant 16 bits of the HgfsOpCapFlags type.
 */

/*
 * Following flags define which optional parameters for file open
 * requests are supported by the host.
 * HGFS_OP_CAPFLAG_OPENV4_EA - host is capable of setting EA when creating
 *                             a new file.
 * HGFS_OP_CAPFLAG_OPENV4_ACL - host is capable of setting ACLs when creating
 *                              a new file.
 * HGFS_OP_CAPFLAG_OPENV4_NAMED_STREAMS - opening/enumerating named streams
 *                                        is supported.
 * HGFS_OP_CAPFLAG_OPENV4_SHARED_ACCESS - host supports file sharing restrictions.
 * HGFS_OP_CAPFLAG_OPENV4_UNIX_PERMISSIONS - host stores POSIX permissions with
 *                                           file.
 * HGFS_OP_CAPFLAG_OPENV4_POSIX_DELETION - host supports POSIX file deletion semantics.
 */
#define HGFS_OP_CAPFLAG_OPENV4_EA                 (1 << 16)
#define HGFS_OP_CAPFLAG_OPENV4_ACL                (1 << 17)
#define HGFS_OP_CAPFLAG_OPENV4_NAMED_STREAMS      (1 << 18)
#define HGFS_OP_CAPFLAG_OPENV4_SHARED_ACCESS      (1 << 19)
#define HGFS_OP_CAPFLAG_OPENV4_UNIX_PERMISSIONS   (1 << 20)
#define HGFS_OP_CAPFLAG_OPENV4_POSIX_DELETION     (1 << 21)

/*
 *  There is a significant difference in byte range locking semantics between Windows
 *  and POSIX file systems. Windows implements mandatory locking which means that every
 *  read or write request that conflicts with byte range locks is rejected. POSIX has
 *  an advisory locking which means that locks are validated only when another lock is
 *  requested and are not enforced for read/write operations.
 *  Applications in guest OS may expect byte range locking semantics that matches guest
 *  OS which may be different from semantics that is natively supported by host OS. In
 *  this case either HGFS server or HGFS client should provide compensation for the host
 *  OS semantics to maintain application compatibility.
 *  Client must know if the server is capable to provide appropriate byte range locking
 *  semantics to perform some compensation on behalf of server when necessary.
 *
 *  Following flags define various capabilities of byte range lock implementation on
 *  the host.
 *
 *  HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_64 means that server is capable of locking 64 bit
 *                                      length ranges.
 *  HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_32 means that server is limited to 32-bit ranges.
 *  HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_MANDATORY means that server is capable of enforcing
 *                                             read/write restrictions for locked ranges.
 *  HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_ADVISORY means that server supports advisory locking;
 *                                            locks are validated only for other bytes
 *                                            range locking and are not enforced
 *                                            for read/write operations.
 */
#define HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_64               (1 << 16)
#define HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_32               (1 << 17)
#define HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_MANDATORY        (1 << 18)
#define HGFS_OP_CAPFLAG_BYTE_RANGE_LOCKS_ADVISORY         (1 << 19)

/* HGFS_SUPPORTS_HARD_LINKS is set when the host supports hard links. */
#define HGFS_OP_CAPFLAG_LINKMOVE_HARD_LINKS               (1 << 16)

 /*
  * HGFS_SET_WATCH_SUPPORTS_FINE_GRAIN_EVENTS is set when host supports
  * fine grain event reporting for directory notification.
  */
#define HGFS_OP_CAPFLAG_SET_WATCH_FINE_GRAIN_EVENTS       (1 << 16)


#pragma pack(push, 1)
typedef struct HgfsOpCapability {
   HgfsOp op;                         /* Op. */
   HgfsOpCapFlags flags;              /* Flags. */
} HgfsOpCapability;
#pragma pack(pop)

typedef HgfsFileName HgfsUserName;
typedef HgfsFileName HgfsGroupName;

/* Following structures describe user identity on the host which runs HGFS service. */

#pragma pack(push, 1)
typedef struct HgfsIdentity {
   uint32 uid;                        /* user id. */
   uint32 gid;                        /* Primary group id. */
   HgfsUserName user;                 /* User name in form specified in RFC 3530. */
   HgfsGroupName group;               /* Group name in form specified in RFC 3530. */
} HgfsIdentity;
#pragma pack(pop)

#define HGFS_INVALID_SESSION_ID     (~((uint64)0))

/*
 * The HGFS session flags. These determine the state and validity of the session
 * information.
 * It is envisaged that flags will be set for notifying the clients of file system
 * feature support that transcend multiple request types i.e., HGFS opcodes.
 */
typedef uint32 HgfsSessionFlags;

#define HGFS_SESSION_MAXPACKETSIZE_VALID    (1 << 0)
#define HGFS_SESSION_CHANGENOTIFY_ENABLED   (1 << 1)
#define HGFS_SESSION_OPLOCK_ENABLED         (1 << 2)
#define HGFS_SESSION_ASYNC_IO_ENABLED       (1 << 3)

#pragma pack(push, 1)
typedef struct HgfsRequestCreateSessionV4 {
   uint32 numCapabilities;            /* Number of capabilities to follow. */
   uint32 maxPacketSize;              /* Maximum packet size supported. */
   HgfsSessionFlags flags;            /* Session capability flags. */
   uint32 reserved;                   /* Reserved for future use. */
   HgfsOpCapability capabilities[1];    /* Array of HgfsCapabilities. */
} HgfsRequestCreateSessionV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyCreateSessionV4 {
   uint64 sessionId;                  /* Session ID. */
   uint32 numCapabilities;            /* Number of capabilities to follow. */
   uint32 maxPacketSize;              /* Maximum packet size supported. */
   uint32 identityOffset;             /* Offset to HgfsIdentity or 0 if no identity. */
   HgfsSessionFlags flags;            /* Flags. */
   uint32 reserved;                   /* Reserved for future use. */
   HgfsOpCapability capabilities[1];    /* Array of HgfsCapabilities. */
} HgfsReplyCreateSessionV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsRequestDestroySessionV4 {
   uint64 reserved;    /* Reserved for future use. */
} HgfsRequestDestroySessionV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyDestroySessionV4 {
   uint64 reserved;    /* Reserved for future use. */
} HgfsReplyDestroySessionV4;
#pragma pack(pop)

/* Adds new error status: HGFS_STATUS_INVALID_SESSION. */

/*
 * If file handle is used to set watch (HGFS_FILE_NAME_USE_FILE_DESC
 * is set in the fileName), closing this handle implicitly removes the watch.
 */
#pragma pack(push, 1)
typedef struct HgfsRequestSetWatchV4 {
    uint64 events;             /* What events to watch? */
    uint32 flags;              /* Flags. */
    uint64 reserved;           /* Reserved for future use. */
    HgfsFileNameV3 fileName;   /* Filename to watch. */
} HgfsRequestSetWatchV4;
#pragma pack(pop)

/*
 *  Coarse grain notification event types.
 */
#define HGFS_ACTION_ADDED        (1 << 0)   /* File was added. */
#define HGFS_ACTION_REMOVED      (1 << 1)   /* File was removed. */
#define HGFS_ACTION_MODIFIED     (1 << 2)   /* File attributes were changed. */
#define HGFS_ACTION_RENAMED      (1 << 3)   /* File was renamed. */

/*
 * Fine grain notification event types.
 * HgfsRequestSetWatch events.
 */
#define HGFS_NOTIFY_ACCESS                   (1 << 0)    /* File accessed (read) */
#define HGFS_NOTIFY_ATTRIB                   (1 << 1)    /* File attributes changed. */
#define HGFS_NOTIFY_SIZE                     (1 << 2)    /* File size changed. */
#define HGFS_NOTIFY_ATIME                    (1 << 3)    /* Access time changed. */
#define HGFS_NOTIFY_MTIME                    (1 << 4)    /* Modification time changed. */
#define HGFS_NOTIFY_CTIME                    (1 << 5)    /* Attribute time changed. */
#define HGFS_NOTIFY_CRTIME                   (1 << 6)    /* Creation time changed. */
#define HGFS_NOTIFY_NAME                     (1 << 7)    /* File / Directory name. */
#define HGFS_NOTIFY_OPEN                     (1 << 8)    /* File opened */
#define HGFS_NOTIFY_CLOSE_WRITE              (1 << 9)    /* Modified file closed. */
#define HGFS_NOTIFY_CLOSE_NOWRITE            (1 << 10)   /* Non-modified file closed. */
#define HGFS_NOTIFY_CREATE_FILE              (1 << 11)   /* File created */
#define HGFS_NOTIFY_CREATE_DIR               (1 << 12)   /* Directory created */
#define HGFS_NOTIFY_DELETE_FILE              (1 << 13)   /* File deleted */
#define HGFS_NOTIFY_DELETE_DIR               (1 << 14)   /* Directory deleted */
#define HGFS_NOTIFY_DELETE_SELF              (1 << 15)   /* Watched directory deleted */
#define HGFS_NOTIFY_MODIFY                   (1 << 16)   /* File modified. */
#define HGFS_NOTIFY_MOVE_SELF                (1 << 17)   /* Watched directory moved. */
#define HGFS_NOTIFY_OLD_FILE_NAME            (1 << 18)   /* Rename: old file name. */
#define HGFS_NOTIFY_NEW_FILE_NAME            (1 << 19)   /* Rename: new file name. */
#define HGFS_NOTIFY_OLD_DIR_NAME             (1 << 20)   /* Rename: old dir name. */
#define HGFS_NOTIFY_NEW_DIR_NAME             (1 << 21)   /* Rename: new dir name. */
#define HGFS_NOTIFY_CHANGE_EA                (1 << 22)   /* Extended attributes. */
#define HGFS_NOTIFY_CHANGE_SECURITY          (1 << 23)   /* Security/permissions. */
#define HGFS_NOTIFY_ADD_STREAM               (1 << 24)   /* Named stream created. */
#define HGFS_NOTIFY_DELETE_STREAM            (1 << 25)   /* Named stream deleted. */
#define HGFS_NOTIFY_CHANGE_STREAM_SIZE       (1 << 26)   /* Named stream size changed. */
#define HGFS_NOTIFY_CHANGE_STREAM_LAST_WRITE (1 << 27)   /* Stream timestamp changed. */
#define HGFS_NOTIFY_WATCH_DELETED            (1 << 28)   /* Dir with watch deleted. */
#define HGFS_NOTIFY_EVENTS_DROPPED           (1 << 29)   /* Notifications dropped. */

/* HgfsRequestSetWatch flags. */
#define HGFS_NOTIFY_FLAG_WATCH_TREE  (1 << 0)    /* Watch the entire directory tree. */
#define HGFS_NOTIFY_FLAG_DONT_FOLLOW (1 << 1)    /* Don't follow symlinks. */
#define HGFS_NOTIFY_FLAG_ONE_SHOT    (1 << 2)    /* Generate only one notification. */
#define HGFS_NOTIFY_FLAG_POSIX_HINT  (1 << 3)    /* Client is POSIX and thus expects
                                                  * fine grain notification. Server
                                                  * may provide coarse grain
                                                  * notification even if this flag is
                                                  * set.
                                                  */

typedef uint64 HgfsSubscriberHandle;
#define HGFS_INVALID_SUBSCRIBER_HANDLE         ((HgfsSubscriberHandle)~((HgfsSubscriberHandle)0))

#pragma pack(push, 1)
typedef struct HgfsReplySetWatchV4 {
    HgfsSubscriberHandle watchId; /* Watch identifier for subsequent references. */
    uint64 reserved;              /* Reserved for future use. */
} HgfsReplySetWatchV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsRequestRemoveWatchV4 {
    HgfsSubscriberHandle watchId;  /* Watch identifier to remove. */
} HgfsRequestRemoveWatchV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyRemoveWatchV4 {
    uint64 reserved;       /* Reserved for future use. */
} HgfsReplyRemoveWatchV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsNotifyEventV4 {
   uint32 nextOffset;        /* Offset of next event; 0 if it i sthe last one. */
   uint64 mask;              /* Event occurred. */
   uint64 reserved;          /* Reserved for future use. */
   HgfsFileName fileName;    /* Filename. */
} HgfsNotifyEventV4;
#pragma pack(pop)

/* Too many events, some or all event were dropped by the server. */
#define HGFS_NOTIFY_FLAG_OVERFLOW          (1 << 0)
/* Watch had been removed either explicitly or implicitly. */
#define HGFS_NOTIFY_FLAG_REMOVED           (1 << 1)
/* Server generated coasrse grain events. */
#define HGFS_NOTIFY_FLAG_COARSE_GRAIN      (1 << 2)

#pragma pack(push, 1)
typedef struct HgfsRequestNotifyV4 {
   HgfsSubscriberHandle watchId; /* Watch identifier. */
   uint32 flags;                 /* Various flags. */
   uint32 count;                 /* Number of events occured. */
   uint64 reserved;              /* Reserved for future use. */
   HgfsNotifyEventV4 events[1];  /* Events. HgfsNotifyEvent(s). */
} HgfsRequestNotifyV4;
#pragma pack(pop)

// Query EA flags values.
#define HGFS_QUERY_EA_INDEX_SPECIFIED (1 << 0)
#define HGFS_QUERY_EA_SINGLE_ENTRY    (1 << 1)
#define HGFS_QUERY_EA_RESTART_SCAN    (1 << 2)

#pragma pack(push, 1)
typedef struct HgfsRequestQueryEAV4 {
   uint32 flags;                 /* EA flags. */
   uint32 index;
   uint64 reserved;              /* Reserved for future use. */
   uint32 eaNameLength;          /* EA name length. */
   uint32 eaNameOffset;          /* Offset of the eaName field. */
   HgfsFileNameV3 fileName;      /* File to watch. */
   char eaNames[1];              /* List of NULL terminated EA names.
                                  * Actual location of the data depends on
                                  * fileName length and defined by eaNameOffset.
                                  */
} HgfsRequestQueryEAV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyQueryEAV4 {
   uint32 nextOffset;            /* Offset of the next structure when more then
                                  * one record is returned.
                                  */
   uint32 flags;                 /* EA flags. */
   uint32 index;                 /* Index needed to resume scan. */
   uint64 reserved;              /* Reserved for future use. */
   uint32 eaDataLength;          /* EA value length. */
   char eaData[1];               /* NULL termianed EA name followed by EA value. */
} HgfsReplyQueryEAV4;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct HgfsEAV4 {
   uint32 nextOffset;      /* Offset of the next structure in the chain. */
   uint32 valueLength;     /* EA value length. */
   char data[1];           /* NULL terminated EA name followed by EA value. */
} HgfsEAV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsRequestSetEAV4 {
   uint32   flags;           /* Flags, see below. */
   uint64   reserved;        /* Reserved for future use. */
   uint32   numEAs;          /* Number of EAs in this request. */
   HgfsEAV4 attributes[1];   /* Array of attributes. */
} HgfsRequestSetEAV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplySetEAV4 {
   uint64 reserved;        /* Reserved for future use. */
} HgfsReplySetEAV4;
#pragma pack(pop)

/*
 * EA Flags. When both flags are set EA is either created or replaced if it exists.
 * HGFS_EA_FLAG_CREATE - create if EA is not present, error otherwise.
 * HGFS_EA_FLAG_REPLACE - Replace exisitng EA. Error if EA not already present.
 */
#define HGFS_EA_FLAG_CREATE      (1 << 0)
#define HGFS_EA_FLAG_REPLACE     (1 << 1)

/*
 * Byte range lock flag values:
 * HGFS_RANGE_LOCK_EXCLUSIVE - Requested lock is exclusive when this flag is set,
 *                             otherwise it is a shared lock.
 * HGFS_RANGE_LOCK_FAIL_IMMEDIATLY - If the flag is not set server waits until the
 *                                   lock becomes available.
 */
#define HGFS_RANGE_LOCK_EXCLUSIVE               (1 << 0)
#define HGFS_RANGE_LOCK_FAIL_IMMEDIATLY         (1 << 1)

#pragma pack(push, 1)
typedef struct HgfsRequestLockRangeV4 {
   HgfsHandle     fid;          /* File to take lock on. */
   uint32 flags;                /* Various flags. */
   uint64 start;                /* Starting offset in the file. */
   uint64 length;               /* Number of bytes to lock. */
   uint64 reserved;             /* Reserved for future use. */
} HgfsRequestLockRangeV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyLockRangeV4 {
   uint64 reserved;             /* Reserved for future use. */
} HgfsReplyLockRangeV4;
#pragma pack(pop)

#define HGFS_RANGE_LOCK_UNLOCK_ALL               (1 << 0)

#pragma pack(push, 1)
typedef struct HgfsRequestUnlockRangeV4 {
   HgfsHandle     fid;          /* File to take lock on. */
   uint32 flags;                /* Various flags. */
   uint64 start;                /* Starting offset in the file. */
   uint64 length;               /* Number of bytes to lock. */
   uint64 reserved;             /* Reserved for future use. */
} HgfsRequestUnlockRangeV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyUnlockRangeV4 {
   uint64 reserved;             /* Reserved for future use. */
} HgfsReplyUnlockRangeV4;
#pragma pack(pop)

/*
 * There are three types of oplocks: level 1, batch, and level 2. Both the level 1 and
 * batch oplocks are "exclusive access" opens. They are used slightly differently,
 * however, and hence have somewhat different semantics. A level 2 oplock is a "shared
 * access" grant on the file.
 * Level 1 is used by a remote client that wishes to modify the data. Once granted a
 * Level 1 oplock, the remote client may cache the data, modify the data in its cache
 * and need not write it back to the server immediately.
 * Batch oplocks are used by remote clients for accessing script files where the file is
 * opened, read or written, and then closed repeatedly. Thus, a batch oplock
 * corresponds not to a particular application opening the file, but rather to a remote
 * clients network file system caching the file because it knows something about the
 * semantics of the given file access. The name "batch" comes from the fact that this
 * behavior was observed by Microsoft with "batch files" being processed by command line
 * utilities. Log files especially exhibit this behavior when a script it being
 * processed each command is executed in turn. If the output of the script is redirected
 * to a log file the file fits the pattern described earlier, namely open/write/close.
 * With many lines in a file this pattern can be repeated hundreds of times.
 * Level 2 is used by a remote client that merely wishes to read the data. Once granted
 * a Level 2 oplock, the remote client may cache the data and need not worry that the
 * data on the remote file server will change without it being advised of that change.
 * An oplock must be broken whenever the cache consistency guarantee provided by the
 * oplock can no longer be provided. Thus, whenever a second network client attempts to
 * access data in the same file across the network, the file server is responsible for
 * "breaking" the oplocks and only then allowing the remote client to access the file.
 * This ensures that the data is guaranteed to be consistent and hence we have preserved
 * the consistency guarantees essential to proper operation.
 *
 * HGFS_OPLOCK_NONE: no oplock. No caching on client side.
 * HGFS_OPLOCK_SHARED: shared (or LEVEL II) oplock. Read caching is allowed.
 * HGFS_OPLOCK_EXCLUSIVE: exclusive (or LEVEL I) oplock. Read/write caching is allowed.
 * HGFS_OPLOCK_BATCH: batch oplock. Read/Write and Open caching is allowed.
 */

#pragma pack(push, 1)
typedef struct HgfsRequestServerLockChangeV2 {
   HgfsHandle fid;                    /* File to take lock on. */
   HgfsLockType serverLock;           /* Lock type. */
   uint64 reserved;
} HgfsRequestServerLockChangeV2;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyServerLockChangeV2 {
   HgfsLockType serverLock;            /* Lock granted. */
   uint64 reserved;
} HgfsReplyServerLockChangeV2;
#pragma pack(pop)

/*
 * This request is sent from server to the client to notify that oplock
 * is revoked or downgraded.
 */

#pragma pack(push, 1)
typedef struct HgfsRequestOplockBreakV4 {
   HgfsHandle fid;                    /* File handle. */
   HgfsLockType serverLock;           /* Lock downgraded to this type. */
   uint64 reserved;                   /* Reserved for future use. */
} HgfsRequestOplockBreakV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyOplockBreakV4 {
   HgfsHandle fid;                    /* File handle. */
   HgfsLockType serverLock;           /* Lock type. */
   uint64 reserved;                   /* Reserved for future use. */
} HgfsReplyOplockBreakV4;
#pragma pack(pop)

/*
 *  Flusing of a whole volume is not supported.
 *  Flusing of reqular files is supported on all hosts.
 *  Flusing of directories is supproted on POSIX hosts and is
 *  NOOP on Windows hosts.
 */
#pragma pack(push, 1)
typedef struct HgfsRequestFsyncV4 {
   HgfsHandle fid;      /* File to sync. */
   uint64 reserved;
} HgfsRequestFsyncV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyFsyncV4 {
   uint64 reserved;
} HgfsReplyFsyncV4;
#pragma pack(pop)

/*
 * This request is name based only.
 * Server fails this request if HGFS_FILE_E_USE_FILE_DESC is set in the fileName.
 */
#pragma pack(push, 1)
typedef struct HgfsRequestAccessCheckV4 {
   HgfsFileNameV3 fileName;     /* File concerned. */
   HgfsPermissions perms;       /* Permissions to check for. */
   uint64 reserved;             /* Reserved for future use. */
} HgfsRequestAccessCheckV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyAccessCheckV4 {
   uint64 reserved;             /* Reserved for future use. */
} HgfsReplyAccessCheckV4;
#pragma pack(pop)

/*
 * Additional HgfsPersmissions type: checks file existense without
 * requesting any particular access.
 * Matches F_OK mode parameter for POSIX access (2) API.
 */
#define HGFS_PERM_EXISTS  8

/*
 * HGFS_PLATFORM_ALL is a HGFS specific platform independent FSCTL
 * that correspond to different OS specific codes.
 * Other types of FSCTL are platform specific to allow better user
 * experience when guest and host OS are the same. HGFS does not interpret
 * platform specific FSCTL in any way, it just passes it through to the
 * host. If the host run appropriate OS it executes FSCTL on user's behalf,
 * otherwise it fails the request.
 */
typedef enum HgfsPlatformType {
   HGFS_PLATFORM_ALL,
   HGFS_PLATFORM_WINDOWS,
   HGFS_PLATFORM_LINUX,
   HGFS_PLATFORM_MAC
}HgfsPlatformType;

#define HGFS_FSCTL_SET_SPARSE 1 /* Platform independent FSCTL to make file sparse. */

/* Platform together with the code define exact meaning of the operation. */
#pragma pack(push, 1)
typedef struct HgfsRequestFsctlV4 {
   HgfsHandle fid;
   uint32 code;
   HgfsPlatformType platform;
   uint32 dataLength;
   char data[1];
} HgfsRequestFsctlV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyFsctlV4 {
   uint32 dataLength;
   char data[1];
} HgfsReplyFsctlV4;
#pragma pack(pop)

/*
 * Creating a new file or reading file attributes involves ACL. There is a good
 * definition of multi-platform ACLs in RFC 3530, section 5.11. HGFS should use
 * ACLs defined in this document (http://tools.ietf.org/html/rfc3530#section-5.11).
 * ACL support is not mandatory. If a request to create file with ACL comes to a host
 * that does not support ACL, the request should succeed and setting ACL is ignored.
 * Such behavior is consistent with other file systems.
 */
typedef uint64 HgfsOpenCreateOptions;

/* O_SYMLINK in Mac OS or FILE_FLAG_OPEN_REPARSE_POINT in Windows. */
#define HGFS_OPENCREATE_OPTION_SYMLINK            (1 << 0)
/* O_SHLOCK in Mac OS or obtain shared range lock for the whole file. */
#define HGFS_OPENCREATE_OPTION_SHLOCK             (1 << 1)
/* O_EXLOCK in Mac OS or obtain exclusive range lock for the whole file. */
#define HGFS_OPENCREATE_OPTION_EXLOCK             (1 << 2)
/* O_SYNC in Linux, ignored in Mac, FILE_FLAG_WRITE_THROUGH in Windows. */
#define HGFS_OPENCREATE_OPTION_WRITETHROUGH       (1 << 3)
/* FILE_FLAG_NO_BUFFERING in Windows, O_SYNC in Linux, ignored on Mac OS. */
#define HGFS_OPENCREATE_OPTION_NO_BUFERING        (1 << 4)
/*
 * O_NOFOLLOW in POSIX. Windows server checks for reparse point
 * and fails the request if file has one.
 */
#define HGFS_OPENCREATE_OPTION_NO_FOLLOW          (1 << 5)
/* FILE_FLAG_NO_RECALL in Windows. Ignored by POSIX host. */
#define HGFS_OPENCREATE_OPTION_NO_RECALL          (1 << 6)
/* FILE_FLAG_RANDOM_ACCESS in Windows. Ignored by POSIX host. */
#define HGFS_OPENCREATE_OPTION_RANDOM             (1 << 7)
/* FILE_FLAG_SEQUENTIAL_SCAN in Windows. Ignored by POSIX host. */
#define HGFS_OPENCREATE_OPTION_SEQUENTIAL         (1 << 8)
/* FILE_FLAG_BACKUP_SEMANTICS in Windows. Ignored by POSIX host. */
#define HGFS_OPENCREATE_OPTION_BACKUP_SEMANTICS   (1 << 9)
/* Fail opening if the file already exists and it is not a directory. */
#define HGFS_OPENCREATE_OPTION_DIRECTORY          (1 << 10)
/* Fail opening if the file already exists and it is a directory. */
#define HGFS_OPENCREATE_OPTION_NON_DIRECTORY      (1 << 11)

#pragma pack(push, 1)
typedef struct HgfsRequestOpenV4 {
   HgfsOpenValid mask;           /* Bitmask that specified which fields are valid. */
   HgfsOpenMode mode;            /* Which type of access requested. See desiredAccess */
   HgfsOpenFlags flags;          /* Which flags to open the file with */
   HgfsPermissions specialPerms; /* Desired 'special' permissions for file creation */
   HgfsPermissions ownerPerms;   /* Desired 'owner' permissions for file creation */
   HgfsPermissions groupPerms;   /* Desired 'group' permissions for file creation */
   HgfsPermissions otherPerms;   /* Desired 'other' permissions for file creation */
   HgfsAttrFlags attr;           /* Attributes, if any, for file creation */
   uint64 allocationSize;        /* How much space to pre-allocate during creation */
   uint32 desiredAccess;         /* Extended support for windows access modes */
   uint32 shareAccess;           /* Windows only, share access modes */
   HgfsOpenCreateOptions createOptions; /* Various options. */
   HgfsLockType requestedLock;   /* The type of lock desired by the client */
   HgfsFileNameV3 fileName;      /* fid can be used only for relative open,
                                  * i.e. to open named stream.
                                  */
   HgfsFileName streamName;      /* Name of the alternative named stream.
                                  * All flags are the same as defined in fileName.
                                  * The name is used in conjuction with fileName
                                  * field, for example if Windows opens file
                                  * "abc.txt:stream" then fileName contains
                                  * "abc.txt" and streamName contains "stream"
                                  */
   /*
    * EA to set if the file is created or overwritten. The parameter should be ignored
    * if the file already exists.
    * It is needed to correctly implement Windows semantics for opening files.
    * It should work atomically - failure to add EA should result in failure to create
    * the new file.
    * If the host file system does not support EA server should fail the request rather
    * then succeeding and silently dropping EA.
    */
   HgfsRequestSetEAV4 extendedAttributes;
   uint32 aclLength;               /* Length of the acl field. */
   char acl[1];                    /* Multi-platform ACL as defined in RFC 3530. */
} HgfsRequestOpenV4;
#pragma pack(pop)

typedef enum HgfsOpenResult {
   HGFS_FILE_OPENED,
   HGFS_FILE_CREATED,
   HGFS_FILE_OVERWRITTEN,
   HGFS_FILE_SUPERSIDED,
 } HgfsOpenResult;

/*
 * Win32 API has a special value for the desired access - MAXIMUM_ALLOWED.
 * Such desired access means that file system must grant as much rights for the file
 * as it is allowed for the current user.
 * HGFS client must know what access rights were granted to properly communicate this
 * information to the IoManager; grantedAccess field is used for this purpose.
 */
#pragma pack(push, 1)
typedef struct HgfsReplyOpenV4 {
   HgfsHandle file;                   /* Opaque file ID used by the server */
   HgfsLockType grantedLock;          /* The type of lock acquired by the server */
   HgfsOpenResult openResult;         /* Opened/overwritten or a new file created? */
   uint32 grantedAccess;              /* Granted access rights. */
   uint64 fileId;                     /* Persistent volume-wide unique file id. */
   uint64 volumeId;                   /* Persistent unique volume id. */
} HgfsReplyOpenV4;
#pragma pack(pop)

/*
 *  Flags that define behaviour of the move/creating hard link operation.
 */
typedef uint64 HgfsMoveLinkFlags;

#define HGFS_LINKMOVE_FLAG_REPLACE_EXISTING   (1 << 0)   /* Delete existing target. */
#define HGFS_LINKMOVE_FLAG_HARD_LINK          (1 << 1)   /* Create hard link. */

#pragma pack(push, 1)
typedef struct HgfsRequestLinkMoveV4 {
   HgfsFileNameV3 oldFileName;      /* Path to the exisitng source file.*/
   HgfsFileNameV3 newFileName;      /* Path to the destinatio name.*/
   HgfsMoveLinkFlags flags;         /* Flags that define behaviour of the operation.*/
} HgfsRequestLinkMoveV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyLinkMove4 {
   uint64 reserved;                 /* Reserved for future use. */
} HgfsReplyLinkMove4;
#pragma pack(pop)

/*
 * HgfsQueryVolumeMaskV4 mask in a request defines which volume properties client needs;
 * mask in a reply defines which properties were actually returned by the host.
 *
 * HGFS_QUERY_VOLUME_MASK_SIZE controls totalBytes, freeBytes and availableBytes.
 * HGFS_QUERY_VOLUME_MASK_FS_CAPABILITIES controls capabilities.
 * HGFS_QUERY_VOLUME_MASK_ATTRIBUTES controls creationTime.
 * HGFS_QUERY_VOLUME_MASK_VOLUME_GEOMETRY controls bytesPerSector and sectorPerCluster.
 * HGFS_QUERY_VOLUME_MASK_VOLUME_LABEL controls volume label.
 * HGFS_QUERY_VOLUME_MASK_FS_NAME controls fileSystemName.
 */
typedef uint64 HgfsQueryVolumeMaskV4;

#define HGFS_QUERY_VOLUME_MASK_SIZE             (1 << 0)
#define HGFS_QUERY_VOLUME_MASK_ATTRIBUTES       (1 << 1)
#define HGFS_QUERY_VOLUME_MASK_FS_CAPABILITIES  (1 << 2)
#define HGFS_QUERY_VOLUME_MASK_VOLUME_LABEL     (1 << 3)
#define HGFS_QUERY_VOLUME_MASK_VOLUME_GEOMETRY  (1 << 4)
#define HGFS_QUERY_VOLUME_MASK_FS_NAME          (1 << 5)

typedef uint64 HgfsFileSystemCapabilities;
#define HGFS_VOLUME_CASE_SENSITIVE           (1 << 0)
#define HGFS_VOLUME_SUPPORTS_EA              (1 << 1)
#define HGFS_VOLUME_SUPPORTS_COMPRESSION     (1 << 2)
#define HGFS_VOLUME_SUPPORTS_SHORT_NAMES     (1 << 3)
#define HGFS_VOLUME_SUPPORTS_ACL             (1 << 4)
#define HGFS_VOLUME_READ_ONLY                (1 << 5)
#define HGFS_VOLUME_SUPPORTS_ENCRYPTION      (1 << 6)
#define HGFS_VOLUME_SUPPORTS_OBJECT_ID       (1 << 7)
#define HGFS_VOLUME_SUPPORTS_REMOTE_STORAGE  (1 << 8)
#define HGFS_VOLUME_SUPPORTS_SYMLINKS        (1 << 9)
#define HGFS_VOLUME_SUPPORTS_SPARSE_FILES    (1 << 10)
#define HGFS_VOLUME_SUPPORTS_UNICODE         (1 << 11)
#define HGFS_VOLUME_SUPPORTS_QUOTA           (1 << 12)
#define HGFS_VOLUME_SUPPORTS_NAMED_STREAMS   (1 << 13)

#pragma pack(push, 1)
typedef struct HgfsRequestQueryVolumeV4 {
   HgfsQueryVolumeMaskV4 mask;
   HgfsFileNameV3 name;
} HgfsRequestQueryVolumeV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyQueryVolumeV4 {
   HgfsQueryVolumeMaskV4 mask; /* Identifies which values were set by the host. */
   uint64 totalBytes;          /* Total volume capacity. */
   uint64 freeBytes;           /* Free space on the volume. */
   uint64 availableBytes;      /* Free space available for the user. */
   HgfsFileSystemCapabilities capabilities; /* File system capabilities. */
   uint64 creationTime;        /* Volume creation time. */
   uint32 bytesPerSector;      /* Sector size for the volume. */
   uint32 sectorsPerCluster;   /* Cluster size for the volume. */
   HgfsFileName volumeLabel;   /* Volume name or label. */
   HgfsFileName fileSystemName;/* File system name. */
} HgfsReplyQueryVolumeV4;
#pragma pack(pop)

typedef uint32 HgfsSearchReadMask;
#define HGFS_SEARCH_READ_NAME                (1 << 0)
#define HGFS_SEARCH_READ_SHORT_NAME          (1 << 1)
#define HGFS_SEARCH_READ_FILE_SIZE           (1 << 2)
#define HGFS_SEARCH_READ_ALLOCATION_SIZE     (1 << 3)
#define HGFS_SEARCH_READ_EA_SIZE             (1 << 4)
#define HGFS_SEARCH_READ_TIME_STAMP          (1 << 5)
#define HGFS_SEARCH_READ_FILE_ATTRIBUTES     (1 << 6)
#define HGFS_SEARCH_READ_FILE_NODE_TYPE      (1 << 7)
#define HGFS_SEARCH_READ_REPARSE_TAG         (1 << 8)
#define HGFS_SEARCH_READ_FILE_ID             (1 << 9)

typedef uint32 HgfsSearchReadFlags;
#define HGFS_SEARCH_READ_INITIAL_QUERY       (1 << 1)
#define HGFS_SEARCH_READ_SINGLE_ENTRY        (1 << 2)
#define HGFS_SEARCH_READ_FID_OPEN_V4         (1 << 3)
#define HGFS_SEARCH_READ_REPLY_FINAL_ENTRY   (1 << 4)

/*
 * Read directory request can be used to enumerate files in a directory.
 * File handle used in the request can be either from HgfsRequestOpenV4 or
 * HgfsRequestSearchOpenV3.
 * searchPattern parameter allows filter out file names in the server for optimization.
 * It is optional - host may ignore patterns and return entries that do not match
 * the pattern. It is client responsibility to filter out names that do not match
 * the pattern.
 *
 * The mask field in request allows client to specify which properties it is
 * interested in. It allows to implement optimization in the server by skipping
 * parameters which client does not need.
 *
 * The HGFS Server fills mask field in the reply buffer to specify which
 * of the requested properties it supports, which may be a subset of the
 * requested properties.
 */

#pragma pack(push, 1)
typedef struct HgfsRequestSearchReadV4 {
   HgfsSearchReadMask mask;
   HgfsSearchReadFlags flags;
   HgfsHandle fid;
   uint32 replyDirEntryMaxSize;
   uint32 restartIndex;
   uint64 reserved;
   HgfsFileName searchPattern;
} HgfsRequestSearchReadV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsDirEntryV4 {
   uint32 nextEntryOffset;
   uint32 fileIndex;
   HgfsSearchReadMask mask;      /* Returned mask: may be a subset of requested mask. */
   HgfsAttrFlags attrFlags;      /* File system attributes of the entry */
   HgfsFileType fileType;
   uint64 fileSize;
   uint64 allocationSize;
   uint64 creationTime;
   uint64 accessTime;
   uint64 writeTime;
   uint64 attrChangeTime;
   uint64 hostFileId;            /* File Id of the file on host: inode_t on Linux */
   uint32 eaSize;                /* Byte size of any extended attributes. */
   uint32 reparseTag;            /* Windows only: reparse point tag. */
   uint64 reserved;              /* Reserved for future use. */
   HgfsShortFileName shortName;  /* Windows only: 8 dot 3 format name. */
   HgfsFileName fileName;        /* Entry file name. */
} HgfsDirEntryV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplySearchReadV4 {
   uint32 numberEntriesReturned; /* number of directory entries in this reply. */
   uint32 offsetToContinue;      /* Entry index of the directory entry. */
   HgfsSearchReadFlags flags;    /* Flags to indicate reply specifics */
   uint64 reserved;              /* Reserved for future use. */
   HgfsDirEntryV4 entries[1];    /* Unused as entries transfered using shared memory. */
} HgfsReplySearchReadV4;
#pragma pack(pop)

/*
 * File handle returned by HgfsRequestOpenV4 or later. Descriptors returned by
 * HgfsHandle fid; earlier versions of HgfsRequestOpen are not supported.
 */
#pragma pack(push, 1)
typedef struct HgfsRequestEnumerateStreamsV4 {
   uint32 restartIndex;
} HgfsRequestEnumerateStreamsV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsRequestStreamEntryV4 {
   uint32 nextEntryOffset;
   uint32 fileIndex;
   HgfsFileName fileName;
} HgfsRequestStreamEntryV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyEnumerateStreamsV4 {
   uint32 numberEntriesReturned;
   uint32 offsetToContinue;
   uint64 reserved;
   HgfsRequestStreamEntryV4 entries[1];
} HgfsReplyEnumerateStreamsV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsRequestGetattrV4 {
   uint32 mask;
   uint32 flags;
   uint64 reserved;
   HgfsFileNameV3 name;
} HgfsRequestGetattrV4;
#pragma pack(pop)

/*
 * V4 reports different file size for symlinks then V3 or V2.
 * It does not return file name length as EOF - it reports actual EOF.
 * On POSIX the value is always 0 and on Windows it is an actual EOF of
 * a file with a reparse point.
 * Each client must adjust the value for file size according to guest OS rules.
 *
 * Mask in HgfsAttr2V2 should be extended to include short name, symlink target and ACL.
 * If the host does not support a requested feature it is free to clear the
 * correspondent bit in the mask and ignore the feature.
 *
 * Multi-platform notice: symbolic link is represented by a file with REPARSE_POINT
 * on Windows. Thus Windows supports swtiching a file type between
 * regular or directory => symlink and back.
 * Setting symlinkTarget attribute on Windows host results in assigning
 * reparse point to the host file.
 */

#pragma pack(push, 1)
typedef struct HgfsAttrV4 {
   HgfsAttrV2 attr;
   uint32 numberOfLinks;
   HgfsFileName shortName;
   HgfsFileName symlinkTarget;
   uint32 aclLength;
   uint64 reserved;
   char acl[1];
} HgfsAttrV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyGetattrV4 {
   HgfsAttrV4 attr;
} HgfsReplyGetattrV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsRequestSetattrV4 {
   HgfsAttrHint hints;
   HgfsAttrV2 attr;
   uint64 reserved;          /* Reserved for future use */
   HgfsFileNameV3 fileName;  /* Filename used when file handle invalid. */
} HgfsRequestSetattrV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplySetattrV4 {
   uint32 mask;                      /* Defines which attributes were set. */
} HgfsReplySetattrV4;
#pragma pack(pop)

/*
 * Unlike V3 deletion this command can be used to delete both files and directories.
 * Its semantics depends on whether fid or file path is specified in the fileName.
 * When path is used it implements/emulates POSIX semantics - name is deleted from
 * the directory however if the file is opened it is still accessible. When fid is used
 * the file name disappears from the folder only when the last handle for the file is
 * closed - Windows style deletion.
 */

#pragma pack(push, 1)
typedef struct HgfsRequestDeleteFileV4 {
   HgfsFileNameV3 fileName;
} HgfsRequestDeleteFileV4;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct HgfsReplyDeleteFileV4 {
   uint64 reserved;
} HgfsReplyDeleteFileV4;
#pragma pack(pop)

#endif /* _HGFS_PROTO_H_ */
