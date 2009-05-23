/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * vixTools.c --
 *
 *    VIX commands that run in the guest OS.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
#include <WTypes.h>
#include <io.h>
#include "wminic.h"
#include "win32u.h"
#else
#include <unistd.h>
#endif

#ifdef _MSC_VER
#   include <windows.h>
#elif _WIN32
#   include "win95.h"
#endif

#include "vmware.h"
#include "procMgr.h"
#include "vm_version.h"
#include "vm_app.h"
#include "message.h"

#if defined(VMTOOLS_USE_GLIB)
#  define G_LOG_DOMAIN  "vix"
#  define Debug         g_debug
#  define Warning       g_warning
#  include <glib.h>
#else
#  include "debug.h"
#  include "eventManager.h"
#endif

#include "util.h"
#include "strutil.h"
#include "str.h"
#include "file.h"
#include "err.h"
#include "hostinfo.h"
#include "guest_os.h"
#include "conf.h"
#include "vixCommands.h"
#include "base64.h"
#include "guestInfo.h"
#include "hostinfo.h"
#include "hgfsServer.h"
#include "hgfs.h"
#include "system.h"
#include "codeset.h"
#include "posix.h"
#include "unicode.h"

#if defined(linux) || defined(_WIN32)
#include "netutil.h"
#endif

/* Only Windows and Linux use impersonation functions. */
#if !defined(__FreeBSD__) && !defined(sun)
#include "impersonate.h"
#endif

#include "vixOpenSource.h"
#include "vixTools.h"

#ifdef _WIN32
#include "registryWin32.h"
#include "win32u.h"
#endif /* _WIN32 */

#ifdef linux
#include "mntinfo.h"
#include <sys/vfs.h>
#endif

#define SECONDS_BETWEEN_POLL_TEST_FINISHED     1

#define PROCESS_CREATOR_USER_TOKEN       ((void *)1)

#define MAX_PROCESS_LIST_RESULT_LENGTH    81920

/*
 * This is used by the PRODUCT_VERSION_STRING macro.
 */
#ifndef PRODUCT_VERSION_NUMBER
#define PRODUCT_VERSION_NUMBER "1.0.0"
#endif


/*
 * State of a single asynch runProgram.
 */
typedef struct VixToolsRunProgramState {
   VixRunProgramOptions runProgramOptions;
   ProcMgr_AsyncProc    *procState;

   char                 *tempScriptFilePath;

   char                 *requestName;

   char                 *userName;
   char                 *password;

   void                 *eventQueue;
#if !defined(VMTOOLS_USE_GLIB)
   struct Event         *timeSyncEvent;
#endif
} VixToolsRunProgramState;


/*
 * Global state.
 */
static Bool thisProcessRunsAsRoot = FALSE;
static Bool allowConsoleUserOps = FALSE;
static VixToolsReportProgramDoneProcType reportProgramDoneProc = NULL;
static void *reportProgramDoneData = NULL;

static VixError VixToolsGetFileInfo(VixCommandRequestHeader *requestMsg,
                                    char **result);

#if defined(VMTOOLS_USE_GLIB)
static gboolean VixToolsMonitorAsyncProc(void *clientData);
#else
static Bool VixToolsMonitorAsyncProc(void *clientData);
#endif

static void VixToolsPrintFileInfo(char *filePathName,
                                  char *fileName,
                                  char **destPtr,
                                  char *endDestPtr);

static const char *fileInfoFormatString = "<FileInfo>"
                                          "<Name>%s</Name>"
                                          "<FileFlags>%d</FileFlags>"
                                          "<FileSize>%"FMT64"d</FileSize>"
                                          "<ModTime>%"FMT64"d</ModTime>"
                                          "</FileInfo>";

static VixError VixToolsGetTempFile(const char *tag,
                                    void *userToken,
                                    char **tempFile,
                                    int *tempFileFd);

static void VixToolsFreeRunProgramState(VixToolsRunProgramState *asyncState);

static VixError VixToolsImpersonateUser(VixCommandRequestHeader *requestMsg,
                                        void **userToken);

static const char *scriptFileBaseName = "vixScript";

static VixError VixToolsMoveFile(VixCommandRequestHeader *requestMsg);

static VixError VixToolsCreateTempFile(VixCommandRequestHeader *requestMsg,
                                       char **result);

static VixError VixToolsReadVariable(VixCommandRequestHeader *requestMsg,
                                     char **result);

static VixError VixToolsWriteVariable(VixCommandRequestHeader *requestMsg);

static VixError VixToolsListProcesses(VixCommandRequestHeader *requestMsg,
                                      char **result);

static VixError VixToolsListDirectory(VixCommandRequestHeader *requestMsg,
                                      size_t maxBufferSize,
                                      char **result);

static VixError VixToolsKillProcess(VixCommandRequestHeader *requestMsg);

static VixError VixToolsCreateDirectory(VixCommandRequestHeader *requestMsg);

static VixError VixToolsRunScript(VixCommandRequestHeader *requestMsg,
                                  char *requestName,
                                  void *eventQueue,
                                  char **result);

static VixError VixToolsOpenUrl(VixCommandRequestHeader *requestMsg);

static VixError VixToolsCheckUserAccount(VixCommandRequestHeader *requestMsg);

static VixError VixToolsProcessHgfsPacket(VixCommandHgfsSendPacket *requestMsg,
                                          char **result,
                                          size_t *resultValueResult);

static VixError VixToolsListFileSystems(VixCommandRequestHeader *requestMsg,
                                        char **result);


#if defined(__linux__) || defined(_WIN32)
static VixError VixToolsGetGuestNetworkingConfig(VixCommandRequestHeader *requestMsg,
                                                 char **resultBuffer,
                                                 size_t *resultBufferLength);
#endif

#if defined(_WIN32)
static VixError VixToolsSetGuestNetworkingConfig(VixCommandRequestHeader *requestMsg);
#endif

static VixError VixTools_Base64EncodeBuffer(char **resultValuePtr, size_t *resultValLengthPtr);

#if defined(_WIN32)
static HRESULT VixToolsEnableDHCPOnPrimary(void);

static HRESULT VixToolsEnableStaticOnPrimary(const char *ipAddr,
                                             const char *subnetMask);
#endif

static VixError VixToolsImpersonateUserImplEx(char const *credentialTypeStr,
                                              int credentialType,
                                              char const *obfuscatedNamePassword,
                                              void **userToken);

#if defined(_WIN32) || defined(linux)
static VixError VixToolsDoesUsernameMatchCurrentUser(const char *username);
#endif

static Bool VixToolsPidRefersToThisProcess(ProcMgr_Pid pid);


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_Initialize --
 *
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixTools_Initialize(Bool thisProcessRunsAsRootParam,                                // IN
                    VixToolsReportProgramDoneProcType reportProgramDoneProcParam,   // IN
                    void *clientData)                                               // IN
{
   VixError err = VIX_OK;

   thisProcessRunsAsRoot = thisProcessRunsAsRootParam;
   reportProgramDoneProc = reportProgramDoneProcParam;
   reportProgramDoneData = clientData;

   return(err);
} // VixTools_Initialize


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_SetConsoleUserPolicy --
 *
 * This allows an external client of the tools to enable/disable this security
 * setting. This may be controlled by config or higher level user settings
 * that are not available to this library.
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
VixTools_SetConsoleUserPolicy(Bool allowConsoleUserOpsParam)     // IN
{
   allowConsoleUserOps = allowConsoleUserOpsParam;
} // VixTools_SetConsoleUserPolicy


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_SetRunProgramCallback --
 *
 * Register a callback that reports when a program has completed.
 * Different clients of this library will use different IPC mechanisms for 
 * sending this message. For example, it may use the backdoor or a socket.
 * Different sockets may use different message protocols, such as the backdoor-on-a-socket
 * or the Foundry network message.
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
VixTools_SetRunProgramCallback(VixToolsReportProgramDoneProcType reportProgramDoneProcParam, // IN
                               void *clientData)                                             // IN
{
   reportProgramDoneProc = reportProgramDoneProcParam;
   reportProgramDoneData = clientData;
} // VixTools_SetRunProgramCallback


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_RunProgram --
 *
 *    Run a named program on the guest.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixTools_RunProgram(VixCommandRequestHeader *requestMsg, // IN
                    char *requestName,                   // IN
                    void *eventQueue,                    // IN
                    char **result)                       // OUT
{
   VixError err = VIX_OK;
   VixMsgRunProgramRequest *runProgramRequest;
   char *commandLine = NULL;
   char *commandLineArgs = NULL;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   int64 pid;
   static char resultBuffer[32];


   runProgramRequest = (VixMsgRunProgramRequest *) requestMsg;
   commandLine = ((char *) runProgramRequest) + sizeof(*runProgramRequest);
   if (0 == *commandLine) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
   if (runProgramRequest->commandLineArgsLength > 0) {
      commandLineArgs = commandLine 
                           + runProgramRequest->programNameLength 
                           + 1;
   }

#ifdef _WIN32
   if (runProgramRequest->runProgramOptions & VIX_RUNPROGRAM_RUN_AS_LOCAL_SYSTEM) {
      if (!VixToolsUserIsMemberOfAdministratorGroup(requestMsg)) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort; 
      }
      userToken = PROCESS_CREATOR_USER_TOKEN;
   }
#endif
  
   if (NULL == userToken) {
      err = VixToolsImpersonateUser(requestMsg, &userToken);
      if (VIX_OK != err) {
         goto abort;
      }
      impersonatingVMWareUser = TRUE;
   }

   err = VixToolsRunProgramImpl(requestName,
                                commandLine,
                                commandLineArgs,
                                runProgramRequest->runProgramOptions,
                                userToken,
                                eventQueue,
                                &pid);
   
abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   Str_Sprintf(resultBuffer, sizeof(resultBuffer), "%"FMT64"d", pid);
   *result = resultBuffer;

   return err;
} // VixTools_RunProgram


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsRunProgramImpl --
 *
 *    Run a named program on the guest.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsRunProgramImpl(char *requestName,      // IN
                       char *commandLine,      // IN
                       char *commandLineArgs,  // IN
                       int  runProgramOptions, // IN
                       void *userToken,        // IN
                       void *eventQueue,       // IN
                       int64 *pid)             // OUT,OPTIONAL
{
   VixError err = VIX_OK;
   char *fullCommandLine = NULL;
   VixToolsRunProgramState *asyncState = NULL;
   char *tempCommandLine = NULL;
   char *startProgramFileName;
   char *stopProgramFileName;
   Bool programExists;
   Bool programIsExecutable;
#if defined(_WIN32)
   Bool forcedRoot = FALSE;
   ProcMgr_ProcArgs procArgs;
   STARTUPINFO si;
#endif
#if defined(VMTOOLS_USE_GLIB)
   GSource *timer;
#endif

   if (NULL != pid) {
      *pid = (int64) -1;
   }


   tempCommandLine = Util_SafeStrdup(commandLine);
   startProgramFileName = tempCommandLine;

   while (' ' == *startProgramFileName) {
      startProgramFileName++;
   }
   if ('\"' == *startProgramFileName) {
      startProgramFileName++;
      stopProgramFileName = strstr(startProgramFileName, "\"");
   } else {
      stopProgramFileName = NULL;
   }
   if (NULL == stopProgramFileName) {
      stopProgramFileName = startProgramFileName + strlen(startProgramFileName);
   }
   *stopProgramFileName = 0;

   /*
    * Check that the program exists.
    * On linux, we run the program by exec'ing /bin/sh, and that does not
    * return a clear error code indicating that the program does not exist
    * or cannot be executed.
    * This is a common and user-correctable error, however, so we want to 
    * check for it and return a specific error code in this case.
    *
    */

   programExists = File_Exists(startProgramFileName);
   programIsExecutable = 
      (FileIO_Access(startProgramFileName, FILEIO_ACCESS_EXEC) == 
                                                       FILEIO_SUCCESS);

   free(tempCommandLine);

   if (!programExists) {
      err = VIX_E_FILE_NOT_FOUND;
      goto abort;
   }
   if (!programIsExecutable) {
      err = VIX_E_GUEST_USER_PERMISSIONS;
      goto abort;
   }

   /*
    * Build up the command line so the args are passed to the command.
    * To be safe, always put quotes around the program name. If the name
    * contains spaces (either in the file name of its directory path),
    * then the quotes are required. If the name doesn't contain spaces, then
    * unnecessary quotes don't seem to create aproblem for both Windows and
    * Linux.
    */
   if (NULL != commandLineArgs) {
      fullCommandLine = Str_Asprintf(NULL,
                                     "\"%s\" %s",
                                     commandLine,
                                     commandLineArgs);
   } else {
      fullCommandLine = Str_Asprintf(NULL,
                                     "\"%s\"",
                                     commandLine);
   }

   if (NULL == fullCommandLine) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   /*
    * Save some strings in the state.
    */
   asyncState = Util_SafeCalloc(1, sizeof *asyncState);
   asyncState->requestName = Util_SafeStrdup(requestName);
   asyncState->runProgramOptions = runProgramOptions;

#if defined(_WIN32)
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      forcedRoot = Impersonate_ForceRoot();
   }

   memset(&procArgs, 0, sizeof procArgs);
   memset(&si, 0, sizeof si);
   procArgs.hToken = (PROCESS_CREATOR_USER_TOKEN == userToken) ? NULL : userToken;
   procArgs.bInheritHandles = TRUE;
   procArgs.lpStartupInfo = &si;
   si.cb = sizeof si;
   si.dwFlags = STARTF_USESHOWWINDOW;
   si.wShowWindow = (VIX_RUNPROGRAM_ACTIVATE_WINDOW & runProgramOptions)
                     ? SW_SHOWNORMAL : SW_MINIMIZE;
   asyncState->procState = ProcMgr_ExecAsync(fullCommandLine, &procArgs);
