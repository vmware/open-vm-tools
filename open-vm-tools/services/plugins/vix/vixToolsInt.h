/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 * vixToolsInt.h --
 *
 *      Helper routines shared between different files in the vixTools
 *      module.
 */

#ifndef __VIX_TOOLS_INT_H__
#define __VIX_TOOLS_INT_H__

#include "vmware.h"
#include "vix.h"
#include "vixCommands.h"
#include <glib.h>

/*
 * Needed for VGAuth support code.
 */
#ifdef _WIN32
#include "VGAuthCommon.h"
#include "VGAuthError.h"
#include "VGAuthAuthentication.h"
#include "VGAuthAlias.h"
#endif

#define PROCESS_CREATOR_USER_TOKEN       ((void *)1)

#ifdef _WIN32

#define VIX_TOOLS_MAX_SSPI_SESSIONS 50
#define VIX_TOOLS_MAX_TICKETED_SESSIONS 50

#endif

extern char *gImpersonatedUsername;
#define  IMPERSONATED_USERNAME   ((gImpersonatedUsername) ? gImpersonatedUsername : "Unset")

typedef struct VixToolsEnvIterator VixToolsEnvIterator;

typedef struct VixToolsUserEnvironment VixToolsUserEnvironment;

typedef void (*VixToolsReportProgramDoneProcType)(const char *requestName,
                                                  VixError err,
                                                  int exitCode,
                                                  int64 pid,
                                                  void *clientData);

VixError VixTools_Initialize(Bool thisProcessRunsAsRootArg,
                             const char * const *originalEnvp,
                             VixToolsReportProgramDoneProcType reportProgramDoneProc,
                             void *clientData);

void VixTools_Uninitialize(void);

#ifdef _WIN32
VixError VixToolsTranslateVGAuthError(VGAuthError vgErr);
#endif

VixError VixToolsImpersonateUser(VixCommandRequestHeader *requestMsg,
                                 Bool loadUserProfile,
                                 void **userToken);

void VixTools_SetConsoleUserPolicy(Bool allowConsoleUserOpsParam);

void VixTools_SetRunProgramCallback(VixToolsReportProgramDoneProcType reportProgramDoneProc,
                                    void *clientData);

gboolean VixTools_ConfigGetBoolean(GKeyFile *confDictRef,
                                   const char *group,
                                   const char *key,
                                   gboolean defValue);

void VixTools_RestrictCommands(gboolean restricted);

/*
 * These are internal procedures that are exposed for the legacy
 * tclo callbacks.
 */
VixError VixToolsRunProgramImpl(char *requestName,
                                const char *commandLine,
                                const char *commandLineArgs,
                                int runProgramOptions,
                                void *userToken,
                                void *eventQueue,
                                int64 *pid);

VixError VixTools_GetToolsPropertiesImpl(GKeyFile *confDictRef,
                                         char **resultBuffer,
                                         size_t *resultBufferLength);

VixError VixTools_ProcessVixCommand(VixCommandRequestHeader *requestMsg,
                                    char *requestName,
                                    size_t maxResultBufferSize,
                                    GKeyFile *confDictRef,
                                    GMainLoop *eventQueue,
                                    char **resultBuffer,
                                    size_t *resultLen,
                                    Bool *deleteResultBufferResult);

uint32 VixTools_GetAdditionalError(uint32 opCode,
                                   VixError error);

Bool VixToolsImpersonateUserImpl(char const *credentialTypeStr,
                                 int credentialType,
                                 char const *password,
                                 void **userToken);

void VixToolsUnimpersonateUser(void *userToken);

void VixToolsLogoutUser(void *userToken);

VixError VixToolsNewEnvIterator(void *userToken,
#ifdef __FreeBSD__
                                char **envp,
#endif
                                VixToolsEnvIterator **envItr);

char *VixToolsGetNextEnvVar(VixToolsEnvIterator *envItr);

void VixToolsDestroyEnvIterator(VixToolsEnvIterator *envItr);

VixError VixToolsNewUserEnvironment(void *userToken,
                                    VixToolsUserEnvironment **env);

char *VixToolsGetEnvFromUserEnvironment(const VixToolsUserEnvironment *env,
                                        const char *name);

void VixToolsDestroyUserEnvironment(VixToolsUserEnvironment *env);

VixError VixToolsValidateEnviron(char const * const *env);

char *VixToolsGetEnvVarFromEnvBlock(const wchar_t *envBlock,
                                    const char *envVarName);

char *VixToolsEscapeXMLString(const char *str);

#ifdef _WIN32
VixError VixToolsInitializeWin32();

Bool VixToolsGetUserName(wchar_t **userName);

VixError VixToolsGetEnvBlock(void *userToken,
                             wchar_t **envBlock);

Bool VixToolsDestroyEnvironmentBlock(wchar_t *envBlock);

VixError VixToolsEnvironToEnvBlock(char const * const *env,
                                   wchar_t **envBlock);

VixError VixToolsGetUserTmpDir(void *userToken,
                               char **tmpDirPath);

Bool VixToolsUserIsMemberOfAdministratorGroup(VixCommandRequestHeader *requestMsg);

void VixToolsDeinitSspiSessionList();
void VixToolsDeinitTicketedSessionList();


VixError VixToolsAuthenticateWithSSPI(VixCommandRequestHeader *requestMsg,
                                      GMainLoop *eventQueue,
                                      char **resultBuffer);

VixError VixToolsGetTokenHandleFromTicketID(const char *ticketID,
                                            char **username,
                                            HANDLE *hToken);

VixError VixToolsReleaseCredentialsImpl(VixCommandRequestHeader *requestMsg);

VixError VixToolsCreateRegKeyImpl(VixCommandRequestHeader *requestMsg);

VixError VixToolsListRegKeysImpl(VixCommandRequestHeader *requestMsg,
                                 size_t maxBufferSize,
                                 void *eventQueue,
                                 char **result);

VixError VixToolsDeleteRegKeyImpl(VixCommandRequestHeader *requestMsg);

VixError VixToolsSetRegValueImpl(VixCommandRequestHeader *requestMsg);

VixError VixToolsListRegValuesImpl(VixCommandRequestHeader *requestMsg,
                                   size_t maxBufferSize,
                                   void *eventQueue,
                                   char **result);

VixError VixToolsDeleteRegValueImpl(VixCommandRequestHeader *requestMsg);

gchar *VixToolsGetCurrentUsername(void);

VixError VixToolsCheckSAMLForSystem(VGAuthContext *ctx,
                                    VGAuthError origErr,
                                    const char *token,
                                    const char *username,
                                    char *serviceUsername,
                                    void **userToken,
                                    VGAuthUserHandle **curUserHandle);
#endif // _WIN32

#ifdef VMX86_DEVEL
void TestVixToolsEnvVars(void);
#endif

#endif // #ifndef __VIX_TOOLS_INT_H__
