/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * hgfsmounter.c --
 *
 *      Helper app for mounting HGFS shares on Linux and FreeBSD. On Linux, we need this
 *      because we must pass a binary blob through mount(2) to the HGFS driver, in order
 *      to properly communicate the share name that we're interested in mounting. On
 *      FreeBSD, we need this because FreeBSD requires that each filesystem type have a
 *      separate mount program installed as /sbin/mount_fstype
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#if defined(linux)
#   include <mntent.h>
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
#   include <sys/uio.h>
#   include <sys/param.h>
#   define MS_MANDLOCK 0
#   define MS_RDONLY MNT_RDONLY
#   define MS_SYNCHRONOUS MNT_SYNCHRONOUS
#   define MS_NOEXEC MNT_NOEXEC
#   define MS_NOSUID MNT_NOSUID
/*
 * MNT_NODEV does not exist, or is set to 0, on newer versions of FreeBSD.
 */
#   if defined(MNT_NODEV) && MNT_NODEV
#      define MS_NODEV MNT_NODEV
#   endif
#   define MS_UNION MNT_UNION
#   define MS_ASYNC MNT_ASYNC
#   define MS_SUIDDIR MNT_SUIDDIR
#   define MS_SOFTDEP MNT_SOFTDEP
#   define MS_NOSYMFOLLOW MNT_NOSYMFOLLOW
#   define MS_JAILDEVFS MNT_JAILDEVFS
#   define MS_MULTILABEL MNT_MULTILABEL
#   define MS_ACLS MNT_ACLS
#   define MS_NODIRATIME 0
#   define MS_NOCLUSTERR MNT_NOCLUSTERR
#   define MS_NOCLUSTERW MNT_NOCLUSTERW
#   define MS_REMOUNT MNT_RELOAD

#  if defined(__FreeBSD__)
#     define MS_NOATIME MNT_NOATIME
#  elif defined(__APPLE__)
/*
 *  XXX This is defined in the sys/mount.h in the OS X 10.5 Kernel.framework but not the
 *  10.4 one. Once this is built against the newer library, this should be changed.
 */
#     define MS_NOATIME 0
#endif

#endif


#include <sys/mount.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef MOUNTED // defined in mntent.h
/*
 * If VM_HAVE_MTAB is set, hgfsmounter will update /etc/mtab. This is not necessary on
 * systems such as FreeBSD (and possibly Solaris) that don't have a mtab or the mntent.h
 * routines.
 */
#   define VM_HAVE_MTAB 1
#endif

#include "hgfsDevLinux.h"
#include "vm_basic_types.h"
#include "vm_assert.h"
#include "str.h"
#include "strutil.h"
#include "hgfsmounter_version.h"

/* XXX embed_version.h does not currently support Mach-O binaries (OS X). */
#if defined(linux) || defined(__FreeBSD__)
#  include "embed_version.h"
   VM_EMBED_VERSION(HGFSMOUNTER_VERSION_STRING);
#endif

/*
 * Not defined in glibc 2.1.3
 */
#ifndef MS_BIND
#define MS_BIND 4096
#endif
#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#define MOUNT_OPTS_BUFFER_SIZE 256

#if defined(__GNUC__) && __GNUC__ < 3
/* __VA_ARGS__ isn't supported by old gcc's, so hack around the situation */
#define LOG(format...) (beVerbose ? printf(format) : 0)
#else
#define LOG(format, ...) (beVerbose ? printf(format, ##__VA_ARGS__) : 0)
#endif

static char *thisProgram;
static char *thisProgramBase;
static char *shareName;
static char *mountPoint;
static Bool beVerbose = FALSE;

static void PrintVersion(void);
static void PrintUsage(void);
static size_t GetPathMax(const char *path);
static Bool ParseShareName(const char *shareName,
                           const char **shareNameHost,
                           const char **shareNameDir);
static Bool ParseUid(const char *uidString,
                     uid_t *uid);
static Bool ParseGid(const char *gidString,
                     gid_t *gid);
static Bool ParseOptions(const char *optionString,
                         HgfsMountInfo *mountInfo,
                         int *flags);
#ifdef VM_HAVE_MTAB
static void UpdateMtab(HgfsMountInfo *mountInfo,
                       int flags);
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * PrintVersion --
 *
 *    Displays version and exits with success.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
