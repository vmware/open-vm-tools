/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
 * @file hgfsPlugin.c
 *
 * Functionality to utilize the hgfs server in bora/lib as a tools plugin.
 */

#if defined(_WIN32)
#include <windows.h>
#include <strsafe.h>
#include <aclapi.h>
#endif // defined(_WIN32)
#include <string.h>

#define G_LOG_DOMAIN "hgfsd"

#include "hgfs.h"
#include "hgfsServerManager.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "vm_vmx_type.h"
#include "vmcheck.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"

#if defined(_WIN32)
#include "hgfsWinNtInternal.h"  // For reconnection of drives

#define NET_BUFFER_SIZE        3000
#endif // defined(_WIN32)

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

#if defined(_WIN32)
typedef enum HgfsClientRdrServiceOp {
   HGFS_CLIENTRDR_SERVICE_START = 0,
   HGFS_CLIENTRDR_SERVICE_QUERY_STARTED,
} HgfsClientRdrServiceOp;

static void HgfsServerDestroyClientRdrSA(PSID *everyoneSID,
                                         PSID *adminSID,
                                         PACL *accessControlList,
                                         PSECURITY_DESCRIPTOR *securityDescriptor);

static DWORD HgfsServerClientRdrWaitForEvent(DWORD millSecTimeout);
static void HgfsServerClientRdrSetEvent(void);
#endif // defined(_WIN32)
static void HgfsServerCloseClientRdrEvent(void);

/**
 * Clean up internal state on shutdown.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  plugin   Plugin registration data.
 */

static void
HgfsServerShutdown(gpointer src,
                   ToolsAppCtx *ctx,
                   ToolsPluginData *plugin)
{
   HgfsServerMgrData *mgrData = plugin->_private;
   HgfsServerManager_Unregister(mgrData);
   g_free(mgrData);
   HgfsServerCloseClientRdrEvent();
}


/**
 * Handles hgfs requests.
 *
 * @param[in]  data  RPC request data.
 *
 * @return TRUE on success, FALSE on error.
 */

static gboolean
HgfsServerRpcDispatch(RpcInData *data)
{
   HgfsServerMgrData *mgrData;
   size_t replySize;
   static char reply[HGFS_LARGE_PACKET_MAX];


   ASSERT(data->clientData != NULL);
   mgrData = data->clientData;

   if (data->argsSize == 0) {
      return RPCIN_SETRETVALS(data, "1 argument required", FALSE);
   }

   replySize = sizeof reply;
   HgfsServerManager_ProcessPacket(mgrData, data->args + 1, data->argsSize - 1, reply, &replySize);

   data->result = reply;
   data->resultLen = replySize;
   return TRUE;
}


/**
 * Sends the HGFS capability to the VMX.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  set      Whether setting or unsetting the capability.
 * @param[in]  data     Unused.
 *
 * @return NULL. The function sends the capability directly.
 */

static GArray *
HgfsServerCapReg(gpointer src,
                 ToolsAppCtx *ctx,
                 gboolean set,
                 ToolsPluginData *plugin)
{
   gchar *msg;
   const char *appName = NULL;

   if (TOOLS_IS_MAIN_SERVICE(ctx)) {
      appName = TOOLS_DAEMON_NAME;
   } else if (TOOLS_IS_USER_SERVICE(ctx)) {
      appName = TOOLS_DND_NAME;
   } else {
      NOT_REACHED();
   }

   msg = g_strdup_printf("tools.capability.hgfs_server %s %d",
                         appName,
                         set ? 1 : 0);

   /*
    * Prior to WS55, the VMX did not know about the "hgfs_server"
    * capability. This doesn't mean that the HGFS server wasn't needed, it's
    * just that the capability was introduced in CS 225439 so that the VMX
    * could decide which HGFS server to communicate with.
    *
    * Long story short, we shouldn't care if this function fails.
    */
   if (ctx->rpc && !RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
      g_warning("Setting HGFS server capability failed!\n");
   }

   g_free(msg);
   return NULL;
}


#if defined(_WIN32)

#define HGFS_CLIENT_START_EVENT_NAME   L"HGFS_CLIENT_START_EVENT"
#define LOCAL_PREFIX                   L"Local\\%s"
#define GLOBAL_PREFIX                  L"Global\\%s"

static HANDLE gHgfsServerStartClientEvent = NULL;


