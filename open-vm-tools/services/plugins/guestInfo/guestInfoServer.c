/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
#include "dynbuf.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/vmbackup.h"

#if defined(_WIN32)
#include "guestStats.h"
#include "win32/guestInfoWin32.h"
#include <time.h>
#endif

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

/**
 * Default poll interval for guestInfo is 30s
 */
#define GUESTINFO_POLL_INTERVAL 30

/**
 * Default poll interval for guestStats is 20s
 */
#define GUESTINFO_STATS_INTERVAL 20

#define GUESTINFO_DEFAULT_DELIMITER ' '

/**
 * The default setting for exclude-nics
 */
#if defined(_WIN32)
#define GUESTINFO_DEFAULT_IFACE_EXCLUDES "vEthernet*"
#else
#define GUESTINFO_DEFAULT_IFACE_EXCLUDES "docker*,veth*,virbr*"
#endif

/*
 * Define what guest info types and NIC info versions could be sent to update
 * the NIC info in the VMX. The order defines a sequence of fallback paths to
 * provide backward compatibility for ESX running with NIC info version older
 * than the guest OS.
 */
typedef enum NicInfoMethod {
   NIC_INFO_V3_WITH_INFO_IPADDRESS_V3,
   NIC_INFO_V3_WITH_INFO_IPADDRESS_V2,
   NIC_INFO_V2_WITH_INFO_IPADDRESS_V2,
   NIC_INFO_V1_WITH_INFO_IPADDRESS,
   NIC_INFO_METHOD_MAX
} NicInfoMethod;

/*
 * Stores information about all guest information sent to the vmx.
 */

typedef struct _GuestInfoCache {
   /* Stores values of all key-value pairs. */
   char                        *value[INFO_MAX];
   HostinfoDetailedDataHeader  *detailedData;
   NicInfoV3                   *nicInfo;
   NicInfoMethod                method;
   GuestDiskInfoInt            *diskInfo;
   Bool                         diskInfoUseJson;
} GuestInfoCache;


/**
 * Defines the current poll interval (in milliseconds).
 *
 * This value is controlled by the guestinfo.poll-interval config file option.
 */
int guestInfoPollInterval = 0;

/**
 * The time when the guestInfo was last gathered.
 */
time_t gGuestInfoLastGatherTime = 0;

/**
 * Defines the current stats interval (in milliseconds).
 *
 * This value is controlled by the guestinfo.stats-interval config file option.
 */
int guestInfoStatsInterval = 0;

/*
 * Detailed guest OS data sending. Reset on channel reset.
 */

static Bool gSendDetailedGosData = TRUE;

/**
 * GuestInfo gather loop timeout source.
 */
static GSource *gatherInfoTimeoutSource = NULL;

/**
 * GuestStats gather loop timeout source.
 */
static GSource *gatherStatsTimeoutSource = NULL;

/* Local cache of the guest information that was last sent to vmx. */
static GuestInfoCache gInfoCache;

/*
 * A boolean flag that specifies whether the state of the VM was
 * changed since the last time guest info was sent to the VMX.
 * Tools daemon sets it to TRUE after the VM was resumed.
 */

static Bool gVMResumed;


/*
 * Local functions
 */


static Bool GuestInfoUpdateVMX(ToolsAppCtx *ctx,
                               GuestInfoType infoType,
                               void *info,
                               size_t infoSize);
static Bool SetGuestInfo(ToolsAppCtx *ctx,
                         GuestInfoType key,
                         const char *value);
static void SendUptime(ToolsAppCtx *ctx);
static Bool DiskInfoChanged(const GuestDiskInfoInt *diskInfo);
static void GuestInfoClearCache(void);
static GuestNicList *NicInfoV3ToV2(const NicInfoV3 *infoV3);
static void TweakGatherLoops(ToolsAppCtx *ctx,
                             gboolean enable);


/*
 ******************************************************************************
 * GuestInfoVMSupport --
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

    g_message("Starting vm-support script - %s\n", vmSupport);
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

     g_message("Starting vm-support script - %s\n", vmSupportCmdArgv[0]);
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
 * GuestInfoCheckIfRunningSlow --
 *
 * Checks the time when the guestInfo was last collected.
 * Logs a warning message and sends a RPC message to the VMX if
 * the function was called after longer than expected interval.
 *
 * @param[in]  ctx     The application context.
 *
 * @return None
 *
 ******************************************************************************
 */

static void
GuestInfoCheckIfRunningSlow(ToolsAppCtx *ctx)
{
   time_t now = time(NULL);

   if (gGuestInfoLastGatherTime != 0) {
      time_t delta = now - gGuestInfoLastGatherTime;
      /*
       * Have a long enough delta to ensure that we have really missed a
       * collection.
       */
      if (((int) delta * 1000) >= (2 * guestInfoPollInterval)) {
         gchar *msg, *rpcMsg;

         msg = g_strdup_printf(
                   "*** WARNING: GuestInfo collection interval longer than "
                   "expected; actual=%d sec, expected=%d sec. ***\n",
                   (int) delta, guestInfoPollInterval / 1000);

         rpcMsg = g_strdup_printf("log %s", msg);

         if (!RpcChannel_Send(ctx->rpc, rpcMsg, strlen(rpcMsg) + 1,
                              NULL, NULL)) {
            g_warning("%s: Error sending RPC message.\n", __FUNCTION__);
         }

         g_warning("%s", msg);

         g_free(rpcMsg);
         g_free(msg);
      }
   }

   gGuestInfoLastGatherTime = now;
}


/*
 ******************************************************************************
 * GuestInfoSetConfigList --
 *
 * Gets a list setting from the config, and sets the list pointed to by pList.
 *
 * @param[in]  ctx           the application context.
 * @param[in]  pCachedValue  a pointer to the location of the old value
 * @param[in]  configName    the configuration name
 * @param[in]  defaultValue  the default value
 * @param[out] pList         pointer to the pointer list, set when the value has
 *                           changed, otherwise this value will stay unchanged.
 *                           Caller needs to free this with g_strfreev().
 *
 * @return TRUE to indicate that the value has changed, FALSE otherwise
 ******************************************************************************
 */

static Bool
GuestInfoSetConfigList(ToolsAppCtx *ctx,
                       gchar **pCachedValue,
                       const gchar *configName,
                       const gchar *defaultValue,
                       gchar ***pList)
{
   gchar *listString = VMTools_ConfigGetString(ctx->config,
                                               CONFGROUPNAME_GUESTINFO,
                                               configName, defaultValue);
   if (g_strcmp0(listString, *pCachedValue) != 0) {
      gchar **list = NULL;

      if (listString != NULL && listString[0] != '\0') {
         list = g_strsplit(listString, ",", 0);
      }
      g_free(*pCachedValue);
      *pCachedValue = listString;
      *pList = list;

      return TRUE;
   } else {
      g_free(listString);
   }
   return FALSE;
}