PrintVersion(void)
{
   printf("%s version: %s\n", thisProgramBase, HGFSMOUNTER_VERSION_STRING);
   exit(EXIT_SUCCESS);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PrintUsage --
 *
 *    Displays usage for the HGFS mounting utility, and exits with failure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
PrintUsage(void)
{
   printf("Usage: %s <sharename> <dir> [-o <options>]\n", thisProgramBase);
   printf("Mount the HGFS share, specified by name, to a local directory.\n");
   printf("Share name must be in host:dir format.\n\nOptions:\n");
   printf("  uid=<arg>             mount owner (by uid or username)\n");
   printf("  gid=<arg>             mount group (by gid or groupname)\n");
   printf("  fmask=<arg>           file umask (in octal)\n");
   printf("  dmask=<arg>           directory umask (in octal)\n");
   printf("  ro                    mount read-only\n");
   printf("  rw                    mount read-write (default)\n");
   printf("  nosuid                ignore suid/sgid bits\n");
   printf("  suid                  allow suid/sgid bits (default)\n");
#ifdef MS_NODEV
   printf("  nodev                 prevent device node access\n");
   printf("  dev                   allow device node access (default)\n");
#endif
   printf("  noexec                prevent program execution\n");
   printf("  exec                  allow program execution (default)\n");
   printf("  sync                  file writes are synchronous\n");
   printf("  async                 file writes are asynchronous (default)\n");
   printf("  mand                  allow mandatory locks\n");
   printf("  nomand                prevent mandatory locks (default)\n");
   printf("  noatime               do not update access times\n");
   printf("  atime                 update access times (default)\n");
   printf("  nodiratime            do not update directory access times\n");
   printf("  adirtime              update directory access times (default)\n");
   printf("  ttl=<arg>             time before file attributes must be\n");
   printf("                        revalidated (in seconds). Improves\n");
   printf("                        performance but decreases coherency.\n");
   printf("                        Defaults to 1 if not set.\n");
   printf("\n");
   printf("This command is intended to be run from within /bin/mount by\n");
   printf("passing the option '-t %s'. For example:\n", HGFS_NAME);
   printf("  mount -t %s .host:/ /mnt/hgfs/\n", HGFS_NAME);
   printf("  mount -t %s .host:/foo /mnt/foo\n", HGFS_NAME);
   printf("  mount -t %s .host:/foo/bar /var/lib/bar\n", HGFS_NAME);
   exit(EXIT_FAILURE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetPathMax
 *
 *    Helper function to get the system's maximum path length for a given
 *    path. In userspace, PATH_MAX may not be defined, and we must use
 *    pathconf(3) to get its value.
 *
 *    This is the realpath(3)-approved way of getting the maximum path size,
 *    inasmuch as realpath(3) can be an approved function (see the BUGS section
 *    in the manpage for details).
 *
 * Results:
 *    Always succeeds, returns the maximum path size.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static size_t
GetPathMax(const char *path) // IN: path to check
{
   size_t pathMax;

#ifndef PATH_MAX
   long sysPathMax;

   /*
    * pathconf(3) may return -1 if the system imposes no pathname bound, or if
    * there was an error. In any case, we're advised by realpath(3)'s manpage
    * not to use the result of pathconf for direct allocation, as it may be too
    * large. So we declare 4096 as our upper bound, to be used when pathconf(3)
    * returns an error, returns zero, returns a very large quantity, or when
    * we learn that there's no limit.
    */
   sysPathMax = pathconf(path, _PC_PATH_MAX);
   if (sysPathMax <= 0 || sysPathMax > 4096) {
      pathMax = 4096;
   } else {
      pathMax = sysPathMax;
   }
#else
   pathMax = PATH_MAX;
#endif

   return pathMax;
}


/*
 *-----------------------------------------------------------------------------
 *
 * IsValidShareName
 *
 *    A helper function to parse the share name from "host:share" format into
 *    two separate strings, reporting errors if any.
 *
 * Results:
 *    TRUE on success, shareNameHost and shareNameDir have been set.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ParseShareName(const char *shareName,      // IN:  Share name to validate
               const char **shareNameHost, // OUT: Share name, host component
               const char **shareNameDir)  // OUT: Share name, dir component
{
   const char *colon, *dir;

   /* 1) Must be colon separated into host and dir. */
   colon = strchr(shareName, ':');
   if (colon == NULL) {
      printf("Share name must be in host:dir format\n");
      return FALSE;
   }

   /* 2) Dir must not be empty. */
   dir = colon + 1;
   if (*dir == '\0') {
      printf("Directory in share name must not be empty\n");
      return FALSE;
   }

   /* 3) Dir must start with forward slash. */
   if (*dir != '/') {
      printf("Directory in share name must be an absolute path\n");
      return FALSE;
   }

   /* 4) Host must be ".host". */
   if (strncmp(shareName, ".host:", 6) != 0) {
      printf("Host in share name must be \".host\"\n");
      return FALSE;
   }

   *shareNameHost = ".host";
   LOG("Host component of share name is \"%s\"\n", *shareNameHost);
   *shareNameDir = dir;
   LOG("Directory component of share name is \"%s\"\n", *shareNameDir);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ParseUid
 *
 *    A helper function to process a string containing either a user name
 *    or a uid and return the resulting uid.
 *
 * Results:
 *    TRUE on success, uid contains the user ID.
 *    FALSE on failure, uid is undefined.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ParseUid(const char *uidString, // IN:  String with uid
         uid_t *uid)            // OUT: Converted uid
{
   Bool success = FALSE;
   uid_t myUid;

   ASSERT(uidString);
   ASSERT(uid);

   /*
    * The uid can be a direct value or a username which we must first
    * translate to its numeric value.
    */
   if (isdigit(*uidString)) {
      errno = 0;
      myUid = strtoul(uidString, NULL, 10);
      if (errno == 0) {
         *uid = myUid;
         success = TRUE;
      } else {
         printf("Bad UID value \"%s\"\n", uidString);
      }
   } else {
      struct passwd *pw;

      if (!(pw = getpwnam(uidString))) {
         printf("Bad user name \"%s\"\n", uidString);
      } else {
         *uid = pw->pw_uid;
         success = TRUE;
      }
      endpwent();
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ParseGid
 *
 *    A helper function to process a string containing either a group name
 *    or a gid and return the resulting gid.
 *
 * Results:
 *    TRUE on success, gid contains the group ID.
 *    FALSE on failure, gid is undefined.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ParseGid(const char *gidString, // IN:  String with gid
         gid_t *gid)            // OUT: Converted gid
{
   Bool success = FALSE;
   gid_t myGid;

   ASSERT(gidString);
   ASSERT(gid);

   /*
    * The gid can be a direct value or a group name which we must first
    * translate to its numeric value.
    */
   if (isdigit(*gidString)) {
      errno = 0;
      myGid = strtoul(gidString, NULL, 10);
      if (errno == 0) {
         *gid = myGid;
         success = TRUE;
      } else {
         printf("Bad GID value \"%s\"\n", gidString);
      }
   } else {
      struct group *gr;

      if (!(gr = getgrnam(gidString))) {
         printf("Bad group name \"%s\"\n", gidString);
      } else {
         *gid = gr->gr_gid;
         success = TRUE;
      }
      endpwent();
   }
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ParseOptions --
 *
 *    Parses the options passed in by mount. Note that this doesn't correspond
 *    to the entire argument string, merely the "-o opt1=val1,opt2=val2"
 *    section.
 *
 * Results:
 *    TRUE if there were no problems.
 *    FALSE if there was an issue parsing an option.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ParseOptions(const char *optionString, // IN:  Option string to parse
             HgfsMountInfo *mountInfo, // OUT: Where we put the results
             int *flags)               // OUT: Flags we might modify
{
   unsigned int commaIndex = 0;
   Bool valid, success = FALSE;
   char *key = NULL;
   char *keyVal = NULL;

   ASSERT(optionString);
   ASSERT(mountInfo);
   ASSERT(flags);

   /* Parse the options string. */
   LOG("Parsing option string: %s\n", optionString);

   /* Loop to tokenize <keyval1>,<keyval2>,<keyval3>. */
   while ((keyVal = StrUtil_GetNextToken(&commaIndex,
                                         optionString, ",")) != NULL) {
      /* Now tokenize <key>[=<val>]. */
      unsigned int equalsIndex = 0;
      key = StrUtil_GetNextToken(&equalsIndex, keyVal, "=");
      if (key == NULL) {
         printf("Malformed options string\n");
         goto out;
      }

      /*
       * Here are all our recognized option keys. Some have corresponding
       * values, others don't.
       */
      if (strcmp(key, "fmask") == 0) {
         char *fmaskString = StrUtil_GetNextToken(&equalsIndex, keyVal, "=");
         if (fmaskString != NULL) {
            unsigned short fmask;

            /*
             * The way to check for an overflow in strtol(3), according to its
             * man page.
             */
            errno = 0;
            fmask = strtol(fmaskString, NULL, 8);
            free(fmaskString);
            if (errno == 0) {
               LOG("Setting mount fmask to %o\n", fmask);
               mountInfo->fmask = fmask;
            } else {
               LOG("Error, overflow in fmask\n");
               goto out;
            }
         } else {
            LOG("Error getting the value for fmask\n");
            goto out;
         }
      } else if (strcmp(key, "dmask") == 0) {
         char *dmaskString = StrUtil_GetNextToken(&equalsIndex, keyVal, "=");
         if (dmaskString != NULL) {
            unsigned short dmask;

            /*
             * The way to check for an overflow in strtol(3), according to its
             * man page.
             */
            errno = 0;
            dmask = strtol(dmaskString, NULL, 8);
            free(dmaskString);
            if (errno == 0) {
               LOG("Setting mount dmask to %o\n", dmask);
               mountInfo->dmask = dmask;
            } else {
               LOG("Error, overflow in dmask\n");
               goto out;
            }
         } else {
            LOG("Error getting the value for dmask\n");
            goto out;
         }
      } else if (strcmp(key, "uid") == 0) {
         char *uidString = StrUtil_GetNextToken(&equalsIndex, keyVal, "=");
         if (uidString != NULL) {
            uid_t uid = 0;

            valid = ParseUid(uidString, &uid);
            free(uidString);
            if (valid) {
               mountInfo->uid = uid;
               mountInfo->uidSet = TRUE;
               LOG("Setting mount owner to %u\n", uid);
            } else {
               goto out;
            }
         } else {
            LOG("Error getting the value for uid\n");
            goto out;
         }
      } else if (strcmp(key, "gid") == 0) {
         char *gidString = StrUtil_GetNextToken(&equalsIndex, keyVal, "=");
         if (gidString != NULL) {
            gid_t gid = 0;

            valid = ParseGid(gidString, &gid);
            free(gidString);
            if (valid) {
               mountInfo->gid = gid;
               mountInfo->gidSet = TRUE;
               LOG("Setting mount group to %u\n", gid);
            } else {
               goto out;
            }
         } else {
            LOG("Error getting the value for gid\n");
            goto out;
         }
      } else if (strcmp(key, "ttl") == 0) {
         int32 ttl;

         if (StrUtil_GetNextIntToken(&ttl, &equalsIndex, keyVal, "=") && ttl > 0) {
            mountInfo->ttl = ttl;
            LOG("Setting maximum attribute TTL to %u\n", ttl);
         } else {
            LOG("Error getting the value for ttl\n");
            goto out;
         }
      } else if (strcmp(key, "rw") == 0) { // can we write to the mount?
         *flags &= ~MS_RDONLY;
         LOG("Setting mount read-write\n");
      } else if (strcmp(key, "ro") == 0) {
         *flags |= MS_RDONLY;
         LOG("Setting mount read-only\n");
      } else if (strcmp(key, "nosuid") == 0) { // allow suid and sgid bits?
         *flags |= MS_NOSUID;
         LOG("Setting mount option for allowing suid/sgid bits off\n");
      } else if (strcmp(key, "suid") == 0) {
         *flags &= ~MS_NOSUID;
         LOG("Setting mount option for allowing suid/sgid bits on\n");
#ifdef MS_NODEV
      } else if (strcmp(key, "nodev") == 0) { // allow access to device nodes?
         *flags |= MS_NODEV;
         LOG("Setting mount option for accessing device nodes off\n");
      } else if (strcmp(key, "dev") == 0) {
         *flags &= ~MS_NODEV;
         LOG("Setting mount option for accessing device nodes on\n");
#endif
      } else if (strcmp(key, "noexec") == 0) { // allow program execution?
         *flags |= MS_NOEXEC;
         LOG("Setting mount option for program execution off\n");
      } else if (strcmp(key, "exec") == 0) {
         *flags &= ~MS_NOEXEC;
         LOG("Setting mount option for program execution on\n");
      } else if (strcmp(key, "sync") == 0) { // meaningless at the moment
         *flags |= MS_SYNCHRONOUS;
         LOG("Setting mount synchronous writes\n");
      } else if (strcmp(key, "async") == 0) {
         *flags &= ~MS_SYNCHRONOUS;
         LOG("Setting mount asynchronous writes\n");
      } else if (strcmp(key, "mand") == 0) { // are mandatory locks allowed?
         *flags |= MS_MANDLOCK;
         LOG("Setting mount option for allow mandatory locks on\n");
      } else if (strcmp(key, "nomand") == 0) {
         *flags &= ~MS_MANDLOCK;
         LOG("Setting mount option for allow mandatory locks off\n");
      } else if (strcmp(key, "noatime") == 0) { // don't update access times?
         *flags |= MS_NOATIME;
         LOG("Setting mount option for updating access times off\n");
      } else if (strcmp(key, "atime") == 0) {
         *flags &= ~MS_NOATIME;
         LOG("Setting mount option for updating access times on\n");
      } else if (strcmp(key, "nodiratime") == 0) { // don't update dir atimes?
         *flags |= MS_NODIRATIME;
         LOG("Setting mount option for updating directory access times off\n");
      } else if (strcmp(key, "diratime") == 0) {
         *flags &= ~MS_NODIRATIME;
         LOG("Setting mount option for updating directory access times on\n");
      } else if (strcmp(key, "bind") == 0) { // bind mount?
         *flags |= MS_BIND;
         LOG("Setting mount type to bind\n");
      } else if (strcmp(key, "move") == 0) { // move an existing mount?
         *flags |= MS_MOVE;
         LOG("Setting mount type to move\n");
      } else if (strcmp(key, "remount") == 0) { // remount?
         *flags |= MS_REMOUNT;
         LOG("Setting mount type to remount\n");
      } else {
         LOG("Skipping unrecognized option \"%s\"\n", key);
      }
      free(key);
      free(keyVal);
      key = NULL;
      keyVal = NULL;
   }
   success = TRUE;

  out:
   free(key);
   free(keyVal);
   return success;
}


#ifdef VM_HAVE_MTAB
/*
 *-----------------------------------------------------------------------------
 *
 * UpdateMtab --
 *
 *      Write the results of the mount into /etc/mtab.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May write to /etc/mtab.
 *
 *-----------------------------------------------------------------------------
 */

static void
UpdateMtab(HgfsMountInfo *mountInfo,  // IN: Info to write into mtab
           int flags)                 // IN: Flags (read-only, etc.)
{
   struct mntent mountEnt;
   FILE *mountFile;
   struct passwd *password;
   char *userName = NULL;

   ASSERT(mountInfo);

   mountFile = setmntent(MOUNTED, "a+");
   if (mountFile == NULL) {
      printf("Could not open mtab for appending, continuing sans mtab\n");
      return;
   }

   /* We only care about the mounting user if it isn't root. */
   if (getuid() != 0) {
      password = getpwuid(getuid());
      if (password == NULL) {
         printf("Could not get user for mounting uid, skipping user entry\n");
      } else {
         userName = password->pw_name;
      }
   }

   /*
    * Create the mtab entry to be written. We'll go ahead and try to write
    * even if we fail to allocate the mount options.
    */
   mountEnt.mnt_fsname = shareName;
   mountEnt.mnt_dir = mountPoint;
   mountEnt.mnt_type = HGFS_NAME;
   mountEnt.mnt_freq = 0;
   mountEnt.mnt_passno = 0;
   mountEnt.mnt_opts = malloc(MOUNT_OPTS_BUFFER_SIZE);
   if (mountEnt.mnt_opts) {
      char *ttlString;

      memset(mountEnt.mnt_opts, 0, MOUNT_OPTS_BUFFER_SIZE);

      /*
       * These are typically the displayed options in /etc/mtab (note that not
       * all options are typically displayed, just those the user may find
       * interesting).
       */
      if (flags & MS_RDONLY) {
         Str_Strcat(mountEnt.mnt_opts, "ro", MOUNT_OPTS_BUFFER_SIZE);
      } else {
         Str_Strcat(mountEnt.mnt_opts, "rw", MOUNT_OPTS_BUFFER_SIZE);
      }
      if (flags & MS_NOSUID) {
         Str_Strcat(mountEnt.mnt_opts, ",nosuid", MOUNT_OPTS_BUFFER_SIZE);
      }
#ifdef MS_NODEV
      if (flags & MS_NODEV) {
         Str_Strcat(mountEnt.mnt_opts, ",nodev", MOUNT_OPTS_BUFFER_SIZE);
      }
#endif
      if (flags & MS_NOEXEC) {
         Str_Strcat(mountEnt.mnt_opts, ",noexec", MOUNT_OPTS_BUFFER_SIZE);
      }
      if (flags & MS_SYNCHRONOUS) {
         Str_Strcat(mountEnt.mnt_opts, ",sync", MOUNT_OPTS_BUFFER_SIZE);
      }
      if (flags & MS_MANDLOCK) {
         Str_Strcat(mountEnt.mnt_opts, ",mand", MOUNT_OPTS_BUFFER_SIZE);
      }
      if (flags & MS_NOATIME) {
         Str_Strcat(mountEnt.mnt_opts, ",noatime", MOUNT_OPTS_BUFFER_SIZE);
      }
      if (flags & MS_NODIRATIME) {
         Str_Strcat(mountEnt.mnt_opts, ",nodiratime", MOUNT_OPTS_BUFFER_SIZE);
      }

      if (userName != NULL) {
         Str_Strcat(mountEnt.mnt_opts, ",user=", MOUNT_OPTS_BUFFER_SIZE);
         Str_Strcat(mountEnt.mnt_opts, userName, MOUNT_OPTS_BUFFER_SIZE);
      }

      ttlString = Str_Asprintf(NULL, "%u", mountInfo->ttl);
      if (ttlString != NULL) {
         Str_Strcat(mountEnt.mnt_opts, ",ttl=", MOUNT_OPTS_BUFFER_SIZE);
         Str_Strcat(mountEnt.mnt_opts, ttlString, MOUNT_OPTS_BUFFER_SIZE);
         free(ttlString);
      } else {
         printf("Could not allocate memory for ttl entry in mtab, "
                "continuing\n");
      }
   }

   /* Add the entry and close. */
   if (addmntent(mountFile, &mountEnt)) {
      printf("Could not add entry to mtab, continuing\n");
   }
   endmntent(mountFile);
   if (mountEnt.mnt_opts) {
      free(mountEnt.mnt_opts);
   }
}
#endif



/*-----------------------------------------------------------------------------
 *
 * main --
 *
 *    Main entry point. Parses the mount options received, makes a call to
 *    mount(2), and handles the results.
 *
 * Results:
 *    Zero on success, non-zero on failure.
 *
 * Side effects:
 *    May mount an HGFS filesystem.
 *
 *-----------------------------------------------------------------------------
 */

int
main(int argc,          // IN
     char *argv[])      // IN
{
#ifdef VM_HAVE_MTAB
   Bool doMtab = TRUE;
#endif
   char c;
   int i, result = EXIT_FAILURE, flags = 0, mntRes = -1;
   char *optionString = NULL;
   const char *shareNameHost, *shareNameDir;
   struct stat statBuf;
   HgfsMountInfo mountInfo;
   char *canonicalizedPath = NULL;
   size_t pathMax;

   thisProgram = argv[0];

   /* Compute the base name of the program, too. */
   thisProgramBase = strrchr(thisProgram, '/');
   if (thisProgramBase != NULL) {
      thisProgramBase++;
   } else {
      thisProgramBase = thisProgram;
   }
   setpwent();

   if (argc < 3) {
      PrintUsage();
   }

   while ((c = getopt(argc, argv, "hno:vV")) != -1) {
      switch (c) {
      case '?':
      case 'h':
         PrintUsage();
#ifdef VM_HAVE_MTAB
      case 'n':
         doMtab = FALSE;
         break;
#endif
      case 'o':
         optionString = strdup(optarg);
         if (optionString == NULL) {
            printf("Error: could not allocate memory for options\n");
            goto out;
         }
         break;
      case 'v':
         beVerbose = TRUE;
         break;
      case 'V':
         PrintVersion();
      default:
         printf("Error: unknown mount option %c\n", c);
         PrintUsage();
      }
   }

   LOG("Original command line: \"%s", thisProgram);
   for (i = 1; i < argc; i++) {
      LOG(" %s", argv[i]);
   }
   LOG("\"\n");

   /* After getopt_long(3), optind is the first non-option argument. */
   shareName = argv[optind];
   mountPoint = argv[optind + 1];

   /*
    * We canonicalize the mount point to avoid any discrepancies between the actual mount
    * point and the listed mount point in /etc/mtab (such discrepancies could prevent
    * umount(8) from removing the mount point from /etc/mtab).
    */
   pathMax = GetPathMax(mountPoint);
   canonicalizedPath = malloc(pathMax * sizeof *canonicalizedPath);
   if (canonicalizedPath == NULL) {
      printf("Error: cannot allocate memory for canonicalized path, "
             "aborting mount\n");
      goto out;
   } else if (!realpath(mountPoint, canonicalizedPath)) {
      perror("Error: cannot canonicalize mount point");
      goto out;
   }
   mountPoint = canonicalizedPath;

   if (!ParseShareName(shareName, &shareNameHost, &shareNameDir)) {
      printf("Error: share name is invalid, aborting mount\n");
      goto out;
   }

   mountInfo.magicNumber = HGFS_SUPER_MAGIC;
   mountInfo.version = HGFS_PROTOCOL_VERSION;
   mountInfo.fmask = 0;
   mountInfo.dmask = 0;
   mountInfo.uidSet = FALSE;
   mountInfo.gidSet = FALSE;
   mountInfo.ttl = HGFS_DEFAULT_TTL;
   mountInfo.shareNameHost = shareNameHost;
   mountInfo.shareNameDir = shareNameDir;

   /*
    * This'll write the rest of the options into HgfsMountInfo and possibly
    * modify the flags.
    */
   if (optionString && !ParseOptions(optionString, &mountInfo, &flags)) {
      printf("Error: could not parse options string\n");
      goto out;
   }

   /* Do some sanity checks on our desired mount point. */
   if (stat(mountPoint, &statBuf)) {
      perror("Error: cannot stat mount point");
      goto out;
   }
   if (S_ISDIR(statBuf.st_mode) == 0) {
      printf("Error: mount point \"%s\" is not a directory\n", mountPoint);
      goto out;
   }

   /*
    * Must be root in one flavor or another. If we're suid root, only proceed
    * if the user owns the mountpoint.
    */
   if (geteuid() != 0) {
      printf("Error: either you're not root, or %s isn't installed SUID\n",
             thisProgram);
      goto out;
   } else if (getuid() != 0 && (getuid() != statBuf.st_uid ||
                                (statBuf.st_mode & S_IRWXU) != S_IRWXU)) {
      printf("Error: for user mounts, user must own the mount point\n");
      goto out;
   }

   /* Go! */
#if defined(linux)
   mntRes = mount(shareName, mountPoint, HGFS_NAME, flags, &mountInfo);
#elif defined(__FreeBSD__)
   {
      struct iovec iov[] = {{"fstype", sizeof("fstype")},
                            {HGFS_NAME, sizeof(HGFS_NAME)},
                            {"target", sizeof("target")},
                            {shareName, strlen(shareName) + 1},
                            {"fspath", sizeof("fspath")},
                            {(void *)mountPoint, strlen(mountPoint) + 1}};

      mntRes = nmount(iov, ARRAYSIZE(iov), flags);
   }
#elif defined(__APPLE__)
   mntRes = mount(HGFS_NAME, mountPoint, flags, NULL);
#endif
   if (mntRes) {
      perror("Error: cannot mount filesystem");
      goto out;
   }
   result = EXIT_SUCCESS;

#ifdef VM_HAVE_MTAB
   if (doMtab) {
      UpdateMtab(&mountInfo, flags);
   }
#endif

  out:
   free(optionString);
   free(canonicalizedPath);
   return result;
}

