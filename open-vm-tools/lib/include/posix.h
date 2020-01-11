/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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

#ifndef VMWARE_POSIX_H
#define VMWARE_POSIX_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <stdio.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <sys/types.h>
#include <dirent.h>
#include <netdb.h>
#endif

#include "vm_basic_types.h"
#include "unicodeTypes.h"
#include "unicodeBase.h"
#include "codeset.h"
#include "err.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Require the _FILE_OFFSET_BITS=64 interface, where all Posix file
 * structures and functions are transparently 64-bit.
 *
 * Some OSes (FreeBSD, OS X) natively use 64-bit file offsets and
 * will never be a problem. Other OSes (Linux, Solaris) default
 * to 32-bit offsets in 32-bit mode. Android ... does its own thing.
 * Windows only offers the Posix APIs as 32-bit.
 *
 * There are two ways of getting 64-bit offsets, defined by the LFS standard:
 *    _LARGEFILE64_SOURCE, which defines xxx64 types and functions
 *    _FILE_OFFSET_BITS=64, which replaces all types and functions
 * The _LARGEFILE64_SOURCE strategy was transitional (late 90s) and is now
 * deprecated. All modern code should use _FILE_OFFSET_BITS=64.
 *
 * Instead of checking for the exact details, check for what matters
 * for the purposes of this file: types (e.g. off_t) must be 64-bit.
 */
#if !defined(_WIN32) && !defined(__ANDROID__)
MY_ASSERTS(_file_offset_bits64, ASSERT_ON_COMPILE(sizeof(off_t) == 8);)
#endif

struct stat;

#if defined(_WIN32)
typedef int mode_t;
#else
struct statfs;
struct utimbuf;
struct timeval;
struct passwd;
#if !defined(sun)
struct mntent;
#else
struct mnttab;
#endif
#endif


int Posix_Creat(const char *pathName, mode_t mode);
int Posix_Open(const char *pathName, int flags, ...);
FILE *Posix_Fopen(const char *pathName, const char *mode);
FILE *Posix_Popen(const char *pathName, const char *mode);
int Posix_Rename(const char *fromPathName, const char *toPathName);
int Posix_Rmdir(const char *pathName);
int Posix_Unlink(const char *pathName);
FILE *Posix_Freopen(const char *pathName, const char *mode, FILE *stream);
int Posix_Access(const char *pathName, int mode);
int Posix_EuidAccess(const char *pathName, int mode);
int Posix_Stat(const char *pathName, struct stat *statbuf);
int Posix_Chmod(const char *pathName, mode_t mode);
void Posix_Perror(const char *str);
int Posix_Printf(const char *format, ...);
int Posix_Fprintf(FILE *stream, const char *format, ...);

int Posix_Mkdir(const char *pathName, mode_t mode);
int Posix_Chdir(const char *pathName);
char *Posix_Getenv(const char *name);
long Posix_Pathconf(const char *pathName, int name);
int Posix_Lstat(const char *pathName, struct stat *statbuf);
char *Posix_MkTemp(const char *pathName);


/*
 *-----------------------------------------------------------------------------
 *
 * Posix_Free --
 *
 *      Wrapper around free() that preserves errno.
 *
 *      C11 (and earlier) does not prohibit free() implementations from
 *      modifying errno.  That is undesirable since it can clobber errno along
 *      cleanup paths, and it is expected to be prohibited by a future (as of
 *      January 2017) version of the POSIX standard.  See:
 *      <http://stackoverflow.com/a/30571921/179715>
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#define Posix_Free(p) WITH_ERRNO_FREE(p)

#if !defined(_WIN32)
/*
 * These Windows APIs actually work with non-ASCII (MBCS) strings.
 * Make them NULL wrappers for all other platforms.
 */
#define Posix_GetHostName gethostname
#if defined(__APPLE__)
#define Posix_GetHostByName gethostbyname
#endif
#define Posix_GetAddrInfo getaddrinfo
#define Posix_FreeAddrInfo freeaddrinfo
#define Posix_GetNameInfo getnameinfo

void *Posix_Dlopen(const char *pathName, int flags);

int Posix_Utime(const char *pathName, const struct utimbuf *times);