/*
 * ****************************************************************************
 * GuestInfoResetNicPrimaryList --
 *
 * Gets primary-nics setting from the config, and sets the list of primary
 * patterns.
 *
 * @param[in]  ctx     The application context.
 *
 * @return TRUE to indicate that the value has changed, FALSE otherwise
 ******************************************************************************
 */

static Bool
GuestInfoResetNicPrimaryList(ToolsAppCtx *ctx)
{
   static gchar *ifacePrimaryStringCached = NULL;
   gchar **list;

   if (GuestInfoSetConfigList(ctx, &ifacePrimaryStringCached,
                             CONFNAME_GUESTINFO_PRIMARYNICS, NULL,
                             &list)) {
      GuestInfo_SetIfacePrimaryList(list);
      g_strfreev(list);
      return TRUE;
   }
   return FALSE;
}


/*
 * ****************************************************************************
 * GuestInfoResetNicLowPriorityList --
 *
 * Gets low-priority-nics setting from the config, and sets the list of low
 * priority patterns.
 *
 * @param[in]  ctx     The application context.
 *
 * @return TRUE to indicate that the value has changed, FALSE otherwise
 ******************************************************************************
 */

static Bool
GuestInfoResetNicLowPriorityList(ToolsAppCtx *ctx)
{
   static gchar *ifaceLowPriorityStringCached = NULL;
   gchar **list;

   if (GuestInfoSetConfigList(ctx, &ifaceLowPriorityStringCached,
                             CONFNAME_GUESTINFO_LOWPRIORITYNICS, NULL,
                             &list)) {
      GuestInfo_SetIfaceLowPriorityList(list);
      g_strfreev(list);
      return TRUE;
   }
   return FALSE;
}


/*
 * ****************************************************************************
 * GuestInfoResetNicExcludeList --
 *
 * Gets exclude-nics setting from the config, and sets the list of exclude
 * patterns.
 *
 * @param[in]  ctx     The application context.
 *
 * @return TRUE to indicate that the value has changed, FALSE otherwise
 ******************************************************************************
 */

static Bool
GuestInfoResetNicExcludeList(ToolsAppCtx *ctx)
{
   static gchar *ifaceExcludeStringCached = NULL;
   gchar **list;

   if (GuestInfoSetConfigList(ctx, &ifaceExcludeStringCached,
                             CONFNAME_GUESTINFO_EXCLUDENICS,
                             GUESTINFO_DEFAULT_IFACE_EXCLUDES,
                             &list)) {
      GuestInfo_SetIfaceExcludeList(list);
      g_strfreev(list);
      return TRUE;
   }
   return FALSE;
}


/*
 ******************************************************************************
 * GuestInfoDetailedDataIsEqual --
 *
 *    Compares two HostinfoDetailedDataHeader and the detailed data that
 *    follows each header.
 *
 * @returns True if equal
 *
 ******************************************************************************
 */

static Bool
GuestInfoDetailedDataIsEqual(const HostinfoDetailedDataHeader *info1,  // IN:
                             const HostinfoDetailedDataHeader *info2)  // IN:
{
   ASSERT(info1 != NULL);
   ASSERT(info2 != NULL);

   return (strncmp(info1->shortName, info2->shortName,
                   sizeof info1->shortName) == 0 &&
           strncmp(info1->fullName, info2->fullName,
                   sizeof info1->fullName) == 0 &&
           strcmp((char *)info1 + sizeof *info1,
                  (char *)info2 + sizeof *info2) == 0);
}


/*
 ******************************************************************************
 * GuestInfoFreeDetailedData --
 *
 * Free the HostinfoStructuredHeader and space allocated for the detailed
 * data.
 *
 * @returns None
 *
 ******************************************************************************
 */

static void
GuestInfoFreeDetailedData(HostinfoDetailedDataHeader *info)  // IN/OUT:
{
   free(info);
}


/*
 ******************************************************************************
 * GuestInfoGather --
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
#if !defined(USERWORLD)
   gboolean disableQueryDiskInfo;
   GuestDiskInfoInt *diskInfo = NULL;
#endif
   NicInfoV3 *nicInfo = NULL;
   ToolsAppCtx *ctx = data;
   Bool primaryChanged;
   Bool lowPriorityChanged;
   int maxIPv4RoutesToGather;
   int maxIPv6RoutesToGather;
   gchar *osNameOverride;
   gchar *osNameFullOverride;

   g_debug("Entered guest info gather.\n");

   GuestInfoCheckIfRunningSlow(ctx);

   /* Send tools version. */
   if (!GuestInfoUpdateVMX(ctx, INFO_BUILD_NUMBER, BUILD_NUMBER, 0)) {
      /*
       * An older vmx talking to new tools wont be able to handle
       * this message. Continue, if thats the case.
       */

      g_warning("Failed to update the VMX with tools version.\n");
   }

   /* Check for manual override of guest information in the config file */
   osNameOverride = VMTools_ConfigGetString(ctx->config,
                                            CONFGROUPNAME_GUESTOSINFO,
                                            CONFNAME_GUESTOSINFO_SHORTNAME,
                                            NULL);

   osNameFullOverride = VMTools_ConfigGetString(ctx->config,
                                                CONFGROUPNAME_GUESTOSINFO,
                                                CONFNAME_GUESTOSINFO_LONGNAME,
                                                NULL);
   /* If only the OS Full Name is provided, continue as normal, but emit
    * warning. */
   if (osNameOverride == NULL && osNameFullOverride != NULL) {
      g_warning("Ignoring " CONFNAME_GUESTOSINFO_LONGNAME " override.\n");
      g_warning("To use the GOS name override, "
                CONFNAME_GUESTOSINFO_SHORTNAME " must be present in the "
                "tools.conf file.\n");
      g_free(osNameFullOverride);
   }

   /* Only use override if at least the short OS name is provided */
   if (osNameOverride == NULL) {
      Bool sendOsNames = FALSE;
      char *osName = NULL;
      char *osFullName = NULL;
      char *detailedGosData = NULL;

      /* Gather all the relevant guest information. */
      osFullName = Hostinfo_GetOSName();
      osName = Hostinfo_GetOSGuestString();

      if (gSendDetailedGosData) {
         detailedGosData = Hostinfo_GetOSDetailedData();
      }

      if (detailedGosData == NULL) {
         g_debug("No detailed data.\n");
         sendOsNames = TRUE;
         gSendDetailedGosData = FALSE;
      } else {
         /* Build and attempt to send the detailed data */
         HostinfoDetailedDataHeader *detailedDataHeader = NULL;
         size_t infoHeaderSize;
         size_t detailedGosDataLen;
         size_t infoSize;

         g_debug("Sending detailed data.\n");
         detailedGosDataLen = strlen(detailedGosData);
         infoHeaderSize = sizeof *detailedDataHeader;
         infoSize = infoHeaderSize + detailedGosDataLen + 1; // cover NUL

         detailedDataHeader = g_malloc(infoSize);
         /* Clear struct and memory allocated for detailed data */
         memset(detailedDataHeader, 0, infoSize);

         /* Set the version of the detailed data header used */
         detailedDataHeader->version = HOSTINFO_STRUCT_HEADER_VERSION;

         if (osName == NULL) {
            g_warning("Failed to get OS name.\n");
         } else {
            Str_Strcpy(detailedDataHeader->shortName, osName,
                       sizeof detailedDataHeader->shortName);
         }
         if (osFullName == NULL) {
            g_warning("Failed to get OS full name.\n");
         } else {
            Str_Strcpy(detailedDataHeader->fullName, osFullName,
                       sizeof detailedDataHeader->fullName);
         }

         Str_Strcpy((char *)detailedDataHeader + infoHeaderSize,
                    detailedGosData, infoSize - infoHeaderSize);

         if (GuestInfoUpdateVMX(ctx, INFO_OS_DETAILED, detailedDataHeader,
                                infoSize)) {
            GuestInfoFreeDetailedData(gInfoCache.detailedData);
            gInfoCache.detailedData = detailedDataHeader;
            g_debug("Detailed data was sent successfully.\n");
         } else {
            /*
             * Only send the OS Name if the VMX failed to receive the detailed
             * data
             */
            gSendDetailedGosData = FALSE;
            sendOsNames = TRUE;
            g_debug("Detailed data was not sent successfully.\n");
         }
      }

      if (sendOsNames) {
         g_debug("Sending the short and long name\n");
         if (osFullName == NULL) {
            g_warning("Failed to get OS info.\n");
         } else {
            if (!GuestInfoUpdateVMX(ctx, INFO_OS_NAME_FULL, osFullName, 0)) {
               g_warning("Failed to update INFO_OS_NAME_FULL\n");
            }
         }
         if (osName == NULL) {
            g_warning("Failed to get OS info.\n");
         } else {
            if (!GuestInfoUpdateVMX(ctx, INFO_OS_NAME, osName, 0)) {
               g_warning("Failed to update INFO_OS_NAME\n");
            }
         }
      }

      free(detailedGosData);
      free(osFullName);
      free(osName);
   } else {
      /* Use osName and osNameFull provided in config file */
      if (osNameFullOverride == NULL) {
         g_warning(CONFNAME_GUESTOSINFO_LONGNAME " was not set in "
                   "tools.conf, using empty string.\n");
      }
      if (!GuestInfoUpdateVMX(ctx,
                              INFO_OS_NAME_FULL,
                              (osNameFullOverride == NULL) ? "" :
                                                             osNameFullOverride,
                              0)) {
         g_warning("Failed to send INFO_OS_NAME_FULL\n");
      }
      g_free(osNameFullOverride);

      if (!GuestInfoUpdateVMX(ctx, INFO_OS_NAME, osNameOverride, 0)) {
         g_warning("Failed to send INFO_OS_NAME\n");
      }
      g_free(osNameOverride);
      g_debug("Using values in tools.conf to override OS Name.\n");
   }

