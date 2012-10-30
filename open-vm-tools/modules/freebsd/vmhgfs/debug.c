/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * debug.c --
 *
 *	Routine(s) for debugging the FreeBSD / Mac OS Hgfs module.
 */


#if defined __APPLE__
#include <sys/proc.h>         // for proc_selfpid/name
#include <string.h>
#include "kernelStubs.h"
#endif // defined __APPLE__
#include "vm_basic_types.h"
#include "debug.h"

/*
 * Global functions
 */
static const char *gHgfsOperationNames[] = {
   "HGFS_OP_OPEN",
   "HGFS_OP_READ",
   "HGFS_OP_WRITE",
   "HGFS_OP_CLOSE",
   "HGFS_OP_SEARCH_OPEN",
   "HGFS_OP_SEARCH_READ",
   "HGFS_OP_SEARCH_CLOSE",
   "HGFS_OP_GETATTR",
   "HGFS_OP_SETATTR",
   "HGFS_OP_CREATE_DIR",
   "HGFS_OP_DELETE_FILE",
   "HGFS_OP_DELETE_DIR",
   "HGFS_OP_RENAME",
   "HGFS_OP_QUERY_VOLUME_INFO",
   "HGFS_OP_OPEN_V2",
   "HGFS_OP_GETATTR_V2",
   "HGFS_OP_SETATTR_V2",
   "HGFS_OP_SEARCH_READ_V2",
   "HGFS_OP_CREATE_SYMLINK",
   "HGFS_OP_SERVER_LOCK_CHANGE",
   "HGFS_OP_CREATE_DIR_V2",
   "HGFS_OP_DELETE_FILE_V2",
   "HGFS_OP_DELETE_DIR_V2",
   "HGFS_OP_RENAME_V2",
   "HGFS_OP_OPEN_V3",
   "HGFS_OP_READ_V3",
   "HGFS_OP_WRITE_V3",
   "HGFS_OP_CLOSE_V3",
   "HGFS_OP_SEARCH_OPEN_V3",
   "HGFS_OP_SEARCH_READ_V3",
   "HGFS_OP_SEARCH_CLOSE_V3",
   "HGFS_OP_GETATTR_V3",
   "HGFS_OP_SETATTR_V3",
   "HGFS_OP_CREATE_DIR_V3",
   "HGFS_OP_DELETE_FILE_V3",
   "HGFS_OP_DELETE_DIR_V3",
   "HGFS_OP_RENAME_V3",
   "HGFS_OP_QUERY_VOLUME_INFO_V3",
   "HGFS_OP_CREATE_SYMLINK_V3",
   "HGFS_OP_SERVER_LOCK_CHANGE_V3",
   "HGFS_OP_WRITE_WIN32_STREAM_V3",
   "HGFS_OP_CREATE_SESSION_V4",
   "HGFS_OP_DESTROY_SESSION_V4",
   "HGFS_OP_READ_FAST_V4",
   "HGFS_OP_WRITE_FAST_V4",
   "HGFS_OP_SET_WATCH_V4",
   "HGFS_OP_REMOVE_WATCH_V4",
   "HGFS_OP_NOTIFY_V4",
   "HGFS_OP_SEARCH_READ_V4",
};

#if defined VMX86_DEVEL
static uint32 HgfsDebugGetSequenceNumber(void);
static int HgfsDebugGetProcessInfo(char *pidName, size_t pidNameBufsize);
static void *HgfsDebugGetCurrentThread(void);
#endif // defined VMX86_DEVEL


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrint --
 *
 *      Prints the operation of an request structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDebugPrint(int type, const char *funcname, unsigned int linenum, const char *fmt, ...)
{
#if defined VMX86_DEVEL
#if defined __APPLE__
   if (0 != (type & VM_DEBUG_LEV) ||
       VM_DEBUG_ALWAYS == type) {
      char *fmsg;
      size_t fmsgLen;
      va_list args;

      va_start(args, fmt);
      fmsg = Str_Vasprintf(&fmsgLen, fmt, args);
      va_end(args);

      if (NULL != fmsg) {
         int pid;
         void *thrd;
         char pidname[64];
         uint32 seqNo;

         thrd = HgfsDebugGetCurrentThread();
         pid = HgfsDebugGetProcessInfo(pidname, sizeof pidname);
         seqNo = HgfsDebugGetSequenceNumber();

         kprintf("|%08u|%p.%08d.%s| %s:%2.2u: %s",
                 seqNo, thrd, pid, pidname, funcname, linenum, fmsg);

         free(fmsg);
      }
   }
#endif // defined __APPLE__
#endif // defined VMX86_DEVEL
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintOperation --
 *
 *      Prints the operation of an request structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDebugPrintOperation(HgfsKReqHandle req)
{
   HgfsRequest *requestHeader;

   ASSERT(NULL != req);

   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);

   if (requestHeader->op < ARRAYSIZE(gHgfsOperationNames)) {
      DEBUG(VM_DEBUG_STRUCT, " operation: %s\n", gHgfsOperationNames[requestHeader->op]);
   } else {
      DEBUG(VM_DEBUG_STRUCT, " operation: INVALID %d\n", requestHeader->op);
   }
}


