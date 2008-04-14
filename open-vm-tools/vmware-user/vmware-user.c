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


#include <string.h>
#include <stdlib.h>
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

#define INVALID_VALUE "Invalid option"
#define INVALID_OPTION "Invalid value"
#define INVALID_COMMAND "Invalid command format"
#define OPTION_BLOCK_FD "-blockFd"

/*
 * Forward Declarations
 */
void VMwareUser_OnDestroy(GtkWidget *widget, gpointer data);
GtkWidget* VMwareUser_Create(void);
gint EventQueuePump(gpointer data);

Bool VMwareUserRpcInResetCB    (char const **result, size_t *resultLen,
                                const char *name, const char *args,
                                size_t argsSize, void *clientData);
Bool VMwareUserRpcInSetOptionCB(char const **result, size_t *resultLen,
                                const char *name, const char *args,
                                size_t argsSize, void *clientData);
Bool VMwareUserRpcInCapRegCB   (char const **result, size_t *resultLen,
                                const char *name, const char *args,
                                size_t argsSize, void *clientData);
void VMwareUserRpcInErrorCB    (void *clientdata, char const *status);

extern Bool ForeignTools_Initialize(GuestApp_Dict *configDictionaryParam);
extern void ForeignTools_Shutdown(void);

/*
 * Globals
 */
static Bool gOpenUrlRegistered;
static Bool gResolutionSetRegistered;
static Bool gDnDRegistered;
static Bool gCopyPasteRegistered;
static Bool gHgfsServerRegistered;
static pid_t gParentPid;
static char gLogFilePath[PATH_MAX];

/*
 * From vmwareuserInt.h
 */
RpcIn *gRpcIn;
Display *gXDisplay;
GtkWidget *gUserMainWidget;
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
   SIGUSR1,
   SIGUSR2,
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
      if (gResolutionSetRegistered) {
         Resolution_Unregister();
         gResolutionSetRegistered = FALSE;
      }
      if (gDnDRegistered) {
         DnD_Unregister(gUserMainWidget);
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
 *      The application will close.
 *
 *-----------------------------------------------------------------------------
 */

void VMwareUserSignalHandler(int sig) // IN
{
   VMwareUserCleanupRpc();
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
 * VMwareUser_Create  --
 *
 *      Create and init the main window. It's hidden, but we need it to recieve
 *      X-Windows messages
 *
 * Results:
 *      The main window widget.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget*
VMwareUser_Create(void)
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
VMwareUserRpcInResetCB(char const **result,     // OUT
                       size_t *resultLen,       // OUT
                       const char *name,        // IN
                       const char *args,        // IN
                       size_t argsSize,         // Unused
                       void *clientData)        // Unused
{
   Debug("----------toolbox: Received 'reset' from vmware\n");
   if (gDnDRegistered) {
      DnD_OnReset(gUserMainWidget);
   }
   if (Unity_IsSupported()) {
      Unity_Exit();
   }
   return RpcIn_SetRetVals(result, resultLen, "ATR " TOOLS_DND_NAME,
                           TRUE);
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
   if (!gResolutionSetRegistered) {
      gResolutionSetRegistered = Resolution_Register();
   } else {
      Resolution_RegisterCapability();
   }
   if (!gDnDRegistered) {
      gDnDRegistered = DnD_Register(gUserMainWidget);
   } else if (DnD_GetVmxDnDVersion() > 1) {
      if (!DnD_RegisterCapability()) {
         DnD_Unregister(gUserMainWidget);
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
            DnD_Register(gUserMainWidget);
            gDnDRegistered = TRUE;
         }
      } else if (strcmp(value, "0") == 0) {
         optionDnD = FALSE;
         if (gDnDRegistered) {
            DnD_Unregister(gUserMainWidget);
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
 *      Handler for all X I/O errors. Xlib documentation says we should not return
 *      when handling I/O errors.
 *
 * Results:
 *      1, but really we don't ever return.
 *
 * Side effects:
 *      Exits the application.
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
   } else {
      Debug("VMwareUserXIOErrorHandler hit from forked() child, not cleaning Rpc\n");
   }
   exit(1);
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
 *      0 on success, -1 otherwise
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
   gResolutionSetRegistered = FALSE;
   gDnDRegistered = FALSE;
   gCopyPasteRegistered = FALSE;
   gHgfsServerRegistered = FALSE;
   gBlockFd = -1;
   struct sigaction olds[ARRAYSIZE(gSignals)];
   int index;
   GuestApp_Dict *confDict = Conf_Load();
   const char *pathName;

   Atomic_Init();

   if (!VmCheck_IsVirtualWorld()) {
#ifndef ALLOW_TOOLS_IN_FOREIGN_VM
      Panic("vmware-user must be run inside a virtual machine.\n");
#else
      runningInForeignVM = TRUE;
#endif
   }

   /* Set to system locale. */
   setlocale(LC_CTYPE, "");

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

   gtk_set_locale();
   gtk_init(&argc, &argv);

   gUserMainWidget = VMwareUser_Create();
   /*
    * I don't want to show the window, but I need it's X window to exist.
    */
   gtk_widget_realize(gUserMainWidget);


   gXDisplay = GDK_WINDOW_XDISPLAY(gUserMainWidget->window);
   gXRoot = RootWindow(gXDisplay, DefaultScreen(gXDisplay));

   gEventQueue = EventManager_Init();
   if (gEventQueue == NULL) {
      Warning("Unable to create the event queue.\n\n");
      return -1;
   }

   if (runningInForeignVM) {
      Bool success = ForeignTools_Initialize(confDict);
      if (!success) {
         return -1;
      }
   }

   EventManager_Add(gEventQueue, CONF_POLL_TIME, VMwareUserConfFileLoop,
                    &confDict);

   Unity_Init(confDict, NULL);
   GHI_Init(NULL, NULL);

   gRpcIn = RpcIn_Construct(gEventQueue);
   if (gRpcIn == NULL) {
      Warning("Unable to create the RpcIn object.\n\n");
      return -1;
   }

   if (!RpcIn_start(gRpcIn, RPCIN_POLL_TIME, VMwareUserRpcInResetCB,
                    NULL, VMwareUserRpcInErrorCB, NULL)) {
      Warning("Unable to start the receive loop.\n\n");
      return -1;
   }

   RpcIn_RegisterCallback(gRpcIn, "Capabilities_Register",
                          VMwareUserRpcInCapRegCB, NULL);
   RpcIn_RegisterCallback(gRpcIn, "Set_Option",
                          VMwareUserRpcInSetOptionCB, NULL);

   Unity_InitBackdoor(gRpcIn);
   GHI_InitBackdoor(gRpcIn);

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

   /*
    * We'll block here until the window is destroyed or a signal is recieved
    */
   gtk_main();

   if (runningInForeignVM) {
      ForeignTools_Shutdown();
   }

   Signal_ResetGroupHandler(gSignals, olds, ARRAYSIZE(gSignals));

   if (gBlockFd >= 0 && !DnD_UninitializeBlocking(gBlockFd)) {
      Debug("vmware-user failed to uninitialize blocking.\n");
   }
   return 0;
}
