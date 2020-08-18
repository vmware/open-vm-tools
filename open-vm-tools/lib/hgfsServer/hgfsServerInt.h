/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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


#ifndef __HGFS_SERVER_INT_H__
#   define __HGFS_SERVER_INT_H__

#include "vm_basic_types.h"

struct DirectoryEntry;

#ifndef _WIN32
   typedef int fileDesc;
#else
#  include <windows.h>
   typedef HANDLE fileDesc;
#endif

#include "dbllnklst.h"
#include "cpName.h"     // for HgfsNameStatus
#include "hgfsCache.h"
#include "hgfsProto.h"
#include "hgfsServer.h" // for the server public types
#include "hgfsServerPolicy.h"
#include "hgfsUtil.h"   // for HgfsInternalStatus
#include "userlock.h"
#include "vm_atomic.h"


#ifndef VMX86_TOOLS

#define LOGLEVEL_MODULE hgfsServer
#include "loglevel_user.h"

#else // VMX86_TOOLS

#undef DOLOG
#undef LOG

/*
 * Map all LOG statements to a Debug or g_debug tools log.
 * Set the level to a default log level of 10 so that we will
 * capture everything if tools logging is set to debug.
 *
 * Note, for future work would be to go through the log
 * statements and set the levels correctly so that we can
 * map to info, error and warnings.
*/
#define LGLEVEL         (10)
#define LGPFX_FMT       "%s:%s:"
#define LGPFX           "hgfsServer"

#if defined VMTOOLS_USE_GLIB
#define Debug                 g_debug
#define Warning               g_warning

#define G_LOG_DOMAIN    LGPFX

#include "vmware/tools/utils.h"
#include "vmware/tools/log.h"

#else // VMTOOLS_USE_GLIB

#include "debug.h"

#endif // VMTOOLS_USE_GLIB

#define DOLOG(_min)     ((_min) <= LGLEVEL)

/* gcc needs special syntax to handle zero-length variadic arguments */
#if defined(_MSC_VER)
#define LOG(_level, fmt, ...)                                     \
   do {                                                           \
      if (DOLOG(_level)) {                                        \
         Debug(LGPFX_FMT fmt, LGPFX , __FUNCTION__, __VA_ARGS__); \
      }                                                           \
   } while (0)
#else
#define LOG(_level, fmt, ...)                                      \
   do {                                                            \
      if (DOLOG(_level)) {                                         \
         Debug(LGPFX_FMT fmt, LGPFX, __FUNCTION__, ##__VA_ARGS__); \
      }                                                            \
   } while (0)
#endif

#endif // VNMX86_TOOLS

#define HGFS_DEBUG_ASYNC   (0)

typedef uintptr_t HOM_HANDLE;

typedef struct HgfsTransportSessionInfo HgfsTransportSessionInfo;

/* Identifier for a local file */
typedef struct HgfsLocalId {
   uint64 volumeId;
   uint64 fileId;
} HgfsLocalId;

typedef enum {
   REQ_ASYNC,    /* Hint that request should be processed Async. */
   REQ_SYNC,     /*               "                       Sync.  */
} RequestHint;


/* Three possible filenode states */
typedef enum {
   FILENODE_STATE_UNUSED,              /* Linked on the free list */
   FILENODE_STATE_IN_USE_CACHED,       /* Linked on the cached nodes list */
   FILENODE_STATE_IN_USE_NOT_CACHED,   /* Not linked on any list */
} FileNodeState;

/* Three possible search types */
typedef enum {
   DIRECTORY_SEARCH_TYPE_DIR,       /* Objects are files and subdirectories */
   DIRECTORY_SEARCH_TYPE_BASE,      /* Objects are shares */
   DIRECTORY_SEARCH_TYPE_OTHER,     /* Objects are the contents of
                                       "root/drive" or contents of "root" */
} DirectorySearchType;

#define HGFS_SEARCH_LAST_ENTRY_INDEX         ((uint32)~((uint32)0))


/* Two possible volume info type */
typedef enum {
   VOLUME_INFO_TYPE_MIN,
   VOLUME_INFO_TYPE_MAX,
} VolumeInfoType;

/*
 * The "default" share access is used in cross-platform code, so it's helpful
 * to have a single macro for accessing it.
 */
#ifdef _WIN32
#  define HGFS_DEFAULT_SHARE_ACCESS (FILE_SHARE_READ | FILE_SHARE_WRITE | \
                                     FILE_SHARE_DELETE)
#else
#  define HGFS_DEFAULT_SHARE_ACCESS 0
#endif // _WIN32

typedef struct HgfsShareInfo {
   /* Filename of the root directory for the shared folder */
   const char *rootDir;

   /* Length of the root directory filename (does not include nul terminator) */
   size_t rootDirLen;

   /* Read permissions for the shared folder, needed for handle => name conversions. */
   Bool readPermissions;

   /* Write permissions for the shared folder, needed for handle => name conversions. */
   Bool writePermissions;

   /*
    *  Shared folder handle used by change directory notification code to identify
    *  shared folder.
    */
   HgfsSharedFolderHandle handle;
} HgfsShareInfo;

