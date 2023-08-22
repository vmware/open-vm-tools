/*********************************************************
 * Copyright (c) 2011-2017,2023 VMware, Inc. All rights reserved.
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

/**
 * @file serviceInt.h --
 *
 *    Service internal data types.
 */

#ifndef _SERVICEINT_H_
#define _SERVICEINT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "VGAuthAuthentication.h"
#include "VGAuthBasicDefs.h"
#include "VGAuthError.h"
#include "prefs.h"
#include "usercheck.h"
#include "audit.h"

#define VMW_TEXT_DOMAIN "VGAuthService"
#include "i18n.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
enum { BUFSIZE = 4096 };
#include "userAccessControl.h"
#endif

#ifdef _WIN32
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

/*
 * Use this for any informational messages, eg "service started".
 */
#define Log   g_message

/*
 * Use this for any error reporting, such as unexpected failures from APIs
 * or syscalls.
 */
#define Warning   g_warning

/*
 * Use this for any debugging messages.
 */
#define Debug  g_debug


/*
 * Set this to be horribly inefficient but to be sure that nothing
 * is assuming it will get a full packet as sent by a single syscall
 * on the other end.
 */
#define  NETWORK_FORCE_TINY_PACKETS 0

typedef struct ProtoRequest ProtoRequest;

#ifdef _WIN32
typedef enum _IOState {
   IO_IDLE = 0,
   IO_PENDING, /* need to wait an issued IO */
   IO_COMPLETE, /* need to process a completed IO */
   IO_BROKEN, /* The other end has closed the pipe handle */
} IOState;

typedef enum _PidVerifyState {
   PID_VERIFY_NONE = 0,    /* Initial state or the check is not needed */
   PID_VERIFY_PENDING, /* Wait for the check to complete */
   PID_VERIFY_GOOD,    /* The check completed successfully */
   PID_VERIFY_BAD      /* The check failed */
} PidVerifyState;
#endif

typedef struct _ServiceConnection {
   gboolean isPublic;

   char  *pipeName;
   char  *userName;

   /*
    * The request currently being processed
    */
   ProtoRequest *curRequest;
   GMarkupParseContext *parseContext;

#ifdef SUPPORT_TCP
   int port;
#endif

   /*
    * To be renamed to gTag
    * Windows: GSource Id returned from g_source_attach()
    * Linux: GSource id returned from g_io_add_watch()
    */
   guint gioId;

#ifdef _WIN32
   HANDLE hComm;
   OVERLAPPED ol;
   gchar readBuffer[BUFSIZE];
   int bytesRead;
   IOState ioState;
   UserAccessControl m_uac;
   DWORD pid;
   HANDLE hProc;
   HANDLE hChallengeEvent;
   HANDLE hChallengeEventDup;
   PidVerifyState pidVerifyState;
#else
   int sock;
#endif

   gboolean eof;
   int connId;
   gboolean isListener;

   /*
    * The last time a listen connection was used.
    */
   GTimeVal lastUse;
   gboolean dataConnectionIncremented;
} ServiceConnection;


typedef enum {
   SUBJECT_TYPE_NAMED,
   SUBJECT_TYPE_ANY,
   SUBJECT_TYPE_UNSET,     // special case for removing all subjects
} ServiceSubjectType;

/*
 * The service's idea of a Subject.
 */
typedef struct _ServiceSubject {
   ServiceSubjectType type;
   gchar *name;
} ServiceSubject;

/*
 * The service's idea of a AliasInfo.
 */
typedef struct _ServiceAliasInfo {
   ServiceSubjectType type;
   gchar *name;
   gchar *comment;
} ServiceAliasInfo;

/*
 * The service's idea of an Alias.
 */
typedef struct _ServiceAlias {
   gchar *pemCert;
   int num;
   ServiceAliasInfo *infos;
} ServiceAlias;

/*
 * The service's idea of an MappedAlias.
 */
typedef struct _ServiceMappedAlias {
   gchar *pemCert;
   int num;
   ServiceSubject *subjects;
   gchar *userName;
} ServiceMappedAlias;

/*
 * Possible types of validation.
 */
typedef enum {
   VALIDATION_RESULTS_TYPE_UNKNOWN,
   VALIDATION_RESULTS_TYPE_SAML,
   VALIDATION_RESULTS_TYPE_SSPI,
   VALIDATION_RESULTS_TYPE_NAMEPASSWORD,
   VALIDATION_RESULTS_TYPE_SAML_INFO_ONLY,
} ServiceValidationResultsType;

/*
 * Data associated with a userHandle validation.
 *
 * Right now this is just SAML data.  If we ever add more,
 * this could become a union.
 */
typedef struct _ServiceValidationResultsData {
   char *samlSubject;
   ServiceAliasInfo aliasInfo;
} ServiceValidationResultsData;


/*
 * Global vars
 */

extern PrefHandle gPrefs;
extern gboolean gVerboseLogging;

extern char *gInstallDir;

#ifndef _WIN32
gboolean ServiceNetworkCreateSocketDir(void);
#endif