#if !defined(USERWORLD)
   disableQueryDiskInfo =
      g_key_file_get_boolean(ctx->config, CONFGROUPNAME_GUESTINFO,
                             CONFNAME_GUESTINFO_DISABLEQUERYDISKINFO, NULL);
   if (!disableQueryDiskInfo) {
      if ((diskInfo = GuestInfo_GetDiskInfo(ctx)) == NULL) {
         g_warning("Failed to get disk info.\n");
      } else {
         if (GuestInfoUpdateVMX(ctx, INFO_DISK_FREE_SPACE, diskInfo, 0)) {
            GuestInfo_FreeDiskInfo(gInfoCache.diskInfo);
            gInfoCache.diskInfo = diskInfo;
         } else {
            g_warning("Failed to update INFO_DISK_FREE_SPACE\n.");
            GuestInfo_FreeDiskInfo(diskInfo);
         }
      }
   }
#endif

   if (!System_GetNodeName(sizeof name, name)) {
      g_warning("Failed to get netbios name.\n");
   } else if (!GuestInfoUpdateVMX(ctx, INFO_DNS_NAME, name, 0)) {
      g_warning("Failed to update INFO_DNS_NAME.\n");
   }

   /* Get NIC information. */

   primaryChanged = GuestInfoResetNicPrimaryList(ctx);
   lowPriorityChanged = GuestInfoResetNicLowPriorityList(ctx);
   GuestInfoResetNicExcludeList(ctx);

   /*
    * Check the config registry for max IPv4/6 routes to gather
    */
   maxIPv4RoutesToGather =
         VMTools_ConfigGetInteger(ctx->config,
                                  CONFGROUPNAME_GUESTINFO,
                                  CONFNAME_GUESTINFO_MAXIPV4ROUTES,
                                  NICINFO_MAX_ROUTES);
   if (maxIPv4RoutesToGather < 0 ||
       maxIPv4RoutesToGather > NICINFO_MAX_ROUTES) {
      g_warning("Invalid %s.%s value: %d. Using default %u.\n",
                CONFGROUPNAME_GUESTINFO,
                CONFNAME_GUESTINFO_MAXIPV4ROUTES,
                maxIPv4RoutesToGather,
                NICINFO_MAX_ROUTES);
      maxIPv4RoutesToGather = NICINFO_MAX_ROUTES;
   }

   maxIPv6RoutesToGather =
         VMTools_ConfigGetInteger(ctx->config,
                                  CONFGROUPNAME_GUESTINFO,
                                  CONFNAME_GUESTINFO_MAXIPV6ROUTES,
                                  NICINFO_MAX_ROUTES);
   if (maxIPv6RoutesToGather < 0 ||
       maxIPv6RoutesToGather > NICINFO_MAX_ROUTES) {
      g_warning("Invalid %s.%s value: %d. Using default %u.\n",
                CONFGROUPNAME_GUESTINFO,
                CONFNAME_GUESTINFO_MAXIPV6ROUTES,
                maxIPv6RoutesToGather,
                NICINFO_MAX_ROUTES);
      maxIPv6RoutesToGather = NICINFO_MAX_ROUTES;
   }

   if (!GuestInfo_GetNicInfo(maxIPv4RoutesToGather,
                             maxIPv6RoutesToGather,
                             &nicInfo)) {
      g_warning("Failed to get NIC info.\n");
      /*
       * Return an empty NIC info.
       */
      nicInfo = Util_SafeCalloc(1, sizeof (struct NicInfoV3));
   }

   /*
    * We need to check if the setting for the primary interfaces or low
    * priority NICs have changed, because GuestInfo_IsEqual_NicInfoV3 does not
    * detect a change in the order.
    */
   if (!primaryChanged && !lowPriorityChanged &&
       GuestInfo_IsEqual_NicInfoV3(nicInfo, gInfoCache.nicInfo)) {
      g_debug("NIC info not changed.\n");
      GuestInfo_FreeNicInfo(nicInfo);
   } else if (GuestInfoUpdateVMX(ctx, INFO_IPADDRESS, nicInfo, 0)) {
      /*
       * Since the update succeeded, free the old cached object, and assign
       * ours to the cache.
       */
      GuestInfo_FreeNicInfo(gInfoCache.nicInfo);
      gInfoCache.nicInfo = nicInfo;
   } else {
      g_warning("Failed to update INFO_IPADDRESS.\n");
      GuestInfo_FreeNicInfo(nicInfo);
   }

   /* Send the uptime to the VMX so that it can detect soft resets. */
   SendUptime(ctx);

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
 * GuestInfoSendMemoryInfo --                                            */ /**
 *
 * Push memory informations about the guest to the vmx
 *
 * @param[in] ctx       Application context.
 * @param[in] infoSize  Size of the struct to send
 * @param[in] info      Struct that contains memory info
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with transmission.
 *
 ******************************************************************************
 */