/*
 * This struct represents a file on the local filesystem that has been
 * opened by a remote client. We store the name of the local file and
 * enough state to keep track of whether the file has changed locally
 * between remote accesses. None of the fields contain cross-platform
 * types; everything has been converted for the local filesystem.
 *
 * A file node object can only be in 1 of these 3 states:
 * 1) FILENODE_STATE_UNUSED: linked on the free list
 * 2) FILENODE_STATE_IN_USE_CACHED: Linked on the cached nodes list
 * 3) FILENODE_STATE_IN_USE_NOT_CACHED: Linked on neither of the above two lists.
 */
typedef struct HgfsFileNode {
   /* Links to place the object on various lists */
   DblLnkLst_Links links;

   /* HGFS handle uniquely identifying this node. */
   HgfsHandle handle;

   /* Local filename (in UTF8) */
   char *utf8Name;

   /* Length of filename (does not include nul terminator) */
   size_t utf8NameLen;

   /* share name */
   char *shareName;

   /* Length of share name (does not include nul terminator) */
   size_t shareNameLen;

   /* ID of file in local filesystem */
   HgfsLocalId localId;

   /* File descriptor */
   fileDesc fileDesc;

   /* On POSIX, access mode. On Windows, desired access */
   uint32 mode;

   /* Share access to open with (Windows only) */
   uint32 shareAccess;

   /* The server lock that the node currently has. */
   HgfsLockType serverLock;

   /* File node state on lists */
   FileNodeState state;

   /* File flags - see below. */
   uint32 flags;

   /*
    * Context as required by some file operations. Eg: BackupWrite on
    * Windows: BackupWrite requires the caller to hold on to a pointer
    * to a Windows internal data structure between subsequent calls to
    * BackupWrite while restoring a file.
    */
   void *fileCtx;

   /* Parameters associated with the share. */
   HgfsShareInfo shareInfo;
} HgfsFileNode;


/* HgfsFileNode flags. */

/* TRUE if opened in append mode */
#define HGFS_FILE_NODE_APPEND_FL               (1 << 0)
/* Whether this file was opened in sequential mode. */
#define HGFS_FILE_NODE_SEQUENTIAL_FL           (1 << 1)
/* Whether this a shared folder open. */
#define HGFS_FILE_NODE_SHARED_FOLDER_OPEN_FL   (1 << 2)

/*
 * This struct represents a file search that a client initiated.
 *
 * A search object can only be in 1 of these 2 states:
 * 1) Unused: linked on the free list
 * 2) In use: unlinked
 */
typedef struct HgfsSearch {
   /* Links to place the object on various lists */
   DblLnkLst_Links links;

   /* Flags to track state and information: see below. */
   uint32 flags;

   /* HGFS handle uniquely identifying this search. */
   HgfsHandle handle;

   /* Local directory name (in UTF8) */
   char *utf8Dir;

   /* Length of directory name (does not include nul terminator) */
   size_t utf8DirLen;

   /* Share name. */
   char *utf8ShareName;

   /* Share name length. */
   size_t utf8ShareNameLen;

   /* Directory entries for this search */
   struct DirectoryEntry **dents;

   /* Number of dents */
   uint32 numDents;

   /*
    * What type of search is this (what objects does it track)? This is
    * important to know so we can do the right kind of stat operation later
    * when we want to retrieve the attributes for each dent.
    */
   DirectorySearchType type;

   /* Parameters associated with the share. */
   HgfsShareInfo shareInfo;
} HgfsSearch;

/* HgfsSearch flags. */

/* TRUE if opened in append mode */
#define HGFS_SEARCH_FLAG_READ_ALL_ENTRIES      (1 << 0)

/* HgfsSessionInfo flags. */
typedef enum {
   HGFS_SESSION_TYPE_REGULAR,      /* Dynamic session, created by the HgfsTransport. */
   HGFS_SESSION_TYPE_INTERNAL,     /* This is a static session. */
} HgfsSessionInfoType;

/* HgfsSessionState, used for session status. */
typedef enum {
   HGFS_SESSION_STATE_OPEN,
   HGFS_SESSION_STATE_CLOSED,
} HgfsSessionInfoState;

typedef struct HgfsAsyncRequestInfo {
   /* Asynchronous request handling. */
   Atomic_uint32   requestCount;
   MXUserExclLock *lock;
   MXUserCondVar  *requestCountIsZero;
} HgfsAsyncRequestInfo;

