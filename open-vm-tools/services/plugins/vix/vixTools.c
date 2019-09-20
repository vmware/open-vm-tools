/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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

/*
 * When adding new functions, be sure to update
 * VixToolsCheckIfVixCommandEnabled() and VixToolsSetAPIEnabledProperties()
 * (adding a property and associated code in apps/lib/foundry/foundryVM.c
 * if necessary).  The enabled properties provide hints to an API developer
 * as to which APIs are available, and can be affected to guest OS attributes
 * or guest-side configuration.
 *
 * See Vim.Vm.Guest.QueryDisabledMethods()
 *
 */

/*
 * Logging messages:
 *
 * All guestOps should log some common information:
 *
 * g_debug() of the operation and arguments for that guestop.
 *    This data could be considered sensitive so it should not be visible
 *    at default logging levels.
 * g_message() of the operation and its VIX return code.
 *
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
#include "windowsu.h"
#include <sys/stat.h>
#include <time.h>
#define  SECURITY_WIN32
#include <Security.h>
#else
#include <unistd.h>
#endif

#if defined(sun) || defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#ifdef _MSC_VER
#   include <windows.h>
#elif _WIN32
#   include "win95.h"
#endif

#include "vmware.h"
#include "procMgr.h"
#include "timeutil.h"
#include "vm_version.h"
#include "message.h"
#include "dynarray.h"

#define G_LOG_DOMAIN  "vix"
#include <glib.h>

#include "util.h"
#include "strutil.h"
#include "str.h"
#include "file.h"
#include "err.h"
#include "hostinfo.h"
#include "guest_os.h"
#include "guest_msg_def.h"
#include "conf.h"
#include "vixCommands.h"
#include "base64.h"
#include "hostinfo.h"
#include "hgfsServerManager.h"
#include "hgfs.h"
#include "system.h"
#include "codeset.h"
#include "posix.h"
#include "unicode.h"
#include "hashTable.h"
#include "su.h"
#include "escape.h"

#if defined(__linux__) || defined(_WIN32)
#include "netutil.h"
#endif

#include "impersonate.h"
#include "vixOpenSource.h"
#include "vixToolsInt.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/log.h"

#ifdef _WIN32
#include "registryWin32.h"
#include "windowsu.h"
#endif /* _WIN32 */
#include "hgfsHelper.h"

#ifdef linux
#include "mntinfo.h"
#include <sys/vfs.h>
#endif

/*
 * No support for userworld.  Enable support for open vm tools when
 * USE_VGAUTH is defined.
 */
#if ((defined(__linux__) && !defined(USERWORLD)) || defined(_WIN32)) && \
     (!defined(OPEN_VM_TOOLS) || defined(USE_VGAUTH))
#define SUPPORT_VGAUTH 1
#else
#define SUPPORT_VGAUTH 0
#endif

#ifdef _WIN32
/*
 * vmwsu can't generate an impersonation token for local SYSTEM.
 */
#define  ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS 1
#endif

#if SUPPORT_VGAUTH
#include "VGAuthCommon.h"
#include "VGAuthError.h"
#include "VGAuthAuthentication.h"
#include "VGAuthAlias.h"

#define VMTOOLSD_APP_NAME "vmtoolsd"

#define VIXTOOLS_CONFIG_USE_VGAUTH_NAME "useVGAuth"
#define USE_VGAUTH_DEFAULT TRUE

static gboolean gSupportVGAuth = USE_VGAUTH_DEFAULT;
static gboolean QueryVGAuthConfig(GKeyFile *confDictRef);

#if ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS
static gchar *gCurrentUsername = NULL;

#define VIXTOOLS_CONFIG_ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS \
      "allowLocalSystemImpersonationBypass"
/*
 * This isn't on by default because it won't leave a complete
 * audit trail.
 */
#define ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS_DEFAULT FALSE

#endif

/*
 * XXX
 *
 * Holds the current impersonation token.
 *
 * This is a hack, dependent on there only being one impersonation
 * possible at a time anyways.  We need the HANDLE from inside the
 * VGAuthUserHandle to pass to other functions, so we can't throw
 * it out until Unimpersonate().
 *
 * A cleaner solution would be to not treat the userToken as a void *
 * (which is really a HANDLE on Windows) but instead make a small wrapper
 * struct containing a type and an optional HANDLE.  But this would
 * require massive changes all over, and make it very hard to turn off
 * VGAuth compilation.
 */

static VGAuthUserHandle *currentUserHandle = NULL;

#endif

/*
 * This should be an allocated string containing the impersonated username
 * while impersonation is active, and NULL when its not.
 */
char *gImpersonatedUsername = NULL;


#define SECONDS_BETWEEN_POLL_TEST_FINISHED     1

/*
 * This is used by the PRODUCT_VERSION_STRING macro.
 */
#ifndef PRODUCT_VERSION_NUMBER
#define PRODUCT_VERSION_NUMBER "1.0.0"
#endif


/*
 * The config file groupname for API configuration.
 */
#define  VIX_TOOLS_CONFIG_API_GROUPNAME               "guestoperations"

/*
 * Authentication configuration.
 * There are various forms of authentication supported,
 * e.g. InfrastructureAgents, NamePassword, SSPI, SAML etc.
 *
 * NOTE: "InfrastructureAgents" refers to hashed shared
 * secret form of authentication.
 */
#define  VIX_TOOLS_CONFIG_API_AUTHENTICATION          "Authentication"
#define  VIX_TOOLS_CONFIG_AUTHTYPE_AGENTS             "InfrastructureAgents"

#define VIX_TOOLS_CONFIG_INFRA_AGENT_DISABLED_DEFAULT  TRUE

/*
 * The switch that controls all APIs
 */
#define  VIX_TOOLS_CONFIG_API_ALL_NAME                "disabled"

/*
 * Individual API names for configuration.
 *
 * These match the WSDL names in the vSphere API.
 */
#define  VIX_TOOLS_CONFIG_API_START_PROGRAM_NAME      "StartProgramInGuest"
#define  VIX_TOOLS_CONFIG_API_LIST_PROCESSES_NAME     "ListProcessesInGuest"
#define  VIX_TOOLS_CONFIG_API_TERMINATE_PROCESS_NAME  "TerminateProcessInGuest"
#define  VIX_TOOLS_CONFIG_API_READ_ENV_VARS_NAME      "ReadEnvironmentVariableInGuest"

#define  VIX_TOOLS_CONFIG_API_MAKE_DIRECTORY_NAME     "MakeDirectoryInGuest"
#define  VIX_TOOLS_CONFIG_API_DELETE_FILE_NAME        "DeleteFileInGuest"
#define  VIX_TOOLS_CONFIG_API_DELETE_DIRECTORY_NAME   "DeleteDirectoryInGuest"
#define  VIX_TOOLS_CONFIG_API_MOVE_DIRECTORY_NAME     "MoveDirectoryInGuest"
#define  VIX_TOOLS_CONFIG_API_MOVE_FILE_NAME          "MoveFileInGuest"
#define  VIX_TOOLS_CONFIG_API_CREATE_TMP_FILE_NAME    "CreateTemporaryFileInGuest"
#define  VIX_TOOLS_CONFIG_API_CREATE_TMP_DIRECTORY_NAME          "CreateTemporaryDirectoryInGuest"
#define  VIX_TOOLS_CONFIG_API_LIST_FILES_NAME         "ListFilesInGuest"
#define  VIX_TOOLS_CONFIG_API_CHANGE_FILE_ATTRS_NAME  "ChangeFileAttributesInGuest"
#define  VIX_TOOLS_CONFIG_API_INITIATE_FILE_TRANSFER_FROM_GUEST_NAME  "InitiateFileTransferFromGuest"
#define  VIX_TOOLS_CONFIG_API_INITIATE_FILE_TRANSFER_TO_GUEST_NAME  "InitiateFileTransferToGuest"

#define  VIX_TOOLS_CONFIG_API_VALIDATE_CREDENTIALS_NAME   "ValidateCredentialsInGuest"
#define  VIX_TOOLS_CONFIG_API_ACQUIRE_CREDENTIALS_NAME   "AcquireCredentialsInGuest"
#define  VIX_TOOLS_CONFIG_API_RELEASE_CREDENTIALS_NAME   "ReleaseCredentialsInGuest"

#define VIX_TOOLS_CONFIG_API_ADD_GUEST_ALIAS_NAME      "AddGuestAlias"
#define VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_NAME   "RemoveGuestAlias"
#define VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_BY_CERT_NAME   "RemoveGuestAliasByCert"
#define VIX_TOOLS_CONFIG_API_LIST_GUEST_ALIASES_NAME    "ListGuestAliases"
#define VIX_TOOLS_CONFIG_API_LIST_GUEST_MAPPED_ALIASES_NAME  "ListGuestMappedAliases"

#define  VIX_TOOLS_CONFIG_API_CREATE_REGISTRY_KEY_NAME     "CreateRegistryKeyInGuest"
#define  VIX_TOOLS_CONFIG_API_LIST_REGISTRY_KEYS_NAME      "ListRegistryKeysInGuest"
#define  VIX_TOOLS_CONFIG_API_DELETE_REGISTRY_KEY_NAME     "DeleteRegistryKeyInGuest"
#define  VIX_TOOLS_CONFIG_API_SET_REGISTRY_VALUE_NAME      "SetRegistryValueInGuest"
#define  VIX_TOOLS_CONFIG_API_LIST_REGISTRY_VALUES_NAME    "ListRegistryValuesInGuest"
#define  VIX_TOOLS_CONFIG_API_DELETE_REGISTRY_VALUE_NAME   "DeleteRegistryValueInGuest"

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
} VixToolsRunProgramState;


/*
 * State of a single asynch startProgram.
 *
 * On Windows, keep the user's token and profile HANDLEs around
 * so the profile isn't unloaded until the program exits.
 */
typedef struct VixToolsStartProgramState {
   ProcMgr_AsyncProc    *procState;

#if defined(_WIN32) && SUPPORT_VGAUTH
   HANDLE hToken;
   HANDLE hProfile;
#endif

   void                 *eventQueue;
} VixToolsStartProgramState;


/*
 * Tracks processes started via StartProgram, so their exit information can
 * be returned with ListProcessesEx()
 *
 * We need live and dead because the exit status is fetched by from
 * a timer loop, and StartProgram of a very short lived program
 * followed immediately by a ListProcesses could miss the program
 * if we don't save it off for before the timer fires.
 *
 * This data is also useful to optimize ListProcessesEx.
 *
 * Note that we save off the procState so that we keep an open
 * handle to the process, to prevent its PID from being recycled.
 * We need to hold this open until we no longer save the result
 * of the exited program.  This is documented as 5 minutes
 * (VIX_TOOLS_EXITED_PROGRAM_REAP_TIME) in the VMODL.
 */
typedef struct VixToolsStartedProgramState {
   char *cmdName;
   char *fullCommandLine;
   char *user;
   uint64 pid;
   time_t startTime;
   int exitCode;
   time_t endTime;
   Bool isRunning;
   ProcMgr_AsyncProc *procState;
   struct VixToolsStartedProgramState *next;
} VixToolsStartedProgramState;

static VixToolsStartedProgramState *startedProcessList = NULL;

/*
 * How long we keep the info of exited processes.
 */
#define  VIX_TOOLS_EXITED_PROGRAM_REAP_TIME  (5 * 60)

/*
 * This is used to cache the results of ListProcessesEx when the reply
 * is too large to fit over the backdoor, so multiple trips are needed
 * to fetch it.
 */
static GHashTable *listProcessesResultsTable = NULL;

/*
 * How long to keep around cached results in case the Vix side dies.
 *
 * Err on the very large; would hate to have it kick in just because
 * the other side is slow or there's an immense ammount of data.
 */
#define  SECONDS_UNTIL_LISTPROC_CACHE_CLEANUP   (10 * 60)

typedef struct VixToolsCachedListProcessesResult {
   char *resultBuffer;
   size_t resultBufferLen;
   int key;
#ifdef _WIN32
   wchar_t *userName;
#else
   uid_t euid;
#endif
} VixToolsCachedListProcessesResult;

/*
 * Simple unique hashkey used for ListProcessesEx results.
 */
static uint32 listProcessesResultsKey = 1;

static void VixToolsFreeCachedResult(gpointer p);

/*
 * This structure is designed to implemente CreateTemporaryFile,
 * CreateTemporaryDirectory VI guest operations.
 */
typedef struct VixToolsGetTempFileCreateNameFuncData {
   char *filePrefix;
   char *tag;
   char *fileSuffix;
} VixToolsGetTempFileCreateNameFuncData;

/*
 * Global state.
 */
static Bool thisProcessRunsAsRoot = FALSE;
static Bool allowConsoleUserOps = FALSE;
static VixToolsReportProgramDoneProcType reportProgramDoneProc = NULL;
static void *reportProgramDoneData = NULL;

/*
 * Reference to global configuration dictionary.
 * This reference is initialized right before processing
 * any VIX command and is reset afterwards.
 */
static GKeyFile *gConfDictRef = NULL;

/*
 * Global state to decide if VIX commands should be restricted.
 *
 * Performing most of the VIX commands when quiesce snapshot operation
 * has frozen the guest filesystem could lead to deadlock in the tools
 * service (bug 1210773). This does not happen with VIM clients using
 * guestOps because hostd enforces the ordering of all VM operations.
 * However, it is possible for VIX clients to issue an op that ends up
 * accessing the guest filesystem in frozen state.
 */
static gboolean gRestrictCommands = FALSE;

#ifndef _WIN32
typedef struct VixToolsEnvironmentTableIterator {
   char **envp;
   size_t pos;
} VixToolsEnvironmentTableIterator;

/*
 * Stores the environment variables to use when executing guest applications.
 */
static HashTable *userEnvironmentTable = NULL;
#endif
static HgfsServerMgrData gVixHgfsBkdrConn;

#define SECONDS_BETWEEN_INVALIDATING_HGFS_SESSIONS    120
#define SECONDS_BETWEEN_INVALIDATING_PROC_HANDLES      60

static VixError VixToolsGetFileInfo(VixCommandRequestHeader *requestMsg,
                                    char **result);

static VixError VixToolsSetFileAttributes(VixCommandRequestHeader *requestMsg);

static gboolean VixToolsMonitorAsyncProc(void *clientData);
static gboolean VixToolsMonitorStartProgram(void *clientData);
static void VixToolsRegisterHgfsSessionInvalidator(void *clientData);
static gboolean VixToolsInvalidateInactiveHGFSSessions(void *clientData);

static GSource *gHgfsSessionInvalidatorTimer = NULL;
static guint gHgfsSessionInvalidatorTimerId;

static GSource *gProcHandleInvalidatorTimer = NULL;
static guint gProcHandleInvalidatorTimerId;
static void VixToolsRegisterProcHandleInvalidator(void *clientData);
static gboolean VixToolsInvalidateStaleProcHandles(void *clientData);

static void VixToolsPrintFileInfo(const char *filePathName,
                                  char *fileName,
                                  Bool escapeStrs,
                                  char **destPtr,
                                  char *endDestPtr);

static int VixToolsGetFileExtendedInfoLength(const char *filePathName,
                                             const char *fileName);

static char *VixToolsPrintFileExtendedInfoEx(const char *filePathName,
                                             const char *fileName);

static void VixToolsPrintFileExtendedInfo(const char *filePathName,
                                          const char *fileName,
                                          char **destPtr,
                                          char *endDestPtr);

static const char *fileInfoFormatString = "<FileInfo>"
                                          "<Name>%s</Name>"
                                          "<FileFlags>%d</FileFlags>"
                                          "<FileSize>%"FMT64"d</FileSize>"
                                          "<ModTime>%"FMT64"d</ModTime>"
                                          "</FileInfo>";

static const char *listFilesRemainingFormatString = "<rem>%d</rem>";

#ifdef _WIN32
static const char *fileExtendedInfoWindowsFormatString = "<fxi>"
                                          "<Name>%s</Name>"
                                          "<ft>%d</ft>"
                                          "<fs>%"FMT64"u</fs>"
                                          "<mt>%"FMT64"u</mt>"
                                          "<ct>%"FMT64"u</ct>"
                                          "<at>%"FMT64"u</at>"
                                          "</fxi>";
#else
static const char *fileExtendedInfoLinuxFormatString = "<fxi>"
                                          "<Name>%s</Name>"
                                          "<ft>%d</ft>"
                                          "<fs>%"FMT64"u</fs>"
                                          "<mt>%"FMT64"u</mt>"
                                          "<at>%"FMT64"u</at>"
                                          "<uid>%d</uid>"
                                          "<gid>%d</gid>"
                                          "<perm>%d</perm>"
                                          "<slt>%s</slt>"
                                          "</fxi>";
#endif

static VixError VixToolsGetTempFile(VixCommandRequestHeader *requestMsg,
                                    void *userToken,
                                    Bool useSystemTemp,
                                    char **tempFile,
                                    int *tempFileFd);

static void VixToolsFreeRunProgramState(VixToolsRunProgramState *asyncState);
static void VixToolsFreeStartProgramState(VixToolsStartProgramState *asyncState);

static void VixToolsUpdateStartedProgramList(VixToolsStartedProgramState *state);
static void VixToolsFreeStartedProgramState(VixToolsStartedProgramState *state);

static VixError VixToolsStartProgramImpl(const char *requestName,
                                         const char *programPath,
                                         const char *arguments,
                                         const char *workingDir,
                                         int numEnvVars,
                                         const char **envVars,
                                         Bool startMinimized,
                                         void *userToken,
                                         void *eventQueue,
                                         int64 *pid);

static char *VixToolsGetImpersonatedUsername(void *userToken);

static const char *scriptFileBaseName = "vixScript";

static VixError VixToolsMoveObject(VixCommandRequestHeader *requestMsg);

static VixError VixToolsCreateTempFile(VixCommandRequestHeader *requestMsg,
                                       char **result);

static VixError VixToolsCreateTempFileInt(VixCommandRequestHeader *requestMsg,
                                          Bool useSystemTemp,
                                          char **result);

static VixError VixToolsReadVariable(VixCommandRequestHeader *requestMsg,
                                     char **result);

static VixError VixToolsGetEnvForUser(void *userToken,
                                      const char *name,
                                      char **value);

static VixError VixToolsReadEnvVariables(VixCommandRequestHeader *requestMsg,
                                         char **result);

static VixError VixToolsGetMultipleEnvVarsForUser(void *userToken,
                                                  const char *names,
                                                  unsigned int numNames,
                                                  char **result);

static VixError VixToolsGetAllEnvVarsForUser(void *userToken, char **result);

static VixError VixToolsWriteVariable(VixCommandRequestHeader *requestMsg);

static VixError VixToolsListProcesses(VixCommandRequestHeader *requestMsg,
                                      size_t maxBufferSize,
                                      char **result);

static VixError VixToolsPrintProcInfoEx(DynBuf *dstBuffer,
                                        const char *cmd,
                                        const char *name,
                                        uint64 pid,
                                        const char *user,
                                        int start,
                                        int exitCode,
                                        int exitTime);

static VixError VixToolsListDirectory(VixCommandRequestHeader *requestMsg,
                                      size_t maxBufferSize,
                                      char **result);

static VixError VixToolsListFiles(VixCommandRequestHeader *requestMsg,
                                  size_t maxBufferSize,
                                  char **result);

static VixError VixToolsInitiateFileTransferFromGuest(VixCommandRequestHeader *requestMsg,
                                                      char **result);

static VixError VixToolsInitiateFileTransferToGuest(VixCommandRequestHeader *requestMsg);

static VixError VixToolsKillProcess(VixCommandRequestHeader *requestMsg);

static VixError VixToolsCreateDirectory(VixCommandRequestHeader *requestMsg);

static VixError VixToolsRunScript(VixCommandRequestHeader *requestMsg,
                                  char *requestName,
                                  void *eventQueue,
                                  char **result);

static VixError VixToolsCheckUserAccount(VixCommandRequestHeader *requestMsg);

static VixError VixToolsProcessHgfsPacket(VixCommandHgfsSendPacket *requestMsg,
                                          GMainLoop *eventQueue,
                                          char **result,
                                          size_t *resultValueResult);

static VixError VixToolsListFileSystems(VixCommandRequestHeader *requestMsg,
                                        char **result);

#if defined(_WIN32) || defined(__linux__)
static VixError VixToolsPrintFileSystemInfo(char **destPtr,
                                            const char *endDestPtr,
                                            const char *name,
                                            uint64 size,
                                            uint64 freeSpace,
                                            const char *type,
                                            Bool escapeStrs,
                                            Bool *truncated);
#endif

static VixError VixToolsValidateCredentials(VixCommandRequestHeader *requestMsg);

static VixError VixToolsAcquireCredentials(VixCommandRequestHeader *requestMsg,
                                           GMainLoop *eventQueue,
                                           char **result);

static VixError VixToolsReleaseCredentials(VixCommandRequestHeader *requestMsg);

static VixError VixToolsCreateRegKey(VixCommandRequestHeader *requestMsg);

static VixError VixToolsListRegKeys(VixCommandRequestHeader *requestMsg,
                                    size_t maxBufferSize,
                                    void *eventQueue,
                                    char **result);

static VixError VixToolsDeleteRegKey(VixCommandRequestHeader *requestMsg);

static VixError VixToolsSetRegValue(VixCommandRequestHeader *requestMsg);

static VixError VixToolsListRegValues(VixCommandRequestHeader *requestMsg,
                                      size_t maxBufferSize,
                                      void *eventQueue,
                                      char **result);

static VixError VixToolsDeleteRegValue(VixCommandRequestHeader *requestMsg);

#if defined(__linux__) || defined(_WIN32)
static VixError VixToolsGetGuestNetworkingConfig(VixCommandRequestHeader *requestMsg,
                                                 char **resultBuffer,
                                                 size_t *resultBufferLength);
#endif

#if defined(_WIN32)
static VixError VixToolsSetGuestNetworkingConfig(VixCommandRequestHeader *requestMsg);
#endif

static VixError VixTools_Base64EncodeBuffer(char **resultValuePtr, size_t *resultValLengthPtr);

static VixError VixToolsSetSharedFoldersProperties(VixPropertyListImpl *propList);

static VixError VixToolsSetAPIEnabledProperties(VixPropertyListImpl *propList,
                                                GKeyFile *confDictRef);

#if defined(_WIN32)
static HRESULT VixToolsEnableDHCPOnPrimary(void);

static HRESULT VixToolsEnableStaticOnPrimary(const char *ipAddr,
                                             const char *subnetMask);
#endif

static VixError VixToolsImpersonateUserImplEx(char const *credentialTypeStr,
                                              int credentialType,
                                              char const *obfuscatedNamePassword,
                                              Bool loadUserProfile,
                                              void **userToken);

static VixError VixToolsDoesUsernameMatchCurrentUser(const char *username);

static Bool VixToolsPidRefersToThisProcess(ProcMgr_Pid pid);

#ifndef _WIN32
static void VixToolsBuildUserEnvironmentTable(const char * const *envp);

static char **VixToolsEnvironmentTableToEnvp(const HashTable *envTable);

static int VixToolsEnvironmentTableEntryToEnvpEntry(const char *key, void *value,
                                                    void *clientData);

static void VixToolsFreeEnvp(char **envp);

#endif

static VixError FoundryToolsDaemon_TranslateSystemErr(void);

static VixError VixToolsRewriteError(uint32 opCode,
                                     VixError origError);

static size_t VixToolsXMLStringEscapedLen(const char *str, Bool escapeStr);

static Bool GuestAuthEnabled(void);

VixError GuestAuthPasswordAuthenticateImpersonate(
   char const *obfuscatedNamePassword,
   Bool loadUserProfile,
   void **userToken);

VixError GuestAuthSAMLAuthenticateAndImpersonate(
   char const *obfuscatedNamePassword,
   Bool loadUserProfile,
   void **userToken);

void GuestAuthUnimpersonate();

static Bool VixToolsCheckIfAuthenticationTypeEnabled(GKeyFile *confDictRef,
                                                     const char *typeName);

#if SUPPORT_VGAUTH

VGAuthError TheVGAuthContext(VGAuthContext **ctx);

#ifdef _WIN32
static void GuestAuthUnloadUserProfileAndToken(HANDLE hToken, HANDLE hProfile);
#endif

