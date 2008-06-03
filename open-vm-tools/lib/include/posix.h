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
#endif

#include "vm_basic_types.h"
#include "unicodeTypes.h"
#include "codeset.h"

/*
 * Force all users of these wrappers to use the LFS (large file) interface
 * versions of the functions wrapper therein. If we don't do this the
 * wrappers may be built with the LFS versions and the callers might not
 * leading to a (potentially undetected) mismatch.
 */

#if defined(linux) && !defined(N_PLAT_NLM) && \
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
int Posix_Stat(ConstUnicode pathName, struct stat *statbuf);
int Posix_Chmod(ConstUnicode pathName, mode_t mode);
void Posix_Perror(ConstUnicode str);

#if !defined(N_PLAT_NLM)
int Posix_Mkdir(ConstUnicode pathName, mode_t mode);
int Posix_Chdir(ConstUnicode pathName);
Unicode Posix_Getenv(ConstUnicode name);
long Posix_Pathconf(ConstUnicode pathName, int name);
#endif

#if !defined(_WIN32)
void *Posix_Dlopen(ConstUnicode pathName, int flags);

int Posix_Utime(ConstUnicode pathName, const struct utimbuf *times);

#if !defined(N_PLAT_NLM)
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
int Posix_Lstat(ConstUnicode pathName, struct stat *statbuf);
DIR *Posix_OpenDir(ConstUnicode pathName);
int Posix_System(ConstUnicode command);
int Posix_Putenv(Unicode name);

/*
 * These functions return dynamically allocated stings that have to be
 * freed by the caller so they must be used in the ESX environment. They
 * are different than their POSIX "base" functions.
 */
Unicode Posix_RealPath(ConstUnicode pathName);
Unicode Posix_ReadLink(ConstUnicode pathName);

struct passwd *Posix_Getpwnam(ConstUnicode name);
struct passwd *Posix_Getpwuid(uid_t uid);

#if !defined(sun)
int Posix_Statfs(ConstUnicode pathName, struct statfs *statfsbuf);
int Posix_Setenv(ConstUnicode name, ConstUnicode value, int overWrite);


#if !defined(__FreeBSD__)
int Posix_Getpwnam_r(ConstUnicode name, struct passwd *pw,
                     char *buf, size_t size, struct passwd **ppw);
int Posix_Getpwuid_r(uid_t uid, struct passwd *pw,
                     char *buf, size_t size, struct passwd **ppw);
struct passwd *Posix_Getpwent(void);
int Posix_GetGroupList(ConstUnicode user, gid_t group, gid_t *groups,
                       int *ngroups);
struct group *Posix_Getgrnam(ConstUnicode name);
int Posix_Getgrnam_r(ConstUnicode name, struct group *gr, 
                 char *buf, size_t size, struct group **pgr);

#if !defined(__APPLE__)
int Posix_Mount(ConstUnicode source, ConstUnicode target,
                const char *filesystemtype, unsigned long mountflags,
		const void *data);
int Posix_Umount(ConstUnicode target);
FILE *Posix_Setmntent(ConstUnicode pathName, const char *mode);
struct mntent *Posix_Getmntent(FILE *fp);
struct mntent *Posix_Getmntent_r(FILE *fp, struct mntent *m,
                                 char *buf, int size);

#endif // !defined(__APPLE__)
#endif // !defined(__FreeBSD__)
#else  // !defined(sun)
int Posix_Getmntent(FILE *fp, struct mnttab *mp);

#endif // !defined(sun)
#endif // !defined(N_PLAT_NLM)
#endif // !define(_WIN32)

#if defined(CURRENT_IS_UTF8) && \
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
#define Posix_Utime utime
#define Posix_Utimes utimes
#endif

#endif // _POSIX_H_