typedef struct HgfsSessionInfo {

   DblLnkLst_Links links;

   Bool isInactive;

   /* The sessions state and capabilities. */
   HgfsSessionFlags flags;

   /* Unique session id. */
   uint64 sessionId;

   /* Max packet size that is supported by both client and server. */
   uint32 maxPacketSize;

   /* Transport session context. */
   HgfsTransportSessionInfo *transportSession;

   /* Current state of the session. */
   HgfsSessionInfoState state;

   /* Lock to ensure some fileIO requests are atomic for a handle. */
   MXUserExclLock *fileIOLock;

   int numInvalidationAttempts;

   Atomic_uint32 refCount;    /* Reference count for session. */

   /*
    ** START NODE ARRAY **************************************************
    *
    * Lock for the following 6 fields: the node array,
    * counters and lists for this session.
    */
   MXUserExclLock *nodeArrayLock;

   /* Open file nodes of this session. */
   HgfsFileNode *nodeArray;

   /* Number of nodes in the nodeArray. */
   uint32 numNodes;

   /* Free list of file nodes. LIFO to be cache-friendly. */
   DblLnkLst_Links nodeFreeList;

   /* List of cached open nodes. */
   DblLnkLst_Links nodeCachedList;

   /* Current number of open nodes. */
   unsigned int numCachedOpenNodes;

   /* Number of open nodes having server locks. */
   unsigned int numCachedLockedNodes;
   /** END NODE ARRAY ****************************************************/

   /*
    ** START SEARCH ARRAY ************************************************
    *
    * Lock for the following three fields: for the search array
    * and it's counter and list, for this session.
    */
   MXUserExclLock *searchArrayLock;

   /* Directory entry cache for this session. */
   HgfsSearch *searchArray;

   /* Number of entries in searchArray. */
   uint32 numSearches;

   /* Free list of searches. LIFO. */
   DblLnkLst_Links searchFreeList;
   /** END SEARCH ARRAY ****************************************************/

   /* Array of session specific capabiities. */
   HgfsOpCapability hgfsSessionCapabilities[HGFS_OP_MAX];

   uint32 numberOfCapabilities;

   /* Asynchronous request handling. */
   HgfsAsyncRequestInfo asyncRequestsInfo;

   /* Cache for symlink check status. */
   HgfsCache *symlinkCache;

   /* Cache for file attributes. */
   HgfsCache *fileAttrCache;
} HgfsSessionInfo;

/*
 * This represents the maximum number of HGFS sessions that can be
 * created in a HGFS transport session. We picked a random value
 * for this variable. There is no specific reason behind picking
 * this value.
 */
#define MAX_SESSION_COUNT 1024

/*
 * This represents the maximum number attempts made by the HGFS
 * invalidator before completely destroying the HGFS session. We
 * picked a random value and there is no specific reason behind
 * the value 4 for thie variable.
 */
#define MAX_SESSION_INVALIDATION_ATTEMPTS 4

/*
 * These structs represent information about file open requests, file
 * attributes, and directory creation requests.
 *
 * The main reason for these structs is data abstraction -- we pass
 * a struct around instead of the individual parameters. This way
 * as more parameters are implemented, we don't have to add more
 * parameters to the functions, instead just extend the structs.
 */

typedef struct HgfsFileOpenInfo {
   HgfsOp requestType;
   HgfsHandle file;                  /* Opaque file ID used by the server */
   HgfsOpenValid mask;               /* Bitmask that specified which fields are valid. */
   HgfsOpenMode mode;                /* Which type of access requested. See desiredAccess */
   HgfsOpenFlags flags;              /* Which flags to open the file with */
   HgfsPermissions specialPerms;     /* Desired 'special' permissions for file creation */
   HgfsPermissions ownerPerms;       /* Desired 'owner' permissions for file creation */
   HgfsPermissions groupPerms;       /* Desired 'group' permissions for file creation */
   HgfsPermissions otherPerms;       /* Desired 'other' permissions for file creation */
   HgfsAttrFlags attr;               /* Attributes, if any, for file creation */
   uint64 allocationSize;            /* How much space to pre-allocate during creation */
   uint32 desiredAccess;             /* Extended support for windows access modes */
   uint32 shareAccess;               /* Windows only, share access modes */
   HgfsLockType desiredLock;         /* The type of lock desired by the client */
   HgfsLockType acquiredLock;        /* The type of lock acquired by the server */
   uint32 cpNameSize;
   const char *cpName;
   char *utf8Name;
   uint32 caseFlags;                 /* Case-sensitivity flags. */
   HgfsShareInfo shareInfo;          /* Parameters associated with the share. */
} HgfsFileOpenInfo;