static Bool
GuestInfoSendMemoryInfo(ToolsAppCtx *ctx,  // IN: Application context
                        uint64 infoSize,   // IN: Size of the following struct
                        void* info)        // IN: Pointer to GuestMemInfoRPC
{
   char *request;
   char header[32];
   size_t headerLen;
   size_t requestSize;
   Bool success = FALSE;

   Str_Sprintf(header, sizeof header, "%s  %d ", GUEST_INFO_COMMAND,
               INFO_MEMORY);

   headerLen = strlen(header);
   requestSize = headerLen + infoSize;

   request = g_malloc(requestSize);

   if (request == NULL) {
      g_warning("Failed to allocate GuestMemInfo memory.\n");
   } else {
      g_debug("Sending GuestMemInfo message.\n");

      /* Yes, stomp on the NUL at the end of the header */
      memcpy(request, header, headerLen);
      memcpy(request + headerLen, info, infoSize);

      /* Send all the information in the message. */
      success = RpcChannel_Send(ctx->rpc, request, requestSize, NULL, NULL);

      g_free(request);
   }

   if (success) {
      g_debug("GuestMemInfo sent successfully.\n");
   } else {
      g_warning("Error sending GuestMemInfo.\n");
   }

   return success;
}


/*
 ******************************************************************************
 *
 * NIC_INFO_V3_WITH_INFO_IPADDRESS_V3: Bump up the NICINFO_MAX_IPS to 2048
 * NIC_INFO_V3_WITH_INFO_IPADDRESS_V2: NICINFO_MAX_IPS to 64
 *
 * The current fallback paths:
 * +---------------+-------------------+-------------------+------------------+
 * |               | INFO_IPADDRESS_V3 | INFO_IPADDRESS_V2 | INFO_IPADDRESS   |
 * +---------------+-------------------+-------------------+------------------+
 * |  NIC_INFO_V3  |        (0)        |        (1)        |                  |
 * +---------------+-------------------+-------------------+------------------+
 * |  NIC_INFO_V2  |                   |        (2)        |                  |
 * +---------------+-------------------+-------------------+------------------+
 * |  NIC_INFO_V1  |                   |                   |        (3)       |
 * +---------------+-------------------+-------------------+------------------+
 *
 ******************************************************************************
 */


/*
 ******************************************************************************
 * GuestInfoSendNicInfoXdr --
 *
 * Push updated nic info to the VMX using XDR data format.
 *
 * @param[in] ctx      Application context.
 * @param[in] message  The protocol for a 'nic info' message.
 * @param[in] type     Guest information type.
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with serialization or transmission.
 *
 ******************************************************************************
 */

static Bool
GuestInfoSendNicInfoXdr(ToolsAppCtx *ctx,          // IN
                        GuestNicProto *message,    // IN
                        GuestInfoType type)        // IN
{
   Bool status = FALSE;
   XDR xdrs;
   gchar *request;
   char *reply = NULL;
   size_t replyLen;

   /* Add the RPC preamble: message name, and type. */
   request = g_strdup_printf("%s  %d ", GUEST_INFO_COMMAND, type);

   if (DynXdr_Create(&xdrs) == NULL) {
      goto exit;
   }

   /* Write preamble and serialized NIC info to XDR stream. */
   if (!DynXdr_AppendRaw(&xdrs, request, strlen(request)) ||
       !xdr_GuestNicProto(&xdrs, message)) {
      g_warning("Error serializing NIC info v%d data.", message->ver);
   } else {
      status = RpcChannel_Send(ctx->rpc, DynXdr_Get(&xdrs), xdr_getpos(&xdrs),
                               &reply, &replyLen);
      if (!status) {
         g_warning("%s: update failed: request \"%s\", reply \"%s\".\n",
                    __FUNCTION__, request, reply);
      }
      vm_free(reply);
   }
   DynXdr_Destroy(&xdrs, TRUE);

exit:
   g_free(request);
   return status;
}


/*
 ******************************************************************************
 * GuestInfoSendData --
 *
 * Push GuestInfo information to the VMX. So far, it is mainly used to
 * send the fixed NIC info V1 data.
 *
 * @param[in] ctx         Application context.
 * @param[in] info        Guest information data.
 * @param[in] infoLength  Length of guest information.
 * @param[in] type        Guest information type.
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with transmission.
 *
 ******************************************************************************
 */

static Bool
GuestInfoSendData(ToolsAppCtx *ctx,                // IN
                  void *info,                      // IN
                  u_int infoLength,                // IN
                  GuestInfoType type)              // IN
{
   Bool status;
   gchar *request;
   u_int msgLength = infoLength;
   gchar *message = NULL;
   char *reply = NULL;
   size_t replyLen;

   /* Add the RPC preamble: message name, and type. */
   request = g_strdup_printf("%s  %d ", GUEST_INFO_COMMAND, type);

   msgLength += strlen(request);
   message = g_malloc(msgLength);

   memcpy(message, request, strlen(request));
   memcpy(message + strlen(request), info, infoLength);

   status = RpcChannel_Send(ctx->rpc, message, msgLength, &reply, &replyLen);
   if (!status) {
      g_warning("%s: update failed: request \"%s\", reply \"%s\".\n",
                __FUNCTION__, request, reply);
   }
   vm_free(reply);

   g_free(message);
   g_free(request);

   return status;
}


/*
 ******************************************************************************
 * GuestNicInfoV3ToV3_64 --                                              */ /**
 *
 * New NicInfoV3 allows more IP addresses than the previous version. Preseve
 * the backward compatibility such that NicInfoV3 with old limit could still
 * go through without falling all the way to V2 or V1. It reduces the number
 * of IP addresses at each interface to the previous max limit 64.
 *
 * @param[in] info  NicInfoV3 container.
 *
 * @retval Pointer to a shallow copy of NicInfoV3 with reduced max IPs.
 *
 ******************************************************************************
 */

