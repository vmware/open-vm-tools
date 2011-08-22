/*********************************************************
 * Copyright (C) 1998-2010 VMware, Inc. All rights reserved.
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

#ifndef WIN32
#   include <arpa/inet.h>
#endif

#include "vmware.h"
#include "buildNumber.h"
#include "conf.h"
#include "debug.h"
#include "dynxdr.h"
#include "hostinfo.h"
#include "guestInfoInt.h"
#include "guest_msg_def.h" // For GUESTMSG_MAX_IN_SIZE
#include "netutil.h"
#include "rpcvmx.h"
#include "procMgr.h"
#include "str.h"
#include "strutil.h"
#include "system.h"
#include "util.h"
#include "xdrutil.h"
#include "vmsupport.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/vmbackup.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

/**
 * Default poll interval is 30s (in milliseconds).
 */
#define GUESTINFO_TIME_INTERVAL_MSEC 30000

#define GUESTINFO_DEFAULT_DELIMITER ' '

/*
 * Stores information about all guest information sent to the vmx.
 */

typedef struct _GuestInfoCache {
   /* Stores values of all key-value pairs. */
   char          *value[INFO_MAX];
   NicInfoV3     *nicInfo;
   GuestDiskInfo *diskInfo;
} GuestInfoCache;


/**
 * Defines the current poll interval (in milliseconds).
 *
 * This value is controlled by the guestinfo.poll-interval config file option.
 */
int guestInfoPollInterval = 0;


/**
 * Gather loop timeout source.
 */
static GSource *gatherTimeoutSource = NULL;


/* Local cache of the guest information that was last sent to vmx. */
static GuestInfoCache gInfoCache;

/*
 * A boolean flag that specifies whether the state of the VM was
 * changed since the last time guest info was sent to the VMX.
 * Tools daemon sets it to TRUE after the VM was resumed.
 */

static Bool vmResumed;


/*
 * Local functions
 */


static Bool GuestInfoUpdateVmdb(ToolsAppCtx *ctx, GuestInfoType infoType, void *info);
static Bool SetGuestInfo(ToolsAppCtx *ctx, GuestInfoType key,
                         const char *value);
static void SendUptime(ToolsAppCtx *ctx);
static Bool DiskInfoChanged(const GuestDiskInfo *diskInfo);
static void GuestInfoClearCache(void);
static GuestNicList *NicInfoV3ToV2(const NicInfoV3 *infoV3);
static void TweakGatherLoop(ToolsAppCtx *ctx, gboolean enable);


/*
 ******************************************************************************
 * GuestInfoVMSupport --                                                 */ /**
 *
 * Launches the vm-support process.  Data returned asynchronously via RPCI.
 *
 * @param[in]   data     RPC request data.
 *
 * @return      TRUE if able to launch script, FALSE if script failed.
 *
 ******************************************************************************
 */

static gboolean
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


/*
 ******************************************************************************
 * GuestInfoGather --                                                    */ /**
 *
 * Collects all the desired guest information and updates the VMX.
 *
 * @param[in]  data     The application context.
 *
 * @return TRUE to indicate that the timer should be rescheduled.
 *
 ******************************************************************************
 */

