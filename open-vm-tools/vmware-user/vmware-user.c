/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * vmware-user.c --
 *
 *     The linux vmware-user app. It's a hidden window app that is supposed
 *     to run on session start. It handles tools features which we want
 *     active all the time, but don't want to impose a visable window on the
 *     user.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <gtk/gtkinvisible.h>
#include <locale.h>
#if defined(__FreeBSD__) && (!defined(USING_AUTOCONF) || defined(HAVE_SYSLIMITS_H))
#include <syslimits.h>  // PATH_MAX
#endif

#include "vmwareuserInt.h"
#include "vm_assert.h"
#include "vm_app.h"
#include "eventManager.h"
#include "hgfsServerManager.h"
#include "vmcheck.h"
#include "debug.h"
#include "rpcin.h"
#include "vmsignal.h"
#include "foundryToolsDaemon.h"
#include "strutil.h"
#include "conf.h" // for Conf_Load()
#include "dnd.h"
#include "syncDriver.h"
#include "str.h"
#include "guestApp.h" // for ALLOW_TOOLS_IN_FOREIGN_VM
#include "unity.h"
#include "ghIntegration.h"
#include "resolution.h"

#include "vm_atomic.h"
#include "hostinfo.h"
#include "vmwareuser_version.h"

#include "embed_version.h"
VM_EMBED_VERSION(VMWAREUSER_VERSION_STRING);

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define VMUSER_TITLE    "vmware-user"
#define LOCK_ATOM_NAME  "vmware-user-lock"

#define INVALID_VALUE "Invalid option"
#define INVALID_OPTION "Invalid value"
#define INVALID_COMMAND "Invalid command format"
#define OPTION_BLOCK_FD "-blockFd"

/*
 * Forward Declarations
 */
void VMwareUser_OnDestroy(GtkWidget *widget, gpointer data);
GtkWidget* VMwareUser_CreateWindow(void);
gint EventQueuePump(gpointer data);

Bool VMwareUserRpcInResetCB    (RpcInData *data);
Bool VMwareUserRpcInSetOptionCB(char const **result, size_t *resultLen,
                                const char *name, const char *args,
                                size_t argsSize, void *clientData);
Bool VMwareUserRpcInCapRegCB   (char const **result, size_t *resultLen,
                                const char *name, const char *args,
                                size_t argsSize, void *clientData);
void VMwareUserRpcInErrorCB    (void *clientdata, char const *status);

extern Bool ForeignTools_Initialize(GuestApp_Dict *configDictionaryParam,
                                    DblLnkLst_Links *eventQueue);
extern void ForeignTools_Shutdown(void);

static Bool InitGroupLeader(Window *groupLeader, Window *rootWindow);
static Bool AcquireDisplayLock(void);
static Bool QueryX11Lock(Display *dpy, Window w, Atom lockAtom);
static void ReloadSelf(void);


/*
 * Globals
 */

static Bool gOpenUrlRegistered;
static Bool gDnDRegistered;
static Bool gCopyPasteRegistered;
static Bool gHgfsServerRegistered;
static pid_t gParentPid;
static char gLogFilePath[PATH_MAX];

/*
 * The following are flags set by our signal handler.  They are evaluated
 * in main() only if gtk_main() ever returns.
 */
static Bool gReloadSelf;        // Set by SIGUSR2; triggers reload.
static Bool gYieldBlock;        // Set by SIGUSR1; triggers DND shutdown
static Bool gSigExit;           // Set by all but SIGUSR1; triggers app shutdown

/*
 * From vmwareuserInt.h
 */
RpcIn *gRpcIn;
Display *gXDisplay;
GtkWidget *gUserMainWidget;

GtkWidget *gHGWnd;
GtkWidget *gGHWnd;

Window gXRoot;
DblLnkLst_Links *gEventQueue;
Bool optionCopyPaste;
Bool optionDnD;
Bool gCanUseVMwareCtrl;
Bool gCanUseVMwareCtrlTopologySet;
guint gTimeoutId;
int gBlockFd;

/*
 * All signals that:
 * . Can terminate the process
 * . May occur even if the program has no bugs
 */
static int const gSignals[] = {
   SIGHUP,
   SIGINT,
   SIGQUIT,
   SIGTERM,
   SIGUSR1,     // yield vmblock, uninit DnD
   SIGUSR2,     // reload vmware-user
   SIGPIPE
};


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserCleanupRpc  --
 *
 *      Unset capabilities and cleanup the backdoor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The application will close.
 *
 *-----------------------------------------------------------------------------
 */

