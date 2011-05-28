/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef _POSIX_H_
#define _POSIX_H_

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

/*
 * Force all users of these wrappers to use the LFS (large file) interface
 * versions of the functions wrapper therein. If we don't do this the
 * wrappers may be built with the LFS versions and the callers might not
 * leading to a (potentially undetected) mismatch.
 */

#if defined(linux) && \
    (!defined(_LARGEFILE64_SOURCE) || _FILE_OFFSET_BITS != 64)
#error LFS support is not enabled!
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


int Posix_Creat(ConstUnicode pathName, mode_t mode);
int Posix_Open(ConstUnicode pathName, int flags, ...);
FILE *Posix_Fopen(ConstUnicode pathName, const char *mode);
FILE *Posix_Popen(ConstUnicode pathName, const char *mode);
int Posix_Rename(ConstUnicode fromPathName, ConstUnicode toPathName);
int Posix_Rmdir(ConstUnicode pathName);
int Posix_Unlink(ConstUnicode pathName);
FILE *Posix_Freopen(ConstUnicode pathName, const char *mode, FILE *stream);
int Posix_Access(ConstUnicode pathName, int mode);
int Posix_EuidAccess(ConstUnicode pathName, int mode);
int Posix_Stat(ConstUnicode pathName, struct stat *statbuf);
int Posix_Chmod(ConstUnicode pathName, mode_t mode);
void Posix_Perror(ConstUnicode str);
int Posix_Printf(ConstUnicode format, ...);
int Posix_Fprintf(FILE *stream, ConstUnicode format, ...);

int Posix_Mkdir(ConstUnicode pathName, mode_t mode);
int Posix_Chdir(ConstUnicode pathName);
Unicode Posix_Getenv(ConstUnicode name);
long Posix_Pathconf(ConstUnicode pathName, int name);
int Posix_Lstat(ConstUnicode pathName, struct stat *statbuf);

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

void *Posix_Dlopen(ConstUnicode pathName, int flags);

int Posix_Utime(ConstUnicode pathName, const struct utimbuf *times);

int Posix_Mknod(ConstUnicode pathName, mode_t mode, dev_t dev);
int Posix_Chown(ConstUnicode pathName, uid_t owner, gid_t group);
int Posix_Lchown(ConstUnicode pathName, uid_t owner, gid_t group);
int Posix_Link(ConstUnicode pathName1, ConstUnicode pathName2);
int Posix_Symlink(ConstUnicode pathName1, ConstUnicode pathName2);
int Posix_Mkfifo(ConstUnicode pathName, mode_t mode);
int Posix_Truncate(ConstUnicode pathName, off_t length);
int Posix_Utimes(ConstUnicode pathName, const struct timeval *time);
int Posix_Execl(ConstUnicode pathName, ConstUnicode arg0, ...);
int Posix_Execlp(ConstUnicode fileName, ConstUnicode arg0, ...);
int Posix_Execv(ConstUnicode pathName, Unicode const argVal[]);
int Posix_Execve(ConstUnicode pathName, Unicode const argVal[], 
                 Unicode const envPtr[]);
int Posix_Execvp(ConstUnicode fileName, Unicode const argVal[]);
DIR *Posix_OpenDir(ConstUnicode pathName);
int Posix_System(ConstUnicode command);
int Posix_Putenv(Unicode name);
void Posix_Unsetenv(ConstUnicode name);

/*
 * These functions return dynamically allocated stings that have to be
 * freed by the caller so they must be used in the ESX environment. They
 * are different than their POSIX "base" functions.
 */

Unicode Posix_RealPath(ConstUnicode pathName);
Unicode Posix_ReadLink(ConstUnicode pathName);

struct passwd *Posix_Getpwnam(ConstUnicode name);
struct passwd *Posix_Getpwuid(uid_t uid);

int Posix_Setenv(ConstUnicode name, ConstUnicode value, int overWrite);

int Posix_Getpwnam_r(ConstUnicode name, struct passwd *pw,
                     char *buf, size_t size, struct passwd **ppw);
int Posix_Getpwuid_r(uid_t uid, struct passwd *pw,
                     char *buf, size_t size, struct passwd **ppw);
struct passwd *Posix_Getpwent(void);
struct group *Posix_Getgrnam(ConstUnicode name);
int Posix_Getgrnam_r(ConstUnicode name, struct group *gr,
                 char *buf, size_t size, struct group **pgr);

#if !defined(sun)
int Posix_Statfs(ConstUnicode pathName, struct statfs *statfsbuf);

int Posix_GetGroupList(ConstUnicode user, gid_t group, gid_t *groups,
                       int *ngroups);

#if !defined(__APPLE__) && !defined(__FreeBSD__)
int Posix_Mount(ConstUnicode source, ConstUnicode target,
                const char *filesystemtype, unsigned long mountflags,
		const void *data);
