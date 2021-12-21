/*********************************************************
 * Copyright (C) 2015-2018,2021 VMware, Inc. All rights reserved.
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
 * config.c --
 *
 */

#include "module.h"
#include <fuse_lowlevel.h>
#include <sys/utsname.h>

#ifdef VMX86_DEVEL
int LOGLEVEL_THRESHOLD = 4;
#endif

/* Kernel version as integer macro */
#define KERNEL_RELEASE(x, y, z) (((x) << 16) | ((y) << 8) | (z))

/*
 * System state for eligibility for HGFS FUSE to be enabled.
 * See comments in the Usage function which must match the enum
 * and messages table below.
 */

#define HGFS_DEFS \
   DEFINE_HFGS_DEF(HGFS_SYSCOMPAT_FUSE_ENABLED,     "HGFS FUSE client enabled") \
   DEFINE_HFGS_DEF(HGFS_SYSCOMPAT_OS_NOT_SUPPORTED, "HGFS FUSE client not supported for this OS version") \
   DEFINE_HFGS_DEF(HGFS_SYSCOMPAT_OS_NO_FUSE,       "HGFS FUSE client needs FUSE environment")

#undef DEFINE_HFGS_DEF

#define DEFINE_HFGS_DEF(a, b) a,

typedef enum {
   HGFS_DEFS
} HgfsSystemCompatibility;

#undef DEFINE_HFGS_DEF

#define DEFINE_HFGS_DEF(a, b) b,

const char *HgfsSystemCompatibilityMsg[] = {
   HGFS_DEFS
};

#undef DEFINE_HFGS_DEF

enum {
   KEY_HELP,
   KEY_VERSION,
   KEY_BIG_WRITES,
   KEY_NO_BIG_WRITES,
   KEY_ENABLED_FUSE,
};

#define VMHGFS_OPT(t, p, v) { t, offsetof(struct vmhgfsConfig, p), v }

const struct fuse_opt vmhgfsOpts[] = {
#ifdef VMX86_DEVEL
     VMHGFS_OPT("--loglevel %i",    logLevel, 4),
     VMHGFS_OPT("-l %i",            logLevel, 4),
#endif
     /* We will change the default value, unless it is specified explicitly. */
#if FUSE_MAJOR_VERSION != 3
     FUSE_OPT_KEY("big_writes",     KEY_BIG_WRITES),
     FUSE_OPT_KEY("nobig_writes",   KEY_NO_BIG_WRITES),
#endif

     FUSE_OPT_KEY("-V",             KEY_VERSION),
     FUSE_OPT_KEY("--version",      KEY_VERSION),
     FUSE_OPT_KEY("-h",             KEY_HELP),
     FUSE_OPT_KEY("--help",         KEY_HELP),
     FUSE_OPT_KEY("-e",             KEY_ENABLED_FUSE),
     FUSE_OPT_KEY("--enabled",      KEY_ENABLED_FUSE),
     FUSE_OPT_END
};