int Posix_Mknod(const char *pathName, mode_t mode, dev_t dev);
int Posix_Chown(const char *pathName, uid_t owner, gid_t group);
int Posix_Lchown(const char *pathName, uid_t owner, gid_t group);
int Posix_Link(const char *oldPath, const char *newPath);
int Posix_Symlink(const char *oldPath, const char *newPath);
int Posix_Mkfifo(const char *pathName, mode_t mode);
int Posix_Truncate(const char *pathName, off_t length);
int Posix_Utimes(const char *pathName, const struct timeval *time);
int Posix_Execl(const char *pathName, const char *arg0, ...);
int Posix_Execlp(const char *fileName, const char *arg0, ...);
int Posix_Execv(const char *pathName, char *const argVal[]);
int Posix_Execve(const char *pathName, char *const argVal[],
                 char *const envPtr[]);
int Posix_Execvp(const char *fileName, char *const argVal[]);
DIR *Posix_OpenDir(const char *pathName);
int Posix_System(const char *command);
int Posix_Putenv(char *name);
int Posix_Unsetenv(const char *name);

/*
 * These functions return dynamically allocated stings that have to be
 * freed by the caller so they must be used in the ESX environment. They
 * are different than their POSIX "base" functions.
 */

char *Posix_RealPath(const char *pathName);
char *Posix_ReadLink(const char *pathName);

struct passwd *Posix_Getpwnam(const char *name);
struct passwd *Posix_Getpwuid(uid_t uid);

int Posix_Setenv(const char *name, const char *value, int overWrite);

int Posix_Getpwnam_r(const char *name, struct passwd *pw,
                     char *buf, size_t size, struct passwd **ppw);
int Posix_Getpwuid_r(uid_t uid, struct passwd *pw,
                     char *buf, size_t size, struct passwd **ppw);
struct passwd *Posix_Getpwent(void);
void Posix_Endpwent(void);
struct group *Posix_Getgrnam(const char *name);
int Posix_Getgrnam_r(const char *name, struct group *gr,
                 char *buf, size_t size, struct group **pgr);

#if !defined(sun)
int Posix_Statfs(const char *pathName, struct statfs *statfsbuf);

int Posix_GetGroupList(const char *user, gid_t group, gid_t *groups,
                       int *ngroups);

#if !defined(__APPLE__) && !defined(__FreeBSD__)
int Posix_Mount(const char *source, const char *target,
                const char *filesystemtype, unsigned long mountflags,
                const void *data);
int Posix_Umount(const char *target);
FILE *Posix_Setmntent(const char *pathName, const char *mode);
struct mntent *Posix_Getmntent(FILE *fp);
struct mntent *Posix_Getmntent_r(FILE *fp, struct mntent *m,
                                 char *buf, int size);

#endif // !defined(__APPLE__) && !defined(__FreeBSD__)
#else  // !defined(sun)
int Posix_Getmntent(FILE *fp, struct mnttab *mp);

#endif // !defined(sun)
#if !defined(__APPLE__)


/*
 *----------------------------------------------------------------------
 *
 * Posix_GetHostByName --
 *
 *      Wrapper for gethostbyname().  Caller should release memory
 *      allocated for the hostent structure returned by calling
 *      Posix_FreeHostent().
 *
 * Results:
 *      NULL    Error
 *      !NULL   Pointer to hostent structure
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE struct hostent*
Posix_GetHostByName(const char *name)  // IN
{
   struct hostent *newhostent;
   int error;
   struct hostent he;
   char buffer[1024];
   struct hostent *phe = &he;
   char **p;
   int i;
   int naddrs;

   ASSERT(name);

   if ((gethostbyname_r(name, &he, buffer, sizeof buffer,
#if !defined(sun) && !defined(SOLARIS) && !defined(SOL10)
                        &phe,
#endif
                        &error) == 0) && phe) {

      newhostent = (struct hostent *)Util_SafeMalloc(sizeof *newhostent);
      newhostent->h_name = Unicode_Alloc(phe->h_name,
                                         STRING_ENCODING_DEFAULT);
      if (phe->h_aliases) {
         newhostent->h_aliases = Unicode_AllocList(phe->h_aliases,
                                                   -1,
                                                   STRING_ENCODING_DEFAULT);
      } else {
         newhostent->h_aliases = NULL;
      }
      newhostent->h_addrtype = phe->h_addrtype;
      newhostent->h_length = phe->h_length;

      naddrs = 1;
      for (p = phe->h_addr_list; *p; p++) {
         naddrs++;
      }
      newhostent->h_addr_list = (char **)Util_SafeMalloc(naddrs *
                                 sizeof(*(phe->h_addr_list)));
      for (i = 0; i < naddrs - 1; i++) {
         newhostent->h_addr_list[i] = (char *)Util_SafeMalloc(phe->h_length);
         memcpy(newhostent->h_addr_list[i], phe->h_addr_list[i], phe->h_length);
      }
      newhostent->h_addr_list[naddrs - 1] = NULL;
      return newhostent;
   }
   /* There has been an error */
   return NULL;
}
#endif // !define(__APPLE__)


