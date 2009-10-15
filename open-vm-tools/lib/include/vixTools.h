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
 * vixTools.h --
 *
 *    Vix Tools functionality.
 *
 */

#ifndef __VIX_TOOLS_H__
#define __VIX_TOOLS_H__


struct DblLnkLst_Links;
struct GuestApp_Dict;


typedef void (*VixToolsReportProgramDoneProcType)(const char *requestName,
                                                  VixError err,
                                                  int exitCode,
                                                  int64 pid,
                                                  void *clientData);

VixError VixTools_Initialize(Bool thisProcessRunsAsRootArg,
                             const char * const *originalEnvp,
                             VixToolsReportProgramDoneProcType reportProgramDoneProc,
                             void *clientData);

void VixTools_SetConsoleUserPolicy(Bool allowConsoleUserOpsParam);

void VixTools_SetRunProgramCallback(VixToolsReportProgramDoneProcType reportProgramDoneProc,
                                    void *clientData);

/*
 * These are internal procedures that are exposed for the legacy
 * tclo callbacks.
 */
VixError VixToolsRunProgramImpl(char *requestName,
                                char *commandLine,
                                char *commandLineArgs,
                                int runProgramOptions,
                                void *userToken,
                                void *eventQueue,
                                int64 *pid);

#if defined(VMTOOLS_USE_GLIB)
#  include <glib.h>

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

#else
VixError VixTools_GetToolsPropertiesImpl(struct GuestApp_Dict **confDictRef,
                                         char **resultBuffer,
                                         size_t *resultBufferLength);

VixError VixTools_ProcessVixCommand(VixCommandRequestHeader *requestMsg,
                                    char *requestName,
                                    size_t maxResultBufferSize,
                                    struct GuestApp_Dict **confDictRef,
                                    DblLnkLst_Links *eventQueue,
                                    char **resultBuffer,
                                    size_t *resultLen,
                                    Bool *deleteResultBufferResult);

#endif

Bool VixToolsImpersonateUserImpl(char const *credentialTypeStr, 
                                 int credentialType,
                                 char const *password,
                                 void **userToken);

void VixToolsUnimpersonateUser(void *userToken);

void VixToolsLogoutUser(void *userToken);

#ifdef _WIN32
VixError VixToolsGetUserTmpDir(void *userToken,
                               char **tmpDirPath);

Bool VixToolsUserIsMemberOfAdministratorGroup(VixCommandRequestHeader *requestMsg);
#endif // _WIN32

#if IMPLEMENT_SOCKET_MGR
VixError VixToolsSocketConnect(VixCommandRequestHeader *requestMsg,
                               char **result);

VixError VixToolsSocketListen(VixCommandRequestHeader *requestMsg,
                              char **result);

VixError VixToolsSocketAccept(VixCommandRequestHeader *requestMsg,
                              char *testName);

VixError VixToolsSocketSend(VixCommandRequestHeader *requestMsg,
                            char *testName);

VixError VixToolsSocketRecv(VixCommandRequestHeader *requestMsg,
                            char *testName);

VixError VixToolsSocketClose(VixCommandRequestHeader *requestMsg);
#endif // IMPLEMENT_SOCKET_MGR


#endif /* __VIX_TOOLS_H__ */