int Posix_Umount(ConstUnicode target);
FILE *Posix_Setmntent(ConstUnicode pathName, const char *mode);
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
Posix_GetHostByName(ConstUnicode name)  // IN
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
      Unicode_Free(he->h_name);
      if (he->h_aliases) {
         Unicode_FreeList(he->h_aliases, -1);
      }
      p = he->h_addr_list;
      while (*p) {
         free(*p++);
      }
      free(he->h_addr_list);
      free(he);
   }
#endif
}

#else  // !define(_WIN32)

#if defined(_WINSOCKAPI_) || defined(_WINSOCK2API_)
#include <winbase.h>
#include "vm_atomic.h"


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
Posix_GetHostName(Unicode name, // OUT
                  int namelen)  // IN
{
   char *nameMBCS = (char *)Util_SafeMalloc(namelen);
   Unicode nameUTF8;
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
      Unicode_Free(nameUTF8);
   }

   free(nameMBCS);
   
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
Posix_GetHostByName(ConstUnicode name)  // IN
{
   struct hostent *newhostent;
   char *nameMBCS;
   struct hostent *hostentMBCS;
   struct hostent *ret = NULL;

   ASSERT(name);

   nameMBCS = (char *)Unicode_GetAllocBytes(name, STRING_ENCODING_DEFAULT);

   if (nameMBCS != NULL) {
      hostentMBCS = gethostbyname(nameMBCS);
      free(nameMBCS);

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
      Unicode_Free(he->h_name);
      if (he->h_aliases) {
         Unicode_FreeList(he->h_aliases, -1);
      }
      free(he);
   }
}
#endif  // defined(_WINSOCKAPI_) || defined(_WINSOCK2API_)

#ifdef _WS2TCPIP_H_
typedef int (WINAPI *GetAddrInfoWFnType)(PCWSTR pNodeName, PCWSTR pServiceName,
                                         const struct addrinfo *pHints,
                                         struct addrinfoW **ppResult);
typedef void (WINAPI *FreeAddrInfoWFnType)(struct addrinfoW *ai);
typedef int (WINAPI *GetNameInfoWFnType)(const SOCKADDR *pSockaddr,
                                         socklen_t SockAddrLength,
                                	 PWCHAR pNodeBuffer,
					 DWORD NodeBufferSize,
                                	 PWCHAR pServiceBuffer,
					 DWORD ServiceBufferSize,
                                	 INT flags);


/*
 *----------------------------------------------------------------------
 *
 * Posix_GetAddrInfo --
 *
 *      Wrapper for getaddrinfo().
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
Posix_GetAddrInfo(ConstUnicode nodename,        // IN
                  ConstUnicode servname,        // IN
                  const struct addrinfo *hints, // IN
                  struct addrinfo **res)        // OUT
{
   HMODULE hWs2_32 = GetModuleHandleW(L"ws2_32");
   GetAddrInfoWFnType GetAddrInfoWFn = NULL;
   FreeAddrInfoWFnType FreeAddrInfoWFn = NULL;
   int retval;
   char *nodenameMBCS;
   char *servnameMBCS;
   struct addrinfo *resA;

   ASSERT(nodename || servname);
   ASSERT(res);
   ASSERT(hWs2_32);

   if (hWs2_32) {
      GetAddrInfoWFn = (GetAddrInfoWFnType)GetProcAddress(hWs2_32,
                                                          "GetAddrInfoW");
      FreeAddrInfoWFn = (FreeAddrInfoWFnType)GetProcAddress(hWs2_32,
                                                            "FreeAddrInfoW");
   }

   /*
    * If the unicode version of getaddrinfo exists, use it.  The string
    * conversion required is between UTF-8 and UTF-16 encodings.  Note
    * that struct addrinfo and ADDRINFOW are identical except for the
    * fields ai_canonname (char * vs. PWSTR) and ai_next (obviously).
    */

   if (GetAddrInfoWFn && FreeAddrInfoWFn) {
      utf16_t *nodenameW = Unicode_GetAllocUTF16(nodename);
      utf16_t *servnameW = Unicode_GetAllocUTF16(servname);
      struct addrinfoW *resW;

      retval = (*GetAddrInfoWFn)(nodenameW, servnameW, hints, &resW);

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
         FreeAddrInfoWFn(resW);
      }

      free(nodenameW);
      free(servnameW);

      goto exit;
   }

   /*
    * We did not find the unicode version of getaddrinfo, so we need to
    * convert strings to and from the local encoding.
    */

   nodenameMBCS = (char *)Unicode_GetAllocBytes(nodename,
                                                STRING_ENCODING_DEFAULT);
   servnameMBCS = (char *)Unicode_GetAllocBytes(servname,
                                                STRING_ENCODING_DEFAULT);

   retval = getaddrinfo(nodenameMBCS, servnameMBCS, hints, &resA);

   if (retval == 0) {
      struct addrinfo **pres = res;
      struct addrinfo *cur;

      for (cur = resA; cur != NULL; cur = cur->ai_next) {
         *pres = (struct addrinfo *)Util_SafeMalloc(sizeof **pres);
         (*pres)->ai_flags = cur->ai_flags;
         (*pres)->ai_family = cur->ai_family;
         (*pres)->ai_socktype = cur->ai_socktype;
         (*pres)->ai_protocol = cur->ai_protocol;
         (*pres)->ai_addrlen = cur->ai_addrlen;
         if (cur->ai_canonname) {
            (*pres)->ai_canonname = Unicode_Alloc(cur->ai_canonname,
                                                  STRING_ENCODING_DEFAULT);
         } else {
            (*pres)->ai_canonname = NULL;
         }
         (*pres)->ai_addr = (struct sockaddr *)
                            Util_SafeMalloc((*pres)->ai_addrlen);
         memcpy((*pres)->ai_addr, cur->ai_addr, (*pres)->ai_addrlen);
         pres = &((*pres)->ai_next);
      }
      *pres = NULL;
      freeaddrinfo(resA);
   }

   free(nodenameMBCS);
   free(servnameMBCS);