/*
 *----------------------------------------------------------------------
 *
 * Posix_FreeHostent --
 *
 *      Free the memory allocated for an hostent structure returned
 *      by Posix_GetHostByName.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
Posix_FreeHostent(struct hostent *he)
{
#if !defined(__APPLE__)
   char **p;

   if (he) {
      // See Posix_Free.
      int err = errno;
      free(he->h_name);
      if (he->h_aliases) {
         Util_FreeStringList(he->h_aliases, -1);
      }
      p = he->h_addr_list;
      while (*p) {
         free(*p++);
      }
      free(he->h_addr_list);
      free(he);
      errno = err;
   }
#else
   (void) he;
#endif
}

#else  // !define(_WIN32)

#if defined(_WINSOCKAPI_) || defined(_WINSOCK2API_)
#include <winbase.h>
#include "vm_atomic.h"
#if defined(VM_WIN_UWP)
/* UWP use the network definition in winsock2.h */
#include <winsock2.h>
#endif

/*
 *----------------------------------------------------------------------
 *
 * Posix_GetHostName --
 *
 *      Wrapper for gethostname().
 *
 * Results:
 *      0    Success
 *      -1   Error
 *
 * Side effects:
 *      On error, error code returned by WSAGetLastError() is updated.
 *
 *----------------------------------------------------------------------
 */