/**
 * Starts the client driver service.
 *
 * @param[in]  serviceControlManager   Service Control Manager handle.
 * @param[in]  accessFlags             Service desired access.
 * @param[in]  serviceName             Name of the driver service to start.
 *
 * @return ERROR_SUCCESS on success, an appropriate error otherwise.
 */

static DWORD
HgfsServerStartClientService(SC_HANDLE serviceControlManager,
                             DWORD accessFlags,
                             PCWSTR serviceName)
{
   SC_HANDLE   service;
   DWORD       result = ERROR_SUCCESS;

   g_info("%s: Info: start service %S\n", __FUNCTION__, serviceName);

   /*
    * Open the handle to the existing service.
    */
   service = OpenServiceW(serviceControlManager, serviceName, accessFlags);
   if (NULL == service) {
      result = GetLastError();
      g_warning("%s: Error: open service %S = %d \n", __FUNCTION__, serviceName, result);
      goto exit;
   }

   /*
    * Start the execution of the service (i.e. start the driver).
    */
   if (!StartServiceW(service, 0, NULL)) {
      result = GetLastError();

      if (ERROR_SERVICE_ALREADY_RUNNING == result) {
         result = ERROR_SUCCESS;
      } else {
         g_warning("%s: Error: start service %S = %d \n", __FUNCTION__, serviceName, result);
      }
      goto exit;
   }

exit:
   if (NULL != service) {
      CloseServiceHandle(service);
   }

   g_info("%s: Info: Done %S: %u\n", __FUNCTION__, serviceName, result);
   return result;
}


/**
 * Query the client driver service status.
 *
 * @param[in]  serviceControlManager   Service Control Manager handle.
 * @param[in]  accessFlags             Service desired access.
 * @param[in]  serviceName             Name of the driver service to query.
 * @param[out] currentState            Current state of the driver service.
 *
 * @return ERROR_SUCCESS on success, an appropriate error otherwise.
 */

static DWORD
HgfsServerQueryClientService(SC_HANDLE serviceControlManager,
                             DWORD accessFlags,
                             PCWSTR serviceName,
                             DWORD *currentState)
{
   SC_HANDLE      service;
   SERVICE_STATUS serviceStatus;
   DWORD          result = ERROR_SUCCESS;

   g_info("%s: query service %S\n", __FUNCTION__, serviceName);

   /*
    * Open the handle to the existing service.
    */
   service = OpenServiceW(serviceControlManager, serviceName, accessFlags);
   if (NULL == service) {
      result = GetLastError();
      g_warning("%s: Error: open service %S = %u\n", __FUNCTION__, serviceName, result);
      goto exit;
   }

   /*
    * Query the status of the service (i.e. is it stopped or started).
    */
   if (!QueryServiceStatus(service, &serviceStatus)) {
      result = GetLastError();
      g_warning("%s: Error: start service %S = %u\n", __FUNCTION__, serviceName, result);
      goto exit;
   }

   *currentState = serviceStatus.dwCurrentState;

exit:
   if (NULL != service) {
      CloseServiceHandle(service);
   }

   g_info("%s: Done %S: %u\n", __FUNCTION__, serviceName, result);
   return result;
}


/**
 * Service the HGFS client redirector.
 *
 * @param[in]  serviceOp      Service operation to perform - start or query.
 * @param[in]  accessFlags    Service access flags.
 *
 * @return ERROR_SUCCESS or an appropriate error.
 */

static DWORD
HgfsServerClientRedirectorExecOpImpl(HgfsClientRdrServiceOp serviceOp,
                                     DWORD accessFlags)
{
   SC_HANDLE   serviceControlManager = NULL;
   PCWSTR      serviceName = HGFS_SERVICE_NAME_U;
   DWORD       result = ERROR_SUCCESS;

   g_info("%s: Info: Op %d on client redirector %S\n",
          __FUNCTION__, serviceOp, serviceName);

   serviceControlManager = OpenSCManagerW(NULL, NULL, accessFlags);
   if (NULL == serviceControlManager) {
      result = GetLastError();
      g_warning("%s: Error: Open SC Manager = %u\n", __FUNCTION__, result);
      goto exit;
   }

   switch (serviceOp) {
      case HGFS_CLIENTRDR_SERVICE_START:
         result = HgfsServerStartClientService(serviceControlManager,
                                               accessFlags,
                                               serviceName);
         break;
      case HGFS_CLIENTRDR_SERVICE_QUERY_STARTED: {
         DWORD currentState;
         result = HgfsServerQueryClientService(serviceControlManager,
                                               accessFlags,
                                               serviceName,
                                               &currentState);
         if (ERROR_SUCCESS == result) {
            if (SERVICE_RUNNING == currentState ||
               SERVICE_START_PENDING == currentState) {
               result = ERROR_SUCCESS;
            } else {
               /* Queried but not actually starting or started. */
               result = ERROR_SERVICE_NOT_ACTIVE;
            }
         }
         break;
      }
      default:
      NOT_REACHED();
   }

exit:
   if (NULL != serviceControlManager) {
      CloseServiceHandle(serviceControlManager);
   }
   g_info("%s: Info: Op %d Done %u\n", __FUNCTION__, serviceOp, result);
   return result;
}


