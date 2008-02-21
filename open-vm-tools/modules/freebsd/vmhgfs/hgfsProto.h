/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
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
   HGFS_OP_DELETE_FILE_V2,    /* Delete a file */
   HGFS_OP_DELETE_DIR_V2,     /* Delete a directory */
   HGFS_OP_RENAME_V2,         /* Rename a file or directory */
   
   HGFS_OP_MAX,                /* Dummy op, must be last in enum */
} HgfsOp;


/*
 * File types, used in HgfsAttr. We support regular files,
 * directories, and symlinks.
 *
 * Changing the order of this enum will break the protocol; new types
 * should be added at the end.
 */

typedef enum {
   HGFS_FILE_TYPE_REGULAR,
   HGFS_FILE_TYPE_DIRECTORY,
   HGFS_FILE_TYPE_SYMLINK,
} HgfsFileType;


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
} HgfsServerLock;


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

typedef
#include "vmware_pack_begin.h"
struct HgfsAttr {
   HgfsFileType type;            /* File type */
   uint64 size;                  /* File size (in bytes) */
   uint64 creationTime;          /* Creation time. Ignored by POSIX */
   uint64 accessTime;            /* Time of last access */
   uint64 writeTime;             /* Time of last write */
   uint64 attrChangeTime;        /* Time file attributess were last
                                  * changed. Ignored by Windows */
   HgfsPermissions permissions;  /* Permissions bits */
}
#include "vmware_pack_end.h"
HgfsAttr;


/* Various flags and Windows attributes. */

typedef uint64 HgfsAttrFlags;                                                             

#define HGFS_ATTR_HIDDEN      (1 << 0)
#define HGFS_ATTR_SYSTEM      (1 << 1)
#define HGFS_ATTR_ARCHIVE     (1 << 2)


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

/*                                                                                        
 *  Version 2 of HgfsAttr
 */