static NicInfoV3*
GuestNicInfoV3ToV3_64(NicInfoV3 *info)             // IN
{
   u_int idx;

   NicInfoV3 *info64 = g_malloc(sizeof(struct NicInfoV3));

   memcpy(info64, info, sizeof(struct NicInfoV3));
   info64->nics.nics_val =
      g_malloc(info64->nics.nics_len * sizeof(struct GuestNicV3));

   for (idx = 0; idx < info64->nics.nics_len; idx++) {
      memcpy(&info64->nics.nics_val[idx], &info->nics.nics_val[idx],
             sizeof(struct GuestNicV3));

      if (info64->nics.nics_val[idx].ips.ips_len > INFO_IPADDRESS_V2_MAX_IPS) {
         info64->nics.nics_val[idx].ips.ips_len = INFO_IPADDRESS_V2_MAX_IPS;
      }
   }

   return info64;
}


/*
 ******************************************************************************
 * GuestInfoSendNicInfo --
 *
 * Push updated NIC info to the VMX. Take care of failed transmissions or
 * unknown guest information types. Use a fixed sequence of fallback paths
 * to retry.
 *
 * @param[in] ctx   Application context.
 * @param[in] info  NicInfoV3 container.
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with serialization or transmission.
 *
 ******************************************************************************
 */

static Bool
GuestInfoSendNicInfo(ToolsAppCtx *ctx,             // IN
                     NicInfoV3 *info)              // IN
{
   Bool status = FALSE;
   GuestNicProto message = {0};
   NicInfoV3 *info64 = NULL;

   do {
      switch (gInfoCache.method) {
      case NIC_INFO_V3_WITH_INFO_IPADDRESS_V3:
         message.ver = NIC_INFO_V3;
         message.GuestNicProto_u.nicInfoV3 = info;
         if (GuestInfoSendNicInfoXdr(ctx, &message, INFO_IPADDRESS_V3)) {
            status = TRUE;
         }
         break;
      case NIC_INFO_V3_WITH_INFO_IPADDRESS_V2:
         info64 = GuestNicInfoV3ToV3_64(info);
         message.ver = NIC_INFO_V3;
         message.GuestNicProto_u.nicInfoV3 = info64;
         if (GuestInfoSendNicInfoXdr(ctx, &message, INFO_IPADDRESS_V2)) {
            status = TRUE;
         }
         break;
      case NIC_INFO_V2_WITH_INFO_IPADDRESS_V2:
         if (!info64) {
            info64 = GuestNicInfoV3ToV3_64(info);
         }
         {
            GuestNicList *nicList = NicInfoV3ToV2(info64);

            message.ver = NIC_INFO_V2;
            message.GuestNicProto_u.nicsV2 = nicList;
            if (GuestInfoSendNicInfoXdr(ctx, &message, INFO_IPADDRESS_V2)) {
               status = TRUE;
            }
            VMX_XDR_FREE(xdr_GuestNicList, nicList);
            free(nicList);
         }
         break;
      case NIC_INFO_V1_WITH_INFO_IPADDRESS:
         {
            GuestNicInfoV1 infoV1;

            GuestInfoConvertNicInfoToNicInfoV1(info, &infoV1);
            if (GuestInfoSendData(ctx, &infoV1, sizeof(infoV1),
                                  INFO_IPADDRESS)) {
               status = TRUE;
            }
         }
         break;
      default:
         g_error("Invalid nicInfo send method: %d\n", gInfoCache.method);
         break;
      }
   } while (!status && (++gInfoCache.method < NIC_INFO_METHOD_MAX));

   if (info64) {
      g_free(info64->nics.nics_val);
      g_free(info64);
   }

   if (status) {
      g_debug("Updating nicInfo successfully: method=%d\n", gInfoCache.method);
   } else {
      gInfoCache.method = NIC_INFO_V3_WITH_INFO_IPADDRESS_V3;
      g_warning("Fail to send nicInfo: method=%d status=%d\n",
                gInfoCache.method, status);
   }

   return status;
}


/*
 ******************************************************************************
 * GuestInfoSendDiskInfoV1 --
 *
 * Push updated Disk info to the VMX, using the json GUEST_DISK_INFO_COMMAND
 * RPC
 *
 * @param[in] ctx       Application context.
 * @param[in] pdi       GuestDiskInfoInt *
 * @param[in] infoSize  Size of disk info.
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with serialization or transmission.
 *
 ******************************************************************************
 */

