/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
 * Win32Util.h --
 *
 *    misc Windows utilities
 */

#ifndef WIN32UTIL_H_
#define WIN32UTIL_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_atomic.h"
#include "unicodeTypes.h"

#ifdef _WIN32

#include <windows.h>

/* Type definitions */

typedef enum AutorunState {
   AUTORUN_UNDEFINED,
   AUTORUN_ON,
   AUTORUN_OFF
} AutorunState;

/* Defines */
#define VMX_SHUTDOWN_ORDER    0x100    // Application reserved last shutdown range.
#define UI_SHUTDOWN_ORDER     0x300    // Application reserved first shutdown range.
#define TOOLS_SHUTDOWN_ORDER  0x100    // Application reserved last shutdown range

/* Function declarations */

char * W32Util_GetInstallPath(void);
char * W32Util_GetInstallPath64(void);

/*
 * The string returned is allocated on the heap and must be freed by the
 * calling routine.
 */


char *W32Util_GetAppDataFilePath(const char *fileName);
char *W32Util_GetLocalAppDataFilePath(const char *fileName);

char *W32Util_GetCommonAppDataFilePath(const char *fileName);
char *W32Util_GetVmwareCommonAppDataFilePath(const char *fileName);

char *W32Util_GetMyDocumentPath(void);
char *W32Util_GetMyVideoPath(BOOL myDocumentsOnFail);

char *W32Util_GetDefaultVMPath(const char *pref);
char *W32Util_GetInstalledFilePath(const char *fileName);
char *W32Util_GetInstalledFilePath64(const char *fileName);

HKEY W32Util_OpenProductRegKey(REGSAM access);
HKEY W32Util_OpenUserRegKey(REGSAM access);

LPTOP_LEVEL_EXCEPTION_FILTER W32Util_SetUnhandledExceptionFilter(void);


Bool W32Util_CreateProcessArgv(const char *lpApplicationName,
                               const char **argv,
                               LPSECURITY_ATTRIBUTES lpProcessAttributes,
                               LPSECURITY_ATTRIBUTES lpThreadAttributes,
                               BOOL bInheritHandles,
                               DWORD dwCreationFlags,
                               LPVOID lpEnvironment,
                               const char *lpCurrentDirectory,
                               LPSTARTUPINFOA lpStartupInfo,
                               LPPROCESS_INFORMATION lpProcessInformation);

void W32Util_SplitCommandLine(char *commandLine,
                              int maxArgs,
                              char *progName,
                              int *argc,
                              char **argv);

BOOL W32Util_ReadFileTimeout(HANDLE hFile,                // handle of file to read
                             LPVOID lpBuffer,             // pointer to buffer that receives data
                             DWORD nNumberOfBytesToRead,  // number of bytes to read
                             LPDWORD lpNumberOfBytesRead, // pointer to number of bytes read
                             DWORD msTimeout);            // timeout in milliseconds

BOOL W32Util_WriteFileTimeout(HANDLE hFile,                    // handle to file to write to
                              LPCVOID lpBuffer,                // pointer to data to write to file
                              DWORD nNumberOfBytesToWrite,     // number of bytes to write
                              LPDWORD lpNumberOfBytesWritten,  // pointer to number of bytes written
                              DWORD msTimeout);                // timeout in milliseconds

char *W32Util_RealPath(const char *path);

BOOL W32Util_CheckForPrivilegeHeld(HANDLE token, LPCTSTR priv);

typedef BOOL (*SidFilterFunction)(PSID psid, void *cbData);
BOOL W32Util_GetEffectiveRightsForName(const char *user,
                                       const char *path,
                                       DWORD *rights);
BOOL W32Util_GetEffectiveRightsForSid(PSID psid,
                                      const char *path,
                                      DWORD *rights);

BOOL W32Util_ModifyRights(PSID psid,
                          const char *path,
                          DWORD rights,
                          BOOL isAllow);
void W32Util_FreeSids(PSID *sidList);
BOOL W32Util_GetMatchingSids(const char *path,
                             PSID **psidList,
                             SidFilterFunction matchCb,
                             void *cbData);