#endif


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
                    const char * const *originalEnvp,                               // IN
                    VixToolsReportProgramDoneProcType reportProgramDoneProcParam,   // IN
                    void *clientData)                                               // IN
{
   VixError err = VIX_OK;
#if SUPPORT_VGAUTH
   ToolsAppCtx *ctx = (ToolsAppCtx *) clientData;
#if ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS
   if (!gCurrentUsername) {
      gCurrentUsername = VixToolsGetCurrentUsername();
      ASSERT(gCurrentUsername);
      g_message("%s: Toolsd running as user '%s'\n",
                __FUNCTION__, gCurrentUsername);
   }
#endif
#endif

   /*
    * Run unit tests on DEVEL builds.
    */
   DEVEL_ONLY(TestVixToolsEnvVars());

   thisProcessRunsAsRoot = thisProcessRunsAsRootParam;
   reportProgramDoneProc = reportProgramDoneProcParam;
   reportProgramDoneData = clientData;

#ifndef _WIN32
   VixToolsBuildUserEnvironmentTable(originalEnvp);
#endif

   /* Register a straight through connection with the Hgfs server. */
   HgfsServerManager_DataInit(&gVixHgfsBkdrConn,
                              VIX_BACKDOORCOMMAND_COMMAND,
                              NULL,    // no RPC registration
                              NULL);   // rpc callback
   HgfsServerManager_Register(&gVixHgfsBkdrConn);

   listProcessesResultsTable = g_hash_table_new_full(g_int_hash, g_int_equal,
                                                     NULL,
                                                     VixToolsFreeCachedResult);

#if SUPPORT_VGAUTH
   /*
    * We don't set up the VGAuth log handler, since the default
    * does what we want, and trying to redirect VGAuth messages
    * to Log() causes recursion and a crash.
    */
#endif

#if SUPPORT_VGAUTH
   gSupportVGAuth = QueryVGAuthConfig(ctx->config);
#endif

#ifdef _WIN32
   err = VixToolsInitializeWin32();
   if (VIX_FAILED(err)) {
      return err;
   }
#endif

   return(err);
} // VixTools_Initialize


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_Uninitialize --
 *
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
VixTools_Uninitialize(void) // IN
{
   if (NULL != gHgfsSessionInvalidatorTimer) {
      g_source_remove(gHgfsSessionInvalidatorTimerId);
      g_source_unref(gHgfsSessionInvalidatorTimer);
      gHgfsSessionInvalidatorTimer = NULL;
      gHgfsSessionInvalidatorTimerId = 0;
      g_message("%s: HGFS session Invalidator detached\n", __FUNCTION__);
   }

   if (NULL != gProcHandleInvalidatorTimer) {
      g_source_remove(gProcHandleInvalidatorTimerId);
      g_source_unref(gProcHandleInvalidatorTimer);
      gProcHandleInvalidatorTimer = NULL;
      gProcHandleInvalidatorTimerId = 0;
      g_debug("%s: Process Handle Invalidator detached\n", __FUNCTION__);
   }

   HgfsServerManager_Unregister(&gVixHgfsBkdrConn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_RestrictCommands --
 *
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
VixTools_RestrictCommands(gboolean restricted) // IN
{
   gRestrictCommands = restricted;
}


#ifndef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsBuildUserEnvironmentTable --
 *
 *      Takes an array of strings of the form "<key>=<value>" storing the
 *      environment variables (as per environ(7)) that should be used when
 *      running programs, and populates the hash table with them.
 *
 *      If 'envp' is NULL, skip creating the user environment table, so that
 *      we just use the current environment.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      May initialize the global userEnvironmentTable.
 *
 *-----------------------------------------------------------------------------
 */

static void
VixToolsBuildUserEnvironmentTable(const char * const *envp)   // IN: optional
{
   if (NULL == envp) {
      ASSERT(NULL == userEnvironmentTable);
      return;
   }

   if (NULL == userEnvironmentTable) {
      userEnvironmentTable = HashTable_Alloc(64,  // buckets (power of 2)
                                             HASH_STRING_KEY | HASH_FLAG_COPYKEY,
                                             free); // freeFn for the values
   } else {
      /*
       * If we're being reinitialized, we can just clear the table and
       * load the new values into it. They shouldn't have changed, but
       * in case they ever do this will cover it.
       */
      HashTable_Clear(userEnvironmentTable);
   }

   for (; NULL != *envp; envp++) {
      char *name;
      char *value;
      char *whereToSplit;
      size_t nameLen;

      whereToSplit = strchr(*envp, '=');
      if (NULL == whereToSplit) {
         /* Our code generated this list, so this shouldn't happen. */
         ASSERT(0);
         continue;
      }

      nameLen = whereToSplit - *envp;
      name = Util_SafeMalloc(nameLen + 1);
      memcpy(name, *envp, nameLen);
      name[nameLen] = '\0';

      whereToSplit++;   // skip over '='

      value = Util_SafeStrdup(whereToSplit);

      HashTable_Insert(userEnvironmentTable, name, value);
      DEBUG_ONLY(value = NULL;)  // the hash table now owns 'value'

      free(name);
      DEBUG_ONLY(name = NULL;)
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsEnvironmentTableToEnvp --
 *
 *      Take a hash table storing environment variables names and values and
 *      build an array out of them.
 *
 * Results:
 *      char ** - envp array as per environ(7). Must be freed using
 *      VixToolsFreeEnvp
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char **
VixToolsEnvironmentTableToEnvp(const HashTable *envTable)   // IN
{
   char **envp;

   if (NULL != envTable) {
      VixToolsEnvironmentTableIterator itr;
      size_t numEntries = HashTable_GetNumElements(envTable);

      itr.envp = envp = Util_SafeMalloc((numEntries + 1) * sizeof *envp);
      itr.pos = 0;

      HashTable_ForEach(envTable, VixToolsEnvironmentTableEntryToEnvpEntry, &itr);

      ASSERT(numEntries == itr.pos);

      envp[numEntries] = NULL;
   } else {
      envp = NULL;
   }

   return envp;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsEnvironmentTableEntryToEnvpEntry --
 *
 *      Callback for HashTable_ForEach(). Gets called for each entry in an
 *      environment table, converting the key (environment variable name) and
 *      value (environment variable value) into a string of the form
 *      "<key>=<value>" and adding that to the envp array passed in with the
 *      VixToolsEnvironmentTableIterator client data.
 *
 * Results:
 *      int - always 0
 *
 * Side effects:
 *      Sets one entry in the envp.
 *
 *-----------------------------------------------------------------------------
 */

static int
VixToolsEnvironmentTableEntryToEnvpEntry(const char *key,     // IN
                                         void *value,         // IN
                                         void *clientData)    // IN/OUT
{
   VixToolsEnvironmentTableIterator *itr = clientData;

   itr->envp[itr->pos++] = Str_SafeAsprintf(NULL, "%s=%s", key, (char *)value);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsFreeEnvp --
 *
 *      Free's an array of strings where both the strings and the array
 *      were heap allocated.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
VixToolsFreeEnvp(char **envp)   // IN
{
   if (NULL != envp) {
      char **itr;

      for (itr = envp; NULL != *itr; itr++) {
         free(*itr);
      }

      free(envp);
   }
}
#endif  // #ifndef _WIN32


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
 * Different sockets may use different message protocols, such as the
 * backdoor-on-a-socket or the Foundry network message.
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
   const char *commandLine = NULL;
   const char *commandLineArgs = NULL;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   int64 pid = -1;
   static char resultBuffer[32];
   VMAutomationRequestParser parser;

   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *runProgramRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   runProgramRequest = (VixMsgRunProgramRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                          runProgramRequest->programNameLength,
                                            &commandLine);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *commandLine) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
   if (runProgramRequest->commandLineArgsLength > 0) {
      err = VMAutomationRequestParserGetString(&parser,
                                     runProgramRequest->commandLineArgsLength,
                                               &commandLineArgs);
      if (VIX_OK != err) {
         goto abort;
      }
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
      err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
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

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixTools_RunProgram


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_StartProgram --
 *
 *    Start a program on the guest.  Much like RunProgram, but
 *    with additional arguments.  Another key difference is that
 *    the program's exitCode and endTime will be available to ListProcessesEx
 *    for a short time.
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
VixTools_StartProgram(VixCommandRequestHeader *requestMsg, // IN
                      char *requestName,                   // IN
                      void *eventQueue,                    // IN
                      char **result)                       // OUT
{
   VixError err = VIX_OK;
   VixMsgStartProgramRequest *startProgramRequest;
   const char *programPath = NULL;
   const char *arguments = NULL;
   const char *workingDir = NULL;
   const char **envVars = NULL;
   const char *bp = NULL;
   const char *cmdNameBegin = NULL;
   Bool impersonatingVMWareUser = FALSE;
   int64 pid = -1;
   int i;
   void *userToken = NULL;
   static char resultBuffer[32];    // more than enough to hold a 64 bit pid
   VixToolsStartedProgramState *spState;
   VMAutomationRequestParser parser;

   err = VMAutomationRequestParserInit(&parser,
                                      requestMsg, sizeof *startProgramRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   startProgramRequest = (VixMsgStartProgramRequest *) requestMsg;

   /*
    * It seems that this functions uses the a string format that includes
    * the '\0' terminator in the length fields.
    * This is different from other "old" vix guest command format.
    */
   err = VMAutomationRequestParserGetOptionalString(&parser,
                                      startProgramRequest->programPathLength,
                                            &programPath);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL == programPath || 0 == *programPath) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VMAutomationRequestParserGetOptionalString(&parser,
                                          startProgramRequest->argumentsLength,
                                                    &arguments);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VMAutomationRequestParserGetOptionalString(&parser,
                                         startProgramRequest->workingDirLength,
                                                    &workingDir);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL != workingDir && '\0' == workingDir[0]) {
      /* Let's treat an empty string the same as NULL: use the default. */
      workingDir = NULL;
   }

   err = VMAutomationRequestParserGetOptionalStrings(&parser,
                                             startProgramRequest->numEnvVars,
                                             startProgramRequest->envVarLength,
                                                    &bp);
   if (VIX_OK != err) {
      goto abort;
   }

   if (startProgramRequest->numEnvVars > 0) {
      envVars = Util_SafeMalloc(sizeof(char*) * (startProgramRequest->numEnvVars + 1));
      for (i = 0; i < startProgramRequest->numEnvVars; i++) {
         envVars[i] = bp;
         bp += strlen(envVars[i]) + 1;
      }
      envVars[i] = NULL;

      err = VixToolsValidateEnviron(envVars);
      if (VIX_OK != err) {
         goto abort;
      }
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s args: progamPath: '%s', arguments: '%s', workingDir: '%s'\n",
           __FUNCTION__, IMPERSONATED_USERNAME, programPath,
          (NULL != arguments) ? arguments : "",
          (NULL != workingDir) ? workingDir : "");

   err = VixToolsStartProgramImpl(requestName,
                                  programPath,
                                  arguments,
                                  workingDir,
                                  startProgramRequest->numEnvVars,
                                  envVars,
                                  startProgramRequest->startMinimized,
                                  userToken,
                                  eventQueue,
                                  &pid);

   if (VIX_OK == err) {

      /*
       * Save off the program so ListProcessesEx can find it.
       *
       * We store it here to avoid the hole between starting it and the
       * exited process polling proc.
       */
      spState = Util_SafeMalloc(sizeof(VixToolsStartedProgramState));

      /*
       * Build up the command line so the args are passed to the command.
       * To be safe, always put quotes around the program name. If the name
       * contains spaces (either in the file name of its directory path),
       * then the quotes are required. If the name doesn't contain spaces, then
       * unnecessary quotes don't seem to create a problem for both Windows and
       * Linux.
       */
      if (NULL != arguments) {
         spState->fullCommandLine = Str_SafeAsprintf(NULL,
                                                     "\"%s\" %s",
                                                     programPath,
                                                     arguments);
      } else {
         spState->fullCommandLine = Str_SafeAsprintf(NULL,
                                                     "\"%s\"",
                                                     programPath);
      }
#if defined(_WIN32)
      /*
       * For windows, we let the VIX client parse the
       * command line to get the real command name.
       */
      spState->cmdName = NULL;
#else
      /*
       * Find the last path separator, to get the cmd name.
       * If no separator is found, then use the whole name.
       */
      cmdNameBegin = strrchr(programPath, '/');
      if (NULL == cmdNameBegin) {
         cmdNameBegin = programPath;
      } else {
         /*
          * Skip over the last separator.
          */
         cmdNameBegin++;
      }
      spState->cmdName = Str_SafeAsprintf(NULL, "%s", cmdNameBegin);
#endif
      spState->user = VixToolsGetImpersonatedUsername(&userToken);
      spState->pid = (uint64) pid;
      spState->startTime = time(NULL);
      spState->exitCode = 0;
      spState->endTime = 0;
      spState->isRunning = TRUE;
      spState->next = NULL;
      spState->procState = NULL;

      // add it to the list of started programs
      VixToolsUpdateStartedProgramList(spState);
   }

   if (NULL != eventQueue) {
      /*
       * Register a timer to periodically invalidate any stale
       * process handles.
       */
      VixToolsRegisterProcHandleInvalidator(eventQueue);
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   Str_Sprintf(resultBuffer, sizeof(resultBuffer), "%"FMT64"d", pid);
   *result = resultBuffer;

   free((char **) envVars);

   guest_debug("%s: returning '%s'\n", __FUNCTION__, resultBuffer);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixTools_StartProgram


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
                       const char *commandLine,      // IN
                       const char *commandLineArgs,  // IN
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
   ProcMgr_ProcArgs procArgs;
#if defined(_WIN32)
   Bool forcedRoot = FALSE;
   STARTUPINFO si;
   wchar_t *envBlock = NULL;
#endif
   GSource *timer;

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
    * unnecessary quotes don't seem to create a problem for both Windows and
    * Linux.
    */
   if (NULL != commandLineArgs) {
      fullCommandLine = Str_SafeAsprintf(NULL,
                                     "\"%s\" %s",
                                     commandLine,
                                     commandLineArgs);
   } else {
      fullCommandLine = Str_SafeAsprintf(NULL,
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

   memset(&procArgs, 0, sizeof procArgs);
#if defined(_WIN32)
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      /*
       * If we are impersonating a user then use the user's environment
       * block. That way the user-specific environment variables will
       * be available to the application (such as the user's TEMP
       * directory instead of the system-wide one).
       */
      err = VixToolsGetEnvBlock(userToken, &envBlock);
      if (VIX_OK != err) {
         goto abort;
      }

      forcedRoot = Impersonate_ForceRoot();
   }

   memset(&si, 0, sizeof si);
   procArgs.hToken = (PROCESS_CREATOR_USER_TOKEN == userToken) ? NULL : userToken;
   procArgs.bInheritHandles = TRUE;
   procArgs.lpStartupInfo = &si;
   si.cb = sizeof si;
   procArgs.dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
   procArgs.lpEnvironment = envBlock;
   si.dwFlags = STARTF_USESHOWWINDOW;
   si.wShowWindow = (VIX_RUNPROGRAM_ACTIVATE_WINDOW & runProgramOptions)
                     ? SW_SHOWNORMAL : SW_MINIMIZE;
#elif !defined(__FreeBSD__)
   procArgs.envp = VixToolsEnvironmentTableToEnvp(userEnvironmentTable);
#endif

   asyncState->procState = ProcMgr_ExecAsync(fullCommandLine, &procArgs);

#if defined(_WIN32)
   if (forcedRoot) {
      Impersonate_UnforceRoot();
   }
#else
   VixToolsFreeEnvp(procArgs.envp);
   DEBUG_ONLY(procArgs.envp = NULL;)
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
   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorAsyncProc, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(eventQueue));
   g_source_unref(timer);

   /*
    * VixToolsMonitorAsyncProc will clean asyncState up when the program finishes.
    */
   asyncState = NULL;

abort:
   free(fullCommandLine);
#ifdef _WIN32
   if (NULL != envBlock) {
      VixToolsDestroyEnvironmentBlock(envBlock);
   }
#endif

   if (VIX_FAILED(err)) {
      VixToolsFreeRunProgramState(asyncState);
   }

   g_message("%s returning %"FMT64"d\n", __FUNCTION__, err);

   return err;
} // VixToolsRunProgramImpl


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsStartProgramImpl --
 *
 *    Start a named program on the guest.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    Saves off its state.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsStartProgramImpl(const char *requestName,            // IN
                         const char *programPath,            // IN
                         const char *arguments,              // IN
                         const char *workingDir,             // IN
                         int numEnvVars,                     // IN
                         const char **envVars,               // IN
                         Bool startMinimized,                // IN
                         void *userToken,                    // IN
                         void *eventQueue,                   // IN
                         int64 *pid)                         // OUT
{
   VixError err = VIX_OK;
   char *fullCommandLine = NULL;
   VixToolsStartProgramState *asyncState = NULL;
   char *tempCommandLine = NULL;
   char *startProgramFileName;
   char *stopProgramFileName;
   Bool programExists;
   Bool programIsExecutable;
   ProcMgr_ProcArgs procArgs;
   char *workingDirectory = NULL;
#if defined(_WIN32)
   Bool forcedRoot = FALSE;
   STARTUPINFO si;
   wchar_t *envBlock = NULL;
   Bool envBlockFromMalloc = TRUE;
#endif
   GSource *timer;
#if defined(_WIN32) && SUPPORT_VGAUTH
   HANDLE hToken = INVALID_HANDLE_VALUE;
   HANDLE hProfile = INVALID_HANDLE_VALUE;
   VGAuthError vgErr;
   VGAuthContext *ctx;
#endif

   /*
    * Initialize this here so we can call free on its member variables in abort
    */
   memset(&procArgs, 0, sizeof procArgs);

   if (NULL != pid) {
      *pid = (int64) -1;
   }

   tempCommandLine = Util_SafeStrdup(programPath);
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
   if (!programExists) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   programIsExecutable =
      (FileIO_Access(startProgramFileName, FILEIO_ACCESS_EXEC) ==
                                                       FILEIO_SUCCESS);
   if (!programIsExecutable) {
      err = VIX_E_GUEST_USER_PERMISSIONS;
      goto abort;
   }

   /* sanity check workingDir if set */
   if (NULL != workingDir && !File_IsDirectory(workingDir)) {
      err = VIX_E_NOT_A_DIRECTORY;
      goto abort;
   }

   /*
    * Adjust the workingDir if needed.
    * For non-Windows, we use the user's $HOME if workingDir isn't supplied.
    */
   if (NULL == workingDir) {
#if defined(__linux__) || defined(sun) || defined(__FreeBSD__) || defined(__APPLE__)
      char *username = NULL;

      if (!ProcMgr_GetImpersonatedUserInfo(&username, &workingDirectory)) {
         g_warning("%s: ProcMgr_GetImpersonatedUserInfo() failed fetching workingDirectory\n", __FUNCTION__);
         err = VIX_E_FAIL;
         goto abort;
      }

      free(username);
#elif defined(_WIN32)
      workingDirectory = (char *)workingDir;
#else
      /*
       * we shouldn't ever get here for unsupported guests, so just
       * be sure it builds.
       */
      workingDirectory = NULL;
#endif
   } else {
      workingDirectory = Util_SafeStrdup(workingDir);
   }


   /*
    * Build up the command line so the args are passed to the command.
    * To be safe, always put quotes around the program name. If the name
    * contains spaces (either in the file name of its directory path),
    * then the quotes are required. If the name doesn't contain spaces, then
    * unnecessary quotes don't seem to create a problem for both Windows and
    * Linux.
    */
   if (NULL != arguments) {
      fullCommandLine = Str_SafeAsprintf(NULL,
                                     "\"%s\" %s",
                                     programPath,
                                     arguments);
   } else {
      fullCommandLine = Str_SafeAsprintf(NULL,
                                     "\"%s\"",
                                     programPath);
   }

   if (NULL == fullCommandLine) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   /*
    * Save some state for when it completes.
    */
   asyncState = Util_SafeCalloc(1, sizeof *asyncState);

#if defined(_WIN32)
   if (NULL != envVars) {
      err = VixToolsEnvironToEnvBlock(envVars, &envBlock);
      if (VIX_OK != err) {
         goto abort;
      }
   } else if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      /*
       * If we are impersonating a user and that user did not supply
       * environment variables to pass, then use the user's environment
       * block. That way the user-specific environment variables will
       * be available to the application (such as the user's TEMP
       * directory instead of the system-wide one).
       */
      err = VixToolsGetEnvBlock(userToken, &envBlock);
      if (VIX_OK != err) {
         goto abort;
      }
      envBlockFromMalloc = FALSE;
   }

   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      forcedRoot = Impersonate_ForceRoot();
   }

   memset(&si, 0, sizeof si);
   procArgs.hToken = (PROCESS_CREATOR_USER_TOKEN == userToken) ? NULL : userToken;
   procArgs.bInheritHandles = TRUE;
   procArgs.lpStartupInfo = &si;
   procArgs.lpCurrentDirectory = UNICODE_GET_UTF16(workingDirectory);
   /*
    * The lpEnvironment is in UTF-16, so we need the CREATE_UNICODE_ENVIRONMENT
    * flag.
    */
   procArgs.dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
   procArgs.lpEnvironment = envBlock;
   si.cb = sizeof si;
   si.dwFlags = STARTF_USESHOWWINDOW;
   si.wShowWindow = (startMinimized) ? SW_MINIMIZE : SW_SHOWNORMAL;
#else
   procArgs.workingDirectory = workingDirectory;
   procArgs.envp = (char **)envVars;
#endif

#if defined(_WIN32) && SUPPORT_VGAUTH
   /*
    * Special case profile handling for StartProgram.  It should stay loaded
    * until the program exits, so copy the profile and user handles for
    * later cleanup, and clobber the profile handle so that it's not unloaded
    * when the impersonation ends.
    *
    * Only do this when we've actually impersonated; its not
    * needed when impersonation isn't done (eg vmusr or SYSTEM bypass).
    */
   if (GuestAuthEnabled() && PROCESS_CREATOR_USER_TOKEN != userToken) {
      vgErr = TheVGAuthContext(&ctx);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         g_warning("%s: Couldn't get the vgauth context\n", __FUNCTION__);
         goto abort;
      }

      vgErr = VGAuth_UserHandleAccessToken(ctx, currentUserHandle, &hToken);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         g_warning("%s: Failed to get user token\n", __FUNCTION__);
         goto abort;
      }
      vgErr = VGAuth_UserHandleGetUserProfile(ctx, currentUserHandle,
                                              &hProfile);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         g_warning("%s: Failed to get user profile\n", __FUNCTION__);
         CloseHandle(hToken);
         goto abort;
      }
   }
   asyncState->hToken = hToken;
   asyncState->hProfile = hProfile;
#endif

   asyncState->procState = ProcMgr_ExecAsync(fullCommandLine, &procArgs);

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

   guest_debug("%s: started '%s', pid %"FMT64"d\n",
               __FUNCTION__, fullCommandLine, *pid);

#if defined(_WIN32) && SUPPORT_VGAUTH
   /*
    * Clobber the profile handle before un-impersonation.
    */
   if (GuestAuthEnabled() && PROCESS_CREATOR_USER_TOKEN != userToken) {
      vgErr = VGAuth_UserHandleSetUserProfile(ctx, currentUserHandle,
                                              INVALID_HANDLE_VALUE);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         g_warning("%s: Failed to clobber user profile\n", __FUNCTION__);
         // VGAuth_EndImpersonation will take care of profile, close hToken
         CloseHandle(asyncState->hToken);
         asyncState->hToken = INVALID_HANDLE_VALUE;
         asyncState->hProfile = INVALID_HANDLE_VALUE;
         goto abort;
      }
   }
#endif

   /*
    * Start a periodic procedure to check the app periodically
    */
   asyncState->eventQueue = eventQueue;
   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorStartProgram, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(eventQueue));
   g_source_unref(timer);

   /*
    * VixToolsMonitorStartProgram will clean asyncState up when the program
    * finishes.
    */
   asyncState = NULL;


abort:
   free(tempCommandLine);
   free(fullCommandLine);
   free(workingDirectory);
#ifdef _WIN32
   if (envBlockFromMalloc) {
      free(envBlock);
   } else {
      VixToolsDestroyEnvironmentBlock(envBlock);
   }
   UNICODE_RELEASE_UTF16(procArgs.lpCurrentDirectory);
#endif

   if (VIX_FAILED(err)) {
      VixToolsFreeStartProgramState(asyncState);
   }

   return err;
} // VixToolsStartProgramImpl


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

static gboolean
VixToolsMonitorAsyncProc(void *clientData) // IN
{
   VixError err = VIX_OK;
   VixToolsRunProgramState *asyncState;
   Bool procIsRunning = FALSE;
   int exitCode = 0;
   ProcMgr_Pid pid = -1;
   int result = -1;
   GSource *timer;
   char *requestName = NULL;
   VixRunProgramOptions runProgramOptions;

   asyncState = (VixToolsRunProgramState *)clientData;
   ASSERT(asyncState);

   /*
    * Check if the program has completed and VIX commands
    * are not being restricted. Performing cleanup involving
    * IO would deadlock the operations like quiesce snapshot
    * that freeze the filesystem.
    */
   procIsRunning = ProcMgr_IsAsyncProcRunning(asyncState->procState);
   if (!procIsRunning) {
      if (gRestrictCommands) {
         g_debug("%s: Deferring RunScript cleanup due to IO freeze\n",
                 __FUNCTION__);
      } else {
         goto cleanup;
      }
   }

   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorAsyncProc, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(asyncState->eventQueue));
   g_source_unref(timer);
   return FALSE;

cleanup:

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

   runProgramOptions = asyncState->runProgramOptions;
   requestName = Util_SafeStrdup(asyncState->requestName);

   VixToolsFreeRunProgramState(asyncState);

   /*
    * We may just be running to clean up after running a script, with the
    * results already reported.
    */
   if ((NULL != reportProgramDoneProc)
       && !(runProgramOptions & VIX_RUNPROGRAM_RETURN_IMMEDIATELY)) {
      (*reportProgramDoneProc)(requestName,
                               err,
                               exitCode,
                               (int64) pid,
                               reportProgramDoneData);
   }

   free(requestName);
   return FALSE;
} // VixToolsMonitorAsyncProc