/**
 * Service the HGFS client redirector.
 *
 * @param[in]  serviceOp      Service operation to perform - start or query.
 *
 * @return ERROR_SUCCESS or an appropriate error.
 */

static DWORD
HgfsServerClientRedirectorExecOp(HgfsClientRdrServiceOp serviceOp)
{
   DWORD       result = ERROR_SUCCESS;
   DWORD       accessFlags = 0;

   g_info("%s: Info: Service client redirector op %d\n",
          __FUNCTION__, serviceOp);

   switch (serviceOp) {
      case HGFS_CLIENTRDR_SERVICE_START:
         accessFlags |= SERVICE_START;
         break;
      case HGFS_CLIENTRDR_SERVICE_QUERY_STARTED:
         accessFlags |= SERVICE_QUERY_STATUS;
         break;
      default:
         NOT_REACHED();
   }

   result = HgfsServerClientRedirectorExecOpImpl(serviceOp,
                                                 accessFlags);

   g_info("%s: Info: Op %d Done %u\n", __FUNCTION__, serviceOp, result);
   return result;
}


/*
 * The calback is called for each network resource that the VMware Shared Folders
 * provides to the user.
 */

typedef BOOL
HgfsServerNetResourceCb(NETRESOURCEW *netRes);

/**
 * Enumerates all mapped network devices matching on the HGFS provider name.
 *
 * Any remembered disk network resources are passed to the callback.
 *
 * @param[in]  hgfsProvider  HGFS provider name.
 * @param[in]  hgfsResCb     Callback to process HGFS resource.
 *
 * @return TRUE on success and matched a network resource provided by
 *         the VMware Shared Folders client, FALSE otherwise.
 */

static BOOL
HgfsServerEnumerateDrives(LPCWSTR hgfsProvider,
                          HgfsServerNetResourceCb *hgfsResCb)
{
   BOOL success = FALSE;
   HANDLE enumHandle = INVALID_HANDLE_VALUE;
   DWORD callResult;

   callResult = WNetOpenEnum(RESOURCE_REMEMBERED,
                             RESOURCETYPE_DISK,
                             0 /*ignored*/,
                             NULL /*MBZ*/,
                             &enumHandle);

   if (callResult != NO_ERROR) {
      g_warning("%s: Failed to enumerate drives: %u\n", __FUNCTION__, callResult);
      return FALSE;
   }

   for (;;) {
      char buffer[NET_BUFFER_SIZE];
      NETRESOURCEW *netRes = (NETRESOURCEW *)&buffer;
      DWORD netResLen = sizeof buffer;
      DWORD count = 1;

      callResult = WNetEnumResource(enumHandle, &count, netRes, &netResLen);
      if (callResult != NO_ERROR || count != 1) {
         if (callResult != ERROR_NO_MORE_ITEMS) {
            g_warning("%s: Enumeration failed with %u %u\n", __FUNCTION__,
                     callResult, count);
         }
         break;
      }

      if (netRes->lpLocalName != NULL) {
         /*
          * If the local path is a drive letter - now check the provider.
          */
         if (hgfsProvider != NULL &&
             _wcsicmp(netRes->lpProvider, hgfsProvider) == 0) {

            g_info("%s: Processing %S -> %S\n", __FUNCTION__,
                   netRes->lpLocalName, netRes->lpRemoteName);

            if (hgfsResCb(netRes)) {
               success = TRUE;
            }
         }
      }
   }

   WNetCloseEnum(enumHandle);

   return success;
}