#if defined VMX86_DEVEL
/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugGetProcessInfo --
 *
 *      Gets the process name and ID making a request.
 *
 * Results:
 *      PID of current process, and name in the buffer.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDebugGetProcessInfo(char *pidname,          // OUT: buffer for name
                        size_t pidNameBufsize)  // IN: size of buffer
{
   int curPid = -1;
   *pidname = '\0';
#if defined __APPLE__
   curPid = proc_selfpid();
   proc_name(curPid, pidname, pidNameBufsize);
#endif // defined __APPLE__
   return curPid;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugGetCurrentThreadId --
 *
 *      Gets the current thread making a request.
 *
 * Results:
 *      TID of current thread.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void *
HgfsDebugGetCurrentThread(void)
{
   void *thread = NULL;
#if defined __APPLE__
   thread = current_thread();
#endif // defined __APPLE__
   return thread;
}
#endif // defined VMX86_DEVEL


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintVattr --
 *
 *      Prints the contents of an attributes structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDebugPrintVattr(const HgfsVnodeAttr *vap)
{
   DEBUG(VM_DEBUG_STRUCT, " va_type: %d\n", vap->va_type);
   DEBUG(VM_DEBUG_STRUCT, " va_mode: %o\n", vap->va_mode);
   DEBUG(VM_DEBUG_STRUCT, " va_uid:  %u\n", vap->va_uid);
   DEBUG(VM_DEBUG_STRUCT, " va_gid: %u\n", vap->va_gid);
   DEBUG(VM_DEBUG_STRUCT, " va_fsid: %u\n", vap->va_fsid);
   DEBUG(VM_DEBUG_STRUCT, " va_rdev: %u\n", vap->va_rdev);
   DEBUG(VM_DEBUG_STRUCT, " va_filerev: %"FMT64"u\n", vap->va_filerev);
   DEBUG(VM_DEBUG_STRUCT, " va_vaflags: %x\n", vap->va_vaflags);

#if defined __FreeBSD__
   /*
    * The next group of attributes have the same name but different sizes on
    * xnu-1228 and FreeBSD 6.2.
    */
   DEBUG(VM_DEBUG_STRUCT, " va_flags: %lx\n", vap->va_flags);
   DEBUG(VM_DEBUG_STRUCT, " va_gen: %lu\n", vap->va_gen);
   DEBUG(VM_DEBUG_STRUCT, " va_fileid: %ld\n", vap->va_fileid);
   DEBUG(VM_DEBUG_STRUCT, " va_nlink: %hd\n", vap->va_nlink);

   /* These attributes names changed have between xnu-1228 and FreeBSD 6.2. */
   DEBUG(VM_DEBUG_STRUCT, " va_size: %ju\n", vap->va_size);
   DEBUG(VM_DEBUG_STRUCT, " va_blocksize: %ld\n", vap->va_blocksize);
   /*
    * XXX time_t is __int32_t on 32-bit architectures and __int64_t on 64-bit
    * architectures.  Would this be better as add'l formats in vm_basic_types.h?
    */
   DEBUG(VM_DEBUG_STRUCT, " va_atime.tv_sec: %jd\n", (intmax_t)vap->va_atime.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_atime.tv_nsec: %ld\n", vap->va_atime.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_mtime.tv_sec: %jd\n", (intmax_t)vap->va_mtime.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_mtime.tv_nsec: %ld\n", vap->va_mtime.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_ctime.tv_sec: %jd\n", (intmax_t)vap->va_ctime.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_ctime.tv_nsec: %ld\n", vap->va_ctime.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_birthtime.tv_sec: %jd\n",
         (intmax_t)vap->va_birthtime.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_birthtime.tv_nsec: %ld\n",
         vap->va_birthtime.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_bytes: %"FMT64"u\n", vap->va_bytes);

#elif defined __APPLE__
   /*
    * The next group of attributes have the same name but different sizes on
    * xnu-1228 and FreeBSD 6.2.
    */
   DEBUG(VM_DEBUG_STRUCT, " va_flags: %x\n", vap->va_flags);
   DEBUG(VM_DEBUG_STRUCT, " va_gen: %u\n", vap->va_gen);
   DEBUG(VM_DEBUG_STRUCT, " va_fileid: %"FMT64"u\n", vap->va_fileid);
   DEBUG(VM_DEBUG_STRUCT, " va_nlink: %"FMT64"u\n", vap->va_nlink);

   /* These attribute names have changed between xnu-1228 and FreeBSD 6.2. */
   DEBUG(VM_DEBUG_STRUCT, " va_size: %"FMT64"u\n", vap->va_data_size);
   DEBUG(VM_DEBUG_STRUCT, " va_iosize: %u\n", vap->va_iosize);

   DEBUG(VM_DEBUG_STRUCT, " va_access_time.tv_sec: %ld\n", vap->va_access_time.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_access_time.tv_nsec: %ld\n", vap->va_access_time.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_modify_time.tv_sec: %ld\n", vap->va_modify_time.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_modify_time.tv_nsec: %ld\n", vap->va_modify_time.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_create_time.tv_sec: %ld\n", vap->va_create_time.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_create_time.tv_nsec: %ld\n", vap->va_create_time.tv_nsec);
#endif
}


#if defined VMX86_DEVEL
/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugGetSequenceNumber --
 *
 *      Log a sequence number in case we suspect log messages getting dropped
 *
 * Results:
 *      The next sequence number.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static uint32
HgfsDebugGetSequenceNumber(void)
{
   static uint32 ghgfsDebugLogSeq = 0;
   return ++ghgfsDebugLogSeq;
}
#endif // defined VMX86_DEVEL