typedef struct HgfsFileAttrInfo {
   HgfsOp requestType;
   HgfsAttrValid mask;
   HgfsFileType type;            /* File type */
   uint64 size;                  /* File size (in bytes) */
   uint64 creationTime;          /* Creation time. Ignored by POSIX */
   uint64 accessTime;            /* Time of last access */
   uint64 writeTime;             /* Time of last write */
   uint64 attrChangeTime;        /* Time file attributes were last
                                  * changed. Ignored by Windows */
   HgfsPermissions specialPerms; /* Special permissions bits. Ignored by Windows */
   HgfsPermissions ownerPerms;   /* Owner permissions bits */
   HgfsPermissions groupPerms;   /* Group permissions bits. Ignored by Windows */
   HgfsPermissions otherPerms;   /* Other permissions bits. Ignored by Windows */
   HgfsAttrFlags flags;          /* Various flags and Windows 'attributes' */
   uint64 allocationSize;        /* Actual size of file on disk */
   uint32 userId;                /* User identifier, ignored by Windows */
   uint32 groupId;               /* group identifier, ignored by Windows */
   uint64 hostFileId;            /* File Id of the file on host: inode_t on Linux */
   uint32 volumeId;              /* Volume Id of the volune on which the file resides */
   uint32 effectivePerms;        /* Permissions in effect for the current user */
   uint32 eaSize;                /* Extended attribute data size */
   uint32 reparseTag;            /* Windows reparse point tag, valid by attr flag */
   HgfsShortFileName shortName;  /* Windows DOS 8 dot 3 name for long names */
} HgfsFileAttrInfo;

typedef struct HgfsSearchReadEntry {
   HgfsSearchReadMask mask;      /* Info returned mask */
   HgfsFileAttrInfo attr;        /* Attributes of entry */
   uint32 fileIndex;             /* Entry directory index */
   char *name;                   /* Name */
   uint32 nameLength;            /* Name byte length */
} HgfsSearchReadEntry;

typedef struct HgfsSearchReadInfo {
   HgfsOp requestType;           /* HGFS request version */
   HgfsSearchReadMask requestedMask; /* Entry info requested mask */
   HgfsSearchReadFlags flags;    /* Request specific flags */
   HgfsSearchReadFlags replyFlags;   /* Reply specific flags */
   char *searchPattern;          /* Search pattern to match entries with */
   uint32 searchPatternLength;   /* Byte length of search pattern */
   uint32 startIndex;            /* Starting index for entries */
   uint32 currentIndex;          /* Current index for entries */
   uint32 numberRecordsWritten;  /* Number of entries written */
   void *reply;                  /* Fixed part of search read reply */
   void *replyPayload;           /* Variable part (dirent records) of reply */
   size_t payloadSize;           /* Remaining bytes in reply payload. */
} HgfsSearchReadInfo;

typedef struct HgfsCreateDirInfo {
   HgfsOp requestType;
   HgfsCreateDirValid mask;
   HgfsPermissions specialPerms; /* Special permissions bits. Ignored by Windows */
   HgfsPermissions ownerPerms;   /* Owner permissions bits */
   HgfsPermissions groupPerms;   /* Group permissions bits. Ignored by Windows */
   HgfsPermissions otherPerms;   /* Other permissions bits. Ignored by Windows */
   uint32 cpNameSize;
   const char *cpName;
   uint32 caseFlags;             /* Case-sensitivity flags. */
   HgfsAttrFlags fileAttr;       /* Various flags and Windows 'attributes' */
} HgfsCreateDirInfo;

typedef struct HgfsCreateSessionInfo {
   uint32 maxPacketSize;
   HgfsSessionFlags flags;       /* Session capability flags. */
} HgfsCreateSessionInfo;

typedef struct HgfsSymlinkCacheEntry {
   HOM_HANDLE handle;            /* File handle. */
   HgfsNameStatus nameStatus;    /* Symlink check status. */
} HgfsSymlinkCacheEntry;

typedef struct HgfsFileAttrCacheEntry {
   HOM_HANDLE handle;            /* File handle. */
   HgfsFileAttrInfo attr;        /* Attributes of entry. */
} HgfsFileAttrCacheEntry;

Bool
HgfsCreateAndCacheFileNode(HgfsFileOpenInfo *openInfo, // IN: Open info struct
                           HgfsLocalId const *localId, // IN: Local unique file ID
                           fileDesc fileDesc,          // IN: OS file handle
                           Bool append,                // IN: Open with append flag
                           HgfsSessionInfo *session);  // IN: Session info

Bool
HgfsSearchHandle2FileName(HgfsHandle handle,       // IN: Hgfs search handle
                          char **fileName,         // OUT: cp file name
                          uint32 *fileNameSize);   // OUT: cp file name size

void
HgfsUpdateNodeNames(const char *oldLocalName,  // IN: Name of file to look for
                    const char *newLocalName,  // IN: Name to replace with
                    HgfsSessionInfo *session); // IN: Session info

Bool
HgfsRemoveSearch(HgfsHandle searchHandle,
                 HgfsSessionInfo *session);

#ifdef VMX86_LOG
#define HGFS_SERVER_DIR_DUMP_DENTS(_searchHandle, _session) do {    \
      if (DOLOG(4)) {                                               \
         HgfsServerDirDumpDents(_searchHandle, _session);           \
      }                                                             \
   } while (0)

void
HgfsServerDirDumpDents(HgfsHandle searchHandle,   // IN: Handle to dump dents from
                       HgfsSessionInfo *session); // IN: Session info
#else
#define HGFS_SERVER_DIR_DUMP_DENTS(_searchHandle, _session) do {} while (0)
#endif


