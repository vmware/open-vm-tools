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


#include "vm_basic_types.h"
#include "debug.h"

/*
 * Global functions
 */


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

#if defined(__FreeBSD__)
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

#elif defined(__APPLE__)
   /*
    * The next group of attributes have the same name but different sizes on
    * xnu-1228 and FreeBSD 6.2.
    */
   DEBUG(VM_DEBUG_STRUCT, " va_flags: %x\n", vap->va_flags);
   DEBUG(VM_DEBUG_STRUCT, " va_gen: %u\n", vap->va_gen);
   DEBUG(VM_DEBUG_STRUCT, " va_fileid: %"FMT64"u\n", vap->va_fileid);
   DEBUG(VM_DEBUG_STRUCT, " va_nlink: %"FMT64"u\n", vap->va_nlink);

   /* These attribute names have changed between xnu-1228 and FreeBSD 6.2. */
   DEBUG(VM_DEBUG_STRUCT, " va_size: %ju\n", vap->va_data_size);
   DEBUG(VM_DEBUG_STRUCT, " va_iosize: %u\n", vap->va_iosize);
   /*
    * XXX time_t is __int32_t on 32-bit architectures and __int64_t on 64-bit
    * architectures.  Would this be better as add'l formats in vm_basic_types.h?
    */
   DEBUG(VM_DEBUG_STRUCT, " va_access_time.tv_sec: %jd\n", (intmax_t)vap->va_access_time.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_access_time.tv_nsec: %ld\n", vap->va_access_time.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_modify_time.tv_sec: %jd\n", (intmax_t)vap->va_modify_time.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_modify_time.tv_nsec: %ld\n", vap->va_modify_time.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_create_time.tv_sec: %jd\n", (intmax_t)vap->va_create_time.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_create_time.tv_nsec: %ld\n", vap->va_create_time.tv_nsec);
#endif
}