/**
 * Reconnect the HGFS provided network resource.
 *
 * @param[in]  netRes   VMware Shared Folders netork resource to connect.
 *
 * @return TRUE on success and matched a network resource provided by
 *         the VMware Shared Folders client, FALSE otherwise.
 */

static BOOL
HgfsServerReconnectNetResource(NETRESOURCEW *netRes)
{
   DWORD remoteNameCharSize = wcslen(netRes->lpRemoteName) + 1;
   DWORD callResult;
   BOOL isConnected = FALSE;

   g_info("%s: Get connection for %S -> %S\n", __FUNCTION__,
            netRes->lpLocalName, netRes->lpRemoteName);

   /* Get the current connection state of the resource. */
   callResult = WNetGetConnectionW(netRes->lpLocalName,
                                   netRes->lpRemoteName,
                                   &remoteNameCharSize);

   if (callResult == NO_ERROR) {
      /* Network resource is connected. */
      isConnected = TRUE;
      goto exit;
   }

   /* Network resource is remembered but not connected. */
   if (callResult == ERROR_CONNECTION_UNAVAIL) {
      /* Found a disconnected VMware Shared Folders network resource. */
      callResult = WNetAddConnection2W(netRes,
                                       NULL,
                                       NULL,
                                       0);
      g_info("%s: Reconnection of %S to %S -> %u\n", __FUNCTION__,
               netRes->lpLocalName, netRes->lpRemoteName, callResult);
      if (callResult == NO_ERROR) {
         isConnected = TRUE;
      }
   }

exit:
   g_info("%s: Connection %S to %S is %u\n", __FUNCTION__,
           netRes->lpLocalName, netRes->lpRemoteName, callResult);
   return isConnected;
}


/**
 * Reads a value from the registry.
 *
 * @param[in]    hInitialKey   initial key to start traversing from
 * @param[in]    subKey        name of subkey to open
 * @param[in]    value         name of the registry value
 * @param[inout] outBufSize  size of output buffer/result
 * @param[out]   type          type of value
 * @param[out]   outputBuffer  contents of value
 *
 * @return TRUE on success and retrieved the registry value,
 *         FALSE otherwise.
 */

static DWORD
HgfsServerGetRegistryValue(HKEY hInitialKey,
                           LPCWSTR subKey,
                           LPCWSTR value,
                           LPDWORD outBufSize,
                           LPDWORD type,
                           LPWSTR outputBuffer)
{
   DWORD regResult;
   regResult = RegGetValueW(hInitialKey,
                            subKey,
                            value,
                            RRF_RT_REG_SZ,
                            type,
                            (LPBYTE)outputBuffer,
                            outBufSize);
   if (regResult != ERROR_SUCCESS) {
      g_warning("%s: Error: querying value %S %u\n", __FUNCTION__,
                value, regResult);
   }

   return regResult;
}


/**
 * Gets the VMWare Shared Folders provider name.
 *
 * @param[inout]  hgfsProviderName        Shared Folders provider name buffer.
 * @param[inout]  hgfsProviderNameSize    Shared Folders provider name buffer size.
 *
 * @return TRUE on success and matched a network resource provided by
 *         the VMware Shared Folders client, FALSE otherwise.
 */

static BOOL
HgfsServerGetProviderName(LPWSTR hgfsProviderName,
                          DWORD *hgfsProviderNameSize)
{
   DWORD regKeyType;
   BOOL result = TRUE;

   /* Get the provider name from the Shared Folders client. */
   if (HgfsServerGetRegistryValue(HKEY_LOCAL_MACHINE,
                                  HGFS_PROVIDER_REGISTRY_KEY,
                                  HGFS_PROVIDER_VALUE_NAME,
                                  hgfsProviderNameSize,
                                  &regKeyType,
                                  hgfsProviderName) != ERROR_SUCCESS) {
      g_warning("%s: Error: querying registry shared folders not installed\n",
                __FUNCTION__);
      result = FALSE;
   }
   return result;
}


/**
 * Reconnect the HGFS mapped drives.
 *
 * @return None.
 */