static gboolean
GuestInfoGather(gpointer data)
{
   char name[256];  // Size is derived from the SUS2 specification
                    // "Host names are limited to 255 bytes"
   char *osString = NULL;
   gboolean disableQueryDiskInfo;
   NicInfoV3 *nicInfo = NULL;
   GuestDiskInfo *diskInfo = NULL;
#if defined(_WIN32) || defined(linux)
   GuestMemInfo vmStats = {0};
   gboolean perfmonEnabled;
#endif
   ToolsAppCtx *ctx = data;

   g_debug("Entered guest info gather.\n");

   /* Send tools version. */
   if (!GuestInfoUpdateVmdb(ctx, INFO_BUILD_NUMBER, BUILD_NUMBER)) {
      /*
       * An older vmx talking to new tools wont be able to handle
       * this message. Continue, if thats the case.
       */

      g_warning("Failed to update VMDB with tools version.\n");
   }

   /* Gather all the relevant guest information. */
   osString = Hostinfo_GetOSName();
   if (osString == NULL) {
      g_warning("Failed to get OS info.\n");
   } else {
      if (!GuestInfoUpdateVmdb(ctx, INFO_OS_NAME_FULL, osString)) {
         g_warning("Failed to update VMDB\n");
      }
   }
   free(osString);

   osString = Hostinfo_GetOSGuestString();
   if (osString == NULL) {
      g_warning("Failed to get OS info.\n");
   } else {
      if (!GuestInfoUpdateVmdb(ctx, INFO_OS_NAME, osString)) {
         g_warning("Failed to update VMDB\n");
      }
   }
   free(osString);

   disableQueryDiskInfo =
      g_key_file_get_boolean(ctx->config, CONFGROUPNAME_GUESTINFO,
                             CONFNAME_GUESTINFO_DISABLEQUERYDISKINFO, NULL);
   if (!disableQueryDiskInfo) {
      if ((diskInfo = GuestInfo_GetDiskInfo()) == NULL) {
         g_warning("Failed to get disk info.\n");
      } else {
         if (GuestInfoUpdateVmdb(ctx, INFO_DISK_FREE_SPACE, diskInfo)) {
            GuestInfo_FreeDiskInfo(gInfoCache.diskInfo);
            gInfoCache.diskInfo = diskInfo;
         } else {
            g_warning("Failed to update VMDB\n.");
            GuestInfo_FreeDiskInfo(diskInfo);
         }
      }
   }

   if (!System_GetNodeName(sizeof name, name)) {
      g_warning("Failed to get netbios name.\n");
   } else if (!GuestInfoUpdateVmdb(ctx, INFO_DNS_NAME, name)) {
      g_warning("Failed to update VMDB.\n");
   }

   /* Get NIC information. */
   if (!GuestInfo_GetNicInfo(&nicInfo)) {
      g_warning("Failed to get nic info.\n");
   } else if (GuestInfo_IsEqual_NicInfoV3(nicInfo, gInfoCache.nicInfo)) {
      g_debug("Nic info not changed.\n");
      GuestInfo_FreeNicInfo(nicInfo);
   } else if (GuestInfoUpdateVmdb(ctx, INFO_IPADDRESS, nicInfo)) {
      /*
       * Since the update succeeded, free the old cached object, and assign
       * ours to the cache.
       */
      GuestInfo_FreeNicInfo(gInfoCache.nicInfo);
      gInfoCache.nicInfo = nicInfo;
   } else {
      g_warning("Failed to update VMDB.\n");
      GuestInfo_FreeNicInfo(nicInfo);
   }

   /* Send the uptime to VMX so that it can detect soft resets. */
   SendUptime(ctx);

#if defined(_WIN32) || defined(linux)
   /* Send the vmstats to the VMX. */
   perfmonEnabled = !g_key_file_get_boolean(ctx->config,
                                            CONFGROUPNAME_GUESTINFO,
                                            CONFNAME_GUESTINFO_DISABLEPERFMON,
                                            NULL);

   if (perfmonEnabled) {
      if (!GuestInfo_PerfMon(&vmStats)) {
         g_warning("Failed to get vmstats.\n");
      } else {
         vmStats.version = 1;
         if (!GuestInfoUpdateVmdb(ctx, INFO_MEMORY, &vmStats)) {
            g_warning("Failed to send vmstats.\n");
         }
      }
   }
#endif

   return TRUE;
}


/*
 ******************************************************************************
 * GuestInfoConvertNicInfoToNicInfoV1 --                                 */ /**
 *
 * Converts V3 XDR NicInfo to hand-packed GuestNicInfoV1.
 *
 * @note Any NICs above MAX_NICS or IPs above MAX_IPS will be truncated.
 *
 * @param[in]  info   V3 input data.
 * @param[out] infoV1 V1 output data.
 *
 * @retval TRUE Conversion succeeded.
 * @retval FALSE Conversion failed.
 *
 ******************************************************************************
 */