static INLINE int
Posix_GetHostName(char *name,   // OUT
                  int namelen)  // IN
{
   char *nameMBCS = (char *)Util_SafeMalloc(namelen);
   char *nameUTF8;
   int retval;

   ASSERT(name);

   retval = gethostname(nameMBCS, namelen);

   if (retval == 0) {
      nameUTF8 = Unicode_Alloc(nameMBCS, STRING_ENCODING_DEFAULT);
      if (!Unicode_CopyBytes(name, nameUTF8, namelen, NULL,
                             STRING_ENCODING_UTF8)) {
         retval = -1;
         WSASetLastError(WSAEFAULT);
      }
      Posix_Free(nameUTF8);
   }

   Posix_Free(nameMBCS);

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_GetHostByName --
 *
 *      Wrapper for gethostbyname().  Caller should release memory
 *      allocated for the hostent structure returned by calling
 *      Posix_FreeHostent().
 *
 * Results:
 *      NULL    Error
 *      !NULL   Pointer to hostent structure
 *
 * Side effects:
 *      On error, error code returned by WSAGetLastError() is updated.
 *
 *----------------------------------------------------------------------
 */

static INLINE struct hostent*
Posix_GetHostByName(const char *name)  // IN
{
   struct hostent *newhostent;
   char *nameMBCS;
   struct hostent *hostentMBCS;
   struct hostent *ret = NULL;

   ASSERT(name);

   nameMBCS = (char *)Unicode_GetAllocBytes(name, STRING_ENCODING_DEFAULT);

   if (nameMBCS != NULL) {
      hostentMBCS = gethostbyname(nameMBCS);
      Posix_Free(nameMBCS);

      if (hostentMBCS != NULL) {
         newhostent = (struct hostent *)Util_SafeMalloc(sizeof *newhostent);

         newhostent->h_name = Unicode_Alloc(hostentMBCS->h_name,
                                            STRING_ENCODING_DEFAULT);
         if (hostentMBCS->h_aliases) {
            newhostent->h_aliases = Unicode_AllocList(hostentMBCS->h_aliases,
                                                      -1,
                                                      STRING_ENCODING_DEFAULT);
         } else {
            newhostent->h_aliases = NULL;
         }
         newhostent->h_addrtype = hostentMBCS->h_addrtype;
         newhostent->h_length = hostentMBCS->h_length;
         newhostent->h_addr_list = hostentMBCS->h_addr_list;
         ret = newhostent;
      }
   } else {
      /* There has been an error converting from UTF-8 to local encoding. */
      WSASetLastError(WSANO_RECOVERY);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_FreeHostent --
 *
 *      Free the memory allocated for an hostent structure returned
 *      by Posix_GetHostByName.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
Posix_FreeHostent(struct hostent *he)
{
   if (he) {
      Posix_Free(he->h_name);
      if (he->h_aliases) {
         Util_FreeStringList(he->h_aliases, -1);
      }
      Posix_Free(he);
   }
}
#endif  // defined(_WINSOCKAPI_) || defined(_WINSOCK2API_)

#ifdef _WS2TCPIP_H_
/*
 *----------------------------------------------------------------------
 *
 * Posix_GetAddrInfo --
 *
 *      Wrapper for getaddrinfo().
 *
 *      Inlined to match Ws2tcpip.h inclusion.
 *
 * Results:
 *      0       Success
 *      != 0    Error
 *
 * Side effects:
 *      On error, error code returned by WSAGetLastError() is updated.
 *
 *----------------------------------------------------------------------
 */

static INLINE int
Posix_GetAddrInfo(const char *nodename,         // IN
                  const char *servname,         // IN
                  const struct addrinfo *hints, // IN
                  struct addrinfo **res)        // OUT
{
   int retval;
   struct addrinfoW *resW;
   utf16_t *nodenameW = Unicode_GetAllocUTF16(nodename);
   utf16_t *servnameW = Unicode_GetAllocUTF16(servname);

   ASSERT(nodename || servname);
   ASSERT(res);

   /*
    * The string conversion required is between UTF-8 and UTF-16 encodings.
    * Note that struct addrinfo and ADDRINFOW are identical except for the
    * fields ai_canonname (char * vs. PWSTR) and ai_next (obviously),
    * and those fields must be NULL, so hints can be cast to UTF-16.
    */

   retval = GetAddrInfoW(nodenameW, servnameW, (struct addrinfoW *)hints,
                         &resW);

   if (retval == 0) {
      struct addrinfoW *cur;
      struct addrinfo **pres = res;

      for (cur = resW; cur != NULL; cur = cur->ai_next) {
         *pres = (struct addrinfo *)Util_SafeMalloc(sizeof **pres);
         (*pres)->ai_flags = cur->ai_flags;
         (*pres)->ai_family = cur->ai_family;
         (*pres)->ai_socktype = cur->ai_socktype;
         (*pres)->ai_protocol = cur->ai_protocol;
         (*pres)->ai_addrlen = cur->ai_addrlen;
         if (cur->ai_canonname) {
            (*pres)->ai_canonname = Unicode_AllocWithUTF16(cur->ai_canonname);
         } else {
            (*pres)->ai_canonname = NULL;
         }
         (*pres)->ai_addr = (struct sockaddr *)
                            Util_SafeMalloc((*pres)->ai_addrlen);
         memcpy((*pres)->ai_addr, cur->ai_addr, (*pres)->ai_addrlen);
         pres = &((*pres)->ai_next);
      }
      *pres = NULL;
      FreeAddrInfoW(resW);
   }

   Posix_Free(nodenameW);
   Posix_Free(servnameW);

   return retval;
}


/*
 *----------------------------------------------------------------------------
 *
 * Posix_FreeAddrInfo --
 *
 *      Free the addrinfo structure allocated by Posix_GetAddrInfo.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
Posix_FreeAddrInfo(struct addrinfo *ai)
{
   struct addrinfo *temp;

   // See Posix_Free.
   int err = errno;
   while (ai) {
      temp = ai;
      ai = ai->ai_next;
      free(temp->ai_canonname);
      free(temp->ai_addr);
      free(temp);
   }
   errno = err;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_GetNameInfo --
 *
 *      Wrapper for getnameinfo().
 *
 *      Inlined to match Ws2tcpip.h inclusion.
 *
 * Results:
 *      0       Success
 *      != 0    Error
 *
 * Side effects:
 *      On error, error code returned by WSAGetLastError() is updated.
 *
 *----------------------------------------------------------------------
 */

static INLINE int
Posix_GetNameInfo(const struct sockaddr *sa,  // IN
                  socklen_t salen,            // IN
                  char *host,                 // OUT
                  DWORD hostlen,              // IN
                  char *serv,                 // OUT
                  DWORD servlen,              // IN
                  int flags)                  // IN
{
   int retval;
   utf16_t *hostW = NULL;
   utf16_t *servW = NULL;
   char *hostUTF8 = NULL;
   char *servUTF8 = NULL;

   if (host) {
      hostW = (utf16_t *)Util_SafeMalloc(hostlen * sizeof *hostW);
   }
   if (serv) {
      servW = (utf16_t *)Util_SafeMalloc(servlen * sizeof *servW);
   }

   retval = GetNameInfoW(sa, salen, hostW, hostlen, servW,
                         servlen, flags);

   if (retval == 0) {
      if (host) {
         hostUTF8 = Unicode_AllocWithUTF16(hostW);

         if (!Unicode_CopyBytes(host, hostUTF8, hostlen, NULL,
                                STRING_ENCODING_UTF8)) {
            retval = EAI_MEMORY;
            WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
            goto exit;
         }
      }
      if (serv) {
         servUTF8 = Unicode_AllocWithUTF16(servW);

         if (!Unicode_CopyBytes(serv, servUTF8, servlen, NULL,
                                STRING_ENCODING_UTF8)) {
            retval = EAI_MEMORY;
            WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
            goto exit;
         }
      }
   }

exit:
   Posix_Free(hostW);
   Posix_Free(servW);
   Posix_Free(hostUTF8);
   Posix_Free(servUTF8);

   return retval;
}
#endif // ifdef _WS2TCPIP_H_
#endif // !define(_WIN32)

#if (defined(VMX86_SERVER) || defined(__APPLE__)) && \
   !defined(UNICODE_BUILDING_POSIX_WRAPPERS)
/*
 * ESX and Mac OS are UTF-8 environments so these functions can be
 * "defined away" - the POSIX wrapper call can be directly mapped to the
 * POSIX function avoiding unneccesary (call and handling) overhead.
 *
 * NOTE: PLEASE KEEP THESE IN SORTED ORDER
 */
#define Posix_Access access
#define Posix_Chdir chdir
#define Posix_Chmod chmod
#define Posix_Chown chown
#define Posix_Creat creat
#define Posix_Dlopen dlopen
#define Posix_Execl execl
#define Posix_Execlp execlp
#define Posix_Execv execv
#define Posix_Execve execve
#define Posix_Execvp execvp
#define Posix_Fopen fopen
#define Posix_Fprintf fprintf
#define Posix_Freopen freopen
#define Posix_Getenv getenv
#define Posix_GetGroupList getgrouplist
#define Posix_Getmntent getmntent
#define Posix_Getmntent_r getmntent_r
#define Posix_Getpwnam getpwnam
#define Posix_Getpwnam_r getpwnam_r
#define Posix_Getpwuid getpwuid
#define Posix_Getpwuid_r getpwuid_r
#define Posix_Getgrnam getgrnam
#define Posix_Getgrnam_r getgrnam_r
#define Posix_Lchown lchown
#define Posix_Link link
#define Posix_Lstat lstat
#define Posix_Mkdir mkdir
#define Posix_Mkfifo mkfifo
#define Posix_Mknod mknod
#define Posix_Mount mount
#define Posix_Open open
#define Posix_OpenDir opendir
#define Posix_Pathconf pathconf
#define Posix_Perror perror
#define Posix_Popen popen
#define Posix_Printf printf
#define Posix_Putenv putenv
#define Posix_Rename rename
#define Posix_Rmdir rmdir
#define Posix_Setenv setenv
#define Posix_Setmntent setmntent
#define Posix_Stat stat
#define Posix_Statfs statfs
#define Posix_Symlink symlink
#define Posix_System system
#define Posix_Truncate truncate
#define Posix_Umount umount
#define Posix_Unlink unlink
#define Posix_Unsetenv unsetenv
#define Posix_Utime utime
#define Posix_Utimes utimes
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // VMWARE_POSIX_H