static Bool
GuestInfoSendDiskInfoV1(ToolsAppCtx *ctx,             // IN
                        GuestDiskInfoInt *pdi,        // IN
                        size_t infoSize)              // IN
{
   DynBuf dynBuffer;
   char tmpBuf[1024];
   int len;
   char *infoReq;
   char *reply = NULL;
   size_t replyLen;
   Bool status;
#ifdef _WIN32
   Bool reportUUID = VMTools_ConfigGetBoolean(ctx->config,
                                              CONFGROUPNAME_GUESTINFO,
                                              CONFNAME_DISKINFO_REPORT_UUID,
                                              CONFIG_GUESTINFO_REPORT_UUID_DEFAULT);
#endif

   /*
    * Currently this format is fixed; the order should not be changed.
    * If a change is required, then DISK_INFO_VERSION must be bumped.
    * Older versions cannot be removed to maintain backwards compatibility
    * with older VMXs.
    */
   static char headerFmt[] = "%s {\n"
                             "\"" DISK_INFO_KEY_VERSION "\":\"%d\",\n"
                             "\"" DISK_INFO_KEY_DISKS "\":[\n";
   static char jsonPerDiskFmt[] = "{"
                                  "\"" DISK_INFO_KEY_DISK_NAME "\":\"%s\","
                                  "\"" DISK_INFO_KEY_DISK_FREE "\":\"%"FMT64"u\","
                                  "\"" DISK_INFO_KEY_DISK_SIZE "\":\"%"FMT64"u\"";
   static char jsonPerDiskFsTypeFmt[] = ",\"" DISK_INFO_KEY_DISK_FSTYPE "\":\"%s\"";
#ifdef _WIN32
   static char jsonPerDiskUUIDFmt[] = ",\"" DISK_INFO_KEY_DISK_UUID "\":\"%s\"";
#else
   static char jsonPerDiskDevArrHdrFmt[] =
                                    ",\"" DISK_INFO_KEY_DISK_DEVICE_ARR "\":[";
   static char jsonPerDiskDeviceFmt[] = "%s\"%s\"";
   static char jsonPerDiskDeviceSep[] = ",";
   static char jsonPerDiskDevArrFmtFooter[] = "]";
#endif
   static char jsonPerDiskFmtFooter[] = "},\n";
   static char jsonPerDiskFmtFooterLast[] = "}\n";
   static char jsonSuffix[] = "]}";
   int i;

   // 20 bytes per number for ascii representation
   // PARTITION_NAME_SIZE * 2 for name and (optional) uuid
   ASSERT_ON_COMPILE(sizeof tmpBuf > sizeof jsonPerDiskFmt +
                     PARTITION_NAME_SIZE * 2 + 20 + 20);

   DynBuf_Init(&dynBuffer);

   len = Str_Snprintf(tmpBuf, sizeof tmpBuf, headerFmt,
                      GUEST_DISK_INFO_COMMAND, DISK_INFO_VERSION_1);
   DynBuf_Append(&dynBuffer, tmpBuf, len);
   for (i = 0; i < pdi->numEntries; i++) {
      gchar *b64name;

      /*
       * If more than a single disk partition or filesystem to be reported,
       * terminate the previous partition element in the disk array.
       */
      if (i != 0) {
         DynBuf_Append(&dynBuffer, jsonPerDiskFmtFooter,
                       sizeof jsonPerDiskFmtFooter - 1);
      }

      /*
       * The '\' in Windows drive names needs escaping for json,
       * so use base64 since its simple and will cover other weird
       * cases like quotes, as well as avoid any utf-8 concerns.
       */
      b64name = g_base64_encode(pdi->partitionList[i].name,
                                strlen(pdi->partitionList[i].name));

      len = Str_Snprintf(tmpBuf, sizeof tmpBuf, jsonPerDiskFmt,
                         b64name,
                         pdi->partitionList[i].freeBytes,
                         pdi->partitionList[i].totalBytes);
      DynBuf_Append(&dynBuffer, tmpBuf, len);
      g_free(b64name);

      if (pdi->partitionList[i].fsType[0] != '\0') {
         len = Str_Snprintf(tmpBuf, sizeof tmpBuf, jsonPerDiskFsTypeFmt,
                            pdi->partitionList[i].fsType);
         DynBuf_Append(&dynBuffer, tmpBuf, len);
      }
#ifdef _WIN32
      if (reportUUID) {
         if (pdi->partitionList[i].uuid[0] != '\0') {
            len = Str_Snprintf(tmpBuf, sizeof tmpBuf, jsonPerDiskUUIDFmt,
                               pdi->partitionList[i].uuid);
            DynBuf_Append(&dynBuffer, tmpBuf, len);
         }
      }
#else
      if (pdi->partitionList[i].diskDevCnt > 0) {
         int idx;

         DynBuf_Append(&dynBuffer, jsonPerDiskDevArrHdrFmt,
                       sizeof jsonPerDiskDevArrHdrFmt - 1);
         len = Str_Snprintf(tmpBuf, sizeof tmpBuf, jsonPerDiskDeviceFmt,
                            "", pdi->partitionList[i].diskDevNames[0]);
         DynBuf_Append(&dynBuffer, tmpBuf, len);
         for (idx = 1; idx < pdi->partitionList[i].diskDevCnt; idx++) {
            len = Str_Snprintf(tmpBuf, sizeof tmpBuf, jsonPerDiskDeviceFmt,
                               jsonPerDiskDeviceSep,
                               pdi->partitionList[i].diskDevNames[idx]);
            DynBuf_Append(&dynBuffer, tmpBuf, len);
         }
         DynBuf_Append(&dynBuffer, jsonPerDiskDevArrFmtFooter,
                       sizeof jsonPerDiskDevArrFmtFooter - 1);
      }
#endif
   }
   if (pdi->numEntries > 0) {
      /* Terminate the last element of the disk partition JSON array. */
      DynBuf_Append(&dynBuffer, jsonPerDiskFmtFooterLast,
                    sizeof jsonPerDiskFmtFooterLast - 1);
   }

   DynBuf_Append(&dynBuffer, jsonSuffix, sizeof jsonSuffix - 1);

   infoReq = DynBuf_GetString(&dynBuffer);

   g_debug("%s: sending diskInfo RPC: '%s'\n", __FUNCTION__, infoReq);

   status = RpcChannel_Send(ctx->rpc, infoReq, strlen(infoReq) + 1, &reply,
                            &replyLen);
   if (status) {
      status = (*reply == '\0');
      if (!status) {
         g_debug("%s: unexpected reply '%s'\n", __FUNCTION__, reply);
      }
   } else {
      g_debug("%s: RPC failed (%d) reply '%s'\n",
              __FUNCTION__, status, reply ? reply : "");
   }

   DynBuf_Destroy(&dynBuffer);
   vm_free(reply);

   return status;
}


#if defined(_WIN64) && (_MSC_VER == 1500) && GLIB_CHECK_VERSION(2, 46, 0)
/*
 * Turn off optimizer for this compiler, since something with new glib
 * makes it go into an infinite loop, only on 64bit.
 */
#pragma optimize("", off)
#endif

/*
 ******************************************************************************
 * GuestInfoSendDiskInfoV0 --
 *
 * Push updated Disk info to the VMX, using the binary INFO_DISK_FREE_SPACE
 * format.
 *
 * @param[in] ctx       Application context.
 * @param[in] pdiInt    GuestDiskInfoInt *
 * @param[in] infoSize  Size of disk info.
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with serialization or transmission.
 *
 ******************************************************************************
 */

static Bool
GuestInfoSendDiskInfoV0(ToolsAppCtx *ctx,             // IN
                        GuestDiskInfoInt *pdiInt,     // IN
                        size_t infoSize)              // IN
{
   /*
    * 2 accounts for the digits of infotype and 3 for the three
    * spaces.
    */
   unsigned int requestSize = sizeof GUEST_INFO_COMMAND + 2 +
                              3 * sizeof (char);
   uint8 partitionCount;
   size_t offset;
   char *request;
   char *reply;
   size_t replyLen;
   Bool status;
   GuestDiskInfo *pdi;
   int i;

   ASSERT((pdiInt->numEntries && pdiInt->partitionList) ||
          (!pdiInt->numEntries && !pdiInt->partitionList));

   /*
    * Build a GuestDiskInfo structure to provide the expected binary
    * format for the binary RPC, copying out the V0 elements.
    */
   pdi = Util_SafeCalloc(1, sizeof *pdi);
   pdi->numEntries = pdiInt->numEntries;
   pdi->partitionList = Util_SafeCalloc(pdi->numEntries,
                                        sizeof *pdi->partitionList);
   for (i = 0; i < pdi->numEntries; i++) {
      pdi->partitionList[i].freeBytes = pdiInt->partitionList[i].freeBytes;
      pdi->partitionList[i].totalBytes = pdiInt->partitionList[i].totalBytes;
      Str_Strcpy(pdi->partitionList[i].name,
                 pdiInt->partitionList[i].name,
                 PARTITION_NAME_SIZE);
   }

   /* partitionCount is a uint8 and cannot be larger than UCHAR_MAX. */
   if (pdi->numEntries > UCHAR_MAX) {
      g_message("%s: Too many local filesystems (%d); truncating to %d entries\n",
                __FUNCTION__, pdi->numEntries, UCHAR_MAX);
      partitionCount = UCHAR_MAX;
   } else {
      partitionCount = pdi->numEntries;
   }

   requestSize += sizeof partitionCount +
                  sizeof *pdi->partitionList * partitionCount;
   request = Util_SafeCalloc(requestSize, sizeof *request);

   Str_Sprintf(request, requestSize, "%s  %d ", GUEST_INFO_COMMAND,
               INFO_DISK_FREE_SPACE);

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
             sizeof *pdi->partitionList * partitionCount);
   }

   g_debug("%s: sizeof request is %d\n", __FUNCTION__, requestSize);
   status = RpcChannel_Send(ctx->rpc, request, requestSize, &reply,
                            &replyLen);
   if (status) {
      status = (*reply == '\0');
      if (!status) {
         g_debug("%s: unexpected reply '%s'\n", __FUNCTION__, reply);
      }
   }

   vm_free(request);
   vm_free(reply);

   vm_free(pdi->partitionList);
   vm_free(pdi);

   return status;
}