void VMwareUserCleanupRpc(void)
{
   if (gRpcIn) {
      Unity_UnregisterCaps();
      GHI_Cleanup();
      Unity_Cleanup();
      Resolution_Cleanup();

      if (gHgfsServerRegistered) {
         HgfsServerManager_Unregister(gRpcIn, TOOLS_DND_NAME);
         gHgfsServerRegistered = FALSE;
      }

      if (!RpcIn_stop(gRpcIn)) {
         Debug("Failed to stop RpcIn loop\n");
      }
      if (gOpenUrlRegistered) {
         FoundryToolsDaemon_UnregisterOpenUrl();
         gOpenUrlRegistered = FALSE;
      }
      if (gDnDRegistered) {
         DnD_Unregister(gHGWnd, gGHWnd);
         gDnDRegistered = FALSE;
      }
      if (gCopyPasteRegistered) {
         CopyPaste_Unregister(gUserMainWidget);
         gCopyPasteRegistered = FALSE;
      }
      RpcIn_Destruct(gRpcIn);
      gRpcIn = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserSignalHandler  --
 *
 *      Handler for Posix signals. We do this to ensure that we exit gracefully.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Application will break out of the gtk_main() loop.  One or more of the
 *      signal flags (gReloadSelf, gYieldBlock, gSigExit) may be set.  For all
 *      signals but SIGUSR1, VMwareUserCleanupRpc() will be called.
 *
 *-----------------------------------------------------------------------------
 */

void VMwareUserSignalHandler(int sig) // IN
{
   switch (sig) {
   case SIGUSR1:
      gYieldBlock = TRUE;
      break;
   case SIGUSR2:
      gReloadSelf = TRUE;
      gSigExit = TRUE;
      break;
   default:
      gSigExit = TRUE;
   }

   if (gSigExit) {
      VMwareUserCleanupRpc();
   }

   gtk_main_quit();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUser_OnDestroy  --
 *
 *      Callback for the gtk signal "destroy" on the main window.
 *      Exit the gtk loop, causing main() to exit.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The application will close.
 *
 *-----------------------------------------------------------------------------
 */

void
VMwareUser_OnDestroy(GtkWidget *widget, // IN: Unused
                     gpointer data)     // IN: Unused
{
   VMwareUserCleanupRpc();
   gtk_main_quit();
}


/*
 *-----------------------------------------------------------------------------
 *
 * EventQueuePump  --
 *
 *      Handle events in the event queue. This function is re-registered as a
 *      gtk_timeout every time, since we only want to be called when it is time
 *      for the next event in the queue.
 *
 * Results:
 *      1 if there were no problems, 0 otherwise
 *
 * Side effects:
 *      The events in the queue will be called, they could do anything.
 *
 *-----------------------------------------------------------------------------
 */

gint
EventQueuePump(gpointer data) // IN: Unused
{
   int ret;
   uint64 sleepUsecs;

   gtk_timeout_remove(gTimeoutId);
   ret = EventManager_ProcessNext(gEventQueue, &sleepUsecs);
   if (ret != 1) {
      Warning("Unexpected end of EventManager loop: returned value is %d.\n\n",
              ret);
      return 0;
   }
   gTimeoutId = gtk_timeout_add(sleepUsecs/1000, &EventQueuePump, NULL);
   return 1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUser_CreateWindow  --
 *
 *      Create and initializes a hidden input only window for dnd and cp.
 *
 * Results:
 *      An invisible gtk widget.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
VMwareUser_CreateWindow(void)
{
   GtkWidget *wnd;

   wnd = gtk_invisible_new();
   gtk_signal_connect(GTK_OBJECT(wnd), "destroy",
                      GTK_SIGNAL_FUNC(VMwareUser_OnDestroy), NULL);
   return wnd;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserRpcInResetCB  --
 *
 *      Callback called when the vmx has done a reset on the backdoor channel
 *
 * Results:
 *      TRUE if we reply successfully, FALSE otherwise
 *
 * Side effects:
 *      Send an "ATR" to thru the backdoor.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMwareUserRpcInResetCB(RpcInData *data)   // IN/OUT
{
   Debug("----------toolbox: Received 'reset' from vmware\n");
   if (gDnDRegistered) {
      DnD_OnReset(gHGWnd, gGHWnd);
   }
   if (gCopyPasteRegistered) {
      CopyPaste_OnReset();
   }
   return RPCIN_SETRETVALS(data, "ATR " TOOLS_DND_NAME, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserRpcInErrorCB  --
 *
 *      Callback called when their is some error on the backdoor channel.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMwareUserRpcInErrorCB(void *clientdata, char const *status)
{
   Warning("Error in the RPC recieve loop: %s\n", status);
   Warning("Another instance of VMwareUser may be running.\n\n");
   VMwareUser_OnDestroy(NULL, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserRpcInCapRegCB --
 *
 *      Handler for TCLO 'Capabilities_Register'.
 *
 * Results:
 *      TRUE if we can reply, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMwareUserRpcInCapRegCB(char const **result,     // OUT
                        size_t *resultLen,       // OUT
                        const char *name,        // IN
                        const char *args,        // IN
                        size_t argsSize,         // Unused
                        void *clientData)        // Unused
{
   Debug("VMwareUserRpcInCapRegCB got called\n");

   if (!gOpenUrlRegistered) {
      gOpenUrlRegistered = FoundryToolsDaemon_RegisterOpenUrl(gRpcIn);
   } else {
      FoundryToolsDaemon_RegisterOpenUrlCapability();
   }
   if (!gDnDRegistered) {
      gDnDRegistered = DnD_Register(gHGWnd, gGHWnd);
      if (gDnDRegistered) {
         UnityDnD state;
         state.detWnd = gGHWnd;
         state.setMode = DnD_SetMode;
         Unity_SetActiveDnDDetWnd(&state);
      }
   } else if (DnD_GetVmxDnDVersion() > 1) {
      if (!DnD_RegisterCapability()) {
         DnD_Unregister(gHGWnd, gGHWnd);
         gDnDRegistered = FALSE;
      }
   }

   if (!gCopyPasteRegistered) {
      gCopyPasteRegistered = CopyPaste_Register(gUserMainWidget);
   }

   if (gCopyPasteRegistered) {
      if (!CopyPaste_RegisterCapability()) {
         CopyPaste_Unregister(gUserMainWidget);
         gCopyPasteRegistered = FALSE;
      }
   }

   if (!HgfsServerManager_CapReg(TOOLS_DND_NAME, gHgfsServerRegistered)) {
      Debug("VMwareUserRpcInCapRegCB: Failed to register HGFS server capability.\n");
   }

   Unity_RegisterCaps();
   Resolution_RegisterCaps();

   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserRpcInSetOptionCB
 *
 *      Parse a "Set_Option" TCLO cmd from the vmx and update the local
 *      copy of the option.
 *
 * Results:
 *      TRUE if the set option command was executed.
 *      FALSE if something failed.
 *
 * Side effects:
 *      Start or stop processes (like time syncing) that could be affected
 *      by option's new value.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMwareUserRpcInSetOptionCB(char const **result,     // OUT
                           size_t *resultLen,       // OUT
                           const char *name,        // IN
                           const char *args,        // IN
                           size_t argsSize,         // Unused
                           void *clientData)        // Unused
{
   char *option;
   char *value;
   unsigned int index = 0;
   Bool ret = FALSE;
   char *retStr = NULL;

   /* parse the option & value string */
   option = StrUtil_GetNextToken(&index, args, " ");
   if (!option) {
      retStr = INVALID_COMMAND;
      goto exit;
   }
   index++; // ignore leading space before value
   value = StrUtil_GetNextToken(&index, args, "");
   if (!value) {
      retStr = INVALID_COMMAND;
      goto free_option;
   } else if (strlen(value) == 0) {
      retStr = INVALID_COMMAND;
      goto free_value;
   }

   Debug("VMwareUserRpcInSetOptionCB got option [%s], value %s\n",
         option, value);

   /*
    * Register or unregister features based on the Tools option setting or
    * unsetting.
    */
   if (strcmp(option, TOOLSOPTION_COPYPASTE) == 0) {
      if (strcmp(value, "1") == 0) {
         optionCopyPaste = TRUE;
         if (!gCopyPasteRegistered) {
            CopyPaste_Register(gUserMainWidget);
            gCopyPasteRegistered = TRUE;
         }
      } else if (strcmp(value, "0") == 0) {
         optionCopyPaste = FALSE;
         if (gCopyPasteRegistered) {
            CopyPaste_Unregister(gUserMainWidget);
            gCopyPasteRegistered = FALSE;
         }
      } else {
         retStr = INVALID_VALUE;
         goto free_value;
      }
   } else if (strcmp(option, TOOLSOPTION_ENABLEDND) == 0) {
      if (strcmp(value, "1") == 0) {
         optionDnD = TRUE;
         if (!gDnDRegistered) {
            DnD_Register(gHGWnd, gGHWnd);
            gDnDRegistered = TRUE;
         }
      } else if (strcmp(value, "0") == 0) {
         optionDnD = FALSE;
         if (gDnDRegistered) {
            DnD_Unregister(gHGWnd, gGHWnd);
            gDnDRegistered = FALSE;
         }
      } else {
         retStr = INVALID_VALUE;
         goto free_value;
      }
   } else {
         retStr = INVALID_OPTION;
         goto free_value;
   }

   ret = TRUE;
   retStr = "";
 free_value:
   free(value);
 free_option:
   free(option);
 exit:
   return RpcIn_SetRetVals(result, resultLen, retStr, ret);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserXIOErrorHandler --
 *
 *      Handler for all X I/O errors. Xlib documentation says we should not
 *      return when handling I/O errors.
 *
 * Results:
 *      On success, and assuming we're called inside the parent vmware-user
 *      process (see comment below), we attempt to restart ourselves.  On
 *      failure, we'll exit with EXIT_FAILURE.
 *
 * Side effects:
 *      This function does not return.
 *
 *-----------------------------------------------------------------------------
 */

int VMwareUserXIOErrorHandler(Display *dpy)
{
   pid_t my_pid = getpid();

   /*
    * ProcMgr_ExecAsync() needs to fork off a child to handle
    * watching the process being run.  When it dies, it will come
    * through here, so we don't want to let it shut down the Rpc
    */
   Debug("> VMwareUserXIOErrorHandler\n");
   if (my_pid == gParentPid) {
      VMwareUserCleanupRpc();
      ReloadSelf();
      exit(EXIT_FAILURE);
   } else {
      Debug("VMwareUserXIOErrorHandler hit from forked() child, not cleaning Rpc\n");
      _exit(EXIT_FAILURE);
   }

   return 1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMwareUserConfFileLoop --
 *
 *    Run the "conf file reload" loop
 *
 * Return value:
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VMwareUserConfFileLoop(void *clientData) // IN
{
   GuestApp_Dict **pConfDict = (GuestApp_Dict **) clientData;

   ASSERT(pConfDict);

   /*
    * With the addition of the Sync Driver we can get into a state
    * where the system drive is frozen, preventing the completion of
    * any disk-based I/O. The event that periodically reloads the conf
    * file then gets blocked, which blocks the main daemon thread and
    * prevents any further GuestRPC messages from getting
    * processed. This effectively deadlocks the tools daemon and among
    * other things makes it impossible to thaw disk I/O once it's been
    * frozen.
    *
    * So, we keep track of when the disks are frozen and skip doing disk
    * I/O during that time.
    */
   if (!SyncDriver_DrivesAreFrozen()) {
      if (Conf_ReloadFile(pConfDict)) {
         const char *pathName = GuestApp_GetDictEntry(*pConfDict, CONFNAME_LOGFILE);

         Debug_Set(GuestApp_GetDictEntryBool(*pConfDict, CONFNAME_LOG),
                   DEBUG_PREFIX);

         if (pathName) {
            /*
             * 2 reasons that should put pid into vmware-user log file name:
             *
             * 1. guestd runs as super user and creates log file with limited
             *    permission. If log in as non-root user, vmware-user has no
             *    permission to write to the log file. Put log to different
             *    file will resolve this.
             * 2. If user first log in as root and start vmware-user logging,
             *    the log file is still with limited permission. Later on
             *    if user re-log in as non-root user, vmware-user has no
             *    permission to that file. With pid in the log file name,
             *    everytime if vmware-user is launched, a new log file will
             *    be created with current account.
             */
            Str_Sprintf(gLogFilePath, sizeof gLogFilePath, "%s.%u",
                        pathName, (unsigned int)getpid());
            Debug_EnableToFile(gLogFilePath, FALSE);
         } else {
            Debug_EnableToFile(NULL, FALSE);
         }
      }
   }

   EventManager_Add(gEventQueue, CONF_POLL_TIME, VMwareUserConfFileLoop,
                    pConfDict);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *      This is main
 *
 * Results:
 *      Returns either EXIT_SUCCESS or EXIT_FAILURE appropriately.
 *
 * Side effects:
 *      The linux toolbox ui will run and do a variety of tricks for your
 *      amusement.
 *
 *-----------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
   gOpenUrlRegistered = FALSE;
   gDnDRegistered = FALSE;
   gCopyPasteRegistered = FALSE;
   gHgfsServerRegistered = FALSE;
   gBlockFd = -1;
   gReloadSelf = FALSE;
   gYieldBlock = FALSE;
   gSigExit = FALSE;
   struct sigaction olds[ARRAYSIZE(gSignals)];
   int index;
   GuestApp_Dict *confDict;
   const char *pathName;
#ifdef USE_NOTIFY
   Bool notifyPresent = TRUE;
#endif

   Atomic_Init();

   if (!VmCheck_IsVirtualWorld()) {
#ifndef ALLOW_TOOLS_IN_FOREIGN_VM
      Warning("vmware-user must be run inside a virtual machine.\n");
      return EXIT_SUCCESS;
#else
      runningInForeignVM = TRUE;
#endif
   }

   confDict = Conf_Load();

   /* Set to system locale. */
   setlocale(LC_CTYPE, "");
   gtk_set_locale();
   gtk_init(&argc, &argv);

   /*
    * Running more than 1 VMware user process (vmware-user) per X11 session
    * invites bad juju.  The following routine ensures that only one instance
    * will run per session.
    *
    * NB:  The lock is tied to this process, so it disappears when we exit.
    * As such, there is no corresponding unlock routine.
    */
   if (AcquireDisplayLock() == FALSE) {
      Warning("Another instance of vmware-user already running.  Exiting.\n");
      return EXIT_FAILURE;
   }

   gParentPid = getpid();

   /*
    * Parse the command line.
    *
    * We do it by hand because getopt() doesn't handle long options, and
    * getopt_long is a GNU extension
    *
    * argv[0] is the program name, as usual
    */

   for (index = 1; index < argc; index++) {
      if (strncmp(argv[index], "-", 1) == 0) {
         if (strncmp(argv[index], OPTION_BLOCK_FD, sizeof OPTION_BLOCK_FD) == 0) {
            /*
             * vmware-user runs as current active account, and can not initialize
             * blocking driver if it is not root. If guestd autostarts vmware-user,
             * guestd will first initialize it and pass block fd in with -blockFd.
             */
            if (index + 1 == argc) {
               Warning("The \""OPTION_BLOCK_FD"\" option on the command line requires an "
                       "argument.\n");
            }

            index++;
            if (!StrUtil_StrToInt(&gBlockFd, argv[index])) {
               Warning("The \""OPTION_BLOCK_FD"\" option on the command line requires a "
                       "valid integer.\n");
               gBlockFd = -1;
            }
            Debug("vmware-user got blockFd = %d\n", gBlockFd);
         } else {
            Warning("Invalid \"%s\" option on the command line.\n", argv[index]);
         }
      }
   }

   if (Signal_SetGroupHandler(gSignals, olds, ARRAYSIZE(gSignals),
                              VMwareUserSignalHandler) == 0 ) {
      Panic("vmware-user can't set signal handler\n");
   }

   Debug_Set(GuestApp_GetDictEntryBool(confDict, CONFNAME_LOG), DEBUG_PREFIX);

   pathName = GuestApp_GetDictEntry(confDict, CONFNAME_LOGFILE);
   if (pathName) {
      /*
       * 2 reasons that should put pid into vmware-user log file name:
       *
       * 1. guestd runs as super user and creates log file with limited
       *    permission. If log in as non-root user, vmware-user has no
       *    permission to write to the log file. Put log to different
       *    file will resolve this.
       * 2. If user first log in as root and start vmware-user logging,
       *    the log file is still with limited permission. Later on
       *    if user re-log in as non-root user, vmware-user has no
       *    permission to that file. With pid in the log file name,
       *    everytime if vmware-user is launched, a new log file will
       *    be created with current account.
       */
      Str_Sprintf(gLogFilePath, sizeof gLogFilePath, "%s.%u", pathName,
                  (unsigned int)getpid());
      Debug_EnableToFile(gLogFilePath, FALSE);
   } else {
      Debug_EnableToFile(NULL, FALSE);
   }

   /*
    * vmware-user runs as current active account, and can not initialize blocking
    * driver if it is not root. If guestd autostarts vmware-user, guestd will first
    * initialize it and pass block fd in. If manually run vmware-user, here will
    * try to initialize the blocking driver.
    */
   if (gBlockFd < 0) {
      gBlockFd = DnD_InitializeBlocking();
      if (gBlockFd < 0) {
         Warning("vmware-user failed to initialize blocking driver.\n");
      }
   }

   gUserMainWidget = VMwareUser_CreateWindow();
   gHGWnd = VMwareUser_CreateWindow();
   gGHWnd = VMwareUser_CreateWindow();
   /*
    * I don't want to show the window, but I need it's X window to exist.
    */
   gtk_widget_realize(gUserMainWidget);
   gtk_widget_realize(gHGWnd);
   gtk_widget_realize(gGHWnd);


   gXDisplay = GDK_WINDOW_XDISPLAY(gUserMainWidget->window);
   gXRoot = RootWindow(gXDisplay, DefaultScreen(gXDisplay));

   gEventQueue = EventManager_Init();
   if (gEventQueue == NULL) {
      Warning("Unable to create the event queue.\n\n");
      return EXIT_FAILURE;
   }

   if (runningInForeignVM) {
      Bool success = ForeignTools_Initialize(confDict, gEventQueue);
      if (!success) {
         return EXIT_FAILURE;
      }
   }

   EventManager_Add(gEventQueue, CONF_POLL_TIME, VMwareUserConfFileLoop,
                    &confDict);

   Unity_Init(confDict, NULL);
   GHI_Init(NULL, NULL);
   Resolution_Init(TOOLS_DND_NAME, gXDisplay);

#ifdef USE_NOTIFY
   if (!Notify_Init(confDict)) {
      Warning("Unable to initialize notification system.\n\n");
      notifyPresent = FALSE;
   }

   Modules_Init();
#endif

   gRpcIn = RpcIn_Construct(gEventQueue);
   if (gRpcIn == NULL) {
      Warning("Unable to create the RpcIn object.\n\n");
      return EXIT_FAILURE;
   }

   if (!RpcIn_start(gRpcIn, RPCIN_POLL_TIME, VMwareUserRpcInResetCB,
                    NULL, VMwareUserRpcInErrorCB, NULL)) {
      Warning("Unable to start the receive loop.\n\n");
      return EXIT_FAILURE;
   }

   RpcIn_RegisterCallback(gRpcIn, "Capabilities_Register",
                          VMwareUserRpcInCapRegCB, NULL);
   RpcIn_RegisterCallback(gRpcIn, "Set_Option",
                          VMwareUserRpcInSetOptionCB, NULL);

   Unity_InitBackdoor(gRpcIn);
   GHI_InitBackdoor(gRpcIn);
   Resolution_InitBackdoor(gRpcIn);

#if !defined(N_PLAT_NLM) && !defined(sun)
   {
      FoundryToolsDaemon_RegisterRoutines(gRpcIn,
                                          &confDict,
                                          gEventQueue,
                                          FALSE);
   }
#endif

   gHgfsServerRegistered = HgfsServerManager_Register(gRpcIn, TOOLS_DND_NAME);

   /*
    * Setup the some events and a pump for the EventManager.
    * We use gtk_timeouts for this.
    */
   gTimeoutId = gtk_timeout_add(0, &EventQueuePump, NULL);

   XSetIOErrorHandler(VMwareUserXIOErrorHandler);

   Pointer_Register(gUserMainWidget);

   for (;;) {
      /*
       * We'll block here until the window is destroyed or a signal is recieved
       */
      gtk_main();

      if (gSigExit) {
         break;
      }

      /* XXX Refactor this. */
      if (gYieldBlock) {
         Debug("Yielding vmblock descriptor.\n");
         if (gDnDRegistered) {
            DnD_Unregister(gHGWnd, gGHWnd);
            gDnDRegistered = FALSE;
         }
         if (gCopyPasteRegistered) {
            CopyPaste_Unregister(gUserMainWidget);
            gCopyPasteRegistered = FALSE;
         }
         if (gBlockFd >= 0 && !DnD_UninitializeBlocking(gBlockFd)) {
            Debug("vmware-user failed to uninitialize blocking.\n");
         }
         gBlockFd = -1;
         gYieldBlock = FALSE;
      }
   }

   if (runningInForeignVM) {
      ForeignTools_Shutdown();
   }

   Signal_ResetGroupHandler(gSignals, olds, ARRAYSIZE(gSignals));

   if (gBlockFd >= 0 && !DnD_UninitializeBlocking(gBlockFd)) {
      Debug("vmware-user failed to uninitialize blocking.\n");
   }

#ifdef USE_NOTIFY
   Modules_Cleanup();

   if (notifyPresent) {
      Notify_Cleanup();
   }
#endif

   /*
    * SIGUSR2 sets this to TRUE, indicating that we should relaunch ourselves.
    * This is useful during a Tools upgrade where we'd like to automatically
    * restart a new vmware-user binary.
    *
    * NB:  This just makes a best effort and relies on the user's PATH
    * environment variable.  If it fails for any reason, then we'll just exit.
    */
   if (gReloadSelf) {
      ReloadSelf();
   }

   return EXIT_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * InitGroupLeader --
 *
 *      This routine sets a few properties related to our main window created
 *      by {gdk,gtk}_init.  Specifically this routine sets the window title,
 *      sets the override_redirect X11 property, and reparents it to the root
 *      window,
 *
 *      In addition, this routine will return Xlib handles for the following
 *      objects:
 *        - Main or group leader window
 *        - Display's root window
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      Errors may be sent to stderr.
 *      Window will have a title of VMUSER_TITLE.
 *      Window, if not already directly parented by the root, will be.
 *
 *      dpy will point to our default display (ex: $DISPLAY).
 *      groupLeader will point to the window created by gtk_init().
 *      rootWindow will point to the root window on $DISPLAY.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
InitGroupLeader(Window *groupLeader,    // OUT: group leader window
                Window *rootWindow)     // OUT: root window
{
   Window myGroupLeader;
   Window myRootWindow;
   XSetWindowAttributes attr = { .override_redirect = True };

   ASSERT(groupLeader);
   ASSERT(rootWindow);

#if GTK_CHECK_VERSION(2,0,0)
   {
      GdkDisplay *gdkDisplay = gdk_display_get_default();
      GdkWindow *gdkLeader = gdk_display_get_default_group(gdkDisplay);
      myGroupLeader = GDK_WINDOW_XWINDOW(gdkLeader);
   }
#else
   /*
    * This requires digging around in gdk 1.x private code.  However, we'll
    * assume that GTK 1.x isn't going anywhere, so this should remain stable.
    */
   myGroupLeader = gdk_leader_window;
#endif

   myRootWindow = GDK_ROOT_WINDOW();

   ASSERT(myGroupLeader);
   ASSERT(myRootWindow);

   XStoreName(GDK_DISPLAY(), myGroupLeader, VMUSER_TITLE);

   /*
    * Sanity check:  Set the override redirect property on our group leader
    * window (not default), then re-parent it to the root window (default).
    * This makes sure that (a) a window manager can't re-parent our window,
    * and (b) that we remain a top-level window.
    */
   XChangeWindowAttributes(GDK_DISPLAY(), myGroupLeader, CWOverrideRedirect,
                           &attr);
   XReparentWindow(GDK_DISPLAY(), myGroupLeader, myRootWindow, 10, 10);
   XSync(GDK_DISPLAY(), FALSE);

   *groupLeader = myGroupLeader;
   *rootWindow = myRootWindow;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AcquireDisplayLock --
 *
 *      This function "locks" the display against being "claimed" by another
 *      instance of vmware-user.  It will succeed if we're the first/only
 *      instance of vmware-user, and fail otherwise.
 *
 *      NB:  This routine must be called -after- gtk_init().
 *
 *      Vmware-user enjoys per-display exclusivity using the following algorithm:
 *
 *        1.  Grab X server.  (I.e., get exclusive access.)
 *        2.  Search for top-level X windows meeting the following criteria:
 *            a.  named "vmware-user"
 *            b.  has the property "vmware-user-lock" set.
 *        3a. If any such windows described above found, then another vmware-user
 *            process is attached to this display, so we consider the display
 *            locked.
 *        3b. Else we're the only one.  Set the "vmware-user-lock" property on
 *            our top-level window.
 *        4.  Ungrab the X server.
 *
 * Results:
 *      TRUE if "lock" acquired (i.e., we're the first/only vmware-user process);
 *      otherwise FALSE.
 *
 * Side effects:
 *      The first time this routine is ever called during the lifetime of an X
 *      session, a new X11 Atom, "vmware-user-lock" is created for the lifetime
 *      of the X server.
 *
 *      The "vmware-user-lock" property may be set on this process's group leader
 *      window.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
AcquireDisplayLock(void)
{
   Display *defaultDisplay;     // Current default X11 display.
   Window rootWindow;           // Root window of defaultDisplay; used as root node
                                // passed to XQueryTree().
   Window groupLeader;          // Our instance's window group leader.  This is
                                // implicitly created by gtk_init().

   Window *children = NULL;     // Array of windows returned by XQueryTree().
   unsigned int nchildren;      // Length of children.

   Window dummy1, dummy2;       // Throwaway window IDs for XQueryTree().
   Atom lockAtom;               // Refers to the "vmware-user-lock" X11 Atom.

   unsigned int index;
   Bool alreadyLocked = FALSE;  // Set to TRUE if we discover lock is held.
   Bool retval = FALSE;

   defaultDisplay = GDK_DISPLAY();

   /*
    * Reset some of our main window's settings & fetch Xlib handles for
    * the GDK group leader and root windows.
    */
   if (InitGroupLeader(&groupLeader, &rootWindow) == FALSE) {
      Warning("%s: unable to initialize main window.\n", __func__);
      return FALSE;
   }

   /*
    * Look up the lock atom, creating it if it doesn't already exist.
    */
   lockAtom = XInternAtom(defaultDisplay, LOCK_ATOM_NAME, False);
   if (lockAtom == None) {
      Warning("%s: unable to create X11 atom: " LOCK_ATOM_NAME "\n", __func__);
      return FALSE;
   }

   /*
    * Okay, so at this point the following is done:
    *
    *   1.  Our top-level / group leader window is a child of the display's
    *       root window.
    *   2.  The window manager can't get its hands on said window.
    *   3.  We have a handle on the X11 atom which will be used to identify
    *       the X11 property used as our lock.
    */

   Debug("%s: Grabbing X server.\n", __func__);

   /*
    * Neither of these can fail, or at least not in the sense that they'd
    * return an error.  Instead we'd likely see an X11 I/O error, tearing
    * the connection down.
    *
    * XSync simply blocks until the XGrabServer request is acknowledged
    * by the server.  It makes sure that we don't continue issuing requests,
    * such as XQueryTree, until the server grants our "grab".
    */
   XGrabServer(defaultDisplay);
   XSync(defaultDisplay, False);

   /*
    * WARNING:  At this point, we have grabbed the X server.  Consider the
    * UI to be completely frozen.  Under -no- circumstances should we return
    * without ungrabbing the server first.
    */

   if (XQueryTree(defaultDisplay, rootWindow, &dummy1, &dummy2, &children,
                  &nchildren) == 0) {
      Warning("%s: XQueryTree failed\n", __func__);
      goto out;
   }

   /*
    * Iterate over array of top-level windows.  Search for those named
    * vmware-user and with the property "vmware-user-lock" set.
    *
    * If any such windows are found, then another process has already
    * claimed this X session.
    */
   for (index = 0; (index < nchildren) && !alreadyLocked; index++) {
      char *name = NULL;

      /* Skip unless window is named vmware-user. */
      if ((XFetchName(defaultDisplay, children[index], &name) == 0) ||
          (name == NULL) ||
          strcmp(name, VMUSER_TITLE)) {
         XFree(name);
         continue;
      }

      /*
       * Query the window for the "vmware-user-lock" property.
       */
      alreadyLocked = QueryX11Lock(defaultDisplay, children[index], lockAtom);
      XFree(name);
   }

   /*
    * Yay.  Lock isn't held, so go ahead and acquire it.
    */
   if (!alreadyLocked) {
      unsigned char dummy[] = "1";
      Debug("%s: Setting property " LOCK_ATOM_NAME "\n", __func__);
      /*
       * NB: Current Xlib always returns one.  This may generate a -fatal- IO
       * error, though.
       */
      XChangeProperty(defaultDisplay, groupLeader, lockAtom, lockAtom, 8,
                      PropModeReplace, dummy, sizeof dummy);
      retval = TRUE;
   }

out:
   XUngrabServer(defaultDisplay);
   XSync(defaultDisplay, False);
   XFree(children);

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueryX11Lock --
 *
 *      This is just a wrapper around XGetWindowProperty which queries the
 *      window described by <dpy,w> for the property described by lockAtom.
 *
 * Results:
 *      TRUE if property defined by parameters exists; FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
QueryX11Lock(Display *dpy,      // IN: X11 display to query
             Window w,          // IN: window to query
             Atom lockAtom)     // IN: atom used for locking
{
   Atom ptype;                  // returned property type
   int pfmt;                    // returned property format
   unsigned long np;            // returned # of properties
   unsigned long remaining;     // amount of data remaining in property
   unsigned char *data = NULL;

   if (XGetWindowProperty(dpy, w, lockAtom, 0, 1, False, lockAtom,
                          &ptype, &pfmt, &np, &remaining, &data) != Success) {
      Warning("%s: Unable to query window %lx for property %s\n", __func__, w,
              LOCK_ATOM_NAME);
      return FALSE;
   }

   /*
    * Xlib is wacky.  If the following test is true, then our property
    * didn't exist for the window in question.  As a result, `data' is
    * unset, so don't worry about the lack of XFree(data) here.
    */
   if (ptype == None) {
      return FALSE;
   }

   /*
    * We care only about the existence of the property, not its value.
    */
   XFree(data);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ReloadSelf --
 *
 *      Re-launch vmware-user by attempting to execute VMUSER_TITLE
 *      ('vmware-user'), relying on the user's search path.
 *
 * Results:
 *      On success, vmware-user is relaunched in our stead.  On failure, we
 *      exit with EXIT_FAILURE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
ReloadSelf(void)
{
   Debug("> %s\n", __func__);
   execlp(VMUSER_TITLE, VMUSER_TITLE, NULL);
   exit(EXIT_FAILURE);
}
