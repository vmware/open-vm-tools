/*********************************************************
 * Copyright (C) 2006-2019 VMware, Inc. All rights reserved.
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
#if defined(__linux__)
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
#  endif
#endif

#include <sys/mount.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef MOUNTED /* defined in mntent.h */
/*
 * If VM_HAVE_MTAB is set, hgfsmounter will update /etc/mtab. This is not necessary on
 * systems such as FreeBSD (and possibly Solaris) that don't have a mtab or the mntent.h
 * routines.
 */
#   define VM_HAVE_MTAB 1
#endif

#include "hgfsDevLinux.h"
#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "str.h"
#include "strutil.h"
#include "vm_version.h"
#include "hgfsmounter_version.h"

/* XXX embed_version.h does not currently support Mach-O binaries (OS X). */
#if defined(__linux__) || defined(__FreeBSD__)
#  include "vm_version.h"
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

typedef struct MountOptions {
   char *opt;           /* Option name */
   int flag;            /* Corresponding flag */
   Bool set;            /* Whether the flag should be set or reset */
   char *helpMsg;       /* Help message for the option */
   char *logMsg;        /* Log message to emit when option was detected */
   /* Special handler for more complex options */
   Bool (*handler)(const char *opt,
                   HgfsMountInfo *mountInfo, int *flags);
} MountOptions;

static char *thisProgram;
static char *thisProgramBase;
static char *shareName;
static char *mountPoint;
static Bool beVerbose = FALSE;

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
   const char *colon;
   const char *dir;

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
 *    or a uid and set up mountInfo accordingly.
 *
 * Results:
 *    TRUE on success, mountInfo modified appropriately, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