/*
 ******************************************************************************
 * GuestInfoSendDiskInfo --
 *
 * Push updated Disk info to the VMX.
 *
 * @param[in] ctx       Application context.
 * @param[in] info      GuestDiskInfoInt *
 * @param[in] infoSize  Size of disk info.
 *
 * @retval TRUE  Update sent successfully.
 * @retval FALSE Had trouble with serialization or transmission.
 *
 ******************************************************************************
 */

static Bool
GuestInfoSendDiskInfo(ToolsAppCtx *ctx,             // IN
                      GuestDiskInfoInt *info,       // IN
                      size_t infoSize)              // IN
{
   if (gInfoCache.diskInfoUseJson &&
       GuestInfoSendDiskInfoV1(ctx, info, infoSize)) {
      return TRUE;
   } else {
      gInfoCache.diskInfoUseJson = FALSE;
   }

   /* fail over to unversioned format */
   g_debug("%s: using V0 (unversioned) diskinfo format\n", __FUNCTION__);

   return GuestInfoSendDiskInfoV0(ctx, info, infoSize);
}


/*
 ******************************************************************************
 * GuestInfoUpdateVMX --
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

static Bool
GuestInfoUpdateVMX(ToolsAppCtx *ctx,        // IN: Application context
                   GuestInfoType infoType,  // IN: guest information type
                   void *info,              // IN: type specific information
                   size_t infoSize)         // IN: size of *info
{
   ASSERT(info);
   g_debug("Entered update the VMX: %d.\n", infoType);

   if (gVMResumed) {
      gVMResumed = FALSE;
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

   case INFO_OS_DETAILED:
      {
         if (gInfoCache.detailedData != NULL &&
             GuestInfoDetailedDataIsEqual(gInfoCache.detailedData,
                                          (HostinfoDetailedDataHeader *)info)) {
            /* The value has not changed */
            g_debug("Value unchanged for detailedData.\n");
            break;
         }

         if (!GuestInfoSendData(ctx, info, infoSize, INFO_OS_DETAILED)) {
            g_warning("Failed to update detailed data");
            return FALSE;
         }
         break;
      }

   case INFO_IPADDRESS:
      {
         if (!GuestInfoSendNicInfo(ctx, (NicInfoV3 *) info)) {
            g_warning("Failed to update NIC information.\n");
            return FALSE;
         }
         break;
      }

   case INFO_MEMORY:
      {
         if (!GuestInfoSendMemoryInfo(ctx, infoSize, info)) {
            g_warning("Unable to send GuestMemInfo\n");
            return FALSE;
         }
         break;
      }

   case INFO_DISK_FREE_SPACE:
      {
         if (!DiskInfoChanged(info)) {
            g_debug("Disk info not changed.\n");
            break;
         }

         if (!GuestInfoSendDiskInfo(ctx, info, infoSize)) {
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

   g_debug("Returning after updating guest information: %d\n", infoType);
   return TRUE;
}
#if defined(_WIN64) && (_MSC_VER == 1500) && GLIB_CHECK_VERSION(2, 46, 0)
/*
 * Restore optimizer.
 */
#pragma optimize("", on)
#endif


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
   GuestInfoUpdateVMX(ctx, INFO_UPTIME, uptime, 0);
   g_free(uptime);
}


/*
 ******************************************************************************
 * SetGuestInfo --
 *
 * Sends a simple key-value update request to the VMX.
 *
 * @param[in] ctx       Application context.
 * @param[in] key       Key sent to the VMX
 * @param[in] value     GuestInfo data sent to the VMX
 *
 * @retval TRUE  RPCI succeeded.
 * @retval FALSE RPCI failed.
 *
 ******************************************************************************
 */

Bool
SetGuestInfo(ToolsAppCtx *ctx,   // IN:
             GuestInfoType key,  // IN:
             const char *value)  // IN:
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
      g_warning("Error sending RPC message: %s\n", reply ? reply : "NULL");
      vm_free(reply);
      return FALSE;
   }

   /* The reply indicates whether the key,value pair was updated in the VMX. */
   status = (*reply == '\0');
   vm_free(reply);
   return status;
}


/*
 ******************************************************************************
 * GuestInfoFindMacAddress --
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
 * DiskInfoChanged --
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
DiskInfoChanged(const GuestDiskInfoInt *diskInfo)
{
   int index;
   char *name;
   int i;
   int matchedPartition;
   GuestDiskInfoInt *cachedDiskInfo;

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
 * GuestInfoClearCache --
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

   GuestInfoFreeDetailedData(gInfoCache.detailedData);
   gInfoCache.detailedData = NULL;

   GuestInfo_FreeDiskInfo(gInfoCache.diskInfo);
   gInfoCache.diskInfo = NULL;
   gInfoCache.diskInfoUseJson = TRUE;

   GuestInfo_FreeNicInfo(gInfoCache.nicInfo);
   gInfoCache.nicInfo = NULL;

   gInfoCache.method = NIC_INFO_V3_WITH_INFO_IPADDRESS_V3;
}


/*
 ***********************************************************************
 * NicInfoV3ToV2 --
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
   unsigned int i, j;
   GuestNicList *nicList = Util_SafeCalloc(sizeof *nicList, 1);

   (void)XDRUTIL_ARRAYAPPEND(nicList, nics, infoV3->nics.nics_len);
   XDRUTIL_FOREACH(i, infoV3, nics) {
      GuestNicV3 *nic = XDRUTIL_GETITEM(infoV3, nics, i);
      GuestNic *oldNic = XDRUTIL_GETITEM(nicList, nics, i);

      Str_Strcpy(oldNic->macAddress, nic->macAddress,
                 sizeof oldNic->macAddress);

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
 * TweakGatherLoop --
 *
 * @brief Start, stop, reconfigure a GuestInfoGather poll loop.
 *
 * This function is responsible for creating, manipulating, and resetting a
 * GuestInfoGather loop timeout source.
 *
 * @param[in]     ctx           The app context.
 * @param[in]     enable        Whether to enable the gather loop.
 * @param[in]     cfgKey        Config key to fetch user pref.
 * @param[in]     defInterval   Default interval value in seconds.
 * @param[in]     callback      Function to be called on expiry of interval.
 * @param[in,out] currInterval  Current interval value in seconds.
 * @param[out]    timeoutSource GSource object created.
 *
 ******************************************************************************
 */