struct DirectoryEntry *
HgfsGetSearchResult(HgfsHandle handle,        // IN: Handle to search
                    HgfsSessionInfo *session, // IN: Session info
                    uint32 offset,            // IN: Offset to retrieve at
                    Bool remove);             // IN: If true, removes the result

Bool
HgfsServerStatFs(const char *pathName, // IN: Path we're interested in
                 size_t pathLength,    // IN: Length of path
                 uint64 *freeBytes,    // OUT: Free bytes on volume
                 uint64 *totalBytes);  // OUT: Total bytes on volume

HgfsNameStatus
HgfsServerGetAccess(char *in,                    // IN:  CP filename to check
                    size_t inSize,               // IN:  Size of name in
                    HgfsOpenMode mode,           // IN:  Requested access mode
                    uint32 caseFlags,            // IN:  Case-sensitivity flags
                    char **bufOut,               // OUT: File name in local fs
                    size_t *outLen);             // OUT: Length of name out

Bool
HgfsServerIsSharedFolderOnly(char const *in,  // IN:  CP filename to check
                             size_t inSize);  // IN:  Size of name in

void *
HgfsServerResEnumInit(void);

Bool
HgfsServerResEnumGet(void *enumState,
                     char const **enumResName,
                     size_t *enumResNameLen,
                     Bool *enumResDone);

Bool
HgfsServerResEnumExit(void *enumState);

HgfsInternalStatus
HgfsServerGetDirEntry(HgfsHandle handle,                // IN: Handle to search
                      HgfsSessionInfo *session,         // IN: Session info
                      uint32 index,                     // IN: index to retrieve at
                      Bool remove,                      // IN: If true, removes the result
                      struct DirectoryEntry **dirEntry);// OUT: directory entry

HgfsInternalStatus
HgfsServerSearchRealDir(char const *baseDir,      // IN: Directory to search
                        size_t baseDirLen,        // IN: Length of directory
                        char const *shareName,    // IN: Share name
                        char const *rootDir,      // IN: Root directory for the share
                        HgfsSessionInfo *session, // IN: Session info
                        HgfsHandle *handle);      // OUT: Search handle

HgfsInternalStatus
HgfsServerSearchVirtualDir(HgfsServerResEnumGetFunc getName,      // IN: Name enumerator
                           HgfsServerResEnumInitFunc initName,    // IN: Init function
                           HgfsServerResEnumExitFunc cleanupName, // IN: Cleanup function
                           DirectorySearchType type,              // IN: Kind of search
                           HgfsSessionInfo *session,              // IN: Session info
                           HgfsHandle *handle);                   // OUT: Search handle

HgfsInternalStatus
HgfsServerRestartSearchVirtualDir(HgfsServerResEnumGetFunc getName,      // IN: Name enumerator
                                  HgfsServerResEnumInitFunc initName,    // IN: Init function
                                  HgfsServerResEnumExitFunc cleanupName, // IN: Cleanup function
                                  HgfsSessionInfo *session,              // IN: Session info
                                  HgfsHandle searchHandle);              // IN: search to restart


void *
HgfsAllocInitReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                   const void *packetHeader,     // IN: incoming packet header
                   size_t replyDataSize,         // IN: payload size
                   HgfsSessionInfo *session);    // IN: Session Info

/* Node cache functions. */

Bool
HgfsRemoveFromCache(HgfsHandle handle,         // IN: Hgfs handle of the node
                    HgfsSessionInfo *session); // IN: Session info

Bool
HgfsAddToCache(HgfsHandle handle,         // IN: Hgfs handle of the node
               HgfsSessionInfo *session); // IN: Session info

Bool
HgfsIsCached(HgfsHandle handle,         // IN: Hgfs handle of the node
             HgfsSessionInfo *session); // IN: Session info

Bool
HgfsIsServerLockAllowed(HgfsSessionInfo *session);  // IN: session info

Bool
HgfsHandle2FileDesc(HgfsHandle handle,        // IN: Hgfs file handle
                    HgfsSessionInfo *session, // IN: session info
                    fileDesc *fd,             // OUT: OS handle (file descriptor)
                    void **fileCtx);          // OUT: OS file context

Bool
HgfsFileDesc2Handle(fileDesc fd,              // IN: OS handle (file descriptor)
                    HgfsSessionInfo *session, // IN: session info
                    HgfsHandle *handle);      // OUT: Hgfs file handle

Bool
HgfsHandle2ShareMode(HgfsHandle handle,         // IN: Hgfs file handle
                     HgfsSessionInfo *session,  // IN: session info
                     HgfsOpenMode *shareMode);  // OUT: UTF8 file name size

Bool
HgfsHandle2FileName(HgfsHandle handle,        // IN: Hgfs file handle
                    HgfsSessionInfo *session, // IN: session info
                    char **fileName,          // OUT: CP file name
                    size_t *fileNameSize);    // OUT: CP file name size