/*
 *----------------------------------------------------------------------
 *
 * Usage
 *
 *    Show Usage.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static void
Usage(char *prog_name)  // IN
{
   fprintf(stderr,
           "Usage:\t%s sharedir mountpoint [options]\nExamples:\n"
           "\t%s .host:/ /mnt/hgfs\n"
           "\t%s .host:/foo/bar /mnt/bar\n\n"
           "general options:\n"
           "    -o opt,[opt...]        mount options\n"
           "    -h   --help            print help\n"
           "    -V   --version         print version\n"
           "    -e   --enabled         check if system is enabled\n"
           "                           for the HGFS FUSE client. Exits with:\n"
           "                           0 - system is enabled for HGFS FUSE\n"
           "                           1 - system OS version is not supported for HGFS FUSE\n"
           "                           2 - system needs FUSE packages for HGFS FUSE\n"
           "\n"
#ifdef VMX86_DEVEL
           "vmhgfs options:\n"
           "    -l   --loglevel NUM    set loglevel=NUM only available in debug build.\n"
           "\n"
#endif
           , prog_name, prog_name, prog_name);
}

#define LIB_MODULEPATH         "/lib/modules"
#define MODULES_DEP            "modules.dep"
#if FUSE_MAJOR_VERSION == 3
#define FUSER_MOUNT_BIN        "/bin/fusermount3"
#define FUSER_MOUNT_USR_BIN    "/usr/bin/fusermount3"
#else
#define FUSER_MOUNT_BIN        "/bin/fusermount"
#define FUSER_MOUNT_USR_BIN    "/usr/bin/fusermount"
#endif
#define PROC_FILESYSTEMS       "/proc/filesystems"
#define FUSER_KERNEL_FS        "fuse"

/*
 *----------------------------------------------------------------------
 *
 * SysCompatFusermountCheck
 *
 *    Test if the FUSE fusermount command is installed.
 *
 * Results:
 *    FALSE the HGFS FUSE client is not installed, TRUE if it is installed.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static Bool
SysCompatFusermountCheck(void)  // IN:
{
   Bool fusermountExists = TRUE;
   int accessResult;

   /*
    * Check both locations as it varies e.g. SUSE uses "/usr/bin"
    * and Ubuntu uses "/bin".
    */
   accessResult = access(FUSER_MOUNT_BIN, F_OK);
   if (accessResult == -1) {
      accessResult = access(FUSER_MOUNT_USR_BIN, F_OK);
      if (accessResult == -1) {
         fprintf(stderr, "failed to access %s or %s %d\n",
                 FUSER_MOUNT_BIN, FUSER_MOUNT_USR_BIN, errno);
         fusermountExists = FALSE;
      }
   }

   return fusermountExists;
}