LPSTR W32Util_WideStrToAsciiStr(LPCWSTR wideStr);
LPWSTR W32Util_AsciiStrToWideStr(LPCSTR multiStr);
LPSTR W32Util_WideStrToMultiByteStr(LPCWSTR wideStr, UINT codePage);
LPWSTR W32Util_MultiByteStrToWideStr(LPCSTR multiStr, UINT codePage);

BOOL W32Util_WinSockAddReference(void);
BOOL W32Util_WinSockDereference(void);

Bool W32Util_RegisterService(Bool bRegister,
                             const char *name,
                             const char *displayName,
                             const char *description,
                             const char *binaryPath,
                             char **errString);

Bool W32Util_DebugService(const char *dbgFile);

Bool W32Util_RegisterEventLog(const char *serviceName,
                              Bool registryVolatile,
                              DWORD typesSupported,
                              const char *eventMsgFile,
                              const char *categoryMsgFile,
                              DWORD categoryCount,
                              const char *paramMsgFile);

Bool W32Util_UnregisterEventLog(const char *serviceName);

typedef enum SetSDPrivsAccounts {
   SDPRIV_GROUP_ADMIN = 0x1,
   SDPRIV_GROUP_VMWARE = 0x2,
   SDPRIV_USER_CURRENT = 0x4,
} SetSDPrivsAccounts;

Bool W32Util_SetSDPrivs(PSECURITY_DESCRIPTOR pSecurityDescriptor,
                        DWORD accessType, SetSDPrivsAccounts accounts,
                        PACL *pAcl, char **errString);

BOOL W32Util_SetSecurityDescriptorW(PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                    const char *Owner, PACL *pAcl);
BOOL W32Util_SetSecurityDescriptorSid(PSECURITY_DESCRIPTOR sd, PSID sid,
                                      PACL *pAcl);
BOOL W32Util_GetThreadHandle(HANDLE *handle);

Bool W32Util_AccessCheck(HANDLE token,
                         PSECURITY_DESCRIPTOR pSecurityDescriptor,
                         int desiredAccess);

Bool W32Util_HasAccessToFile(const char *filename, ACCESS_MASK desiredAccess,
                             HANDLE token);

Bool W32Util_GetSecurityDescriptor(const char *path,
                                   PSECURITY_DESCRIPTOR *ppSecurityDescriptor);

void W32Util_InitStdioConsole(void);
void W32Util_ExitStdioConsole(void);

Bool W32Util_CreateWellKnownSid(WELL_KNOWN_SID_TYPE wsdType,
                                PSID domainSid,
                                PSID *pSid);

Bool W32Util_GetCurrentUserSid(PSID *pSid);
Bool W32Util_GetLocalAdminGroupSid(PSID *pSid);
Bool W32Util_GetVMwareGroupSid(PSID *pSid);
Bool W32Util_MakeSafeDirectory(const char *path);
Bool W32Util_IsDirectorySafe(const char *path);
Bool W32Util_DoesVolumeSupportAcls(const char *path);

Bool W32Util_GetRegistryAutorun(AutorunState* state);
Bool W32Util_SetRegistryAutorun(const AutorunState state);

Bool W32Util_AllowAdminCOM(void);

char * W32Util_GetAppDataPath(void);
char * W32Util_RobustGetLongPath(const char *path);

typedef enum SecureObjectType {
   SecureObject_Process,
   SecureObject_Thread
} SecureObjectType;

PSECURITY_DESCRIPTOR W32Util_ConstructSecureObjectSD(HANDLE hToken,
                                                     SecureObjectType type);
Bool W32Util_ReplaceObjectSD(HANDLE hObject,
                             const PSECURITY_DESCRIPTOR pSD);

HMODULE W32Util_GetModuleByAddress(const void *addr);

Bool W32Util_VerifyXPModeHostLicense(void);

char * W32Util_GetPipeNameFromFilePath(const char *fileName);

Bool W32Util_CheckGroupMembership(HANDLE hToken,
                                  BOOL respectUAC,
                                  char * *errString,
                                  BOOL *bMember);

Bool W32Util_DismountVolumes(uint16 drive,
                             uint64 offset,
                             uint64 size,
                             void **handle);
Bool W32Util_CloseDismountHandle(void *handle);

Bool W32Util_EnableSafePathSearching(void);

CRITICAL_SECTION *W32Util_GetSingletonCriticalSection(Atomic_Ptr *csMemory);

#endif // _WIN32
#endif // WIN32UTIL_H_