#else
   asyncState->procState = ProcMgr_ExecAsync(fullCommandLine, NULL);
#endif

#if defined(_WIN32)
   if (forcedRoot) {
      Impersonate_UnforceRoot();
   }
#endif

   if (NULL == asyncState->procState) {
      err = VIX_E_PROGRAM_NOT_STARTED;
      goto abort;
   }

   if (NULL != pid) {
      *pid = (int64) ProcMgr_GetPid(asyncState->procState);
   }

   /*
    * Start a periodic procedure to check the app periodically
    */
   asyncState->eventQueue = eventQueue;
#if defined(VMTOOLS_USE_GLIB)
   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorAsyncProc, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(eventQueue));
   g_source_unref(timer);
#else
   asyncState->timeSyncEvent = EventManager_Add(eventQueue,
                                                SECONDS_BETWEEN_POLL_TEST_FINISHED * 100,
                                                VixToolsMonitorAsyncProc,
                                                asyncState);
   if (NULL == asyncState->timeSyncEvent) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }
#endif

   /*
    * VixToolsMonitorAsyncProc will clean asyncState up when the program finishes.
    */
   asyncState = NULL;

abort:
   free(fullCommandLine);

   if (VIX_FAILED(err)) {
      VixToolsFreeRunProgramState(asyncState);
   }

   return err;
} // VixToolsRunProgramImpl


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsMonitorAsyncProc --
 *
 *    This polls a program running in the guest to see if it has completed.
 *    It is used by the test/dev code to detect when a test application
 *    completes.
 *
 * Return value:
 *    TRUE on non-glib implementation.
 *    FALSE on glib implementation.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMTOOLS_USE_GLIB)
static gboolean
#else
static Bool
#endif
VixToolsMonitorAsyncProc(void *clientData) // IN
{
   VixError err = VIX_OK;
   VixToolsRunProgramState *asyncState;
   Bool procIsRunning = FALSE;
   int exitCode = 0;
   ProcMgr_Pid pid = -1;
   int result = -1;
#if defined(VMTOOLS_USE_GLIB)
   GSource *timer;
#endif

   asyncState = (VixToolsRunProgramState *)clientData;
   ASSERT(asyncState);

#if !defined(VMTOOLS_USE_GLIB)
   /* The event has fired: it is no longer valid */
   asyncState->timeSyncEvent = NULL;
#endif

   /*
    * Check if the program has completed.
    */
   procIsRunning = ProcMgr_IsAsyncProcRunning(asyncState->procState);
   if (!procIsRunning) {
      goto done;
   }

#if defined(VMTOOLS_USE_GLIB)
   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorAsyncProc, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(asyncState->eventQueue));
   g_source_unref(timer);
   return FALSE;
#else
   /*
    * If it's still running, check back later.
    */
   asyncState->timeSyncEvent = EventManager_Add(asyncState->eventQueue,
                                                SECONDS_BETWEEN_POLL_TEST_FINISHED * 100,
                                                VixToolsMonitorAsyncProc,
                                                asyncState);
   if (asyncState->timeSyncEvent == NULL) {
      err = VIX_E_OUT_OF_MEMORY;
      goto done;
   }

   return TRUE;
#endif

done:
   
   /*
    * We need to always check the exit code, even if there is no need to
    * report it. On POSIX systems, ProcMgr_GetExitCode() does things like
    * call waitpid() to clean up the child process.
    */
   result = ProcMgr_GetExitCode(asyncState->procState, &exitCode);
   pid = ProcMgr_GetPid(asyncState->procState);
   if (0 != result) {
      exitCode = -1;
   }
   
   /*
    * We may just be running to clean up after running a script, with the
    * results already reported.
    */
   if ((NULL != reportProgramDoneProc)
       && !(asyncState->runProgramOptions & VIX_RUNPROGRAM_RETURN_IMMEDIATELY)) {
      (*reportProgramDoneProc)(asyncState->requestName, 
                               err,
                               exitCode,
                               (int64) pid,
                               reportProgramDoneData);
   }

   VixToolsFreeRunProgramState(asyncState);

#if defined(VMTOOLS_USE_GLIB)
   return FALSE;
#else
   return TRUE;
#endif
} // VixToolsMonitorAsyncProc


/*
 *-----------------------------------------------------------------------------
 *
 * FoundryToolsDaemon_TranslateSystemErr --
 *
 *    Looks at errno/GetLastError() and returns the foundry errcode
 *    that it best maps to.
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
FoundryToolsDaemon_TranslateSystemErr(void)
{
#ifdef _WIN32
   return Vix_TranslateSystemError(GetLastError());
#else
   return Vix_TranslateSystemError(errno);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_GetToolsPropertiesImpl --
 *
 *    Get information about test features.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMTOOLS_USE_GLIB)
VixError
VixTools_GetToolsPropertiesImpl(GKeyFile *confDictRef,            // IN
                                char **resultBuffer,              // OUT
                                size_t *resultBufferLength)       // OUT
#else
VixError
VixTools_GetToolsPropertiesImpl(GuestApp_Dict **confDictRef,      // IN
                                char **resultBuffer,              // OUT
                                size_t *resultBufferLength)       // OUT
#endif
{
   VixError err = VIX_OK;
   VixPropertyListImpl propList;
   char *serializedBuffer = NULL;
   size_t serializedBufferLength = 0;
#if !defined(__FreeBSD__) && !defined(sun)
   char *guestName;
   int osFamily;
   char *packageList = NULL;
   const char *powerOffScript = NULL;
   const char *powerOnScript = NULL;
   const char *resumeScript = NULL;
   const char *suspendScript = NULL;
   char osNameFull[MAX_VALUE_LEN];
   char osName[MAX_VALUE_LEN];
   Bool foundHostName;
   char *tempDir = NULL;
   int wordSize = 32;


   VixPropertyList_Initialize(&propList);
   
   /*
    * Collect some values about the host.
    *
    * XXX: 512 is the old hardcoded value for the size of the "guestName"
    * buffer. Since Win32U_GetComputerName returns a new buffer, we do this
    * hack, since the GuestInfo API expects a pre-allocated buffer.
    */
   guestName = Util_SafeMalloc(512);
   foundHostName = GuestInfo_GetFqdn(512, guestName);
   if (!foundHostName) {
      free(guestName);
#ifdef _WIN32
      /*
       * Give it another try to read NetBIOS name.
       */
      guestName = Win32U_GetComputerName();
#else
      guestName = Util_SafeStrdup("");
#endif
   }

#ifdef _WIN32
   osFamily = GUEST_OS_FAMILY_WINDOWS;
#elif defined(N_PLAT_NLM)
   osFamily = GUEST_OS_FAMILY_NETWARE;
#else
   osFamily = GUEST_OS_FAMILY_LINUX;
#endif
   if (!(Hostinfo_GetOSName(sizeof osNameFull, sizeof osName, osNameFull,
                            osName))) {
      osNameFull[0] = 0;
      osName[0] = 0;
   }
   wordSize = Hostinfo_GetSystemBitness();
   if (wordSize <= 0) {
      wordSize = 32;
   }

   /*
    * TODO: Something with this.
    */
   packageList = "";

#if defined(VMTOOLS_USE_GLIB)
   if (confDictRef != NULL) {
      powerOffScript = g_key_file_get_string(confDictRef, "powerops", 
                                             CONFNAME_POWEROFFSCRIPT, NULL);
      powerOnScript = g_key_file_get_string(confDictRef, "powerops",
                                            CONFNAME_POWERONSCRIPT, NULL);
      resumeScript = g_key_file_get_string(confDictRef, "powerops",
                                           CONFNAME_RESUMESCRIPT, NULL);
      suspendScript = g_key_file_get_string(confDictRef, "powerops",
                                            CONFNAME_SUSPENDSCRIPT, NULL);
   }
#else
   if ((NULL != confDictRef) && (NULL != *confDictRef)) {
      powerOffScript = GuestApp_GetDictEntry(*confDictRef, CONFNAME_POWEROFFSCRIPT);
      powerOnScript = GuestApp_GetDictEntry(*confDictRef, CONFNAME_POWERONSCRIPT);
      resumeScript = GuestApp_GetDictEntry(*confDictRef, CONFNAME_RESUMESCRIPT);
      suspendScript = GuestApp_GetDictEntry(*confDictRef, CONFNAME_SUSPENDSCRIPT);
   }
#endif
   tempDir = File_GetTmpDir(TRUE);

   /*
    * Now, record these values in a property list.
    */
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_OS_VERSION,
                                   osNameFull);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_OS_VERSION_SHORT,
                                   osName);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_TOOLS_PRODUCT_NAM,
                                   PRODUCT_SHORT_NAME);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_TOOLS_VERSION,
                                   PRODUCT_VERSION_STRING);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_NAME,
                                   guestName);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_GUEST_TOOLS_API_OPTIONS,
                                    VIX_TOOLSFEATURE_SUPPORT_GET_HANDLE_STATE
                                    | VIX_TOOLSFEATURE_SUPPORT_OPEN_URL);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_GUEST_OS_FAMILY,
                                    osFamily);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_OS_PACKAGE_LIST,
                                   packageList);
   if (VIX_OK != err) {
      goto abort;
   }
   if (NULL != powerOffScript) {
      err = VixPropertyList_SetString(&propList,
                                      VIX_PROPERTY_GUEST_POWER_OFF_SCRIPT,
                                      powerOffScript);
      if (VIX_OK != err) {
         goto abort;
      }
   }
   if (NULL != resumeScript) {
      err = VixPropertyList_SetString(&propList,
                                      VIX_PROPERTY_GUEST_RESUME_SCRIPT,
                                      resumeScript);
      if (VIX_OK != err) {
         goto abort;
      }
   }
   if (NULL != powerOnScript) {
      err = VixPropertyList_SetString(&propList,
                                      VIX_PROPERTY_GUEST_POWER_ON_SCRIPT,
                                      powerOnScript);
      if (VIX_OK != err) {
         goto abort;
      }
   }
   if (NULL != suspendScript) {
      err = VixPropertyList_SetString(&propList,
                                      VIX_PROPERTY_GUEST_SUSPEND_SCRIPT,
                                      suspendScript);
      if (VIX_OK != err) {
         goto abort;
      }
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_VM_GUEST_TEMP_DIR_PROPERTY,
                                   tempDir);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_GUEST_TOOLS_WORD_SIZE,
                                    wordSize);
   if (VIX_OK != err) {
      goto abort;
   }

   /*
    * Serialize the property list to buffer then encode it.
    * This is the string we return to the VMX process.
    */
   err = VixPropertyList_Serialize(&propList,
                                   FALSE,
                                   &serializedBufferLength,
                                   &serializedBuffer);

   if (VIX_OK != err) {
      goto abort;
   }
   *resultBuffer = serializedBuffer;
   *resultBufferLength = (int)serializedBufferLength;
   serializedBuffer = NULL;

