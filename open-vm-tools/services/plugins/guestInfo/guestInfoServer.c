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
 * guestInfoServer.c --
 *
 *      This is the implementation of the common code in the guest tools
 *      to send out guest information to the host. The guest info server
 *      runs in the context of the tools daemon's event loop and periodically
 *      gathers all guest information and sends updates to the host if required.
 *      This file implements the platform independent framework for this.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "vmware.h"
#include "buildNumber.h"
#include "conf.h"
#include "debug.h"
#include "dynxdr.h"
#include "hostinfo.h"
#include "guestInfoInt.h"
#include "guest_msg_def.h" // For GUESTMSG_MAX_IN_SIZE
#include "netutil.h"
#include "rpcChannel.h"
#include "rpcvmx.h"
#include "procMgr.h"
#include "str.h"
#include "strutil.h"
#include "system.h"
#include "util.h"
#include "vm_app.h"
#include "vmtools.h"
#include "vmtoolsApp.h"
#include "xdrutil.h"
#include "vmsupport.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


#define GUESTINFO_DEFAULT_DELIMITER ' '

/*
 * Stores information about all guest information sent to the vmx.
 */

typedef struct _GuestInfoCache{
   char value[INFO_MAX][MAX_VALUE_LEN]; /* Stores values of all key-value pairs. */
   GuestNicList  nicInfo;
   GuestDiskInfo diskInfo;
} GuestInfoCache;


/* Local cache of the guest information that was last sent to vmx. */
static GuestInfoCache gInfoCache;

/*
 * A boolean flag that specifies whether the state of the VM was
 * changed since the last time guest info was sent to the VMX.
 * Tools daemon sets it to TRUE after the VM was resumed.
 */

static Bool vmResumed;

static Bool GuestInfoUpdateVmdb(ToolsAppCtx *ctx, GuestInfoType infoType, void *info);
static Bool SetGuestInfo(ToolsAppCtx *ctx, GuestInfoType key,
                         const char *value, char delimiter);
static Bool NicInfoChanged(GuestNicList *nicInfo);
static Bool DiskInfoChanged(PGuestDiskInfo diskInfo);
static void GuestInfoClearCache(void);
static int PrintNicInfo(GuestNicList *nicInfo, int (*PrintFunc)(const char *, ...));


/**
 * Lauches the VM support process in the guest when requested by the host.
 *
 * @param[in]  data     RPC request data.
 *
 * @return TRUE on success.
 */

static Bool
GuestInfoVMSupport(RpcInData *data)
{
#if defined(_WIN32)

    char vmSupportCmd[] = "vm-support.vbs";
    char *vmSupportPath = NULL;
    gchar *vmSupport = NULL;

    SECURITY_ATTRIBUTES saProcess = {0}, saThread = {0};

    ProcMgr_AsyncProc *vmSupportProc = NULL;
    ProcMgr_ProcArgs vmSupportProcArgs = {0};

    /*
     * Construct the commandline to be passed during execution
     * This will be the path of our vm-support.vbs
     */
    vmSupportPath = GuestApp_GetInstallPath();

    if (vmSupportPath == NULL) {
       return RPCIN_SETRETVALS(data,
                               "GuestApp_GetInstallPath failed", FALSE);
    }

    /* Put together absolute vm-support filename. */
    vmSupport = g_strdup_printf("cscript \"%s%s%s\" -u",
                                vmSupportPath, DIRSEPS, vmSupportCmd);
    vm_free(vmSupportPath);

    saProcess.nLength = sizeof saProcess;
    saProcess.bInheritHandle = TRUE;

    saThread.nLength = sizeof saThread;

    vmSupportProcArgs.lpProcessAttributes = &saProcess;
    vmSupportProcArgs.lpThreadAttributes = &saThread;
    vmSupportProcArgs.dwCreationFlags = CREATE_NO_WINDOW;

    g_debug("Starting vm-support script - %s\n", vmSupport);
    vmSupportProc = ProcMgr_ExecAsync(vmSupport, &vmSupportProcArgs);
    g_free(vmSupport);

    if (vmSupportProc == NULL) {
       g_warning("Error starting vm-support script\n");
       return RPCIN_SETRETVALS(data,
                               "Error starting vm-support script", FALSE);
    }

    ProcMgr_Free(vmSupportProc);
    return RPCIN_SETRETVALS(data, "", TRUE);

#else

     gchar *vmSupportCmdArgv[] = {"vm-support", "-u", NULL};

     g_debug("Starting vm-support script - %s\n", vmSupportCmdArgv[0]);
     if (!g_spawn_async(NULL, vmSupportCmdArgv, NULL,
                        G_SPAWN_SEARCH_PATH |
                        G_SPAWN_STDOUT_TO_DEV_NULL |
                        G_SPAWN_STDERR_TO_DEV_NULL,
                        NULL, NULL, NULL, NULL)) {
        g_warning("Error starting vm-support script\n");
        return RPCIN_SETRETVALS(data,
                                "Error starting vm-support script", FALSE);
     }

     return RPCIN_SETRETVALS(data, "", TRUE);

#endif
}


