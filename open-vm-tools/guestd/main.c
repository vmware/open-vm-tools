/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * main.c --
 *
 *    Guest application started as a service
 *    Linux and FreeBSD implementation
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#if !defined(__FreeBSD__) && !defined(sun)
#include <utmp.h>
#include <pwd.h>
#include <grp.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif


#include "vmware.h"
#include "toolsDaemon.h"
#include "eventManager.h"
#include "rpcout.h"
#include "rpcin.h"
#include "vm_app.h"
#include "debug.h"
#include "vmsignal.h"
#include "guestApp.h"
#include "vmcheck.h"
#include "util.h"
#include "strutil.h"
#include "str.h"
#include "vm_version.h"
#include "procMgr.h"
#include "system.h"
#include "conf.h"
#include "guestInfo.h"
#include "guestInfoServer.h"
#include "escape.h"
#include "vmstdio.h"
#include "vmBackup.h"
#include "codeset.h"

#if !defined(__FreeBSD__) && !defined(sun)
#include "socketMgr.h"
#include "dnd.h"
#endif

/* Header to get prototype of daemon() implementation for Solaris. */
#ifdef sun
#   include "miscSolaris.h"
#endif

#if defined(sun)
#   define VM_GUESTD_MOUNTS_HGFS 1
#   define VM_GUESTD_RUNS_HGFS_PSERVER 1
#endif


/* Headers needed by the hgfs pserver */
#if defined(VM_GUESTD_MOUNTS_HGFS)
#   if defined(sun)
#      include <sys/param.h>
#      include <sys/mount.h>
#      include <sys/stat.h>
#      include <fcntl.h>
#   endif
#endif
#if defined(VM_GUESTD_RUNS_HGFS_PSERVER)
#   include "hgfsDevLinux.h"
#   include "hgfsBd.h"
#   include "hgfsProto.h"
#   include "hgfs.h"
#endif
#include "procMgr.h"
#include "guestd_version.h"

#include "embed_version.h"
VM_EMBED_VERSION(GUESTD_VERSION_STRING);

/*
 * Global constants
 */

#define DEFAULT_PIDFILE             "/var/run/vmware-guestd.pid"
#define EXEC_LOG                    "/var/log/vmware-tools-guestd"
#define UPGRADER_FILENAME           "vmware-tools-upgrader"

#if defined(VM_GUESTD_MOUNTS_HGFS)
 /*
  * The Hgfs device is in /dev.  Solaris' umount program doesn't
  * unmount filesystem's with name's longer than 8 characters, so we shorten
  * it.  Solaris' mount(2) doesn't take NULL as the mount source, so we
  * specify anything to get past the Kernel's VFS mount function.  We also
  * need to specify MS_DATA so it looks for the optional data we pass in.
  */
#   define HGFS_FS_NAME           "vmhgfs"
#   define HGFS_MOUNT_SRC         "/hgfs"
#   if defined(sun)
#      define HGFS_DEVICE            "/dev/vmware-hgfs"
#      define HGFS_MOUNT_FLAGS       MS_DATA			/* from <sys/mount.h> */
#   endif
#endif

#if !defined(__FreeBSD__) && !defined(sun)
#define VMWAREUSER_CHECK_TIME          1000 /* 10 seconds */
#define XAUTOSTART_CONF                "xautostart.conf"
#define NR_XBLESSED_APP_DEFAULT        2
/* Default blessed app for gnome. */
#define XBLESSED_APP1_DEFAULT          "gnome-panel"
/*
 * Default blessed app for KDE. Only a few apps can be chosen, because most
 * X apps, like kicker, are loaded by kdeinit with empty environ. ksmserver
 * is KDE session manager and has environ.
 */
#define XBLESSED_APP2_DEFAULT          "ksmserver"
#endif
/*
 * All signals that:
 * . Can terminate the process
 * . May occur even if the program has no bugs
 */
static int const cSignals[] = {
   SIGHUP,
   SIGINT,
   SIGQUIT,
   SIGTERM,
   SIGUSR1,
   SIGUSR2,
};


/*
 * Global variables
 */

#if !defined(__FreeBSD__) && !defined(sun)
static const char *gBinDir;
static char *gDefaultBlessedApp[] = {
   XBLESSED_APP1_DEFAULT,
   XBLESSED_APP2_DEFAULT,
};
typedef struct blessedAppNode {
   char *name;
   struct blessedAppNode *next;
} blessedAppNode;
#endif

/*
 * What a pity that the signal API doesn't allow to pass a clientData
 * parameter :( --hpreg
 */
static int gDaemonSignal;
static int gCommandLineRpciSignal;

#ifdef VM_GUESTD_MOUNTS_HGFS
typedef struct HgfsMountState {
   const char *mountPoint; /* Mount point for hgfs filesystem */
   Bool mounted;           /* TRUE if mounted, FALSE if not */
} HgfsMountState;
#endif

#ifdef VM_GUESTD_RUNS_HGFS_PSERVER
/* Hgfs state needed per mounted filesystem */
typedef struct PserverState {
   HgfsMountState mountState;
   char *requestPacket;    /* Buffer for request packets */
   RpcOut *out;            /* Backdoor communications channel */
   int fd;                 /* Hgfs device fd (communication with kernel) */
} PserverState;

static PserverState globalHgfsState = {
   {NULL, FALSE},  /* mountState */
   NULL,           /* requestPacket */
   NULL,           /* out */
   -1              /* fd */
};
#elif defined(VM_GUESTD_MOUNTS_HGFS)
static HgfsMountState globalHgfsState = {
   NULL, /* mountPoint */
   FALSE /* mounted */
};
#endif