Bool
HgfsHandle2FileNameMode(HgfsHandle handle,       // IN: Hgfs file handle
                        HgfsSessionInfo *session,// IN: Session info
                        Bool *readPermissions,   // OUT: shared folder permissions
                        Bool *writePermissions,  // OUT: shared folder permissions
                        char **fileName,         // OUT: UTF8 file name
                        size_t *fileNameSize);   // OUT: UTF8 file name size
Bool
HgfsHandle2AppendFlag(HgfsHandle handle,        // IN: Hgfs file handle
                      HgfsSessionInfo *session, // IN: session info
                      Bool *appendFlag);        // OUT: Append flag

Bool
HgfsHandle2LocalId(HgfsHandle handle,        // IN: Hgfs file handle
                   HgfsSessionInfo *session, // IN: session info
                   HgfsLocalId *localId);    // OUT: Local id info


Bool
HgfsUpdateNodeFileDesc(HgfsHandle handle,        // IN: Hgfs file handle
                       HgfsSessionInfo *session, // IN: session info
                       fileDesc fd,              // IN: OS handle (file desc
                       void *fileCtx);           // IN: OS file context

Bool
HgfsUpdateNodeServerLock(fileDesc fd,                // IN: OS handle
                         HgfsSessionInfo *session,   // IN: session info
                         HgfsLockType serverLock);   // IN: new oplock

Bool
HgfsUpdateNodeAppendFlag(HgfsHandle handle,        // IN: Hgfs file handle
                         HgfsSessionInfo *session, // IN: session info
                         Bool appendFlag);         // OUT: Append flag

Bool
HgfsGetNodeCopy(HgfsHandle handle,        // IN: Hgfs file handle
                HgfsSessionInfo *session, // IN: session info
                Bool copyName,            // IN: Should we copy the name?
                HgfsFileNode *copy);      // IN/OUT: Copy of the node

Bool
HgfsHandleIsSequentialOpen(HgfsHandle handle,        // IN:  Hgfs file handle
                           HgfsSessionInfo *session, // IN: session info
                           Bool *sequentialOpen);    // OUT: If open was sequential

Bool
HgfsHandleIsSharedFolderOpen(HgfsHandle handle,        // IN:  Hgfs file handle
                             HgfsSessionInfo *session, // IN: session info
                             Bool *sharedFolderOpen);  // OUT: If shared folder

Bool
HgfsGetSearchCopy(HgfsHandle handle,        // IN: Hgfs search handle
                  HgfsSessionInfo *session, // IN: Session info
                  HgfsSearch *copy);        // IN/OUT: Copy of the search

Bool
HgfsServerGetOpenMode(HgfsFileOpenInfo *openInfo, // IN:  Open info to examine
                      uint32 *modeOut);           // OUT: Local mode


char*
HgfsServerGetTargetRelativePath(const char* source,    // IN: source file name
                                const char* target);   // IN: target file name

Bool
HgfsServerCheckOpenFlagsForShare(HgfsFileOpenInfo *openInfo, // IN: Hgfs file handle
                                 HgfsOpenFlags *flags);      // IN/OUT: open mode

/* Platform specific exports. */
Bool
HgfsPlatformInit(void);
void
HgfsPlatformDestroy(void);
HgfsInternalStatus
HgfsPlatformCloseFile(fileDesc fileDesc,            // IN: OS handle of the file
                      void *fileCtx);               // IN: file context
Bool
HgfsPlatformDoFilenameLookup(void);
HgfsNameStatus
HgfsPlatformFilenameLookup(const char *sharePath,             // IN: share path in question
                           size_t sharePathLength,            // IN
                           char *fileName,                    // IN: filename to be looked up
                           size_t fileNameLength,             // IN
                           uint32 caseFlags,                  // IN: case-sensitivity flags
                           char **convertedFileName,          // OUT: case-converted filename
                           size_t *convertedFileNameLength);  // OUT
HgfsInternalStatus
HgfsPlatformConvertFromNameStatus(HgfsNameStatus status);  // IN: name status
HgfsNameStatus
HgfsPlatformPathHasSymlink(const char *fileName,      // IN: fileName to be checked
                           size_t fileNameLength,     // IN
                           const char *sharePath,     // IN: share path in question
                           size_t sharePathLen);      // IN
HgfsInternalStatus
HgfsPlatformSymlinkCreate(char *localSymlinkName,   // IN: symbolic link file name
                          char *localTargetName);   // IN: symlink target name
HgfsInternalStatus
HgfsPlatformGetattrFromName(char *fileName,                 // IN: file name
                            HgfsShareOptions configOptions, // IN: configuration options
                            char *shareName,                // IN: share name
                            HgfsFileAttrInfo *attr,         // OUT: file attributes
                            char **targetName);             // OUT: Symlink target
HgfsInternalStatus
HgfsPlatformGetDirEntry(HgfsSearch *search,                // IN: search
                        HgfsSessionInfo *session,          // IN: Session info
                        uint32 offset,                     // IN: Offset to retrieve at
                        Bool remove,                       // IN: If true, removes the result
                        struct DirectoryEntry **dirEntry); // OUT: dirent