exit:
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

   while (ai) {
      temp = ai;
      ai = ai->ai_next;
      free(temp->ai_canonname);
      free(temp->ai_addr);
      free(temp);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_GetNameInfo --
 *
 *      Wrapper for getnameinfo().
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
Posix_GetNameInfo(const struct sockaddr *sa,    // IN
                  socklen_t salen,              // IN
                  Unicode host,                 // OUT
                  DWORD hostlen,                // IN
                  Unicode serv,                 // OUT
                  DWORD servlen,                // IN
                  int flags)                    // IN
{
   HMODULE hWs2_32;
   int retval;
   char *hostMBCS = NULL;
   char *servMBCS = NULL;
   utf16_t *hostW = NULL;
   utf16_t *servW = NULL;
   Unicode hostUTF8 = NULL;
   Unicode servUTF8 = NULL;
   GetNameInfoWFnType GetNameInfoWFn;

   hWs2_32 = LoadLibraryW(L"ws2_32");

   if (hWs2_32) {
      /*
       * If the unicode version of getnameinfo exists, use it.  The string
       * conversion required is between UTF-8 and UTF-16 encodings.
       */

      GetNameInfoWFn = (GetNameInfoWFnType)GetProcAddress(hWs2_32,
                                                          "GetNameInfoW");

      if (GetNameInfoWFn) {
         if (host) {
            hostW = (utf16_t *)Util_SafeMalloc(hostlen * sizeof *hostW);
         }
         if (serv) {
            servW = (utf16_t *)Util_SafeMalloc(servlen * sizeof *servW);
         }

         retval = (*GetNameInfoWFn)(sa, salen, hostW, hostlen, servW,
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
         
         goto exit;
      }
   }

   /*
    * We did not find the unicode version of getnameinfo, so we need to
    * convert strings to and from the local encoding.
    */

   if (host) {
      hostMBCS = (char *)Util_SafeMalloc(hostlen * sizeof *hostMBCS);
   }
   if (serv) {
      servMBCS = (char *)Util_SafeMalloc(servlen * sizeof *servMBCS);
   }

   retval = getnameinfo(sa, salen, hostMBCS, hostlen, servMBCS, servlen,
                        flags);

   if (retval == 0) {
      if (host) {
         hostUTF8 = Unicode_Alloc(hostMBCS, STRING_ENCODING_DEFAULT);

         if (!Unicode_CopyBytes(host, hostUTF8, hostlen, NULL,
                                STRING_ENCODING_UTF8)) {
            retval = EAI_MEMORY;
            WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
            goto exit;
         }
      }
      if (serv) {
         servUTF8 = Unicode_Alloc(servMBCS, STRING_ENCODING_DEFAULT);

         if (!Unicode_CopyBytes(serv, servUTF8, servlen, NULL,
                                STRING_ENCODING_UTF8)) {
            retval = EAI_MEMORY;
            WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
            goto exit;
         }
      }
   }

exit:
   if (hWs2_32) {
      FreeLibrary(hWs2_32);
      free(hostW);
      free(servW);
   }
   free(hostMBCS);
   free(servMBCS);
   free(hostUTF8);
   free(servUTF8);

   return retval;
}
#endif // ifdef _WS2TCPIP_H_
#endif // !define(_WIN32)

#if (defined(VMX86_SERVER) || defined(__APPLE__)) && \
   !defined(UNICODE_BUILDING_POSIX_WRAPPERS)
/*
 * ESX and MacOS X are UTF-8 environments so these functions can be
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

#endif // _POSIX_H_