/*
 *----------------------------------------------------------------------
 *
 * SysCompatIsInstalledFuse
 *
 *    Test if the FUSE file system is installed but not yet loaded.
 *
 * Results:
 *    FALSE the HGFS FUSE client is not installed, TRUE if it is installed.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static Bool
SysCompatIsInstalledFuse(char *utsRelease)  // IN: kernel release
{
   char *fuseFilesystem;
   char *modulesDep;
   char *modulesDepData = NULL;
   struct stat modulesDepStat;
   size_t modulesDepLen;
   size_t modulesDepDataSize;
   size_t modulesDepFileSize;
   int modulesDepFd = -1;
   Bool installedFuse = FALSE;
   int statResult;

   modulesDep = Str_Asprintf(&modulesDepLen, "%s/%s/%s",
                             LIB_MODULEPATH, utsRelease, MODULES_DEP);

   if (modulesDep == NULL) {
      fprintf(stderr, "failed to create path str for %s\n", MODULES_DEP);
      goto exit;
   }

   modulesDepFd = open(modulesDep, O_RDONLY);
   if (modulesDepFd == -1) {
      fprintf(stderr, "failed to open %s %d\n", modulesDep, errno);
      goto exit;
   }

   statResult = fstat(modulesDepFd, &modulesDepStat);
   if (statResult == -1) {
      fprintf(stderr, "failed to stat %s %d\n", modulesDep, errno);
      goto exit;
   }
   modulesDepFileSize = (size_t)modulesDepStat.st_size;
   modulesDepData = malloc(modulesDepFileSize + 1);
   if (modulesDepData == NULL) {
      fprintf(stderr, "failed to alloc data buf for %s\n", modulesDep);
      goto exit;
   }

   modulesDepDataSize = read(modulesDepFd,
                             modulesDepData,
                             modulesDepFileSize);

   if (modulesDepDataSize != modulesDepFileSize) {
      fprintf(stderr, "failed to read %s %d\n", modulesDep, errno);
      goto exit;
   }

   modulesDepData[modulesDepDataSize] = '\0';
   fuseFilesystem = strstr(modulesDepData,
                           FUSER_KERNEL_FS);

   installedFuse = (fuseFilesystem != NULL);

exit:
   if (modulesDepFd != -1) {
      close(modulesDepFd);
   }
   free(modulesDepData);
   free(modulesDep);
   return installedFuse;
}


/*
 *----------------------------------------------------------------------
 *
 * SysCompatIsRegisteredFuse
 *
 *    Test if the FUSE file system is registered.
 *
 * Results:
 *    FALSE the FUSE is not registered, TRUE if it is.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static Bool
SysCompatIsRegisteredFuse(void)  // IN
{
   Bool registeredFuse = FALSE;
   int procFilesystemsFd;
   char procFilesystems[4096];
   char *fuseFilesystem;
   ssize_t procFilesystemsDataSize;

   procFilesystemsFd = open(PROC_FILESYSTEMS, O_RDONLY);
   if (procFilesystemsFd == -1) {
      fprintf(stderr, "failed to open %s %d\n", PROC_FILESYSTEMS, errno);
      goto exit;
   }

   procFilesystemsDataSize = read(procFilesystemsFd,
                                  procFilesystems,
                                  sizeof procFilesystems - 1);

   if (procFilesystemsDataSize < sizeof FUSER_KERNEL_FS) {
      fprintf(stderr, "failed to read %s %d\n", PROC_FILESYSTEMS, errno);
      goto exit;
   }

   procFilesystems[procFilesystemsDataSize] = '\0';
   fuseFilesystem = strstr(procFilesystems,
                           FUSER_KERNEL_FS);


   registeredFuse = (fuseFilesystem != NULL);

exit:
   if (procFilesystemsFd != -1) {
      close(procFilesystemsFd);
   }
   return registeredFuse;
}


/*
 *----------------------------------------------------------------------
 *
 * SysCompatCheck
 *
 *    Check if the system is compatible for FUSE.
 *
 * Results:
 *    0 the system compatible so FUSE is enabled,
 *    1 the system OS version is not supported (disabled)
 *    2 the system does not have FUSE installed.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static HgfsSystemCompatibility
SysCompatCheck(char *prog_name)  // IN
{
   struct utsname buf;
   HgfsSystemCompatibility sysCompat = HGFS_SYSCOMPAT_OS_NOT_SUPPORTED;
   char *utsRelease = NULL;
   int k[3] = {0};
   int release;

   if (uname(&buf) == -1) {
      fprintf(stderr, "%s: failed to retrieve kernel info %d\n", __func__, errno);
      goto exit;
   }

   utsRelease = strdup(buf.release);

   if (sscanf(utsRelease, "%d.%d.%d", &k[0], &k[1], &k[2]) == 0) {
      fprintf(stderr, "%s: failed to extract kernel release\n", __func__);
      goto exit;
   } else {
      release = KERNEL_RELEASE(k[0], k[1], k[2]);
   }

   if (release < KERNEL_RELEASE(3, 10, 0)) {
      fprintf(stderr, "%s: incompatible kernel version %02d.%02d.%02d\n",
              __func__, k[0], k[1], k[2]);
      goto exit;
   }

   if (!SysCompatIsRegisteredFuse()) {
      /* Check if FUSE is installed but not loaded yet. */
      if (!SysCompatIsInstalledFuse(utsRelease)) {
         fprintf(stderr, "%s: failed FUSE install checks\n", __func__);
         sysCompat = HGFS_SYSCOMPAT_OS_NO_FUSE;
         goto exit;
      }
   }

   /*
    * Finally check the system paths to see if the user has the
    * needed fusemount binary installed.
    */
   if (!SysCompatFusermountCheck()) {
      sysCompat = HGFS_SYSCOMPAT_OS_NO_FUSE;
      goto exit;
   }

   sysCompat = HGFS_SYSCOMPAT_FUSE_ENABLED;

exit:
   free(utsRelease);
   fprintf(stderr, "%s: %d - %s\n", prog_name, sysCompat,
           HgfsSystemCompatibilityMsg[sysCompat]);
   return sysCompat;
}


/*
 *----------------------------------------------------------------------
 *
 * vmhgfsOptProc
 *
 *    Process options the fuse way. check the fuse document for details.
 *
 * Results:
 *    -1 on error, 0 if arg is to be discarded, 1 if arg should be kept
 *
 * Side effects:
 *    In KEY_HELP or KEY_VERSION cases, they will be processed and
 *    the process will exit immediately.
 *
 *----------------------------------------------------------------------
 */