HgfsInternalStatus
HgfsPlatformSetDirEntry(HgfsSearch *search,              // IN: search
                        HgfsShareOptions configOptions,  // IN: share configuration settings
                        HgfsSessionInfo *session,        // IN: session info
                        struct DirectoryEntry *dirEntry, // IN: the indexed dirent
                        Bool getAttr,                    // IN: get the entry attributes
                        HgfsFileAttrInfo *entryAttr,     // OUT: entry attributes, optional
                        char **entryName,                // OUT: entry name
                        uint32 *entryNameLength);        // OUT: entry name length
HgfsInternalStatus
HgfsPlatformScandir(char const *baseDir,             // IN: Directory to search in
                    size_t baseDirLen,               // IN: Length of directory
                    Bool followSymlinks,             // IN: followSymlinks config option
                    struct DirectoryEntry ***dents,  // OUT: Array of DirectoryEntrys
                    int *numDents);                  // OUT: Number of DirectoryEntrys
HgfsInternalStatus
HgfsPlatformScanvdir(HgfsServerResEnumGetFunc enumNamesGet,   // IN: Function to get name
                     HgfsServerResEnumInitFunc enumNamesInit, // IN: Setup function
                     HgfsServerResEnumExitFunc enumNamesExit, // IN: Cleanup function
                     DirectorySearchType type,                // IN: Kind of search
                     struct DirectoryEntry ***dents,          // OUT: Array of DirectoryEntrys
                     uint32 *numDents);                       // OUT: total number of directory entrys
HgfsInternalStatus
HgfsPlatformSearchDir(HgfsNameStatus nameStatus,       // IN: name status
                      const char *dirName,             // IN: relative directory name
                      size_t dirNameLength,            // IN: length of dirName
                      uint32 caseFlags,                // IN: case flags
                      HgfsShareInfo *shareInfo,        // IN: sharfed folder information
                      char *baseDir,                   // IN: name of the shared directory
                      uint32 baseDirLen,               // IN: length of the baseDir
                      HgfsSessionInfo *session,        // IN: session info
                      HgfsHandle *handle);             // OUT: search handle
HgfsInternalStatus
HgfsPlatformRestartSearchDir(HgfsHandle handle,               // IN: search handle
                             HgfsSessionInfo *session,        // IN: session info
                             DirectorySearchType searchType); // IN: Kind of search
#ifdef VMX86_LOG
void
HgfsPlatformDirDumpDents(HgfsSearch *search);         // IN: search
#endif

HgfsInternalStatus
HgfsPlatformReadFile(fileDesc readFile,           // IN: file descriptor
                     HgfsSessionInfo *session,    // IN: session info
                     uint64 offset,               // IN: file offset to read from
                     uint32 requiredSize,         // IN: length of data to read
                     void* payload,               // OUT: buffer for the read data
                     uint32 *actualSize);         // OUT: actual length read
HgfsInternalStatus
HgfsPlatformWriteFile(fileDesc writeFile,          // IN: file descriptor
                      HgfsSessionInfo *session,    // IN: session info
                      uint64 writeOffset,          // IN: file offset to write to
                      uint32 writeDataSize,        // IN: length of data to write
                      HgfsWriteFlags writeFlags,   // IN: write flags
                      Bool writeSequential,        // IN: write is sequential
                      Bool writeAppend,            // IN: write is appended
                      const void *writeData,       // IN: data to be written
                      uint32 *writtenSize);        // OUT: byte length written
HgfsInternalStatus
HgfsPlatformWriteWin32Stream(HgfsHandle file,           // IN: packet header
                             char *dataToWrite,         // IN: data to write
                             size_t requiredSize,       // IN: data size
                             Bool doSecurity,           // IN: write ACL
                             uint32  *actualSize,       // OUT: written data size
                             HgfsSessionInfo *session); // IN: session info
HgfsInternalStatus
HgfsPlatformVDirStatsFs(HgfsSessionInfo *session,  // IN: session info
                        HgfsNameStatus nameStatus, // IN:
                        VolumeInfoType infoType,   // IN:
                        uint64 *outFreeBytes,      // OUT:
                        uint64 *outTotalBytes);    // OUT:

HgfsInternalStatus
HgfsPlatformGetFd(HgfsHandle hgfsHandle,    // IN:  HGFS file handle
                  HgfsSessionInfo *session, // IN:  Session info
                  Bool append,              // IN:  Open with append flag
                  fileDesc *fd);            // OUT: Opened file descriptor
HgfsInternalStatus
HgfsPlatformFileExists(char *utf8LocalName); // IN: Full file path utf8 encoding

/*
 * NOTE.
 * This function requires valid localSrcName and localTargetName even when
 * srcFile and targetFile are specified.
 * Depending on some various conditions it may fall back on using file names
 * instead of file handles.
 */
HgfsInternalStatus
HgfsPlatformRename(char *localSrcName,     // IN: local path to source file
                   fileDesc srcFile,       // IN: source file handle
                   char *localTargetName,  // IN: local path to target file
                   fileDesc targetFile,    // IN: target file handle
                   HgfsRenameHint hints);  // IN: rename hints