/**
 * Cleanup internal data on shutdown.
 *
 * @param[in]  src     The source object.
 * @param[in]  ctx     Unused.
 * @param[in]  data    Unused.
 */

static void
GuestInfoServerShutdown(gpointer src,
                        ToolsAppCtx *ctx,
                        gpointer data)
{
   GuestInfoClearCache();
}


/**
 * Reset callback - sets the internal flag that says we should purge all
 * caches.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 */

static void
GuestInfoServerReset(gpointer src,
                     ToolsAppCtx *ctx,
                     gpointer data)
{
   vmResumed = TRUE;
}


/**
 * Send the guest uptime through the backdoor, if @a set is TRUE.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The application context.
 * @param[in]  set      Whether capabilities are being set.
 * @param[in]  data     Unused.
 *
 * @return NULL
 */

static GArray *
GuestInfoServerSendUptime(gpointer src,
                          ToolsAppCtx *ctx,
                          gboolean set,
                          gpointer data)
{
   if (set) {
      gchar *uptime = g_strdup_printf("%"FMT64"u", System_Uptime());
      g_debug("Setting guest uptime to '%s'\n", uptime);
      GuestInfoUpdateVmdb(ctx, INFO_UPTIME, uptime);
      g_free(uptime);
   }
   return NULL;
}


/**
 * Responds to a "broadcastIP" Set_Option command, by sending the primary IP
 * back to the VMX.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The application context.
 * @param[in]  option   Option name.
 * @param[in]  value    Option value.
 * @param[in]  data     Unused.
 */

static gboolean
GuestInfoServerSetOption(gpointer src,
                         ToolsAppCtx *ctx,
                         const gchar *option,
                         const gchar *value,
                         gpointer data)
{
   char *ip;
   Bool ret = FALSE;

   if (strcmp(option, TOOLSOPTION_BROADCASTIP) != 0) {
      goto exit;
   }

   if (strcmp(value, "0") == 0) {
      ret = TRUE;
      goto exit;
   }

   if (strcmp(value, "1") != 0) {
      goto exit;
   }

   ip = NetUtil_GetPrimaryIP();

   if (ip != NULL) {
      gchar *msg;
      msg = g_strdup_printf("info-set guestinfo.ip %s", ip);
      ret = RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL);
      vm_free(ip);
      g_free(msg);
   }

exit:
   return (gboolean) ret;
}


/**
 * Periodically collects all the desired guest information and updates VMDB.
 *
 * @param[in]  data     The application context.
 *
 * @return TRUE.
 */