/*
 *----------------------------------------------------------------------------
 *
 * VixToolsInvalidateStaleProcHandles --
 *
 *    Remove stale proc handles from started programs list.
 *
 * Return value:
 *    TRUE if the timer needs to be re-registerd.
 *    FALSE if the timer needs to be deleted.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static gboolean
VixToolsInvalidateStaleProcHandles(void *clientData)   // IN:
{
   VixToolsUpdateStartedProgramList(NULL);

   if (startedProcessList != NULL) {
      /*
       * There are still proc handles on the list, so keep the periodic timer
       * registered.
       */
      return TRUE;
   } else {
      g_source_unref(gProcHandleInvalidatorTimer);
      gProcHandleInvalidatorTimer = NULL;
      gProcHandleInvalidatorTimerId = 0;
      g_debug("%s: Process Handle Invalidator is successfully detached\n",
              __FUNCTION__);
      return FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VixToolsInvalidateInactiveHGFSSessions --
 *
 *    Send a request to HGFS server to invalidate inactive sessions.
 *    Registers a timer to call the invalidator.
 *
 * Return value:
 *    TRUE if the timer needs to be re-registerd.
 *    FALSE if the timer needs to be deleted.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static gboolean
VixToolsInvalidateInactiveHGFSSessions(void *clientData)   // IN:
{
   if (HgfsServerManager_InvalidateInactiveSessions(&gVixHgfsBkdrConn) > 0) {
      /*
       * There are still active sessions, so keep the periodic timer
       * registered.
       */
      return TRUE;
   } else {

      g_debug("%s: HGFS session Invalidator is successfully detached\n",
              __FUNCTION__);

      g_source_unref(gHgfsSessionInvalidatorTimer);
      gHgfsSessionInvalidatorTimer = NULL;
      gHgfsSessionInvalidatorTimerId = 0;
      return FALSE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VixToolsRegisterProcHandleInvalidator --
 *
 *    Check bug 2308222 for more details. This function is designed to
 *    cleanup any garbage proc handles in the Started Process List.
 *
 *    If there is a timer already registered, then this function doesn't
 *    do anything.
 *
 * Return value:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
VixToolsRegisterProcHandleInvalidator(void *clientData)    // IN:
{
   ASSERT(clientData);

   if (NULL != gProcHandleInvalidatorTimer) {
      return;
   }

   gProcHandleInvalidatorTimer =
         g_timeout_source_new(SECONDS_BETWEEN_INVALIDATING_PROC_HANDLES * 1000);

   g_source_set_callback(gProcHandleInvalidatorTimer,
                         VixToolsInvalidateStaleProcHandles,
                         NULL,
                         NULL);

   gProcHandleInvalidatorTimerId =
         g_source_attach(gProcHandleInvalidatorTimer,
                         g_main_loop_get_context((GMainLoop *) clientData));

   g_debug("%s: Process Handle Invalidator registered\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------------
 *
 * VixToolsRegisterHgfsSessionInvalidator --
 *
 *    Check bug 783263 for more details. This function is designed to
 *    cleanup any hgfs state left by remote clients that got
 *    disconnected abruptly during a file copy process.
 *
 *    If there is a timer already registered, then this function doesn't
 *    do anything.
 *
 * Return value:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
VixToolsRegisterHgfsSessionInvalidator(void *clientData)    // IN:
{
   ASSERT(clientData);

   if (NULL != gHgfsSessionInvalidatorTimer) {
      return;
   }

   gHgfsSessionInvalidatorTimer =
         g_timeout_source_new(SECONDS_BETWEEN_INVALIDATING_HGFS_SESSIONS * 1000);

   g_source_set_callback(gHgfsSessionInvalidatorTimer,
                         VixToolsInvalidateInactiveHGFSSessions,
                         NULL,
                         NULL);

   gHgfsSessionInvalidatorTimerId =
         g_source_attach(gHgfsSessionInvalidatorTimer,
                         g_main_loop_get_context((GMainLoop *) clientData));

   g_debug("%s: HGFS session Invalidator registered\n", __FUNCTION__);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsMonitorStartProgram --
 *
 *    This polls a program started by StartProgram to see if it has completed.
 *    If it has, saves off its exitCode and endTime so they can be queried
 *    via ListProcessesEx.
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

static gboolean
VixToolsMonitorStartProgram(void *clientData) // IN
{
   VixToolsStartProgramState *asyncState;
   Bool procIsRunning = FALSE;
   int exitCode = 0;
   ProcMgr_Pid pid = -1;
   int result = -1;
   VixToolsStartedProgramState *spState;
   GSource *timer;

   asyncState = (VixToolsStartProgramState *) clientData;
   ASSERT(asyncState);

   /*
    * Check if the program has completed.
    */
   procIsRunning = ProcMgr_IsAsyncProcRunning(asyncState->procState);
   if (!procIsRunning) {
      goto done;
   }

   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorStartProgram, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(asyncState->eventQueue));
   g_source_unref(timer);
   return FALSE;

done:

   result = ProcMgr_GetExitCode(asyncState->procState, &exitCode);
   pid = ProcMgr_GetPid(asyncState->procState);
   if (0 != result) {
      exitCode = -1;
   }

   /*
    * Save off the program exit state so ListProcessesEx can find it.
    *
    * We only bother to set pid, exitCode and endTime -- we have the
    * other data from when we made the initial record whne the
    * progrtam started; that record will be updated with the exitCode
    * and endTime.
    */
   spState = Util_SafeMalloc(sizeof(VixToolsStartedProgramState));
   spState->cmdName = NULL;
   spState->fullCommandLine = NULL;
   spState->user = NULL;
   spState->pid = pid;
   spState->startTime = 0;
   spState->exitCode = exitCode;
   spState->endTime = time(NULL);
   spState->isRunning = FALSE;
   spState->next = NULL;
   spState->procState = asyncState->procState;

   // add it to the list of exited programs
   VixToolsUpdateStartedProgramList(spState);

   VixToolsFreeStartProgramState(asyncState);

   return FALSE;
} // VixToolsMonitorStartProgram


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsUpdateStartedProgramList --
 *
 *    Adds a new started program's state to the saved list, and
 *    removes any that have been there too long.
 *
 * Return value:
 *    None
 *
 * Side effects:
 *    Apps that have been saved past their expiration date are dropped.
 *
 *-----------------------------------------------------------------------------
 */

static void
VixToolsUpdateStartedProgramList(VixToolsStartedProgramState *state)        // IN
{
   VixToolsStartedProgramState *spList = NULL;
   VixToolsStartedProgramState *last = NULL;
   VixToolsStartedProgramState *old = NULL;
   time_t now;

   now = time(NULL);

   /*
    * Update the 'running' record if the process has completed.
    */
   if (state && (state->isRunning == FALSE)) {
      spList = startedProcessList;
      while (spList) {
         if (spList->pid == state->pid) {
            /*
             * Update the two exit fields now that we have them
             */
            spList->exitCode = state->exitCode;
            spList->endTime = state->endTime;
            spList->isRunning = FALSE;

            guest_debug("%s: started program '%s' has completed, "
                        "exitCode %d\n", __FUNCTION__,
                        spList->fullCommandLine, spList->exitCode);

            /*
             * Don't let the procState be free'd on Windows to
             * keep OS from reusing the pid. We need to free
             * procState in case of Posix to avoid unnecessary
             * caching of FDs, which might make the service run
             * out of FDs as FDs are limited (usually 1024 by
             * default) for a process.
             */
#ifdef WIN32
            spList->procState = state->procState;
            state->procState = NULL;
#else
            spList->procState = NULL;
#endif

            VixToolsFreeStartedProgramState(state);
            // NULL it out so we don't try to add it later in this function
            state  = NULL;
            break;
         } else {
            spList = spList->next;
         }
      }
   }


   /*
    * Find and toss any old records.
    */
   last = NULL;
   spList = startedProcessList;
   while (spList) {
      /*
       * Sanity check we don't have a duplicate entry -- this should
       * only happen when the OS re-uses the PID before we reap the record
       * of its exit status.
       */
      if (state) {
         if (state->pid == spList->pid) {
            // XXX just whine for M/N, needs better fix in *main
            g_warning("%s: found duplicate entry in startedProcessList\n",
                      __FUNCTION__);
         }
      }
      if (!spList->isRunning &&
          (spList->endTime < (now - VIX_TOOLS_EXITED_PROGRAM_REAP_TIME))) {
         if (last) {
            last->next = spList->next;
         } else {
            startedProcessList = spList->next;
         }
         old = spList;
         spList = spList->next;
         VixToolsFreeStartedProgramState(old);
      } else {
         last = spList;
         spList = spList->next;
      }
   }


   /*
    * Add any new record to the list
    */
   if (state) {
      if (last) {
         last->next = state;
      } else {
         startedProcessList = state;
      }
   }

} // VixToolsUpdateStartedProgramList


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsFreeStartedProgramState --
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
VixToolsFreeStartedProgramState(VixToolsStartedProgramState *spState) // IN
{
   if (NULL == spState) {
      return;
   }

   free(spState->cmdName);
   free(spState->fullCommandLine);
   free(spState->user);

   if (NULL != spState->procState) {
      ProcMgr_Free(spState->procState);
   }

   free(spState);
} // VixToolsFreeStartedProgramState


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsFindStartedProgramState --
 *
 *    Searches the list of running/exited apps to see if the given
 *    pid was started via StartProgram.
 *
 * Results:
 *    Any state matching the given pid.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

VixToolsStartedProgramState *
VixToolsFindStartedProgramState(uint64 pid)
{
   VixToolsStartedProgramState *spList;

   spList = startedProcessList;
   while (spList) {
      if (spList->pid == pid) {
         return spList;
      }
      spList = spList->next;
   }

   return NULL;
}


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


#if SUPPORT_VGAUTH
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

VixError
VixToolsTranslateVGAuthError(VGAuthError vgErr)
{
   VixError err;

   switch (VGAUTH_ERROR_CODE(vgErr)) {
   case VGAUTH_E_OK:
      err = VIX_OK;
      break;
   case VGAUTH_E_INVALID_ARGUMENT:
      err = VIX_E_INVALID_ARG;
      break;
   case VGAUTH_E_INVALID_CERTIFICATE:
      err = VIX_E_INVALID_ARG; // XXX -- needs a Vix equiv
      break;
   case VGAUTH_E_PERMISSION_DENIED:
      err = VIX_E_GUEST_USER_PERMISSIONS;
      break;
   case VGAUTH_E_OUT_OF_MEMORY:
      err = VIX_E_OUT_OF_MEMORY;
      break;
   case VGAUTH_E_COMM:
      err = VIX_E_FAIL;
      break;
   case VGAUTH_E_NOTIMPLEMENTED:
      err = VIX_E_NOT_SUPPORTED;
      break;
   case VGAUTH_E_NOT_CONNECTED:
      err = VIX_E_FAIL;
      break;
   case VGAUTH_E_VERSION_MISMATCH:
      err = VIX_E_FAIL;
      break;
   case VGAUTH_E_SECURITY_VIOLATION:
      err = VIX_E_FAIL;
      break;
   case VGAUTH_E_CERT_ALREADY_EXISTS:
      err = VIX_E_INVALID_ARG;
      break;
   case VGAUTH_E_AUTHENTICATION_DENIED:
      err = VIX_E_INVALID_LOGIN_CREDENTIALS;
      break;
   case VGAUTH_E_INVALID_TICKET:
      err = VIX_E_INVALID_ARG;
      break;
   case VGAUTH_E_MULTIPLE_MAPPINGS:
      err = VIX_E_GUEST_AUTH_MULIPLE_MAPPINGS;
      break;
   case VGAUTH_E_ALREADY_IMPERSONATING:
      err = VIX_E_FAIL;
      break;
   case VGAUTH_E_NO_SUCH_USER:
      err = VIX_E_INVALID_ARG;
      break;
   case VGAUTH_E_SERVICE_NOT_RUNNING:
   case VGAUTH_E_SYSTEM_ERRNO:
   case VGAUTH_E_SYSTEM_WINDOWS:
   case VGAUTH_E_TOO_MANY_CONNECTIONS:
      err = VIX_E_FAIL;
      break;
   case VGAUTH_E_UNSUPPORTED:
      err = VIX_E_NOT_SUPPORTED;
      break;
   default:
      err = VIX_E_FAIL;
      g_warning("%s: error code "VGAUTHERR_FMT64X" has no translation\n",
                __FUNCTION__, vgErr);
      break;
   }
   g_debug("%s: translated VGAuth err "VGAUTHERR_FMT64X" to Vix err %"FMT64"d\n",
           __FUNCTION__, vgErr, err);


   return err;
}
#endif


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

VixError
VixTools_GetToolsPropertiesImpl(GKeyFile *confDictRef,            // IN
                                char **resultBuffer,              // OUT
                                size_t *resultBufferLength)       // OUT
{
   VixError err = VIX_OK;
   VixPropertyListImpl propList;
   char *serializedBuffer = NULL;
   size_t serializedBufferLength = 0;
#if !defined(__FreeBSD__)
   char *guestName;
   int osFamily;
   char *packageList = NULL;
   char *powerOffScript = NULL;
   char *powerOnScript = NULL;
   char *resumeScript = NULL;
   char *suspendScript = NULL;
   char *osName = NULL;
   char *osNameFull = NULL;
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
   foundHostName = System_GetNodeName(512, guestName);
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
#else
   osFamily = GUEST_OS_FAMILY_LINUX;
#endif

   osNameFull = Hostinfo_GetOSName();
   if (osNameFull == NULL) {
      osNameFull = Util_SafeStrdup("");
   }

   osName = Hostinfo_GetOSGuestString();
   if (osName == NULL) {
      osName = Util_SafeStrdup("");
   }

   wordSize = Hostinfo_GetSystemBitness();
   if (wordSize <= 0) {
      wordSize = 32;
   }

   /*
    * TODO: Something with this.
    */
   packageList = "";

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

   tempDir = File_GetSafeRandomTmpDir(TRUE);

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
                                    VIX_TOOLSFEATURE_SUPPORT_GET_HANDLE_STATE);
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

   /* Retrieve the share folders UNC root path. */
   err = VixToolsSetSharedFoldersProperties(&propList);
   if (VIX_OK != err) {
      goto abort;
   }

   /* Set up the API status properties */
   err = VixToolsSetAPIEnabledProperties(&propList, confDictRef);
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
   free(osName);
   free(osNameFull);
   free(suspendScript);
   free(resumeScript);
   free(powerOnScript);
   free(powerOffScript);
#else
   /*
    * FreeBSD. We do not require all the properties above.
    * We only Support VMODL Guest Ops for now (Bug 228398).
    */

   VixPropertyList_Initialize(&propList);

   /* InitiateFileTransfer(From|To)Guest operations require this */
   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_GUEST_OS_FAMILY,
                                    GUEST_OS_FAMILY_LINUX);
   if (VIX_OK != err) {
      goto abort;
   }
   /* Retrieve the share folders UNC root path. */
   err = VixToolsSetSharedFoldersProperties(&propList);
   if (VIX_OK != err) {
      goto abort;
   }
   /*
    * Set up the API status properties.
    * This is done so that the client side can tell the
    * difference between OutOfDate tools and NotSupported.
    */
   err = VixToolsSetAPIEnabledProperties(&propList, confDictRef);
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
   free(serializedBuffer);
#endif // __FreeBSD__

   return err;
} // VixTools_GetToolsPropertiesImpl


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsSetSharedFoldersProperties --
 *
 *    Set information about the shared folders feature.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsSetSharedFoldersProperties(VixPropertyListImpl *propList)    // IN
{
   VixError err = VIX_OK;

   /* Retrieve the share folders UNC root path. */
   char *hgfsRootPath = NULL;

   if (!HgfsHlpr_QuerySharesDefaultRootPath(&hgfsRootPath)) {
      /* Exit ok as we have nothing to set from shared folders. */
      goto exit;
   }

   ASSERT(hgfsRootPath != NULL);

   err = VixPropertyList_SetString(propList,
                                   VIX_PROPERTY_GUEST_SHAREDFOLDERS_SHARES_PATH,
                                   hgfsRootPath);
   if (VIX_OK != err) {
      goto exit;
   }

exit:
   if (hgfsRootPath != NULL) {
      HgfsHlpr_FreeSharesRootPath(hgfsRootPath);
   }
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetAPIDisabledFromConf --
 *
 *    Helper function for fetching the API config setting.
 *
 *    If the varName is NULL, only the global switch is checked.
 *
 * Return value:
 *    Bool
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VixToolsGetAPIDisabledFromConf(GKeyFile *confDictRef,            // IN
                               const char *varName)              // IN
{
   gboolean disabled = FALSE;
   char disabledName[128];

   /*
    * g_key_get_file_boolean() will also return FALSE if there's no
    * entry in the config file.
    */


   /*
    * First check the global kill-switch, which will override the
    * per-API configs if its set.
    */
   if (confDictRef != NULL) {
      disabled = g_key_file_get_boolean(confDictRef,
                                        VIX_TOOLS_CONFIG_API_GROUPNAME,
                                        VIX_TOOLS_CONFIG_API_ALL_NAME,
                                        NULL);
      if (disabled) {
         return TRUE;
      }
   }

   /*
    * Check the individual API if the global kill-switch isn't on.
    */
   if (NULL != varName) {
      Str_Snprintf(disabledName, sizeof(disabledName), "%s.disabled", varName);
      if (confDictRef != NULL) {
         disabled = g_key_file_get_boolean(confDictRef,
                                           VIX_TOOLS_CONFIG_API_GROUPNAME,
                                           disabledName,
                                           NULL);
      }
   }

#if !SUPPORT_VGAUTH
   /*
    * Make sure vgauth related stuff does not show as enabled.
    */
   if (NULL != varName) {
      if ((strcmp(varName, VIX_TOOLS_CONFIG_API_ADD_GUEST_ALIAS_NAME) == 0) ||
          (strcmp(varName, VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_NAME) == 0) ||
          (strcmp(varName, VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_BY_CERT_NAME) == 0) ||
          (strcmp(varName, VIX_TOOLS_CONFIG_API_LIST_GUEST_ALIASES_NAME) == 0) ||
          (strcmp(varName, VIX_TOOLS_CONFIG_API_LIST_GUEST_MAPPED_ALIASES_NAME) == 0)) {
         disabled = TRUE;
      }
   }
#endif

   return disabled;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsComputeEnabledProperty --
 *
 *    Wrapper function for setting ENABLED properties for VMODL APIs.
 *    For supported guest OSes, it uses VixToolsGetAPIDisabledFromConf() to
 *    check.  Otherwise its FALSE.
 *
 *
 * Return value:
 *    Bool
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VixToolsComputeEnabledProperty(GKeyFile *confDictRef,            // IN
                               const char *varName)              // IN
{
   return VixToolsGetAPIDisabledFromConf(confDictRef, varName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsSetAPIEnabledProperties --
 *
 *    Set information about the state of APIs.
 *
 *    This is done for all guests, even those that can't do VMODL
 *    guest APIs, so that the client side knows if the tools are
 *    up-to-date.  If the client side doesn't see an ENABLED property
 *    for an API it knows about, it assumes the tools are out-of-date,
 *    and returns the appropriate error.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsSetAPIEnabledProperties(VixPropertyListImpl *propList,    // IN
                                GKeyFile *confDictRef)            // IN
{
   VixError err = VIX_OK;

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_START_PROGRAM_ENABLED,
                              VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_START_PROGRAM_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_LIST_PROCESSES_ENABLED,
                              VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_LIST_PROCESSES_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_TERMINATE_PROCESS_ENABLED,
                              VixToolsComputeEnabledProperty(confDictRef,
                                 VIX_TOOLS_CONFIG_API_TERMINATE_PROCESS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_READ_ENVIRONMENT_VARIABLE_ENABLED,
                              VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_READ_ENV_VARS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_VALIDATE_CREDENTIALS_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                   VIX_TOOLS_CONFIG_API_VALIDATE_CREDENTIALS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_ACQUIRE_CREDENTIALS_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_ACQUIRE_CREDENTIALS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_RELEASE_CREDENTIALS_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_RELEASE_CREDENTIALS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_MAKE_DIRECTORY_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                                  VIX_TOOLS_CONFIG_API_MAKE_DIRECTORY_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_DELETE_FILE_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                       VIX_TOOLS_CONFIG_API_DELETE_FILE_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_DELETE_DIRECTORY_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_DELETE_DIRECTORY_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_MOVE_DIRECTORY_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                                   VIX_TOOLS_CONFIG_API_MOVE_DIRECTORY_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_MOVE_FILE_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                       VIX_TOOLS_CONFIG_API_MOVE_FILE_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_CREATE_TEMP_FILE_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_CREATE_TMP_FILE_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_CREATE_TEMP_DIRECTORY_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_CREATE_TMP_DIRECTORY_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_LIST_FILES_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                       VIX_TOOLS_CONFIG_API_LIST_FILES_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_CHANGE_FILE_ATTRIBUTES_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_CHANGE_FILE_ATTRS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_INITIATE_FILE_TRANSFER_FROM_GUEST_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_INITIATE_FILE_TRANSFER_FROM_GUEST_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_INITIATE_FILE_TRANSFER_TO_GUEST_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_INITIATE_FILE_TRANSFER_TO_GUEST_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_ADD_AUTH_ALIAS_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_ADD_GUEST_ALIAS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }
   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_REMOVE_AUTH_ALIAS_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }
   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_LIST_AUTH_ALIASES_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_LIST_GUEST_ALIASES_NAME));
   if (VIX_OK != err) {
      goto exit;
   }
   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_LIST_MAPPED_ALIASES_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_LIST_GUEST_MAPPED_ALIASES_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_CREATE_REGISTRY_KEY_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_CREATE_REGISTRY_KEY_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_LIST_REGISTRY_KEYS_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_LIST_REGISTRY_KEYS_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_DELETE_REGISTRY_KEY_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_DELETE_REGISTRY_KEY_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_SET_REGISTRY_VALUE_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_SET_REGISTRY_VALUE_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_LIST_REGISTRY_VALUES_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_LIST_REGISTRY_VALUES_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_DELETE_REGISTRY_VALUE_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_DELETE_REGISTRY_VALUE_NAME));
   if (VIX_OK != err) {
      goto exit;
   }

   err = VixPropertyList_SetBool(propList,
                                 VIX_PROPERTY_GUEST_REMOVE_AUTH_ALIAS_BY_CERT_ENABLED,
                                 VixToolsComputeEnabledProperty(confDictRef,
                                    VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_BY_CERT_NAME));
   if (VIX_OK != err) {
      goto exit;
   }
exit:
   g_debug("%s: returning err %"FMT64"d\n", __FUNCTION__, err);
   return err;
} // VixToolsSetAPIEnabledProperties


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
   VMAutomationRequestParser parser;

   /*
    * Parse the argument
    */
   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *registryRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   registryRequest = (VixMsgRegistryRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            registryRequest->registryKeyLength,
                                            &(const char*)registryPathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *registryPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   if (VIX_PROPERTYTYPE_INTEGER == registryRequest->expectedRegistryKeyType) {
      errResult = Registry_ReadInteger(registryPathName, &valueInt);
      if (ERROR_SUCCESS != errResult) {
         /*
          * E_UNEXPECTED isn't a system err. Don't use Vix_TranslateSystemError
          */
         if (E_UNEXPECTED == errResult) {
            err = VIX_E_REG_INCORRECT_VALUE_TYPE;
         } else {
            err = Vix_TranslateSystemError(errResult);
         }
         goto abort;
      }

      valueStr = Str_SafeAsprintf(NULL, "%d", valueInt);
      if (NULL == valueStr) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
   } else if (VIX_PROPERTYTYPE_STRING == registryRequest->expectedRegistryKeyType) {
      errResult = Registry_ReadString(registryPathName, &valueStr);
      if (ERROR_SUCCESS != errResult) {
         /*
          * E_UNEXPECTED isn't a system err. Don't use Vix_TranslateSystemError
          */
         if (E_UNEXPECTED == errResult) {
            err = VIX_E_REG_INCORRECT_VALUE_TYPE;
         } else {
            err = Vix_TranslateSystemError(errResult);
         }
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

   guest_debug("%s: returning '%s'\n", __FUNCTION__, valueStr);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

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
   VMAutomationRequestParser parser;

   /*
    * Parse the argument
    */
   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *registryRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   registryRequest = (VixMsgRegistryRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            registryRequest->registryKeyLength,
                                            &(const char*)registryPathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *registryPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   if (VIX_PROPERTYTYPE_INTEGER == registryRequest->expectedRegistryKeyType) {
      err = VMAutomationRequestParserGetData(&parser,
                                             registryRequest->dataToWriteSize,
                                             &(const char*)registryData);
      if (VIX_OK != err) {
         goto abort;
      }

      intValue = *((int *) registryData);

      errResult = Registry_WriteInteger(registryPathName, intValue);
      if (ERROR_SUCCESS != errResult) {
         err = Vix_TranslateSystemError(errResult);
         goto abort;
      }
   } else if (VIX_PROPERTYTYPE_STRING == registryRequest->expectedRegistryKeyType) {
      err = VMAutomationRequestParserGetOptionalString(&parser,
                                            registryRequest->dataToWriteSize,
                                               &(const char*)registryData);
      if (VIX_OK != err) {
         goto abort;
      }

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

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

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
   const char *pathName = NULL;
   int resultInt;
   Bool resultBool;
   Bool success;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgSimpleFileRequest *fileRequest;
   VMAutomationRequestParser parser;

   /*
    * Parse the argument
    */
   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *fileRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   fileRequest = (VixMsgSimpleFileRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            fileRequest->guestPathNameLength,
                                            &pathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *pathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s path: '%s'\n",
           __FUNCTION__, IMPERSONATED_USERNAME, pathName);

   ///////////////////////////////////////////
   if ((VIX_COMMAND_DELETE_GUEST_FILE == requestMsg->opCode) ||
       (VIX_COMMAND_DELETE_GUEST_FILE_EX == requestMsg->opCode)) {
      /*
       * if pathName is an invalid symbolic link, we still want to delete it.
       */
      if (FALSE == File_IsSymLink(pathName)) {
         if (!(File_Exists(pathName))) {
            err = FoundryToolsDaemon_TranslateSystemErr();
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
         err = FoundryToolsDaemon_TranslateSystemErr();
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
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
      resultBool = File_IsDirectory(pathName);
      if (!resultBool) {
         err = VIX_E_NOT_A_DIRECTORY;
         goto abort;
      }
      success = File_DeleteEmptyDirectory(pathName);
      if (!success) {
#if !defined(_WIN32)
         /*
          * If the specified directory is not empty then
          * File_DeleteEmptyDirectory() fails and
          * 1. errno is set to either EEXIST or ENOTEMPTY on linux platforms.
          * 2. errno is set EEXIST on Solaris platforms.
          *
          * To maintain consistency across different Posix platforms, lets
          * re-write the error before returning.
          */
         if (EEXIST == errno) {
            errno = ENOTEMPTY;
         }
#endif
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

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsDeleteObject


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsDeleteDirectory --
 *
 *    Delete a directory on the guest.
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
VixToolsDeleteDirectory(VixCommandRequestHeader *requestMsg)  // IN
{
   VixError err = VIX_OK;
   const char *directoryPath = NULL;
   Bool success;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   Bool recursive = TRUE;
   VixMsgDeleteDirectoryRequest *deleteDirectoryRequest;
   VMAutomationRequestParser parser;

   ASSERT(NULL != requestMsg);

   /*
    * Parse the argument
    */
   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg,
                                       sizeof *deleteDirectoryRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   deleteDirectoryRequest = (VixMsgDeleteDirectoryRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            deleteDirectoryRequest->guestPathNameLength,
                                            &directoryPath);
   if (VIX_OK != err) {
      goto abort;
   }

   if ('\0' == *directoryPath) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   recursive = deleteDirectoryRequest->recursive;

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s path: '%s' recursive: %d\n",
           __FUNCTION__, IMPERSONATED_USERNAME, directoryPath, (int) recursive);

   success = File_Exists(directoryPath);
   if (!success) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   if (File_IsSymLink(directoryPath) || File_IsFile(directoryPath)) {
      err = VIX_E_NOT_A_DIRECTORY;
      goto abort;
   }

   if (recursive) {
      success = File_DeleteDirectoryTree(directoryPath);
   } else {
      success = File_DeleteEmptyDirectory(directoryPath);
   }

   if (!success) {
      if (!recursive) {
#if !defined(_WIN32)
         /*
          * If the specified directory is not empty then
          * File_DeleteEmptyDirectory() fails and
          * 1. errno is set to either EEXIST or ENOTEMPTY on linux platforms.
          * 2. errno is set EEXIST on Solaris platforms.
          *
          * To maintain consistency across different Posix platforms, lets
          * re-write the error before returning.
          */
         if (EEXIST == errno) {
            errno = ENOTEMPTY;
         }
#endif
      }
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsDeleteDirectory


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
   VMAutomationRequestParser parser;

   /*
    * Parse the argument
    */
   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *fileRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   fileRequest = (VixMsgSimpleFileRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            fileRequest->guestPathNameLength,
                                            (const char **)&pathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *pathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s path: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME, pathName);

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

   guest_debug("%s: returning '%s'\n", __FUNCTION__, resultBuffer);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsObjectExists


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsCreateTempFile --
 *
 *    Wrapper to call VixToolsCreateTempFileInt.
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
   VixError err = VixToolsCreateTempFileInt(requestMsg, FALSE, result);
#ifdef _WIN32
   /*
    * PR 2155708: CreateTemporaryFileInGuest succeeds and returns a file
    * path that does not exist when using user name format "domain\user"
    * if the user does not have a profile folder created before, such as
    * by interactively logging onto Windows. What happens here is that
    * Win32 API UnloadUserProfile() deletes the temp user profile folder
    * "C:\Users\TEMP" in the end.
    * Verify existence of the returned path, retry the guest OP using
    * system temp folder if the path disappears.
    */
   if (VIX_SUCCEEDED(err) && *result != NULL && !File_Exists(*result)) {
      host_warning("%s: retry using system temp.\n",
                   __FUNCTION__);
      guest_warning("%s: '%s' does not exist, retry using system temp.\n",
                    __FUNCTION__, *result);
      free(*result);
      *result = NULL;
      err = VixToolsCreateTempFileInt(requestMsg, TRUE, result);
   }
#endif

   return err;
} // VixToolsCreateTempFile


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsCreateTempFileInt --
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
VixToolsCreateTempFileInt(VixCommandRequestHeader *requestMsg,   // IN
                          Bool useSystemTemp,                    // IN
                          char **result)                         // OUT: UTF-8
{
   VixError err = VIX_OK;
   char *filePathName = NULL;
   int fd = -1;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;

   if ((VIX_COMMAND_CREATE_TEMPORARY_FILE != requestMsg->opCode) &&
       (VIX_COMMAND_CREATE_TEMPORARY_FILE_EX != requestMsg->opCode) &&
       (VIX_COMMAND_CREATE_TEMPORARY_DIRECTORY != requestMsg->opCode)) {
      ASSERT(0);
      err = VIX_E_FAIL;
      g_warning("%s: Received a request with an invalid opcode: %d\n",
                __FUNCTION__, requestMsg->opCode);
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);

   err = VixToolsGetTempFile(requestMsg, userToken, useSystemTemp,
                             &filePathName, &fd);
   if (VIX_FAILED(err)) {
      goto abort;
   }

   /*
    * Just close() the file, since we're not going to use it. But, when we
    * create a temporary directory, VixToolsGetTempFile() sets 'fd' to 0 on
    * success. On windows, close() shouldn't be called for invalid fd values.
    * So, call close() only if 'fd' is valid.
    */
   if (fd > 0) {
      if (close(fd) < 0) {
         g_warning("%s: Unable to close a file, errno is %d.\n",
                   __FUNCTION__, errno);
      }
   }

   *result = filePathName;

   guest_debug("%s: returning '%s'\n", __FUNCTION__, filePathName);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsCreateTempFileInt


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
   const char *valueName = NULL;
   VMAutomationRequestParser parser;

   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *readRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   readRequest = (VixMsgReadVariableRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            readRequest->nameLength,
                                            &valueName);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s var: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME, valueName);

   switch (readRequest->variableType) {
   case VIX_GUEST_ENVIRONMENT_VARIABLE:
      /*
       * Alwasy get environment variable for the current user, even if the
       * current user is root/administrator
       */
#ifndef _WIN32
      /*
       * If we are maintaining our own set of environment variables
       * because the application we're running from changed the user's
       * environment, then we should be reading from that.
       */
      if (NULL != userEnvironmentTable) {
         if (HashTable_Lookup(userEnvironmentTable, valueName,
                              (void **) &value)) {
            value = Util_SafeStrdup(value);
         } else {
            value = Util_SafeStrdup("");
         }
         break;
      }
#endif

      err = VixToolsGetEnvForUser(userToken, valueName, &value);
      if (VIX_OK != err) {
         goto abort;
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

   guest_debug("%s: returning '%s'\n", __FUNCTION__, value);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsReadVariable


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetEnvVarForUser --
 *
 *      Reads a single environment variable from the given user's
 *      environment.
 *
 * Results:
 *      VixError
 *      'value' points to a heap-allocated string containing the value.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsGetEnvForUser(void *userToken,       // IN
                      const char *name,      // IN
                      char **value)          // OUT
{
   VixError err;
   VixToolsUserEnvironment *env;

   ASSERT(NULL != value);

   err = VixToolsNewUserEnvironment(userToken, &env);
   if (VIX_FAILED(err)) {
      return err;
   }

   *value = VixToolsGetEnvFromUserEnvironment(env, name);
   VixToolsDestroyUserEnvironment(env);
   if (NULL == *value) {
      *value = Util_SafeStrdup("");
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsReadEnvVariables --
 *
 *    Read environment variables in the guest. The name of the environment
 *    variables are expected to be in UTF-8.
 *
 *    If a variable doesn't exist, nothing is returned for it.
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
VixToolsReadEnvVariables(VixCommandRequestHeader *requestMsg,   // IN
                         char **result)                         // OUT: UTF-8
{
   VixError err = VIX_OK;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgReadEnvironmentVariablesRequest *readRequest;
   char *results = NULL;
   VMAutomationRequestParser parser;
   const char *names = NULL;

   err = VMAutomationRequestParserInit(&parser,
                                      requestMsg, sizeof *readRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   readRequest = (VixMsgReadEnvironmentVariablesRequest *) requestMsg;
   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s reading %d vars\n",
           __FUNCTION__, IMPERSONATED_USERNAME, readRequest->numNames);

   err = VMAutomationRequestParserGetOptionalStrings(&parser,
                                                     readRequest->numNames,
                                                     readRequest->namesLength,
                                                     &names);
   if (VIX_OK != err) {
      goto abort;
   }

   if (readRequest->numNames > 0) {
      err = VixToolsGetMultipleEnvVarsForUser(userToken, names,
                                              readRequest->numNames,
                                              &results);
      if (VIX_FAILED(err)) {
         goto abort;
      }
   } else {
      /*
       * If none are specified, return all of them.
       */
      err = VixToolsGetAllEnvVarsForUser(userToken, &results);
      if (VIX_FAILED(err)) {
         goto abort;
      }
   }

   *result = results;

   guest_debug("%s: returning '%s'\n", __FUNCTION__, results);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsReadEnvVariables


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetMultipleEnvVarsForUser --
 *
 *      Populates result with an XML-like string containing all the
 *      environment variables listed starting at 'names' (each name is
 *      separated by a null character).
 *      The result string will contain zero or more entries of the form
 *      <ev>NAME=VALUE</ev> without any delimiting characters.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsGetMultipleEnvVarsForUser(void *userToken,       // IN
                                  const char *names,     // IN
                                  unsigned int numNames, // IN
                                  char **result)         // OUT
{
   VixError err;
   unsigned int i;
   char *resultLocal = Util_SafeStrdup("");  // makes the loop cleaner.
   VixToolsUserEnvironment *env;

#ifdef __FreeBSD__
   if (NULL == userEnvironmentTable) {
      err = VIX_E_FAIL;
      free(resultLocal);
      return err;
   }
#endif

   err = VixToolsNewUserEnvironment(userToken, &env);
   if (VIX_FAILED(err)) {
      env = NULL;
      goto abort;
   }

   for (i = 0; i < numNames; i++) {
      char *value;

#ifdef __FreeBSD__
      /*
       * We should check the original envp for all vars except
       * a few whitelisted ones that we set/unset on impersonate
       * user start/stop. for them we need to do getenv()
       */
      if (!strcmp(names, "USER") ||
          !strcmp(names, "HOME") ||
          !strcmp(names, "SHELL")) {
         value = VixToolsGetEnvFromUserEnvironment(env, names);
      }
      else {
         if (HashTable_Lookup(userEnvironmentTable,
                              names, (void **) &value)) {
            value = Util_SafeStrdup(value);
         } else {
            value = Util_SafeStrdup("");
         }
      }
#else
      value = VixToolsGetEnvFromUserEnvironment(env, names);
#endif

      if (NULL != value) {
         char *tmp = resultLocal;
         char *tmpVal;
         char *escapedName;

         escapedName = VixToolsEscapeXMLString(names);
         if (NULL == escapedName) {
            err = VIX_E_OUT_OF_MEMORY;
            goto loopCleanup;
         }

         tmpVal = VixToolsEscapeXMLString(value);
         if (NULL == tmpVal) {
            err = VIX_E_OUT_OF_MEMORY;
            goto loopCleanup;
         }
         free(value);
         value = tmpVal;

         resultLocal = Str_SafeAsprintf(NULL, "%s<ev>%s=%s</ev>",
                                    tmp, escapedName, value);
         free(tmp);
         if (NULL == resultLocal) {
            err = VIX_E_OUT_OF_MEMORY;
         }

      loopCleanup:
         free(value);
         free(escapedName);
         if (VIX_OK != err) {
            goto abort;
         }
      }

      names += strlen(names) + 1;
   }

   *result = resultLocal;
   resultLocal = NULL;
   err = VIX_OK;

abort:
   free(resultLocal);
   VixToolsDestroyUserEnvironment(env);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetAllEnvVarsForUser --
 *
 *      Populates result with an XML-like string containing all the
 *      environment variables set for the user represented by 'userToken'.
 *      The result string will contain zero or more entries of the form
 *      <ev>NAME=VALUE</ev> without any delimiting characters.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsGetAllEnvVarsForUser(void *userToken,     // IN
                             char **result)       // OUT
{
   VixError err;
   char *resultLocal;
   VixToolsEnvIterator *itr;
   char *envVar;
#ifdef __FreeBSD__
   char **envp;
   if (NULL == userEnvironmentTable) {
      err = VIX_E_FAIL;
      return err;
   }
   envp = VixToolsEnvironmentTableToEnvp(userEnvironmentTable);
#endif

   if (NULL == result) {
      err = VIX_E_FAIL;
      return err;
   }

   resultLocal = Util_SafeStrdup("");  // makes the loop cleaner.

#ifdef __FreeBSD__
   err = VixToolsNewEnvIterator(userToken, envp, &itr);
#else
   err = VixToolsNewEnvIterator(userToken, &itr);
#endif
   if (VIX_FAILED(err)) {
      goto abort;
   }

   while ((envVar = VixToolsGetNextEnvVar(itr)) != NULL) {
      char *tmp = resultLocal;
      char *tmpVal;
#ifdef __FreeBSD__
      /*
       * For variables we change during Impersonatation of user,
       * we need to fetch from getenv() system call, all else
       * can be read from the hash table of the original envp.
       */
      if (StrUtil_StartsWith(envVar, "USER=") ||
          StrUtil_StartsWith(envVar, "HOME=") ||
          StrUtil_StartsWith(envVar, "SHELL=")) {
         char *name = NULL;
         char *escapedName = NULL;
         char *whereToSplit;
         size_t nameLen;

         whereToSplit = strchr(envVar, '=');
         if (NULL == whereToSplit) {
            /* Our code generated this list, so this shouldn't happen. */
            ASSERT(0);
            continue;
         }

         nameLen = whereToSplit - envVar;
         name = Util_SafeMalloc(nameLen + 1);
         memcpy(name, envVar, nameLen);
         name[nameLen] = '\0';

         escapedName = VixToolsEscapeXMLString(name);

         free(envVar);
         envVar = Str_SafeAsprintf(NULL, "%s=%s",
                                   escapedName, Posix_Getenv(name));

         free(name);
         free(escapedName);
      }
#endif
      tmpVal = VixToolsEscapeXMLString(envVar);
      free(envVar);
      if (NULL == tmpVal) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      envVar = tmpVal;

      resultLocal = Str_SafeAsprintf(NULL, "%s<ev>%s</ev>", tmp, envVar);
      free(tmp);
      free(envVar);
      if (NULL == resultLocal) {
         g_warning("%s: Out of memory.\n", __FUNCTION__);
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
   }

abort:
   VixToolsDestroyEnvIterator(itr);
#ifdef __FreeBSD__
   VixToolsFreeEnvp(envp);
#endif
   *result = resultLocal;

   return err;
}


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

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s name: %s value %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME, valueName, value);

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
         goto abort;
      }

#ifndef _WIN32
      /*
       * We need to make sure that this change is reflected in the table of
       * environment variables we use when launching programs. This is so if a
       * a user sets LD_LIBRARY_PATH with WriteVariable, and then calls
       * RunProgramInGuest, that program will see the new value.
       */
      if (NULL != userEnvironmentTable) {
         /*
          * The hash table will make a copy of valueName, but we have to supply
          * a deep copy of the value.
          */
         HashTable_ReplaceOrInsert(userEnvironmentTable, valueName,
                                   Util_SafeStrdup(value));
      }
#endif
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

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsWriteVariable


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsMoveObject --
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
VixToolsMoveObject(VixCommandRequestHeader *requestMsg)        // IN
{
   VixError err = VIX_OK;
   const char *srcFilePathName = NULL;
   const char *destFilePathName = NULL;
   Bool success;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   Bool overwrite = TRUE;
   VMAutomationRequestParser parser;
   int srcPathLen, destPathLen;


   if (VIX_COMMAND_MOVE_GUEST_FILE == requestMsg->opCode) {
      VixCommandRenameFileRequest *renameRequest;

      err = VMAutomationRequestParserInit(&parser,
                                          requestMsg, sizeof *renameRequest);
      if (VIX_OK != err) {
         goto abort;
      }

      renameRequest = (VixCommandRenameFileRequest *) requestMsg;
      srcPathLen = renameRequest->oldPathNameLength;
      destPathLen = renameRequest->newPathNameLength;
   } else if ((VIX_COMMAND_MOVE_GUEST_FILE_EX == requestMsg->opCode) ||
              (VIX_COMMAND_MOVE_GUEST_DIRECTORY == requestMsg->opCode)) {
      VixCommandRenameFileRequestEx *renameRequest;

      err = VMAutomationRequestParserInit(&parser,
                                          requestMsg, sizeof *renameRequest);
      if (VIX_OK != err) {
         goto abort;
      }

      renameRequest = (VixCommandRenameFileRequestEx *) requestMsg;
      srcPathLen = renameRequest->oldPathNameLength;
      destPathLen = renameRequest->newPathNameLength;
      overwrite = renameRequest->overwrite;
   } else {
      ASSERT(0);
      g_warning("%s: Invalid request with opcode %d received\n ",
                __FUNCTION__, requestMsg->opCode);
      err = VIX_E_FAIL;
      goto abort;
   }

   err = VMAutomationRequestParserGetString(&parser,
                                            srcPathLen,
                                            &srcFilePathName);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VMAutomationRequestParserGetString(&parser,
                                            destPathLen,
                                            &destFilePathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if ((0 == *srcFilePathName) || (0 == *destFilePathName)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s src: %s dst: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME,
           srcFilePathName, destFilePathName);

   if (!(File_Exists(srcFilePathName))) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

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
    * pre-check the dest arg -- File_Move() will return
    * diff err codes depending on OS, so catch it up front (bug 133165)
    */
   if (File_IsDirectory(destFilePathName)) {
      if ((VIX_COMMAND_MOVE_GUEST_FILE_EX == requestMsg->opCode) ||
          (VIX_COMMAND_MOVE_GUEST_DIRECTORY == requestMsg->opCode)) {

         /*
          * If File_IsDirectory() returns true, it doesn't mean the
          * filepath points to a real directory. It may point to a symlink.
          * So perform a quick symlink check. Do this only for opcodes
          * related to VI Guest Operations. Otherwise, it may affect
          * the existing tests.
          */
         if (!File_IsSymLink(destFilePathName)) {
            /*
             * If we are implementing opcodes related to VI Guest operations,
             * then return VIX_E_FILE_ALREADY_EXISTS. Don't change the error
             * code for opcode related to VIX C api. It will break the existing
             * tests.
            */
            err = VIX_E_FILE_ALREADY_EXISTS;
            goto abort;
         }
      } else {
         err = VIX_E_ALREADY_EXISTS;
         goto abort;
      }
   }

   if (VIX_COMMAND_MOVE_GUEST_FILE_EX == requestMsg->opCode) {
      if (File_IsDirectory(srcFilePathName)) {
         /*
          * Be careful while executing File_[File|Directory] operations.
          * In case of symlinks, these functions are smart engough to
          * resolve the final component pointed by the symlink and do
          * the check on the final component.
          *
          * For VI guest operations, MoveFile should return
          * VIX_E_NOT_A_FILE if the file path points to a real directory.
          * File_IsDirectory() returns true if it is invoked on a
          * symlink that points to a directory. So, we have to
          * filter out that case before returning VIX_E_NOT_A_FILE.
          */
         if (!File_IsSymLink(srcFilePathName)) {
            err = VIX_E_NOT_A_FILE;
            goto abort;
         }
      }
      if (!overwrite) {
         if (File_Exists(destFilePathName)) {
            err = VIX_E_FILE_ALREADY_EXISTS;
            goto abort;
         }
      }
   } else if (VIX_COMMAND_MOVE_GUEST_DIRECTORY == requestMsg->opCode) {
      /*
       * For VI guest operations, MoveDirectory should return
       * VIX_E_NOT_A_DIRECTORY if the file path doesn't point to a real
       * directory. File_IsDirectory() returns false if it is invoked on
       * a symlink that points to a file. So, we should include that
       * check before returning VIX_E_NOT_A_DIRECTORY.
       */
      if (!(File_IsDirectory(srcFilePathName)) ||
          (File_IsSymLink(srcFilePathName))) {
         err = VIX_E_NOT_A_DIRECTORY;
         goto abort;
      }

      /*
       * In case of moving a directory, File_Move() returns different
       * errors on different Guest Os if the destination file path points
       * to an existing file. We should catch them upfront and report them
       * to the user.
       * As per the documentation for rename() on linux, if the source
       * file points to an existing directory, then destionation file
       * should not point to anything other than a directory.
       */
      if (File_IsSymLink(destFilePathName) || File_IsFile(destFilePathName)) {
         err = VIX_E_FILE_ALREADY_EXISTS;
         goto abort;
      }
   }

   success = File_Move(srcFilePathName, destFilePathName, NULL);
   if (!success) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      g_warning("%s: File_Move failed.\n", __FUNCTION__);
      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsMoveObject


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsInitiateFileTransferFromGuest --
 *
 *    This function is called to implement
 *    InitiateFileTransferFromGuest VI guest operation. Specified filepath
 *    should not point to a directory or a symlink.
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
VixToolsInitiateFileTransferFromGuest(VixCommandRequestHeader *requestMsg,    // IN
                                      char **result)                          // OUT
{
   VixError err = VIX_OK;
   const char *filePathName = NULL;
   char *resultBuffer = NULL;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   // re-use of ListFiles op
   VixMsgListFilesRequest *commandRequest = NULL;
   VMAutomationRequestParser parser;

   ASSERT(NULL != requestMsg);
   ASSERT(NULL != result);

   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *commandRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   commandRequest = (VixMsgListFilesRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            commandRequest->guestPathNameLength,
                                            &filePathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *filePathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s filePath: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME, filePathName);

   if (File_IsSymLink(filePathName)){
      g_warning("%s: File path cannot point to a symlink.\n", __FUNCTION__);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   if (File_IsDirectory(filePathName)) {
      err = VIX_E_NOT_A_FILE;
      goto abort;
   }

   if (!File_Exists(filePathName)) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   resultBuffer = VixToolsPrintFileExtendedInfoEx(filePathName, filePathName);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   if (NULL == resultBuffer) {
      resultBuffer = Util_SafeStrdup("");
   }
   *result = resultBuffer;

   guest_debug("%s: returning '%s'\n", __FUNCTION__, resultBuffer);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsInitiateFileTransferFromGuest


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsInitiateFileTransferToGuest --
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
VixToolsInitiateFileTransferToGuest(VixCommandRequestHeader *requestMsg)  // IN
{
   VixError err = VIX_OK;
   const char *guestPathName = NULL;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   Bool overwrite = TRUE;
   char *dirName = NULL;
   char *baseName = NULL;
   int32 fileAttributeOptions = 0;
#if defined(_WIN32)
   int fd = -1;
   char *tempFilePath = NULL;
   static char *tempFileBaseName = "vmware";
#endif
   FileIOResult res;

   VixCommandInitiateFileTransferToGuestRequest *commandRequest;
   VMAutomationRequestParser parser;

   ASSERT(NULL != requestMsg);

   /*
    * Parse the argument
    */
   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg,
                                       sizeof *commandRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   commandRequest = (VixCommandInitiateFileTransferToGuestRequest *) requestMsg;
   overwrite = commandRequest->overwrite;

   err = VMAutomationRequestParserGetString(&parser,
                                            commandRequest->guestPathNameLength,
                                            &guestPathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if ('\0' == *guestPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   fileAttributeOptions = commandRequest->options;

#if defined(_WIN32)
   if ((fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_OWNERID) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_GROUPID) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_PERMISSIONS)) {
      g_warning("%s: Invalid attributes received for Windows Guest\n",
                __FUNCTION__);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
#else
   if ((fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_HIDDEN) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_READONLY)) {
      g_warning("%s: Invalid attributes received for Unix Guest\n",
                __FUNCTION__);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
#endif

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s path: %s attrs: %d\n",
           __FUNCTION__, IMPERSONATED_USERNAME,
           guestPathName, fileAttributeOptions);

   if (File_IsSymLink(guestPathName)) {
      g_warning("%s: Filepath cannot point to a symlink.\n", __FUNCTION__);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   if (File_Exists(guestPathName)) {
      if (File_IsDirectory(guestPathName)) {
         err = VIX_E_NOT_A_FILE;
      } else if (!overwrite) {
         err = VIX_E_FILE_ALREADY_EXISTS;
      } else {
         /*
          * If the file exists and overwrite flag is true, then check
          * if the file is writable. If not, return a proper error.
          */
         res = FileIO_Access(guestPathName, FILEIO_ACCESS_WRITE);
         if (FILEIO_SUCCESS != res) {
            /*
             * On Linux guests, FileIO_Access sets the proper errno
             * on failure. On Windows guests, last errno is not
             * set when FileIO_Access fails. So, we cannot use
             * FoundryToolsDaemon_TranslateSystemErr() to translate the
             * error. To maintain consistency for all the guests,
             * return an explicit VIX_E_FILE_ACCESS_ERROR.
             */
            err = VIX_E_FILE_ACCESS_ERROR;
            g_warning("%s: Unable to get access permissions for the file: %s\n",
                       __FUNCTION__, guestPathName);
         }
      }
      goto abort;
   }

   File_GetPathName(guestPathName, &dirName, &baseName);
   if ((NULL == dirName) || (NULL == baseName)) {
      g_debug("%s: File_GetPathName failed for '%s', dirName='%s', "
              "baseName='%s'.\n", __FUNCTION__, guestPathName,
              dirName ? dirName : "(null)",
              baseName ? baseName : "(null)");
      err = VIX_E_FILE_NAME_INVALID;
      goto abort;
   }

#ifndef _WIN32
   if ('\0' == *dirName && '/' == *guestPathName) {
      /*
       * dirName is empty and represents root directory
       * For *nix like paths, changing dirName to '/'
       */
      free(dirName);
      dirName = Util_SafeStrdup("/");
   }
#endif

   if (!File_IsDirectory(dirName)) {
#ifdef _WIN32
      DWORD sysErr = GetLastError();
#else
      int sysErr = errno;
#endif
      g_debug("%s: File_IsDirectory failed for '%s', err=%d.\n",
              __FUNCTION__, dirName, sysErr);
      err = VIX_E_FILE_NAME_INVALID;
      goto abort;
   }

#if defined(_WIN32)
   /*
    * Ideally, we just need to check if the user has proper write
    * access to create a child inside the directory. This can be
    * checked by calling FileIO_Access(). FileIO_Access works perfectly
    * fine for linux platforms. But on Windows, FileIO_Access just
    * checks the read-only attribute of the directory and returns the result
    * based on that. This is not the proper way to check the write
    * permissions.
    *
    * One other way to check the write access is to call CreateFile()
    * with GENERIC_WRITE and OPEN_EXISTING flags. Check the documentation
    * for CreateFile() at
    * http://msdn.microsoft.com/en-us/library/aa363858%28v=VS.85%29.aspx.
    * But this has got few limitations. CreateFile() doesn't return proper
    * result when called for directories on NTFS systems.
    * Checks the KB article available at
    * http://support.microsoft.com/kb/810881.
    *
    * So, for windows, the best bet is to create an empty temporary file
    * inside the directory and immediately unlink that. If creation is
    * successful, it ensures that the user has proper write access for
    * the directory.
    *
    * Since we are just checking the write access, there is no need to
    * create the temporary file with the exact specified filename. Any name
    * would be fine.
    */
   fd = File_MakeTempEx(dirName, tempFileBaseName, &tempFilePath);

   if (fd > 0) {
      close(fd);
      File_UnlinkNoFollow(tempFilePath);
   } else {
      /*
       * File_MakeTempEx() function internally uses Posix variant
       * functions and proper error will be stuffed in errno variable.
       * If File_MakeTempEx() fails, then use Vix_TranslateErrno()
       * to translate the errno to a proper foundry error.
       */
      err = Vix_TranslateErrno(errno);
      g_warning("%s: Unable to create a temp file to test directory "
                "permissions, errno is %d\n", __FUNCTION__, errno);
      goto abort;
   }

   free(tempFilePath);
#else
   /*
    * We need to check if the user has write access to create
    * a child inside the directory. Call FileIO_Access() to check
    * for the proper write permissions for the directory.
    */
   res = FileIO_Access(dirName, FILEIO_ACCESS_WRITE);

   if (FILEIO_SUCCESS != res) {
      /*
       * On Linux guests, FileIO_Access sets the proper errno
       * on failure. On Windows guests, last errno is not
       * set when FileIO_Access fails. So, we cannot use
       * FoundryToolsDaemon_TranslateSystemErr() to translate the
       * error. To maintain consistency for all the guests,
       * return an explicit VIX_E_FILE_ACCESS_ERROR.
       */
      err = VIX_E_FILE_ACCESS_ERROR;
      g_warning("%s: Unable to get access permissions for the directory: %s\n",
                __FUNCTION__, dirName);
      goto abort;
   }
#endif

abort:
   free(baseName);
   free(dirName);

   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsInitiateFileTransferToGuest


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
                      size_t maxBufferSize,                // IN
                      char **result)                       // OUT
{
   VixError err = VIX_OK;
   int i;
   static char resultBuffer[GUESTMSG_MAX_IN_SIZE];
   ProcMgrProcInfoArray *procList = NULL;
   ProcMgrProcInfo *procInfo;
   char *destPtr;
   char *endDestPtr;
   char *cmdNamePtr = NULL;
   char *procBufPtr = NULL;
   size_t procBufSize;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   Bool escapeStrs;
   char *escapedName = NULL;
   char *escapedUser = NULL;
   char *escapedCmd = NULL;
   size_t procCount;

   ASSERT(maxBufferSize <= GUESTMSG_MAX_IN_SIZE);

   destPtr = resultBuffer;
   *destPtr = 0;

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s \n",
           __FUNCTION__, IMPERSONATED_USERNAME);

   escapeStrs = (requestMsg->requestFlags &
                 VIX_REQUESTMSG_ESCAPE_XML_DATA) != 0;

   procList = ProcMgr_ListProcesses();
   if (NULL == procList) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   endDestPtr = resultBuffer + maxBufferSize;

   if (escapeStrs) {
      destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr, "%s",
                             VIX_XML_ESCAPED_TAG);
   }

   procCount = ProcMgrProcInfoArray_Count(procList);
   for (i = 0; i < procCount; i++) {
      const char *name;
      const char *user;

      procInfo = ProcMgrProcInfoArray_AddressOf(procList, i);

      if (NULL != procInfo->procCmdName) {
         if (escapeStrs) {
            escapedCmd =
            VixToolsEscapeXMLString(procInfo->procCmdName);
            if (NULL == escapedCmd) {
               err = VIX_E_OUT_OF_MEMORY;
               goto abort;
            }
            cmdNamePtr = Str_SafeAsprintf(NULL,
                                          "<cmd>%s</cmd>",
                                          escapedCmd);
         } else {
            cmdNamePtr = Str_SafeAsprintf(NULL,
                                          "<cmd>%s</cmd>",
                                          procInfo->procCmdName);
         }
      } else {
         cmdNamePtr = Util_SafeStrdup("");
      }

      if (escapeStrs) {
         name = escapedName =
            VixToolsEscapeXMLString(procInfo->procCmdLine);
         if (NULL == escapedName) {
            err = VIX_E_OUT_OF_MEMORY;
            goto abort;
         }
      } else {
         name = procInfo->procCmdLine;
      }

      if (NULL != procInfo->procOwner) {
         if (escapeStrs) {
            user = escapedUser =
               VixToolsEscapeXMLString(procInfo->procOwner);
            if (NULL == escapedUser) {
               err = VIX_E_OUT_OF_MEMORY;
               goto abort;
            }
         } else {
            user = procInfo->procOwner;
         }
      } else {
         user = "";
      }

      procBufPtr = Str_SafeAsprintf(&procBufSize,
                                    "<proc>"
                                    "%s"              // has <cmd>...</cmd> tags if there is cmd, else "";
                                    "<name>%s</name>"
                                    "<pid>%d</pid>"
#if defined(_WIN32)
                                    "<debugged>%d</debugged>"
#endif
                                    "<user>%s</user>"
                                    "<start>%d</start>"
                                    "</proc>",
                                    cmdNamePtr, name, (int) procInfo->procId,
#if defined(_WIN32)
                                    (int) procInfo->procDebugged,
#endif
                                    user, (int) procInfo->procStartTime);
      if (NULL == procBufPtr) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      if ((destPtr + procBufSize) < endDestPtr) {
         destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr,
                                "%s", procBufPtr);
      } else { // out of space
         Log("%s: proc list results too large, truncating", __FUNCTION__);
         goto abort;
      }
      free(cmdNamePtr);
      cmdNamePtr = NULL;
      free(procBufPtr);
      procBufPtr = NULL;
      free(escapedName);
      escapedName = NULL;
      free(escapedUser);
      escapedUser = NULL;
      free(escapedCmd);
      escapedCmd = NULL;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);
   ProcMgr_FreeProcList(procList);
   free(cmdNamePtr);
   free(procBufPtr);
   free(escapedName);
   free(escapedUser);
   free(escapedCmd);

   *result = resultBuffer;

   // XXX result too large for g_debug()

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return(err);
} // VixToolsListProcesses


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsFreeCachedResult --
 *
 *    Hash table value destroy func.
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
VixToolsFreeCachedResult(gpointer ptr)          // IN
{
   VixToolsCachedListProcessesResult *p = (VixToolsCachedListProcessesResult *) ptr;

   if (NULL != p) {
      free(p->resultBuffer);
#ifdef _WIN32
      free(p->userName);
#endif
      free(p);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListProcCacheCleanup --
 *
 *
 * Return value:
 *    FALSE -- tells glib not to clean up
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
VixToolsListProcCacheCleanup(void *clientData) // IN
{
   int key = (int)(intptr_t)clientData;
   gboolean ret;

   ret = g_hash_table_remove(listProcessesResultsTable, &key);
   g_debug("%s: list proc cache timed out, purged key %d (found? %d)\n",
           __FUNCTION__, key, ret);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListProcessesExGenerateData --
 *
 *    Does the work to generate the results into a string buffer.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    Allocates and creates the result buffer.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsListProcessesExGenerateData(uint32 numPids,          // IN
                                    const uint64 *pids,      // IN
                                    size_t *resultSize,      // OUT
                                    char **resultBuffer)     // OUT
{
   VixError err = VIX_OK;
   ProcMgrProcInfoArray *procList = NULL;
   ProcMgrProcInfo *procInfo;
   DynBuf dynBuffer;
   VixToolsStartedProgramState *spList;
   int numReported = 0;
   int i;
   int j;
   Bool bRet;
   size_t procCount;

   DynBuf_Init(&dynBuffer);

   /*
    * First check the processes we've started via StartProgram, which
    * will find those running and recently deceased.
    */
   VixToolsUpdateStartedProgramList(NULL);
   if (numPids > 0) {
      for (i = 0; i < numPids; i++) {
         spList = startedProcessList;
         while (spList) {
            if (pids[i] == spList->pid) {
               err = VixToolsPrintProcInfoEx(&dynBuffer,
                                             spList->cmdName,
                                             spList->fullCommandLine,
                                             spList->pid,
                                             spList->user,
                                             (int) spList->startTime,
                                             spList->exitCode,
                                             (int) spList->endTime);
               if (VIX_OK != err) {
                  goto abort;
               }
               numReported++;
               break;
            }
            spList = spList->next;
         }
      }
   } else {
      spList = startedProcessList;
      while (spList) {
         err = VixToolsPrintProcInfoEx(&dynBuffer,
                                       spList->cmdName,
                                       spList->fullCommandLine,
                                       spList->pid,
                                       spList->user,
                                       (int) spList->startTime,
                                       spList->exitCode,
                                       (int) spList->endTime);
         if (VIX_OK != err) {
            goto abort;
         }
         spList = spList->next;
      }
   }

   /*
    * If we found data for all requested processes from the startedProcess
    * list, then we're done.
    */
   if (numPids > 0 && (numPids == numReported)) {
      g_debug("%s: found all %d requested pids on the startedProcess list; finished\n",
              __FUNCTION__, numPids);
      goto done;
   }

   /*
    * The startedProcess list didn't give everything we need, so
    * ask the OS.
    *
    * XXX Need a smarter version of ProcMgr_ListProcesses().
    * It should allow a list of pids for optimization, and
    * return an error code so there's no risk of errno/LastError
    * being clobbered.
    */
   procList = ProcMgr_ListProcesses();
   if (NULL == procList) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }


   /*
    * Now look at the running list.  Note that we set endTime
    * and exitCode to dummy values, since we'll be getting results on
    * the Vix side with GetNthProperty, and can have a mix of live and
    * dead processes.
    */
   procCount = ProcMgrProcInfoArray_Count(procList);
   if (numPids > 0) {
      for (i = 0; i < numPids; i++) {
         // ignore it if its on the started list -- we added it above
         if (VixToolsFindStartedProgramState(pids[i])) {
            continue;
         }
         for (j = 0; j < procCount; j++) {
            procInfo = ProcMgrProcInfoArray_AddressOf(procList, j);
            if (pids[i] == procInfo->procId) {
               err = VixToolsPrintProcInfoEx(&dynBuffer,
                                             procInfo->procCmdName,
                                             procInfo->procCmdLine,
                                             procInfo->procId,
                                             (NULL == procInfo->procOwner)
                                             ? "" : procInfo->procOwner,
                                             (int) procInfo->procStartTime,
                                             0, 0);
               if (VIX_OK != err) {
                  goto abort;
               }
            }
         }
      }
   } else {
      for (i = 0; i < procCount; i++) {
         procInfo = ProcMgrProcInfoArray_AddressOf(procList, i);
         // ignore it if its on the started list -- we added it above
         if (VixToolsFindStartedProgramState(procInfo->procId)) {
            continue;
         }
         err = VixToolsPrintProcInfoEx(&dynBuffer,
                                       procInfo->procCmdName,
                                       procInfo->procCmdLine,
                                       procInfo->procId,
                                       (NULL == procInfo->procOwner)
                                       ? "" : procInfo->procOwner,
                                       (int) procInfo->procStartTime,
                                       0, 0);
         if (VIX_OK != err) {
            goto abort;
         }
      }
   }

done:

   // add the final NUL
   bRet = DynBuf_Append(&dynBuffer, "", 1);
   if (!bRet) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   DynBuf_Trim(&dynBuffer);
   *resultSize = DynBuf_GetSize(&dynBuffer);
   *resultBuffer  = DynBuf_Detach(&dynBuffer);

abort:
   DynBuf_Destroy(&dynBuffer);
   ProcMgr_FreeProcList(procList);
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListProcessesEx --
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
VixToolsListProcessesEx(VixCommandRequestHeader *requestMsg, // IN
                        size_t maxBufferSize,                // IN
                        void *eventQueue,                    // IN
                        char **result)                       // OUT
{
   VixError err = VIX_OK;
   char *fullResultBuffer = NULL;
   char *finalResultBuffer = NULL;
   size_t fullResultSize = 0;
   size_t curPacketLen = 0;
   int32 leftToSend = 0;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgListProcessesExRequest *listRequest;
   uint64 *pids = NULL;
   uint32 numPids;
   uint32 key;
   uint32 offset;
   int len;
   VixToolsCachedListProcessesResult *cachedResult = NULL;
   GSource *timer;
#ifdef _WIN32
   Bool bRet;
   wchar_t *userName = NULL;
#endif
   static const char resultHeaderFormatString[] =
                                 "<key>%u</key><totalSize>%d</totalSize>"
                                 "<leftToSend>%d</leftToSend>";
   // room for header plus 3 32-bit ints
   int resultHeaderSize = sizeof(resultHeaderFormatString) + 3 * 10;
   static const char leftHeaderFormatString[] =
                                 "<leftToSend>%d</leftToSend>";
   // room for header plus 1 32-bit ints
   int leftHeaderSize = sizeof(leftHeaderFormatString) + 10;

   ASSERT(maxBufferSize <= GUESTMSG_MAX_IN_SIZE);
   ASSERT(maxBufferSize > resultHeaderSize);

   listRequest = (VixMsgListProcessesExRequest *) requestMsg;

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);

#if defined(__APPLE__)
   /*
    * On MacOS, to fetch info on processes owned by others
    * we need to be root. Even /bin/ps and /bin/top in
    * MacOS have the setuid bit set to allow any user
    * list all processes. For linux & FreeBSD, this API
    * does return info on all processes by all users. So
    * to keep the result consistent on MacOS, we need to
    * stop impersonating user for this API.
    *
    * NOTE: We still do the impersonation before this
    * to authenticate the user as usual.
    */
   VixToolsUnimpersonateUser(userToken);
   impersonatingVMWareUser = FALSE;
#endif

   key = listRequest->key;
   offset = listRequest->offset;

   /*
    * If the request has a key, then go look up the cached results
    * it should point to.
    */
   if (0 != key) {

      // find the cached data
      cachedResult = g_hash_table_lookup(listProcessesResultsTable,
                                         &key);
      if (NULL == cachedResult) {
         g_warning("%s: failed to find cached data with key %d\n",
                   __FUNCTION__, key);
         err = VIX_E_FAIL;
         goto abort;
      }

      // sanity check offset
      if (listRequest->offset > cachedResult->resultBufferLen) {
         /*
          * Since this isn't user-set, assume any problem is in the
          * code and return VIX_E_FAIL
          */
         err = VIX_E_FAIL;
         goto abort;
      }

      // security check -- validate user
#ifdef _WIN32
      bRet = VixToolsGetUserName(&userName);
      if (!bRet) {
         g_warning("%s: VixToolsGetUserName() failed\n", __FUNCTION__);
         err = VIX_E_FAIL;
         goto abort;
      }
      if (0 != wcscmp(userName, cachedResult->userName)) {
         /*
          * Since this isn't user-set, assume any problem is in the
          * code and return VIX_E_FAIL
          */
         g_warning("%s: username mismatch validating cached data (have %S, want %S)\n",
                   __FUNCTION__, userName, cachedResult->userName);
         err = VIX_E_FAIL;
         goto abort;
      }
#else
      if (cachedResult->euid != Id_GetEUid()) {
         /*
          * Since this isn't user-set, assume any problem is in the
          * code and return VIX_E_FAIL
          */
         err = VIX_E_FAIL;
         g_warning("%s: euid mismatch validating cached data (want %d, got %d)\n",
                   __FUNCTION__, (int) cachedResult->euid, (int) Id_GetEUid());
         goto abort;
      }
#endif

   } else {
      /*
       * No key, so this is the initial/only request.  Generate data,
       * cache if necessary.
       */

      numPids = listRequest->numPids;
      if (numPids > 0) {
         pids = (uint64 *)((char *)requestMsg + sizeof(*listRequest));
      }

      err = VixToolsListProcessesExGenerateData(numPids, pids,
                                                &fullResultSize,
                                                &fullResultBuffer);

      /*
       * Check if the result is large enough to require more than one trip.
       * Stuff it in the hash table if so.
       */
      if ((fullResultSize + resultHeaderSize) > maxBufferSize) {
         g_debug("%s: answer requires caching.  have %d bytes\n",
                 __FUNCTION__, (int) (fullResultSize + resultHeaderSize));
         /*
          * Save it off in the hashtable.
          */
         key = listProcessesResultsKey++;
         cachedResult = Util_SafeMalloc(sizeof(*cachedResult));
         cachedResult->resultBufferLen = fullResultSize;
         cachedResult->resultBuffer = fullResultBuffer;
         cachedResult->key = key;
#ifdef _WIN32
         bRet = VixToolsGetUserName(&cachedResult->userName);
         if (!bRet) {
            g_warning("%s: failed to get current userName\n", __FUNCTION__);
            goto abort;
         }
#else
         cachedResult->euid = Id_GetEUid();
#endif

         g_hash_table_replace(listProcessesResultsTable, &cachedResult->key,
                              cachedResult);

         /*
          * Set timer callback to clean this up in case the Vix side
          * never finishes
          */
         timer = g_timeout_source_new(SECONDS_UNTIL_LISTPROC_CACHE_CLEANUP * 1000);
         g_source_set_callback(timer, VixToolsListProcCacheCleanup,
                               (void *)(intptr_t) key, NULL);
         g_source_attach(timer, g_main_loop_get_context(eventQueue));
         g_source_unref(timer);
      }
   }

   /*
    * Now package up the return data.
    */
   if (NULL != cachedResult) {
      int hdrSize;

      /*
       * For the first packet, sent the key and total size and leftToSend.
       * After that, just send leftToSend.
       */
      if (0 == offset) {
         hdrSize = resultHeaderSize;
      } else {
         hdrSize = leftHeaderSize;
      }

      leftToSend = cachedResult->resultBufferLen - offset;

      if (leftToSend > (maxBufferSize - hdrSize)) {
         curPacketLen = maxBufferSize - hdrSize;
      } else {
         curPacketLen = leftToSend;
      }

      leftToSend -= curPacketLen;

      finalResultBuffer = Util_SafeMalloc(curPacketLen + hdrSize + 1);
      if (0 == offset) {

         len = Str_Sprintf(finalResultBuffer, maxBufferSize,
                           resultHeaderFormatString,
                           key, (int) cachedResult->resultBufferLen,
                           leftToSend);
      } else {
         len = Str_Sprintf(finalResultBuffer, maxBufferSize,
                           leftHeaderFormatString,
                           leftToSend);
      }

      memcpy(finalResultBuffer + len,
             cachedResult->resultBuffer + offset, curPacketLen);
      finalResultBuffer[curPacketLen + len] = '\0';

      /*
       * All done, clean it out of the hash table.
       */
      if (0 == leftToSend) {
         g_hash_table_remove(listProcessesResultsTable, &key);
      }

   } else {
      /*
       * In the simple/common case, just return the basic proces info.
       */
      finalResultBuffer = fullResultBuffer;
   }


abort:
#ifdef _WIN32
   free(userName);
#endif
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   *result = finalResultBuffer;

   // XXX result too large for g_debug()

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return(err);
} // VixToolsListProcessesEx


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsPrintProcInfoEx --
 *
 *      Appends a single process entry to the XML-like string starting at
 *      *destPtr.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsPrintProcInfoEx(DynBuf *dstBuffer,             // IN/OUT
                        const char *cmd,               // IN
                        const char *name,              // IN
                        uint64 pid,                    // IN
                        const char *user,              // IN
                        int start,                     // IN
                        int exitCode,                  // IN
                        int exitTime)                  // IN
{
   VixError err;
   char *escapedName = NULL;
   char *escapedCmd = NULL;
   char *escapedUser = NULL;
   char *cmdNamePtr = NULL;
   size_t bytesPrinted;
   char *procInfoEntry;
   Bool success;

   if (NULL != cmd) {
      escapedCmd = VixToolsEscapeXMLString(cmd);
      if (NULL == escapedCmd) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      cmdNamePtr = Str_SafeAsprintf(NULL, "<cmd>%s</cmd>", escapedCmd);
   } else {
      cmdNamePtr = Util_SafeStrdup("");
   }

   escapedName = VixToolsEscapeXMLString(name);
   if (NULL == escapedName) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   escapedUser = VixToolsEscapeXMLString(user);
   if (NULL == escapedUser) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   procInfoEntry = Str_SafeAsprintf(&bytesPrinted,
                                    "<proc>"
                                    "%s"              // has <cmd>...</cmd> tags if there is cmd, else "";
                                    "<name>%s</name>"
                                    "<pid>%"FMT64"d</pid>"
                                    "<user>%s</user>"
                                    "<start>%d</start>"
                                    "<eCode>%d</eCode>"
                                    "<eTime>%d</eTime>"
                                    "</proc>",
                                    cmdNamePtr, escapedName, pid, escapedUser,
                                    start, exitCode, exitTime);
   if (NULL == procInfoEntry) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   success = DynBuf_Append(dstBuffer, procInfoEntry, bytesPrinted);
   free(procInfoEntry);
   if (!success) {
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   err = VIX_OK;

abort:
   free(cmdNamePtr);
   free(escapedName);
   free(escapedUser);
   free(escapedCmd);

   return err;
}


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
#ifdef _WIN32
   DWORD dwErr;
   const VixToolsStartedProgramState *spState;
#else
   int sysErrno;
#endif

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   killProcessRequest = (VixCommandKillProcessRequest *) requestMsg;

   g_debug("%s: User: %s pid: %"FMT64"d\n",
           __FUNCTION__, IMPERSONATED_USERNAME, killProcessRequest->pid);

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

#if defined(__APPLE__)
   /*
    * On OS X, we can only impersonate the effective UID, not real.
    * But the kill(2) syscall looks at real UID.  This means its working
    * as root, and therefore its a massive security hole to leave it
    * as-is.
    *
    * Its unclear if anyone actually cares, so for now, just turn
    * it off.  Its trivial to workaround (RunProgram of 'kill <pid>'.)
    *
    * Two possible fixes if necessary:
    *
    * - sleaze things and rewrite as a RunProgram("kill <pid>").  This
    *   will lose probably error info.
    * - do the kill inside fork(), sending back any error code
    *   on a pipe.
    */
   err = VIX_E_NOT_SUPPORTED;
   goto abort;
#endif

   if (!ProcMgr_KillByPid(killProcessRequest->pid)) {
      /*
       * Save off the error code so any debug statements added later
       * (or when debugging something else) doesn't change the error code.
       */
#ifdef _WIN32
      dwErr = GetLastError();
#else
      sysErrno = errno;
#endif


#ifdef _WIN32
      /*
       * If we know it's already gone, just say so.  If this gets called
       * on a process we started but is still on the 'exited' list,
       * then Windows returns an ACCESS_ERROR.  So rewrite it.
       */
       spState = VixToolsFindStartedProgramState(killProcessRequest->pid);
       if ((NULL != spState) && !spState->isRunning) {
         err = VIX_E_NO_SUCH_PROCESS;
         goto abort;
      }
#endif

      /*
       * Vix_TranslateSystemError() assumes that any perm error
       * is file related, and returns VIX_E_FILE_ACCESS_ERROR.  Bogus
       * for this case, so rewrite it here.
       */
#ifdef _WIN32
      if (ERROR_ACCESS_DENIED == dwErr) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
#else
      if ((EPERM == sysErrno) || (EACCES == sysErrno)) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }
#endif


#ifdef _WIN32
      /*
       * Windows doesn't give us an obvious error for a non-existent
       * PID.  But we can make a pretty good guess that it returned
       * ERROR_INVALID_PARAMETER because the PID was bad, so rewrite
       * that error if we see it.
       */
      if (ERROR_INVALID_PARAMETER == dwErr) {
         err = VIX_E_NO_SUCH_PROCESS;
         goto abort;
      }
#endif

#ifdef _WIN32
      err = Vix_TranslateSystemError(dwErr);
#else
      err = Vix_TranslateSystemError(sysErrno);
#endif

      goto abort;
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

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
   const char *dirPathName = NULL;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   Bool createParentDirectories = TRUE;
   VMAutomationRequestParser parser;
   int dirPathLen;

   if (VIX_COMMAND_CREATE_DIRECTORY == requestMsg->opCode) {
      VixMsgCreateFileRequest *dirRequest = NULL;

      err = VMAutomationRequestParserInit(&parser,
                                          requestMsg, sizeof *dirRequest);
      if (VIX_OK != err) {
         goto abort;
      }

      dirRequest = (VixMsgCreateFileRequest *) requestMsg;
      dirPathLen = dirRequest->guestPathNameLength;
   } else if (VIX_COMMAND_CREATE_DIRECTORY_EX == requestMsg->opCode) {
      VixMsgCreateFileRequestEx *dirRequest = NULL;

      err = VMAutomationRequestParserInit(&parser,
                                          requestMsg, sizeof *dirRequest);
      if (VIX_OK != err) {
         goto abort;
      }

      dirRequest = (VixMsgCreateFileRequestEx *) requestMsg;
      dirPathLen = dirRequest->guestPathNameLength;
      createParentDirectories = dirRequest->createParentDirectories;
   } else {
      ASSERT(0);
      g_warning("%s: Invalid request with opcode %d received\n ",
                __FUNCTION__, requestMsg->opCode);
      err = VIX_E_FAIL;
      goto abort;
   }

   err = VMAutomationRequestParserGetString(&parser,
                                            dirPathLen, &dirPathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *dirPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s dirPathName: %s createParent: %d\n",
           __FUNCTION__, IMPERSONATED_USERNAME,
           dirPathName, (int) createParentDirectories);

   if (File_Exists(dirPathName)) {
      err = VIX_E_FILE_ALREADY_EXISTS;
      goto abort;
   }

   if (createParentDirectories) {
      if (!(File_CreateDirectoryHierarchyEx(dirPathName, 0700, NULL))) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   } else {
      if (!(File_CreateDirectoryEx(dirPathName, 0700))) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   }

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

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
   const char *dirPathName = NULL;
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
   VMAutomationRequestParser parser;
   int dirPathLen;
   Bool escapeStrs;

   legacyListRequest = (VixMsgSimpleFileRequest *) requestMsg;
   if (legacyListRequest->fileOptions & VIX_LIST_DIRECTORY_USE_OFFSET) {
      /*
       * Support updated ListDirectory format.
       */
      err = VMAutomationRequestParserInit(&parser,
                                          requestMsg, sizeof *listRequest);
      if (VIX_OK != err) {
         goto abort;
      }

      listRequest = (VixMsgListDirectoryRequest *) requestMsg;
      offset = listRequest->offset;

      dirPathLen = listRequest->guestPathNameLength;
      isLegacyFormat = FALSE;
   } else {
      /*
       * Support legacy ListDirectory format.
       */
      err = VMAutomationRequestParserInit(&parser,
                                        requestMsg, sizeof *legacyListRequest);
      if (VIX_OK != err) {
         goto abort;
      }

      dirPathLen = legacyListRequest->guestPathNameLength;
      isLegacyFormat = TRUE;
   }

   err = VMAutomationRequestParserGetString(&parser,
                                            dirPathLen, &dirPathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *dirPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s path: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME, dirPathName);

   escapeStrs = (requestMsg->requestFlags &
                 VIX_REQUESTMSG_ESCAPE_XML_DATA) != 0;

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
   if (escapeStrs) {
      resultBufferSize += strlen(VIX_XML_ESCAPED_TAG);
   }
   lastGoodResultBufferSize = resultBufferSize;
   ASSERT_NOT_IMPLEMENTED(lastGoodResultBufferSize < maxBufferSize);
   formatStringLength = strlen(fileInfoFormatString);

   for (fileNum = offset; fileNum < numFiles; fileNum++) {
      currentFileName = fileNameList[fileNum];

      resultBufferSize += formatStringLength;
      resultBufferSize += VixToolsXMLStringEscapedLen(currentFileName,
                                                      escapeStrs);
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

   if (escapeStrs) {
      destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr, "%s",
                             VIX_XML_ESCAPED_TAG);
   }

   for (fileNum = offset; fileNum < lastGoodNumFiles; fileNum++) {
      /* File_ListDirectory never returns "." or ".." */
      char *pathName;

      currentFileName = fileNameList[fileNum];

      pathName = Str_SafeAsprintf(NULL, "%s%s%s", dirPathName, DIRSEPS,
                                  currentFileName);

      VixToolsPrintFileInfo(pathName, currentFileName, escapeStrs, &destPtr,
                            endDestPtr);

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

   // XXX result too large for g_debug()

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsListDirectory


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListFiles --
 *
 *    This function is called to implement ListFilesInGuest VI Guest operation.
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
VixToolsListFiles(VixCommandRequestHeader *requestMsg,    // IN
                  size_t maxBufferSize,                   // IN
                  char **result)                          // OUT
{
   VixError err = VIX_OK;
   const char *dirPathName = NULL;
   char *fileList = NULL;
   char **fileNameList = NULL;
   size_t resultBufferSize = 0;
   size_t lastGoodResultBufferSize = 0;
   int numFiles = 0;
   int fileNum;
   char *currentFileName;
   char *destPtr;
   char *endDestPtr;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   VixMsgListFilesRequest *listRequest = NULL;
   Bool truncated = FALSE;
   uint64 offset = 0;
   Bool listingSingleFile = FALSE;
   const char *pattern = NULL;
   int index = 0;
   int maxResults = 0;
   int count = 0;
   int remaining = 0;
   int numResults;
   GRegex *regex = NULL;
   GError *gErr = NULL;
   char *pathName;
   VMAutomationRequestParser parser;

   ASSERT(NULL != requestMsg);

   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *listRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   listRequest = (VixMsgListFilesRequest *) requestMsg;
   offset = listRequest->offset;
   index = listRequest->index;
   maxResults = listRequest->maxResults;

   err = VMAutomationRequestParserGetString(&parser,
                                            listRequest->guestPathNameLength,
                                            &dirPathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (listRequest->patternLength > 0) {
      err = VMAutomationRequestParserGetString(&parser,
                                               listRequest->patternLength,
                                               &pattern);
      if (VIX_OK != err) {
         goto abort;
      }

      g_debug("%s: pattern length is %d, value is '%s'\n",
              __FUNCTION__, listRequest->patternLength, pattern);
   }

   if (0 == *dirPathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s listing files in '%s' with pattern '%s' "
           "index %d, maxResults %d (offset %d)\n",
           __FUNCTION__, IMPERSONATED_USERNAME, dirPathName,
           (NULL != pattern) ? pattern : "",
           index, maxResults, (int) offset);

   if (pattern) {
      regex = g_regex_new(pattern, 0, 0, &gErr);
      if (!regex) {
         g_warning("%s: bad regex pattern '%s' (%s);"
                   "failing with INVALID_ARG\n",
                   __FUNCTION__, pattern, gErr ? gErr->message : "");
         err = VIX_E_INVALID_ARG;
         goto abort;
      }
   }

   /*
    * First check for symlink -- File_IsDirectory() will lie
    * if its a symlink to a directory.
    */
   if (!File_IsSymLink(dirPathName) && File_IsDirectory(dirPathName)) {
      numFiles = File_ListDirectory(dirPathName, &fileNameList);
      if (numFiles < 0) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
      /*
       * File_ListDirectory() doesn't return '.' and '..', but we want them,
       * so add '.' and '..' to the list.  Place them in front since that's
       * a more normal location.
       */
      numFiles += 2;
      {
         char **newFileNameList = NULL;

         newFileNameList = Util_SafeMalloc(numFiles * sizeof(char *));
         newFileNameList[0] = Unicode_Alloc(".", STRING_ENCODING_UTF8);
         newFileNameList[1] = Unicode_Alloc("..", STRING_ENCODING_UTF8);
         memcpy(newFileNameList + 2, fileNameList, (numFiles - 2) * sizeof(char *));
         free(fileNameList);
         fileNameList = newFileNameList;
      }
   } else {
      if (File_Exists(dirPathName)) {
         listingSingleFile = TRUE;
         numFiles = 1;
         fileNameList = Util_SafeMalloc(sizeof(char *));
         fileNameList[0] = Util_SafeStrdup(dirPathName);
      } else {
         /*
          * We don't know what they intended to list, but we'll
          * assume file since that gives a fairly sane error.
          */
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   }

   /*
    * Calculate the size of the result buffer and keep track of the
    * max number of entries we can store.  Also compute the number
    * we won't be returning (anything > maxResults).
    */
   resultBufferSize = 3; // truncation bool + space + '\0'
   // space for the 'remaining' tag up front
   resultBufferSize += strlen(listFilesRemainingFormatString) + 10;
   lastGoodResultBufferSize = resultBufferSize;
   ASSERT_NOT_IMPLEMENTED(lastGoodResultBufferSize < maxBufferSize);

   for (fileNum = offset + index;
        fileNum < numFiles;
        fileNum++) {

      currentFileName = fileNameList[fileNum];

      if (regex) {
         if (!g_regex_match(regex, currentFileName, 0, NULL)) {
            continue;
         }
      }

      if (count < maxResults) {
         count++;
      } else {
         remaining++;
         continue;   // stop computing buffersize
      }

      if (listingSingleFile) {
         resultBufferSize += VixToolsGetFileExtendedInfoLength(currentFileName,
                                                               currentFileName);
      } else {
         pathName = Str_SafeAsprintf(NULL, "%s%s%s", dirPathName, DIRSEPS,
                                     currentFileName);
         resultBufferSize += VixToolsGetFileExtendedInfoLength(pathName,
                                                               currentFileName);
         free(pathName);
      }

      if (resultBufferSize < maxBufferSize) {
         lastGoodResultBufferSize = resultBufferSize;
      } else {
         truncated = TRUE;
         break;
      }
   }
   resultBufferSize = lastGoodResultBufferSize;
   numResults = count;

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
   if ((destPtr + 2) < endDestPtr) {
      *destPtr++ = truncated ? '1' : '0';
      *destPtr++ = ' ';
   } else {
      ASSERT(0);
      err = VIX_E_OUT_OF_MEMORY;
      goto abort;
   }

   destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr,
                          listFilesRemainingFormatString, remaining);


   for (fileNum = offset + index, count = 0;
        count < numResults;
        fileNum++) {

      currentFileName = fileNameList[fileNum];

      if (regex) {
         if (!g_regex_match(regex, currentFileName, 0, NULL)) {
            continue;
         }
      }

      if (listingSingleFile) {
         pathName = Util_SafeStrdup(currentFileName);
      } else {
         pathName = Str_SafeAsprintf(NULL, "%s%s%s", dirPathName, DIRSEPS,
                                     currentFileName);
      }

      VixToolsPrintFileExtendedInfo(pathName, currentFileName,
                                    &destPtr, endDestPtr);

      free(pathName);
      count++;
   } // for (fileNum = 0; fileNum < lastGoodNumFiles; fileNum++)
   *destPtr = '\0';

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   if (NULL != regex) {
      g_regex_unref(regex);
   }

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

   // XXX result too large for g_debug()

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsListFiles


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetFileExtendedInfoLength --
 *
 *    This function calculates the total number of bytes required to hold
 *    the extended info about the specified file.
 *
 * Return value:
 *    Size of extended info buffer.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
VixToolsGetFileExtendedInfoLength(const char *filePathName,   // IN
                                  const char *fileName)       // IN
{
   int fileExtendedInfoBufferSize = 0;

   ASSERT(NULL != filePathName);
   ASSERT(NULL != fileName);

#ifdef _WIN32
   fileExtendedInfoBufferSize = strlen(fileExtendedInfoWindowsFormatString);
#else
   fileExtendedInfoBufferSize = strlen(fileExtendedInfoLinuxFormatString);
#endif

   fileExtendedInfoBufferSize += 2; // DIRSEPC chars
   fileExtendedInfoBufferSize += 10 + 20 + (20 * 2); // properties + size + times
#ifdef _WIN32
   fileExtendedInfoBufferSize += 20;                // createTime
#else
   fileExtendedInfoBufferSize += 10 * 3;            // uid, gid, perms
#endif

#if defined(__linux__) || defined(sun) || defined(__FreeBSD__)
   if (File_IsSymLink(filePathName)) {
      char *symlinkTarget;
      symlinkTarget = Posix_ReadLink(filePathName);
      if (NULL != symlinkTarget) {
         fileExtendedInfoBufferSize +=
            VixToolsXMLStringEscapedLen(symlinkTarget, TRUE);
      }
      free(symlinkTarget);
   }
#endif

   fileExtendedInfoBufferSize += VixToolsXMLStringEscapedLen(fileName, TRUE);

   return fileExtendedInfoBufferSize;
}


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
   const char *filePathName;
   VixMsgSimpleFileRequest *simpleFileReq;
   VMAutomationRequestParser parser;

   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *simpleFileReq);
   if (VIX_OK != err) {
      goto abort;
   }

   simpleFileReq = (VixMsgSimpleFileRequest *)requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            simpleFileReq->guestPathNameLength,
                                            &filePathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (0 == *filePathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s path: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME, filePathName);

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
   VixToolsPrintFileInfo(filePathName, "", FALSE, &destPtr, resultBuffer + resultBufferSize);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   if (NULL == resultBuffer) {
      resultBuffer = Util_SafeStrdup("");
   }
   *result = resultBuffer;

   guest_debug("%s: returning '%s'\n", __FUNCTION__, resultBuffer);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsGetFileInfo


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsSetFileAttributes --
 *
 *    Set the file attributes for a specified file.
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
VixToolsSetFileAttributes(VixCommandRequestHeader *requestMsg)    // IN
{
   VixError err = VIX_OK;
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   const char *filePathName = NULL;
   VixMsgSetGuestFileAttributesRequest *setGuestFileAttributesRequest = NULL;
   struct timespec timeBuf;
   Bool success = FALSE;
   int64 createTime;
   int64 accessTime;
   int64 modificationTime;
   VMAutomationRequestParser parser;
   int64 tempTime;
   Bool timeAttributeSpecified = FALSE;
   Bool windowsAttributeSpecified = FALSE;
   int32 fileAttributeOptions = 0;

#ifdef _WIN32
   DWORD fileAttr = 0;
#else
   int ownerId = 0;
   int groupId = 0;
   struct stat statbuf;
#endif

   ASSERT(NULL != requestMsg);

   /*
    * Parse the argument
    */
   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg,
                                       sizeof *setGuestFileAttributesRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   setGuestFileAttributesRequest =
               (VixMsgSetGuestFileAttributesRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                            setGuestFileAttributesRequest->guestPathNameLength,
                                            &filePathName);
   if (VIX_OK != err) {
      goto abort;
   }

   if ('\0' == *filePathName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   fileAttributeOptions = setGuestFileAttributesRequest->fileOptions;

   if ((fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_HIDDEN) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_READONLY)) {
      windowsAttributeSpecified = TRUE;
   }

   if ((fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_ACCESS_DATE) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_MODIFY_DATE)) {
      timeAttributeSpecified = TRUE;
   }

#if defined(_WIN32)
   if ((fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_OWNERID) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_GROUPID) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_PERMISSIONS)) {
      g_warning("%s: Invalid attributes received for Windows Guest\n",
                __FUNCTION__);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
#else
   if (windowsAttributeSpecified) {
      g_warning("%s: Invalid attributes received for Posix Guest\n",
                __FUNCTION__);
      err = VIX_E_INVALID_ARG;
      goto abort;
   }
#endif

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s path: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME, filePathName);

   if (!(File_Exists(filePathName))) {
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   if (timeAttributeSpecified) {
      success = File_GetTimes(filePathName,
                              &createTime,
                              &accessTime,
                              &modificationTime,
                              &tempTime);

      if (!success) {
         g_warning("%s: Failed to get the times.\n", __FUNCTION__);
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }

      /*
       * User specifies the time in Unix Time Format. File_SetTimes()
       * accepts times in Windows NT Format. We should convert the time
       * from Unix Format to Windows NT Format.
       */

      if (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_ACCESS_DATE ) {
         timeBuf.tv_sec  = setGuestFileAttributesRequest->accessTime;
         timeBuf.tv_nsec = 0;
         accessTime      = TimeUtil_UnixTimeToNtTime(timeBuf);
      }

      if (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_MODIFY_DATE) {
         timeBuf.tv_sec    = setGuestFileAttributesRequest->modificationTime;
         timeBuf.tv_nsec   = 0;
         modificationTime  = TimeUtil_UnixTimeToNtTime(timeBuf);
      }

      success = File_SetTimes(filePathName,
                              createTime,
                              accessTime,
                              modificationTime,
                              modificationTime);
      if (!success) {
         g_warning("%s: Failed to set the times.\n", __FUNCTION__);
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   }
#if defined(_WIN32)
   if (windowsAttributeSpecified) {
      fileAttr = Win32U_GetFileAttributes(filePathName);

      if (fileAttr != INVALID_FILE_ATTRIBUTES) {
         if (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_HIDDEN) {
            if (setGuestFileAttributesRequest->hidden) {
               fileAttr |= FILE_ATTRIBUTE_HIDDEN;
            } else {
               fileAttr &= (~FILE_ATTRIBUTE_HIDDEN);
            }
         }

         if (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_READONLY) {
            if (setGuestFileAttributesRequest->readOnly) {
               fileAttr |= FILE_ATTRIBUTE_READONLY;
            } else {
               fileAttr &= (~FILE_ATTRIBUTE_READONLY);
            }
         }

         if (!Win32U_SetFileAttributes(filePathName, fileAttr)) {
            err = FoundryToolsDaemon_TranslateSystemErr();
            g_warning("%s: Failed to set the file attributes\n", __FUNCTION__);
            goto abort;
         }
      } else {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("%s: Failed to get the file attributes\n", __FUNCTION__);
         goto abort;
      }
   }
#else
   if ((fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_OWNERID) ||
       (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_GROUPID)) {

      if (-1 != Posix_Stat(filePathName, &statbuf)) {
         ownerId = statbuf.st_uid;
         groupId = statbuf.st_gid;
      } else {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("%s: Posix_Stat(%s) failed with %d\n",
                   __FUNCTION__, filePathName, errno);
         goto abort;
      }

      if (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_OWNERID) {
         ownerId = setGuestFileAttributesRequest->ownerId;
      }

      if (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_GROUPID) {
         groupId = setGuestFileAttributesRequest->groupId;
      }

      if (Posix_Chown(filePathName, ownerId, groupId)) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("%s: Failed to set the owner/group Id\n", __FUNCTION__);
         goto abort;
      }
   }

   /*
    * NOTE: Setting ownership clears SUID and SGID bits, therefore set the
    * file permissions after setting ownership.
    */
   if (fileAttributeOptions & VIX_FILE_ATTRIBUTE_SET_UNIX_PERMISSIONS) {
      success = File_SetFilePermissions(filePathName,
                                        setGuestFileAttributesRequest->permissions);
      if (!success) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("%s: Failed to set the file permissions\n", __FUNCTION__);
         goto abort;
      }
   }
#endif

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
} // VixToolsSetGuestFileAttributes


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
VixToolsPrintFileInfo(const char *filePathName,     // IN
                      char *fileName,               // IN
                      Bool escapeStrs,              // IN
                      char **destPtr,               // IN/OUT
                      char *endDestPtr)             // IN
{
   int64 fileSize = 0;
   int64 modTime;
   int32 fileProperties = 0;
   char *escapedFileName = NULL;

   modTime = File_GetModTime(filePathName);
   if (File_IsDirectory(filePathName)) {
      fileProperties |= VIX_FILE_ATTRIBUTES_DIRECTORY;
   } else {
      if (File_IsSymLink(filePathName)) {
         fileProperties |= VIX_FILE_ATTRIBUTES_SYMLINK;
      }
      if (File_IsFile(filePathName)) {
         fileSize = File_GetSize(filePathName);
      }
   }

   if (escapeStrs) {
      fileName = escapedFileName = VixToolsEscapeXMLString(fileName);
      ASSERT_MEM_ALLOC(NULL != escapedFileName);
   }

   *destPtr += Str_Sprintf(*destPtr,
                           endDestPtr - *destPtr,
                           fileInfoFormatString,
                           fileName,
                           fileProperties,
                           fileSize,
                           modTime);
   free(escapedFileName);
} // VixToolsPrintFileInfo


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsPrintFileExtendedInfo --
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
VixToolsPrintFileExtendedInfo(const char *filePathName,     // IN
                              const char *fileName,         // IN
                              char **destPtr,               // IN/OUT
                              char *endDestPtr)             // IN
{
   int64 fileSize = 0;
   VmTimeType modTime = 0;
   VmTimeType accessTime = 0;
   int32 fileProperties = 0;
#ifdef _WIN32
   DWORD fileAttr = 0;
   Bool hidden = FALSE;
   Bool readOnly = FALSE;
   VmTimeType createTime = 0;
#else
   int permissions = 0;
   int ownerId = 0;
   int groupId = 0;
   char *symlinkTarget = NULL;
   char *tmp;
#endif
   struct stat statbuf;
   char *escapedFileName = NULL;

   /*
    * First check for symlink -- File_IsDirectory() will lie
    * if its a symlink to a directory.
    */
   if (File_IsSymLink(filePathName)) {
      fileProperties |= VIX_FILE_ATTRIBUTES_SYMLINK;
   } else if (File_IsDirectory(filePathName)) {
      fileProperties |= VIX_FILE_ATTRIBUTES_DIRECTORY;
   } else if (File_IsFile(filePathName)) {
      fileSize = File_GetSize(filePathName);
   }

#if !defined(_WIN32)
   /*
    * If the file is a symlink, figure out where it points.
    */
   if (fileProperties & VIX_FILE_ATTRIBUTES_SYMLINK) {
      symlinkTarget = Posix_ReadLink(filePathName);
   }

   /*
    * Have a nice empty value if it's not a link or there's some error
    * reading the link.
    */
   if (NULL == symlinkTarget) {
      symlinkTarget = Util_SafeStrdup("");
   }

   tmp = VixToolsEscapeXMLString(symlinkTarget);
   ASSERT_MEM_ALLOC(NULL != tmp);
   free(symlinkTarget);
   symlinkTarget = tmp;
#endif

#ifdef _WIN32
   fileAttr = Win32U_GetFileAttributes(filePathName);
   if (fileAttr != INVALID_FILE_ATTRIBUTES) {
      if (fileAttr & FILE_ATTRIBUTE_HIDDEN) {
         fileProperties |= VIX_FILE_ATTRIBUTES_HIDDEN;
      }
      if (fileAttr & FILE_ATTRIBUTE_READONLY) {
         fileProperties |= VIX_FILE_ATTRIBUTES_READONLY;
      }
   }
#endif

   if (Posix_Stat(filePathName, &statbuf) != -1) {
#if !defined(_WIN32)
      ownerId = statbuf.st_uid;
      groupId = statbuf.st_gid;
      permissions = statbuf.st_mode;
#endif
      /*
       * We want create time.  ctime is the inode change time for Linux,
       * so we can't report anything.
       */
#ifdef _WIN32
      createTime = statbuf.st_ctime;
#endif
      modTime = statbuf.st_mtime;
      accessTime = statbuf.st_atime;
   } else {
      g_warning("%s: Posix_Stat(%s) failed with %d\n",
                __FUNCTION__, filePathName, errno);
   }

   escapedFileName = VixToolsEscapeXMLString(fileName);
   ASSERT_MEM_ALLOC(NULL != escapedFileName);

#ifdef _WIN32
   *destPtr += Str_Sprintf(*destPtr,
                           endDestPtr - *destPtr,
                           fileExtendedInfoWindowsFormatString,
                           escapedFileName,
                           fileProperties,
                           fileSize,
                           modTime,
                           createTime,
                           accessTime,
                           hidden,
                           readOnly);
#else
   *destPtr += Str_Sprintf(*destPtr,
                           endDestPtr - *destPtr,
                           fileExtendedInfoLinuxFormatString,
                           escapedFileName,
                           fileProperties,
                           fileSize,
                           modTime,
                           accessTime,
                           ownerId,
                           groupId,
                           permissions,
                           symlinkTarget);
   free(symlinkTarget);
#endif
   free(escapedFileName);
} // VixToolsPrintFileExtendedInfo


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsPrintFileExtendedInfoEx --
 *
 *    Given a specified file, this function returns a properly XML
 *    formatted string representing the extended information of the file.
 *
 * Return value:
 *    char * - Dynamically allocated string that holds the extended info
 *    about the specified file. It is the responsibility of the caller
 *    to free the memory.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static char *
VixToolsPrintFileExtendedInfoEx(const char *filePathName,          // IN
                                const char *fileName)              // IN
{
   int resultBufferSize;
   char *destPtr = NULL;
   char *endDestPtr = NULL;
   char *resultBuffer = NULL;

   resultBufferSize = VixToolsGetFileExtendedInfoLength(filePathName,
                                                        fileName);
   resultBuffer = Util_SafeMalloc(resultBufferSize);
   destPtr = resultBuffer;
   endDestPtr = resultBuffer + resultBufferSize;

   VixToolsPrintFileExtendedInfo(filePathName, filePathName, &destPtr,
                                 endDestPtr);

   *destPtr = '\0';
   return resultBuffer;
}


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

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
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
   const char *propertiesString = NULL;
   const char *script = NULL;
   const char *interpreterName = NULL;
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
   ProcMgr_ProcArgs procArgs;
#if defined(_WIN32)
   Bool forcedRoot = FALSE;
   wchar_t *envBlock = NULL;
#endif
   GSource *timer;
   VMAutomationRequestParser parser;

   err = VMAutomationRequestParserInit(&parser,
                                       requestMsg, sizeof *scriptRequest);
   if (VIX_OK != err) {
      goto abort;
   }

   scriptRequest = (VixMsgRunScriptRequest *) requestMsg;

   err = VMAutomationRequestParserGetString(&parser,
                                          scriptRequest->interpreterNameLength,
                                            &interpreterName);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VMAutomationRequestParserGetString(&parser,
                                            scriptRequest->propertiesLength,
                                            &propertiesString);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VMAutomationRequestParserGetString(&parser,
                                            scriptRequest->scriptLength,
                                            &script);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);


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
       * Just fall through to use the old fashioned File_GetSafeRandomTmpDir().
       */

      err = VIX_OK;
   }
#endif

   if (NULL == tempDirPath) {
      tempDirPath = File_GetSafeRandomTmpDir(TRUE);

      if (NULL == tempDirPath) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }
   }
   for (var = 0; var < MAX_INT32; var++) {
      free(tempScriptFilePath);
      tempScriptFilePath = Str_SafeAsprintf(NULL,
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
                     | O_RDWR,
                      0600);
      if (fd >= 0) {
         break;
      }
#if defined(_WIN32)
      if ((errno == EACCES) && (File_Exists(tempScriptFilePath))) {
         /*
          * On windows, Posix_Open() fails with EACCES if there is any
          * permissions check failure while creating the file. Also, EACCES is
          * returned if a directory already exists with the same name. In such
          * case, we need to check if a file already exists and ignore
          * EACCES error.
          */
         continue;
      }
#endif
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
      /*
       * We use Posix variant function i.e. Posix_Open to create a
       * temporary file. If Posix_Open() fails, then proper error is
       * stuffed in errno variable. So, use Vix_TranslateErrno()
       * to translate the errno to a proper foundry error.
       */
      err = Vix_TranslateErrno(errno);
      g_warning("%s: Unable to create a temporary file, errno is %d.\n",
                __FUNCTION__, errno);
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
      err = Vix_TranslateErrno(errno);
      g_warning("%s: Unable to write the script to the temporary file, errno is %d.\n",
                __FUNCTION__, errno);
      if (close(fd) < 0) {
         g_warning("%s: Unable to close a file, errno is %d\n",
                   __FUNCTION__, errno);
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
      err = Vix_TranslateErrno(errno);
      g_warning("%s: Unable to close a file, errno is %d\n",
                __FUNCTION__, errno);
      goto abort;
   }

   if ((NULL != interpreterName) && (*interpreterName)) {
      fullCommandLine = Str_SafeAsprintf(NULL, // resulting string length
                                     "\"%s\" %s \"%s\"",
                                     interpreterName,
                                     interpreterFlags,
                                     tempScriptFilePath);
   } else {
      fullCommandLine = Str_SafeAsprintf(NULL,  // resulting string length
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

   memset(&procArgs, 0, sizeof procArgs);
#if defined(_WIN32)
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
      /*
       * If we are impersonating a user then use the user's environment
       * block. That way the user-specific environment variables will
       * be available to the application (such as the user's TEMP
       * directory instead of the system-wide one).
       */
      err = VixToolsGetEnvBlock(userToken, &envBlock);
      if (VIX_OK != err) {
         goto abort;
      }

      forcedRoot = Impersonate_ForceRoot();
   }
   procArgs.hToken = (PROCESS_CREATOR_USER_TOKEN == userToken) ? NULL : userToken;
   procArgs.bInheritHandles = TRUE;
   procArgs.dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
   procArgs.lpEnvironment = envBlock;
#else
   procArgs.envp = VixToolsEnvironmentTableToEnvp(userEnvironmentTable);
#endif

   asyncState->procState = ProcMgr_ExecAsync(fullCommandLine, &procArgs);

#if defined(_WIN32)
   if (forcedRoot) {
      Impersonate_UnforceRoot();
   }
#else
   VixToolsFreeEnvp(procArgs.envp);
   DEBUG_ONLY(procArgs.envp = NULL;)
#endif

   if (NULL == asyncState->procState) {
      err = VIX_E_PROGRAM_NOT_STARTED;
      goto abort;
   }

   pid = (int64) ProcMgr_GetPid(asyncState->procState);

   asyncState->eventQueue = eventQueue;
   timer = g_timeout_source_new(SECONDS_BETWEEN_POLL_TEST_FINISHED * 1000);
   g_source_set_callback(timer, VixToolsMonitorAsyncProc, asyncState, NULL);
   g_source_attach(timer, g_main_loop_get_context(eventQueue));
   g_source_unref(timer);

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

#ifdef _WIN32
   if (NULL != envBlock) {
      VixToolsDestroyEnvironmentBlock(envBlock);
   }
#endif

   free(fullCommandLine);
   free(tempDirPath);
   free(tempScriptFilePath);

   Str_Sprintf(resultBuffer, sizeof(resultBuffer), "%"FMT64"d", pid);
   *result = resultBuffer;

   guest_debug("%s: returning '%s'\n", __FUNCTION__, resultBuffer);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

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
                        Bool loadUserProfile,                  // IN
                        void **userToken)                      // OUT
{
   VixError err = VIX_OK;
   char *credentialField;
   int credentialType;

   credentialField = ((char *) requestMsg)
                           + requestMsg->commonHeader.headerLength
                           + requestMsg->commonHeader.bodyLength;

   credentialType = requestMsg->userCredentialType;

   switch (credentialType) {
   case VIX_USER_CREDENTIAL_TICKETED_SESSION:
   {
      VixCommandTicketedSession *commandTicketedSession = (VixCommandTicketedSession *) credentialField;
      size_t ticketLength = commandTicketedSession->ticketLength;

      credentialField += sizeof(VixCommandTicketedSession);

      if (ticketLength != strlen(credentialField)) {
         g_warning("%s: Ticket Length Does Not Match Expected\n", __FUNCTION__);
         err = VIX_E_INVALID_MESSAGE_BODY;
         goto done;
      }

      err = VixToolsImpersonateUserImplEx(NULL,
                                          credentialType,
                                          credentialField,
                                          loadUserProfile,
                                          userToken);
      break;
   }
   case VIX_USER_CREDENTIAL_ROOT:
   {
      if ((requestMsg->requestFlags & VIX_REQUESTMSG_HAS_HASHED_SHARED_SECRET) &&
          !VixToolsCheckIfAuthenticationTypeEnabled(gConfDictRef,
                                            VIX_TOOLS_CONFIG_AUTHTYPE_AGENTS)) {
          /*
           * Don't accept hashed shared secret if disabled.
           */
          g_message("%s: Requested authentication type has been disabled.\n",
                    __FUNCTION__);
          err = VIX_E_GUEST_AUTHTYPE_DISABLED;
          goto done;
      }
   }
   // fall through

   case VIX_USER_CREDENTIAL_CONSOLE_USER:
      err = VixToolsImpersonateUserImplEx(NULL,
                                          credentialType,
                                          NULL,
                                          loadUserProfile,
                                          userToken);
      break;
   case VIX_USER_CREDENTIAL_NAME_PASSWORD:
   case VIX_USER_CREDENTIAL_NAME_PASSWORD_OBFUSCATED:
   case VIX_USER_CREDENTIAL_NAMED_INTERACTIVE_USER:
   {
      VixCommandNamePassword *namePasswordStruct =
         (VixCommandNamePassword *) credentialField;
      credentialField += sizeof(*namePasswordStruct);

      err = VixToolsImpersonateUserImplEx(NULL,
                                          credentialType,
                                          credentialField,
                                          loadUserProfile,
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
      break;
   }
#if SUPPORT_VGAUTH
   case VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN:
   {
      VixCommandSAMLToken *samlStruct =
         (VixCommandSAMLToken *) credentialField;
      credentialField += sizeof(*samlStruct);

      err = VixToolsImpersonateUserImplEx(NULL,
                                          credentialType,
                                          credentialField,
                                          loadUserProfile,
                                          userToken);
      break;
   }
#endif
   case VIX_USER_CREDENTIAL_SSPI:
      /*
       * SSPI currently only supported in ticketed sessions
       */
   default:
      g_warning("%s: Unsupported credentialType = %d\n",
                __FUNCTION__, credentialType);
      err = VIX_E_NOT_SUPPORTED;
   }

done:
   if (err != VIX_OK) {
      g_warning("%s: impersonation failed (%"FMT64"d)\n",
                  __FUNCTION__, err);
   } else {
      g_debug("%s: successfully impersonated user %s\n", __FUNCTION__,
              IMPERSONATED_USERNAME);
   }

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
                                                  TRUE,
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
                              Bool loadUserProfile,                  // IN
                              void **userToken)                      // OUT
{
   VixError err = VIX_E_INVALID_LOGIN_CREDENTIALS;

   if (NULL == userToken) {
      g_warning("%s: Invalid userToken pointer\n", __FUNCTION__);
      return VIX_E_FAIL;
   }

   *userToken = NULL;

///////////////////////////////////////////////////////////////////////
// NOTE: The following lines need to be uncommented to disable either
// FreeBSD and/or MacOS support for VMODL Guest Operations completely.
//#if defined(__FreeBSD__)
//   return VIX_E_NOT_SUPPORTED;
//#endif
//#if defined(__APPLE__)
//   return VIX_E_NOT_SUPPORTED;
//#endif
///////////////////////////////////////////////////////////////////////
   {
      AuthToken authToken;
      char *unobfuscatedUserName = NULL;
      char *unobfuscatedPassword = NULL;
      Bool success = FALSE;

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
         *userToken = PROCESS_CREATOR_USER_TOKEN;

         gImpersonatedUsername = Util_SafeStrdup("_ROOT_");
         err = VIX_OK;
         goto abort;
      }

      /*
       * If the VMX asks to be root, then we allow them.
       * The VMX will make sure that only it will pass this value in,
       * and only when the VM and host are configured to allow this.
       *
       * XXX This has been deprecated XXX
       */
      if ((VIX_USER_CREDENTIAL_CONSOLE_USER == credentialType)
            && ((allowConsoleUserOps) || !(thisProcessRunsAsRoot))) {
         *userToken = PROCESS_CREATOR_USER_TOKEN;

         gImpersonatedUsername = Util_SafeStrdup("_CONSOLE_USER_NAME_");
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
            err = VixMsg_DeObfuscateNamePassword(obfuscatedNamePassword,
                                                 &unobfuscatedUserName,
                                                 &unobfuscatedPassword);
            if (err != VIX_OK) {
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

            *userToken = PROCESS_CREATOR_USER_TOKEN;
            gImpersonatedUsername = Util_SafeStrdup(unobfuscatedUserName);

            g_debug("%s: allowing interactive mode for user '%s'\n",
                    __FUNCTION__, gImpersonatedUsername);

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
       * Use the GuestAuth library to do name-password authentication
       * and impersonation.
       */

      if (GuestAuthEnabled() &&
          (VIX_USER_CREDENTIAL_NAME_PASSWORD == credentialType ||
           VIX_USER_CREDENTIAL_NAME_PASSWORD_OBFUSCATED == credentialType)) {
         err =
            GuestAuthPasswordAuthenticateImpersonate(obfuscatedNamePassword,
                                                     loadUserProfile,
                                                     userToken);
      }

#if SUPPORT_VGAUTH
      else if (VIX_USER_CREDENTIAL_SAML_BEARER_TOKEN == credentialType) {
         if (GuestAuthEnabled()) {
            err = GuestAuthSAMLAuthenticateAndImpersonate(obfuscatedNamePassword,
                                                          loadUserProfile,
                                                          userToken);
         } else {
            err = VIX_E_NOT_SUPPORTED;
         }
      }
#endif

      /* Get the authToken and impersonate */
      else if (VIX_USER_CREDENTIAL_TICKETED_SESSION == credentialType) {
#ifdef _WIN32
         char *username;

         err = VixToolsGetTokenHandleFromTicketID(obfuscatedNamePassword,
                                                  &username,
                                                  &authToken);

         if (VIX_OK != err) {
            goto abort;
         }

         unobfuscatedUserName = Util_SafeStrdup(username);
         *userToken = (void *) authToken;
         success = Impersonate_Do(unobfuscatedUserName, authToken);
         if (!success) {
            err = VIX_E_INVALID_LOGIN_CREDENTIALS;
            goto abort;
         }

         gImpersonatedUsername = Util_SafeStrdup(username);
         err = VIX_OK;
#else
         err = VIX_E_NOT_SUPPORTED;
#endif
      } else if (VIX_USER_CREDENTIAL_NAME_PASSWORD == credentialType ||
                 VIX_USER_CREDENTIAL_NAME_PASSWORD_OBFUSCATED == credentialType) {

         /*
          * Other credential types, like guest, are all turned into
          * a name/password by the VMX.
          */

         err = VixMsg_DeObfuscateNamePassword(obfuscatedNamePassword,
                                              &unobfuscatedUserName,
                                              &unobfuscatedPassword);
         if (err != VIX_OK) {
            goto abort;
         }

         authToken = Auth_AuthenticateUser(unobfuscatedUserName,
                                           unobfuscatedPassword);
         if (NULL == authToken) {
            err = VIX_E_INVALID_LOGIN_CREDENTIALS;
            goto abort;
         }

         *userToken = (void *) authToken;
#ifdef _WIN32
         success = Impersonate_Do(unobfuscatedUserName, authToken);
#else
         /*
          * Use a tools-special version of user impersonation, since
          * lib/impersonate model isn't quite what we want on linux.
          */
         success = ProcMgr_ImpersonateUserStart(unobfuscatedUserName,
                                                authToken);
#endif
         if (!success) {
            err = VIX_E_INVALID_LOGIN_CREDENTIALS;
            goto abort;
         }

         gImpersonatedUsername = Util_SafeStrdup(unobfuscatedUserName);
         err = VIX_OK;
      } else {
         /*
          * If this is something else, then we are talking to a newer
          * version of the VMX.
          */
         err = VIX_E_NOT_SUPPORTED;
      }

abort:
      free(unobfuscatedUserName);
      Util_ZeroFreeString(unobfuscatedPassword);
   }

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
   free(gImpersonatedUsername);
   gImpersonatedUsername = NULL;

#if SUPPORT_VGAUTH
#if ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS
   if (PROCESS_CREATOR_USER_TOKEN == userToken) {
      g_debug("%s: Faking unimpersonate\n", __FUNCTION__);
   }
#endif
   if (NULL != currentUserHandle) {
      GuestAuthUnimpersonate();
      return;
   }
#endif
   if (PROCESS_CREATOR_USER_TOKEN != userToken) {
#if defined(_WIN32)
      Impersonate_Undo();
#else
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

#if SUPPORT_VGAUTH
   if (NULL != currentUserHandle) {
#ifdef _WIN32
      // close the handle we copied out
      CloseHandle((HANDLE) userToken);
#endif
      VGAuth_UserHandleFree(currentUserHandle);
      currentUserHandle = NULL;
      return;
   }
#endif

   if (NULL != userToken) {
      AuthToken authToken = (AuthToken) userToken;
      Auth_CloseToken(authToken);
   }
} // VixToolsLogoutUser


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsGetImpersonatedUsername --
 *
 * Return value:
 *    The name of the user currently being impersonated.  Must be freed
 *    by caller.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static char *
VixToolsGetImpersonatedUsername(void *userToken)
{
   char *userName = NULL;
   char *homeDir = NULL;

#if SUPPORT_VGAUTH
   if (NULL != currentUserHandle) {
      VGAuthContext *ctx;
      VGAuthError vgErr = TheVGAuthContext(&ctx);
      ASSERT(vgErr == VGAUTH_E_OK);

      vgErr = VGAuth_UserHandleUsername(ctx, currentUserHandle, &userName);
      if (VGAUTH_FAILED(vgErr)) {
         g_warning("%s: Unable to get username from userhandle %p\n",
                   __FUNCTION__, currentUserHandle);
      }
      return userName;
   }
#endif

   if (!ProcMgr_GetImpersonatedUserInfo(&userName, &homeDir)) {
      return Util_SafeStrdup("XXX failed to get username XXX");
   }
   free(homeDir);

   return userName;
} // VixToolsUnimpersonateUser


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
 * VixToolsFreeStartProgramState --
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
VixToolsFreeStartProgramState(VixToolsStartProgramState *asyncState) // IN
{
   if (NULL == asyncState) {
      return;
   }
#if defined(_WIN32) && SUPPORT_VGAUTH
   /*
    * Unload the user profile if saved.
    */
   if (asyncState->hProfile != INVALID_HANDLE_VALUE &&
       asyncState->hToken != INVALID_HANDLE_VALUE) {
      GuestAuthUnloadUserProfileAndToken(asyncState->hToken,
                                         asyncState->hProfile);
   }
#endif

   free(asyncState);
} // VixToolsFreeStartProgramState


/*
 *----------------------------------------------------------------------------
 *
 *  VixToolsGetTempFileCreateNameFunc --
 *
 *       This function is designed as part of implementing CreateTempFile,
 *       CreateTempDirectory VI guest operations.
 *
 *       This function will be passed to File_MakeTempEx2 when
 *       VixToolsGetTempFile() is called.
 *
 *  Return Value:
 *       If success, a dynamically allocated string with the base name of
 *       of the file. NULL otherwise.
 *
 *  Side effects:
 *        None.
 *
 *----------------------------------------------------------------------------
 */

static char *
VixToolsGetTempFileCreateNameFunc(uint32 num,     // IN:
                                  void *payload)  // IN:
{
   char *fileName = NULL;

   VixToolsGetTempFileCreateNameFuncData *data =
                           (VixToolsGetTempFileCreateNameFuncData *) payload;

   if (payload == NULL) {
      goto abort;
   }

   if ((data->filePrefix == NULL) ||
       (data->tag == NULL) ||
       (data->fileSuffix == NULL)) {
      goto abort;
   }

   fileName = Str_SafeAsprintf(NULL,
                               "%s%s%u%s",
                               data->filePrefix,
                               data->tag,
                               num,
                               data->fileSuffix);

abort:
   return fileName;
} // VixToolsGetTempFileCreateNameFunc


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
VixToolsGetTempFile(VixCommandRequestHeader *requestMsg,   // IN
                    void *userToken,                       // IN
                    Bool useSystemTemp,                    // IN
                    char **tempFile,                       // OUT
                    int *tempFileFd)                       // OUT
{
   VixError err = VIX_E_FAIL;
   char *tempFilePath = NULL;
   int fd = -1;
   char *directoryPath = NULL;
   VixToolsGetTempFileCreateNameFuncData data;
   Bool createTempFile = TRUE;

   if (NULL == tempFile || NULL == tempFileFd) {
      ASSERT(0);
      return err;
   }

   *tempFile = NULL;
   *tempFileFd = -1;

   data.filePrefix = NULL;
   data.fileSuffix = NULL;
   data.tag = Util_SafeStrdup("vmware");

   if ((VIX_COMMAND_CREATE_TEMPORARY_FILE_EX == requestMsg->opCode) ||
       (VIX_COMMAND_CREATE_TEMPORARY_DIRECTORY == requestMsg->opCode)) {
      VixMsgCreateTempFileRequestEx *makeTempFileRequest;
      char *tempPtr = NULL;

      makeTempFileRequest = (VixMsgCreateTempFileRequestEx *) requestMsg;

      if ((requestMsg->commonHeader.bodyLength +
           requestMsg->commonHeader.headerLength) !=
          (((uint64) sizeof(*makeTempFileRequest)) +
           makeTempFileRequest->filePrefixLength + 1 +
           makeTempFileRequest->fileSuffixLength + 1 +
           makeTempFileRequest->directoryPathLength + 1 +
           makeTempFileRequest->propertyListLength)) {
         ASSERT(0);
         g_warning("%s: Invalid request message received\n", __FUNCTION__);
         err = VIX_E_INVALID_MESSAGE_BODY;
         goto abort;
      }

      tempPtr = ((char *) makeTempFileRequest) + sizeof(*makeTempFileRequest);

      if ('\0' != *(tempPtr + makeTempFileRequest->filePrefixLength)) {
         ASSERT(0);
         g_warning("%s: Invalid request message received\n", __FUNCTION__);
         err = VIX_E_INVALID_MESSAGE_BODY;
         goto abort;
      }

      data.filePrefix = Util_SafeStrdup(tempPtr);
      tempPtr += makeTempFileRequest->filePrefixLength + 1;

      if ('\0' != *(tempPtr + makeTempFileRequest->fileSuffixLength)) {
         ASSERT(0);
         g_warning("%s: Invalid request message received\n", __FUNCTION__);
         err = VIX_E_INVALID_MESSAGE_BODY;
         goto abort;
      }

      data.fileSuffix = Util_SafeStrdup(tempPtr);
      tempPtr += makeTempFileRequest->fileSuffixLength + 1;

      if ('\0' != *(tempPtr + makeTempFileRequest->directoryPathLength)) {
         ASSERT(0);
         g_warning("%s: Invalid request message received\n", __FUNCTION__);
         err = VIX_E_INVALID_MESSAGE_BODY;
         goto abort;
      }

      if (VIX_COMMAND_CREATE_TEMPORARY_DIRECTORY == requestMsg->opCode) {
         createTempFile = FALSE;
      }

      directoryPath = Util_SafeStrdup(tempPtr);
   } else {
      data.filePrefix = Util_SafeStrdup("");
      data.fileSuffix = Util_SafeStrdup("");
      directoryPath = Util_SafeStrdup("");
   }

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
      if (!(strcmp(directoryPath, ""))) {
         free(directoryPath);
         directoryPath = NULL;
         if (useSystemTemp) {
            wchar_t tempPath[MAX_PATH];
            DWORD dwRet = GetTempPathW(ARRAYSIZE(tempPath), tempPath);
            if (0 < dwRet && dwRet < ARRAYSIZE(tempPath)) {
               directoryPath = Unicode_AllocWithUTF16(tempPath);
               err = VIX_OK;
            } else {
               g_warning("%s: unable to get temp path: windows error code %d\n",
                         __FUNCTION__, GetLastError());
               err = VIX_E_FAIL;
            }
         } else {
            err = VixToolsGetUserTmpDir(userToken, &directoryPath);
         }
      } else {
         /*
          * Initially, when 'err' variable is defined, it is initialized to
          * VIX_E_FAIL. At this point in the code, user has already specified
          * the directory path in which the temporary file has to be created.
          * This is completely fine. So, just set 'err' to VIX_OK.
          */
         err = VIX_OK;
      }

      if (VIX_SUCCEEDED(err)) {

         /*
          * If the specified directory path doesn't exist or points to an
          * existing regular file, then File_MakeTempEx2() returns different
          * errors on Windows and Linux platforms. So, check for the proper
          * filetype and return proper errors before calling
          * File_MakeTempEx2().
          */
         if (!File_Exists(directoryPath)) {
            err = FoundryToolsDaemon_TranslateSystemErr();
            goto abort;
         }

         if (File_IsFile(directoryPath)) {
            err = VIX_E_NOT_A_DIRECTORY;
            goto abort;
         }

         fd = File_MakeTempEx2(directoryPath,
                               createTempFile,
                               VixToolsGetTempFileCreateNameFunc,
                               &data,
                               &tempFilePath);
         if (fd < 0) {
            /*
             * File_MakeTempEx() function internally uses Posix variant
             * functions and proper error will be stuffed in errno variable.
             * If File_MakeTempEx() fails, then use Vix_TranslateErrno()
             * to translate the errno to a proper foundry error.
             */
            err = Vix_TranslateErrno(errno);
            goto abort;
         }
      } else {
         /*
          * Don't give up if VixToolsGetUserTmpDir() failed. It might just
          * have failed to load DLLs, so we might be running on Win 9x.
          * Just fall through to use the old fashioned
          * File_GetSafeRandomTmpDir().
          */

         ASSERT(directoryPath == NULL);
         directoryPath = Util_SafeStrdup("");
         err = VIX_OK;
      }
   }
#endif

   if (NULL == tempFilePath) {
      if (!strcmp(directoryPath, "")) {
         free(directoryPath);
         directoryPath = NULL;
         directoryPath = File_GetSafeRandomTmpDir(TRUE);
      }

      /*
       * If the specified directory path doesn't exist or points to an
       * existing regular file, then File_MakeTempEx2() returns different
       * errors on Windows and Linux platforms. So, check for the proper
       * filetype and return proper errors before calling
       * File_MakeTempEx2().
       */
      if (!File_Exists(directoryPath)) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         goto abort;
      }

      if (File_IsFile(directoryPath)) {
         err = VIX_E_NOT_A_DIRECTORY;
         goto abort;
      }

      fd = File_MakeTempEx2(directoryPath,
                            createTempFile,
                            VixToolsGetTempFileCreateNameFunc,
                            &data,
                            &tempFilePath);
      if (fd < 0) {
         /*
          * File_MakeTempEx2() function internally uses Posix variant
          * functions and proper error will be stuffed in errno variable.
          * If File_MakeTempEx2() fails, then use Vix_TranslateErrno()
          * to translate the errno to a proper foundry error.
          */
         err = Vix_TranslateErrno(errno);
         goto abort;
      }
   }

   *tempFile = tempFilePath;
   *tempFileFd = fd;
   err = VIX_OK;

abort:
   free(data.filePrefix);
   free(data.fileSuffix);
   free(data.tag);

   free(directoryPath);

   return err;
} // VixToolsGetTempFile


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsProcessHgfsPacket --
 *
 *    This sends a packet to the HGFS server in the guest.
 *    We parse the user credential type and authentication
 *    information as strings, followed by the actual HGFS packet
 *    that is to be sent to the HGFS Server in the guest Tools.
 *
 *    The authentication information is used to impersonate a user
 *    in the guest using VixToolsImpersonateUser, and then calls
 *    HgfsServerManager_ProcessPacket to issue the HGFS packet to
 *    the HGFS Server. The HGFS Server reply is contained in the
 *    HGFS reply packet, which will be returned back to
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
                          GMainLoop *eventQueue,                  // IN
                          char **result,                          // OUT
                          size_t *resultValueResult)              // OUT
{
   VixError err = VIX_OK;
   void *userToken = NULL;
   Bool impersonatingVMWareUser = FALSE;
   const char *hgfsPacket;
   size_t hgfsReplyPacketSize = 0;
   static char hgfsReplyPacket[HGFS_LARGE_PACKET_MAX];
   VMAutomationRequestParser parser;

   if ((NULL == requestMsg) || (0 == requestMsg->hgfsPacketSize)) {
      ASSERT(0);
      err = VIX_E_FAIL;
      goto abort;
   }

   err = VMAutomationRequestParserInit(&parser,
                                      &requestMsg->header, sizeof *requestMsg);
   if (VIX_OK != err) {
      goto abort;
   }

   /*
    * When transferring file to/from guest, VixToolsProcessHgfsPacket is
    * repeatedly called. Skip loading user profile, which is unnecessary
    * here, to minimize performance impact.
    */
   err = VixToolsImpersonateUser((VixCommandRequestHeader *) requestMsg,
                                 FALSE, // Do not load user profile
                                 &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   err = VMAutomationRequestParserGetData(&parser,
                                          requestMsg->hgfsPacketSize,
                                          &hgfsPacket);
   if (VIX_OK != err) {
      goto abort;
   }

   hgfsReplyPacketSize = sizeof hgfsReplyPacket;

   /*
    * Impersonation was okay, so let's give our packet to
    * the HGFS server and forward the reply packet back.
    */
   HgfsServerManager_ProcessPacket(&gVixHgfsBkdrConn,          // connection
                                   hgfsPacket,                 // packet in buf
                                   requestMsg->hgfsPacketSize, // packet in size
                                   hgfsReplyPacket,            // packet out buf
                                   &hgfsReplyPacketSize);      // in/out size

   if (eventQueue != NULL) {
      /*
       * Register a timer to periodically invalidate any inactive
       * HGFS sessions.
       */
      VixToolsRegisterHgfsSessionInvalidator(eventQueue);
   }

   if (NULL != resultValueResult) {
      *resultValueResult = hgfsReplyPacketSize;
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
   static char resultBuffer[GUESTMSG_MAX_IN_SIZE];
   Bool impersonatingVMWareUser = FALSE;
   void *userToken = NULL;
   char *destPtr;
   char *endDestPtr;
   Bool escapeStrs;
#if defined(_WIN32) || defined(__linux__)
   Bool truncated;
#endif
#if defined(_WIN32)
   char **driveList = NULL;
   int numDrives = -1;
   uint64 freeBytesToUser = 0;
   uint64 totalBytesToUser = 0;
   uint64 freeBytes = 0;
   char *fileSystemType;
   int i;
#endif
#ifdef linux
   MNTHANDLE fp;
   DECLARE_MNTINFO(mnt);
   const char *mountfile = NULL;
#endif

   destPtr = resultBuffer;
   *destPtr = 0;
   endDestPtr = resultBuffer + sizeof(resultBuffer);

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);

   escapeStrs = (requestMsg->requestFlags &
                 VIX_REQUESTMSG_ESCAPE_XML_DATA) != 0;

#if defined(_WIN32)
   numDrives = Win32U_GetLogicalDriveStrings(&driveList);
   if (-1 == numDrives) {
      g_warning("%s: unable to get drive listing: windows error code %d\n",
                __FUNCTION__, GetLastError());
      err = FoundryToolsDaemon_TranslateSystemErr();
      goto abort;
   }

   if (escapeStrs) {
      destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr, "%s",
                             VIX_XML_ESCAPED_TAG);
   }

   for (i = 0; i < numDrives; i++) {
      if (!Win32U_GetDiskFreeSpaceEx(driveList[i],
                                     (PULARGE_INTEGER) &freeBytesToUser,
                                     (PULARGE_INTEGER) &totalBytesToUser,
                                     (PULARGE_INTEGER) &freeBytes)) {
         /*
          * If we encounter an error, just return 0 values for the space info
          */
         freeBytesToUser = 0;
         totalBytesToUser = 0;
         freeBytes = 0;

         g_warning("unable to get drive size info: windows error code %d\n",
                    GetLastError());
      }

      // If it fails, fileSystemType will be NULL
      Win32U_GetVolumeInformation(driveList[i],
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  &fileSystemType);
      err = VixToolsPrintFileSystemInfo(&destPtr, endDestPtr,
                                        driveList[i], totalBytesToUser,
                                        freeBytesToUser,
                                        fileSystemType ? fileSystemType : "",
                                        escapeStrs, &truncated);
      if ((VIX_OK != err) || truncated) {
         goto abort;
      }
      free(fileSystemType);
   }

#elif defined(__linux__)

   mountfile = "/etc/mtab";

   fp = Posix_Setmntent(mountfile, "r");
   if (fp == NULL) {
      g_warning("failed to open mount file\n");
      err = VIX_E_FILE_NOT_FOUND;
      goto abort;
   }

   while (GETNEXT_MNTINFO(fp, mnt)) {
      struct statfs statfsbuf;
      uint64 size, freeSpace;

      if (Posix_Statfs(MNTINFO_MNTPT(mnt), &statfsbuf)) {
         g_warning("%s unable to stat mount point %s\n",
                   __FUNCTION__, MNTINFO_MNTPT(mnt));
         continue;
      }
      size = (uint64) statfsbuf.f_blocks * (uint64) statfsbuf.f_bsize;
      freeSpace = (uint64) statfsbuf.f_bfree * (uint64) statfsbuf.f_bsize;
      err = VixToolsPrintFileSystemInfo(&destPtr, endDestPtr,
                                        MNTINFO_NAME(mnt), size, freeSpace,
                                        MNTINFO_FSTYPE(mnt), escapeStrs,
                                        &truncated);
      if ((VIX_OK != err) || truncated) {
         goto abort;
      }
   }
   CLOSE_MNTFILE(fp);
#else
   err = VIX_E_NOT_SUPPORTED;
#endif

abort:
#if defined(_WIN32)
   for (i = 0; i < numDrives; i++) {
      free(driveList[i]);
   }

   free(driveList);
#endif

   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   *result = resultBuffer;

   // XXX result too large for g_debug()

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return(err);
} // VixToolsListFileSystems


#if defined(_WIN32) || defined(__linux__)
/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsPrintFileSystemInfo --
 *
 *      Appends a single file system entry to the XML-like string starting at
 *      *destPtr.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static VixError
VixToolsPrintFileSystemInfo(char **destPtr,                // IN/OUT
                            const char *endDestPtr,        // IN
                            const char *name,              // IN
                            uint64 size,                   // IN
                            uint64 freeSpace,              // IN
                            const char *type,              // IN
                            Bool escapeStrs,               // IN
                            Bool *truncated)               // OUT
{
   VixError err;
   char *escapedName = NULL;
   char *escapedType = NULL;
   int bytesPrinted;

   ASSERT(endDestPtr > *destPtr);

   *truncated = FALSE;

   if (escapeStrs) {
      name = escapedName = VixToolsEscapeXMLString(name);
      if (NULL == escapedName) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }

      type = escapedType = VixToolsEscapeXMLString(type);
      if (NULL == escapedType) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
   }

   bytesPrinted = Str_Snprintf(*destPtr, endDestPtr - *destPtr,
                                "<filesystem>"
                               "<name>%s</name>"
                               "<size>%"FMT64"u</size>"
                               "<freeSpace>%"FMT64"u</freeSpace>"
                               "<type>%s</type>"
                               "</filesystem>",
                               name, size, freeSpace, type);
   if (bytesPrinted != -1) {
      *destPtr += bytesPrinted;
   } else { // out of space
      **destPtr = '\0';
      g_warning("%s: file system list results too large, truncating",
                 __FUNCTION__);
      *truncated = TRUE;
      err = VIX_OK;
      goto abort;
   }

   err = VIX_OK;

abort:
   free(escapedName);
   free(escapedType);

   return err;
}
#endif // #if defined(_WIN32) || defined(__linux__)


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsValidateCredentials --
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
VixToolsValidateCredentials(VixCommandRequestHeader *requestMsg)    // IN
{
   VixError err = VIX_OK;
   void *userToken = NULL;
   Bool impersonatingVMWareUser = FALSE;

   if (NULL == requestMsg) {
      ASSERT(0);
      err = VIX_E_FAIL;
      goto abort;
   }

   err = VixToolsImpersonateUser((VixCommandRequestHeader *) requestMsg,
                                 TRUE,
                                 &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);

abort:
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);


   return err;
}

/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsAcquireCredentials --
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
VixToolsAcquireCredentials(VixCommandRequestHeader *requestMsg,    // IN
                           GMainLoop *eventQueue,                  // IN
                           char **result)                          // OUT
{
   VixError err;

#if !defined(_WIN32)
   err = VIX_E_NOT_SUPPORTED;
   goto abort;
#else
   err = VixToolsAuthenticateWithSSPI(requestMsg, eventQueue, result);

   if (VIX_OK != err) {
      g_warning("%s: Failed to authenticate with SSPI with error %d\n",
                __FUNCTION__, err);
      goto abort;
   }
#endif

abort:
   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsReleaseCredentials --
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
VixToolsReleaseCredentials(VixCommandRequestHeader *requestMsg)    // IN
{
   VixError err = VIX_OK;

#if !defined(_WIN32)
   err = VIX_E_NOT_SUPPORTED;
#else
    err = VixToolsReleaseCredentialsImpl(requestMsg);
#endif

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
}


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
   if (NULL != nicEntry) {
      VMX_XDR_FREE(xdr_GuestNic, nicEntry);
      free(nicEntry);
   }

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

   err = VixToolsImpersonateUser(requestMsg, TRUE, &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);

   setGuestNetworkingConfigRequest = (VixMsgSetGuestNetworkingConfigRequest *)requestMsg;
   messageBody = (char *) requestMsg + sizeof(*setGuestNetworkingConfigRequest);

   VixPropertyList_Initialize(&propList);
   err = VixPropertyList_Deserialize(&propList,
                                     messageBody,
                                     setGuestNetworkingConfigRequest -> bufferSize,
                                     VIX_PROPERTY_LIST_BAD_ENCODING_ERROR);
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

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;

} // VixToolsSetGuestNetworkingConfig
#endif


#if SUPPORT_VGAUTH

/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsAddAuthAlias --
 *
 *    Calls to VGAuth to add a new alias.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    VGAuth alias store is updated.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsAddAuthAlias(VixCommandRequestHeader *requestMsg)    // IN
{
   VixError err = VIX_OK;
   VGAuthError vgErr;
   void *userToken = NULL;
   VGAuthContext *ctx = NULL;
   VixMsgAddAuthAliasRequest *req;
   const char *userName;
   const char *pemCert;
   const char *subjectName;
   const char *aliasComment;
   VGAuthAliasInfo ai;
   VMAutomationRequestParser parser;
   Bool impersonatingVMWareUser = FALSE;

   err = VMAutomationRequestParserInit(&parser, requestMsg, sizeof *req);
   if (VIX_OK != err) {
      goto abort;
   }

   req = (VixMsgAddAuthAliasRequest *) requestMsg;
   err = VMAutomationRequestParserGetOptionalString(&parser, req->userNameLen,
                                                    &userName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL == userName || 0 == *userName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VMAutomationRequestParserGetOptionalString(&parser, req->pemCertLen,
                                                    &pemCert);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL == pemCert || 0 == *pemCert) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   if ((req->subjectType != VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED) &&
       (req->subjectType != VIX_GUEST_AUTH_SUBJECT_TYPE_ANY)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VMAutomationRequestParserGetOptionalString(&parser, req->subjectNameLen,
                                                    &subjectName);
   if (VIX_OK != err) {
      goto abort;
   }

   if ((req->subjectType == VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED) &&
       (NULL == subjectName || 0 == *subjectName)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VMAutomationRequestParserGetOptionalString(&parser, req->aliasCommentLen,
                                                    &aliasComment);
   if (VIX_OK != err) {
      goto abort;
   }

   err = VixToolsImpersonateUser((VixCommandRequestHeader *) requestMsg,
                                 TRUE,
                                 &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);
   /*
    * For aliasStore APIs, make a fresh context so we know
    * the security is correct.
    */
   vgErr = VGAuth_Init(VMTOOLSD_APP_NAME, 0, NULL, &ctx);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto abort;
   }

   ai.subject.type = (req->subjectType == VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED) ?
      VGAUTH_SUBJECT_NAMED : VGAUTH_SUBJECT_ANY;
   ai.subject.val.name = (char *) subjectName;
   ai.comment = (char *) aliasComment;

   vgErr = VGAuth_AddAlias(ctx, userName, req->addMapping, pemCert, &ai,
                           0, NULL);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
   }

abort:
   if (ctx) {
      vgErr = VGAuth_Shutdown(ctx);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         // fall thru
      }
   }

   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsRemoveAuthAlias --
 *
 *    Calls to VGAuth to remove an alias.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    VGAuth Alias store is updated.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsRemoveAuthAlias(VixCommandRequestHeader *requestMsg)    // IN
{
   VixError err = VIX_OK;
   VGAuthError vgErr;
   void *userToken = NULL;
   VGAuthContext *ctx = NULL;
   VixMsgRemoveAuthAliasRequest *req;
   const char *userName;
   const char *pemCert;
   const char *subjectName;
   VGAuthSubject subj;
   VMAutomationRequestParser parser;
   Bool impersonatingVMWareUser = FALSE;

   err = VMAutomationRequestParserInit(&parser, requestMsg, sizeof *req);
   if (VIX_OK != err) {
      goto abort;
   }

   req = (VixMsgRemoveAuthAliasRequest *) requestMsg;
   err = VMAutomationRequestParserGetOptionalString(&parser, req->userNameLen,
                                                    &userName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL == userName || 0 == *userName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VMAutomationRequestParserGetOptionalString(&parser, req->pemCertLen,
                                                    &pemCert);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL == pemCert || 0 == *pemCert) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   if ((req->subjectType != VIX_GUEST_AUTH_SUBJECT_TYPE_NONE) &&
       (req->subjectType != VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED) &&
       (req->subjectType != VIX_GUEST_AUTH_SUBJECT_TYPE_ANY)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VMAutomationRequestParserGetOptionalString(&parser, req->subjectNameLen,
                                                    &subjectName);
   if (VIX_OK != err) {
      goto abort;
   }

   if ((req->subjectType == VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED) &&
       (NULL == subjectName || 0 == *subjectName)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser((VixCommandRequestHeader *) requestMsg,
                                 TRUE,
                                 &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);
   /*
    * For aliasStore APIs, make a fresh context so we know
    * the security is correct.
    */
   vgErr = VGAuth_Init(VMTOOLSD_APP_NAME, 0, NULL, &ctx);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto abort;
   }

   if (VIX_GUEST_AUTH_SUBJECT_TYPE_NONE == req->subjectType) {
#ifdef notyet
      /*
       * XXX turn on this assert() 'soon' -- if done now it could be hit
       * with these tools and an old hostd/VMX that still shares the opcode.
       */
      ASSERT(requestMsg->opCode == VIX_COMMAND_REMOVE_AUTH_ALIAS_BY_CERT);
#endif
      vgErr = VGAuth_RemoveAliasByCert(ctx, userName, pemCert, 0, NULL);
   } else {
      ASSERT(requestMsg->opCode == VIX_COMMAND_REMOVE_AUTH_ALIAS);
      subj.type = (req->subjectType == VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED) ?
         VGAUTH_SUBJECT_NAMED : VGAUTH_SUBJECT_ANY;
      subj.val.name = (char *) subjectName;

      vgErr = VGAuth_RemoveAlias(ctx, userName, pemCert, &subj, 0, NULL);
   }
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
   }

abort:
   if (ctx) {
      vgErr = VGAuth_Shutdown(ctx);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         // fall thru
      }
   }
   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListAuthAliases --
 *
 *    Calls to VGAuth to list user aliases.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    VGAuth Alias store is updated.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsListAuthAliases(VixCommandRequestHeader *requestMsg, // IN
                        size_t maxBufferSize,                // IN
                        char **result)                       // OUT
{
   VixError err = VIX_OK;
   VGAuthError vgErr;
   void *userToken = NULL;
   VGAuthContext *ctx = NULL;
   VixMsgListAuthAliasesRequest *req;
   const char *userName;
   VMAutomationRequestParser parser;
   Bool impersonatingVMWareUser = FALSE;
   int num = 0;
   int i;
   int j;
   VGAuthUserAlias *uaList = NULL;
   static char resultBuffer[GUESTMSG_MAX_IN_SIZE];
   char *destPtr;
   char *endDestPtr;
   char *tmpBuf = NULL;
   char *recordBuf;
   size_t recordSize;
   char *escapedStr = NULL;
   char *escapedStr2 = NULL;

   ASSERT(maxBufferSize <= GUESTMSG_MAX_IN_SIZE);

   *result = NULL;

   destPtr = resultBuffer;
   *destPtr = 0;

   err = VMAutomationRequestParserInit(&parser, requestMsg, sizeof *req);
   if (VIX_OK != err) {
      goto abort;
   }

   req = (VixMsgListAuthAliasesRequest *) requestMsg;
   err = VMAutomationRequestParserGetOptionalString(&parser, req->userNameLen,
                                                    &userName);
   if (VIX_OK != err) {
      goto abort;
   }

   if (NULL == userName || 0 == *userName) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   err = VixToolsImpersonateUser((VixCommandRequestHeader *) requestMsg,
                                 TRUE,
                                 &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);

   /*
    * For aliasStore APIs, make a fresh context so we know
    * the security is correct.
    */
   vgErr = VGAuth_Init(VMTOOLSD_APP_NAME, 0, NULL, &ctx);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto abort;
   }

   vgErr = VGAuth_QueryUserAliases(ctx, userName, 0, NULL, &num, &uaList);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto abort;
   }

   endDestPtr = resultBuffer + maxBufferSize;
   destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr, "%s",
                          VIX_XML_ESCAPED_TAG);
   for (i = 0; i < num; i++) {
      escapedStr = VixToolsEscapeXMLString(uaList[i].pemCert);
      if (escapedStr == NULL) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      tmpBuf = Str_Asprintf(NULL, "<record><pemCert>%s</pemCert>",
                            escapedStr);
      free(escapedStr);
      escapedStr = NULL;
      if (tmpBuf == NULL) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      for (j = 0; j < uaList[i].numInfos; j++) {
         char *nextBuf;

         if (uaList[i].infos[j].comment) {
            escapedStr = VixToolsEscapeXMLString(uaList[i].infos[j].comment);
            if (escapedStr == NULL) {
               err = VIX_E_OUT_OF_MEMORY;
               goto abort;
            }
         }
         if (uaList[i].infos[j].subject.type == VGAUTH_SUBJECT_NAMED) {
            escapedStr2 = VixToolsEscapeXMLString(uaList[i].infos[j].subject.val.name);
            if (escapedStr2 == NULL) {
               err = VIX_E_OUT_OF_MEMORY;
               goto abort;
            }
         }
         nextBuf = Str_Asprintf(NULL,
                                "%s"
                                "<alias>"
                                "<type>%d</type>"
                                "<name>%s</name>"
                                "<comment>%s</comment>"
                                "</alias>",
                                tmpBuf,
                                (uaList[i].infos[j].subject.type ==
                                   VGAUTH_SUBJECT_NAMED) ?
                                      VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED :
                                      VIX_GUEST_AUTH_SUBJECT_TYPE_ANY,
                                escapedStr2 ? escapedStr2 : "",
                                escapedStr ? escapedStr : "");
         if (nextBuf == NULL) {
            err = VIX_E_OUT_OF_MEMORY;
            goto abort;
         }
         free(tmpBuf);
         tmpBuf = nextBuf;
         free(escapedStr);
         escapedStr = NULL;
         free(escapedStr2);
         escapedStr2 = NULL;
      }
      recordBuf = Str_Asprintf(&recordSize,
                               "%s</record>",
                               tmpBuf);
      free(tmpBuf);
      tmpBuf = NULL;
      if (recordBuf == NULL) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      if ((destPtr + recordSize) < endDestPtr) {
         destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr,
                                "%s", recordBuf);
      } else {
         free(recordBuf);
         recordBuf = NULL;
         Log("%s: ListAuth list results too large, truncating", __FUNCTION__);
         goto abort;
      }
   }

   *result = resultBuffer;

abort:
   free(tmpBuf);
   free(escapedStr);
   free(escapedStr2);
   VGAuth_FreeUserAliasList(num, uaList);
   if (ctx) {
      vgErr = VGAuth_Shutdown(ctx);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         // fall thru
      }
   }

   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListMappedAliases --
 *
 *    Calls to VGAuth to list mapped aliases.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    VGAuth Alias store is updated.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsListMappedAliases(VixCommandRequestHeader *requestMsg, // IN
                          size_t maxBufferSize,                // IN
                          char **result)                       // OUT
{
   VixError err = VIX_OK;
   VGAuthError vgErr;
   void *userToken = NULL;
   VGAuthContext *ctx = NULL;
   VixMsgListMappedAliasesRequest *req;
   VMAutomationRequestParser parser;
   Bool impersonatingVMWareUser = FALSE;
   int num = 0;
   int i;
   int j;
   VGAuthMappedAlias *maList = NULL;
   static char resultBuffer[GUESTMSG_MAX_IN_SIZE];
   char *destPtr;
   char *endDestPtr;
   char *tmpBuf = NULL;
   char *recordBuf;
   char *escapedStr = NULL;
   char *escapedStr2 = NULL;
   size_t recordSize;

   ASSERT(maxBufferSize <= GUESTMSG_MAX_IN_SIZE);

   *result = NULL;
   destPtr = resultBuffer;
   *destPtr = 0;

   err = VMAutomationRequestParserInit(&parser, requestMsg, sizeof *req);
   if (VIX_OK != err) {
      goto abort;
   }

   req = (VixMsgListMappedAliasesRequest *) requestMsg;
   err = VixToolsImpersonateUser((VixCommandRequestHeader *) requestMsg,
                                 TRUE,
                                 &userToken);
   if (VIX_OK != err) {
      goto abort;
   }
   impersonatingVMWareUser = TRUE;

   g_debug("%s: User: %s\n",
           __FUNCTION__, IMPERSONATED_USERNAME);

   vgErr = TheVGAuthContext(&ctx);
   if (vgErr != VGAUTH_E_OK) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto abort;
   }

   /*
    * For aliasStore APIs, make a fresh context so we know
    * the security is correct.
    */
   vgErr = VGAuth_Init(VMTOOLSD_APP_NAME, 0, NULL, &ctx);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto abort;
   }

   vgErr = VGAuth_QueryMappedAliases(ctx, 0, NULL, &num, &maList);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto abort;
   }

   endDestPtr = resultBuffer + maxBufferSize;
   destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr, "%s",
                          VIX_XML_ESCAPED_TAG);
   for (i = 0; i < num; i++) {
      escapedStr = VixToolsEscapeXMLString(maList[i].pemCert);
      if (escapedStr == NULL) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      escapedStr2 = VixToolsEscapeXMLString(maList[i].userName);
      if (escapedStr2 == NULL) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      tmpBuf = Str_Asprintf(NULL, "<record><pemCert>%s</pemCert>"
                            "<userName>%s</userName>",
                            escapedStr,
                            escapedStr2);
      g_free(escapedStr2);
      g_free(escapedStr);
      escapedStr = NULL;
      escapedStr2 = NULL;
      if (tmpBuf == NULL) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      for (j = 0; j < maList[i].numSubjects; j++) {
         char *nextBuf;

         if (maList[i].subjects[j].type == VGAUTH_SUBJECT_NAMED) {
            escapedStr = VixToolsEscapeXMLString(maList[i].subjects[j].val.name);
            if (escapedStr == NULL) {
               err = VIX_E_OUT_OF_MEMORY;
               goto abort;
            }
         }
         nextBuf = Str_Asprintf(NULL,
                                "%s"
                                "<alias>"
                                "<type>%d</type>"
                                "<name>%s</name>"
                                "</alias>",
                                tmpBuf,
                                (maList[i].subjects[j].type ==
                                   VGAUTH_SUBJECT_NAMED) ?
                                      VIX_GUEST_AUTH_SUBJECT_TYPE_NAMED :
                                      VIX_GUEST_AUTH_SUBJECT_TYPE_ANY,
                                escapedStr ? escapedStr : "");
         if (nextBuf == NULL) {
            err = VIX_E_OUT_OF_MEMORY;
            goto abort;
         }
         free(tmpBuf);
         tmpBuf = nextBuf;
         free(escapedStr);
         escapedStr = NULL;
      }
      recordBuf = Str_Asprintf(&recordSize,
                               "%s</record>",
                               tmpBuf);
      free(tmpBuf);
      tmpBuf = NULL;
      if (recordBuf == NULL) {
         err = VIX_E_OUT_OF_MEMORY;
         goto abort;
      }
      if ((destPtr + recordSize) < endDestPtr) {
         destPtr += Str_Sprintf(destPtr, endDestPtr - destPtr,
                                "%s", recordBuf);
      } else {
         free(recordBuf);
         recordBuf = NULL;
         Log("%s: ListMapped results too large, truncating", __FUNCTION__);
         goto abort;
      }
   }

   *result = resultBuffer;

abort:
   free(tmpBuf);
   free(escapedStr);
   free(escapedStr2);
   VGAuth_FreeMappedAliasList(num, maList);
   if (ctx) {
      vgErr = VGAuth_Shutdown(ctx);
      if (VGAUTH_FAILED(vgErr)) {
         err = VixToolsTranslateVGAuthError(vgErr);
         // fall thru
      }
   }

   if (impersonatingVMWareUser) {
      VixToolsUnimpersonateUser(userToken);
   }
   VixToolsLogoutUser(userToken);

   g_message("%s: opcode %d returning %"FMT64"d\n", __FUNCTION__,
             requestMsg->opCode, err);

   return err;
}
#endif   // SUPPORT_VGAUTH


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsCreateRegKey --
 *
 *    Calls the function to create a new Windows Registry Key.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    May affect applications reading the key.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsCreateRegKey(VixCommandRequestHeader *requestMsg)    // IN
{
#ifdef _WIN32
   return VixToolsCreateRegKeyImpl(requestMsg);
#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListRegKeys --
 *
 *    Calls the function to list all subkeys for a given Windows Registry Key.
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
VixToolsListRegKeys(VixCommandRequestHeader *requestMsg,    // IN
                    size_t maxBufferSize,                   // IN
                    void *eventQueue,                       // IN
                    char **result)                          // OUT
{
#ifdef _WIN32
   return VixToolsListRegKeysImpl(requestMsg, maxBufferSize, eventQueue, result);
#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsDeleteRegKey --
 *
 *    Calls the function to delete a Windows Registry Key.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    May affect applications reading the key.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsDeleteRegKey(VixCommandRequestHeader *requestMsg)    // IN
{
#ifdef _WIN32
   return VixToolsDeleteRegKeyImpl(requestMsg);
#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsSetRegValue --
 *
 *    Calls the function to set/create a Windows Registry Value for a given Key.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    May affect applications reading the key.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsSetRegValue(VixCommandRequestHeader *requestMsg)    // IN
{
#ifdef _WIN32
   return VixToolsSetRegValueImpl(requestMsg);
#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsListRegValues --
 *
 *    Calls the function to list all values for a given Windows Registry Key.
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
VixToolsListRegValues(VixCommandRequestHeader *requestMsg,    // IN
                      size_t maxBufferSize,                   // IN
                      void *eventQueue,                       // IN
                      char **result)                          // OUT
{
#ifdef _WIN32
   return VixToolsListRegValuesImpl(requestMsg, maxBufferSize, eventQueue, result);
#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsDeleteRegValue --
 *
 *    Calls the function to delete a Windows Registry Value for a given Key.
 *
 * Return value:
 *    VixError
 *
 * Side effects:
 *    May affect applications reading the key.
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsDeleteRegValue(VixCommandRequestHeader *requestMsg)    // IN
{
#ifdef _WIN32
   return VixToolsDeleteRegValueImpl(requestMsg);
#else
   return VIX_E_OP_NOT_SUPPORTED_ON_GUEST;
#endif
}


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
   DWORD retVal = 0;
   HANDLE processToken = INVALID_HANDLE_VALUE;
   PTOKEN_USER processTokenInfo = NULL;
   DWORD processTokenInfoSize = 0;
   char *sidUserName = NULL;
   DWORD sidUserNameSize = 0;
   char *sidDomainName = NULL;
   DWORD sidDomainNameSize = 0;
   SID_NAME_USE sidNameUse;

   /*
    * Check to see if the user provided a '<Domain>\<User>' formatted username
    */
   if (NULL != Str_Strchr(username, '\\')) {
      /*
       * A '<Domain>\<User>' formatted username was provided.
       * We must retrieve the domain as well as the username to verify
       * the current vixtools user matches the username provided
       */
      retVal = OpenProcessToken(GetCurrentProcess(),
                                TOKEN_READ,
                                &processToken);

      if (!retVal || !processToken) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("unable to open process token: windows error code %d\n",
                   GetLastError());

         goto abort;
      }

      // Determine necessary buffer size
      GetTokenInformation(processToken,
                          TokenUser,
                          NULL,
                          0,
                          &processTokenInfoSize);

      if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("unable to get token info: windows error code %d\n",
                   GetLastError());

         goto abort;
      }

      processTokenInfo = Util_SafeMalloc(processTokenInfoSize);

      if (!GetTokenInformation(processToken,
                               TokenUser,
                               processTokenInfo,
                               processTokenInfoSize,
                               &processTokenInfoSize)) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("unable to get token info: windows error code %d\n",
                   GetLastError());

         goto abort;
      }

      // Retrieve user name and domain name based on user's SID.
      Win32U_LookupAccountSid(NULL,
                              processTokenInfo->User.Sid,
                              NULL,
                              &sidUserNameSize,
                              NULL,
                              &sidDomainNameSize,
                              &sidNameUse);

      if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("unable to lookup account sid: windows error code %d\n",
                   GetLastError());
         goto abort;
      }

      sidUserName = Util_SafeMalloc(sidUserNameSize);
      sidDomainName = Util_SafeMalloc(sidDomainNameSize);

      if (!Win32U_LookupAccountSid(NULL,
                                   processTokenInfo->User.Sid,
                                   sidUserName,
                                   &sidUserNameSize,
                                   sidDomainName,
                                   &sidDomainNameSize,
                                   &sidNameUse)) {
         err = FoundryToolsDaemon_TranslateSystemErr();
         g_warning("unable to lookup account sid: windows error code %d\n",
                   GetLastError());
         goto abort;
     }

      // Populate currentUser with Domain + '\' + Username
      currentUser = Str_SafeAsprintf(NULL, "%s\\%s", sidDomainName, sidUserName);
   } else {
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
   }

   if (0 != Unicode_CompareIgnoreCase(username, currentUser)) {
      err = VIX_E_INTERACTIVE_SESSION_USER_MISMATCH;
      goto abort;
   }

   err = VIX_OK;

abort:
   free(sidDomainName);
   free(sidUserName);
   free(processTokenInfo);
   CloseHandle(processToken);
   free(currentUser);

#else /* Below is the POSIX case. */
   uid_t currentUid;
   struct passwd pwd;
   struct passwd *ppwd = &pwd;
   char *buffer = NULL; // a pool of memory for Posix_Getpwnam_r() to use.
   long bufferSize;

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
   errno = 0;
   bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
   if ((errno != 0) || (bufferSize <= 0)) {
      bufferSize = 16 * 1024;  // Unlimited; pick something reasonable
   }

   bufferSize *= 4;

   buffer = Util_SafeMalloc((size_t)bufferSize);

   if (Posix_Getpwnam_r(username, &pwd, buffer, bufferSize, &ppwd) != 0 ||
       NULL == ppwd) {
      /*
       * This username should exist, since it should have already
       * been validated by guestd. Assume it is a system error.
       */
      err = FoundryToolsDaemon_TranslateSystemErr();
      g_warning("Unable to get the uid for username %s.\n", username);
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
 * VixToolsCheckIfVixCommandEnabled --
 *
 *    Checks to see if the opcode has been disabled via the tools
 *    configuration.
 *
 *    This does not affect VIX_COMMAND_GET_TOOLS_STATE; that always
 *    needs to work.
 *
 *    Many non-VMODL APIs do not have an API specific option; those
 *    are only affected by the global setting.
 *
 * Return value:
 *    TRUE if enabled, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VixToolsCheckIfVixCommandEnabled(int opcode,                          // IN
                                 GKeyFile *confDictRef)               // IN
{
   Bool enabled = TRUE;
   switch (opcode) {
      /*
       * We always let this through, since its needed to do basic
       * init work.
       */
      case VIX_COMMAND_GET_TOOLS_STATE:
         enabled = TRUE;
         break;

      case VIX_COMMAND_LIST_PROCESSES:
      case VIX_COMMAND_LIST_PROCESSES_EX:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                    VIX_TOOLS_CONFIG_API_LIST_PROCESSES_NAME);
         break;

      case VIX_COMMAND_LIST_FILES:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_LIST_FILES_NAME);
         break;
      case VIX_COMMAND_DELETE_GUEST_FILE:
      case VIX_COMMAND_DELETE_GUEST_FILE_EX:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_DELETE_FILE_NAME);
         break;
      case VIX_COMMAND_DELETE_GUEST_DIRECTORY:
      case VIX_COMMAND_DELETE_GUEST_EMPTY_DIRECTORY:
      case VIX_COMMAND_DELETE_GUEST_DIRECTORY_EX:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_DELETE_DIRECTORY_NAME);
         break;
      case VIX_COMMAND_KILL_PROCESS:
      case VIX_COMMAND_TERMINATE_PROCESS:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_TERMINATE_PROCESS_NAME);
         break;
      case VIX_COMMAND_CREATE_DIRECTORY:
      case VIX_COMMAND_CREATE_DIRECTORY_EX:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_MAKE_DIRECTORY_NAME);
         break;
      case VIX_COMMAND_MOVE_GUEST_FILE:
      case VIX_COMMAND_MOVE_GUEST_FILE_EX:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_MOVE_FILE_NAME);
         break;
      case VIX_COMMAND_MOVE_GUEST_DIRECTORY:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_MOVE_DIRECTORY_NAME);
         break;
      case VIX_COMMAND_START_PROGRAM:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_START_PROGRAM_NAME);
         break;
      case VIX_COMMAND_CREATE_TEMPORARY_FILE:
      case VIX_COMMAND_CREATE_TEMPORARY_FILE_EX:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                   VIX_TOOLS_CONFIG_API_CREATE_TMP_FILE_NAME);
         break;
      case VIX_COMMAND_CREATE_TEMPORARY_DIRECTORY:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_CREATE_TMP_DIRECTORY_NAME);
         break;
      case VIX_COMMAND_READ_ENV_VARIABLES:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_READ_ENV_VARS_NAME);
         break;
      case VIX_COMMAND_SET_GUEST_FILE_ATTRIBUTES:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_CHANGE_FILE_ATTRS_NAME);
         break;

      case VIX_COMMAND_INITIATE_FILE_TRANSFER_FROM_GUEST:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_INITIATE_FILE_TRANSFER_FROM_GUEST_NAME);
         break;

      case VIX_COMMAND_INITIATE_FILE_TRANSFER_TO_GUEST:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_INITIATE_FILE_TRANSFER_TO_GUEST_NAME);
         break;

      case VIX_COMMAND_VALIDATE_CREDENTIALS:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_VALIDATE_CREDENTIALS_NAME);
         break;

      case VIX_COMMAND_ACQUIRE_CREDENTIALS:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_ACQUIRE_CREDENTIALS_NAME);
         break;

      case VIX_COMMAND_RELEASE_CREDENTIALS:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_RELEASE_CREDENTIALS_NAME);
         break;

      case VIX_COMMAND_ADD_AUTH_ALIAS:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_ADD_GUEST_ALIAS_NAME);
         break;

      case VIX_COMMAND_REMOVE_AUTH_ALIAS:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                               VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_NAME);
         break;

      case VIX_COMMAND_REMOVE_AUTH_ALIAS_BY_CERT:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                               VIX_TOOLS_CONFIG_API_REMOVE_GUEST_ALIAS_BY_CERT_NAME);
         break;

      case VIX_COMMAND_LIST_AUTH_PROVIDER_ALIASES:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_LIST_GUEST_ALIASES_NAME);
         break;

      case VIX_COMMAND_LIST_AUTH_MAPPED_ALIASES:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                              VIX_TOOLS_CONFIG_API_LIST_GUEST_MAPPED_ALIASES_NAME);
         break;

      case VIX_COMMAND_CREATE_REGISTRY_KEY:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_CREATE_REGISTRY_KEY_NAME);
         break;

      case VIX_COMMAND_LIST_REGISTRY_KEYS:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_LIST_REGISTRY_KEYS_NAME);
         break;

      case VIX_COMMAND_DELETE_REGISTRY_KEY:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_DELETE_REGISTRY_KEY_NAME);
         break;

      case VIX_COMMAND_SET_REGISTRY_VALUE:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_SET_REGISTRY_VALUE_NAME);
         break;

      case VIX_COMMAND_LIST_REGISTRY_VALUES:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_LIST_REGISTRY_VALUES_NAME);
         break;

      case VIX_COMMAND_DELETE_REGISTRY_VALUE:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef,
                                VIX_TOOLS_CONFIG_API_DELETE_REGISTRY_VALUE_NAME);
         break;

      /*
       * None of these opcode have a matching config entry (yet),
       * so they can all share.
       */
      case VIX_COMMAND_CHECK_USER_ACCOUNT:
      case VIX_COMMAND_LOGOUT_IN_GUEST:
      case VIX_COMMAND_GUEST_FILE_EXISTS:
      case VIX_COMMAND_DIRECTORY_EXISTS:
      case VIX_COMMAND_GET_FILE_INFO:
      case VIX_COMMAND_LIST_FILESYSTEMS:
      case VIX_COMMAND_READ_VARIABLE:
      case VIX_COMMAND_WRITE_VARIABLE:
      case VIX_COMMAND_GET_GUEST_NETWORKING_CONFIG:
      case VIX_COMMAND_SET_GUEST_NETWORKING_CONFIG:

      case VIX_COMMAND_REGISTRY_KEY_EXISTS:
      case VIX_COMMAND_READ_REGISTRY:
      case VIX_COMMAND_WRITE_REGISTRY:
      case VIX_COMMAND_DELETE_GUEST_REGISTRY_KEY:

      /*
       * These may want to use the VMODL API name that most closely
       * matches, but for now, leave them alone.
       */
      case VIX_COMMAND_RUN_SCRIPT_IN_GUEST:
      case VIX_COMMAND_RUN_PROGRAM:
      case VIX_COMMAND_LIST_DIRECTORY:
      case VMXI_HGFS_SEND_PACKET_COMMAND:
      default:
         enabled = !VixToolsGetAPIDisabledFromConf(confDictRef, NULL);
         break;
   }

   return enabled;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsCheckIfAuthenticationTypeEnabled --
 *
 *    Checks to see if a given authentication type has been
 *    disabled via the tools configuration.
 *
 * Return value:
 *    TRUE if enabled, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VixToolsCheckIfAuthenticationTypeEnabled(GKeyFile *confDictRef,     // IN
                                         const char *typeName)      // IN
{
   char authnDisabledName[64]; // Authentication.<AuthenticationType>.disabled
   gboolean disabled;

   Str_Snprintf(authnDisabledName, sizeof(authnDisabledName),
                VIX_TOOLS_CONFIG_API_AUTHENTICATION ".%s.disabled",
                typeName);

   ASSERT(confDictRef != NULL);

   /*
    * XXX Skip doing the strcmp() to verify the auth type since we only
    * have the one typeName (VIX_TOOLS_CONFIG_AUTHTYPE_AGENTS), and default
    * it to VIX_TOOLS_CONFIG_INFRA_AGENT_DISABLED_DEFAULT.
    */
   disabled = VixTools_ConfigGetBoolean(confDictRef,
                                        VIX_TOOLS_CONFIG_API_GROUPNAME,
                                        authnDisabledName,
                                        VIX_TOOLS_CONFIG_INFRA_AGENT_DISABLED_DEFAULT);

   return !disabled;
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

VixError
VixTools_ProcessVixCommand(VixCommandRequestHeader *requestMsg,   // IN
                           char *requestName,                     // IN
                           size_t maxResultBufferSize,            // IN
                           GKeyFile *confDictRef,                 // IN
                           GMainLoop *eventQueue,                 // IN
                           char **resultBuffer,                   // OUT
                           size_t *resultLen,                     // OUT
                           Bool *deleteResultBufferResult)        // OUT
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

   g_message("%s: command %d\n", __FUNCTION__, requestMsg->opCode);

   /*
    * PR 1210773: Check if new VIX commands can be processed.
    *
    * Most of the VIX commands require access to the guest
    * filesystem and therefore they could block when quiesced
    * snapshot operation has frozen the guest filesystem. A
    * blocked VIX command would not allow Tools service to
    * process other important ops like resuming filesystem
    * because Tools service is single threaded. Effectively,
    * a VIX command could deadlock a quiesce snapshot operation.
    *
    * A quiesce snapshot operation that follows a long running
    * VIX command like runprogram/startprogram is not an issue
    * because the running command gets blocked temporarily
    * only when it needs to access the filesystem, otherwise
    * it continues to run like any other application inside
    * guest.
    *
    * Return a generic error to make clients retry the command
    * in a graceful manner.
    */
   if (gRestrictCommands) {
      g_warning("%s: IO freeze restricted command %d\n",
                __FUNCTION__, requestMsg->opCode);
      err = VIX_E_OBJECT_IS_BUSY;
      goto abort;
   }

   /*
    * Set the global reference to configuration dictionary.
    * We do this to avoid passing this reference through multiple
    * interfaces for consumers like VixToolsImpersonateUser().
    *
    * ASSUMPTION: We are single threaded here, so we don't need
    * to acquire any locks for this step.
    */
   ASSERT(confDictRef != NULL);
   gConfDictRef = confDictRef;

   if (!VixToolsCheckIfVixCommandEnabled(requestMsg->opCode, confDictRef)) {
      err = VIX_E_OPERATION_DISABLED;
      g_message("%s: command %d disabled by configuration\n",
                __FUNCTION__, requestMsg->opCode);
      goto abort;
   }

   switch (requestMsg->opCode) {
      ////////////////////////////////////
      case VIX_COMMAND_CHECK_USER_ACCOUNT:
      case VIX_COMMAND_LOGOUT_IN_GUEST:
         err = VixToolsCheckUserAccount(requestMsg);
         break;

      ////////////////////////////////////
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
         err = VixToolsListProcesses(requestMsg,
                                     maxResultBufferSize,
                                     &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_LIST_PROCESSES_EX:
         err = VixToolsListProcessesEx(requestMsg,
                                      maxResultBufferSize,
                                      eventQueue,
                                      &resultValue);
         deleteResultValue = TRUE;
         break;

      ////////////////////////////////////
      case VIX_COMMAND_LIST_DIRECTORY:
         err = VixToolsListDirectory(requestMsg,
                                     maxResultBufferSize,
                                     &resultValue);
         deleteResultValue = TRUE;
         break;

      ////////////////////////////////////
      case VIX_COMMAND_LIST_FILES:
         err = VixToolsListFiles(requestMsg,
                                 maxResultBufferSize,
                                 &resultValue);
         deleteResultValue = TRUE;
         break;
      ////////////////////////////////////
      case VIX_COMMAND_DELETE_GUEST_FILE:
      case VIX_COMMAND_DELETE_GUEST_FILE_EX:
      case VIX_COMMAND_DELETE_GUEST_REGISTRY_KEY:
      case VIX_COMMAND_DELETE_GUEST_DIRECTORY:
      case VIX_COMMAND_DELETE_GUEST_EMPTY_DIRECTORY:
         err = VixToolsDeleteObject(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_DELETE_GUEST_DIRECTORY_EX:
         err = VixToolsDeleteDirectory(requestMsg);
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
      case VIX_COMMAND_TERMINATE_PROCESS:
         err = VixToolsKillProcess(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_CREATE_DIRECTORY:
      case VIX_COMMAND_CREATE_DIRECTORY_EX:
         err = VixToolsCreateDirectory(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_MOVE_GUEST_FILE:
      case VIX_COMMAND_MOVE_GUEST_FILE_EX:
      case VIX_COMMAND_MOVE_GUEST_DIRECTORY:
         err = VixToolsMoveObject(requestMsg);
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
      case VIX_COMMAND_START_PROGRAM:
         err = VixTools_StartProgram(requestMsg, requestName, eventQueue, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_CREATE_TEMPORARY_FILE:
      case VIX_COMMAND_CREATE_TEMPORARY_FILE_EX:
      case VIX_COMMAND_CREATE_TEMPORARY_DIRECTORY:
         err = VixToolsCreateTempFile(requestMsg, &resultValue);
         deleteResultValue = TRUE;
         break;

      ///////////////////////////////////
      case VIX_COMMAND_READ_VARIABLE:
         err = VixToolsReadVariable(requestMsg, &resultValue);
         deleteResultValue = TRUE;
         break;

      ///////////////////////////////////
      case VIX_COMMAND_READ_ENV_VARIABLES:
         err = VixToolsReadEnvVariables(requestMsg, &resultValue);
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
      case VIX_COMMAND_SET_GUEST_FILE_ATTRIBUTES:
         err = VixToolsSetFileAttributes(requestMsg);
         break;

      ///////////////////////////////////
      case VMXI_HGFS_SEND_PACKET_COMMAND:
         err = VixToolsProcessHgfsPacket((VixCommandHgfsSendPacket *) requestMsg,
                                         eventQueue,
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

      ////////////////////////////////////
      case VIX_COMMAND_LIST_FILESYSTEMS:
         err = VixToolsListFileSystems(requestMsg, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_INITIATE_FILE_TRANSFER_FROM_GUEST:
         err = VixToolsInitiateFileTransferFromGuest(requestMsg,
                                                     &resultValue);
         deleteResultValue = TRUE;
         break;

      ////////////////////////////////////
      case VIX_COMMAND_INITIATE_FILE_TRANSFER_TO_GUEST:
         err = VixToolsInitiateFileTransferToGuest(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_VALIDATE_CREDENTIALS:
         err = VixToolsValidateCredentials(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_ACQUIRE_CREDENTIALS:
         err = VixToolsAcquireCredentials(requestMsg, eventQueue, &resultValue);
         // resultValue is static. Do not free it.
         break;

      ////////////////////////////////////
      case VIX_COMMAND_RELEASE_CREDENTIALS:
         err = VixToolsReleaseCredentials(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_WAIT_FOR_TOOLS:
         /*
          * Older VMX's can send this.  We don't want to do anything, but
          * we also don't want it to be treated as unknown and return
          * VIX_E_UNRECOGNIZED_COMMAND_IN_GUEST.
          */
         break;

      case VIX_COMMAND_CAPTURE_SCREEN:
         /*
          * The VMX sends this through just to validate the auth info.
          * Just no-op it so we don't fall through to the 'default'.
          */
         break;

#if SUPPORT_VGAUTH
      case VIX_COMMAND_ADD_AUTH_ALIAS:
         err = VixToolsAddAuthAlias(requestMsg);
         break;
      case VIX_COMMAND_REMOVE_AUTH_ALIAS:
      case VIX_COMMAND_REMOVE_AUTH_ALIAS_BY_CERT:
         err = VixToolsRemoveAuthAlias(requestMsg);
         break;
      case VIX_COMMAND_LIST_AUTH_PROVIDER_ALIASES:
          err = VixToolsListAuthAliases(requestMsg, maxResultBufferSize,
                                        &resultValue);
         // resultValue is static. Do not free it.
         break;
      case VIX_COMMAND_LIST_AUTH_MAPPED_ALIASES:
          err = VixToolsListMappedAliases(requestMsg, maxResultBufferSize,
                                          &resultValue);
         // resultValue is static. Do not free it.
         break;
#endif

      ////////////////////////////////////
      case VIX_COMMAND_CREATE_REGISTRY_KEY:
         err = VixToolsCreateRegKey(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_LIST_REGISTRY_KEYS:
         err = VixToolsListRegKeys(requestMsg,
                                   maxResultBufferSize,
                                   eventQueue,
                                   &resultValue);
         deleteResultValue = TRUE;
         break;

      ////////////////////////////////////
      case VIX_COMMAND_DELETE_REGISTRY_KEY:
         err = VixToolsDeleteRegKey(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_SET_REGISTRY_VALUE:
         err = VixToolsSetRegValue(requestMsg);
         break;

      ////////////////////////////////////
      case VIX_COMMAND_LIST_REGISTRY_VALUES:
         err = VixToolsListRegValues(requestMsg,
                                     maxResultBufferSize,
                                     eventQueue,
                                     &resultValue);
         deleteResultValue = TRUE;
         break;

      ////////////////////////////////////
      case VIX_COMMAND_DELETE_REGISTRY_VALUE:
         err = VixToolsDeleteRegValue(requestMsg);
         break;

      ////////////////////////////////////
      default:
         /*
          * If the opcode is not recognized, tools might be old and the
          * VIX client might be sending new opcodes. In such case,
          * we should return VIX_E_UNRECOGNIZED_COMMAND_IN_GUEST.
          */
         err = VIX_E_UNRECOGNIZED_COMMAND_IN_GUEST;
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

   /*
    * Remaps specific errors for backward compatibility purposes.
    */
   err = VixToolsRewriteError(requestMsg->opCode, err);

   /*
    * Reset the global reference to configuration dictionary
    */
   gConfDictRef = NULL;

   return(err);
} // VixTools_ProcessVixCommand


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsRewriteError --
 *
 *    Rewrites the error if necessary.
 *
 *    Some errors returned by tools need to be changed so
 *    that error code consistency with old VIX is maintained.
 *
 *    So specific errors from specific operations are rewritten here.
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VixError
VixToolsRewriteError(uint32 opCode,          // IN
                     VixError origError)     // IN
{
   VixError newError = origError;

   switch (opCode) {
      /*
       * This should include all non-VI guest operations.
       */
   case VIX_COMMAND_CHECK_USER_ACCOUNT:
   case VIX_COMMAND_LOGOUT_IN_GUEST:
   case VIX_COMMAND_GET_TOOLS_STATE:
   case VIX_COMMAND_LIST_PROCESSES:
   case VIX_COMMAND_LIST_DIRECTORY:
   case VIX_COMMAND_DELETE_GUEST_FILE:
   case VIX_COMMAND_DELETE_GUEST_REGISTRY_KEY:
   case VIX_COMMAND_DELETE_GUEST_DIRECTORY:
   case VIX_COMMAND_DELETE_GUEST_EMPTY_DIRECTORY:
   case VIX_COMMAND_REGISTRY_KEY_EXISTS:
   case VIX_COMMAND_GUEST_FILE_EXISTS:
   case VIX_COMMAND_DIRECTORY_EXISTS:
   case VIX_COMMAND_READ_REGISTRY:
   case VIX_COMMAND_WRITE_REGISTRY:
   case VIX_COMMAND_KILL_PROCESS:
   case VIX_COMMAND_CREATE_DIRECTORY:
   case VIX_COMMAND_MOVE_GUEST_FILE:
   case VIX_COMMAND_RUN_SCRIPT_IN_GUEST:
   case VIX_COMMAND_RUN_PROGRAM:
   case VIX_COMMAND_CREATE_TEMPORARY_FILE:
   case VIX_COMMAND_READ_VARIABLE:
   case VIX_COMMAND_WRITE_VARIABLE:
   case VIX_COMMAND_GET_FILE_INFO:
   case VMXI_HGFS_SEND_PACKET_COMMAND:
   case VIX_COMMAND_GET_GUEST_NETWORKING_CONFIG:
   case VIX_COMMAND_LIST_FILESYSTEMS:
   case VIX_COMMAND_WAIT_FOR_TOOLS:
   case VIX_COMMAND_CAPTURE_SCREEN:
      ASSERT(VIX_ERROR_CODE(origError) == origError);
      switch (origError) {
      case VIX_E_INVALID_LOGIN_CREDENTIALS:
         newError = VIX_E_GUEST_USER_PERMISSIONS;
         break;
      }
      break;
   }

   return newError;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixTools_GetAdditionalError --
 *
 *    Gets the vix extra/additional error if any.
 *
 *    Some errors returned by tools may have extra error in
 *    the higher order 32 bits. We need to pass that back.
 *
 * Results:
 *      uint32
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

uint32
VixTools_GetAdditionalError(uint32 opCode,    // IN
                            VixError error)   // IN
{
   uint32 err;

   switch (opCode) {
      case VIX_COMMAND_CREATE_REGISTRY_KEY:
      case VIX_COMMAND_LIST_REGISTRY_KEYS:
      case VIX_COMMAND_DELETE_REGISTRY_KEY:
      case VIX_COMMAND_SET_REGISTRY_VALUE:
      case VIX_COMMAND_LIST_REGISTRY_VALUES:
      case VIX_COMMAND_DELETE_REGISTRY_VALUE:
         err = VIX_ERROR_EXTRA_ERROR(error);
         break;
      default:
        err = Err_Errno();
   }

   return err;
}


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


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsEscapeXMLString --
 *
 *      Escapes a string to be included in VMAutomation XML.
 *
 * Results:
 *      Pointer to a heap-allocated escaped string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
VixToolsEscapeXMLString(const char *str)    // IN
{
   /*
    * Escape the escape character (%) and the five characters that are XML
    * sensitive - ', ", &, < and >.
    */

   static const int bytesToEscape[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,   // ", %, & and '
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,   // < and >
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   };

   return Escape_Do(VIX_XML_ESCAPE_CHARACTER, bytesToEscape, str, strlen(str),
                    NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VixToolsXMLStringEscapedLen --
 *
 *      Computes the length of the supplied string if it were escaped
 *      (if escapeStr is TRUE), or the length of the string as is.
 *
 * Results:
 *      The length.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static size_t
VixToolsXMLStringEscapedLen(const char *str,    // IN
                            Bool escapeStr)     // IN
{
   if (escapeStr) {
      size_t totalLen = 0;

      while (TRUE) {
         size_t nextLen = strcspn(str, "%<>&\'\"");

         totalLen += nextLen;
         if ('\0' == str[nextLen]) {
            break;
         }

         /*
          * str[nextLen] is a character that needs to be escaped. Each
          * escapeStr that is escaped will take up 3 bytes (an escape
          * character and two hex digits) in the escaped string.
          */

         totalLen += 3;
         str += nextLen + 1;
      }

      return totalLen;
   } else {
      return strlen(str);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAuthEnabled --
 *
 *      Returns whether we use the guest auth library.
 *
 * Results:
 *      TRUE if we do. FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestAuthEnabled(void)
{
#if SUPPORT_VGAUTH
   return gSupportVGAuth;
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAuthPasswordAuthenticateImpersonate
 *
 *      Do name-password authentication and impersonation using
 *      the GuestAuth library.
 *
 * Results:
 *      VIX_OK if successful.Other VixError code otherwise.
 *
 * Side effects:
 *      Current process impersonates.
 *
 *-----------------------------------------------------------------------------
 */

VixError
GuestAuthPasswordAuthenticateImpersonate(
   char const *obfuscatedNamePassword, // IN
   Bool loadUserProfile,               // IN
   void **userToken)                   // OUT
{
#if SUPPORT_VGAUTH
   VixError err;
   char *username = NULL;
   char *password = NULL;
   VGAuthContext *ctx = NULL;
   VGAuthError vgErr;
   VGAuthUserHandle *newHandle = NULL;
   VGAuthExtraParams extraParams[1];
   Bool impersonated = FALSE;

   extraParams[0].name = VGAUTH_PARAM_LOAD_USER_PROFILE;
   extraParams[0].value = loadUserProfile ? VGAUTH_PARAM_VALUE_TRUE :
                                            VGAUTH_PARAM_VALUE_FALSE;

   err = VixMsg_DeObfuscateNamePassword(obfuscatedNamePassword,
                                        &username,
                                        &password);
   if (err != VIX_OK) {
      goto done;
   }

   err = VIX_E_INVALID_LOGIN_CREDENTIALS;

   vgErr = TheVGAuthContext(&ctx);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }

   vgErr = VGAuth_ValidateUsernamePassword(ctx, username, password,
                                           0, NULL,
                                           &newHandle);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }

   vgErr = VGAuth_Impersonate(ctx, newHandle,
                              (int)ARRAYSIZE(extraParams),
                              extraParams);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }

   impersonated = TRUE;

#ifdef _WIN32
   // this is making a copy of the token, be sure to close it
   vgErr = VGAuth_UserHandleAccessToken(ctx, newHandle, userToken);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }
#endif

   currentUserHandle = newHandle;
   gImpersonatedUsername = Util_SafeStrdup(username);

   err = VIX_OK;

done:
   free(username);
   Util_ZeroFreeString(password);

   if (VIX_OK != err) {
      if (impersonated) {
         vgErr = VGAuth_EndImpersonation(ctx);
         ASSERT(vgErr == VGAUTH_E_OK);
      }
      VGAuth_UserHandleFree(newHandle);
      newHandle = NULL;
   }
   return err;
#else
   return VIX_E_NOT_SUPPORTED;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAuthSAMLAuthenticateAndImpersonate
 *
 *      Do SAML bearer token authentication and impersonation using
 *      the GuestAuth library.
 *
 * Results:
 *      VIX_OK if successful.  Other VixError code otherwise.
 *
 * Side effects:
 *      Current process impersonates.
 *
 *-----------------------------------------------------------------------------
 */

VixError
GuestAuthSAMLAuthenticateAndImpersonate(
   char const *obfuscatedNamePassword, // IN
   Bool loadUserProfile,               // IN
   void **userToken)                   // OUT
{
#if SUPPORT_VGAUTH
   VixError err;
   char *token = NULL;
   char *username = NULL;
   VGAuthContext *ctx = NULL;
   VGAuthError vgErr;
   VGAuthUserHandle *newHandle = NULL;
   VGAuthExtraParams extraParams[1];
   Bool impersonated = FALSE;

   extraParams[0].name = VGAUTH_PARAM_LOAD_USER_PROFILE;
   extraParams[0].value = loadUserProfile ? VGAUTH_PARAM_VALUE_TRUE :
                                            VGAUTH_PARAM_VALUE_FALSE;

   err = VixMsg_DeObfuscateNamePassword(obfuscatedNamePassword,
                                        &token,
                                        &username);
   if (err != VIX_OK) {
      goto done;
   }

   err = VIX_E_INVALID_LOGIN_CREDENTIALS;

   vgErr = TheVGAuthContext(&ctx);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }

   vgErr = VGAuth_ValidateSamlBearerToken(ctx,
                                          token,
                                          username,
                                          0,
                                          NULL,
                                          &newHandle);
#if ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS
   /*
    * Special support for local SYSTEM account.
    *
    * If validation fails, try again without token
    * creation, and if it passes, fake the impersonation.
    */

   // normal case worked
   if (!VGAUTH_FAILED(vgErr)) {
      goto impersonate;
   }

   /*
    * If the config is off, bypass the special-case.
    */
   if (!VixTools_ConfigGetBoolean(gConfDictRef,
                                  VIX_TOOLS_CONFIG_API_GROUPNAME,
                      VIXTOOLS_CONFIG_ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS,
                      ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS_DEFAULT)) {
      g_debug("%s: SAML authn failed, %s not set, skipping local SYSTEM check",
              __FUNCTION__,
              VIXTOOLS_CONFIG_ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS);
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }

   /*
    * VGAUTH_E_FAIL will be returned if token creation fails.
    */
   if (vgErr != VGAUTH_E_FAIL) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   } else {
      /*
       * See if we have a SAML token associated with the toolsd owner.
       * If this returns OK, we don't bother to impersonate.
       * If it fails, return an error.
       */
      err = VixToolsCheckSAMLForSystem(ctx,
                                       vgErr,
                                       token,
                                       username,
                                       gCurrentUsername,
                                       userToken,
                                       &currentUserHandle);
      goto done;
   }
#else
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }
#endif

#if ALLOW_LOCAL_SYSTEM_IMPERSONATION_BYPASS
impersonate:
#endif
   vgErr = VGAuth_Impersonate(ctx, newHandle,
                              (int)ARRAYSIZE(extraParams),
                              extraParams);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }

   impersonated = TRUE;

#ifdef _WIN32
   // this is making a copy of the token, be sure to close it
   vgErr = VGAuth_UserHandleAccessToken(ctx, newHandle, userToken);
   if (VGAUTH_FAILED(vgErr)) {
      err = VixToolsTranslateVGAuthError(vgErr);
      goto done;
   }
#endif

   currentUserHandle = newHandle;
   gImpersonatedUsername = VixToolsGetImpersonatedUsername(NULL);

   err = VIX_OK;

done:
   Util_ZeroFreeString(token);
   Util_ZeroFreeString(username);

   if (VIX_OK != err) {
      if (impersonated) {
         vgErr = VGAuth_EndImpersonation(ctx);
         ASSERT(vgErr == VGAUTH_E_OK);
      }
      VGAuth_UserHandleFree(newHandle);
      newHandle = NULL;
   }

   return err;
#else
   return VIX_E_NOT_SUPPORTED;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAuthUnimpersonate
 *
 *      End the current impersonation using the VGAuth library.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Current process un-impersonates.
 *
 *-----------------------------------------------------------------------------
 */

void
GuestAuthUnimpersonate(void)
{
#if SUPPORT_VGAUTH
   VGAuthContext *ctx;
   VGAuthError vgErr = TheVGAuthContext(&ctx);
   ASSERT(vgErr == VGAUTH_E_OK);


   vgErr = VGAuth_EndImpersonation(ctx);
   ASSERT(vgErr == VGAUTH_E_OK);

#else
   ASSERT(0);
#endif
}


/**
 *-----------------------------------------------------------------------------
 * VixTools_ConfigGetBoolean
 *
 *    Get boolean entry for the key from the config file.
 *
 * Return value:
 *    Value for the key if key is found; otherwise defValue.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

gboolean
VixTools_ConfigGetBoolean(GKeyFile *confDictRef,      // IN
                          const char *group,          // IN
                          const char *key,            // IN
                          gboolean defValue)          // IN
{
   GError *gErr = NULL;
   gboolean value = defValue;

   ASSERT(confDictRef != NULL && group != NULL && key != NULL);

   if (confDictRef == NULL || group == NULL || key == NULL) {
      goto done;
   }

   value = g_key_file_get_boolean(confDictRef, group, key, &gErr);

   /*
    * g_key_file_get_boolean() will return FALSE and set an error
    * if the value isn't in config, so use the default in that
    * case.
    */
   if (!value && gErr != NULL) {
      g_clear_error(&gErr);
      value = defValue;
   }

done:

   return value;
}


#if SUPPORT_VGAUTH
/*
 *-----------------------------------------------------------------------------
 *
 * QueryVGAuthConfig
 *
 *      Check the tools configuration to see if VGAuth should be used.
 *
 * Results:
 *      TRUE if vgauth should be used, FALSE if not.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
QueryVGAuthConfig(GKeyFile *confDictRef)                       // IN
{
   gboolean retVal = USE_VGAUTH_DEFAULT;

   if (confDictRef != NULL) {
      retVal = VixTools_ConfigGetBoolean(confDictRef,
                                         VIX_TOOLS_CONFIG_API_GROUPNAME,
                                         VIXTOOLS_CONFIG_USE_VGAUTH_NAME,
                                         USE_VGAUTH_DEFAULT);
   }

   g_message("%s: vgauth usage is: %d\n", __FUNCTION__, retVal);

   return retVal;
}


/*
 *-----------------------------------------------------------------------------
 *
 * TheVGAuthContext
 *
 *      Get the global VGAuthContext object.
 *
 *      Lazily create the global VGAuthContext when needed.
 *      We need a single shared context to handle authentication in order to
 *      properly share the SSPI handshake state(s).
 *
 *      Creating the global context may also cause the VGAuth Service to
 *      be started.
 *
 *      This context should only be used when not impersonating, since it
 *      will be running over the SUPER_USER connection and can cause
 *      security issues if used when impersonating.
 *
 *
 * Results:
 *      VGAUTH_E_OK if successful, the global context object is returned in
 *      the OUT parameter ctx.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

VGAuthError
TheVGAuthContext(VGAuthContext **ctx) // OUT
{
   static VGAuthContext *vgaCtx = NULL;
   VGAuthError vgaCode = VGAUTH_E_OK;

   /*
    * XXX This needs to handle errors better -- if the VGAuthService
    * service gets reset, the context will point to junk and anything
    * using it will fail.
    *
    * Maybe add a no-op API here to poke it?  Or make the underlying
    * VGAuth code smarter.
    */
   if (vgaCtx == NULL) {
      vgaCode = VGAuth_Init(VMTOOLSD_APP_NAME, 0, NULL, &vgaCtx);
   }

   *ctx = vgaCtx;
   return vgaCode;
}


#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * GuestAuthUnloadUserProfileAndToken --
 *
 *    Unload user profile and close user token.
 *
 *    Helper to handle StartProgram cleanup.
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
GuestAuthUnloadUserProfileAndToken(HANDLE hToken,
                                   HANDLE hProfile)
{
   if (GuestAuthEnabled()) {
      g_debug("%s: special-case profile unload %p\n", __FUNCTION__, hProfile);
      if (!UnloadUserProfile(hToken, hProfile)) {
         g_warning("%s: UnloadUserProfile() failed %d\n",
                    __FUNCTION__, GetLastError());
      }
      CloseHandle(hToken);
   }
}
#endif // _WIN32

#endif // SUPPORT_VGAUTH