HgfsInternalStatus
HgfsPlatformCreateDir(HgfsCreateDirInfo *info,  // IN: direcotry properties
                      char *utf8Name);          // IN: full path for the new directory
HgfsInternalStatus
HgfsPlatformDeleteFileByHandle(HgfsHandle file,           // IN: file being deleted
                               HgfsSessionInfo *session); // IN: session info
HgfsInternalStatus
HgfsPlatformDeleteFileByName(char const *utf8Name); // IN: full file path in utf8 encoding
HgfsInternalStatus
HgfsPlatformDeleteDirByHandle(HgfsHandle dir,            // IN: directory being deleted
                              HgfsSessionInfo *session); // IN: session info
HgfsInternalStatus
HgfsPlatformDeleteDirByName(char const *utf8Name); // IN: full file path in utf8 encoding
HgfsInternalStatus
HgfsPlatformHandleIncompleteName(HgfsNameStatus nameStatus,  // IN: name status
                                 HgfsFileAttrInfo *attr);    // OUT: attributes
void
HgfsPlatformGetDefaultDirAttrs(HgfsFileAttrInfo *attr); // OUT: attributes
HgfsInternalStatus
HgfsPlatformGetattrFromFd(fileDesc fileDesc,        // IN: file descriptor to query
                          HgfsSessionInfo *session, // IN: session info
                          HgfsFileAttrInfo *attr);  // OUT: file attributes
HgfsInternalStatus
HgfsPlatformSetattrFromFd(HgfsHandle file,          // IN: file descriptor
                          HgfsSessionInfo *session, // IN: session info
                          HgfsFileAttrInfo *attr,   // IN: attrs to set
                          HgfsAttrHint hints,       // IN: attr hints
                          Bool useHostTime);        // IN: use current host time
HgfsInternalStatus
HgfsPlatformSetattrFromName(char *utf8Name,                 // IN: local file path
                            HgfsFileAttrInfo *attr,         // IN: attrs to set
                            HgfsShareOptions configOptions, // IN: share options
                            HgfsAttrHint hints,             // IN: attr hints
                            Bool useHostTime);              // IN: use current host time
HgfsInternalStatus
HgfsPlatformValidateOpen(HgfsFileOpenInfo *openInfo, // IN: Open info struct
                         Bool followLinks,           // IN: follow symlinks on the host
                         HgfsSessionInfo *session,   // IN: Session info
                         HgfsLocalId *localId,       // OUT: Local unique file ID
                         fileDesc *newHandle);       // OUT: Handle to the file

void *
HSPU_GetMetaPacket(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                   size_t *metaPacketSize,               // OUT: Size of metaPacket
                   HgfsServerChannelCallbacks *chanCb);  // IN: Channel callbacks

Bool
HSPU_ValidateDataPacketSize(HgfsPacket *packet,     // IN: Hgfs Packet
                            size_t dataSize);       // IN: data size

void *
HSPU_GetDataPacketBuf(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                      MappingType mappingType,              // IN: Readable/ Writeable ?
                      HgfsServerChannelCallbacks *chanCb);  // IN: Channel callbacks

void
HSPU_SetDataPacketSize(HgfsPacket *packet,            // IN/OUT: Hgfs Packet
                       size_t dataSize);              // IN: data size

void
HSPU_PutDataPacketBuf(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                      HgfsServerChannelCallbacks *chanCb);  // IN: Channel callbacks

void
HSPU_PutMetaPacket(HgfsPacket *packet,                   // IN/OUT: Hgfs Packet
                   HgfsServerChannelCallbacks *chanCb);  // IN: Channel callbacks

Bool
HSPU_ValidateRequestPacketSize(HgfsPacket *packet,        // IN: Hgfs Packet
                               size_t requestHeaderSize,  // IN: request header size
                               size_t requestOpSize,      // IN: request packet size
                               size_t requestOpDataSize); // IN: request packet data size

Bool
HSPU_ValidateReplyPacketSize(HgfsPacket *packet,         // IN: Hgfs Packet
                             size_t replyHeaderSize,     // IN: reply header size
                             size_t replyResultSize,     // IN: reply result size
                             size_t replyResultDataSize, // IN: reply result data size
                             Bool useMappedMetaPacket);    // IN: using meta buffer

void *
HSPU_GetReplyPacket(HgfsPacket *packet,                  // IN/OUT: Hgfs Packet
                    HgfsServerChannelCallbacks *chanCb,  // IN: Channel callbacks
                    size_t replyDataSize,                // IN: Size of reply data
                    size_t *replyPacketSize);            // OUT: Size of reply Packet

void
HSPU_PutReplyPacket(HgfsPacket *packet,                  // IN/OUT: Hgfs Packet
                    HgfsServerChannelCallbacks *chanCb); // IN: Channel callbacks
#endif /* __HGFS_SERVER_INT_H__ */