void
GuestInfoConvertNicInfoToNicInfoV1(NicInfoV3 *info,
                                   GuestNicInfoV1 *infoV1)
{
   uint32 maxNics;
   u_int i;

   ASSERT(info);
   ASSERT(infoV1);

   maxNics = MIN(info->nics.nics_len, MAX_NICS);
   infoV1->numNicEntries = maxNics;
   if (maxNics < info->nics.nics_len) {
      g_debug("Truncating NIC list for backwards compatibility.\n");
   }

   XDRUTIL_FOREACH(i, info, nics) {
      u_int j;
      uint32 maxIPs;
      GuestNicV3 *nic = XDRUTIL_GETITEM(info, nics, i);

      Str_Strcpy(infoV1->nicList[i].macAddress,
                 nic->macAddress,
                 sizeof infoV1->nicList[i].macAddress);

      maxIPs = MIN(nic->ips.ips_len, MAX_IPS);
      infoV1->nicList[i].numIPs = 0;

      XDRUTIL_FOREACH(j, nic, ips) {
         IpAddressEntry *ip = XDRUTIL_GETITEM(nic, ips, j);
         TypedIpAddress *typedIp = &ip->ipAddressAddr;

         if (typedIp->ipAddressAddrType != IAT_IPV4) {
            continue;
         }

         if (NetUtil_InetNToP(AF_INET, typedIp->ipAddressAddr.InetAddress_val,
                              infoV1->nicList[i].ipAddress[j],
                              sizeof infoV1->nicList[i].ipAddress[j])) {
            infoV1->nicList[i].numIPs++;
            if (infoV1->nicList[i].numIPs == maxIPs) {
               break;
            }
         }
      }

      if (infoV1->nicList[i].numIPs != nic->ips.ips_len) {
         g_debug("Some IP addresses were ignored for compatibility.\n");
      }
      if (i == maxNics) {
         break;
      }
   }
}