VGAuthError ServiceNetworkListen(ServiceConnection *conn,            // IN/OUT
                                 gboolean makeSecure);

void ServiceInitListenConnectionPrefs(void);

void ServiceNetworkRemoveListenPipe(ServiceConnection *conn);

#ifdef _WIN32
VGAuthError ServiceNetworkStartRead(ServiceConnection *conn);        // IN/OUT
#endif

VGAuthError ServiceNetworkAcceptConnection(ServiceConnection *connIn,
                                           ServiceConnection *connOut);// IN/OUT

void ServiceNetworkCloseConnection(ServiceConnection *conn);

VGAuthError ServiceNetworkReadData(ServiceConnection *conn,
                                   gsize *len,                        // OUT
                                   gchar **data);                     // OUT

VGAuthError ServiceNetworkWriteData(ServiceConnection *conn,
                                    gsize len,
                                    gchar *data);

gboolean ServiceNetworkIsConnectionPrivateSuperUser(ServiceConnection *conn);

/*
 * May have to expose more of these.  This is all we need at this point.
 */
typedef VGAuthError (* ServiceStartListeningForIOFunc)(ServiceConnection *conn);
typedef VGAuthError (* ServiceStopListeningForIOFunc)(ServiceConnection *conn);

VGAuthError ServiceRegisterIOFunctions(ServiceStartListeningForIOFunc startFunc,
                                       ServiceStopListeningForIOFunc stopFunc);


/*
 * Ticket functions
 */

VGAuthError ServiceInitTickets(void);
void ServiceInitTicketPrefs(void);

#ifdef _WIN32
VGAuthError ServiceCreateTicketWin(const char *userName,
                                   ServiceValidationResultsType type,
                                   ServiceValidationResultsData *svData,
                                   HANDLE clientProcHandle,
                                   const char *token,
                                   char **ticket);
VGAuthError ServiceValidateTicketWin(const char *ticket,
                                     HANDLE clientProcHandle,
                                     char **userName,
                                     ServiceValidationResultsType *type,
                                     ServiceValidationResultsData **svData,
                                     char **token);
#else
VGAuthError ServiceCreateTicketPosix(const char *userName,
                                     ServiceValidationResultsType type,
                                     ServiceValidationResultsData *svData,
                                     char **ticket);
VGAuthError ServiceValidateTicketPosix(const char *ticket,
                                       char **userName,
                                       ServiceValidationResultsType *type,
                                       ServiceValidationResultsData **svData);
#endif
VGAuthError ServiceRevokeTicket(ServiceConnection *conn,
                                const char *ticket);
VGAuthError ServiceLookupTicketOwner(const char *ticket,
                                     char **userName);


/*
 * Alias functions
 */
VGAuthError ServiceAliasInitAliasStore(void);

VGAuthError ServiceAliasAddAlias(const gchar *reqUserName,
                                 const gchar *userName,
                                 gboolean addMapped,
                                 const gchar *pemCert,
                                 ServiceAliasInfo *ai);

VGAuthError ServiceAliasRemoveAlias(const gchar *reqUserName,
                                    const gchar *userName,
                                    const gchar *pemCert,
                                    ServiceSubject *subj);

VGAuthError ServiceAliasQueryAliases(const gchar *userName,
                                     int *num,
                                     ServiceAlias **aList);

VGAuthError ServiceAliasQueryMappedAliases(int *num,
                                           ServiceMappedAlias **maList);

void ServiceAliasFreeAliasList(int num, ServiceAlias *aList);

void ServiceAliasFreeAliasInfo(ServiceAliasInfo *ai);

void ServiceAliasFreeAliasInfoContents(ServiceAliasInfo *ai);

void ServiceAliasCopyAliasInfoContents(const ServiceAliasInfo *src,
                                       ServiceAliasInfo *dst);

void ServiceAliasFreeMappedAliasList(int num, ServiceMappedAlias *maList);

gboolean ServiceAliasIsSubjectEqual(ServiceSubjectType t1,
                                    ServiceSubjectType t2,
                                    const gchar *n1,
                                    const gchar *n2);

gboolean ServiceComparePEMCerts(const gchar *pemCert1,
                                const gchar *pemCert2);

/*
 * Connection functions
 */
void ServiceConnectionShutdown(ServiceConnection *conn);

VGAuthError ServiceConnectionClone(ServiceConnection *parent,
                                   ServiceConnection **clone);       // OUT

VGAuthError ServiceCreatePublicConnection(ServiceConnection **returnConn);// OUT

VGAuthError ServiceCreateUserConnection(const char *userName,
                                        ServiceConnection **returnConn); // OUT


/*
 * Protocol parsing and generation.
 */
VGAuthError ServiceProtoReadAndProcessRequest(ServiceConnection *conn);

VGAuthError ServiceProtoDispatchRequest(ServiceConnection *conn,
                                        ProtoRequest *req);

void ServiceProtoCleanupParseState(ServiceConnection *conn);

VGAuthError ServiceStartUserConnection(const char *userName,
                                       char **pipeName);       // OUT