#ifndef sun
static Bool
ParseUid(const char *option,        // IN:  option string along with value
         HgfsMountInfo *mountInfo,  // OUT: mount data
         int *flags)                // OUT: mount flags
{
   Bool success = FALSE;
   unsigned int idx = 0;
   char *optString;
   char *uidString;
   uid_t myUid = 0;

   ASSERT(option);
   ASSERT(mountInfo);
   ASSERT(flags);

   /* Figure where value starts, we don't need result, just index. */
   optString = StrUtil_GetNextToken(&idx, option, "=");
   ASSERT(optString);
   uidString = StrUtil_GetNextToken(&idx, option, "=");
   if (!uidString) {
      LOG("Error getting the value for uid\n");
      goto out;
   }

   /*
    * The uid can be a direct value or a username which we must first
    * translate to its numeric value.
    */
   if (isdigit(*uidString)) {
      errno = 0;
      myUid = strtoul(uidString, NULL, 10);
      if (errno == 0) {
         success = TRUE;
      } else {
         printf("Bad UID value \"%s\"\n", uidString);
      }
   } else {
      struct passwd *pw;

      if (!(pw = getpwnam(uidString))) {
         printf("Bad user name \"%s\"\n", uidString);
      } else {
         myUid = pw->pw_uid;
         success = TRUE;
      }
      endpwent();
   }

   if (success) {
      mountInfo->uid = myUid;
      mountInfo->uidSet = TRUE;
      LOG("Setting mount owner to %"FMTUID"\n", myUid);
   }

   free(uidString);
out:
   free(optString);
   return success;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * ParseGid
 *
 *    A helper function to process a string containing either a group name
 *    or a gid and set up mountInfo accordingly.
 *
 * Results:
 *    TRUE on success, mountInfo modified appropriately, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
#ifndef sun
static Bool
ParseGid(const char *option,        // IN:  option string along with value
         HgfsMountInfo *mountInfo,  // OUT: mount data
         int *flags)                // OUT: mount flags
{
   Bool success = FALSE;
   unsigned int idx = 0;
   char *optString;
   char *gidString;
   gid_t myGid = 0;

   ASSERT(option);
   ASSERT(mountInfo);
   ASSERT(flags);

   /* Figure where value starts, we don't need result, just index. */
   optString = StrUtil_GetNextToken(&idx, option, "=");
   ASSERT(optString);
   gidString = StrUtil_GetNextToken(&idx, option, "=");
   if (!gidString) {
      LOG("Error getting the value for gid\n");
      goto out;
   }

   /*
    * The gid can be a direct value or a group name which we must first
    * translate to its numeric value.
    */
   if (isdigit(*gidString)) {
      errno = 0;
      myGid = strtoul(gidString, NULL, 10);
      if (errno == 0) {
         success = TRUE;
      } else {
         printf("Bad GID value \"%s\"\n", gidString);
      }
   } else {
      struct group *gr;

      if (!(gr = getgrnam(gidString))) {
         printf("Bad group name \"%s\"\n", gidString);
      } else {
         myGid = gr->gr_gid;
         success = TRUE;
      }
      endpwent();
   }

   if (success) {
      mountInfo->gid = myGid;
      mountInfo->gidSet = TRUE;
      LOG("Setting mount group to %"FMTUID"\n", myGid);
   }

   free(gidString);
out:
   free(optString);
   return success;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * ParseMask
 *
 *    A helper function to parse a string containing File/Directory
 *    mask value.
 *
 * Results:
 *    TRUE on success,  along with the mask, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
#ifndef sun
static Bool
ParseMask(const char *option,         // IN:  option string along with value
          unsigned short *pmask)      // OUT: parsed mask
{
   Bool success = FALSE;
   unsigned int idx = 0;
   char *optString;
   char *maskString;
   unsigned short mask;

   optString = StrUtil_GetNextToken(&idx, option, "=");
   ASSERT(optString);
   maskString = StrUtil_GetNextToken(&idx, option, "=");
   if (!maskString) {
      LOG("Error getting the value for %s\n", optString);
      goto out;
   }

   /*
    * The way to check for an overflow in strtol(3), according to its
    * man page.
    */
   errno = 0;
   mask = strtol(maskString, NULL, 8);
   if (errno == 0) {
      *pmask = mask;
      success = TRUE;
   } else {
      LOG("Error, overflow in %s\n", optString);
   }

   free(maskString);
out:
   free(optString);
   return success;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ParseFmask
 *
 *    A helper function to process a string containing File mask value.
 *
 * Results:
 *    TRUE on success, mountInfo modified appropriately, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
#ifndef sun
static Bool
ParseFmask(const char *option,         // IN:  option string along with value
           HgfsMountInfo *mountInfo,   // OUT: mount data
           int *flags)                 // OUT: mount flags
{
   unsigned short fmask = 0;
   ASSERT(option);
   ASSERT(mountInfo);

   if (ParseMask(option, &fmask)) {
      LOG("Setting mount fmask to %o\n", fmask);
      mountInfo->fmask = fmask;
      return TRUE;
   }

   return FALSE;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ParseDmask
 *
 *    A helper function to process a string containing dmask value.
 *
 * Results:
 *    TRUE on success, mountInfo modified appropriately, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
#ifndef sun
static Bool
ParseDmask(const char *option,         // IN:  option string along with value
           HgfsMountInfo *mountInfo,   // OUT: mount data
           int *flags)                 // OUT: mount flags
{
   unsigned short dmask = 0;
   ASSERT(option);
   ASSERT(mountInfo);

   if (ParseMask(option, &dmask)) {
      LOG("Setting mount dmask to %o\n", dmask);
      mountInfo->dmask = dmask;
      return TRUE;
   }

   return FALSE;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ParseTtl
 *
 *    A helper function to process a string containing TTL value.
 *
 * Results:
 *    TRUE on success, mountInfo modified appropriately, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */
#ifndef sun
static Bool
ParseTtl(const char *option,        // IN:  option string along with value
         HgfsMountInfo *mountInfo,  // OUT: mount data
         int *flags)                // OUT: mount flags
{
   Bool success = FALSE;
   unsigned int idx = 0;
   char *optString;
   int32 ttl;

   ASSERT(option);
   ASSERT(mountInfo);
   ASSERT(flags);

   /* Figure where value starts, we don't need result, just index. */
   optString = StrUtil_GetNextToken(&idx, option, "=");
   ASSERT(optString);

   if (StrUtil_GetNextIntToken(&ttl, &idx, option, "=") && ttl >= 0) {
      LOG("Setting maximum attribute TTL to %u\n", ttl);
      mountInfo->ttl = ttl;
      success = TRUE;
   } else {
      LOG("Error getting the value for ttl\n");
   }

   free(optString);
   return success;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ParseServerIno --
 *
 *    A helper function to process a string containing serverino value.
 *
 * Results:
 *    TRUE always success.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ParseServerIno(const char *option,         // IN:  option string along with value
               HgfsMountInfo *mountInfo,   // OUT: mount data
               int *flags)                 // OUT: mount flags unused
{
   ASSERT(option);
   ASSERT(mountInfo);

   mountInfo->flags |= HGFS_MNTINFO_SERVER_INO;
   LOG("Setting mount flag server ino in 0x%x\n", mountInfo->flags);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ParseNoServerIno --
 *
 *    A helper function to process a string containing noserverino value.
 *
 * Results:
 *    TRUE always success.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ParseNoServerIno(const char *option,         // IN:  option string along with value
                 HgfsMountInfo *mountInfo,   // OUT: mount data
                 int *flags)                 // OUT: mount flags unused
{
   ASSERT(option);
   ASSERT(mountInfo);

   mountInfo->flags &= ~HGFS_MNTINFO_SERVER_INO;
   LOG("Clearing mount flag server ino in 0x%x\n", mountInfo->flags);
   return TRUE;
}


static MountOptions mountOptions[] = {
   { "ro", MS_RDONLY, TRUE, "mount read-only",
      "Setting mount read-only", NULL },
   { "rw", MS_RDONLY, FALSE, "mount read-write",
      "Setting mount read-write", NULL },
   { "nosuid", MS_NOSUID, TRUE, "ignore suid/sgid bits",
      "Setting mount option for allowing suid/sgid bits off", NULL },
   { "suid", MS_NOSUID, FALSE, "allow suid/sgid bits (default)",
      "Setting mount option for allowing suid/sgid bits on", NULL },

#ifndef sun /* Solaris does not have any of these options */

   { "uid=<arg>", 0, TRUE, "mount owner (by uid or username)",
      NULL, ParseUid },
   { "gid=<arg>", 0, TRUE, "mount group (by gid or groupname)",
      NULL, ParseGid },
   { "fmask=<arg>", 0, TRUE, "file umask (in octal)",
      NULL, ParseFmask },
   { "dmask=<arg>", 0, TRUE, "directory umask (in octal)",
      NULL, ParseDmask },
#ifdef MS_NODEV
   { "nodev", MS_NODEV, TRUE, "prevent device node access",
      "Setting mount option for accessing device nodes off", NULL },
   { "dev", MS_NODEV, FALSE, "allow device node access (default)",
      "Setting mount option for accessing device nodes on", NULL },
#endif
   { "noexec", MS_NOEXEC, TRUE, "prevent program execution",
      "Setting mount option for program execution off", NULL },
   { "exec", MS_NOEXEC, FALSE, "allow program execution (default)",
      "Setting mount option for program execution on", NULL },
   { "sync", MS_SYNCHRONOUS, TRUE, "file writes are synchronous",
      "Setting mount synchronous writes", NULL },
   { "async", MS_SYNCHRONOUS, FALSE, "file writes are asynchronous (default)",
      "Setting mount synchronous writes", NULL },
   { "mand", MS_MANDLOCK, TRUE, "allow mandatory locks",
      "Setting mount option for allow mandatory locks on", NULL },
   { "nomand", MS_MANDLOCK, FALSE, "prevent mandatory locks (default)",
      "Setting mount option for allow mandatory locks off", NULL },
   { "noatime", MS_NOATIME, TRUE, "do not update access times",
      "Setting mount option for updating access times off", NULL },
   { "atime", MS_NOATIME, FALSE, "update access times (default)",
      "Setting mount option for updating access times on", NULL },
   { "nodiratime", MS_NOATIME, TRUE, "do not update directory access times",
      "Setting mount option for updating directory access times off", NULL },
   { "diratime", MS_NOATIME, FALSE, "update access directory times (default)",
      "Setting mount option for updating directory access times on", NULL },
   { "ttl=<arg>", 0, TRUE,
      "time before file attributes must be\n"
      "revalidated (in seconds). Improves\n"
      "performance but decreases coherency.\n"
      "Defaults to 1 if not set.\n",
      NULL, ParseTtl },
   { "bind", MS_BIND, TRUE, "perform bind mount",
      "Setting mount type to bind", NULL },
   { "bind", MS_MOVE, TRUE, "move an existig mount point",
      "Setting mount type to move", NULL },
#endif
   { "serverino", 0, TRUE,
      "Use server generated inode numbers.\n",
      "Setting mount option for using Server inode numbers on", ParseServerIno },
   { "noserverino", 0, FALSE,
      "Use client generated inode numbers.\n",
      "Setting mount option for using Server inode numbers off", ParseNoServerIno },

   { "remount", MS_REMOUNT, TRUE, "remount already mounted filesystem",
      "Setting mount type to remount", NULL }
};

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
   Bool success = FALSE;
   char *key = NULL;
   char *keyVal = NULL;
   int i;

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

      for (i = 0; i < ARRAYSIZE(mountOptions); i++) {
         Bool match = FALSE;
         unsigned int idx = 0;
         char *optName = StrUtil_GetNextToken(&idx, mountOptions[i].opt, "=");

         if (!optName) {
            printf("Failed to parse option name, out of memory?\n");
            goto out;
         }

         match = strcmp(key, optName) == 0;
         free(optName);

         if (match) {
            if (mountOptions[i].handler) {
               if (!mountOptions[i].handler(keyVal, mountInfo, flags)) {
                  goto out;
               }
            } else {
               if (mountOptions[i].set) {
                  *flags |= mountOptions[i].flag;
               } else {
                  *flags &= ~mountOptions[i].flag;
               }
               LOG("%s\n", mountOptions[i].logMsg);
            }
            break;
         }
      }

      if (i == ARRAYSIZE(mountOptions)) {
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
   int i;

   printf("Usage: %s <sharename> <dir> [-o <options>]\n", thisProgramBase);
   printf("Mount the HGFS share, specified by name, to a local directory.\n");
   printf("Share name must be in host:dir format.\n\nOptions:\n");

   for (i = 0; i < ARRAYSIZE(mountOptions); i++) {
      Bool printOptName = TRUE;
      unsigned int tokidx = 0;
      char *msg;

      while ((msg = StrUtil_GetNextToken(&tokidx,
                                         mountOptions[i].helpMsg,
                                         "\n")) != NULL) {
         printf("  %-15s       %s\n",
                printOptName ? mountOptions[i].opt : "", msg);
         free(msg);
         printOptName = FALSE;
      }
   }

   printf("\n");
   printf("This command is intended to be run from within /bin/mount by\n");
   printf("passing the option '-t %s'. For example:\n", HGFS_NAME);
   printf("  mount -t %s .host:/ /mnt/hgfs/\n", HGFS_NAME);
   printf("  mount -t %s .host:/foo /mnt/foo\n", HGFS_NAME);
   printf("  mount -t %s .host:/foo/bar /var/lib/bar\n", HGFS_NAME);
   exit(EXIT_FAILURE);
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
   int i;
   int result = EXIT_FAILURE;
   int flags = 0;
   int mntRes = -1;
   char *optionString = NULL;
   const char *shareNameHost = NULL;
   const char *shareNameDir = NULL;
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
         if (optionString == NULL) {
            optionString = strdup(optarg);
         } else {
            size_t newLength;
            char *savedString = optionString;
            newLength = strlen(optionString) + strlen(",") +
                        strlen(optarg) + sizeof '\0';
            optionString = realloc(optionString, newLength);
            if (optionString != NULL) {
               Str_Strcat(optionString, ",", newLength);
               Str_Strcat(optionString, optarg, newLength);
            } else {
               free(savedString);
            }
         }
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
   mountInfo.infoSize = sizeof mountInfo;
   mountInfo.version = HGFS_MOUNTINFO_VERSION_2;

#ifndef sun
   mountInfo.fmask = 0;
   mountInfo.dmask = 0;
   mountInfo.uidSet = FALSE;
   mountInfo.gidSet = FALSE;
   mountInfo.ttl = HGFS_DEFAULT_TTL;
#if defined(__APPLE__)
   strlcpy(mountInfo.shareNameHost, shareNameHost, MAXPATHLEN);
   strlcpy(mountInfo.shareNameDir, shareNameDir, MAXPATHLEN);
#else
   mountInfo.shareNameHost = shareNameHost;
   mountInfo.shareNameDir = shareNameDir;
#endif
#endif
   /*
    * Default flags which maybe modified by user passed options.
    */
   mountInfo.flags = HGFS_MNTINFO_SERVER_INO;

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
#if defined(__linux__)
   mntRes = mount(shareName, mountPoint, HGFS_NAME, flags, &mountInfo);
#elif defined(__FreeBSD__)
   {
      struct iovec iov[] = {{"fstype", sizeof("fstype")},
                            {HGFS_NAME, sizeof(HGFS_NAME)},
                            {"target", sizeof("target")},
                            {shareName, strlen(shareName) + 1},
                            {"fspath", sizeof("fspath")},
                            {(void *)mountPoint, strlen(mountPoint) + 1},
                            {"uidSet", sizeof("uidSet")},
                            {&mountInfo.uidSet, sizeof(mountInfo.uidSet)},
                            {"uid", sizeof("uid")},
                            {&mountInfo.uid, sizeof(mountInfo.uid)},
                            {"gidSet", sizeof("gidSet")},
                            {&mountInfo.gidSet, sizeof(mountInfo.gidSet)},
                            {"gid", sizeof("gid")},
                            {&mountInfo.gid, sizeof(mountInfo.gid)}};

      mntRes = nmount(iov, ARRAYSIZE(iov), flags);
   }
#elif defined(__APPLE__)
   mntRes = mount(HGFS_NAME, mountPoint, flags, &mountInfo);
#elif defined(sun)
   mntRes = mount(mountPoint, mountPoint, MS_DATA | flags, HGFS_NAME,
                  &mountInfo, sizeof mountInfo);
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