static void
HgfsServerReconnectDrives(void)
{
   WCHAR hgfsProviderName[MAX_PATH] = {0};
   DWORD hgfsProviderNameSize = sizeof hgfsProviderName;

   g_info("%s: Start connecting drives\n", __FUNCTION__);

   /* Obtain the HGFS network provider name. */
   if (HgfsServerGetProviderName(hgfsProviderName,
                                 &hgfsProviderNameSize)) {
      /*
       * Scan the network resources provided by the HGFS network provider.
       * Check if they are connected or try to reconnect them.
       */
      if (HgfsServerEnumerateDrives(hgfsProviderName,
                                    HgfsServerReconnectNetResource)) {
         g_info("%s: Reconnected %S provides drives\n", __FUNCTION__, hgfsProviderName);
      }
   }
}


/**
 * Create the security attributes for Everyone and the Admininstrators group.
 *
 * @param[out]   everyoneSID        Everyone SID.
 * @param[out]   adminSID           Administrators group SID.
 * @param[out]   accessControlList  ACL.
 * @param[out]   securityDescriptor Security descriptor.
 * @param[out]   ea                 Explicit access array.
 * @param[in]    numEAEntries       Explicit access array size.
 * @param[inout] sa                 Security attributes.
 *
 * @return ERROR_SUCCESS on success and an appropriate error otherwise.
 */

static DWORD
HgfsServerCreateClientRdrSA(PSID *everyoneSID,
                            PSID *adminSID,
                            PACL *accessControlList,
                            PSECURITY_DESCRIPTOR *securityDescriptor,
                            EXPLICIT_ACCESSW *ea,
                            ULONG numEAEntries,
                            SECURITY_ATTRIBUTES *sa)
{
   SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
   SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;
   DWORD result;

   /* Initialize the out arguments. */
   *everyoneSID = NULL;
   *adminSID = NULL;
   *accessControlList = NULL;
   *securityDescriptor = NULL;
   ZeroMemory(ea, numEAEntries * sizeof *ea);
   ZeroMemory(sa, sizeof *sa);

   /* Create a well-known SID for the Everyone group. */
   if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                                 SECURITY_WORLD_RID,
                                 0, 0, 0, 0, 0, 0, 0,
                                 everyoneSID)) {
      result = GetLastError();
      g_warning("%s: Error: AllocateAndInitializeSid %u\n",
                __FUNCTION__, result);
      goto cleanup;
   }

   /*
    * Initialize an EXPLICIT_ACCESS structure for an ACE.
    * The ACE will allow Everyone read access to the key.
    */
   ea->grfAccessPermissions = SYNCHRONIZE | EVENT_MODIFY_STATE;
   ea->grfAccessMode = SET_ACCESS;
   ea->grfInheritance= NO_INHERITANCE;
   ea->Trustee.TrusteeForm = TRUSTEE_IS_SID;
   ea->Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
   ea->Trustee.ptstrName  = (LPWSTR) *everyoneSID;

   /* Create a SID for the BUILTIN\Administrators group. */
   if (!AllocateAndInitializeSid(&SIDAuthNT, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 adminSID)) {
      result = GetLastError();
      g_warning("%s: Error: AllocateAndInitializeSid %u\n",
                __FUNCTION__, GetLastError());
      goto cleanup;
   }

   if (numEAEntries == 2) {
      /*
       * Initialize an EXPLICIT_ACCESS structure for an ACE.
       * The ACE will allow the Administrators group full access to
       * the key.
       */
      (ea+1)->grfAccessPermissions =  EVENT_ALL_ACCESS;
      (ea+1)->grfAccessMode = SET_ACCESS;
      (ea+1)->grfInheritance= NO_INHERITANCE;
      (ea+1)->Trustee.TrusteeForm = TRUSTEE_IS_SID;
      (ea+1)->Trustee.TrusteeType = TRUSTEE_IS_GROUP;
      (ea+1)->Trustee.ptstrName  = (LPWSTR) *adminSID;
   }

   /* Create a new ACL that contains the new ACEs. */
   result = SetEntriesInAclW(numEAEntries, ea, NULL, accessControlList);
   if (ERROR_SUCCESS != result) {
      result = GetLastError();
      g_warning("%s: Error: SetEntriesInAcl Error %u\n", __FUNCTION__, result);
      goto cleanup;
   }

   /* Initialize a security descriptor. */
   *securityDescriptor = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
   if (NULL == *securityDescriptor) {
      result = GetLastError();
      g_warning("%s: Error: LocalAlloc Error %u\n", __FUNCTION__, result);
      goto cleanup;
   }

   if (!InitializeSecurityDescriptor(*securityDescriptor,
                                     SECURITY_DESCRIPTOR_REVISION)) {
      result = GetLastError();
      g_warning("%s: Error: InitializeSecurityDescriptor Error %u\n",
               __FUNCTION__, result);
      goto cleanup;
   }

   /* Add the ACL to the security descriptor. */
   if (!SetSecurityDescriptorDacl(*securityDescriptor,
                                  TRUE,     // dacl present
                                  *accessControlList,
                                  FALSE)) {
      result = GetLastError();
      g_warning("%S: Error: SetSecurityDescriptorDacl Error %u\n",
               __FUNCTION__, result);
      goto cleanup;
   }

   /* Initialize a security attributes structure. */
   sa->nLength = sizeof *sa;
   sa->lpSecurityDescriptor = *securityDescriptor;
   sa->bInheritHandle = FALSE;
   result = ERROR_SUCCESS;