static gboolean
GuestInfoGather(gpointer data)
{
   char name[255];
   char osNameFull[MAX_VALUE_LEN];
   char osName[MAX_VALUE_LEN];
   gboolean disableQueryDiskInfo;
   GuestNicList nicInfo;
   GuestDiskInfo diskInfo;
#if defined(_WIN32) || defined(linux)
   GuestMemInfo vmStats = {0};
#endif
   ToolsAppCtx *ctx = data;

   g_debug("Entered guest info gather.\n");

   memset(&nicInfo, 0, sizeof nicInfo);

   /* Send tools version. */
   if (!GuestInfoUpdateVmdb(ctx, INFO_BUILD_NUMBER, BUILD_NUMBER)) {
      /*
       * An older vmx talking to new tools wont be able to handle
       * this message. Continue, if thats the case.
       */

      g_warning("Failed to update VMDB with tools version.\n");
   }

   /* Gather all the relevant guest information. */
   if (!Hostinfo_GetOSName(sizeof osNameFull, sizeof osName, osNameFull,
                           osName)) {
      g_warning("Failed to get OS info.\n");
   } else {
      if (!GuestInfoUpdateVmdb(ctx, INFO_OS_NAME_FULL, osNameFull)) {
         g_warning("Failed to update VMDB\n");
      }
      if (!GuestInfoUpdateVmdb(ctx, INFO_OS_NAME, osName)) {
         g_warning("Failed to update VMDB\n");
      }
   }

   disableQueryDiskInfo = g_key_file_get_boolean(ctx->config,
                                                 "guestinfo",
                                                 CONFNAME_DISABLEQUERYDISKINFO,
                                                 NULL);
   if (!disableQueryDiskInfo) {
      if (!GuestInfo_GetDiskInfo(&diskInfo)) {
         g_warning("Failed to get disk info.\n");
      } else {
         if (!GuestInfoUpdateVmdb(ctx, INFO_DISK_FREE_SPACE, &diskInfo)) {
            g_warning("Failed to update VMDB\n.");
         }
      }
      /* Free memory allocated in GuestInfoGetDiskInfo. */
      if (diskInfo.partitionList != NULL) {
         vm_free(diskInfo.partitionList);
         diskInfo.partitionList = NULL;
      }
   }

   if (!GuestInfo_GetFqdn(sizeof name, name)) {
      g_warning("Failed to get netbios name.\n");
   } else if (!GuestInfoUpdateVmdb(ctx, INFO_DNS_NAME, name)) {
      g_warning("Failed to update VMDB.\n");
   }

   /* Get NIC information. */
   if (!GuestInfo_GetNicInfo(&nicInfo)) {
      g_warning("Failed to get nic info.\n");
   } else if (NicInfoChanged(&nicInfo)) {
      if (GuestInfoUpdateVmdb(ctx, INFO_IPADDRESS, &nicInfo)) {
         /*
          * Update the cache. Release the memory previously used by the cache,
          * and copy the new information into the cache.
          */
         VMX_XDR_FREE(xdr_GuestNicList, &gInfoCache.nicInfo);
         gInfoCache.nicInfo = nicInfo;
      } else {
         g_warning("Failed to update VMDB.\n");
      }
   } else {
      g_debug("Nic info not changed.\n");
      VMX_XDR_FREE(xdr_GuestNicList, &nicInfo);
   }

   /* Send the uptime to VMX so that it can detect soft resets. */
   GuestInfoServerSendUptime(NULL, ctx, TRUE, NULL);

#if defined(_WIN32) || defined(linux)
   /* Send the vmstats to the VMX. */

   if (!GuestInfo_PerfMon(&vmStats)) {
      g_warning("Failed to get vmstats.\n");
   } else {
      vmStats.version = 1;
      if (!GuestInfoUpdateVmdb(ctx, INFO_MEMORY, &vmStats)) {
         g_warning("Failed to send vmstats.\n");
      }
   }
#endif

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoConvertNicInfoToNicInfoV1 --
 *
 *      Convert the new dynamic nicInfoNew to fixed size struct GuestNicInfoV1.
 *
 * Results:
 *      TRUE if successfully converted
 *      FALSE otherwise
 *
 * Side effects:
 *      If number of NICs or number of IP addresses on any of the NICs
 *      exceeding MAX_NICS and MAX_IPS respectively, the extra ones
 *      are truncated, on successful return.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestInfoConvertNicInfoToNicInfoV1(GuestNicList *info,        // IN
                                   GuestNicInfoV1 *infoV1)    // OUT
{
   uint32 maxNics;
   u_int i;

   if ((NULL == info) ||
       (NULL == infoV1)) {
      return FALSE;
   }

   maxNics = MIN(info->nics.nics_len, MAX_NICS);
   infoV1->numNicEntries = maxNics;
   if (maxNics < info->nics.nics_len) {
      g_debug("Truncating NIC list for backwards compatibility.\n");
   }

   XDRUTIL_FOREACH(i, info, nics) {
      u_int j;
      uint32 maxIPs;
      GuestNic *nic = XDRUTIL_GETITEM(info, nics, i);

      Str_Strcpy(infoV1->nicList[i].macAddress,
                 nic->macAddress,
                 sizeof infoV1->nicList[i].macAddress);

      maxIPs = MIN(nic->ips.ips_len, MAX_IPS);
      infoV1->nicList[i].numIPs = 0;

      XDRUTIL_FOREACH(j, nic, ips) {
         VmIpAddress *ip = XDRUTIL_GETITEM(nic, ips, j);

         if (strlen(ip->ipAddress) < sizeof infoV1->nicList[i].ipAddress[j]) {
            Str_Strcpy(infoV1->nicList[i].ipAddress[j],
                       ip->ipAddress,
                       sizeof infoV1->nicList[i].ipAddress[j]);
            infoV1->nicList[i].numIPs++;
            if (infoV1->nicList[i].numIPs == maxIPs) {
               break;
            }
         } else {
            g_debug("Ignoring IPV6 address for compatibility.\n");
         }
      }

      if (infoV1->nicList[i].numIPs != nic->ips.ips_len) {
         g_debug("Some IP addresses were ignored for compatibility.\n");
      }
      if (i == maxNics) {
         break;
      }
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfoUpdateVmdb --
 *
 *    Update VMDB with new guest information.
 *    This is the only function that should need to change when the VMDB pipe
 *    is implemented. Since we dont currently have a VMDB instance in the guest
 *    the function updates the VMDB instance on the host. Updates are sent only
 *    if the values have changed.
 *
 * Result
 *    TRUE on success, FALSE on failure.
 *
 * Side-effects
 *    VMDB is updated if the given value has changed.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfoUpdateVmdb(ToolsAppCtx *ctx,       // IN: Application context
                    GuestInfoType infoType, // IN: guest information type
                    void *info)             // IN: type specific information
{
   ASSERT(info);
   g_debug("Entered update vmdb: %d.\n", infoType);

   if (vmResumed) {
      vmResumed = FALSE;
      GuestInfoClearCache();
   }

   switch (infoType) {
   case INFO_DNS_NAME:
   case INFO_BUILD_NUMBER:
   case INFO_OS_NAME:
   case INFO_OS_NAME_FULL:
   case INFO_UPTIME:
      /*
       * This is one of our key value pairs. Update it if it has changed.
       * Above fall-through is intentional.
       */

      if (strcmp(gInfoCache.value[infoType], (char *)info) == 0) {
         /* The value has not changed */
         g_debug("Value unchanged for infotype %d.\n", infoType);
         break;
      }

      if (!SetGuestInfo(ctx, infoType, (char *)info, 0)) {
         g_warning("Failed to update key/value pair for type %d.\n", infoType);
         return FALSE;
      }

      /* Update the value in the cache as well. */
      Str_Strcpy(gInfoCache.value[infoType], (char *)info, MAX_VALUE_LEN);
      break;

   case INFO_IPADDRESS:
      {
         static Bool isCmdV1 = FALSE;
         char *reply = NULL;
         size_t replyLen;
         Bool status;

         if (FALSE == isCmdV1) {
            /* 13 = max size of string representation of an int + 3 spaces. */
            char request[sizeof GUEST_INFO_COMMAND + 13];
            GuestNicProto message;
            XDR xdrs;

            if (DynXdr_Create(&xdrs) == NULL) {
               return FALSE;
            }

            /* Add the RPC preamble: message name, and type. */
            Str_Sprintf(request, sizeof request, "%s  %d ",
                        GUEST_INFO_COMMAND, INFO_IPADDRESS_V2);

            message.ver = NIC_INFO_V2;
            message.GuestNicProto_u.nicsV2 = info;

            /* Write preamble and serialized nic info to XDR stream. */
            if (!DynXdr_AppendRaw(&xdrs, request, strlen(request)) ||
                !xdr_GuestNicProto(&xdrs, &message)) {
               g_warning("Error serializing nic info v2 data.");
               DynXdr_Destroy(&xdrs, TRUE);
               return FALSE;
            }

            status = RpcChannel_Send(ctx->rpc,
                                     DynXdr_Get(&xdrs),
                                     xdr_getpos(&xdrs),
                                     &reply,
                                     &replyLen);
            DynXdr_Destroy(&xdrs, TRUE);
            if (!status) {
               g_warning("Failed to send V2 nic info message.\n");
            }

            if (g_key_file_get_boolean(ctx->config, "guestinfo", "printNicInfo", NULL)) {
               PrintNicInfo((GuestNicList *) info,
                            (int (*)(const char *fmt, ...)) RpcVMX_Log);
            }
         } else {
            status = FALSE;
         }
         if (!status) {
            /*
             * Could be that we are talking to the old protocol that GuestNicInfo is
             * still fixed size.  Another try to send the fixed sized Nic info.
             */
            char request[sizeof (GuestNicInfoV1) + sizeof GUEST_INFO_COMMAND +
                         2 +                 /* 2 bytes are for digits of infotype. */
                         3 * sizeof (char)]; /* 3 spaces */
            GuestNicInfoV1 nicInfo;

            vm_free(reply);
            reply = NULL;

            Str_Sprintf(request,
                        sizeof request,
                        "%s  %d ",
                        GUEST_INFO_COMMAND,
                        INFO_IPADDRESS);
            if (GuestInfoConvertNicInfoToNicInfoV1(info, &nicInfo)) {
               memcpy(request + strlen(request),
                      &nicInfo,
                      sizeof(GuestNicInfoV1));

               g_debug("Sending nic info message.\n");
               /* Send all the information in the message. */
               status = RpcChannel_Send(ctx->rpc,
                                        request,
                                        sizeof request,
                                        &reply,
                                        &replyLen);

               g_debug("Just sent fixed sized nic info message.\n");
               if (!status) {
                  g_debug("Failed to update fixed sized nic information\n");
                  vm_free(reply);
                  return FALSE;
               }
               isCmdV1 = TRUE;
            } else {
               return FALSE;
            }
         }

         g_debug("Updated new NIC information\n");
         vm_free(reply);
         reply = NULL;
      }
      break;

   case INFO_MEMORY:
   {
      char request[sizeof(GuestMemInfo) + sizeof GUEST_INFO_COMMAND +
                   2 +                    /* 2 bytes are for digits of infotype. */
                   3 * sizeof (char)];    /* 3 spaces */
      Bool status;

      g_debug("Sending GuestMemInfo message.\n");
      Str_Sprintf(request,
                  sizeof request,
                  "%s  %d ",
                  GUEST_INFO_COMMAND,
                  INFO_MEMORY);
      memcpy(request + strlen(request),
             info, sizeof(GuestMemInfo));

      /* Send all the information in the message. */
      status = RpcChannel_Send(ctx->rpc, request, sizeof(request), NULL, NULL);
      if (!status) {
         g_warning("Error sending GuestMemInfo.\n");
         return FALSE;
      }
      g_debug("GuestMemInfo sent successfully.\n");
      break;
   }

   case INFO_DISK_FREE_SPACE:
      {
         /*
          * 2 accounts for the digits of infotype and 3 for the three
          * spaces.
          */
         unsigned int requestSize = sizeof GUEST_INFO_COMMAND + 2 + 3 * sizeof (char);
         uint8 partitionCount;
         size_t offset;
         char *request;
         char *reply;
         size_t replyLen;
         Bool status;
         PGuestDiskInfo pdi = (PGuestDiskInfo)info;
         int j = 0;

         if (!DiskInfoChanged(pdi)) {
            g_debug("Disk info not changed.\n");
            break;
         }

         requestSize += sizeof pdi->numEntries +
                        sizeof *pdi->partitionList * pdi->numEntries;
         request = (char *)calloc(requestSize, sizeof (char));
         if (request == NULL) {
            g_warning("Could not allocate memory for request.\n");
            break;
         }

         Str_Sprintf(request, requestSize, "%s  %d ", GUEST_INFO_COMMAND,
                     INFO_DISK_FREE_SPACE);

         /* partitionCount is a uint8 and cannot be larger than UCHAR_MAX. */
         if (pdi->numEntries > UCHAR_MAX) {
            g_warning("Too many partitions.\n");
            vm_free(request);
            return FALSE;
         }
         partitionCount = pdi->numEntries;

         offset = strlen(request);

         /*
          * Construct the disk information message to send to the host.  This
          * contains a single byte indicating the number partitions followed by
          * the PartitionEntry structure for each one.
          *
          * Note that the use of a uint8 to specify the partitionCount is the
          * result of a bug (see bug 117224) but should not cause a problem
          * since UCHAR_MAX is 255.  Also note that PartitionEntry is packed so
          * it's safe to send it from 64-bit Tools to a 32-bit VMX, etc.
          */
         memcpy(request + offset, &partitionCount, sizeof partitionCount);
         memcpy(request + offset + sizeof partitionCount, pdi->partitionList,
                sizeof *pdi->partitionList * pdi->numEntries);

         g_debug("sizeof request is %d\n", requestSize);
         status = RpcChannel_Send(ctx->rpc, request, requestSize, &reply, &replyLen);

         if (!status || (strncmp(reply, "", 1) != 0)) {
            g_warning("Failed to update disk information.\n");
            vm_free(request);
            vm_free(reply);
            return FALSE;
         }

         g_debug("Updated disk info information\n");
         vm_free(reply);
         vm_free(request);

         /* Free any memory previously allocated in the cache. */
         if (gInfoCache.diskInfo.partitionList != NULL) {
            vm_free(gInfoCache.diskInfo.partitionList);
            gInfoCache.diskInfo.partitionList = NULL;
         }
         gInfoCache.diskInfo.numEntries = pdi->numEntries;
         gInfoCache.diskInfo.partitionList = calloc(pdi->numEntries,
                                                         sizeof(PartitionEntry));
         if (gInfoCache.diskInfo.partitionList == NULL) {
            g_warning("Could not allocate memory for the disk info cache.\n");
            return FALSE;
         }

         for (j = 0; j < pdi->numEntries; j++) {
            gInfoCache.diskInfo.partitionList[j] = pdi->partitionList[j];
         }
         break;
      }
   default:
      g_error("Invalid info type: %d\n", infoType);
      break;
   }

   g_debug("Returning after updating guest information\n");
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * SetGuestInfo --
 *
 *      Ask Vmx to write some information about the guest into VMDB.
 *
 * Results:
 *
 *      TRUE/FALSE depending on whether the RPCI succeeded or failed.
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
SetGuestInfo(ToolsAppCtx *ctx,   // IN: application context
             GuestInfoType key,  // IN: the VMDB key to set
             const char *value,  // IN:
             char delimiter)     // IN: delimiting character for the rpc
                                 //     message. 0 indicates default.
{
   Bool status;
   char *reply;
   gchar *msg;
   size_t replyLen;

   ASSERT(key);
   ASSERT(value);

   if (!delimiter) {
      delimiter = GUESTINFO_DEFAULT_DELIMITER;
   }

   msg = g_strdup_printf("%s %c%d%c%s", GUEST_INFO_COMMAND,
                         delimiter, key, delimiter, value);

   status = RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, &reply, &replyLen);
   g_free(msg);

   if (!status) {
      g_warning("Error sending rpc message: %s\n", reply ? reply : "NULL");
      vm_free(reply);
      return FALSE;
   }

   /* The reply indicates whether the key,value pair was updated in VMDB. */
   status = (strncmp(reply, "", 1) == 0);
   vm_free(reply);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfoFindMacAddress --
 *
 *      Locates a NIC with the given MAC address in the NIC list.
 *
 * Return value:
 *      If there is an entry in nicInfo which corresponds to this MAC address,
 *      it is returned. If not NULL is returned.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GuestNic *
GuestInfoFindMacAddress(GuestNicList *nicInfo,  // IN/OUT
                        const char *macAddress) // IN
{
   u_int i;

   for (i = 0; i < nicInfo->nics.nics_len; i++) {
      GuestNic *nic = &nicInfo->nics.nics_val[i];
      if (strncmp(nic->macAddress, macAddress, NICINFO_MAC_LEN) == 0) {
         return nic;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * NicInfoChanged --
 *
 *      Checks whether Nic information just obtained is different from
 *      the information last sent to VMDB.
 *
 * Results:
 *
 *      TRUE if the NIC info has changed, FALSE otherwise
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
NicInfoChanged(GuestNicList *nicInfo)  // IN
{
   u_int i;
   GuestNicList *cachedNicInfo = &gInfoCache.nicInfo;

   if (cachedNicInfo->nics.nics_len != nicInfo->nics.nics_len) {
      g_debug("Number of nics has changed\n");
      return TRUE;
   }

   for (i = 0; i < cachedNicInfo->nics.nics_len; i++) {
      u_int j;
      GuestNic *cachedNic = &cachedNicInfo->nics.nics_val[i];
      GuestNic *matchedNic;

      /* Find the corresponding nic in the new nic info. */
      matchedNic = GuestInfoFindMacAddress(nicInfo, cachedNic->macAddress);

      if (NULL == matchedNic) {
         /* This mac address has been deleted. */
         return TRUE;
      }

      if (matchedNic->ips.ips_len != cachedNic->ips.ips_len) {
         g_debug("Count of ip addresses for mac %d\n",
                 matchedNic->ips.ips_len);
         return TRUE;
      }

      /* Which IP addresses have been modified for this NIC? */
      for (j = 0; j < cachedNic->ips.ips_len; j++) {
         VmIpAddress *cachedIp = &cachedNic->ips.ips_val[j];
         Bool foundIP = FALSE;
         u_int k;

         for (k = 0; k < matchedNic->ips.ips_len; k++) {
            VmIpAddress *matchedIp = &matchedNic->ips.ips_val[k];
            if (0 == strncmp(cachedIp->ipAddress,
                             matchedIp->ipAddress,
                             NICINFO_MAX_IP_LEN)) {
               foundIP = TRUE;
               break;
            }
         }

         if (FALSE == foundIP) {
            /* This ip address couldn't be found and has been modified. */
            g_debug("MAC address %s, ipaddress %s deleted\n",
                    cachedNic->macAddress,
                    cachedIp->ipAddress);
            return TRUE;
         }

      }

   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * PrintNicInfo --
 *
 *      Print NIC info struct using the specified print function.
 *
 * Results:
 *      Sum of return values of print function (for printf, this will be the
 *      number of characters printed).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
PrintNicInfo(GuestNicList *nicInfo,               // IN
             int (*PrintFunc)(const char *, ...)) // IN
{
   int ret = 0;
   u_int i = 0;

   ret += PrintFunc("NicInfo: count: %ud\n", nicInfo->nics.nics_len);
   for (i = 0; i < nicInfo->nics.nics_len; i++) {
      u_int j;
      GuestNic *nic = &nicInfo->nics.nics_val[i];

      ret += PrintFunc("NicInfo: nic [%d/%d] mac:      %s",
                       i+1,
                       nicInfo->nics.nics_len,
                       nic->macAddress);

      for (j = 0; j < nic->ips.ips_len; j++) {
         VmIpAddress *ipAddress = &nic->ips.ips_val[j];

         ret += PrintFunc("NicInfo: nic [%d/%d] IP [%d/%d]: %s",
                          i+1,
                          nicInfo->nics.nics_len,
                          j+1,
                          nic->ips.ips_len,
                          ipAddress->ipAddress);
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * DiskInfoChanged --
 *
 *      Checks whether disk info information just obtained is different from
 *      the information last sent to VMDB.
 *
 * Results:
 *
 *      TRUE if the disk info has changed, FALSE otherwise.
 *
 * Side effects:
 *
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
DiskInfoChanged(PGuestDiskInfo diskInfo)     // IN:
{
   int index;
   char *name;
   int i;
   int matchedPartition;
   PGuestDiskInfo cachedDiskInfo;

   cachedDiskInfo = &gInfoCache.diskInfo;

   if (cachedDiskInfo->numEntries != diskInfo->numEntries) {
      g_debug("Number of disks has changed\n");
      return TRUE;
   }

   /* Have any disks been modified? */
   for (index = 0; index < cachedDiskInfo->numEntries; index++) {
      name = cachedDiskInfo->partitionList[index].name;

      /* Find the corresponding partition in the new partition info. */
      for (i = 0; i < diskInfo->numEntries; i++) {
         if (!strncmp(diskInfo->partitionList[i].name, name, PARTITION_NAME_SIZE)) {
            break;
         }
      }

      matchedPartition = i;
      if (matchedPartition == diskInfo->numEntries) {
         /* This partition has been deleted. */
         g_debug("Partition %s deleted\n", name);
         return TRUE;
      } else {
         /* Compare the free space. */
         if (diskInfo->partitionList[matchedPartition].freeBytes !=
             cachedDiskInfo->partitionList[index].freeBytes) {
            g_debug("Free space changed\n");
            return TRUE;
         }
         if (diskInfo->partitionList[matchedPartition].totalBytes !=
            cachedDiskInfo->partitionList[index].totalBytes) {
            g_debug("Total space changed\n");
            return TRUE;
         }
      }
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfoClearCache --
 *
 *    Clears the cached guest info data.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    gInfoCache is cleared.
 *
 *----------------------------------------------------------------------
 */

static void
GuestInfoClearCache(void)
{
   int i;

   for (i = 0; i < INFO_MAX; i++) {
      gInfoCache.value[i][0] = 0;
   }

   VMX_XDR_FREE(xdr_GuestNicList, &gInfoCache.nicInfo);
   memset(&gInfoCache.nicInfo, 0, sizeof gInfoCache.nicInfo);
}


/**
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "guestInfo",
      NULL,
      NULL
   };

   GSource *src;

   RpcChannelCallback rpcs[] = {
      { RPC_VMSUPPORT_START, GuestInfoVMSupport, &regData, NULL, NULL, 0 }
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_CAPABILITIES, GuestInfoServerSendUptime, NULL },
      { TOOLS_CORE_SIG_RESET, GuestInfoServerReset, NULL },
      { TOOLS_CORE_SIG_SET_OPTION, GuestInfoServerSetOption, NULL },
      { TOOLS_CORE_SIG_SHUTDOWN, GuestInfoServerShutdown, NULL }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   memset(&gInfoCache, 0, sizeof gInfoCache);
   vmResumed = FALSE;

   /* Add the first timer event. */
   if (ctx->rpc) {
      src = g_timeout_source_new(GUESTINFO_TIME_INTERVAL_MSEC * 10);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, src, GuestInfoGather, ctx, NULL);
      g_source_unref(src);
   }

   return &regData;
}