abort:
   VixPropertyList_RemoveAllWithoutHandles(&propList);
   free(guestName);
   free(serializedBuffer);
   free(tempDir);
#else
   /*
    * FreeBSD or Solaris.  Return an empty serialized property list.
    */

   VixPropertyList_Initialize(&propList);

   /*
    * Serialize the property list to buffer then encode it.
    * This is the string we return to the VMX process.
    */
   err = VixPropertyList_Serialize(&propList,
                                   FALSE,
                                   &serializedBufferLength,
                                   &serializedBuffer);
   if (VIX_OK != err) {
      goto abort;
   }
   *resultBuffer = serializedBuffer;
   *resultBufferLength = (int)serializedBufferLength;
   serializedBuffer = NULL;

abort:
   VixPropertyList_RemoveAllWithoutHandles(&propList);
   free(serializedBuffer);
#endif // __FreeBSD__ || sun
   
   return err;
} // VixTools_GetToolsPropertiesImpl


#if 0
/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsSetProperties --
 *
 *    Get information about test features.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsSetProperties(VixCommandRequestHeader *requestMsg,    // IN
                      GuestApp_Dict **confDictRef)            // IN
{
   VixError err = VIX_OK;
#if !defined(__FreeBSD__) && !defined(sun)
   size_t serialBufferLength;
   char *serialBuffer = NULL;
   VixPropertyListImpl propList;
   char *propString = NULL;
   VixMsgSetVMStateRequest *setPropertiesRequest;

   VixPropertyList_Initialize(&propList);

   setPropertiesRequest = (VixMsgSetVMStateRequest *) requestMsg;
   serialBufferLength = setPropertiesRequest->bufferSize;
   serialBuffer = ((char*)setPropertiesRequest + sizeof(*setPropertiesRequest));

   /*
    * Create a temporary property list and deserialize the buffer into.
    */
   err = VixPropertyList_Deserialize(&propList, 
                                     serialBuffer, 
                                     serialBufferLength);
   if (VIX_OK != err) {
      goto abort;
   }

   ////////////////////////////////////////////
   free(propString);
   propString = NULL;
   err = VixPropertyList_GetString(&propList,
                                   VIX_PROPERTY_GUEST_SUSPEND_SCRIPT,
                                   0,
                                   &propString);
   if ( (VIX_OK == err) && (NULL != confDictRef) && (NULL != *confDictRef)) {
      GuestApp_SetDictEntry(*confDictRef,
                            CONFNAME_SUSPENDSCRIPT,
                            propString);
   }

   ////////////////////////////////////////////
   free(propString);
   propString = NULL;
   err = VixPropertyList_GetString(&propList,
                                   VIX_PROPERTY_GUEST_RESUME_SCRIPT,
                                   0,
                                   &propString);
   if ( (VIX_OK == err) && (NULL != confDictRef) && (NULL != *confDictRef))  {
      GuestApp_SetDictEntry(*confDictRef,
                            CONFNAME_RESUMESCRIPT,
                            propString);
   }

   ////////////////////////////////
   free(propString);
   propString = NULL;
   err = VixPropertyList_GetString(&propList,
                                   VIX_PROPERTY_GUEST_POWER_ON_SCRIPT,
                                   0,
                                   &propString);
   if ( (VIX_OK == err) && (NULL != confDictRef) && (NULL != *confDictRef))  {
      GuestApp_SetDictEntry(*confDictRef,
                            CONFNAME_POWERONSCRIPT,
                            propString);
   }

   ////////////////////////////////
   free(propString);
   propString = NULL;
   err = VixPropertyList_GetString(&propList,
                                   VIX_PROPERTY_GUEST_POWER_OFF_SCRIPT,
                                   0,
                                   &propString);
   if ( (VIX_OK == err) && (NULL != confDictRef) && (NULL != *confDictRef))  {
      GuestApp_SetDictEntry(*confDictRef,
                            CONFNAME_POWEROFFSCRIPT,
                            propString);
   }


   err = VIX_OK;

abort:
   VixPropertyList_RemoveAllWithoutHandles(&propList);
#endif // __FreeBSD__ || sun

   return err;
} // VixToolsSetProperties
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsReadRegistry --
 *
 *    Read an int from the registry on the guest.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsReadRegistry(VixCommandRequestHeader *requestMsg,  // IN
                     char **result)                        // OUT
{
#ifdef _WIN32
   VixError err = VIX_OK;
   char *registryPathName = NULL;
   int valueInt = 0;
   int errResult;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   char *valueStr = NULL;
   VixMsgRegistryRequest *registryRequest;

   /*
    * Parse the argument
    */
   registryRequest = (VixMsgRegistryRequest *) requestMsg;
   registryPathName = ((char *) registryRequest) + sizeof(*registryRequest);
   if (0 == *registryPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   if (VIX_PROPERTYTYPE_INTEGER == registryRequest->expectedRegistryKeyType) {
      errResult = Registry_ReadInteger(registryPathName, &valueInt);
      if (ERROR_SUCCESS != errResult) {
         err = Vix_TranslateSystemError(errResult);
         goto abort;
      }

      valueStr = Str_Asprintf(NULL, "%d", valueInt);
      if (NULL == valueStr) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
   } else if (VIX_PROPERTYTYPE_STRING == registryRequest->expectedRegistryKeyType) {
      errResult = Registry_ReadString(registryPathName, &valueStr);
      if (ERROR_SUCCESS != errResult) {
         err = Vix_TranslateSystemError(errResult);
         goto abort;
      }
   } else {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   if (NULL == valueStr) {
      valueStr = Util_SafeStrdup("");
   }
   *result = valueStr;

   return err;

#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
} // VixToolsReadRegistry


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsWriteRegistry --
 *
 *    Write an integer to the registry on the guest.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsWriteRegistry(VixCommandRequestHeader *requestMsg) // IN
{
#ifdef _WIN32
   VixError err = VIX_OK;
   char *registryPathName = NULL;
   char *registryData = NULL;
   int errResult;
   int intValue;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgRegistryRequest *registryRequest;

   /*
    * Parse the argument
    */
   registryRequest = (VixMsgRegistryRequest *) requestMsg;
   registryPathName = ((char *) registryRequest) + sizeof(*registryRequest);
   if (0 == *registryPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
   registryData = registryPathName + registryRequest->registryKeyLength + 1;

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   if (VIX_PROPERTYTYPE_INTEGER == registryRequest->expectedRegistryKeyType) {
      intValue = *((int *) registryData);

      errResult = Registry_WriteInteger(registryPathName, intValue);
      if (ERROR_SUCCESS != errResult) {
         err = Vix_TranslateSystemError(errResult);
         goto abort;
      }
   } else if (VIX_PROPERTYTYPE_STRING == registryRequest->expectedRegistryKeyType) {
      errResult = Registry_WriteString(registryPathName, registryData);
      if (ERROR_SUCCESS != errResult) {
         err = Vix_TranslateSystemError(errResult);
         goto abort;
      }
   } else {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }


abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;

#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
} // VixToolsWriteRegistry


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsDeleteObject --
 *
 *    Delete a file on the guest.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsDeleteObject(VixCommandRequestHeader *requestMsg)  // IN
{
   VixError err = VIX_OK;
   char *pathName = NULL;
   int resultInt;
   Bool resultBool;
   Bool success;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgSimpleFileRequest *fileRequest;

   /*
    * Parse the argument
    */
   fileRequest = (VixMsgSimpleFileRequest *) requestMsg;
   pathName = ((char *) fileRequest) + sizeof(*fileRequest);
   if (0 == *pathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   ///////////////////////////////////////////
   if (VIX_COMMAND_DELETE_GUEST_FILE == requestMsg->opCode) {
      /*
       * if pathName is an invalid symbolic link, we still want to delete it.
       */
      if (FALSE == File_IsSymLink(pathName)) {
         if (!(File_Exists(pathName))) {      
            err = VIX_E_FILE_NOT_FOUND;
            goto abort;
         }

         if (!(File_IsFile(pathName))) {
            err = VIX_E_NOT_A_FILE;
            goto abort;
         }
      }

      resultInt = File_UnlinkNoFollow(pathName);
      if (0 != resultInt) {
         err = FoundryToolsDaemon_TranslateSystemErr();
      }
   ///////////////////////////////////////////
   } else if (VIX_COMMAND_DELETE_GUEST_REGISTRY_KEY == requestMsg->opCode) {
#ifdef _WIN32
      err = VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#else
      err = VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
   ///////////////////////////////////////////
   } else if (VIX_COMMAND_DELETE_GUEST_DIRECTORY == requestMsg->opCode) {
      resultBool = File_Exists(pathName);
      if (!resultBool) {
         err = VIX_E_FILE_NOT_FOUND;
         goto abort;
      }
      resultBool = File_IsDirectory(pathName);
      if (!resultBool) {
         err = VIX_E_NOT_A_DIRECTORY;
         goto abort;
      }
      success = File_DeleteDirectoryTree(pathName);
      if (!success) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   ///////////////////////////////////////////
   } else if (VIX_COMMAND_DELETE_GUEST_EMPTY_DIRECTORY == requestMsg->opCode) {
      resultBool = File_Exists(pathName);
      if (!resultBool) {
         err = VIX_E_FILE_NOT_FOUND;
         goto abort;
      }
      resultBool = File_IsDirectory(pathName);
      if (!resultBool) {
         err = VIX_E_NOT_A_DIRECTORY;
         goto abort;
      }
      success = File_DeleteEmptyDirectory(pathName);
      if (!success) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   ///////////////////////////////////////////
   } else {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsDeleteObject


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsObjectExists --
 *
 *    Find a file on the guest.
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsObjectExists(VixCommandRequestHeader *requestMsg,  // IN
                     char **result)                        // OUT
{
   VixError err = VIX_OK;
   char *pathName = NULL;
   int resultInt = 0;
   Bool resultBool;
   static char resultBuffer[32];
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgSimpleFileRequest *fileRequest;

   /*
    * Parse the argument
    */
   fileRequest = (VixMsgSimpleFileRequest *) requestMsg;
   pathName = ((char *) fileRequest) + sizeof(*fileRequest);
   if (0 == *pathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   /*
    * Do the action appropriate for this type of object.
    */
   ///////////////////////////////////////////
   if (VIX_COMMAND_GUEST_FILE_EXISTS == requestMsg->opCode) {
      resultBool = File_IsFile(pathName);
      if (resultBool) {
         resultInt = 1;
      } else {
         resultInt = 0;
      }
   ///////////////////////////////////////////
   } else if (VIX_COMMAND_REGISTRY_KEY_EXISTS == requestMsg->opCode) {
#ifdef _WIN32
      resultInt = Registry_KeyExists(pathName);
#else
      resultInt = 0;
      err = VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
   ///////////////////////////////////////////
   } else if (VIX_COMMAND_DIRECTORY_EXISTS == requestMsg->opCode) {
      resultBool = File_IsDirectory(pathName);
      if (resultBool) {
         resultInt = 1;
      } else {
         resultInt = 0;
      }
   ///////////////////////////////////////////
   } else {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   Str_Sprintf(resultBuffer, sizeof(resultBuffer), "%d", resultInt);
   *result = resultBuffer;

   return err;
} // VixToolsObjectExists


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsOpenUrl --
 *
 *    Open a URL on the guest.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsOpenUrl(VixCommandRequestHeader *requestMsg) // IN
{
   VixError err = VIX_OK;
   char *url = NULL;
   char *windowState = "default";
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgOpenUrlRequest *openUrlRequest;

   openUrlRequest = (VixMsgOpenUrlRequest *) requestMsg;
   url = ((char *) openUrlRequest) + sizeof(*openUrlRequest);

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   /* Actually open the URL. */
   if (!GuestApp_OpenUrl(url, strcmp(windowState, "maximize") == 0)) {
      err = VIX_E_FAIL;
      Debug("Failed to open the url \"%s\"\n", url);
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsOpenUrl


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsCreateTempFile --
 *
 *    Create a temporary file on the guest.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsCreateTempFile(VixCommandRequestHeader *requestMsg,   // IN
                       char **result)                         // OUT: UTF-8
{
   VixError err = VIX_OK;
   char *filePathName = NULL;
   int fd = -1;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgCreateTempFileRequest *makeTempFileRequest;

   makeTempFileRequest = (VixMsgCreateTempFileRequest *) requestMsg;

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   err = VixToolsGetTempFile("vmware", userToken, &filePathName, &fd);
   if (VIX_FAILED(err)) {
      goto abort;
   }
   
   /*
    * Just close() the file, since we're not going to use it.
    */

   if (close(fd) < 0) {
      Debug("Unable to close a file, errno is %d.\n", errno);
   }

   *result = filePathName;

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsCreateTempFile


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsReadVariable --
 *
 *    Read an environment variable in the guest. The name of the environment
 *    variable is expected to be in UTF-8.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsReadVariable(VixCommandRequestHeader *requestMsg,   // IN
                     char **result)                         // OUT: UTF-8
{
   VixError err = VIX_OK;
   char *value = "";
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgReadVariableRequest *readRequest;
   char *valueName = NULL;

   readRequest = (VixMsgReadVariableRequest *) requestMsg;
   valueName = ((char *) readRequest) + sizeof(*readRequest);

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   switch (readRequest->variableType) {
   case VIX_GUEST_ENVIRONMENT_VARIABLE:
      /*
       * Alwasy get environment variable for the current user, even if the
       * current user is root/administrator
       */
      value = System_GetEnv(FALSE, valueName);
      if (NULL == value) {
         value = Util_SafeStrdup("");
      }
      break;

   case VIX_GUEST_CONFIG:
   case VIX_VM_CONFIG_RUNTIME_ONLY:
   case VIX_VM_GUEST_VARIABLE:
   default:
      err = VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
      break;
   } // switch (readRequest->variableType)

   *result = value;

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsReadVariable


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsWriteVariable --
 *
 *    Write an environment variable in the guest. The name of the environment
 *    variable and its value are expected to be in UTF-8.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    Yes, may change the environment variables.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsWriteVariable(VixCommandRequestHeader *requestMsg)   // IN
{
   VixError err = VIX_OK;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgWriteVariableRequest *writeRequest;
   char *valueName = NULL;
   char *value = NULL;
   int result;

   writeRequest = (VixMsgWriteVariableRequest *) requestMsg;
   err = VixMsg_ParseWriteVariableRequest(writeRequest, &valueName, &value);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   switch (writeRequest->variableType) {
   case VIX_GUEST_ENVIRONMENT_VARIABLE:
#if !defined(_WIN32)
      /*
       * On Linux, we only allow root to set environment variables.
       * On Windows we can put ACLs on the registry keys, but we can't do that
       * on Linux. The threat is if an unpriveleged user changes path or lib
       * settings, which could cause a later call from a priveleged user
       * to RunProgramInGuest to misbehave by using compromised libs or environment.
       */
      if (1 != Util_HasAdminPriv()) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
#endif
      /* 
       * At this point, we want to set environmental variable for current
       * user, even if the current user is root/administrator
       */
      result = System_SetEnv(FALSE, valueName, value);
      if (0 != result) {
         err = FoundryToolsDaemon_TranslateSystemErr();
      }
      break;

   case VIX_GUEST_CONFIG:
   case VIX_VM_CONFIG_RUNTIME_ONLY:
   case VIX_VM_GUEST_VARIABLE:
   default:
      err = VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
      break;
   } // switch (readRequest->variableType)   

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsWriteVariable


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsMoveFile --
 *
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsMoveFile(VixCommandRequestHeader *requestMsg)        // IN
{
   VixError err = VIX_OK;
   char *srcFilePathName = NULL;
   char *destFilePathName = NULL;
   Bool success;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixCommandRenameFileRequest *renameRequest;

   renameRequest = (VixCommandRenameFileRequest *) requestMsg;
   srcFilePathName = ((char *) renameRequest) + sizeof(*renameRequest);
   destFilePathName = srcFilePathName + renameRequest->oldPathNameLength + 1;
   if ((0 == *srcFilePathName) || (0 == *destFilePathName)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   /*
    * Be careful. Renaming a file to itself can cause it to be deleted.
    * This should be a no-op anyway.
    */
#if !defined(sun) && !defined(__FreeBSD__)
   if (File_IsSameFile(srcFilePathName, destFilePathName)) {
      err = VIX_OK;
      goto abort;
   }
#else
   /*
    * Do something better for Solaris and FreeBSD once we support them.
    */
   if (strcmp(srcFilePathName, destFilePathName) == 0) {
      err = VIX_OK;
      goto abort;
   }
#endif

   /*
    * pre-check the dest arg -- File_Rename() will return
    * diff err codes depending on OS, so catch it up front (bug 133165)
    */
   if (File_IsDirectory(destFilePathName)) {
      err = VIX_E_ALREADY_EXISTS;
      goto abort;
   }

   success = File_Rename(srcFilePathName, destFilePathName);
   if (!success) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsMoveFile


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListProcesses --
 *
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsListProcesses(VixCommandRequestHeader *requestMsg, // IN
                      char **result)                       // OUT
{
   VixError err = VIX_OK;
   int index;
   static char resultBuffer[MAX_PROCESS_LIST_RESULT_LENGTH];
   ProcMgr_ProcList *procList = NULL;
   char *destPtr;
   char *endDestPtr;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;

   destPtr = resultBuffer;
   *destPtr = 0;

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   procList = ProcMgr_ListProcesses();
   if (NULL == procList) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   endDestPtr = resultBuffer + sizeof(resultBuffer);
   for (index = 0; index < procList->procCount; index++) {
      destPtr += Str_Sprintf(destPtr,
                             endDestPtr - destPtr,
                             "<proc><name>%s</name><pid>%d</pid>"
#if defined(_WIN32)
                             "<debugged>%d</debugged>"
#endif
                             "<user>%s</user><start>%d</start></proc>",
                             procList->procCmdList[index],
                             (int) procList->procIdList[index],
#if defined(_WIN32)
                             (int) procList->procDebugged[index],
#endif
                             (NULL == procList->procOwnerList
                               || NULL == procList->procOwnerList[index])
                                 ? ""
                                 : procList->procOwnerList[index],
                             (NULL == procList->startTime)
                                 ? 0
                                 : (int) procList->startTime[index]);
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);
   ProcMgr_FreeProcList(procList);

   *result = resultBuffer;

   return(err);
} // VixToolsListProcesses


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsKillProcess --
 *
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsKillProcess(VixCommandRequestHeader *requestMsg) // IN
{
   VixError err = VIX_OK;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixCommandKillProcessRequest *killProcessRequest;
   
   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   killProcessRequest = (VixCommandKillProcessRequest *) requestMsg;

   /*
    * This is here for two reasons:
    *  1) If you kill this process, then it cannot report back to
    *     you that the command succeeded.
    *  2) On Linux, you can either always send a signal to youself,
    *     or it just compares the source and destination real, effective,
    *     and saved UIDs. Anyway, no matter who guestd is impersonating,
    *     this will succeed. However, normally a regular user cannot
    *     kill guestd, and should not be able to because of an implementation
    *     detail.
    */
   if (VixToolsPidRefersToThisProcess(killProcessRequest->pid)) {
      err = VIX_E_GUEST_USER_PERMISSIONS;
      goto abort;
   }

   if (!ProcMgr_KillByPid(killProcessRequest->pid)) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsKillProcess


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsCreateDirectory --
 *
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsCreateDirectory(VixCommandRequestHeader *requestMsg)  // IN
{
   VixError err = VIX_OK;
   char *dirPathName = NULL;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgCreateFileRequest *dirRequest = NULL;

   dirRequest = (VixMsgCreateFileRequest *) requestMsg;
   dirPathName = ((char *) dirRequest) + sizeof(*dirRequest);
   if (0 == *dirPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   if (File_Exists(dirPathName)) {
      err = VIX_E_FILE_ALREADY_EXISTS;
      goto abort;
   }

   if (!(File_CreateDirectoryHierarchy(dirPathName))) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsCreateDirectory


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListDirectory --
 *
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsListDirectory(VixCommandRequestHeader *requestMsg,    // IN
                      size_t maxBufferSize,                   // IN
                      char **result)                          // OUT
{
   VixError err = VIX_OK;
   char *dirPathName = NULL;
   char *fileList = NULL;
   char **fileNameList = NULL;
   size_t resultBufferSize = 0;
   size_t lastGoodResultBufferSize = 0;
   int numFiles = 0;
   int lastGoodNumFiles = 0;
   int fileNum;
   char *currentFileName;
   char *destPtr;
   char *endDestPtr;
   Bool impersonatingVMWareUser = FALSE;
   size_t formatStringLength;
   void *userToken = NULL;
   VixMsgListDirectoryRequest *listRequest = NULL;
   VixMsgSimpleFileRequest *legacyListRequest = NULL;
   Bool truncated = FALSE;
   int64 offset = 0;
   Bool isLegacyFormat;

   legacyListRequest = (VixMsgSimpleFileRequest *) requestMsg;
   if (legacyListRequest->fileOptions & VIX_LIST_DIRECTORY_USE_OFFSET) {
      /*
       * Support updated ListDirectory format.
       */
      listRequest = (VixMsgListDirectoryRequest *) requestMsg;
      offset = listRequest->offset;
      dirPathName = ((char *) requestMsg) + sizeof(*listRequest);
      isLegacyFormat = FALSE;
   } else {
      /*
       * Support legacy ListDirectory format.
       */
      dirPathName = ((char *) requestMsg) + sizeof(*legacyListRequest);
      isLegacyFormat = TRUE;
   }

   if (0 == *dirPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   if (!(File_IsDirectory(dirPathName))) {
      err = VIX_E_NOT_A_DIRECTORY;
      goto abort;
   }

   numFiles = File_ListDirectory(dirPathName, &fileNameList);
   if (numFiles < 0) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   /*
    * Calculate the size of the result buffer and keep track of the
    * max number of entries we can store.
    */
   resultBufferSize = 3; // truncation bool + space + '\0'
   lastGoodResultBufferSize = resultBufferSize;
   ASSERT_NOT_IMPLEMENTED(lastGoodResultBufferSize < maxBufferSize);
   formatStringLength = strlen(fileInfoFormatString);

   for (fileNum = offset; fileNum < numFiles; fileNum++) {
      currentFileName = fileNameList[fileNum];

      resultBufferSize += formatStringLength;
      resultBufferSize += strlen(currentFileName);
      resultBufferSize += 2; // DIRSEPC chars
      resultBufferSize += 10 + 20 + 20; // properties + size + modTime

      if (resultBufferSize < maxBufferSize) {
         /*
          * lastGoodNumFiles is a count (1 based), while fileNum is
          * an array index (zero based).  So lastGoodNumFiles is
          * fileNum + 1.
          */
         lastGoodNumFiles = fileNum + 1;
         lastGoodResultBufferSize = resultBufferSize;
      } else {
         truncated = TRUE;
         break;
      }
   }
   resultBufferSize = lastGoodResultBufferSize;

   /*
    * Print the result buffer.
    */
   fileList = Util_SafeMalloc(resultBufferSize);
   destPtr = fileList;
   endDestPtr = fileList + resultBufferSize;

   /*
    * Indicate if we have a truncated buffer with "1 ", otherwise "0 ".
    * This should only happen for non-legacy requests.
    */
   if (!isLegacyFormat) {
      if ((destPtr + 2) < endDestPtr) {
         *destPtr++ = truncated ? '1' : '0';
         *destPtr++ = ' ';
      } else {
         ASSERT(0);
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
   }

   for (fileNum = offset; fileNum < lastGoodNumFiles; fileNum++) {
      /* File_ListDirectory never returns "." or ".." */
      char *pathName;

      currentFileName = fileNameList[fileNum];

      pathName = Str_SafeAsprintf(NULL, "%s%s%s", dirPathName, DIRSEPS,
                                  currentFileName);

      VixToolsPrintFileInfo(pathName, currentFileName, &destPtr, endDestPtr);

      free(pathName);
   } // for (fileNum = 0; fileNum < lastGoodNumFiles; fileNum++)
   *destPtr = '\0';

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   if (NULL == fileList) {
      fileList = Util_SafeStrdup("");
   }
   *result = fileList;

   if (NULL != fileNameList) {
      for (fileNum = 0; fileNum < numFiles; fileNum++) {
         free(fileNameList[fileNum]);
      }
      free(fileNameList);
   }

   return err;
} // VixToolsListDirectory


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetFileInfo --
 *
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsGetFileInfo(VixCommandRequestHeader *requestMsg,    // IN
                    char **result)                          // OUT
{
   VixError err = VIX_OK;
   char *resultBuffer = NULL;
   size_t resultBufferSize = 0;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   char *destPtr;
   char *filePathName;

   filePathName = ((char *) requestMsg) + sizeof(VixMsgSimpleFileRequest);
   if (0 == *filePathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
   
   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   if (!(File_Exists(filePathName))) {
      err = VIX_E_FILE_NOT_FOUND;
      goto abort;
   }

   /*
    * Calculate the size of the result buffer.
    */
   resultBufferSize = strlen(fileInfoFormatString)
                           + 1 // strlen("")
                           + 20 + 20 + 10; // space for the modTime, size and flags.
   resultBuffer = Util_SafeMalloc(resultBufferSize);

   /*
    * Print the result buffer
    */
   destPtr = resultBuffer;
   VixToolsPrintFileInfo(filePathName, "", &destPtr, resultBuffer + resultBufferSize);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   if (NULL == resultBuffer) {
      resultBuffer = Util_SafeStrdup("");
   }
   *result = resultBuffer;

   return err;
} // VixToolsGetFileInfo


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsPrintFileInfo --
 *
 *    This does not retrieve some of the more interesting properties, like
 *    read-only, owner name, and permissions. I'll add those later.
 *
 *    This also does not yet provide UTF-8 versions of some of the File_ functions,
 *    so that may create problems on international guests.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
VixToolsPrintFileInfo(char *filePathName,     // IN
                      char *fileName,         // IN
                      char **destPtr,         // IN
                      char *endDestPtr)       // OUT
{
   int64 fileSize = 0;
   int64 modTime;
   int32 fileProperties = 0;

   modTime = File_GetModTime(filePathName);
   if (File_IsDirectory(filePathName)) {
      fileProperties |= VIX_FILE_ATTRIBUTES_DIRECTORY;
   } else {
      if (File_IsSymLink(filePathName)) {
         fileProperties |= VIX_FILE_ATTRIBUTES_SYMLINK;
      }
      fileSize = File_GetSize(filePathName);
   }

   *destPtr += Str_Sprintf(*destPtr, 
                           endDestPtr - *destPtr, 
                           fileInfoFormatString,
                           fileName,
                           fileProperties,
                           fileSize,
                           modTime);
} // VixToolsPrintFileInfo


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsCheckUserAccount --
 *
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsCheckUserAccount(VixCommandRequestHeader *requestMsg) // IN
{
   VixError err = VIX_OK;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsCheckUserAccount


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsRunScript --
 *
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsRunScript(VixCommandRequestHeader *requestMsg,  // IN
                  char *requestName,                    // IN
                  void *eventQueue,                     // IN
                  char **result)                        // OUT
{
   VixError err = VIX_OK;
   char *propertiesString = NULL;
   char *script = NULL;
   char *interpreterName = NULL;
   char *fileSuffix = "";
   Bool impersonatingVMWareUser = FALSE;
   VixToolsRunProgramState *asyncState = NULL;
   void *userToken = NULL;
   char *tempDirPath = NULL;
   char *tempScriptFilePath = NULL;
   char *fullCommandLine = NULL;
   int var;
   int fd = -1;
   int writeResult;
   Bool programExists;
   Bool programIsExecutable;
   int64 pid = (int64) -1;
   static char resultBuffer[32];
   VixMsgRunScriptRequest *scriptRequest;
   const char *interpreterFlags = "";
#if defined(_WIN32)
   Bool forcedRoot = FALSE;
   ProcMgr_ProcArgs procArgs;
#endif
#if defined(VMTOOLS_USE_GLIB)
   GSource *timer;
#endif

   scriptRequest = (VixMsgRunScriptRequest *) requestMsg;
   interpreterName = ((char *) scriptRequest) + sizeof(*scriptRequest);
   propertiesString = interpreterName + scriptRequest->interpreterNameLength + 1;
   script = propertiesString + scriptRequest->propertiesLength + 1;

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;


if (0 == *interpreterName) {
#ifdef _WIN32
      //interpreterName = "cmd.exe";
      fileSuffix = ".bat";
#else
      interpreterName = "/bin/sh";
#endif
   }
   
   if (*interpreterName) {
      programExists = File_Exists(interpreterName);

      /*
       * TODO: replace FileIO_Access with something more UTF8/forward-
       * thinking.
       */

      programIsExecutable = 
         (FileIO_Access(interpreterName, FILEIO_ACCESS_EXEC) ==
                                                   FILEIO_SUCCESS);
      if (!programExists) {
         err = VIX_E_FILE_NOT_FOUND;
         goto abort;
      }
      if (!programIsExecutable) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
   }
   
   /*
    * Create a temporary file that we can run as a script.
    * TODO: Plumb a file suffix/extention throught to the File
    * module's code, so that we can avoid duplicating this code.
    */

#ifdef _WIN32
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      err = VixToolsGetUserTmpDir(userToken, &tempDirPath);

      /* 
       * Don't give up if VixToolsGetUserTmpDir() failed. It might just
       * have failed to load DLLs, so we might be running on Win 9x.
       * Just fall through to use the old fashioned File_GetTmpDir().
       */

      err = VIX_OK;
   }
#endif

   if (NULL == tempDirPath) {
      tempDirPath = File_GetTmpDir(TRUE);
      if (NULL == tempDirPath) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   }
   for (var = 0; var <= 0xFFFFFFFF; var++) {
      free(tempScriptFilePath);
      tempScriptFilePath = Str_Asprintf(NULL, 
                                        "%s"DIRSEPS"%s%d%s", 
                                        tempDirPath, 
                                        scriptFileBaseName, 
                                        var, 
                                        fileSuffix);
      if (NULL == tempScriptFilePath) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      
      fd = Posix_Open(tempScriptFilePath, // UTF-8
                      O_CREAT | O_EXCL
#if defined(_WIN32)
                     | O_BINARY
#endif
#if defined(linux) && defined(GLIBC_VERSION_21)
                     | O_LARGEFILE
#endif
                     | O_RDWR,
                      0600);
      if (fd >= 0) {
         break;
      }

      if (errno != EEXIST) {
         /*
          * While persistence is generally a worthwhile trail, if something
          * happens to the temp directory while we're using it (e.g., someone
          * deletes it), we should not try 4+ billion times.
          */
         break;
      }
   }
   if (fd < 0) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      Debug("Unable to create a temporary file, errno is %d.\n", errno);
      goto abort;
   }

#if defined(_WIN32)
   writeResult = _write(fd, script, (unsigned int)strlen(script));
#else
   writeResult = write(fd, script, strlen(script));
#endif

   if (writeResult < 0) {
      /*
       * Yes, I'm duplicating code by running this check before the call to 
       * close(), but if close() succeeds it will clobber the errno, causing
       * something confusing to be reported to the user.
       */
      err = FoundryToolsDaemon_TranslateSystemErr();
      Debug("Unable to write the script to the temporary file, errno is %d.\n", errno);
      if (close(fd) < 0) {
         Debug("Unable to close a file, errno is %d\n", errno);
      }
      goto abort;
   }

   if (close(fd) < 0) {
      /*
       * If close() fails, we don't want to try to run the script. According to
       * the man page:
       *    "Not checking the return value of close is a common but nevertheless
       *     serious programming error.  It is quite possible that errors on a
       *     previous write(2) operation  are first reported at the final close. Not
       *     checking the return value when closing the file may lead to silent loss
       *     of data.  This can especially be observed with NFS and disk quotas."
       */
      err = FoundryToolsDaemon_TranslateSystemErr();
      Debug("Unable to close a file, errno is %d\n", errno);
      goto abort;
   }

   if ((NULL != interpreterName) && (*interpreterName)) {
      fullCommandLine = Str_Asprintf(NULL, // resulting string length
                                     "\"%s\" %s \"%s\"", 
                                     interpreterName,
                                     interpreterFlags,
                                     tempScriptFilePath);
   } else {
      fullCommandLine = Str_Asprintf(NULL,  // resulting string length
                                     "\"%s\"", 
                                     tempScriptFilePath);
   }

   if (NULL == fullCommandLine) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   /*
    * Save some strings in the state.
    */
   asyncState = Util_SafeCalloc(1, sizeof *asyncState);
   asyncState->tempScriptFilePath = tempScriptFilePath;
   tempScriptFilePath = NULL;
   asyncState->requestName = Util_SafeStrdup(requestName);
   asyncState->runProgramOptions = scriptRequest->scriptOptions;

#if defined(_WIN32)
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      forcedRoot = Impersonate_ForceRoot();
   }
   memset(&procArgs, 0, sizeof procArgs);
   procArgs.hToken = (PROCESS_CREATOR_USER_TOKEN == userToken) ? NULL : userToken;
   procArgs.bInheritHandles = TRUE;
   asyncState->procState = ProcMgr_ExecAsync(fullCommandLine, &procArgs);
#else
   asyncState->procState = ProcMgr_ExecAsync(fullCommandLine, NULL);
#endif

#if defined(_WIN32)
   if (forcedRoot) {
      Impersonate_UnforceRoot();
   }
#endif

   if (NULL == asyncState->procState) {
      err = VIX_E_PROGRAM_NOT_STARTED;
      goto abort;
   }

   pid = (int64) ProcMgr_GetPid(asyncState->procState);

   asyncState->eventQueue = eventQueue;
#if defined(VMTOOLS_USE_GLIB)
   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorAsyncProc, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(eventQueue));
   g_source_unref(timer);
#else
   /*
    * Start a periodic procedure to check the app periodically.
    * We always run this (regardless of whether the VIX_RUNPROGRAM_RETURN_IMMEDIATELY
    * flag is set), since we need to clean up the temp file after the script is
    * done running.
    */
   asyncState->timeSyncEvent = EventManager_Add(eventQueue,
                                                SECONDS_BETWEEN_POLL_TEST_FINISHED * 100,
                                                VixToolsMonitorAsyncProc,
                                                asyncState);
   if (NULL == asyncState->timeSyncEvent) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }
#endif

   /*
    * VixToolsMonitorAsyncProc will clean asyncState up when the program finishes.
    */
   asyncState = NULL;

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   if (VIX_FAILED(err)) {
      VixToolsFreeRunProgramState(asyncState);
   }

   free(fullCommandLine);
   free(tempDirPath);
   free(tempScriptFilePath);

   Str_Sprintf(resultBuffer, sizeof(resultBuffer), "%"FMT64"d", pid);
   *result = resultBuffer;

   return err;
} // VixToolsRunScript


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsImpersonateUser --
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsImpersonateUser(VixCommandRequestHeader *requestMsg,   // IN
                        void **userToken)                      // OUT
{
   VixError err = VIX_OK;
   char *credentialField;
   VixCommandNamePassword *namePasswordStruct;
   int credentialType;

   Debug(">%s\n", __FUNCTION__);

   credentialField = ((char *) requestMsg)
                           + requestMsg->commonHeader.headerLength 
                           + requestMsg->commonHeader.bodyLength;

   namePasswordStruct = (VixCommandNamePassword *) credentialField;
   credentialField += sizeof(VixCommandNamePassword);
   credentialType = requestMsg->userCredentialType;

   err = VixToolsImpersonateUserImplEx(NULL, 
                                       credentialType,
                                       credentialField, 
                                       userToken);
   if ((VIX_OK != err)
         && ((VIX_USER_CREDENTIAL_NAME_PASSWORD_OBFUSCATED == credentialType)
               || (VIX_USER_CREDENTIAL_NAME_PASSWORD == credentialType))) {
      /*
       * Windows does not allow you to login with an empty password. Only
       * the console allows this login, which means the console does not
       * call the simple public LogonUser api.
       *
       * See the description for ERROR_ACCOUNT_RESTRICTION.
       * For example, the error codes are described here:
       *      http://support.microsoft.com/kb/155012
       */
#ifdef _WIN32
      if (namePasswordStruct->passwordLength <= 0) {
         err = VIX_E_EMPTY_PASSWORD_NOT_ALLOWED_IN_GUEST;
      }
#endif
   }

   Debug("<%s\n", __FUNCTION__);

   return(err);
} // VixToolsImpersonateUser


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsImpersonateUserImpl --
 *
 *    Little compatability wrapper for legacy Foundry Tools implementations. 
 *
 * Return value:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
VixToolsImpersonateUserImpl(char const *credentialTypeStr,         // IN
                            int credentialType,                    // IN
                            char const *obfuscatedNamePassword,    // IN
                            void **userToken)                      // OUT
{
   return(VIX_OK == VixToolsImpersonateUserImplEx(credentialTypeStr,
                                                  credentialType,
                                                  obfuscatedNamePassword,
                                                  userToken));
} // VixToolsImpersonateUserImpl


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsImpersonateUserImplEx --
 *
 *   On Windows:
 *   To retrieve the security context of another user 
 *   call LogonUser to log the user whom you want to impersonate on to the 
 *   local computer, specifying the name of the user account, the user's 
 *   domain, and the user's password. This function returns a pointer to 
 *   a handle to the access token of the logged-on user as an out parameter.
 *   Call ImpersonateLoggedOnUser using the handle to the access token obtained 
 *   in the call to LogonUser.
 *   Run RegEdt32 to load the registry hive of the impersonated user manually. 
 *
 * Return value:
 *    VIX_OK on success, or an appropriate error code on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsImpersonateUserImplEx(char const *credentialTypeStr,         // IN
                              int credentialType,                    // IN
                              char const *obfuscatedNamePassword,    // IN
                              void **userToken)                      // OUT
{
   VixError err = VIX_E_GUEST_USER_PERMISSIONS;

   if (NULL != userToken) {
      *userToken = NULL;
   }

///////////////////////////////////////////////////////////////////////
#if defined(__FreeBSD__) || defined(sun)
   err = VIX_E_NOT_SUPPORTED;
///////////////////////////////////////////////////////////////////////
#elif defined(_WIN32) || defined(linux)
   {
      Bool success = FALSE;
      AuthToken authToken;
      char *unobfuscatedUserName = NULL;
      char *unobfuscatedPassword = NULL;

      if (NULL != credentialTypeStr) {
         if (!StrUtil_StrToInt(&credentialType, credentialTypeStr)) {
            /*
             * This is an internal error, since the VMX supplies this string.
             */
            err = VIX_E_FAIL;
            goto abort;
         }
      }

      /*
       * If the VMX asks to be root, then we allow them.
       * The VMX will make sure that only it will pass this value in,
       * and only when the VM and host are configured to allow this.
       */
      if ((VIX_USER_CREDENTIAL_ROOT == credentialType) 
            && (thisProcessRunsAsRoot)) {
         if (NULL != userToken) {
            *userToken = PROCESS_CREATOR_USER_TOKEN;
         }
         err = VIX_OK;
         goto abort;
      }

      /*
       * If the VMX asks to be root, then we allow them.
       * The VMX will make sure that only it will pass this value in,
       * and only when the VM and host are configured to allow this.
       */
      if ((VIX_USER_CREDENTIAL_CONSOLE_USER == credentialType) 
            && ((allowConsoleUserOps) || !(thisProcessRunsAsRoot))) {
         if (NULL != userToken) {
            *userToken = PROCESS_CREATOR_USER_TOKEN;
         }
         err = VIX_OK;
         goto abort;
      }

      /*
       * If the VMX asks us to run commands in the context of the current
       * user, make sure that the user who requested the command is the
       * same as the current user.
       * We don't need to make sure the password is valid (in fact we should
       * not receive one) because the VMX should have validated the
       * password by other means. Currently it sends it to the Tools daemon.
       */
      if (VIX_USER_CREDENTIAL_NAMED_INTERACTIVE_USER == credentialType) {
         if (!thisProcessRunsAsRoot) {
            success = VixMsg_DeObfuscateNamePassword(obfuscatedNamePassword,
                                                     &unobfuscatedUserName,
                                                     &unobfuscatedPassword);
            if (!success || (NULL == unobfuscatedUserName)) {
               err = VIX_E_FAIL;
               goto abort;
            }

            /*
             * Make sure that the user who requested the command is the
             * current user.
             */

            err = VixToolsDoesUsernameMatchCurrentUser(unobfuscatedUserName);
            if (VIX_OK != err) {
               goto abort;
            }

            if (NULL != userToken) {
               *userToken = PROCESS_CREATOR_USER_TOKEN;
            }

            goto abort;
         } else {
            /*
             * This should only be sent to vmware-user, not guestd.
             * Something is wrong.
             */
            ASSERT(0);
            err = VIX_E_FAIL;
            goto abort;
         }
      }

      /*
       * Other credential types, like guest, are all turned into a name/password
       * by the VMX. If this is something else, then we are talking to a newer
       * version of the VMX.
       */
      if ((VIX_USER_CREDENTIAL_NAME_PASSWORD != credentialType) 
            && (VIX_USER_CREDENTIAL_NAME_PASSWORD_OBFUSCATED != credentialType)) {
         err = VIX_E_NOT_SUPPORTED;
         goto abort;
      }

      success = VixMsg_DeObfuscateNamePassword(obfuscatedNamePassword,
                                               &unobfuscatedUserName,
                                               &unobfuscatedPassword);
      if (!success) {
         err = VIX_E_FAIL;
         goto abort;
      }

      authToken = Auth_AuthenticateUser(unobfuscatedUserName, unobfuscatedPassword);
      if (NULL == authToken) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
      if (NULL != userToken) {
         *userToken = (void *) authToken;
      }

#ifdef _WIN32
      success = Impersonate_Do(unobfuscatedUserName, authToken);
#else
      /*
       * Use a tools-special version of user impersonation, since 
       * lib/impersonate model isn't quite what we want on linux.
       */
      success = ProcMgr_ImpersonateUserStart(unobfuscatedUserName, authToken);
#endif
      if (!success) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }

      err = VIX_OK;

abort:
      free(unobfuscatedUserName);
      Util_ZeroFreeString(unobfuscatedPassword);
   }

#else
   err = VIX_E_NOT_SUPPORTED;
#endif   // else linux

   return err;
} // VixToolsImpersonateUserImplEx


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsUnimpersonateUser --
 *
 *
 * Return value:
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
VixToolsUnimpersonateUser(void *userToken)
{
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
#if defined(_WIN32)
      Impersonate_Undo();
#elif defined(linux)
      ProcMgr_ImpersonateUserStop();
#endif
   }
} // VixToolsUnimpersonateUser


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsLogoutUser --
 *
 *
 * Return value:
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
VixToolsLogoutUser(void *userToken)    // IN
{
   if (PROCESS_CREATOR_USER_TOKEN == userToken) {
      return;
   }

#if !defined(__FreeBSD__) && !defined(sun)
   if (NULL != userToken) {
      AuthToken authToken = (AuthToken) userToken;
      Auth_CloseToken(authToken);
   }
#endif
} // VixToolsLogoutUser


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsFreeRunProgramState --
 *
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
VixToolsFreeRunProgramState(VixToolsRunProgramState *asyncState) // IN
{
   if (NULL == asyncState) {
      return;
   }

   if (NULL != asyncState->tempScriptFilePath) {
      /*
       * Use UnlinkNoFollow() since we created the file and we know it is not
       * a symbolic link.
       */
      File_UnlinkNoFollow(asyncState->tempScriptFilePath);
   }
   if (NULL != asyncState->procState) {
      ProcMgr_Free(asyncState->procState);
   }

   free(asyncState->requestName);
   free(asyncState->tempScriptFilePath);
   free(asyncState);
} // VixToolsFreeRunProgramState


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetTempFile --
 *
 *     Creates and opens a new temporary file, appropriate for the user
 *     that is represented by the userToken.
 *
 * Return value:
 *     VixError
 *     *tempFile will point to the name of the temporary file, or NULL on error.
 *     *fd will be the file descriptor of the temporary file, or -1 on error.
 *
 * Side effects:
 *     The temp file will be created and opened.
 *
 *-----------------------------------------------------------------------------
 */

static VixError 
VixToolsGetTempFile(const char *tag,   // IN
                    void *userToken,   // IN
                    char **tempFile,   // OUT
                    int *tempFileFd)   // OUT
{
   VixError err = VIX_E_FAIL;
   char *tempDirPath = NULL;
   char *tempFilePath = NULL;
   int fd = -1;

   if (NULL == tempFile || NULL == tempFileFd) {
      ASSERT(0);
      goto abort;
   }

   *tempFile = NULL;
   *tempFileFd = -1;

#ifdef _WIN32
   /*
    * Don't try this if we're not impersonating anyone, since either
    *   1) It's running as System and System won't have the environment variables
    *      we want.
    *   2) It's the console user and then it's running within the user's session and
    *      we don't know who we're impersonating and also the environment variables
    *      will be directly present in the environment, so GetTempPath will do the
    *      trick.
    */
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      err = VixToolsGetUserTmpDir(userToken, &tempDirPath);

      /* 
       * Don't give up if VixToolsGetUserTmpDir() failed. It might just
       * have failed to load DLLs, so we might be running on Win 9x.
       * Just fall through to use the old fashioned File_MakeTemp().
       */

      if (VIX_SUCCEEDED(err)) {
         fd = File_MakeTempEx(tempDirPath, tag, &tempFilePath);
         if (fd < 0) {
            err = FoundryToolsDaemon_TranslateSystemErr();
            goto abort;
         }
      }
      err = VIX_OK;
   }
#endif

   /*
    * We need to use File_MakeTemp and not Util_MakeSafeTemp.
    * File_MakeTemp uses File_GetTmpDir, while Util_MakeSafeTemp 
    * uses Util_GetSafeTmpDir. We can't use Util_GetSafeTmpDir 
    * because much of win32util.c which gets used in that call creates 
    * dependencies on code that won't run on win9x.
    */
   if (NULL == tempFilePath) {
      fd = File_MakeTemp(tag, &tempFilePath);
      if (fd < 0) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   }

   *tempFile = tempFilePath;
   *tempFileFd = fd;
   err = VIX_OK;

abort:
   free(tempDirPath);

   return err;
} // VixToolsGetTempFile


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsProcessHgfsPacket --
 *
 *    This sends a packet to the HGFS server in the guest.
 *    We pass in the user credential type and authenication
 *    information as strings, followed by the actual HGFS packet
 *    to send to the HGFS Server in the guest Tools.
 *    The recipient of this string is ToolsDaemonHgfsImpersonated,
 *    which lives in foundryToolsDaemon.c.  It parses the authentication
 *    information, impersonates a user in the guest using
 *    ToolsDaemonImpersonateUser, and then calls HgfsServerManager_ProcessPacket
 *    to issue the HGFS packet to the HGFS Server.  The HGFS Server
 *    replies with an HGFS packet, which will be forwarded back to
 *    us and handled in VMAutomationOnBackdoorCallReturns.
 *
 * Results:
 *    VIX_OK if success, VixError error code otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsProcessHgfsPacket(VixCommandHgfsSendPacket *requestMsg,   // IN
                          char **result,                          // OUT
                          size_t *resultValueResult)              // OUT
{
   VixError err = VIX_OK;
   void *userToken = NULL;
   Bool impersonatingVMWareUser = FALSE;
   char *hgfsPacket;
   size_t hgfsPacketSize = 0;
   static char hgfsReplyPacket[HGFS_LARGE_PACKET_MAX];

   if ((NULL == requestMsg) || (0 == requestMsg->hgfsPacketSize)) {
      ASSERT(0);
      err = VIX_E_FAIL;
      goto abort;
   }
   
   err = VixToolsImpersonateUser((VixCommandRequestHeader *) requestMsg,
                                 &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   hgfsPacket = ((char *) requestMsg) + sizeof(*requestMsg);
   hgfsPacketSize = requestMsg->hgfsPacketSize;

#if !defined(N_PLAT_NLM) && !defined(__FreeBSD__)
   /*
    * Impersonation was okay, so let's give our packet to
    * the HGFS server and forward the reply packet back.
    */
   HgfsServer_ProcessPacket(hgfsPacket,        // packet in buf
                            hgfsReplyPacket,   // packet out buf
                            &hgfsPacketSize,   // in/out size
                            0);                // in flags
#endif

   if (NULL != resultValueResult) {
      *resultValueResult = hgfsPacketSize;
   }
   if (NULL != result) {
      *result = hgfsReplyPacket;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;
} // VixToolsProcessHgfsPacket


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListFileSystems --
 *
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsListFileSystems(VixCommandRequestHeader *requestMsg, // IN
                        char **result)                       // OUT
{
   VixError err = VIX_OK;
   static char resultBuffer[MAX_PROCESS_LIST_RESULT_LENGTH];
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   char *destPtr;
#ifdef linux
   char *endDestPtr;
   VixCommandListFileSystemsRequest *lfReq = (VixCommandListFileSystemsRequest *) requestMsg;
   MNTHANDLE fp;
   DECLARE_MNTINFO(mnt);
   const char *mountfile = NULL;
#endif

   Debug(">%s\n", __FUNCTION__);

   destPtr = resultBuffer;
   *destPtr = 0;

   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

#if defined(_WIN32)
   // XXX TBD
   err = VIX_E_NOT_SUPPORTED;
#elif defined(linux)

   endDestPtr = resultBuffer + sizeof(resultBuffer);

   if (lfReq->options & VIX_FILESYSTEMS_HIDE_NETWORK) {
      mountfile = "/etc/fstab";
   } else {
      mountfile = "/etc/mtab";
   }

   fp = Posix_Setmntent(mountfile, "r");
   if (fp == NULL) {
      Warning("failed to open mount file\n");
      err = VIX_E_FILE_NOT_FOUND;
      goto abort;
   }

   while (GETNEXT_MNTINFO(fp, mnt)) {
      struct statfs statfsbuf;
      uint64 size, freeSpace;

      if (Posix_Statfs(MNTINFO_MNTPT(mnt), &statfsbuf)) {
         Warning("%s unable to stat mount point %s\n",
                 __FUNCTION__, MNTINFO_MNTPT(mnt));
         continue;
      }
      size = (uint64) statfsbuf.f_blocks * (uint64) statfsbuf.f_bsize;
      freeSpace = (uint64) statfsbuf.f_bfree * (uint64) statfsbuf.f_bsize;
      destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr,
                             "<filesystem>"
                             "<name>%s</name>"
                             "<size>%"FMT64"u</size>"
                             "<freeSpace>%"FMT64"u</freeSpace>"
                             "<type>%s</type>"
                             "</filesystem>",
                             MNTINFO_NAME(mnt),
                             size,
                             freeSpace,
                             MNTINFO_FSTYPE(mnt));

   }
   CLOSE_MNTFILE(fp);
#else
   err = VIX_E_NOT_SUPPORTED;
#endif

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   *result = resultBuffer;

   Debug("<%s\n", __FUNCTION__);

   return(err);
} // VixToolsListFileSystems


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetGuestNetworkingConfig --
 *
 *
 * Return value:
 *    VIX_OK on success
 *    VixError on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__linux__) || defined(_WIN32)
VixError 
VixToolsGetGuestNetworkingConfig(VixCommandRequestHeader *requestMsg,   // IN
                                 char **resultBuffer,                   // OUT
                                 size_t *resultBufferLength)            // OUT
{
   VixError err = VIX_OK;
   VixPropertyListImpl propList;
   char *serializedBuffer = NULL;
   size_t serializedBufferLength = 0;
   GuestNic *nicEntry = NULL;
   VmIpAddress *ipAddr;

   ASSERT(NULL != requestMsg);
   ASSERT(NULL != resultBuffer);
   ASSERT(NULL != resultBufferLength);

   VixPropertyList_Initialize(&propList);

   nicEntry = NetUtil_GetPrimaryNic();
   if (NULL == nicEntry) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   ipAddr = &nicEntry->ips.ips_val[0];

   /*
    *  Now, record these values in a property list.
    */
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_VM_IP_ADDRESS,
                                   ipAddr->ipAddress);
   if (VIX_OK != err) {
      goto abort;
   }

#if defined(_WIN32)
   err = VixPropertyList_SetBool(&propList,
                                 VIX_PROPERTY_VM_DHCP_ENABLED,
                                 ipAddr->dhcpEnabled);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_VM_SUBNET_MASK,
                                   ipAddr->subnetMask);
   if (VIX_OK != err) {
      goto abort;
   }
#endif

   /*
    * Serialize the property list to buffer then encode it.
    * This is the string we return to the VMX process.
    */
   err = VixPropertyList_Serialize(&propList,
                                   FALSE,
                                   &serializedBufferLength,
                                   &serializedBuffer);

   if (VIX_OK != err) {
      goto abort;
   }
   *resultBuffer = serializedBuffer;
   *resultBufferLength = serializedBufferLength;
   serializedBuffer = NULL;


abort:
   VixPropertyList_RemoveAllWithoutHandles(&propList);
   VMX_XDR_FREE(xdr_GuestNic, nicEntry);
   free(nicEntry);

   return err;
} // VixToolsGetGuestNetworkingConfig
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsSetGuestNetworkingConfig --
 *
 *
 * Return value:
 *    Vix_OK on success
 *    VixError on failure
 *
 * Side effects:
 *    networking configuration on hte guest may change
 *
 *-----------------------------------------------------------------------------
 */

#if defined(_WIN32)
VixError 
VixToolsSetGuestNetworkingConfig(VixCommandRequestHeader *requestMsg)    // IN
{
   VixError err = VIX_OK;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgSetGuestNetworkingConfigRequest *setGuestNetworkingConfigRequest = NULL;
   VixPropertyListImpl propList;
   VixPropertyValue *propertyPtr = NULL;
   char *messageBody = NULL;
   char ipAddr[IP_ADDR_SIZE]; 
   char subnetMask[IP_ADDR_SIZE];
   Bool dhcpEnabled = FALSE;
   HRESULT hrErr;

   ASSERT(NULL != requestMsg);

   ipAddr[0] = '\0';  
   subnetMask[0] = '\0';
   
   err = VixToolsImpersonateUser(requestMsg, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   setGuestNetworkingConfigRequest = (VixMsgSetGuestNetworkingConfigRequest *)requestMsg;
   messageBody = (char *) requestMsg + sizeof(*setGuestNetworkingConfigRequest);

   VixPropertyList_Initialize(&propList);
   err = VixPropertyList_Deserialize(&propList, 
                                     messageBody, 
                                     setGuestNetworkingConfigRequest -> bufferSize);
   if (VIX_OK != err) {
      goto abort;
   }

   propertyPtr = propList.properties;
   while (propertyPtr != NULL) {
      switch (propertyPtr->propertyID) {
      ///////////////////////////////////////////
      case VIX_PROPERTY_VM_DHCP_ENABLED:
         if (propertyPtr->value.boolValue) { 
            dhcpEnabled = TRUE;
         }
         break;

      /////////////////////////////////////////// 
      case VIX_PROPERTY_VM_IP_ADDRESS:
         if (strlen(propertyPtr->value.strValue) < sizeof ipAddr) {
            Str_Strcpy(ipAddr,
                       propertyPtr->value.strValue, 
                       sizeof ipAddr);
            } else {
               err = VIX_E_INVALID_ARG;
               goto abort;
            }
         break;

      ///////////////////////////////////////////
      case VIX_PROPERTY_VM_SUBNET_MASK:
         if (strlen(propertyPtr->value.strValue) < sizeof subnetMask) {
            Str_Strcpy(subnetMask, 
                       propertyPtr->value.strValue,
                       sizeof subnetMask); 
         } else {
            err = VIX_E_INVALID_ARG;
            goto abort;
         }
         break;   
         
      ///////////////////////////////////////////
      default:
         /*
          * Be more tolerant.  Igonore unknown properties.
          */
         break;
      } // switch 

      propertyPtr = propertyPtr->next;
   } // while {propList.properties != NULL)

   if (dhcpEnabled) {
      hrErr = VixToolsEnableDHCPOnPrimary();
   } else {
      if (('\0' != ipAddr[0]) ||
          ('\0' != subnetMask[0])) { 
         hrErr = VixToolsEnableStaticOnPrimary(ipAddr, subnetMask);
      } else {
         /*
          * Setting static ip, both ip and subnet mask are missing
          */
         err = VIX_E_MISSING_REQUIRED_PROPERTY;
         goto abort;
      }
   }
   if (S_OK != hrErr) {
      if (FACILITY_WIN32 != HRESULT_FACILITY(hrErr)) {
         err = Vix_TranslateCOMError(hrErr);
      } else {
         err = Vix_TranslateSystemError(hrErr);
      } 
   }

abort:
   VixPropertyList_RemoveAllWithoutHandles(&propList);

   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   return err;

} // VixToolsSetGuestNetworkingConfig
#endif


#if defined(_WIN32) || defined(linux)
/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsDoesUsernameMatchCurrentUser --
 *
 *    Check if the provider username matches the current user.
 *
 * Return value:
 *    VIX_OK if it does, otherwise an appropriate error code.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsDoesUsernameMatchCurrentUser(const char *username)  // IN
{
   VixError err = VIX_E_FAIL;

#ifdef _WIN32
   char *currentUser = NULL;
   DWORD currentUserSize = 0;
   
   /*
    * For Windows, get the name of the owner of this process, then
    * compare it to the provided username.
    */
   if (!Win32U_GetUserName(currentUser, &currentUserSize)) {
      if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }

      currentUser = Util_SafeMalloc(currentUserSize);

      if (!Win32U_GetUserName(currentUser, &currentUserSize)) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   }

   if (0 != Unicode_CompareIgnoreCase(username, currentUser)) {
      err = VIX_E_INTERACTIVE_SESSION_USER_MISMATCH;
      goto abort;
   }

   err = VIX_OK;

abort:
   free(currentUser);

#else /* Below is the POSIX case. */
   uid_t currentUid;
   struct passwd pwd;
   struct passwd *ppwd = &pwd;
   char *buffer = NULL; // a pool of memory for Posix_Getpwnam_r() to use.
   size_t bufferSize;
   
   /*
    * For POSIX systems, look up the uid of 'username', and compare
    * it to the uid of the owner of this process. This handles systems
    * where multiple usernames map to the name user.
    */
   
   /*
    * Get the maximum size buffer needed by getpwuid_r.
    * Multiply by 4 to compensate for the conversion to UTF-8 by
    * the Posix_Getpwnam_r() wrapper.
    */
   bufferSize = (size_t) sysconf(_SC_GETPW_R_SIZE_MAX) * 4;

   buffer = Util_SafeMalloc(bufferSize);

   if (Posix_Getpwnam_r(username, &pwd, buffer, bufferSize, &ppwd) != 0 ||
       NULL == ppwd) {
      /* 
       * This username should exist, since it should have already
       * been validated by guestd. Assume it is a system error.
       */
      err = FoundryToolsDaemon_TranslateSystemErr();
      Warning("Unable to get the uid for username %s.\n", username);
      goto abort;
   }

   /*
    * In the Windows version, GetUserNameW() returns the name of the
    * user the thread is impersonating (if it is impersonating someone),
    * so geteuid() seems to be the moral equivalent.
    */
   currentUid = geteuid();

   if (currentUid != ppwd->pw_uid) {
      err = VIX_E_INTERACTIVE_SESSION_USER_MISMATCH;
      goto abort;
   }

   err = VIX_OK;

 abort:
   Util_ZeroFree(buffer, bufferSize);

#endif
   
   return err;
}
#endif  // #if defined(_WIN32) || defined(linux)


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsPidRefersToThisProcess --
 *
 *    Determines if the given pid refers to the current process, in
 *    that if it passed to the appropriate OS-specific process killing
 *    function, will this process get killed.
 *
 * Return value:
 *    TRUE if killing pid kills us, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
VixToolsPidRefersToThisProcess(ProcMgr_Pid pid)  // IN
{
#ifdef _WIN32
   return (GetCurrentProcessId() == pid);
#else
   /*
    * POSIX is complicated. Pid could refer to this process directly,
    * be 0 which kills all processes in this process's group, be -1
    * which kill everything to which it can send a signal, or be -1 times
    * the process group ID of this process.
    */
   return ((getpid() == pid) || (0 == pid) || (-1 == pid) ||
           ((pid < -1) && (getpgrp() == (pid * -1))));
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_ProcessVixCommand --
 *
 *
 * Return value:
 *    VIX_OK on success
 *    VixError on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMTOOLS_USE_GLIB)
VixError
VixTools_ProcessVixCommand(VixCommandRequestHeader *requestMsg,   // IN
                           char *requestName,                     // IN
                           size_t maxResultBufferSize,            // IN
                           GKeyFile *confDictRef,                 // IN
                           GMainLoop *eventQueue,                 // IN
                           char **resultBuffer,                   // OUT
                           size_t *resultLen,                     // OUT
                           Bool *deleteResultBufferResult)        // OUT
#else
VixError
VixTools_ProcessVixCommand(VixCommandRequestHeader *requestMsg,   // IN
                           char *requestName,                     // IN
                           size_t maxResultBufferSize,            // IN
                           GuestApp_Dict **confDictRef,           // IN
                           DblLnkLst_Links *eventQueue,           // IN
                           char **resultBuffer,                   // OUT
                           size_t *resultLen,                     // OUT
                           Bool *deleteResultBufferResult)        // OUT
#endif
{
   VixError err = VIX_OK;
   char *resultValue = NULL;
   size_t resultValueLength = 0;
   Bool mustSetResultValueLength = TRUE;
   Bool deleteResultValue = FALSE;
  

   if (NULL != resultBuffer) {
      *resultBuffer = NULL;
   }
   if (NULL != resultLen) {
      *resultLen = 0;
   }
   if (NULL != deleteResultBufferResult) {
      *deleteResultBufferResult = FALSE;
   }

   Debug("%s: command %d\n", __FUNCTION__, requestMsg->opCode);

   switch (requestMsg->opCode) {
      ////////////////////////////////////
      case VIX_COMMAND_CHECK_USER_ACCOUNT:
      case VIX_COMMAND_LOGOUT_IN_GUEST:
         err = VixToolsCheckUserAccount(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_GET_PROPERTY:
      case VIX_COMMAND_GET_TOOLS_STATE:
         err = VixTools_GetToolsPropertiesImpl(confDictRef,
                                               &resultValue,
                                               &resultValueLength);
         if (VIX_FAILED(err)) {
            /*
             * VixTools_GetToolsPropertiesImpl failed, so resultVal is still NULL,
             * so let it get replaced with the empty string at the abort label.
             */
            goto abort;
         }

         /*
          * resultVal always points to something heap-allocated after this point
          */
         deleteResultValue = TRUE;

         err = VixTools_Base64EncodeBuffer(&resultValue, &resultValueLength);
         mustSetResultValueLength = FALSE;

         break;

      ////////////////////////////////////
      case VIX_COMMAND_LIST_PROCESSES:
         err = VixToolsListProcesses(requestMsg, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_LIST_DIRECTORY:
         err = VixToolsListDirectory(requestMsg,
                                     maxResultBufferSize,
                                     &resultValue);
         deleteResultValue = TRUE;
         break;

      ////////////////////////////////////
      case VIX_COMMAND_DELETE_GUEST_FILE:
      case VIX_COMMAND_DELETE_GUEST_REGISTRY_KEY:
      case VIX_COMMAND_DELETE_GUEST_DIRECTORY:
      case VIX_COMMAND_DELETE_GUEST_EMPTY_DIRECTORY:
         err = VixToolsDeleteObject(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_REGISTRY_KEY_EXISTS:
      case VIX_COMMAND_GUEST_FILE_EXISTS:
      case VIX_COMMAND_DIRECTORY_EXISTS:
         err = VixToolsObjectExists(requestMsg, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_READ_REGISTRY:
         err = VixToolsReadRegistry(requestMsg, &resultValue);
         deleteResultValue = TRUE;
         break;

      ////////////////////////////////////
      case VIX_COMMAND_WRITE_REGISTRY:
         err = VixToolsWriteRegistry(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_KILL_PROCESS:
         err = VixToolsKillProcess(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_CREATE_DIRECTORY:
         err = VixToolsCreateDirectory(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_MOVE_GUEST_FILE:
         err = VixToolsMoveFile(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_RUN_SCRIPT_IN_GUEST:
         err = VixToolsRunScript(requestMsg, requestName, eventQueue, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_RUN_PROGRAM:
         err = VixTools_RunProgram(requestMsg, requestName, eventQueue, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_OPEN_URL:
         err = VixToolsOpenUrl(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_CREATE_TEMPORARY_FILE:
         err = VixToolsCreateTempFile(requestMsg, &resultValue);
         deleteResultValue = TRUE;
         break;

      ///////////////////////////////////
      case VIX_COMMAND_READ_VARIABLE:
         err = VixToolsReadVariable(requestMsg, &resultValue);
         deleteResultValue = TRUE;
         break;

      ///////////////////////////////////
      case VIX_COMMAND_WRITE_VARIABLE:
         err = VixToolsWriteVariable(requestMsg);
         break;

      ///////////////////////////////////
      case VIX_COMMAND_GET_FILE_INFO:
         err = VixToolsGetFileInfo(requestMsg, &resultValue);
         deleteResultValue = TRUE;
         break;

      ///////////////////////////////////
      case VMXI_HGFS_SEND_PACKET_COMMAND:
         err = VixToolsProcessHgfsPacket((VixCommandHgfsSendPacket *) requestMsg,
                                         &resultValue,
                                         &resultValueLength);
         deleteResultValue = FALSE; // TRUE;
         mustSetResultValueLength = FALSE;
         break;

#if defined(__linux__) || defined(_WIN32)
      ////////////////////////////////////
      case VIX_COMMAND_GET_GUEST_NETWORKING_CONFIG:
         err = VixToolsGetGuestNetworkingConfig(requestMsg,
                                                &resultValue,
                                                &resultValueLength);
         if (VIX_FAILED(err)) {
            /*
             * VixToolsGetGuestNetworkingConfig() failed, so resultVal is still NULL, 
             * so let it get replaced with the empty string at the abort label.  
             */
            goto abort;
         }

         /*
          * resultVal always points to something heap-allocated after this point
          */
         deleteResultValue = TRUE;
         mustSetResultValueLength = FALSE;
         break;
#endif

#if defined(_WIN32)
      ////////////////////////////////////
      case VIX_COMMAND_SET_GUEST_NETWORKING_CONFIG:
         err = VixToolsSetGuestNetworkingConfig(requestMsg);
         break;
#endif

#if 0
      /*
       * Do not register for commands that have not been tested and publicly released.
       * When we do support these commands, we do not want to accidentally run
       * pre-release untested versions.
       */
      ////////////////////////////////////
      case VIX_COMMAND_SET_HANDLE_STATE:
         err = VixToolsSetProperties(requestMsg, confDictRef);
         break;
#endif

      ////////////////////////////////////
      case VIX_COMMAND_LIST_FILESYSTEMS:
         err = VixToolsListFileSystems(requestMsg, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      default:
         break;
   } // switch (requestMsg->opCode)

abort:
   if (NULL == resultValue) {
      // Prevent "(null)" from getting sprintf'ed into the result buffer
      resultValue = "";
      deleteResultValue = FALSE;
   }

   /*
    * Some commands return both a result and its length. Some return just
    * the result. Others return nothing at all. Previously, we assumed that
    * all results are based on plain-text, but this is incorrect (for example,
    * VixToolsProcessHgfsPacket will return a binary packet).
    *
    * Instead, let's assume that commands returning without a length are based
    * on plain-text. This seems reasonable, because any binary result must
    * provide a length if one is to make sense of it.
    */
   if (mustSetResultValueLength) {
      resultValueLength = strlen(resultValue);
   }

   if (NULL != resultBuffer) {
      *resultBuffer = resultValue;
   }
   if (NULL != resultLen) {
      *resultLen = resultValueLength;
   }
   if (NULL != deleteResultBufferResult) {
      *deleteResultBufferResult = deleteResultValue;
   }

   return(err);
} // VixTools_ProcessVixCommand


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_Base64EncodeBuffer --
 *
 * Return value:
 *    VIX_OK on success
 *    VixError on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixTools_Base64EncodeBuffer(char **resultValuePtr,      // IN/OUT
                            size_t *resultValLengthPtr) // IN/OUT
{
   VixError err = VIX_OK;
   char *base64Buffer = NULL;
   size_t base64BufferLength = 0;
   Bool success = FALSE;

   ASSERT(resultValuePtr != NULL);
   ASSERT(*resultValuePtr != NULL);
   ASSERT(resultValLengthPtr != NULL);

   base64BufferLength = Base64_EncodedLength(*resultValuePtr, *resultValLengthPtr) + 1;
   base64Buffer = Util_SafeMalloc(base64BufferLength);
   success = Base64_Encode(*resultValuePtr,
                           *resultValLengthPtr,
                           base64Buffer,
                           base64BufferLength,
                           &base64BufferLength);
   if (!success) {
      (*resultValuePtr)[0] = 0;
      free(base64Buffer);
      base64Buffer = NULL;
      err = VIX_E_FAIL;
      goto abort;
   }

   base64Buffer[base64BufferLength] = 0;

   free(*resultValuePtr);
   *resultValuePtr = base64Buffer;
   *resultValLengthPtr = base64BufferLength;

abort:
   return err;

} // VixTools_Base64EncodeBuffer


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsEnableDHCPOnPrimary --
 *      
 *      Enable DHCP on primary NIC. A primary NIC is the
 *      first interface you get using ipconfig. You can change the order
 *      of NIC cards on a computer via Windows GUI.
 *
 * Results:
 *      S_OK on success.  COM error codes on failure.
 *
 * Side effects:
 *      None.
 * 
 *-----------------------------------------------------------------------------
 */

#if defined(_WIN32)
HRESULT
VixToolsEnableDHCPOnPrimary(void)
{
   HRESULT ret;
   GuestNic *primaryNic;

   primaryNic = NetUtil_GetPrimaryNic();
   if (NULL == primaryNic) {
      return HRESULT_FROM_WIN32(GetLastError());
   }

   ret = WMI_EnableDHCP(primaryNic->macAddress);
   VMX_XDR_FREE(xdr_GuestNic, primaryNic);
   free(primaryNic);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsEnableStaticOnPrimary --
 *      
 *      Set the IP address and/or subnet mask of the primary NIC. A primary NIC 
 *      is the first interface you get using ipconfig. You can change the order
 *      of NIC cards on a computer via Windows GUI.  
 *
 * Results:
 *      S_OK on success.  COM error codes on failure.
 *
 * Side effects:
 *      None.
 * 
 *-----------------------------------------------------------------------------
 */

HRESULT
VixToolsEnableStaticOnPrimary(const char *ipAddr,       // IN
                              const char *subnetMask)   // IN
{
   HRESULT ret;
   GuestNic *primaryNic;
   VmIpAddress *primaryIp;
   char actualIpAddress[IP_ADDR_SIZE];
   char actualSubnetMask[IP_ADDR_SIZE];

   if ((NULL == ipAddr) || 
       (NULL == subnetMask)) {
      return E_INVALIDARG;
   }

   actualIpAddress[0] = '\0';
   actualSubnetMask[0] = '\0';

   primaryNic = NetUtil_GetPrimaryNic();
   if (NULL == primaryNic) {
      return HRESULT_FROM_WIN32(GetLastError());
   }

   /*
    * Set IP address if client provides it.
    */
   
   primaryIp = &primaryNic->ips.ips_val[0];
 
   if ('\0' != ipAddr[0]) {
      Str_Strcpy(actualIpAddress,
                 ipAddr,
                 sizeof actualIpAddress);
   } else {
      Str_Strcpy(actualIpAddress,
                 primaryIp->ipAddress,
                 sizeof actualIpAddress);
   }

   /*
    * Set subnet mask if client provides it.
    */
   if ('\0' != subnetMask[0]) {
      Str_Strcpy(actualSubnetMask,
                 subnetMask, 
                 sizeof actualSubnetMask);
   } else {
      Str_Strcpy(actualSubnetMask,
                 primaryIp->subnetMask,
                 sizeof actualSubnetMask);
   }

   ret = WMI_EnableStatic(primaryNic->macAddress, 
                          actualIpAddress, 
                          actualSubnetMask);

   VMX_XDR_FREE(xdr_GuestNic, primaryNic);
   free(primaryNic);
   return ret;
}
#endif