cleanup:
   if (ERROR_SUCCESS != result) {
      HgfsServerDestroyClientRdrSA(everyoneSID,
                                   adminSID,
                                   accessControlList,
                                   securityDescriptor);
      sa->lpSecurityDescriptor = NULL;
      sa->nLength = 0;
      ZeroMemory(ea, numEAEntries * sizeof *ea);
   }
   return result;
}


/**
 * Destroy all the security attributes resources.
 *
 * @param[inout]  everyoneSID        Everyone SID.
 * @param[inout]  adminSID           Administrators group SID.
 * @param[inout]  accessControlList  ACL.
 * @param[inout]  securityDescriptor Security descriptor.
 *
 * @return None
 */

static void
HgfsServerDestroyClientRdrSA(PSID *everyoneSID,
                             PSID *adminSID,
                             PACL *accessControlList,
                             PSECURITY_DESCRIPTOR *securityDescriptor)
{
   if (*everyoneSID) {
      FreeSid(*everyoneSID);
      *everyoneSID = NULL;
   }
   if (*adminSID) {
      FreeSid(*adminSID);
      *adminSID = NULL;
   }
   if (*accessControlList) {
      LocalFree(*accessControlList);
      *accessControlList = NULL;
   }
   if (*securityDescriptor) {
      LocalFree(*securityDescriptor);
      *securityDescriptor = NULL;
   }
}


/**
 * Create the client start synchronization lock.
 *
 * @param[in]  syncEventName       Synchronization event name.
 *
 * @return ERROR_SUCCESS on success, an error otherwise
 */

static DWORD
HgfsServerClientRdrCreateEvent(LPCWSTR syncEventName)
{
   DWORD result = ERROR_SUCCESS;
   PSID everyoneSID = NULL, adminSID = NULL;
   PACL accessControlList = NULL;
   PSECURITY_DESCRIPTOR securityDescriptor = NULL;
   EXPLICIT_ACCESSW ea[2];
   SECURITY_ATTRIBUTES sa;

   ASSERT(NULL == gHgfsServerStartClientEvent);
   ASSERT(NULL != syncEventName);

   result = HgfsServerCreateClientRdrSA(&everyoneSID,
                                        &adminSID,
                                        &accessControlList,
                                        &securityDescriptor,
                                        &ea[0],
                                        ARRAYSIZE(ea),
                                        &sa);
   if (ERROR_SUCCESS != result) {
      /* Error result already logged. */
      goto exit;
   }

   gHgfsServerStartClientEvent = CreateEventW(&sa, TRUE, FALSE, syncEventName);
   if (NULL == gHgfsServerStartClientEvent) {
      result = GetLastError();
      g_warning("%s: Error: Creating %S = %u\n", __FUNCTION__, syncEventName, result);
   }

   HgfsServerDestroyClientRdrSA(&everyoneSID,
                                 &adminSID,
                                 &accessControlList,
                                 &securityDescriptor);

exit:
   return result;
}


/**
 * Open the client start synchronization event.
 *
 * @param[in]  syncEventName       Synchronization event name.
 *
 * @return ERROR_SUCCESS on success, an error otherwise
 */

static uint32
HgfsServerClientRdrOpenEvent(LPCWSTR syncEventName)
{
   uint32 result = ERROR_SUCCESS;

   ASSERT(NULL == gHgfsServerStartClientEvent);
   ASSERT(NULL != syncEventName);

   gHgfsServerStartClientEvent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE,
                                            FALSE,
                                            syncEventName);
   if (NULL == gHgfsServerStartClientEvent) {
      result = GetLastError();
      g_warning("%s: Error: Opening %S = %u\n", __FUNCTION__, syncEventName, result);
   }

   return result;
}