static void
TweakGatherLoop(ToolsAppCtx *ctx,
                gboolean enable,
                gchar *cfgKey,
                gint defInterval,
                GSourceFunc callback,
                gint *currInterval,
                GSource **timeoutSource)
{
   gint pollInterval = 0;

   if (enable) {
      pollInterval = defInterval * 1000;

      /*
       * Check the config registry for custom poll interval,
       * converting from seconds to milliseconds.
       */
      if (g_key_file_has_key(ctx->config, CONFGROUPNAME_GUESTINFO,
                             cfgKey, NULL)) {
         GError *gError = NULL;

         pollInterval = g_key_file_get_integer(ctx->config,
                                               CONFGROUPNAME_GUESTINFO,
                                               cfgKey, &gError);
         pollInterval *= 1000;

         if (pollInterval < 0 || gError) {
            g_warning("Invalid %s.%s value. Using default %us.\n",
                      CONFGROUPNAME_GUESTINFO, cfgKey, defInterval);
            pollInterval = defInterval * 1000;
         }

         g_clear_error(&gError);
      }
   }

   if (*timeoutSource != NULL) {
      /*
       * If the interval hasn't changed, let's not interfere with the existing
       * timeout source.
       */
      if (pollInterval == *currInterval) {
         ASSERT(pollInterval);
         return;
      }

      /*
       * Destroy the existing timeout source since the interval has changed.
       */

      g_source_destroy(*timeoutSource);
      *timeoutSource = NULL;
   }

   /*
    * All checks have passed.  Create a new timeout source and attach it.
    */
   *currInterval = pollInterval;

   if (*currInterval) {
      g_info("New value for %s is %us.\n", cfgKey, *currInterval / 1000);

      *timeoutSource = g_timeout_source_new(*currInterval);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, *timeoutSource, callback, ctx, NULL);
      g_source_unref(*timeoutSource);
   } else {
      g_info("Poll loop for %s disabled.\n", cfgKey);
   }
}


/*
 ******************************************************************************
 * TweakGatherLoops --
 *
 * @brief Start, stop, reconfigure the GuestInfoGather loops.
 *
 * This function is responsible for creating, manipulating, and resetting
 * the GuestInfoGather loops (info and stats) timeout sources.
 *
 * @param[in]  ctx      The app context.
 * @param[in]  enable   Whether to enable the gather loops.
 *
 * @sa CONFNAME_GUESTINFO_POLLINTERVAL
 * @sa CONFNAME_GUESTINFO_STATSINTERVAL
 *
 ******************************************************************************
 */

static void
TweakGatherLoops(ToolsAppCtx *ctx,
                 gboolean enable)
{
#if defined(__linux__) || defined(USERWORLD) || defined(_WIN32)
   gboolean perfmonEnabled;

   perfmonEnabled = !g_key_file_get_boolean(ctx->config,
                                            CONFGROUPNAME_GUESTINFO,
                                            CONFNAME_GUESTINFO_DISABLEPERFMON,
                                            NULL);

   if (perfmonEnabled) {
      /*
       * Tweak GuestStats gather loop
       */
      TweakGatherLoop(ctx, enable,
                      CONFNAME_GUESTINFO_STATSINTERVAL,
                      GUESTINFO_STATS_INTERVAL,
                      GuestInfo_StatProviderPoll,
                      &guestInfoStatsInterval,
                      &gatherStatsTimeoutSource);
   } else {
      /*
       * Destroy the existing timeout source, if it exists.
       */
      if (gatherStatsTimeoutSource != NULL) {
         g_source_destroy(gatherStatsTimeoutSource);
         gatherStatsTimeoutSource = NULL;

         g_info("PerfMon gather loop disabled.\n");
      }
   }
#endif

   /*
    * Tweak GuestInfo gather loop
    */
   TweakGatherLoop(ctx, enable,
                   CONFNAME_GUESTINFO_POLLINTERVAL,
                   GUESTINFO_POLL_INTERVAL,
                   GuestInfoGather,
                   &guestInfoPollInterval,
                   &gatherInfoTimeoutSource);
}


/*
 ******************************************************************************
 *
 * GuestInfo_ServerReportStats --
 *
 *      Report gathered stats.
 *
 * Results:
 *      Stats reported to the VMX. Returns FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 ******************************************************************************
 */

Bool
GuestInfo_ServerReportStats(ToolsAppCtx *ctx,  // IN
                            DynBuf *stats)     // IN
{
   return GuestInfoUpdateVMX(ctx,
                             INFO_MEMORY,
                             DynBuf_Get(stats),
                             DynBuf_GetSize(stats));
}


/*
 ******************************************************************************
 * BEGIN Tools Core Services goodies.
 */


/*
 ******************************************************************************
 * GuestInfoServerConfReload --
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
   TweakGatherLoops(ctx, TRUE);
}


/*
 ******************************************************************************
 * GuestInfoServerIOFreeze --
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
   TweakGatherLoops(ctx, !freeze);
}


/*
 ******************************************************************************
 * GuestInfoServerShutdown --
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

   GuestInfo_SetIfaceExcludeList(NULL);

   if (gatherInfoTimeoutSource != NULL) {
      g_source_destroy(gatherInfoTimeoutSource);
      gatherInfoTimeoutSource = NULL;
   }

   if (gatherStatsTimeoutSource != NULL) {
      g_source_destroy(gatherStatsTimeoutSource);
      gatherStatsTimeoutSource = NULL;
   }

#if defined(__linux__) || defined(USERWORLD) || defined(_WIN32)
   GuestInfo_StatProviderShutdown();
#endif

#ifdef _WIN32
   NetUtil_FreeIpHlpApiDll();
#endif
}


/*
 ******************************************************************************
 * GuestInfoServerReset --
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
   gVMResumed = TRUE;

   /* Reset the last gather time */
   gGuestInfoLastGatherTime = 0;

   /* Reset detailed guest OS data sending */
   gSendDetailedGosData = TRUE;
}


/*
 ******************************************************************************
 * GuestInfoServerSendCaps --
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
 * GuestInfoServerSetOption --
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
   gchar *msg;

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

   ip = GuestInfo_GetPrimaryIP();

   msg = g_strdup_printf("info-set guestinfo.ip %s", ip);
   ret = RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL);
   vm_free(ip);
   g_free(msg);

exit:
   return (gboolean) ret;
}


/*
 ******************************************************************************
 * ToolsOnLoad --
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

      ToolsCore_InitializeCOM(ctx);
#endif

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

      memset(&gInfoCache, 0, sizeof gInfoCache);
      gVMResumed = FALSE;
      gInfoCache.method = NIC_INFO_V3_WITH_INFO_IPADDRESS_V3;

      /*
       * Set up the GuestInfo gather loops.
       */
      TweakGatherLoops(ctx, TRUE);

      return &regData;
   }

   return NULL;
}


/*
 * END Tools Core Services goodies.
 ******************************************************************************
 */