int
vmhgfsOptProc(void *data,                // IN
              const char *arg,           // IN
              int key,                   // IN
              struct fuse_args *outargs) // OUT
{
#if FUSE_MAJOR_VERSION != 3
   struct vmhgfsConfig *config = data;
#endif

   switch (key) {
   case FUSE_OPT_KEY_NONOPT:
      if (!gState->basePath &&
         strncmp(arg, HOSTNAME_PREFIX, strlen(HOSTNAME_PREFIX)) == 0) {
         const char *p = arg + strlen(HOSTNAME_PREFIX);
         char *q;

         /*
         * remove hostname and trailing spaces. e.g.
         * ".host:/" => ""
         * ".host:/abc/" => "/abc"
         */
         gState->basePath = strdup(p);
         q = gState->basePath + strlen(gState->basePath) - 1;
         if (*q == '/' && q >= gState->basePath) {
            *q = '\0';
         }
         gState->basePathLen = strlen(gState->basePath);
         if (gState->basePathLen == 0) {
            free(gState->basePath);
            gState->basePath = NULL;
         }
         return 0;
      }
      return 1;

#if FUSE_MAJOR_VERSION != 3
   case KEY_BIG_WRITES:
      config->addBigWrites = TRUE;
      return 0;

   case KEY_NO_BIG_WRITES:
      config->addBigWrites = FALSE;
      return 0;
#endif

   case KEY_HELP:
      Usage(outargs->argv[0]);
#if FUSE_MAJOR_VERSION != 3
      fuse_opt_add_arg(outargs, "-ho");
      fuse_main(outargs->argc, outargs->argv, NULL, NULL);
#else
      fprintf(stdout, "FUSE options:\n");
      fuse_cmdline_help();
      fuse_lib_help(outargs);
#endif
      exit(1);

   case KEY_ENABLED_FUSE: {
      HgfsSystemCompatibility sysCompat;
      sysCompat = SysCompatCheck(outargs->argv[0]);
      exit(sysCompat);
   }

   case KEY_VERSION:
      fprintf(stderr, "%s: version %s\n\n", outargs->argv[0],
              VMHGFS_DRIVER_VERSION_STRING);
      fuse_opt_add_arg(outargs, "--version");
      fuse_main(outargs->argc, outargs->argv, NULL, NULL);
      exit(0);
   }
   return 1;
}


/*
 *----------------------------------------------------------------------
 *
 * vmhgfsPreprocessArgs
 *
 *    Process arguments we care about before passing them on to fuse.
 *
 * Results:
 *    Returns -1 on error, 0 on success.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
vmhgfsPreprocessArgs(struct fuse_args *outargs)    // IN/OUT
{
   struct vmhgfsConfig config;
   int res;

   gState->basePath = NULL;
   gState->basePathLen = 0;

   VMTools_LoadConfig(NULL, G_KEY_FILE_NONE, &gState->conf, NULL);
   VMTools_ConfigLogging(G_LOG_DOMAIN, gState->conf, FALSE, FALSE);

#ifdef VMX86_DEVEL
   config.logLevel = LOGLEVEL_THRESHOLD;
#endif
#if defined(__APPLE__) || FUSE_MAJOR_VERSION == 3
   /* osxfuse and fuse3 does not have option 'big_writes'. */
   config.addBigWrites = FALSE;
#else
   config.addBigWrites = TRUE;
#endif

   res = fuse_opt_parse(outargs, &config, vmhgfsOpts, vmhgfsOptProc);
   if (res != 0) {
      goto exit;
   }

#ifdef VMX86_DEVEL
   LOGLEVEL_THRESHOLD = config.logLevel;
#endif
   /* Default option changes for vmhgfs fuse client. */
   if (config.addBigWrites) {
      res = fuse_opt_add_arg(outargs, "-obig_writes");
      if (res != 0) {
         goto exit;
      }
   }

exit:
   return res;
}