/**
 * Wait for the client start synchronization event.
 *
 * @param[in]  millisecTimeout     Wait time out in ms.
 *
 * @return WAIT_OBJECT_0 or a wait error
 */

static DWORD
HgfsServerClientRdrWaitForEvent(DWORD millisecTimeout)
{
   DWORD result;
   ASSERT(NULL != gHgfsServerStartClientEvent);

   if (NULL != gHgfsServerStartClientEvent) {
      result = WaitForSingleObject(gHgfsServerStartClientEvent,
                                   millisecTimeout);
      if (result != WAIT_OBJECT_0) {
         g_warning("%s: Error: Wait for event = %u\n", __FUNCTION__, result);
      }
   }
   return result;
}


/**
 * Set the client start synchronization event.
 *
 * @return None
 */

static void
HgfsServerClientRdrSetEvent(void)
{
   ASSERT(gHgfsServerStartClientEvent != NULL);

   if (NULL != gHgfsServerStartClientEvent) {
      if (!SetEvent(gHgfsServerStartClientEvent)) {
         g_warning("%s: Error: Set event = %u\n", __FUNCTION__, GetLastError());
      }
   }
}


/**
 * Get the client driver synchronization event name.
 *
 * @param[out]    syncEventName      Synchronization mutex name
 * @param[in]     syncEventNameSize  Synchronization mutex name size in bytes.
 *
 * @return ERROR_SUCCESS on success, an appropriate error otherwise.
 */

static DWORD
HgfsServerGetEventName(LPWSTR syncEventName,
                       size_t syncEventNameSize)
{
   DWORD result = ERROR_SUCCESS;

   /* Place the mutex name into the global name space. */
   if (StringCbPrintfW(syncEventName,
                       syncEventNameSize,
                       GLOBAL_PREFIX,
                       HGFS_CLIENT_START_EVENT_NAME) != S_OK) {
      result = ERROR_INSUFFICIENT_BUFFER;
      g_warning("%s: Error: StringCbPrintf failed (%u)\n", __FUNCTION__, result);
   }

   return result;
}


/**
 * Create or Open the client start synchronization event.
 *
 * @return ERROR_SUCCESS on success, an error otherwise
 */

static DWORD
HgfsServerClientRdrGetEvent(void)
{
   DWORD result = ERROR_SUCCESS;
   WCHAR syncEventName[MAX_PATH] = {0};

   ASSERT(NULL == gHgfsServerStartClientEvent);

   result = HgfsServerGetEventName(syncEventName, sizeof syncEventName);
   if (ERROR_SUCCESS != result) {
      goto exit;
   }

   /* Try to create the event incase we are first. */
   result = HgfsServerClientRdrCreateEvent(syncEventName);
   if (ERROR_ACCESS_DENIED == result) {
      /* Already created and we only have permission to open the event. */
      g_info("%s: Info: Opening existing %S\n", __FUNCTION__, syncEventName);
      result = HgfsServerClientRdrOpenEvent(syncEventName);
      if (ERROR_SUCCESS != result) {
         g_warning("%s: Error: Opening %S = %u\n", __FUNCTION__, syncEventName, result);
         ASSERT(NULL == gHgfsServerStartClientEvent);
      }
   }

exit:
   g_info("%s: Done getting %S = %u\n", __FUNCTION__, syncEventName, result);
   return result;
}
#endif // defined(_WIN32)


/**
 * Close the client start synchronization mutex.
 *
 * @return None.
 */

static void
HgfsServerCloseClientRdrEvent(void)
{
#if defined(_WIN32)
   if (NULL != gHgfsServerStartClientEvent) {
      CloseHandle(gHgfsServerStartClientEvent);
      gHgfsServerStartClientEvent = NULL;
      g_info("%s: Info: Closed handle\n", __FUNCTION__);
   }
#endif // defined(_WIN32)
}


/**
 * Start the client redirector.
 *
 * @return None.
 */