VGAuthError ServiceProtoHandleSessionRequest(ServiceConnection *conn,
                                             ProtoRequest *req);

VGAuthError ServiceProtoHandleConnection(ServiceConnection *conn,
                                         ProtoRequest *req);

VGAuthError ServiceProtoAddAlias(ServiceConnection *conn,
                                   ProtoRequest *req);

VGAuthError ServiceProtoRemoveAlias(ServiceConnection *conn,
                                    ProtoRequest *req);

VGAuthError ServiceProtoQueryAliases(ServiceConnection *conn,
                                     ProtoRequest *req);

VGAuthError ServiceProtoQueryMappedAliases(ServiceConnection *conn,
                                           ProtoRequest *req);

VGAuthError ServiceProtoCreateTicket(ServiceConnection *conn,
                                     ProtoRequest *req);

VGAuthError ServiceProtoValidateTicket(ServiceConnection *conn,
                                       ProtoRequest *req);

VGAuthError ServiceProtoRevokeTicket(ServiceConnection *conn,
                                     ProtoRequest *req);

/*
 * File wrapper functions.
 */
int ServiceFileRenameFile(const gchar *srcName,
                          const gchar *dstName);

int ServiceFileUnlinkFile(const gchar *fileName);

#ifdef _WIN32
int ServiceFileWinMakeTempfile(gchar **fileName,
                               UserAccessControl *uac);
#else
int ServiceFilePosixMakeTempfile(gchar *fileName,
                                 int mode);
#endif

#ifndef _WIN32
VGAuthError ServiceFileSetOwner(const gchar *fileName,
                                const char *userName);
#endif

VGAuthError ServiceFileCopyOwnership(const gchar *srcFilename,
                                     const gchar *dstFilename);

int ServiceFileMakeDirTree(const gchar *dirName,
                           int mode);

#ifndef _WIN32
int ServiceFileSetPermissions(const gchar *fileName,
                              int mode);

int ServiceFileGetPermissions(const gchar *fileName,
                              int *mode);
#endif

#ifdef _WIN32
VGAuthError ServiceFileVerifyAdminGroupOwned(const char *fileName);
VGAuthError ServiceFileVerifyEveryoneReadable(const char *fileName);
VGAuthError ServiceFileVerifyUserAccess(const char *fileName,
                                        const char *userName);
VGAuthError ServiceFileVerifyAdminGroupOwnedByHandle(const HANDLE hFile);
VGAuthError ServiceFileVerifyEveryoneReadableByHandle(const HANDLE hFile);
VGAuthError ServiceFileVerifyUserAccessByHandle(const HANDLE hFile,
                                                const char *userName);
#else
VGAuthError ServiceFileVerifyFileOwnerAndPerms(const char *fileName,
                                               const char *userName,
                                               int mode,
                                               uid_t *uidRet,
                                               gid_t *gidRet);
#endif

VGAuthError ServiceRandomBytes(int size,
                               guchar *buffer);


#ifdef _WIN32
VGAuthError ServiceStartVerifyPid(ServiceConnection *conn,
                                  const char *pidInText,
                                  char **event);
VGAuthError ServiceEndVerifyPid(ServiceConnection *conn);
#endif


VGAuthError ServiceVerifyAndCheckTrustCertChainForSubject(int numCerts,
                                                          const char **pemCertChain,
                                                          const char *userName,
                                                          ServiceSubject *subj,
                                                          char **userNameOut,
                                                          ServiceAliasInfo **verifyAi);


VGAuthError ServiceInitVerify(void);


void Service_ReloadPrefs(void);
void Service_Shutdown(void);

gchar *ServiceEncodeUserName(const char *userName);
gchar *ServiceDecodeUserName(const char *userName);

VGAuthError SAML_Init(void);

/* clang-format off */
VGAuthError SAML_VerifyBearerTokenEx(const char *xmlText,
                                     const char *userName,
                                     gboolean hostVerified,
                                     char **userNameOut,
                                     char **subjectNameOut,
                                     ServiceAliasInfo **verifyAi);
VGAuthError SAML_VerifyBearerToken(const char *xmlText,
                                   const char *userName,
                                   char **userNameOut,
                                   char **subjectNameOut,
                                   ServiceAliasInfo **verifyAi);
VGAuthError SAML_VerifyBearerTokenAndChain(const char *xmlText,
                                           const char *userName,
                                           gboolean hostVerified,
                                           char **userNameOut,
                                           char **subjectNameOut,
                                           ServiceAliasInfo **verifyAi);
/* clang-format on */

void SAML_Shutdown(void);
void SAML_Reload(void);

void ServiceFreeValidationResultsData(ServiceValidationResultsData *samlData);

WIN32_ONLY(gboolean ServiceOldInstanceExists(void);)

void ServiceReplyTooManyConnections(ServiceConnection *conn,
                                    int connLimit);

VGAuthError ServiceAcceptConnection(ServiceConnection *lConn,
                                    ServiceConnection *newConn);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif   // _SERVICEINT_H_