typedef                                                                                   
#include "vmware_pack_begin.h"                                                            
struct HgfsAttrV2 {
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
   uint64 reserved1;             /* Reserved for future use */
   uint64 reserved2;             /* Reserved for future use */
}                                                                                         
#include "vmware_pack_end.h"                                                              
HgfsAttrV2; 


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
 * characters therefore hosts such as Mac OS X which
 * use HFS+ and unicode form D should convert names before
 * processing or sending HGFS requests.
 *
 * Precomposed (normal form C) versus Decomposed (normal form D)
 * -------------------------------------------------------------
 * Certain Unicode characters can be encoded in more than one way. 
 * For example, an Á (A acute) can be encoded either precomposed,
 * as U+00C1 (LATIN CAPITAL LETTER A WITH ACUTE), or decomposed, 
 * as U+0041 U+0301 (LATIN CAPITAL LETTER A followed by a COMBINING ACUTE ACCENT).
 * Precomposed characters are more common in the Windows world, 
 * whereas decomposed characters are more common on the Mac.
 *
 * See UAX 15 (http://unicode.org/reports/tr15/)
 */

typedef
#include "vmware_pack_begin.h"
struct HgfsFileName {
   uint32 length; /* Does NOT include terminating NUL */
   char name[1];
}
#include "vmware_pack_end.h"
HgfsFileName;


/*
 * Request/reply structs. These are the first members of all
 * operation request and reply messages, respectively.
 */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequest {
   HgfsHandle id;        /* Opaque request ID used by the requestor */
   HgfsOp op;
}
#include "vmware_pack_end.h"
HgfsRequest;


typedef
#include "vmware_pack_begin.h"
struct HgfsReply {
   HgfsHandle id;        /* Opaque request ID used by the requestor */
   HgfsStatus status;
}
#include "vmware_pack_end.h"
HgfsReply;


/*
 * Messages for our file operations.
 */

/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestOpen {
   HgfsRequest header;
   HgfsOpenMode mode;            /* Which type of access is requested */
   HgfsOpenFlags flags;          /* Which flags to open the file with */
   HgfsPermissions permissions;  /* Which permissions to *create* a new file with */
   HgfsFileName fileName;
}
#include "vmware_pack_end.h"
HgfsRequestOpen;


/* Version 2 of HgfsRequestOpen */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestOpenV2 {                                                                
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
   HgfsServerLock desiredLock;   /* The type of lock desired by the client */
   uint64 reserved1;             /* Reserved for future use */
   uint64 reserved2;             /* Reserved for future use */
   HgfsFileName fileName;                                                                 
}                                                                                         
#include "vmware_pack_end.h"                                                              
HgfsRequestOpenV2;  


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyOpen {
   HgfsReply header;
   HgfsHandle file;      /* Opaque file ID used by the server */
}
#include "vmware_pack_end.h"
HgfsReplyOpen;


/* Version 2 of HgfsReplyOpen */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyOpenV2 {
   HgfsReply header;
   HgfsHandle file;                  /* Opaque file ID used by the server */
   HgfsServerLock acquiredLock;      /* The type of lock acquired by the server */
}
#include "vmware_pack_end.h"
HgfsReplyOpenV2;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestRead {
   HgfsRequest header;
   HgfsHandle file;      /* Opaque file ID used by the server */
   uint64 offset;
   uint32 requiredSize;
}
#include "vmware_pack_end.h"
HgfsRequestRead;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplyRead {
   HgfsReply header;
   uint32 actualSize;
   char payload[1];
}
#include "vmware_pack_end.h"
HgfsReplyRead;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestWrite {
   HgfsRequest header;
   HgfsHandle file;      /* Opaque file ID used by the server */
   HgfsWriteFlags flags;
   uint64 offset;
   uint32 requiredSize;
   char payload[1];
}
#include "vmware_pack_end.h"
HgfsRequestWrite;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplyWrite {
   HgfsReply header;
   uint32 actualSize;
}
#include "vmware_pack_end.h"
HgfsReplyWrite;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestClose {
   HgfsRequest header;
   HgfsHandle file;      /* Opaque file ID used by the server */
}
#include "vmware_pack_end.h"
HgfsRequestClose;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplyClose {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplyClose;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestSearchOpen {
   HgfsRequest header;
   HgfsFileName dirName;
}
#include "vmware_pack_end.h"
HgfsRequestSearchOpen;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplySearchOpen {
   HgfsReply header;
   HgfsHandle search;    /* Opaque search ID used by the server */
}
#include "vmware_pack_end.h"
HgfsReplySearchOpen;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestSearchRead {
   HgfsRequest header;
   HgfsHandle search;    /* Opaque search ID used by the server */
   uint32 offset;        /* The first result is offset 0 */
}
#include "vmware_pack_end.h"
HgfsRequestSearchRead;


/* Version 2 of HgfsRequestSearchRead */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestSearchReadV2 {
   HgfsRequest header;
   HgfsHandle search;    /* Opaque search ID used by the server */
   uint32 offset;        /* The first result is offset 0 */
}
#include "vmware_pack_end.h"
HgfsRequestSearchReadV2;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplySearchRead {
   HgfsReply header;
   HgfsAttr attr;
   HgfsFileName fileName;
   /* fileName.length = 0 means "no entry at this offset" */
}
#include "vmware_pack_end.h"
HgfsReplySearchRead;


/* Version 2 of HgfsReplySearchRead */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplySearchReadV2 {                                                            
   HgfsReply header;                                                                      
   HgfsAttrV2 attr;
   
   /* 
    * fileName.length = 0 means "no entry at this offset" 
    * If the file is a symlink (as specified in attr)
    * this name is the name of the symlink, not the target.
    */   
   HgfsFileName fileName;   
}                                                                                         
#include "vmware_pack_end.h"                                                              
HgfsReplySearchReadV2; 


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestSearchClose {
   HgfsRequest header;
   HgfsHandle search;    /* Opaque search ID used by the server */
}
#include "vmware_pack_end.h"
HgfsRequestSearchClose;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplySearchClose {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplySearchClose;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestGetattr {
   HgfsRequest header;
   HgfsFileName fileName;
}
#include "vmware_pack_end.h"
HgfsRequestGetattr;


/* Version 2 of HgfsRequestGetattr */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestGetattrV2 {
   HgfsRequest header;
   HgfsAttrHint hints;     /* Flags for file handle valid. */
   HgfsHandle file;        /* Opaque file ID used by the server. */
   HgfsFileName fileName;  /* Filename used when file handle invalid. */
}
#include "vmware_pack_end.h"
HgfsRequestGetattrV2;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyGetattr {
   HgfsReply header;
   HgfsAttr attr;
}
#include "vmware_pack_end.h"
HgfsReplyGetattr;


/* Version 2 of HgfsReplyGetattr */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyGetattrV2 {
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
}
#include "vmware_pack_end.h"
HgfsReplyGetattrV2;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestSetattr {
   HgfsRequest header;
   HgfsAttrChanges update;  /* Which fields need to be updated */
   HgfsAttr attr;
   HgfsFileName fileName;
}
#include "vmware_pack_end.h"
HgfsRequestSetattr;


/* Version 2 of HgfsRequestSetattr */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestSetattrV2 {
   HgfsRequest header;
   HgfsAttrHint hints;
   HgfsAttrV2 attr;
   HgfsHandle file;        /* Opaque file ID used by the server. */
   HgfsFileName fileName;  /* Filename used when file handle invalid. */
}
#include "vmware_pack_end.h"
HgfsRequestSetattrV2;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplySetattr {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplySetattr;


/* Version 2 of HgfsReplySetattr */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplySetattrV2 {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplySetattrV2;


/* Version 2 of HgfsRequestCreateDir */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestCreateDirV2 {
   HgfsRequest header;
   HgfsCreateDirValid mask;
   HgfsPermissions specialPerms;
   HgfsPermissions ownerPerms;
   HgfsPermissions groupPerms;
   HgfsPermissions otherPerms;
   HgfsFileName fileName;
}
#include "vmware_pack_end.h"
HgfsRequestCreateDirV2;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestCreateDir {
   HgfsRequest header;
   HgfsPermissions permissions;
   HgfsFileName fileName;
}
#include "vmware_pack_end.h"
HgfsRequestCreateDir;


/* Version 2 of HgfsReplyCreateDir */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyCreateDirV2 {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplyCreateDirV2;


/* Deprecated */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyCreateDir {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplyCreateDir;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestDelete {
   HgfsRequest header;
   HgfsFileName fileName;
}
#include "vmware_pack_end.h"
HgfsRequestDelete;

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestDeleteV2 {
   HgfsRequest header;
   HgfsDeleteHint hints;
   HgfsHandle file;        /* Opaque file ID used by the server. */
   HgfsFileName fileName;  /* Name used if the file is HGFS_HANDLE_INVALID */
}
#include "vmware_pack_end.h"
HgfsRequestDeleteV2;

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyDelete {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplyDelete;

/* Version 2 of HgfsReplyDelete */

typedef
#include "vmware_pack_begin.h"
struct HgfsReplyDeleteV2 {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplyDeleteV2;

/*
 * The size of the HgfsFileName struct is variable depending on the
 * length of the name, so you can't use request->newName to get the
 * actual address of the new name, because where it starts is
 * dependant on how long the oldName is. To get the address of
 * newName, use this:
 *
 *          &oldName + sizeof(HgfsFileName) + oldName.length
 */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestRename {
   HgfsRequest header;
   HgfsFileName oldName;
   HgfsFileName newName;
}
#include "vmware_pack_end.h"
HgfsRequestRename;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplyRename {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplyRename;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestRenameV2 {
   HgfsRequest header;
   HgfsRenameHint hints;
   HgfsHandle srcFile;           /* Opaque file ID to "old name" used by the server. */
   HgfsHandle targetFile;        /* Opaque file ID to "old name" used by the server. */
   HgfsFileName oldName;
   HgfsFileName newName;
}
#include "vmware_pack_end.h"
HgfsRequestRenameV2;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplyRenameV2 {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplyRenameV2;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestQueryVolume {
   HgfsRequest header;
   HgfsFileName fileName;
}
#include "vmware_pack_end.h"
HgfsRequestQueryVolume;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplyQueryVolume {
   HgfsReply header;
   uint64 freeBytes;
   uint64 totalBytes;
}
#include "vmware_pack_end.h"
HgfsReplyQueryVolume;


/* New operations for Version 2 */

typedef
#include "vmware_pack_begin.h"
struct HgfsRequestServerLockChange {
   HgfsRequest header;
   HgfsHandle file;
   HgfsServerLock newServerLock;
}
#include "vmware_pack_end.h"
HgfsRequestServerLockChange;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplyServerLockChange {
   HgfsReply header;
   HgfsServerLock serverLock;
}
#include "vmware_pack_end.h"
HgfsReplyServerLockChange;


typedef
#include "vmware_pack_begin.h"
struct HgfsRequestSymlinkCreate {
   HgfsRequest header;
   HgfsFileName symlinkName;

   /* This filename is in "CPNameLite" format. See CPNameLite.c for details. */
   HgfsFileName targetName;
}
#include "vmware_pack_end.h"
HgfsRequestSymlinkCreate;


typedef
#include "vmware_pack_begin.h"
struct HgfsReplySymlinkCreate {
   HgfsReply header;
}
#include "vmware_pack_end.h"
HgfsReplySymlinkCreate;


#endif /* _HGFS_PROTO_H_ */