static void
HgfsServerClientRdrStart(void)
{
#if defined(_WIN32)
   DWORD eventResult;
   DWORD startResult;

   /* Obtain the client redirector synchronization event. */
   eventResult = HgfsServerClientRdrGetEvent();
   /* Always start the client redirector service. */
   startResult = HgfsServerClientRedirectorExecOp(HGFS_CLIENTRDR_SERVICE_START);
   if (ERROR_SUCCESS == startResult &&
       ERROR_SUCCESS == eventResult) {
      /* Client has started so signal the synchronization event. */
      HgfsServerClientRdrSetEvent();
   }
   g_info("%s: Info: Done start %u notify %u\n", __FUNCTION__, startResult, eventResult);
#endif // defined(_WIN32)
}


/**
 * Reconnect the client redirector's mapped drives.
 *
 * @return None.
 */

static void
HgfsServerClientRdrConnectDrives(void)
{
#if defined(_WIN32)
   DWORD result;

#define HGFS_SERVER_WAIT_FOR_CLIENT_COUNT    5
#define HGFS_SERVER_WAIT_FOR_CLIENT_PERIOD   2000

   result = HgfsServerClientRdrGetEvent();
   /* Obtain the client redirector synchronization event. */
   if (ERROR_SUCCESS == result) {
      DWORD retries = HGFS_SERVER_WAIT_FOR_CLIENT_COUNT;

      do {
         /* Check if the client redirector service has started. */
         result = HgfsServerClientRedirectorExecOp(HGFS_CLIENTRDR_SERVICE_QUERY_STARTED);
         /* Check if the main service created the event yet. If not we wait and retry. */
         if (ERROR_SERVICE_NOT_ACTIVE == result) {
            retries--;
            g_info("%s: client rdr not active will retry %u times\n", __FUNCTION__, retries);
            HgfsServerClientRdrWaitForEvent(HGFS_SERVER_WAIT_FOR_CLIENT_PERIOD);
         }
      } while (ERROR_SERVICE_NOT_ACTIVE == result && retries > 0);
   }

   if (ERROR_SUCCESS == result) {
      HgfsServerReconnectDrives();
   }
   g_info("%s: Done %u\n", __FUNCTION__, result);
#endif // defined(_WIN32)
}



/**
 * Returns the registration data for the HGFS server.
 *
 * @param[in]  ctx   The application context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "hgfsServer",
      NULL,
      NULL
   };
   HgfsServerMgrData *mgrData;
   uint32 vmxVersion = 0;
   uint32 vmxType = VMX_TYPE_UNSET;

   if (!TOOLS_IS_MAIN_SERVICE(ctx) && !TOOLS_IS_USER_SERVICE(ctx)) {
      g_info("Unknown container '%s', not loading HGFS plugin.", ctx->name);
      return NULL;
   }

   /*
    * If not running in a VMware VM, return NULL to disable the plugin.
    */
   if (!ctx->isVMware) {
      return NULL;
   }

   /*
    * Check for VM is running in a hosted environment and if so initialize
    * the Shared Folders HGFS client redirector.
    */
   if (VmCheck_GetVersion(&vmxVersion, &vmxType) &&
       vmxType != VMX_TYPE_SCALABLE_SERVER) {
      if (TOOLS_IS_MAIN_SERVICE(ctx)) {
         /* Start the Shared Folders redirector client. */
         HgfsServerClientRdrStart();
      } else if (TOOLS_IS_USER_SERVICE(ctx)) {
         /*
          * If Explorer recreated the mapped drives prior to the client being up and
          * running by the main service, we will need to reconnect the Shared Folders
          * drives.
          */
         HgfsServerClientRdrConnectDrives();
      } else {
         NOT_REACHED();
      }
   } else {
      g_debug("VM is not running in a hosted product skip HGFS client redirector initialization.");
   }

   mgrData = g_malloc0(sizeof *mgrData);
   HgfsServerManager_DataInit(mgrData,
                              ctx->name,
                              NULL,       // rpc channel unused
                              NULL);      // no rpc callback

   if (!HgfsServerManager_Register(mgrData)) {
      g_warning("HgfsServer_InitState() failed, aborting HGFS server init.\n");
      g_free(mgrData);
      return NULL;
   }

   {
      RpcChannelCallback rpcs[] = {
         { HGFS_SYNC_REQREP_CMD, HgfsServerRpcDispatch, mgrData, NULL, NULL, 0 }
      };
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CAPABILITIES, HgfsServerCapReg, &regData },
         { TOOLS_CORE_SIG_SHUTDOWN, HgfsServerShutdown, &regData }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
         { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
      };

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   }
   regData._private = mgrData;

   return &regData;
}
