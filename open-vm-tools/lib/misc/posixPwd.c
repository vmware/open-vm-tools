/*********************************************************
 * Copyright (C) 2008-2018 VMware, Inc. All rights reserved.
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

#if !defined(_POSIX_PTHREAD_SEMANTICS) && defined(sun)
#define _POSIX_PTHREAD_SEMANTICS 1 // Needed to get POSIX-correct getpw*_r() on Solaris
#endif

#define UNICODE_BUILDING_POSIX_WRAPPERS

#include "vmware.h"
#include "posixInt.h"

#include <pwd.h>
#include <grp.h>

#if (!defined(__FreeBSD__) || __FreeBSD_release >= 503001) && !defined __ANDROID__
#define VM_SYSTEM_HAS_GETPWNAM_R 1
#define VM_SYSTEM_HAS_GETPWUID_R 1
#define VM_SYSTEM_HAS_GETGRNAM_R 1
#endif

static struct passwd *GetpwInternal(struct passwd *pw);
static int GetpwInternal_r(struct passwd *pw, char *buf, size_t size,
                           struct passwd **ppw);

#if defined __ANDROID__
/*
 * Android doesn't support getpwent().
 */
#define NO_GETPWENT
#endif


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwnam --
 *
 *      POSIX getpwnam()
 *
 * Results:
 *      Pointer to updated passwd struct on NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct passwd *