/*
 ******************************************************************************
 * GuestInfoUpdateVmdb --                                                */ /**
 *
 * Push singular GuestInfo snippets to the VMX.
 *
 * @note Data are cached, so updates are sent only if they have changed.
 *
 * @param[in] ctx       Application context.
 * @param[in] infoType  Guest information type.
 * @param[in] info      Type-specific information.
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with serialization or transmission.
 *
 ******************************************************************************
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

      if (gInfoCache.value[infoType] != NULL &&
          strcmp(gInfoCache.value[infoType], (char *)info) == 0) {
         /* The value has not changed */
         g_debug("Value unchanged for infotype %d.\n", infoType);
         break;
      }

      if (!SetGuestInfo(ctx, infoType, (char *)info)) {
         g_warning("Failed to update key/value pair for type %d.\n", infoType);
         return FALSE;
      }

      /* Update the value in the cache as well. */
      free(gInfoCache.value[infoType]);
      gInfoCache.value[infoType] = Util_SafeStrdup((char *) info);
      break;

   case INFO_IPADDRESS:
      {
         static NicInfoVersion supportedVersion = NIC_INFO_V3;
         NicInfoV3 *nicInfoV3 = (NicInfoV3 *)info;
         char *reply = NULL;
         size_t replyLen;
         Bool status;

nicinfo_fsm:
         switch (supportedVersion) {
         case NIC_INFO_V3:
         case NIC_INFO_V2:
            {
               /*
                * 13 = max size of string representation of an int + 3 spaces.
                * XXX Ditch the magic numbers.
                */
               char request[sizeof GUEST_INFO_COMMAND + 13];
               GuestNicProto message = {0};
               GuestNicList *nicList = NULL;
               NicInfoVersion fallbackVersion;
               XDR xdrs;

               if (DynXdr_Create(&xdrs) == NULL) {
                  return FALSE;
               }

               /* Add the RPC preamble: message name, and type. */
               Str_Sprintf(request, sizeof request, "%s  %d ",
                           GUEST_INFO_COMMAND, INFO_IPADDRESS_V2);

               /*
                * Fill in message according to the version we'll attempt.  Also
                * note which version we'll fall back to should the VMX reject
                * our update.
                */
               if (supportedVersion == NIC_INFO_V3) {
                  message.ver = NIC_INFO_V3;
                  message.GuestNicProto_u.nicInfoV3 = nicInfoV3;

                  fallbackVersion = NIC_INFO_V2;
               } else {
                  nicList = NicInfoV3ToV2(nicInfoV3);
                  message.ver = NIC_INFO_V2;
                  message.GuestNicProto_u.nicsV2 = nicList;

                  fallbackVersion = NIC_INFO_V1;
               }

               /* Write preamble and serialized nic info to XDR stream. */
               if (!DynXdr_AppendRaw(&xdrs, request, strlen(request)) ||
                   !xdr_GuestNicProto(&xdrs, &message)) {
                  g_warning("Error serializing nic info v%d data.", message.ver);
                  DynXdr_Destroy(&xdrs, TRUE);
                  return FALSE;
               }

               status = RpcChannel_Send(ctx->rpc,
                                        DynXdr_Get(&xdrs),
                                        xdr_getpos(&xdrs),
                                        &reply,
                                        &replyLen);
               DynXdr_Destroy(&xdrs, TRUE);

               /*
                * Do not free/destroy contents of `message`.  The v3 nicInfo
                * passed to us belongs to our caller.  Instead, we'll only
                * destroy our local V2 converted data.
                */
               if (nicList) {
                  VMX_XDR_FREE(xdr_GuestNicList, nicList);
                  free(nicList);
                  nicList = NULL;
               }

               if (!status) {
                  g_warning("NicInfo update failed: version %u, reply \"%s\".\n",
                            supportedVersion, reply);
                  supportedVersion = fallbackVersion;
                  vm_free(reply);
                  reply = NULL;
                  goto nicinfo_fsm;
               }
            }
            break;

         case NIC_INFO_V1:
            {
               /*
                * Could be that we are talking to the old protocol that
                * GuestNicInfo is still fixed size.  Another try to send the
                * fixed sized Nic info.
                */
               char request[sizeof (GuestNicInfoV1) + sizeof GUEST_INFO_COMMAND +
                         2 +                 /* 2 bytes are for digits of infotype. */
                         3 * sizeof (char)]; /* 3 spaces */
               GuestNicInfoV1 nicInfo;

               Str_Sprintf(request,
                           sizeof request,
                           "%s  %d ",
                           GUEST_INFO_COMMAND,
                           INFO_IPADDRESS);

               GuestInfoConvertNicInfoToNicInfoV1(nicInfoV3, &nicInfo);

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
            }
            break;
         default:
            NOT_REACHED();
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
         GuestDiskInfo *pdi = info;

         if (!DiskInfoChanged(pdi)) {
            g_debug("Disk info not changed.\n");
            break;
         }

         ASSERT((pdi->numEntries && pdi->partitionList) ||
                (!pdi->numEntries && !pdi->partitionList));

         requestSize += sizeof pdi->numEntries +
                        sizeof *pdi->partitionList * pdi->numEntries;
         request = Util_SafeCalloc(requestSize, sizeof *request);

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

         /*
          * Conditioned because memcpy(dst, NULL, 0) -may- lead to undefined
          * behavior.
          */
         if (pdi->partitionList) {
            memcpy(request + offset + sizeof partitionCount, pdi->partitionList,
                   sizeof *pdi->partitionList * pdi->numEntries);
         }

         g_debug("sizeof request is %d\n", requestSize);
         status = RpcChannel_Send(ctx->rpc, request, requestSize, &reply, &replyLen);
         if (status) {
            status = (*reply == '\0');
         }

         vm_free(request);
         vm_free(reply);

         if (!status) {
            g_warning("Failed to update disk information.\n");
            return FALSE;
         }

         g_debug("Updated disk info information\n");

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
 ******************************************************************************
 * SendUptime --                                                         */ /**
 *
 * Send the guest uptime through the backdoor.
 *
 * @param[in]  ctx      The application context.
 *
 ******************************************************************************
 */

static void
SendUptime(ToolsAppCtx *ctx)
{
   gchar *uptime = g_strdup_printf("%"FMT64"u", System_Uptime());
   g_debug("Setting guest uptime to '%s'\n", uptime);
   GuestInfoUpdateVmdb(ctx, INFO_UPTIME, uptime);
   g_free(uptime);
}


/*
 ******************************************************************************
 * SetGuestInfo --                                                       */ /**
 *
 * Sends a simple key-value update request to the VMX.
 *
 * @param[in] ctx       Application context.
 * @param[in] key       VMDB key to set
 * @param[in] value     GuestInfo data
 *
 * @retval TRUE  RPCI succeeded.
 * @retval FALSE RPCI failed.
 *
 ******************************************************************************
 */

Bool
SetGuestInfo(ToolsAppCtx *ctx,
             GuestInfoType key,
             const char *value)
{
   Bool status;
   char *reply;
   gchar *msg;
   size_t replyLen;

   ASSERT(key);
   ASSERT(value);

   /*
    * XXX Consider retiring this runtime "delimiter" business and just
    * insert raw spaces into the format string.
    */
   msg = g_strdup_printf("%s %c%d%c%s", GUEST_INFO_COMMAND,
                         GUESTINFO_DEFAULT_DELIMITER, key,
                         GUESTINFO_DEFAULT_DELIMITER, value);

   status = RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, &reply, &replyLen);
   g_free(msg);

   if (!status) {
      g_warning("Error sending rpc message: %s\n", reply ? reply : "NULL");
      vm_free(reply);
      return FALSE;
   }

   /* The reply indicates whether the key,value pair was updated in VMDB. */
   status = (*reply == '\0');
   vm_free(reply);
   return status;
}


/*
 ******************************************************************************
 * GuestInfoFindMacAddress --                                            */ /**
 *
 * Locates a NIC with the given MAC address in the NIC list.
 *
 * @param[in] nicInfo    NicInfoV3 container.
 * @param[in] macAddress Requested MAC address.
 *
 * @return Valid pointer if NIC found, else NULL.
 *
 ******************************************************************************
 */


GuestNicV3 *
GuestInfoFindMacAddress(NicInfoV3 *nicInfo,     // IN/OUT
                        const char *macAddress) // IN
{
   u_int i;

   for (i = 0; i < nicInfo->nics.nics_len; i++) {
      GuestNicV3 *nic = &nicInfo->nics.nics_val[i];
      if (strncmp(nic->macAddress, macAddress, NICINFO_MAC_LEN) == 0) {
         return nic;
      }
   }

   return NULL;
}


/*
 ******************************************************************************
 * DiskInfoChanged --                                                    */ /**
 *
 * Checks whether disk info information just obtained is different from the
 * information last sent to the VMX.
 *
 * @param[in]  diskInfo New disk info.
 *
 * @retval TRUE  Data has changed.
 * @retval FALSE Data has not changed.
 *
 ******************************************************************************
 */

static Bool
DiskInfoChanged(const GuestDiskInfo *diskInfo)
{
   int index;
   char *name;
   int i;
   int matchedPartition;
   PGuestDiskInfo cachedDiskInfo;

   cachedDiskInfo = gInfoCache.diskInfo;

   if (cachedDiskInfo == diskInfo) {
      return FALSE;
      /* Implies that either cachedDiskInfo or diskInfo != NULL. */
   } else if (!cachedDiskInfo || !diskInfo) {
      return TRUE;
   }

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
 ******************************************************************************
 * GuestInfoClearCache --                                                */ /**
 *
 * Clears the cached guest info data.
 *
 ******************************************************************************
 */

static void
GuestInfoClearCache(void)
{
   int i;

   for (i = 0; i < INFO_MAX; i++) {
      free(gInfoCache.value[i]);
      gInfoCache.value[i] = NULL;
   }

   GuestInfo_FreeDiskInfo(gInfoCache.diskInfo);
   gInfoCache.diskInfo = NULL;

   GuestInfo_FreeNicInfo(gInfoCache.nicInfo);
   gInfoCache.nicInfo = NULL;
}


/*
 ***********************************************************************
 * NicInfoV3ToV2 --                                             */ /**
 *
 * @brief Converts the NicInfoV3 NIC list to a GuestNicList.
 *
 * @note  This function performs @e shallow copies of things such as
 *        IP address array, making it depend on the source NicInfoV3.
 *        In other words, do @e not free NicInfoV3 before freeing the
 *        returned pointer.
 *
 * @param[in]  infoV3   Source NicInfoV3 container.
 *
 * @return Pointer to a GuestNicList.  Caller should free it using
 *         plain ol' @c free.
 *
 ***********************************************************************
 */

static GuestNicList *
NicInfoV3ToV2(const NicInfoV3 *infoV3)
{
   GuestNicList *nicList;
   unsigned int i, j;

   nicList = Util_SafeCalloc(sizeof *nicList, 1);

   (void)XDRUTIL_ARRAYAPPEND(nicList, nics, infoV3->nics.nics_len);
   XDRUTIL_FOREACH(i, infoV3, nics) {
      GuestNicV3 *nic = XDRUTIL_GETITEM(infoV3, nics, i);
      GuestNic *oldNic = XDRUTIL_GETITEM(nicList, nics, i);

      Str_Strcpy(oldNic->macAddress, nic->macAddress, sizeof oldNic->macAddress);

      (void)XDRUTIL_ARRAYAPPEND(oldNic, ips, nic->ips.ips_len);

      XDRUTIL_FOREACH(j, nic, ips) {
         IpAddressEntry *ipEntry = XDRUTIL_GETITEM(nic, ips, j);
         TypedIpAddress *ip = &ipEntry->ipAddressAddr;
         VmIpAddress *oldIp = XDRUTIL_GETITEM(oldNic, ips, j);

         /* XXX */
         oldIp->addressFamily = (ip->ipAddressAddrType == IAT_IPV4) ?
            NICINFO_ADDR_IPV4 : NICINFO_ADDR_IPV6;

         NetUtil_InetNToP(ip->ipAddressAddrType == IAT_IPV4 ?
                          AF_INET : AF_INET6,
                          ip->ipAddressAddr.InetAddress_val, oldIp->ipAddress,
                          sizeof oldIp->ipAddress);

         Str_Sprintf(oldIp->subnetMask, sizeof oldIp->subnetMask, "%u",
                     ipEntry->ipAddressPrefixLength);
      }
   }

   return nicList;
}


/*
 ******************************************************************************
 * TweakGatherLoop --                                                    */ /**
 *
 * @brief Start, stop, reconfigure the GuestInfoGather poll loop.
 *
 * This function is responsible for creating, manipulating, and resetting the
 * GuestInfoGather loop timeout source.
 *
 * @param[in]  ctx      The app context.
 * @param[in]  enable   Whether to enable the gather loop.
 *
 * @sa CONFNAME_GUESTINFO_POLLINTERVAL
 *
 ******************************************************************************
 */

static void
TweakGatherLoop(ToolsAppCtx *ctx,
                gboolean enable)
{
   GError *gError = NULL;
   gint pollInterval = 0;

   if (enable) {
      pollInterval = GUESTINFO_TIME_INTERVAL_MSEC;

      /*
       * Check the config registry for a custom poll interval, converting from
       * seconds to milliseconds.
       */
      if (g_key_file_has_key(ctx->config, CONFGROUPNAME_GUESTINFO,
                             CONFNAME_GUESTINFO_POLLINTERVAL, NULL)) {
         pollInterval = g_key_file_get_integer(ctx->config, CONFGROUPNAME_GUESTINFO,
                                               CONFNAME_GUESTINFO_POLLINTERVAL, &gError);
         pollInterval *= 1000;

         if (pollInterval < 0 || gError) {
            g_warning("Invalid %s.%s value.  Using default.\n",
                      CONFGROUPNAME_GUESTINFO, CONFNAME_GUESTINFO_POLLINTERVAL);
            pollInterval = GUESTINFO_TIME_INTERVAL_MSEC;
         }
      }
   }

   /*
    * If the interval hasn't changed, let's not interfere with the existing
    * timeout source.
    */
   if (guestInfoPollInterval == pollInterval) {
      ASSERT(pollInterval || gatherTimeoutSource == NULL);
      return;
   }

   /*
    * All checks have passed.  Destroy the existing timeout source, if it
    * exists, then create and attach a new one.
    */

   if (gatherTimeoutSource != NULL) {
      g_source_destroy(gatherTimeoutSource);
      gatherTimeoutSource = NULL;
   }

   guestInfoPollInterval = pollInterval;

   if (guestInfoPollInterval) {
      g_info("New poll interval is %us.\n", guestInfoPollInterval / 1000);

      gatherTimeoutSource = g_timeout_source_new(guestInfoPollInterval);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, gatherTimeoutSource, GuestInfoGather, ctx, NULL);
      g_source_unref(gatherTimeoutSource);
   } else {
      g_info("Poll loop disabled.\n");
   }

   g_clear_error(&gError);
}


/*
 ******************************************************************************
 * BEGIN Tools Core Services goodies.
 */


/*
 ******************************************************************************
 * GuestInfoServerConfReload --                                          */ /**
 *
 * @brief Reconfigures the poll loop interval upon config file reload.
 *
 * @param[in]  src     The source object.
 * @param[in]  ctx     The application context.
 * @param[in]  data    Unused.
 *
 ******************************************************************************
 */

static void
GuestInfoServerConfReload(gpointer src,
                          ToolsAppCtx *ctx,
                          gpointer data)
{
   TweakGatherLoop(ctx, TRUE);
}


/*
 ******************************************************************************
 * GuestInfoServerIOFreeze --                                           */ /**
 *
 * IO freeze signal handler. Disables info gathering while I/O is frozen.
 * See bug 529653.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The application context.
 * @param[in]  freeze   Whether I/O is being frozen.
 * @param[in]  data     Unused.
 *
 ******************************************************************************
 */

static void
GuestInfoServerIOFreeze(gpointer src,
                        ToolsAppCtx *ctx,
                        gboolean freeze,
                        gpointer data)
{
   TweakGatherLoop(ctx, !freeze);
}


/*
 ******************************************************************************
 * GuestInfoServerShutdown --                                            */ /**
 *
 * Cleanup internal data on shutdown.
 *
 * @param[in]  src     The source object.
 * @param[in]  ctx     Unused.
 * @param[in]  data    Unused.
 *
 ******************************************************************************
 */

static void
GuestInfoServerShutdown(gpointer src,
                        ToolsAppCtx *ctx,
                        gpointer data)
{
   GuestInfoClearCache();

   if (gatherTimeoutSource != NULL) {
      g_source_destroy(gatherTimeoutSource);
      gatherTimeoutSource = NULL;
   }

#ifdef _WIN32
   NetUtil_FreeIpHlpApiDll();
#endif
}


/*
 ******************************************************************************
 * GuestInfoServerReset --                                               */ /**
 *
 * Reset callback - sets the internal flag that says we should purge all
 * caches.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 *
 ******************************************************************************
 */

static void
GuestInfoServerReset(gpointer src,
                     ToolsAppCtx *ctx,
                     gpointer data)
{
   vmResumed = TRUE;
}


/*
 ******************************************************************************
 * GuestInfoServerSendCaps --                                            */ /**
 *
 * Send capabilities callback.  If setting capabilities, sends VM's uptime.
 *
 * This is weird.  There's sort of an old Tools <-> VMX understanding that
 * vmsvc should report the guest's uptime in response to a "what're your
 * capabilities?" RPC.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The application context.
 * @param[in]  set      TRUE if setting capabilities, FALSE if unsetting them.
 * @param[in]  data     Client data.
 *
 * @retval NULL This function returns no capabilities.
 *
 ******************************************************************************
 */

static GArray *
GuestInfoServerSendCaps(gpointer src,
                        ToolsAppCtx *ctx,
                        gboolean set,
                        gpointer data)
{
   if (set) {
      SendUptime(ctx);
   }
   return NULL;
}


/*
 ******************************************************************************
 * GuestInfoServerSetOption --                                           */ /**
 *
 * Responds to a "broadcastIP" Set_Option command, by sending the primary IP
 * back to the VMX.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The application context.
 * @param[in]  option   Option name.
 * @param[in]  value    Option value.
 * @param[in]  data     Unused.
 *
 ******************************************************************************
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


/*
 ******************************************************************************
 * ToolsOnLoad --                                                        */ /**
 *
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 *
 ******************************************************************************
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "guestInfo",
      NULL,
      NULL
   };

   /*
    * This plugin is useless without an RpcChannel.  If we don't have one,
    * just bail.
    */
   if (ctx->rpc != NULL) {
      RpcChannelCallback rpcs[] = {
         { RPC_VMSUPPORT_START, GuestInfoVMSupport, &regData, NULL, NULL, 0 }
      };
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CAPABILITIES, GuestInfoServerSendCaps, NULL },
         { TOOLS_CORE_SIG_CONF_RELOAD, GuestInfoServerConfReload, NULL },
         { TOOLS_CORE_SIG_IO_FREEZE, GuestInfoServerIOFreeze, NULL },
         { TOOLS_CORE_SIG_RESET, GuestInfoServerReset, NULL },
         { TOOLS_CORE_SIG_SET_OPTION, GuestInfoServerSetOption, NULL },
         { TOOLS_CORE_SIG_SHUTDOWN, GuestInfoServerShutdown, NULL }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
         { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
      };

#ifdef _WIN32
      if (NetUtil_LoadIpHlpApiDll() != ERROR_SUCCESS) {
         g_warning("Failed to load iphlpapi.dll.  Cannot report networking details.\n");
         return NULL;
      }
#endif

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

      memset(&gInfoCache, 0, sizeof gInfoCache);
      vmResumed = FALSE;

      /*
       * Set up the GuestInfoGather loop.
       */
      TweakGatherLoop(ctx, TRUE);

      return &regData;
   }

   return NULL;
}


/*
 * END Tools Core Services goodies.
 ******************************************************************************
 */