#ifdef VM_GUESTD_MOUNTS_HGFS
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsMountState_Cleanup --
 *
 *      Unmounts the hgfs filesystem
 *
 * Results:
 *      TRUE on success.
 *      FALSE if one or more steps failed.
 *
 * Side effects:
 *      Unmounts the filesystem and cleans up the HgfsMountState structure.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsMountState_Cleanup(HgfsMountState *state) // IN: Hgfs mount state
{
   Bool success = TRUE;
   int result;

   ASSERT(state);

   /* Try to unmount hgfs */
   if (state->mounted) {
      Debug("HgfsMountState_Cleanup: attempting to unmount %s\n",
            state->mountPoint);
#if defined(sun)
      result = umount(state->mountPoint);
#endif
      if (result < 0) {
         int error = errno;
         Debug("HgfsMountState_Cleanup: error %d (%s) unmounting filesystem\n",
               error,
               strerror(error));
         success = FALSE;
      } else {
         state->mounted = FALSE;
         Debug("HgfsMountState_Cleanup: unmount succeeded\n");
      }
   }
   if (state->mountPoint) {
      free((void *)state->mountPoint);
      state->mountPoint = NULL;
   }

   return success;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsMountState_Init --
 *
 *      Mounts the hgfs filesystem
 *
 * Results:
 *      TRUE on success.
 *      FALSE if one or more steps failed.
 *
 * Side effects:
 *      Allocates a copy of the mount point string, updates the HgfsMountState structure,
 *      and mounts the filesystem.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsMountState_Init(HgfsMountState *state,   // IN: Hgfs mount state
                    const char *pMountPoint, // IN: Shared folder mount target
                    int fd)                  // IN: Pserver file descriptor, or -1
{
   int mntRes;

   ASSERT(state);
   ASSERT(pMountPoint);

   state->mountPoint = strdup(pMountPoint);
   state->mounted = FALSE;

#if defined(sun)
   {
      HgfsMountInfo mntInfo;   /* Data to pass to kernel when mounting */

      mntInfo.fd = fd;
      mntInfo.version = HGFS_PROTOCOL_VERSION;
      mntInfo.magicNumber = HGFS_SUPER_MAGIC;

      Debug("HgfsMountState_Init: trying to mount %s...\n", state->mountPoint);
      mntRes = mount(HGFS_MOUNT_SRC, state->mountPoint, HGFS_MOUNT_FLAGS,
                     HGFS_FS_NAME, &mntInfo, sizeof mntInfo);
   }
#endif

   if (mntRes == -1) {
      int error = errno;
      Debug("HgfsMountState_Init: mount failed: %s\n", strerror(error));
      HgfsMountState_Cleanup(state);
      return FALSE;
   }
   state->mounted = TRUE;
   Debug("HgfsMountState_Init: mount succeeded\n");

   return TRUE;
}
#endif

/*
 * Hgfs pserver functions
 */

#ifdef VM_GUESTD_RUNS_HGFS_PSERVER
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPserver_Cleanup --
 *
 *    Teardown the communication channel with the server, unmount this
 *    hgfs filesystem, and close the driver proc file.
 *
 * Results:
 *    TRUE on success.
 *    FALSE if one or more steps failed.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsPserver_Cleanup(PserverState *state) // IN: State for this mount
{
   Bool success = TRUE;
   int result;

   ASSERT(state);

   /* Try to close the backdoor */
   if (!HgfsBd_CloseBackdoor(&state->out)) {
      Debug("HgfsPserver_Cleanup: failed to close backdoor\n");
      success = FALSE;
   }
   state->out = NULL;

   /* Release the request packet buffer */
   if (state->requestPacket) {
      HgfsBd_PutBuf(state->requestPacket);
      state->requestPacket = NULL;
   }

   /*
    * XXX The unmount should be done in the driver (NOT here), in case
    * the pserver gets killed or is misbehaved.
    */
   if (!HgfsMountState_Cleanup(&state->mountState)) {
      success = FALSE;
   }

   /* Try to close the hgfs device */
   if (state->fd >= 0) {
      result = close(state->fd);
      if (result < 0) {
         int error = errno;
         Debug("HgfsPserver_Cleanup: failed to close file: %s\n",
               strerror(error));
         success = FALSE;
      }
      state->fd = -1;
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPserver_Init --
 *
 *    Set up pserver communication state.
 *
 *    Specifically: open the proc file, try to mount the filesystem,
 *    get a request packet buffer from the backdoor layer and open the
 *    backdoor communication channel.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsPserver_Init(PserverState *state,     // IN/OUT: State for this mount
                 const char *pMountPoint) // IN: Shared folder mount target
{
   ASSERT(state);
   ASSERT(pMountPoint);

   state->fd = -1;
   state->out = NULL;
   state->requestPacket = NULL;

   state->fd = open(HGFS_DEVICE, O_RDWR | O_NONBLOCK);
   if (state->fd < 0) {
      int error = errno;
      Debug("HgfsPserver_Init: error opening proc file: %s\n",
            strerror(error));
      HgfsPserver_Cleanup(state);
      return FALSE;
   }

   /*
    * We need to make sure children of guestd do not inherit
    * this file descriptor, otherwise they'll keep references to
    * the hgfs module.
    */
   if ((fcntl(state->fd, F_SETFD, FD_CLOEXEC)) < 0) {
      int error = errno;
      Debug("HgfsPserver_Init: error setting proc file flags: %s\n",
            strerror(error));
      HgfsPserver_Cleanup(state);
      return FALSE;
   }

   if (HgfsMountState_Init(&state->mountState, pMountPoint, state->fd) == FALSE) {
      HgfsPserver_Cleanup(state);
      return FALSE;
   }

   /*
    * Eventually we may want to frob /etc/mtab here to reflect the
    * mount.
    */

   state->requestPacket = HgfsBd_GetBuf();
   if (!state->requestPacket) {
      Debug("HgfsPserver_Init: couldn't get bd buffer\n");
      HgfsPserver_Cleanup(state);
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPserverHandleRequest --
 *
 *      Read a request from the driver, send it to the server, and
 *      pass the reply back to the driver.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsPserverHandleRequest(PserverState *pState) // IN: Pserver state
{
   char const *replyPacket; // Buffer returned by HgfsBd_Dispatch
   size_t packetSize;
   int error;
   int n;

   ASSERT(pState);

   /* Get the request from the driver. */
   n = read(pState->fd, pState->requestPacket, HGFS_PACKET_MAX);
   if (n < 0) {
      error = errno;
      Debug("HgfsPserverHandleRequest: Read error from hgfs fd: %s\n",
            strerror(error));
      return FALSE;
   }

   packetSize = n;

   /*
    * Ensure the backdoor is open and then attempt to send the request across it
    * to the hgfs server. 
    *
    * Failure of either action may indicate that Shared Folders have been disabled,
    * in which case we send an error response to the fs module and clean up the
    * backdoor (so that it might be re-opened with a subsequent request).
    *
    * If both actions succeed, the replyPacket will have a valid reply and the
    * packetSize will be set to the size of the replyPacket contents.
    */
   if (!HgfsBd_OpenBackdoor(&pState->out) ||
       (HgfsBd_Dispatch(pState->out,
                        pState->requestPacket,
                        &packetSize,
                        &replyPacket) < 0)) {
      HgfsRequest *request = (HgfsRequest *)pState->requestPacket;
      HgfsReply errorReply;

      Debug("HgfsPserverHandleRequest: Problem sending request across backdoor\n");

      errorReply.id = request->id;
      errorReply.status = HGFS_STATUS_PROTOCOL_ERROR;

      n = write(pState->fd, &errorReply, sizeof errorReply);
      if (n < 0) {
         error = errno;
         Debug("HgfsPserverHandleRequest: Problem writing error reply: %s\n",
               strerror(error));
      }

      if (!HgfsBd_CloseBackdoor(&pState->out)) {
         Debug("HgfsPserverHandleRequest: Problem closing backdoor\n");
      }

      return FALSE;
   }

   ASSERT(replyPacket);
   ASSERT(packetSize);

   /* Send reply to driver */
   n = write(pState->fd, replyPacket, packetSize);
   if (n < 0) {
      error = errno;
      Debug("HgfsPserverHandleRequest: Write error to file: %s\n", strerror(error));
      return FALSE;
   }

   return TRUE;
}
#endif

#if defined(VM_GUESTD_MOUNTS_HGFS)
/*
 *-----------------------------------------------------------------------------
 *
 * GuestdGetValidMountPt --
 *
 *    Return returns the shared folder mount point from the configuration
 *    dictionary with trailing /'s removed.  The caller is responsible for
 *    freeing the returned string.
 *
 * Return value:
 *    NULL on failure (No entry in configuration dictionary or the entry
 *    consisted of 0 or more consecutive /'s.
 *    non-NULL on success (Pointer to valid name, with trailing /'s
 *    removed).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static const char *
GuestdGetValidMountPt(GuestApp_Dict *Dict) // IN: Configuration dictionary
{
   const char *pMount = GuestApp_GetDictEntry(Dict, CONFNAME_MOUNT_POINT);
   const char *end;
   char *p;
   int len;

   if (!pMount) {
      return NULL;
   }

   len = strlen(pMount);
   if (len <= 0) {
      return NULL;
   }

   for (end = &pMount[len - 1]; end > pMount; end--) {
      if (*end != '/') {
	 break;
      }
   }
   if (end == pMount) {
      /*
       * Pathological case string consisting of all /'s
       */
      return NULL;
   }

   /*
    * end points to the last non-/ (and non-'\0') character in pMount.
    */
   p = strdup(pMount);
   if (p) {
      p[end - pMount + 1] = '\0';
   }
   return p;
}
#endif /* VM_GUESTD_MOUNTS_HGFS */


#if !defined(__FreeBSD__) && !defined(sun)
/*
 *----------------------------------------------------------------------
 *
 * GuestdInitBlessedAppList --
 *
 *    Initialize blessed app list.
 *
 * Results:
 *    Head of blessed app list.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static blessedAppNode*
GuestdInitBlessedAppList(char *name)
{
   blessedAppNode *head;
   head = Util_SafeMalloc(sizeof *head);
   head->name = name;
   head->next = NULL;
   return head;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestdAddBlessedAppNode --
 *
 *    Add a node to end of the blessed app list.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static void
GuestdAddBlessedAppNode(blessedAppNode *head, char *name)
{
   ASSERT(head);
   while (head->next) {
      head = head->next;
   }
   head->next = Util_SafeMalloc(sizeof *head);
   head = head->next;
   head->name = name;
   head->next = NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestdFreeBlessedAppList --
 *
 *    Free blessed app list.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static void
GuestdFreeBlessedAppList(blessedAppNode *head)
{
   blessedAppNode *cur = head;
   while (cur) {
      head = cur->next;
      free(cur->name);
      free(cur);
      cur = head;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * GuestdGetBlessedApp --
 *
 *    Try to get blessed app list from X autostart conf file. The conf
 *    is used to set blessed file list, which is used to check if X is
 *    loaded or not. Blessed file lists for different X distro are
 *    not same. If failed, try to get default list.
 *
 * Results:
 *    Blessed app name list if any, NULL otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */
static blessedAppNode*
GuestdGetBlessedAppList(void)
{
   FILE* confFile = NULL;
   char *fileName = NULL;
   char *fileNameUtf8 = NULL;
   char *installPath = GuestApp_GetInstallPath();
   char *appName = NULL;
   size_t myLineLen;
   int index;
   blessedAppNode *ret = NULL;

   if (installPath == NULL) {
      Debug("GuestdGetBlessedAppList: Unable to get install path\n");
      goto exit;
   }

   fileNameUtf8 = Str_Asprintf(NULL, "%s"DIRSEPS"%s", installPath, XAUTOSTART_CONF);
   free(installPath);
   if (NULL == fileNameUtf8) {
      Debug("GuestdGetBlessedAppList: Unable to allocate memory\n");
      goto exit;
   }

   if (!CodeSet_Utf8ToCurrent(fileNameUtf8,
                              strlen(fileNameUtf8),
                              (char **)&fileName,
                              NULL)) {
      Debug("GuestdGetBlessedAppList: Unable to convert to current encoding\n");
      goto exit;
   }
   confFile = fopen(fileName, "r");
   free(fileName);
   if (NULL == confFile) {
      /* If can not open conf file, use default value. */
      Debug("GuestdGetBlessedAppList: Unable to open conf file, use default value.\n");
      goto default_exit;
   }

   while (TRUE) {
      switch (StdIO_ReadNextLine(confFile, &appName, 0, &myLineLen)) {
      case StdIO_Error:
         Debug("GuestdGetBlessedAppList: Unable to read conf file, use default value.\n");
         goto default_exit;

      case StdIO_EOF:
         goto exit;

      case StdIO_Success:
         if (ret == NULL) {
            ret = GuestdInitBlessedAppList(appName);
         } else {
            GuestdAddBlessedAppNode(ret, appName);
         }
         break;

      default:
         NOT_IMPLEMENTED();
      }
   }
   NOT_REACHED();

default_exit:
   if (ret != NULL) {
      GuestdFreeBlessedAppList(ret);
      ret = NULL;
   }
   for (index = 0; index < NR_XBLESSED_APP_DEFAULT; index++) {
      appName = Str_Asprintf(NULL, "%s", gDefaultBlessedApp[index]);
      if (appName == NULL) {
         Debug("GuestdGetBlessedAppList: Unable to allocate memory\n");
         goto exit;
      }
      if (ret == NULL) {
         ret = GuestdInitBlessedAppList(appName);
      } else {
         GuestdAddBlessedAppNode(ret, appName);
      }
   }
exit:
   free(fileNameUtf8);
   if (confFile) {
      fclose(confFile);
   }
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdGetAppEnv  --
 *
 *    Get an environ string from environ table.
 *
 * Results:
 *    Environ string if found, NULL otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
GuestdGetAppEnv(pid_t pid,             // IN: app pid;
                const char *envName)   // IN: env variable name
{
   char *fileName = NULL;
   FILE *envFile;
   char buffer[65536];
   size_t readSize;
   unsigned int index = 0;
   char *ret = NULL;

   fileName = Str_Asprintf(NULL, "/proc/%d/environ", pid);
   if (NULL == fileName) {
      Debug("GuestdGetAppEnv: Unable to allocate memory\n");
      return ret;
   }
   envFile = fopen(fileName, "r");
   if (NULL == envFile) {
      /*
       * We may not be able to open the file due to the security reason.
       */
      Debug("GuestdGetAppEnv: Unable to open env file for %s.\n", fileName);
      free(fileName);
      return ret;
   }

   readSize = fread(buffer, 1, sizeof buffer, envFile);

   /* Separator in environ is '\0'. */
   while (index < readSize) {
      if (!strncmp(&buffer[index], envName, strlen(envName))) {
         ret = Str_Asprintf(NULL, "%s", &buffer[index]);
         break;
      }
      index += strlen(&buffer[index]) + 1;
   }
   free(fileName);
   fclose(envFile);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdGetAppDisplay  --
 *
 *    Get app $DISPLAY from environ.
 *
 * Results:
 *    $DISPALY if any, -1 otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
GuestdGetAppDisplay(pid_t pid)   // IN: app pid
{
   int ret = -1;
   int display;

   char *env = GuestdGetAppEnv(pid, "DISPLAY");
   if (env) {
      if (sscanf(env, "DISPLAY=:%d.", &display) == 1) {
         ret = display;
      }
      free(env);
   }
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdGetActiveXDisplay  --
 *
 *    Get active X display. Call the VT_GETSTATE ioctl on /dev/tty0 or
 *    /dev/vc/0, this returns the current VT number. Then Iterate all X in
 *    /tmp/.X#-lock, and get all X PIDs. For each XPID,  Iterate the X server's
 *    file descriptors in /proc/<PID>/fd/ looking for open /dev/tty# devices.
 *    Compare the current VT and tty#, find out active $DISPLAY.
 *
 * Results:
 *    Active X display, -1 otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
GuestdGetActiveXDisplay(void)
{
   char fileName[PATH_MAX];
   FILE *xPidFile = NULL;
   pid_t xPid = 0;
   int tty;
   int display;
   struct vt_stat state;
   DIR *tmpdir = NULL;
   struct dirent *tmpent;
   char linkName[PATH_MAX];
   char ttyName[32];
   char vcName[32];
   char *dirName = NULL;
   int ret = -1;

   /*
    * Call the VT_GETSTATE ioctl on /dev/tty0 or /dev/vc/0, this returns the
    * current VT number.
    */
   tty = open("/dev/tty0", O_RDONLY);
   if (tty < 0) {
      tty = open("/dev/vc/0", O_RDONLY);
   }

   if (tty < 0) {
      Debug("GuestdGetActiveXDisplay: Cannot get current tty\n");
      goto exit;
   }

   if (ioctl(tty, VT_GETSTATE, &state) < 0) {
      Debug("GuestdGetActiveXDisplay: Cannot get state info for tty\n");
      close(tty);
      goto exit;
   }
   close(tty);

   Str_Sprintf(ttyName, sizeof(ttyName), "/dev/tty%u", state.v_active);
   Str_Sprintf(vcName, sizeof(vcName), "/dev/vc/%u", state.v_active);
   Debug("GuestdGetActiveXDisplay: active tty is %d\n", state.v_active);

   /* Try to get all X PIDs from /tmp/.X#-lock. */
   tmpdir = opendir("/tmp");
   if (NULL == tmpdir) {
      Debug("GuestdGetActiveXDisplay: Unable to open /tmp\n");
      goto exit;
   }

   while ((tmpent = readdir(tmpdir))) {
      int res = sscanf(tmpent->d_name, ".X%u%s", &display, fileName);
      if ((res == 2) && (strncmp(fileName, "-lock", 5) == 0)) {
         DIR *xdir;
         struct dirent *xent;

         Str_Snprintf(fileName, sizeof fileName, "/tmp/%s", tmpent->d_name);
         xPidFile = fopen(fileName, "r");
         if (xPidFile) {
            fscanf(xPidFile, "%"FMTPID"\n", &xPid);
            fclose(xPidFile);
         } else {
            Debug("GuestdGetActiveXDisplay: Cannot get current X pid.\n");
            goto exit;
         }

         /*
          * Iterate the X server's file descriptors in /proc/<PID>/fd/ looking
          * for open /dev/tty# devices.
          */
         dirName = Str_Asprintf(NULL, "/proc/%d/fd", xPid);
         if (NULL == dirName) {
            Debug("GuestdGetActiveXDisplay: Unable to allocate memory\n");
            goto exit;
         }
         xdir = opendir(dirName);

         if (NULL == xdir) {
            Debug("GuestdGetActiveXDisplay: Unable to open %s\n", dirName);
            free(dirName);
            goto exit;
         }
         free(dirName);

         while ((xent = readdir(xdir))) {
            Str_Snprintf(fileName, sizeof fileName, "/proc/%u/fd/%s",
                         xPid, xent->d_name);
            memset(linkName, 0, sizeof linkName);
            readlink(fileName, linkName, sizeof linkName);
            if ((strcmp(linkName, ttyName) == 0) ||
                (strcmp(linkName, vcName) == 0)) {
               closedir(xdir);
               ret = display;
               goto exit;
            }
         }
         closedir(xdir);
     }
   }
exit:
   if (tmpdir) {
      closedir(tmpdir);
   }
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdRunVMwareUser  --
 *
 *    Run vmware-user as "foundUser" with environ from blessedPid. We do this
 *    so that we get a reasonable login environment for "foundUser". We take
 *    care to shell escape the username.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    May start vmware-user. Because it is running is a separated process,
 *    in any case it will exit.
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdRunVMwareUser(pid_t blessedPid)  // IN: blessed pid
{
   char *fileName = NULL;
   char *xauth = NULL;
   Bool isXauthSet = FALSE;
   FILE *envFile = NULL;
   char buffer[65536];
   size_t readSize;
   char *cmd;
   struct passwd *pwd;
   unsigned int index = 0;
   int blockFd;
   struct stat sb;
   int envFd = -1;

   /*
    * If current active account is non-root, vmware-user can not initialize
    * blocking driver for host->guest file copy paste. So open here before
    * setuid and pass fd to vmware-user.
    */
   blockFd = DnD_InitializeBlocking();
   if (blockFd < 0) {
      Debug("GuestdRunVMwareUser: Unable to initialize block driver.\n");
   }

   /* Try to get user from blessed pid's environ. */
   fileName = Str_Asprintf(NULL, "/proc/%d/environ", blessedPid);
   if (NULL == fileName) {
      Debug("GuestdRunVMwareUser: Unable to allocate memory\n");
      goto childerror;
   }
   envFile = fopen(fileName, "r");
   if (NULL == envFile) {
      /*
       * We may not be able to open the file due to the security reason.
       */
      Debug("GuestdRunVMwareUser: Unable to open env file for %s\n", fileName);
      goto childerror;
   }

   readSize = fread(buffer, 1, sizeof buffer, envFile);

   /*
    * Rather than consulting the user's USER environment variable, assume we
    * can instead trust procfs's file permissions to determine processes'
    * owners.
    */
   if ((envFd = fileno(envFile)) == -1) {
      Debug("GuestdRunVMwareUser: Unable to convert FILE * to descriptor\n");
      goto childerror;
   }

   if (fstat(envFd, &sb) == -1) {
      Debug("GuestdRunVMwareUser: Unable to lookup environment details\n");
      goto childerror;
   }

   /*
    * At this point, we're finished with these, so go ahead and release them before
    * executing vmware-user.
    */
   free(fileName);
   fileName = NULL;

   fclose(envFile);
   envFile = NULL;

   /* Switch from root to Xsession user. */
   pwd = getpwuid(sb.st_uid);
   if (pwd == NULL) {
      Debug("GuestdRunVMwareUser: Unable to lookup account for uid %u\n", sb.st_uid);
      goto childerror;
   }

   if (setgid(pwd->pw_gid) != 0) {
      Debug("GuestdRunVMwareUser: Unable to setgid for %s\n", pwd->pw_name);
      goto childerror;
   }

   if (setuid(pwd->pw_uid) != 0) {
      Debug("GuestdRunVMwareUser: Unable to setuid for %s\n", pwd->pw_name);
      goto childerror;
   }

   /* Set env. Separator in environ is '\0'. */
   index = 0;
   while (index < readSize) {
      if (putenv((char *)&buffer[index]) != 0) {
         Debug("GuestdRunVMwareUser: Unable to putenv: %s\n", &buffer[index]);
         goto childerror;
      }
      if (!strncmp(&buffer[index], "HOME=", 5)) {
         xauth = Str_Asprintf(NULL, "XAUTHORITY=%s/.Xauthority", &buffer[index + 5]);
      }
      if (!strncmp(&buffer[index], "XAUTHORITY=", 11)) {
         isXauthSet = TRUE;
      }
      index += strlen(&buffer[index]) + 1;
   }

   /* In KDE XAUTHORITY may not be set, so set it to default value. */
   if (!isXauthSet) {
      if (!xauth || (putenv(xauth) != 0)) {
         Debug("GuestdRunVMwareUser: Unable to putenv for xauth\n");
         goto childerror;
      }
   }
   free(xauth);
   xauth = NULL;

   /* Load vmware-user. */
   cmd = Str_Asprintf(NULL, "%s/vmware-user", gBinDir);
   if (cmd == NULL) {
      Debug("GuestdRunVMwareUser unable to allocate \"cmd\"\n");
      goto childerror;
   }
   Debug("GuestdRunVMwareUser: Run cmd \"%s\"\n", cmd);

   Str_Sprintf(buffer, sizeof buffer, "%d", blockFd);
   execl(cmd, cmd, ">/dev/null", "2>&1", "-blockFd", buffer, NULL);
   Debug("GuestdRunVMwareUser: Cannot run cmd \"%s\"\n", cmd);

   /* We should only get here from a goto. */
childerror:
   free(xauth);
   free(fileName);
   if (envFile != NULL) {
      fclose(envFile);
   }
   if (blockFd >= 0 && !DnD_UninitializeBlocking(blockFd)) {
      Debug("GuestdRunVMwareUser unable to uninitialize blocking.\n");
   }
   exit(1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdCheckVMwareUser  --
 *
 *    EventManager callback function that checks up on the vmware-user process.
 *    If a user is logged in to the active X disaply, vmware-user should be
 *    running. If no active one, vmware-user should NOT be running.
 *
 * Results:
 *    TRUE. If we return FALSE, the event manager falls over.
 *
 * Side effects:
 *    May start or kill vmware-user.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdCheckVMwareUser(void *clientData) // IN/OUT: vmware-user's pid
{
   pid_t *pid = (pid_t *)clientData;
   pid_t blessedPid = 0;
   char *blessedApp = NULL;
   char *blessedUser = NULL;
   char *userUser = NULL;
   char processName[256];
   int iProc = 0;
   int activeDisplay = -1;
   int userDisplay = -1;
   uint32 period = VMWAREUSER_CHECK_TIME;
   blessedAppNode *appList;
   blessedAppNode *cur;
   ProcMgr_ProcList *procList;


   /* Get active X Window $DISPLAY. */
   activeDisplay = GuestdGetActiveXDisplay();
   Debug("GuestdCheckVMwareUser: Get active x display %d\n", activeDisplay);

   /* Get process list. */
   procList = ProcMgr_ListProcesses();
   if (procList == NULL) {
      Debug("GuestdCheckVMwareUser: Could not get process listing\n");
      return FALSE;
   }

   /* Get vmware-user id. If not existing, set to 0. */
   *pid = 0;
   for (iProc = procList->procCount - 1; iProc >= 0; iProc--) {
      Util_GetProcessName(procList->procIdList[iProc],
                          processName,
                          sizeof processName);
      if (strcmp(processName, "vmware-user") == 0) {
         *pid = procList->procIdList[iProc];
         userDisplay = GuestdGetAppDisplay(*pid);
         break;
      }
   }
   Debug("GuestdCheckVMwareUser: vmware-user pid is %d\n", *pid);
   if (activeDisplay < 0) {
      goto stopVMwareUser;
   }

   if (activeDisplay == userDisplay) {
      goto exit;
   }

   if (*pid > 0) {
      userUser = GuestdGetAppEnv(*pid, "USER");
   }

   /*
    * Get blessed app list, then check if any app is up. App's $DISPLAY
    * should also be same as active X display.
    */
   appList = GuestdGetBlessedAppList();
   cur = appList;
   while (cur) {
      blessedApp = cur->name;
      Debug("GuestdCheckVMwareUser: Got blessed app %s\n", blessedApp);
      for (iProc = procList->procCount - 1; iProc >= 0; iProc--) {
         Util_GetProcessName(procList->procIdList[iProc],
                             processName,
                             sizeof processName);
         if ((strcmp(processName, blessedApp) == 0) &&
             (GuestdGetAppDisplay(procList->procIdList[iProc]) == activeDisplay)) {
            blessedPid = procList->procIdList[iProc];
            break;
         }
      }
      /* If any app started, start vmware-user. */
      if (blessedPid > 0) {
         break;
      }
      cur = cur->next;
   }
   GuestdFreeBlessedAppList(appList);

   Debug("GuestdCheckVMwareUser: got blessed pid %d\n", blessedPid);
   if (blessedPid == 0) {
      goto stopVMwareUser;
   }

   if (*pid == 0) {
      /*
       * There is a active X session and vmware-user isn't running,
       * we should start it with fork/exec.
       */
      *pid = fork();
      if (*pid == -1) {
         Debug("GuestdCheckVMwareUser: Unable to fork: %s.\n\n", strerror(errno));
         /*
          * Reset pid to 0, maybe we'll be able to fork next time around
          */
         *pid = 0;
      } else if (*pid == 0) {
         /* Child. This function will not return. */
         GuestdRunVMwareUser(blessedPid);
      } else {
         /*
          * This is the parent after the fork. The parent doesn't need to do
          * anything special since the child's pid is already recorded. I left this
          * comment here anyway as a placeholder.
          */
      }
   } else {
      blessedUser = GuestdGetAppEnv(blessedPid, "USER");
      if ((strcmp(userUser, blessedUser) != 0) ||
          (userDisplay != activeDisplay)) {
         /*
          * vmware-user's user name is not right one, or display does match active
          * X display so kill current vmware-user and start another one soon.
          */
         period /= 10;
         goto stopVMwareUser;
      }
   }
   goto exit;

stopVMwareUser:
   if (*pid != 0) {
      Debug("GuestdCheckVMwareUser: Killing vmware-user.\n");
      kill(*pid, SIGTERM);
      *pid = 0;
   }

exit:
   free(userUser);
   free(blessedUser);
   ProcMgr_FreeProcList(procList);
   EventManager_Add(ToolsDaemonEventQueue, period,
                    GuestdCheckVMwareUser, clientData);
   return TRUE;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdCommandLineRpciSignal --
 *
 *    Command line RPCI signal handler
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdCommandLineRpciSignal(int signal)
{
   Debug("Received signal %d\n", signal);

   /*
    * Sending a command line RPCI doesn't take a long time. Delay the handling
    * of the signal until we have closed the RpcOut object --hpreg
    */

   if (gCommandLineRpciSignal == 0) {
      /*
       * This is the first signal we receive
       */

      ASSERT(signal);
      gCommandLineRpciSignal = signal;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdCommandLineRpci --
 *
 *    Make VMware execute a RPCI string command, and output the string result
 *    on stdout
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdCommandLineRpci(char const *cmd) // IN: RPCI command
{
   struct sigaction olds[ARRAYSIZE(cSignals)];
   char *result = NULL;
   Bool status = FALSE;
   Bool signaled = FALSE;

   gCommandLineRpciSignal = 0;

   if (Signal_SetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals),
                              GuestdCommandLineRpciSignal) == 0) {
      return FALSE;
   }

   status = RpcOut_sendOne(&result, NULL, "%s", cmd);

   if (gCommandLineRpciSignal) {
      fprintf(stderr, "Interrupted by signal %d.\n\n", gCommandLineRpciSignal);
      signaled = TRUE;
   }

   if ((Signal_ResetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals)) == 0) ||
       signaled) {
      status = FALSE;
   } else if (!status) {
      fprintf(stderr, "%s\n", result ? result : "NULL");
   } else {
      printf("%s\n", result);
   }

   free(result);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdDaemonSignal --
 *
 *    Daemon signal handler
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdDaemonSignal(int signal)
{
   Debug("Received signal %d\n", signal);

   /*
    * This code can be executed at any time, and can preempt the "normal" flow
    * of execution.
    *
    * To avoid re-entrancy and concurrency issues in the "normal" code, we
    * defer the handling of the signal until we are in a well-known context
    *
    *   --hpreg
    */

   if (gDaemonSignal == 0) {
      /*
       * This is the first signal we receive
       */

      ASSERT(signal);
      gDaemonSignal = signal;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdExecute --
 *
 *      Callback-able wrappers to execute halt/reboot commands.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdExecute(void *clientData, const char *confName)
{
   GuestApp_Dict **pConfDict = (GuestApp_Dict **) clientData;
   const char *cmd;

   ASSERT(confName);
   ASSERT(pConfDict);
   ASSERT(*pConfDict);

   cmd = GuestApp_GetDictEntry(*pConfDict, confName);
   ASSERT(cmd);

   return ProcMgr_ExecSync(cmd, NULL);
}

static Bool
GuestdExecuteHalt(void *clientData)
{
   return GuestdExecute(clientData, CONFNAME_HALT);
}

static Bool
GuestdExecuteReboot(void *clientData)
{
   return GuestdExecute(clientData, CONFNAME_REBOOT);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdCreateUpgrader --
 *
 *      Spawn a new process that manages the auto-upgrade procedure.
 *
 * Results:
 *      Return value specifies result of fork/exec.
 *
 * Side effects:
 *      Spawns a new process.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdCreateUpgrader(char const **result,     // OUT
                     size_t *resultLen,       // OUT
                     const char *name,        // Ignored
                     const char *args,        // Ignored
                     size_t argsSize,         // Ignored
                     void *clientData)        // Ignored
{
   int32 version;
   unsigned int index = 0;
   const char *upgraderPath = NULL;
   char *upgraderCmd = NULL;
   ProcMgr_AsyncProc *upgraderProc = NULL;

   if (StrUtil_GetNextIntToken(&version, &index, args, " ")) {
      /* New protocol. Host sent 'upgrader.create <version>' */
      if (version == 1) {
         upgraderPath = ToolsDaemon_GetGuestTempDirectory();
         if (upgraderPath == NULL) {
            Log("ToolsDaemon_GetGuestTempDirectory failed.\n");
            return RpcIn_SetRetVals(result, resultLen,
                                    "ToolsDaemon_GetGuestTempDirectory failed", FALSE);
         }

         upgraderCmd = Str_Asprintf(NULL, "bash %s%srun_upgrader.sh",
                                    upgraderPath, DIRSEPS);
         if (upgraderCmd == NULL) {
            Log("Str_Asprintf failed.\n");
            return RpcIn_SetRetVals(result, resultLen,
                                    "Str_Asprintf failed", FALSE);
         }
      } else {
         return RpcIn_SetRetVals(result, resultLen, "Unknown protocol version", FALSE);
      }
   } else {
      /* Old protocol. Host sent 'upgrader.create' */
      upgraderCmd = Util_SafeStrdup(UPGRADER_FILENAME);
   }

   upgraderProc = ProcMgr_ExecAsync(upgraderCmd, NULL);
   free(upgraderCmd);
   if (upgraderProc == NULL) {
      Warning("Failed to start upgrader.\n");
      return RpcIn_SetRetVals(result, resultLen, "ProcMgr_ExecAsync failed", FALSE);
   }
   ProcMgr_Free(upgraderProc);
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdSleep --
 *
 *      Sleep in a non-blocking way for the given number of
 *      micro-seconds. The callback is called if the async proc
 *      exits.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdSleep(uint64 numUsecs,                 // IN
            const ToolsDaemon_Data *tdData)  // IN
{
   static ProcMgr_AsyncProc *curAsyncProc = NULL;
   static int asyncFd;              /*
                                     * Fd for asyncProc. Only meaningful
                                     * if (curAsyncProc != NULL).
                                     */
   int maxFd;                       /* Max fd of all Fd sets */
   fd_set readFds;                  /* Read fd set to select on */
   fd_set writeFds;
#if !defined(__FreeBSD__) && !defined(sun)
   SocketSelectable *sockReadFds = NULL;
   SocketSelectable *sockWriteFds = NULL;
   int numSockReadFds = 0;
   int numSockWriteFds = 0;
   int index;
#endif
   struct timeval tv;
   int status;
#if defined(VM_GUESTD_RUNS_HGFS_PSERVER)
   PserverState *pState = &globalHgfsState;
#endif

   ASSERT(tdData);

   /* If there is a new async proc, init the fd set & fd max */
   if (tdData->asyncProc && !curAsyncProc) {
      asyncFd = ProcMgr_GetAsyncProcSelectable(tdData->asyncProc);
      curAsyncProc = tdData->asyncProc;
   } else {
      /*
       * Make sure the caller doesn't try to change the asyncProc before
       * its fd has been selected.
       */
      ASSERT(tdData->asyncProc == curAsyncProc);
   }

   /* Init readFds & writeFds */
   FD_ZERO(&readFds);
   FD_ZERO(&writeFds);
   maxFd = -1;

   if (curAsyncProc) {
      FD_SET(asyncFd, &readFds);
      maxFd = asyncFd;
   }

#if defined(VM_GUESTD_RUNS_HGFS_PSERVER)
   /* If hgfs is enabled, add its fd */
   if (pState->fd >= 0) {
      FD_SET(pState->fd, &readFds);
      if (pState->fd > maxFd) {
         maxFd = pState->fd;
      }
   }
#endif

#if !defined(__FreeBSD__) && !defined(sun)
   SocketMgr_GetSelectables(SOCKETMGR_IN,
                            &sockReadFds,
                            &numSockReadFds);
   for (index = 0; index < numSockReadFds; index++) {
      FD_SET(sockReadFds[index], &readFds);
      if (sockReadFds[index] > maxFd) {
         maxFd = sockReadFds[index];
      }
   }

   SocketMgr_GetSelectables(SOCKETMGR_OUT,
                            &sockWriteFds,
                            &numSockWriteFds);
   for (index = 0; index < numSockWriteFds; index++) {
      FD_SET(sockWriteFds[index], &writeFds);
      if (sockWriteFds[index] > maxFd) {
         maxFd = sockWriteFds[index];
      }
   }
#endif

   tv.tv_sec = numUsecs / 1000000L;
   tv.tv_usec = numUsecs % 1000000L;

   status = select(maxFd + 1, &readFds, &writeFds, NULL, &tv);

   if (status == -1) {
      Debug("Select encountered an error: %s\n", strerror(errno));
   } else if (status > 0) {
      Debug("Select returned status > 0\n");

#ifdef VM_GUESTD_RUNS_HGFS_PSERVER
      if (pState->fd >= 0 && FD_ISSET(pState->fd, &readFds)) {
         /* There is an hgfs request waiting in the driver. */
         HgfsPserverHandleRequest(pState);
      }
#endif

      if (curAsyncProc && FD_ISSET(asyncFd, &readFds)) {
         /* The async proc fd was written to */

         Bool ret;

         ASSERT(tdData->asyncProcCb);

         if (!ProcMgr_GetAsyncStatus(curAsyncProc, &ret)) {
            ret = FALSE;
            Debug("Failed to get return status for async process.\n");
         }

         tdData->asyncProcCb(ret, tdData->asyncProcCbData);
         Debug("Done executing asynchronous cmd\n");

         /* Reinitialize */
         curAsyncProc = NULL;
      }

#if !defined(__FreeBSD__) && !defined(sun)
      for (index = 0; index < numSockReadFds; index++) {
         if (FD_ISSET(sockReadFds[index], &readFds)) {
            SocketMgr_ProcessSelectable(sockReadFds[index], SOCKETMGR_IN);
         }
      }

      for (index = 0; index < numSockWriteFds; index++) {
         if (FD_ISSET(sockWriteFds[index], &writeFds)) {
            SocketMgr_ProcessSelectable(sockWriteFds[index], SOCKETMGR_OUT);
         }
      }
#endif
   }

#if !defined(__FreeBSD__) && !defined(sun)
   free((void *) sockReadFds);
   free((void *) sockWriteFds);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdDaemon --
 *
 *    Setup a TCLO channel with VMware, and run the event loop
 *
 * Return value:
 *    TRUE on normal exit (when sent SIGTERM)
 *    FALSE otherwise (detail is displayed)
 *
 * Side effects:
 *    if a signal is trapped, gDaemonSignalPtr is given its value
 *      & the method exits
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdDaemon(GuestApp_Dict **pConfDict,       // IN/OUT
             int *gDaemonSignalPtr)           // IN/OUT
{
#if !defined(__FreeBSD__) && !defined(sun)
   pid_t vmwareuserPid = 0;
#endif
   ToolsDaemon_Data *data = NULL;
   VmBackupSyncProvider *syncProvider = NULL;

#if VM_GUESTD_MOUNTS_HGFS
   const char *mountPoint = GuestdGetValidMountPt(*pConfDict);

   if (!mountPoint) {
      Warning("Could not determine shared folder mount point, %s occurred.\n",
	      strerror(errno));
   } else if (!Util_MakeSureDirExistsAndAccessible(mountPoint, 0755)) {
      Warning("Shared folder mount point %s is not an accessible directory.\n",
	      mountPoint);
      /*
       * free what GuestdGetValidMountPt allocated.
       */
      free((void *)mountPoint);
      mountPoint = NULL;
   } else {
#   ifdef VM_GUESTD_RUNS_HGFS_PSERVER
      /* Setup the hgfs pserver (guest can operate on host file name space) */
      if (HgfsPserver_Init(&globalHgfsState, mountPoint) == FALSE) {
	 Warning("Unable to start hgfs.\n");
      }
#   else
      /* Just mount the hgfs filesystem */
      if (HgfsMountState_Init(&globalHgfsState, mountPoint, -1) == FALSE) {
         Warning("Unable to mount hgfs.\n");
      }
#   endif

      free((void *)mountPoint);
   }
#endif

   data = ToolsDaemon_Init(pConfDict, EXEC_LOG,
                           GuestdExecuteHalt, pConfDict,
                           GuestdExecuteReboot, pConfDict,
                           NULL, NULL);
   if (!data) {
      Warning("Unable to start guestd.\n");
      goto out;
   }

   ASSERT(data->in);

   /*
    * Set up the guest info 'server'. On Linux, a separate event loop thread
    * is not spawned.
    */
   if (!GuestInfoServer_Init(ToolsDaemonEventQueue)) {
      Warning("Unable to start guest info server.\n");
   } else {
      GuestInfoServer_DisableDiskInfoQuery(
         GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_DISABLEQUERYDISKINFO));
   }

#if !defined(__FreeBSD__) && !defined(sun)
   gBinDir = GuestApp_GetDictEntry(*pConfDict, CONFNAME_BINDIR);
   if (gBinDir == NULL) {
      gBinDir = "/usr/bin";
   }
   /*
    * This function will add itself to the event manager
    */
   GuestdCheckVMwareUser(&vmwareuserPid);
#endif

   /*
    * Start listening for VMX requests to create the upgrader.
    */
   RpcIn_RegisterCallback(data->in, "upgrader.create",
                          GuestdCreateUpgrader, NULL);

   /*
    * Initialize the vmbackup subsystem, if it's supported in the current
    * platform.
    */
   syncProvider = VmBackup_NewSyncDriverProvider();
   if (syncProvider != NULL) {
      VmBackup_Init(data->in, ToolsDaemonEventQueue, syncProvider);
   } else {
      Debug("No vmBackup implementation available!\n");
   }

   /*
    * Event loop
    */

   for (;;) {
      int nr = 0;
      uint64 sleepUsecs = 0;

      nr = EventManager_ProcessNext(ToolsDaemonEventQueue, &sleepUsecs);
      if (nr != 1) {
         fprintf(stderr,
                 "Unexpected end of the main loop: returned value is %d\n",
                 nr);
         goto out;
      }

      /* Reap our zombie children. */
      waitpid(-1, NULL, WNOHANG);

      if (*gDaemonSignalPtr) {
         /*
          * We are in a well-known context: the processing of the previous
          * event is done, and we haven't started to process the next event.
          *
          * In particular, if the previous event handler executed a TCLO
          * command that ended up sending a signal to us, we are sure that the
          * reply message for that command has been crafted. So it is the right
          * time to stop 'in', which will send this last reply back to VMware.
          *
          *   --hpreg
          */

         fprintf(stderr, "Interrupted by signal %d.\n\n", *gDaemonSignalPtr);
         goto out;
      }

      if (!ToolsDaemon_CheckReset(data, &sleepUsecs)) {
         goto out;
      }
      GuestdSleep(sleepUsecs, data);
   }

   NOT_REACHED();

out:
#if defined(VM_GUESTD_RUNS_HGFS_PSERVER)
   HgfsPserver_Cleanup(&globalHgfsState);
#elif defined(VM_GUESTD_MOUNTS_HGFS)
   HgfsMountState_Cleanup(&globalHgfsState);
#endif
   if (data) {
      ToolsDaemon_Cleanup(data);
   }

#if !defined(__FreeBSD__) && !defined(sun)
   if (vmwareuserPid != 0) {
      kill(vmwareuserPid, SIGTERM);
   }
#endif

   if (syncProvider != NULL) {
      VmBackup_Shutdown(data->in);
   }

   return *gDaemonSignalPtr == SIGTERM ? TRUE : FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdDaemonWrapper --
 *
 *    Wrap the call to GuestdDaemon so the signal handler gets both set
 *    & reset.
 *
 * Return value:
 *    TRUE on success (never happens)
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdDaemonWrapper(GuestApp_Dict **pConfDict) // IN
{
   Bool returnVal;
   struct sigaction olds[ARRAYSIZE(cSignals)];

   gDaemonSignal = 0;

   /*
    * Do not check return value.
    * setsid() only fails if we are already process group leader.
    */
   setsid();

   if (Signal_SetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals),
                              GuestdDaemonSignal) == 0) {
      return FALSE;
   }

   returnVal = GuestdDaemon(pConfDict, &gDaemonSignal);

   if (Signal_ResetGroupHandler(cSignals, olds, ARRAYSIZE(cSignals)) == 0) {
      return FALSE;
   }

   return returnVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdAlreadyRunning --
 *
 *    Check if there is an instance of guestd already running.
 *
 *    Note that we used to use pgrep(1) but that approach produces false
 *    positives when the init script that starts guestd has the same name as
 *    the guestd binary, as is done for open-vm-tools packages.
 *
 * Return value:
 *    TRUE if there is another guestd running.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdAlreadyRunning(char const *pidFileName) // IN
{
   FILE *pidFile = NULL;
   pid_t pid = 0;

   ASSERT(pidFileName);

   pidFile = fopen(pidFileName, "r");
   if (pidFile) {
      fscanf(pidFile, "%"FMTPID"\n", &pid);
      fclose(pidFile);

      /*
       * XXX There is an assumption that if the process with pid is alive,
       * the process is just guestd. Actually the process name should be 
       * also checked because it is possible that there is another process 
       * with same pid. 2 reasons it is not checked. First we can not find
       * a cross-platform method to check the process name. Second is that
       * the possibility is very low in our case because the PID file should
       * always be with guestd process. Even user manually kills the guestd,
       * the PID file will also be removed. Perhaps longer term we should 
       * add a function like System_GetProcessName(pid_t) to 
       * bora-vmsoft/lib/system that will hide the platform-specific 
       * messiness. 
       */
      if (pid != getpid() && kill(pid, 0) == 0) {  
         return TRUE;
      }
      /*
       * If process with pid is dead, the PID file will be removed. If pid
       * is same as getpid(), PID file will also be removed. 
       */
      unlink(pidFileName);
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdWritePidfile --
 *
 *    Write a pid into a pidfile
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestdWritePidfile(char const *fileName, // IN
                   pid_t pid)            // IN
{
   FILE *pidFile;

   pidFile = fopen(fileName, "w+");
   if (pidFile == NULL) {
      fprintf(stderr, "Unable to open the \"%s\" PID file: %s.\n\n", fileName,
              strerror(errno));

      return FALSE;
   }

   fprintf(pidFile, "%"FMTPID"\n", pid);

   if (fclose(pidFile)) {
      fprintf(stderr, "Unable to close the \"%s\" PID file: %s.\n\n", fileName,
              strerror(errno));

      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestdUsage --
 *
 *   Write an help message on stderr and exit
 *
 * Results:
 *   None
 *
 * Side effects:
 *   None
 *
 *-----------------------------------------------------------------------------
 */

static void
GuestdUsage(char const *prog, // IN: Name of this program
            int exitCode)     // IN
{
   fprintf(stderr,
      "Usage:\n"
      "\n"
      "   %s --help\n"
      "      Display this help message\n"
      "\n"
      "   %s --cmd <command>\n"
      "      Make the %s execute a command\n"
      "\n"
      "   %s\n"
      "      Run in daemon mode\n"
      "\n"
      "      [--background <PID file>]\n"
      "         Start in the background, and write the PID of the background\n"
      "         process in a file.\n"
      "\n"
      "      [--halt-command <command>]\n"
      "         Command to use to halt the system.\n"
      "         The default is \"%s\"\n"
      "\n"
      "      [--reboot-command <command>]\n"
      "         Command to use to reboot the system.\n"
      "         The default is \"%s\"\n"
      "\n",
      prog, prog, PRODUCT_LINE_NAME, prog, CONFVAL_HALT_DEFAULT,
      CONFVAL_REBOOT_DEFAULT);

   exit(exitCode);
}


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *   Run the program: parse the command line and do the requested job
 *
 * Results:
 *   Exit code
 *
 * Side effects:
 *   None
 *
 *-----------------------------------------------------------------------------
 */

int
main(int argc,    // IN: Number of command line arguments
     char **argv) // IN: Array of arguments
{
   Bool retVal;
   Bool rpci;
   Bool help;
   char const *pidFile;
   GuestApp_Dict *confDict;

   int index;
   Bool parseOptions;
   const char *arguments[1];
   int argumentIndex;
   int expectedArgumentIndex;

   confDict = Conf_Load();

   /*
    * Parse the command line.
    *
    * We do it by hand because getopt() doesn't handle long options, and
    * getopt_long is a GNU extension --hpreg
    *
    * argv[0] is the program name, as usual
    */

   /*
    * Optional arguments
    */

   /* Default values */
   rpci = FALSE;
   help = FALSE;
   pidFile = NULL;

   argumentIndex = 0;
   parseOptions = TRUE;
   for (index = 1; index < argc; index++) {
      Bool isOption;

      if (parseOptions) {
         if (strcmp(argv[index], "--") == 0) {
            /*
             * Special options to specify the end of options (in order to pass
             * arguments that begin with '-'
             */

            parseOptions = FALSE;
            continue;
	 }

         isOption = strncmp(argv[index], "-", 1) == 0;
      } else {
         isOption = FALSE;
      }

      if (isOption) {
	 char const *option;

         option = argv[index] + 1;

         if (strcmp(option, "-cmd") == 0) {
            rpci = TRUE;
	 } else if (strcmp(option, "-help") == 0) {
            help = TRUE;
	 } else if (strcmp(option, "-background") == 0) {
            if (index + 1 == argc) {
               fprintf(stderr, "The \"%s\" option on the command line requires an "
                       "argument.\n\n", option);
               GuestdUsage(argv[0], 1);
            }

            index++;
            pidFile = argv[index];
	 } else if (strcmp(option, "-halt-command") == 0) {
            if (index + 1 == argc) {
               fprintf(stderr, "The \"%s\" option on the command line requires an "
                       "argument.\n\n", option);
               GuestdUsage(argv[0], 1);
            }

            index++;
            GuestApp_SetDictEntry(confDict, CONFNAME_HALT, argv[index]);
	 } else if (strcmp(option, "-reboot-command") == 0) {
            if (index + 1 == argc) {
               fprintf(stderr, "The \"%s\" option on the command line requires an "
                       "argument.\n\n", option);
               GuestdUsage(argv[0], 1);
            }

            index++;
            GuestApp_SetDictEntry(confDict, CONFNAME_REBOOT, argv[index]);
	 } else {
            fprintf(stderr, "Invalid \"%s\" option on the command line.\n\n", option);
            GuestdUsage(argv[0], 1);
	 }
      } else {
         if (argumentIndex >= ARRAYSIZE(arguments)) {
            fprintf(stderr, "Too many mandatory argument(s) on the command line. The "
                    "maximum is %"FMTSZ"u.\n\n", ARRAYSIZE(arguments));
            GuestdUsage(argv[0], 1);
         }

	 arguments[argumentIndex++] = argv[index];
      }
   }

   /*
    * Mandatory arguments
    */

   if (rpci) {
      expectedArgumentIndex = 1;
   } else {
      expectedArgumentIndex = 0;
   }

   if (argumentIndex != expectedArgumentIndex) {
      fprintf(stderr, "Incorrect number of mandatory argument(s) "
                      "on the command line: %u instead of %u.\n\n",
                      argumentIndex, expectedArgumentIndex);
      GuestdUsage(argv[0], 1);
   }

   /*
    * Do the requested job
    */

   if (help) {
      GuestdUsage(argv[0], 0);
   }

   if (!VmCheck_IsVirtualWorld()) {
#ifndef ALLOW_TOOLS_IN_FOREIGN_VM
      Panic("The VMware service must be run from within a virtual machine.\n");
#else
      runningInForeignVM = TRUE;
#endif
   }

   if (rpci) {
      return GuestdCommandLineRpci(arguments[0]) ? 0 : 1;
   }


   /*
    * We must (attempt to) check for another instance running, even when the
    * '--background <PID file>' option wasn't specified (fix for bug 8098).
    * In such cases, we'll assume that the PID file can be found at
    * DEFAULT_PIDFILE, which should work for Linux, Solaris, and FreeBSD guests.
    */
   if (GuestdAlreadyRunning(pidFile ? pidFile : DEFAULT_PIDFILE)) {
      fprintf(stderr, "Guestd is already running, exiting.\n");
      GuestApp_FreeDict(confDict);
      /*
       * Here we still should return 0 otherwise if vmware-tools.sh
       * get an error return, it will ask user to run config.pl
       * again. We should quit here silently.
       */
      exit(0);
   }

   if (pidFile) {
      if (daemon(FALSE, FALSE)) {
         fprintf(stderr, "Unable to daemonize: %s\n", strerror(errno));
         GuestApp_FreeDict(confDict);
         exit(1);
      }
      /* From here on we are detached from controlling terminal. */
      GuestdWritePidfile(pidFile, getpid());
   }

   retVal = GuestdDaemonWrapper(&confDict) ? 0 : 1;

   if (pidFile) {
      unlink(pidFile);
   }

   GuestApp_FreeDict(confDict);

   return retVal;
}