Posix_Getpwnam(const char *name)  // IN:
{
   struct passwd *pw;
   char *tmpname;

   if (!PosixConvertToCurrent(name, &tmpname)) {
      return NULL;
   }
   pw = getpwnam(tmpname);
   Posix_Free(tmpname);

   return GetpwInternal(pw);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwuid --
 *
 *      POSIX getpwuid()
 *
 * Results:
 *      Pointer to updated passwd struct on NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct passwd *
Posix_Getpwuid(uid_t uid)  // IN:
{
   struct passwd *pw = getpwuid(uid);

   return GetpwInternal(pw);
}


/*
 *----------------------------------------------------------------------
 *
 * GetpwInternal --
 *
 *      Helper function for Posix_Getpwnam, Posix_Getpwuid and Posix_Getpwent
 *
 * Results:
 *      Pointer to updated passwd struct on NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

static struct passwd *
GetpwInternal(struct passwd *pw)  // IN:
{
   int ret;
   static struct passwd spw = {0};

   if (!pw) {
      return NULL;
   }

   /* Free static structure string pointers before reuse. */
   Posix_Free(spw.pw_passwd);
   spw.pw_passwd = NULL;
   Posix_Free(spw.pw_dir);
   spw.pw_dir = NULL;
   Posix_Free(spw.pw_name);
   spw.pw_name = NULL;
#if !defined __ANDROID__
   Posix_Free(spw.pw_gecos);
   spw.pw_gecos = NULL;
#endif
   Posix_Free(spw.pw_shell);
   spw.pw_shell = NULL;
#if defined(__FreeBSD__)
   Posix_Free(spw.pw_class);
   spw.pw_class = NULL;
#endif

   /* Fill out structure with new values. */
   spw.pw_uid = pw->pw_uid;
   spw.pw_gid = pw->pw_gid;
#if defined(__FreeBSD__)
   spw.pw_change = pw->pw_change;
   spw.pw_expire = pw->pw_expire;
   spw.pw_fields = pw->pw_fields;
#endif

#if !defined(sun)
   ret = ENOMEM;
#else
   ret = EIO;
#endif
   if (pw->pw_passwd &&
       (spw.pw_passwd = Unicode_Alloc(pw->pw_passwd,
                                      STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_dir &&
       (spw.pw_dir = Unicode_Alloc(pw->pw_dir,
                                   STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_name &&
       (spw.pw_name = Unicode_Alloc(pw->pw_name,
                                    STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
#if !defined __ANDROID__
   if (pw->pw_gecos &&
       (spw.pw_gecos = Unicode_Alloc(pw->pw_gecos,
                                     STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
#endif
   if (pw->pw_shell &&
       (spw.pw_shell = Unicode_Alloc(pw->pw_shell,
                                     STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
#if defined(__FreeBSD__)
   if (pw->pw_class &&
       (spw.pw_class = Unicode_Alloc(pw->pw_class,
                                     STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
#endif
   ret = 0;

exit:
   if (ret != 0) {
      errno = ret;
      return NULL;
   }
   return &spw;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwent --
 *
 *      POSIX getpwent()
 *
 * Results:
 *      Pointer to updated passwd struct or NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct passwd *
Posix_Getpwent(void)
{
#if defined NO_GETPWENT
   NOT_IMPLEMENTED();
   errno = ENOSYS;
   return NULL;
#else
   struct passwd *pw = getpwent();

   return GetpwInternal(pw);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Endpwent --
 *
 *      POSIX endpwent()
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Posix_Endpwent(void)
{
#if defined(__ANDROID__)
   /*
    * endpwent() not avail until Android O
    * TODO: When Android O(Oreo) becomes mainstream, we can remove this #if
    * Refer https://github.com/android-ndk/ndk/issues/77
    */
   return;
#else
   endpwent();
#endif
}


#if !defined(VM_SYSTEM_HAS_GETPWNAM_R) || \
   !defined(VM_SYSTEM_HAS_GETPWUID_R) || \
   !defined(VM_SYSTEM_HAS_GETGRNAM_R) // {
/*
 *-----------------------------------------------------------------------------
 *
 * CopyFieldIntoBuf --
 *
 *      Copies a field in a passwd/group structure into the supplied buffer,
 *      and sets that pointer into dest. Used as a helper function for the
 *      EmulateGet* routines.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      Updates *buf and *bufLen to allocate space for the copied field.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CopyFieldIntoBuf(const char *src,  // IN:
                 char **dest,      // OUT:
                 char **buf,       // OUT
                 size_t *bufLen)   // OUT:
{
   if (src) {
      size_t needLen = strlen(src) + 1;

      if (*bufLen < needLen) {
         return FALSE;
      }

      *dest = *buf;
      memcpy(*dest, src, needLen);
      *buf += needLen;
      *bufLen -= needLen;
   } else {
      *dest = NULL;
   }

   return TRUE;
}


#endif // }


#if !defined(VM_SYSTEM_HAS_GETPWNAM_R) || !defined(VM_SYSTEM_HAS_GETPWUID_R) // {
/*
 *-----------------------------------------------------------------------------
 *
 * PasswdCopy --
 *
 *      Copies a password structure as part of emulating the getpw*_r routines.
 *
 * Results:
 *      'new' if successful, NULL otherwise.
 *
 * Side effects:
 *      Modifies 'buf'
 *
 *-----------------------------------------------------------------------------
 */

static struct passwd *
PasswdCopy(struct passwd *orig, // IN
           struct passwd *new,  // IN/OUT
           char *buf,           // IN
           size_t bufLen)       // IN
{
   if (!orig) {
      return NULL;
   }

   *new = *orig;

   if (!CopyFieldIntoBuf(orig->pw_name, &new->pw_name, &buf, &bufLen)) {
      return NULL;
   }
   if (!CopyFieldIntoBuf(orig->pw_passwd, &new->pw_passwd, &buf, &bufLen)) {
      return NULL;
   }
#if !defined __ANDROID__
   if (!CopyFieldIntoBuf(orig->pw_gecos, &new->pw_gecos, &buf, &bufLen)) {
      return NULL;
   }
#endif
   if (!CopyFieldIntoBuf(orig->pw_dir, &new->pw_dir, &buf, &bufLen)) {
      return NULL;
   }
   if (!CopyFieldIntoBuf(orig->pw_shell, &new->pw_shell, &buf, &bufLen)) {
      return NULL;
   }
#ifdef __FreeBSD__
   if (!CopyFieldIntoBuf(orig->pw_class, &new->pw_class, &buf, &bufLen)) {
      return NULL;
   }
#endif

   return new;
}
#endif // }


#ifndef VM_SYSTEM_HAS_GETPWNAM_R // {
/*
 *-----------------------------------------------------------------------------
 *
 * EmulateGetpwnam_r --
 *
 *      Emulates getpwnam_r() for old/odd systems that don't have it
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Data may be stored in 'buf'.
 *
 *-----------------------------------------------------------------------------
 */

static int
EmulateGetpwnam_r(const char *name,       // IN
                  struct passwd *pwbuf,   // IN/OUT
                  char *buf,              // IN
                  size_t buflen,          // IN
                  struct passwd **pwbufp) // IN/OUT
{
   static Atomic_uint32 mutex = {0};
   struct passwd *pw;
   int savedErrno;

   ASSERT(pwbuf);
   ASSERT(name);
   ASSERT(buf);
   ASSERT(pwbufp);

   /*
    * XXX Use YIELD() here when it works on FreeBSD.
    */
   while (Atomic_ReadWrite(&mutex, 1)); // Spinlock.

   pw = getpwnam(name);
   savedErrno = errno;
   *pwbufp = PasswdCopy(pw, pwbuf, buf, buflen);

   Atomic_Write(&mutex, 0);

   if (pw) {
      return 0;
   } else if (savedErrno) {
      return savedErrno;
   } else {
      return ENOENT;
   }
}
#endif


#ifndef VM_SYSTEM_HAS_GETPWUID_R
/*
 *-----------------------------------------------------------------------------
 *
 * EmulateGetpwuid_r --
 *
 *      Emulates getpwuid_r() for old/odd systems that don't have it
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
EmulateGetpwuid_r(uid_t uid,               // IN:
                  struct passwd *pwbuf,    // IN/OUT:
                  char *buf,               // IN:
                  size_t buflen,           // IN:
                  struct passwd **pwbufp)  // IN/OUT:
{
   static Atomic_uint32 mutex = {0};
   struct passwd *pw;
   int savedErrno;

   ASSERT(pwbuf);
   ASSERT(buf);
   ASSERT(pwbufp);

   /*
    * XXX Use YIELD() here when it works on FreeBSD.
    */

   while (Atomic_ReadWrite(&mutex, 1)); // Spinlock

   pw = getpwuid(uid);
   savedErrno = errno;
   *pwbufp = PasswdCopy(pw, pwbuf, buf, buflen);

   Atomic_Write(&mutex, 0);

   if (pw) {
      return 0;
   } else if (savedErrno) {
      return savedErrno;
   } else {
      return ENOENT;
   }
}
#endif // }


#ifndef VM_SYSTEM_HAS_GETGRNAM_R // {
/*
 *-----------------------------------------------------------------------------
 *
 * GroupCopy --
 *
 *      Copies a password structure as part of emulating the getgr*_r routines.
 *
 * Results:
 *      'new' if successful, NULL otherwise.
 *
 * Side effects:
 *      Modifies 'buf'
 *
 *-----------------------------------------------------------------------------
 */

static struct group *
GroupCopy(struct group *orig,  // IN:
          struct group *new,   // IN/OUT:
          char *buf,           // IN:
          size_t bufLen)       // IN:
{
   if (!orig) {
      return NULL;
   }

   *new = *orig;

   if (!CopyFieldIntoBuf(orig->gr_name, &new->gr_name, &buf, &bufLen)) {
      return NULL;
   }
   if (!CopyFieldIntoBuf(orig->gr_passwd, &new->gr_passwd, &buf, &bufLen)) {
      return NULL;
   }

   if (orig->gr_mem) {
      int i;
      uintptr_t alignLen;
      char **newGrMem;

      /*
       * Before putting the gr_mem 'char **' array into 'buf', aligns the
       * buffer to a pointer-size boundary.
       */

      alignLen = ((((uintptr_t) buf) +
                    (sizeof(void *) - 1)) & ~(sizeof(void *) - 1));
      alignLen -= ((uintptr_t) buf);

      if (bufLen < alignLen) {
         return NULL;
      }
      buf += alignLen;
      bufLen -= alignLen;

      /*
       * Count the number of items in the gr_mem array, and then copy them all.
       */

      for (i = 0; orig->gr_mem[i]; i++);
      i++; // need space for a terminating NULL

      if (bufLen < (i * sizeof(void *))) {
         return NULL;
      }
      newGrMem = (char **)buf;
      buf += i * sizeof(void *);
      bufLen -= i * sizeof(void *);

      for (i = 0; orig->gr_mem[i]; i++, newGrMem++) {
         size_t flen;

         flen = strlen(orig->gr_mem[i]) + 1;
         if (bufLen < flen) {
            return NULL;
         }

         *newGrMem = buf;
         memcpy(*newGrMem, orig->gr_mem[i], flen);
         buf += flen;
         bufLen -= flen;
      }
      *newGrMem = NULL;
   }

   return new;
}
#endif // }


#ifndef VM_SYSTEM_HAS_GETGRNAM_R // {
/*
 *-----------------------------------------------------------------------------
 *
 * EmulateGetgrnam_r --
 *
 *      Emulates getgrnam_r() for old/odd systems that don't have it
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Data may be stored in 'buf'.
 *
 *-----------------------------------------------------------------------------
 */

static int
EmulateGetgrnam_r(const char *name,       // IN:
                  struct group *grbuf,    // IN/OUT:
                  char *buf,              // IN:
                  size_t buflen,          // IN:
                  struct group **grbufp)  // IN/OUT:
{
   static Atomic_uint32 mutex = {0};
   struct group *gr;
   int savedErrno;

   ASSERT(grbuf);
   ASSERT(name);
   ASSERT(buf);
   ASSERT(grbufp);

   /*
    * XXX Use YIELD() here once it is available on FreeBSD
    */

   while (Atomic_ReadWrite(&mutex, 1)); // Spinlock

   gr = getgrnam(name);
   savedErrno = errno;
   *grbufp = GroupCopy(gr, grbuf, buf, buflen);

   Atomic_Write(&mutex, 0);

   if (gr) {
      return 0;
   } else if (savedErrno) {
      return savedErrno;
   } else {
      return ENOENT;
   }
}
#endif // }


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwnam_r --
 *
 *      POSIX getpwnam_r()
 *
 * Results:
 *      Returns 0 with success and pointer to updated passwd struct
 *      or returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Getpwnam_r(const char *name,     // IN:
                 struct passwd *pw,    // IN:
                 char *buf,            // IN:
                 size_t size,          // IN:
                 struct passwd **ppw)  // OUT:
{
   int ret;
   char *tmpname;

   if (!PosixConvertToCurrent(name, &tmpname)) {
      /*
       * Act like nonexistent user, almost.
       * While getpwnam_r() returns 0 on nonexistent user,
       * we will return the errno instead.
       */

      *ppw = NULL;

      return errno;
   }

#if defined(VM_SYSTEM_HAS_GETPWNAM_R)
   ret = getpwnam_r(tmpname, pw, buf, size, ppw);
#else
   ret = EmulateGetpwnam_r(tmpname, pw, buf, size, ppw);
#endif

   Posix_Free(tmpname);

   // ret is errno on failure, *ppw is NULL if no matching entry found.
   if (ret != 0 || *ppw == NULL) {
      return ret;
   }

   return GetpwInternal_r(pw, buf, size, ppw);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwuid_r --
 *
 *      POSIX getpwuid_r()
 *
 * Results:
 *      Returns 0 with success and pointer to updated passwd struct
 *      or returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Getpwuid_r(uid_t uid,            // IN:
                 struct passwd *pw,    // IN:
                 char *buf,            // IN:
                 size_t size,          // IN:
                 struct passwd **ppw)  // OUT:
{
   int ret;

#if defined(VM_SYSTEM_HAS_GETPWNAM_R)
   ret = getpwuid_r(uid, pw, buf, size, ppw);
#else
   ret = EmulateGetpwuid_r(uid, pw, buf, size, ppw);
#endif
   if (ret != 0 || *ppw == NULL) {
      // ret is errno on failure, *ppw is NULL if no matching entry found.
      return ret;
   }

   return GetpwInternal_r(pw, buf, size, ppw);
}


/*
 *----------------------------------------------------------------------
 *
 * GetpwInternal_r --
 *
 *      Helper function for Posix_Getpwnam_r and Posix_Getpwuid_r
 *
 * Results:
 *      Returns 0 with success and pointer to updated passwd struct
 *      or returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
GetpwInternal_r(struct passwd *pw,    // IN:
                char *buf,            // IN:
                size_t size,          // IN:
                struct passwd **ppw)  // OUT:
{
   int ret;
   char *pwname = NULL;
   char *passwd = NULL;
#if !defined __ANDROID__
   char *gecos = NULL;
#endif
   char *dir = NULL;
   char *shell = NULL;
   size_t n;

   /*
    * Maybe getpwnam_r didn't use supplied struct, but we don't care.
    * We just fix up the one it gives us.
    */

   pw = *ppw;

   /*
    * Convert strings to UTF-8
    */

   ret = ENOMEM;
   if (pw->pw_name &&
       (pwname = Unicode_Alloc(pw->pw_name,
                               STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_passwd &&
       (passwd = Unicode_Alloc(pw->pw_passwd,
                               STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
#if !defined __ANDROID__
   if (pw->pw_gecos &&
       (gecos = Unicode_Alloc(pw->pw_gecos,
                              STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
#endif
   if (pw->pw_dir &&
       (dir = Unicode_Alloc(pw->pw_dir,
                            STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_shell &&
       (shell = Unicode_Alloc(pw->pw_shell,
                              STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }

   /*
    * Put UTF-8 strings into the structure.
    */

   ret = ERANGE;
   n = 0;

   if (pwname) {
      size_t len = strlen(pwname) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      pw->pw_name = memcpy(buf + n, pwname, len);
      n += len;
   }

   if (passwd != NULL) {
      size_t len = strlen(passwd) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      pw->pw_passwd = memcpy(buf + n, passwd, len);
      n += len;
   }
#if !defined __ANDROID__
   if (gecos) {
      size_t len = strlen(gecos) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      pw->pw_gecos = memcpy(buf + n, gecos, len);
      n += len;
   }
#endif
   if (dir) {
      size_t len = strlen(dir) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      pw->pw_dir = memcpy(buf + n, dir, len);
      n += len;
   }

   if (shell) {
      size_t len = strlen(shell) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      pw->pw_shell = memcpy(buf + n, shell, len);
      n += len;
   }
   ret = 0;

exit:
   Posix_Free(passwd);
   Posix_Free(dir);
   Posix_Free(pwname);
#if !defined __ANDROID__
   Posix_Free(gecos);
#endif
   Posix_Free(shell);

   return ret;
}


#if !defined(sun) // {
/*
 *----------------------------------------------------------------------
 *
 * Posix_GetGroupList --
 *
 *      POSIX getgrouplist()
 *
 * Results:
 *      Returns number of groups found, or -1 if *ngroups is
 *      smaller than number of groups found.  Also returns
 *      the list of groups.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Posix_GetGroupList(const char *user,  // IN:
                   gid_t group,       // IN:
                   gid_t *groups,     // OUT:
                   int *ngroups)      // IN/OUT:
{
   char *tmpuser;
   int ret;

   if (!PosixConvertToCurrent(user, &tmpuser)) {
      /*
       * Act like nonexistent user.
       * The supplied gid is always returned, so there's exactly
       * one group.
       * While the man page doesn't say, the return value is
       * the same as *ngroups in the success case.
       *
       * Should we always return -1 instead?
       *
       * -- edward
       */

      int n = *ngroups;

      *ngroups = 1;
      if (n < 1) {
         return -1;
      }
      ASSERT(groups != NULL);
      *groups = group;

      return 1;
   }

   ret = getgrouplist(tmpuser, group, groups, ngroups);

   Posix_Free(tmpuser);

   return ret;
}


#endif // }

/*
 *----------------------------------------------------------------------
 *
 * Posix_Getgrnam --
 *
 *      POSIX getgrnam()
 *
 * Results:
 *      Pointer to updated group struct on NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct group *
Posix_Getgrnam(const char *name)  // IN:
{
   struct group *gr;
   char *tmpname;
   int ret;
   static struct group sgr = {0};

   if (!PosixConvertToCurrent(name, &tmpname)) {
      return NULL;
   }
   gr = getgrnam(tmpname);
   Posix_Free(tmpname);

   if (!gr) {
      return NULL;
   }

   /* Free static structure string pointers before reuse. */
   Posix_Free(sgr.gr_name);
   sgr.gr_name = NULL;
   Posix_Free(sgr.gr_passwd);
   sgr.gr_passwd = NULL;
   Util_FreeStringList(sgr.gr_mem, -1);
   sgr.gr_mem = NULL;

   /* Fill out structure with new values. */
   sgr.gr_gid = gr->gr_gid;

   ret = ENOMEM;
   if (gr->gr_passwd &&
       (sgr.gr_passwd = Unicode_Alloc(gr->gr_passwd,
                                      STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (gr->gr_name &&
       (sgr.gr_name = Unicode_Alloc(gr->gr_name,
                                    STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (gr->gr_mem) {
      sgr.gr_mem = Unicode_AllocList(gr->gr_mem, -1,
                                     STRING_ENCODING_DEFAULT);
   }

   ret = 0;

 exit:
   if (ret != 0) {
      errno = ret;
      return NULL;
   }

   return &sgr;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getgrnam_r --
 *
 *      POSIX getgrnam_r()
 *
 * Results:
 *      Returns 0 with success and pointer to updated group struct
 *      or returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Getgrnam_r(const char *name,     // IN:
                 struct group *gr,     // IN:
                 char *buf,            // IN:
                 size_t size,          // IN:
                 struct group **pgr)   // OUT:
{
   int ret, i;
   char *tmpname;
   char *grname = NULL;
   char *grpasswd = NULL;
   char **grmem = NULL;
   size_t n;

   if (!PosixConvertToCurrent(name, &tmpname)) {
      /*
       * Act like nonexistent group, almost.
       * While getgrnam_r() returns 0 on nonexistent group,
       * we will return the errno instead.
       */

      *pgr = NULL;

      return errno;
   }

#if defined(VM_SYSTEM_HAS_GETGRNAM_R)
   ret = getgrnam_r(tmpname, gr, buf, size, pgr);
#else
   ret = EmulateGetgrnam_r(tmpname, gr, buf, size, pgr);
#endif
   Posix_Free(tmpname);

   // ret is errno on failure, *pgr is NULL if no matching entry found.
   if (ret != 0 || *pgr == NULL) {
      return ret;
   }

   /*
    * Maybe getgrnam_r didn't use supplied struct, but we don't care.
    * We just fix up the one it gives us.
    */

   gr = *pgr;

   /*
    * Convert strings to UTF-8
    */

   ret = ENOMEM;
   if (gr->gr_name &&
       (grname = Unicode_Alloc(gr->gr_name,
                               STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (gr->gr_passwd &&
       (grpasswd = Unicode_Alloc(gr->gr_passwd,
                                 STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (gr->gr_mem) {
      grmem = Unicode_AllocList(gr->gr_mem, -1, STRING_ENCODING_DEFAULT);
   }

   /*
    * Put UTF-8 strings into the structure.
    */

   ret = ERANGE;
   n = 0;

   if (grname) {
      size_t len = strlen(grname) + 1;

      if (n + len > size) {
         goto exit;
      }
      gr->gr_name = memcpy(buf + n, grname, len);
      n += len;
   }

   if (grpasswd != NULL) {
      size_t len = strlen(grpasswd) + 1;

      if (n + len > size) {
         goto exit;
      }
      gr->gr_passwd = memcpy(buf + n, grpasswd, len);
      n += len;
   }

   if (grmem) {
      for (i = 0; grmem[i]; i++) {
         size_t len = strlen(grmem[i]) + 1;

         if (n + len > size) {
            goto exit;
         }
         gr->gr_mem[i] = memcpy(buf + n, grmem[i], len);
         n += len;
      }
   }

   ret = 0;

 exit:
   Posix_Free(grpasswd);
   Posix_Free(grname);
   Util_FreeStringList(grmem, -1);

   return ret;
}
