/*********************************************************
 * Copyright (C) 2002-2022 VMware, Inc. All rights reserved.
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
 * conf.h --
 *
 *    Manage the tools configuration file.
 *
 */


#ifndef __CONF_H__
#define __CONF_H__

#define CONF_FILE         "tools.conf"

#if ! defined(_WIN32)
#   define CONFVAL_POWERONSCRIPT_DEFAULT  "poweron-vm-default"
#   define CONFVAL_POWEROFFSCRIPT_DEFAULT "poweroff-vm-default"
#   define CONFVAL_RESUMESCRIPT_DEFAULT   "resume-vm-default"
#   define CONFVAL_SUSPENDSCRIPT_DEFAULT  "suspend-vm-default"
#else
#   define CONFVAL_POWERONSCRIPT_DEFAULT  "poweron-vm-default.bat"
#   define CONFVAL_POWEROFFSCRIPT_DEFAULT "poweroff-vm-default.bat"
#   define CONFVAL_RESUMESCRIPT_DEFAULT   "resume-vm-default.bat"
#   define CONFVAL_SUSPENDSCRIPT_DEFAULT  "suspend-vm-default.bat"
#endif

#define CONFNAME_POWERONSCRIPT            "poweron-script"
#define CONFNAME_POWEROFFSCRIPT           "poweroff-script"
#define CONFNAME_RESUMESCRIPT             "resume-script"
#define CONFNAME_SUSPENDSCRIPT            "suspend-script"
#define CONFNAME_LOG                      "log"
#define CONFNAME_LOGFILE                  "log.file"
#define CONFNAME_LOGLEVEL                 "log.level"
#define CONFNAME_DISABLETOOLSVERSION      "disable-tools-version"
#define CONFNAME_HIDETOOLSVERSION         "hide-tools-version"
#define CONFNAME_DISABLEPMTIMERWARNING    "disable-pmtimerwarning"
#define CONFGROUPNAME_VMTOOLS             "vmtools"

/*
 ******************************************************************************
 * BEGIN AppInfo goodies.
 */

/**
 * Defines the string used for the AppInfo config file group.
 */
#define CONFGROUPNAME_APPINFO "appinfo"

/**
 * Define a custom AppInfo poll interval (in seconds).
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * poll interval.
 *
 * @param int   User-defined poll interval.  Set to 0 to disable polling.
 */
#define CONFNAME_APPINFO_POLLINTERVAL "poll-interval"

/**
 * Defines the configuration to publish the application information or not.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * value.
 *
 * @param boolean Set to TRUE to disable publishing.
 *                Set to FALSE to enable publishing.
 */
#define CONFNAME_APPINFO_DISABLED "disabled"

/**
 * Defines the configuration to remove duplicate applications.
 *
 * @param boolean Set to TRUE to remove duplicate apps.
 *                Set to FALSE to keep duplicate apps.
 */
#define CONFNAME_APPINFO_REMOVE_DUPLICATES "remove-duplicates"

/**
 * Defines the configuration to use the WMI for getting the application
 * version information.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * value.
 *
 * @param boolean Set to TRUE to use WMI.
 *                Set to FALSE to use native Win32 APIs.
 */
#define CONFNAME_APPINFO_USE_WMI "useWMI"

/*
 * END AppInfo goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN containerInfo goodies.
 */

/**
 * Defines the string used for the ContainerInfo config file group.
 */
#define CONFGROUPNAME_CONTAINERINFO "containerinfo"

/**
 * Define a custom ContainerInfo poll interval (in seconds).
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * poll interval.
 *
 * @param int   User-defined poll interval.  Set to 0 to disable polling.
 */
#define CONFNAME_CONTAINERINFO_POLLINTERVAL "poll-interval"

/**
 * Define the limit on the maximum number of containers to collect info from.
 * If the number of running containers exceeds the limit, only the most recently
 * created containers will be published.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * max container limit.
 *
 * @param int   User-defined max limit for # of containers queried.
 */
#define CONFNAME_CONTAINERINFO_LIMIT "max-containers"

/**
 * Defines the configuration to remove duplicate containers.
 *
 * @param boolean Set to TRUE to remove duplicate containers.
 *                Set to FALSE to keep duplicate containers.
 */
#define CONFNAME_CONTAINERINFO_REMOVE_DUPLICATES "remove-duplicates"

/**
 * Define the docker unix socket to use to communicate with the docker daemon.
 *
 * @note Illegal values result in a @c g_warning and fallback to docker's
 * default unix socket.
 *
 * @param string  absolute file path of the docker unix socket in use.
 */
#define CONFNAME_CONTAINERINFO_DOCKERSOCKET "docker-unix-socket"

/**
 * Define the containerd unix socket to connect with containerd gRPC server.
 * This should be used to get containers.
 *
 * @note Illegal values result in a @c g_warning and fallback to the containerd
 * default unix socket.
 *
 * @param string  absolute file path of the containerd unix socket in use.
 */
#define CONFNAME_CONTAINERINFO_CONTAINERDSOCKET "containerd-unix-socket"

/**
 * Define the list of namespaces to be queried for the running containers.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * list of namespaces.
 *
 * @param string  Comma separated list of namespaces to be queried.
 */
#define CONFNAME_CONTAINERINFO_ALLOWED_NAMESPACES "allowed-namespaces"

/*
 * END containerInfo goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN ServiceDiscovery goodies.
 */

/**
 * Defines the string used for the ServiceDiscovery config file group.
 */
#define CONFGROUPNAME_SERVICEDISCOVERY "servicediscovery"

/**
 * Defines the configuration to perform service discovery or not.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * value.
 *
 * @param boolean Set to TRUE to disable publishing.
 *                Set to FALSE to enable publishing.
 */
#define CONFNAME_SERVICEDISCOVERY_DISABLED "disabled"

/*
 * END ServiceDiscovery goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN ComponentMgr goodies.
 */

/**
 * Defines the current poll interval (in seconds).
 * This value is controlled by the componentMgr.poll-interval config file
 * option.
 */
#define COMPONENTMGR_CONF_POLLINTERVAL "poll-interval"

/**
 * Name of section of configuration to be read from tools.conf.
 */
#define COMPONENTMGR_CONF_GROUPNAME "componentmgr"

/**
 * Defines the components  managed by the componentMgr plugin.
 * This value is controlled by the componentMgr.included config file option.
 */
#define COMPONENTMGR_CONF_INCLUDEDCOMPONENTS "included"

/*
 * END ComponentMgr goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN GuestStore upgrader goodies.
 ******************************************************************************
 */

/**
 * Defines the string used for the GuestStore upgrade config file group.
 */
#define CONFGROUPNAME_GSUPGRADE "gueststoreupgrade"

/**
 * Defines the value for GuestStore upgrade feature to be enabled or not.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * value.
 *
 * @param string  Set to "off" no tools upgrade from GuestStore.
 *                Set to "manual" tools upgrade from GuestStore manual start.
 *                Set to "immediate" tools upgrade from GuestStore start
 *                autoCONFNAME_GSUPGRADE_RESOURCEmatically checking on the poll-interval frequency.
 *                Set to "powercycle" tools upgrade from GuestStore on system
 *                power on.
 */
#define CONFNAME_GSUPGRADE_POLICY "policy"

/**
 * Define a custom GuestStore check poll interval (in seconds).
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * poll interval.
 *
 * @param int   User-defined poll interval.  Set to 0 to disable polling.
 */
#define CONFNAME_GSUPGRADE_POLLINTERVAL "poll-interval"

/**
 * Define a custom GuestStore periodic Upgrade interval (in seconds).
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * upgrade interval.
 *
 * @param int   User-defined upgrade interval.  Set to 0 to disable polling.
 */
#define CONFNAME_GSUPGRADE_UPGRADEINTERVAL "upgrade-interval"

/**
 * Define a custom GuestStore content path prefix.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * resource path.
 *
 * @param string  User-defined GuestStore resource path.
 */
#define CONFNAME_GSUPGRADE_RESOURCE "resource"

/**
 * Define a custom GuestStore content vmtools key.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * vmtools key.
 *
 * @param string  User-defined GuestStore vmtools key.
 *                Set to "vmtools" for the latest version.
 *                Suggested examples for earlier versions are:
 *                Set to "vmtools-11.0.0" for "vmtools-11.0.0/"
 *                Set to "vmtools-11.1.0" for "vmtools-11.1.0/"
 *                Set to "vmtools-11.2.0" for "vmtools-11.2.0/"
 *                Set to "vmtools-11.3.0" for "vmtools-11.3.0/"
 */
#define CONFNAME_GSUPGRADE_VMTOOLS_VERSION "vmtools-version-key"

/*
 * END GuestStore upgrader goodies.
 ******************************************************************************
 */


/*
 ******************************************************************************
 * BEGIN GlobalConf goodies.
 */

/**
 * Defines the string used for the GlobalConf config file group.
 */
#define CONFGROUPNAME_GLOBALCONF "globalconf"

/**
 * Defines the configuration to enable/disable the GlobalConf module.
 *
 * @note Illegal values result in @c g_warning and fallback to the default
 * value.
 *
 * @param boolean Set to TRUE to enable the module.
 *                Set to FALSE to disable the module.
 */
#define CONFNAME_GLOBALCONF_ENABLED "enabled"

/**
 * Define a custom GlobalConf poll interval (in seconds).
 *
 * @note Illegal values result in @c g_warning and fallback to the default
 * value.
 *
 * @param int   User-defined poll interval.
 */
#define CONFNAME_GLOBALCONF_POLL_INTERVAL "poll-interval"

/**
 * Define the global configuration resource in GuestStore.
 *
 * @note Illegal values result in @c g_warning and fallback to the default
 * value.
 *
 * @param string   Resource identifier in GuestStore.
 */
#define CONFNAME_GLOBALCONF_RESOURCE "resource"

/*
 * END GlobalConf goodies.
 ******************************************************************************
 */


/*
 ******************************************************************************
 * BEGIN GuestInfo goodies.
 */

/**
 * Defines the string used for the GuestInfo config file group.
 */
#define CONFGROUPNAME_GUESTINFO "guestinfo"

/**
 * Lets user disable just the perf monitor.
 */
#define CONFNAME_GUESTINFO_DISABLEPERFMON "disable-perf-mon"

/**
 * Lets user disable just DiskInfo.
 *
 * If thinking of deprecating this, please read bug 535343 first.
 */
#define CONFNAME_GUESTINFO_DISABLEQUERYDISKINFO "disable-query-diskinfo"

/**
 * Define a custom GuestInfo poll interval (in seconds).
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * poll interval.
 *
 * @param int   User-defined poll interval.  Set to 0 to disable polling.
 */
#define CONFNAME_GUESTINFO_POLLINTERVAL "poll-interval"

/**
 * Define a custom GuestStats poll interval (in seconds).
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * stats interval.
 *
 * @param int   User-defined poll interval for stats.  Set to 0 to disable polling.
 */
#define CONFNAME_GUESTINFO_STATSINTERVAL "stats-interval"

/**
 * Indicates whether stat results should be written to the log.
 */
#define CONFNAME_GUESTINFO_ENABLESTATLOGGING "enable-stat-logging"

/**
 * Set a comma separated list of network interface names that can be the
 * primary one
 *
 * @note interface names can use wildcards like '*' and '?'
 *
 * @param string comma separated list of interface name patterns.
 */

#define CONFNAME_GUESTINFO_PRIMARYNICS "primary-nics"

/**
 * Set a comma separated list of network interface names that have
 * low priority (so they will be sorted to the end).
 *
 * @note interface names can use wildcards like '*' and '?'
 *
 * @param string comma separated list of interface name patterns.
 */

#define CONFNAME_GUESTINFO_LOWPRIORITYNICS "low-priority-nics"

/**
 * Set a comma separated list of network interface names that shall be ignored.
 *
 * @note interface names can use wildcards like '*' and '?'
 *
 * @param string comma separated list of interface name patterns.
 */

#define CONFNAME_GUESTINFO_EXCLUDENICS "exclude-nics"

/**
 * Define a custom max IPv4 routes to gather.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * NICINFO_MAX_ROUTES.
 *
 * @param int   User-defined max routes with range [0, NICINFO_MAX_ROUTES].
 *              Set to 0 to disable gathering.
 */
#define CONFNAME_GUESTINFO_MAXIPV4ROUTES "max-ipv4-routes"

/**
 * Define a custom max IPv6 routes to gather.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * NICINFO_MAX_ROUTES.
 *
 * @param int   User-defined max routes with range [0, NICINFO_MAX_ROUTES].
 *              Set to 0 to disable gathering.
 */
#define CONFNAME_GUESTINFO_MAXIPV6ROUTES "max-ipv6-routes"

/**
 * Lets user include reserved space in diskInfo space metrics on Linux.
 *
 * @param boolean Set to true to include reserved space.
 */
#define CONFNAME_DISKINFO_INCLUDERESERVED "diskinfo-include-reserved"

/**
 * Report UUID of disks for vmdk mapping via vim.
 *
 * @param boolean Set to true to report UUID to VMX.
 */
#define CONFNAME_DISKINFO_REPORT_UUID "diskinfo-report-uuid"

/**
 * Avoid buggy USB driver when querying disks for UUID.
 * Turning this on can result in a major delay if the vmdk is
 * being replicated.  See PR 2575285, 26539.
 *
 * @param boolean Set to true to work around buggy 3rd party driver.
 */
#define CONFNAME_DISKINFO_DRIVER_WORKAROUND "diskinfo-usb-workaround"

/**
 * Report Linux disk device for vmdk mapping via vim.
 *
 * @param boolean Set to true to report devices to VMX.
 */
#define CONFNAME_DISKINFO_REPORT_DEVICE "diskinfo-report-device"

/*
 * END GuestInfo goodies.
 ******************************************************************************
 */


/*
 ******************************************************************************
 * BEGIN GuestOSInfo goodies.
 */

/**
 * Defines the string used for the GuestOSInfo config file group.
 */
#define CONFGROUPNAME_GUESTOSINFO "guestosinfo"

/**
 * Lets users override the short OS name sent by Tools.
 */
#define CONFNAME_GUESTOSINFO_SHORTNAME "short-name"

/**
 * Lets users override the long OS name sent by Tools.
 */
#define CONFNAME_GUESTOSINFO_LONGNAME "long-name"

/*
 * END GuestOSInfo goodies.
 ******************************************************************************
 */


/*
 ******************************************************************************
 * BEGIN Unity goodies.
 */

/**
 * Defines the string used for the Unity config file group.
 */
#define CONFGROUPNAME_UNITY "unity"

/**
 * Lets users override system decisions about whether unity should be available.
 */
#define CONFNAME_UNITY_FORCEENABLE "forceEnable"

/**
 * Lets users override the desktop background color when in Unity mode.
 */
#define CONFNAME_UNITY_BACKGROUNDCOLOR "desktop.backgroundColor"

/**
 * Lets users enable (or disable) the Protocol Buffer enabled service
 */
#define CONFNAME_UNITY_ENABLEPBRPC "pbrpc.enable"

/**
 * Lets users configure the socket type for the PBRPC Services
 */
#define CONFNAME_UNITY_PBRPCSOCKETTYPE "pbrpc.socketType"
#define CONFNAME_UNITY_PBRPCSOCKETTYPE_IPSOCKET "ipsocket"
#define CONFNAME_UNITY_PBRPCSOCKETTYPE_VSOCKET "vsocket"

/*
 * END Unity goodies.
 ******************************************************************************
 */


/*
 ******************************************************************************
 * BEGIN deployPkg goodies.
 */

/**
 * Defines the string used for the deployPkg config file group.
 */
#define CONFGROUPNAME_DEPLOYPKG "deployPkg"

/**
 * Lets users configure the process timeout value in deployPkg
 * Valid value range: 0x01 ~ 0xFF
 */
#define CONFNAME_DEPLOYPKG_PROCESSTIMEOUT "process-timeout"

/**
 * The guest customization is allowed or not
 * Valid value: true / false
 */
#define CONFNAME_DEPLOYPKG_ENABLE_CUST "enable-customization"

/*
 * END deployPkg goodies.
 ******************************************************************************
 */


/** Where to find Tools data in the Win32 registry. */
#define CONF_VMWARE_TOOLS_REGKEY    "Software\\VMware, Inc.\\VMware Tools"

/* Wait 5 seconds between polls to see if the conf file has changed */
#define CONF_POLL_TIME     5

/*
 ******************************************************************************
 * BEGIN upgrader goodies.
 */

#define CONFGROUPNAME_AUTOUPGRADE "autoupgrade"

#define CONFNAME_AUTOUPGRADE_ALLOW_UPGRADE "allow-upgrade"
#define CONFNAME_AUTOUPGRADE_ALLOW_ADD_FEATURE "allow-add-feature"
#define CONFNAME_AUTOUPGRADE_ALLOW_REMOVE_FEATURE "allow-remove-feature"
#define CONFNAME_AUTOUPGRADE_ALLOW_MSI_TRANSFORMS "allow-msi-transforms"

/*
 * END upgrader goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN logging goodies.
 */

/**
 * Defines the string used for the logging config file group.
 */
#define CONFGROUPNAME_LOGGING "logging"

#define CONFNAME_LOGGING_INSTALL_VMXGUESTLOGDISABLED "install-vmxGuestLogDisabled"

/*
 * END logging goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN environment goodies.
 */

/**
 * Defines the strings used for setenvironment and unsetenvironment
 * config groups.
 *
 * These config groups are used for setting and unsetting environment
 * variables for the service. The keys in this config group can be
 * specified in following 2 formats:
 *
 * 1. <variableName> = <value>
 * 2. <serviceName>.<variableName> = <value>
 *
 * Variables specified in format #1 are applied to all services reading
 * the config file whereas variables specified in format #2 are applied
 * only to the specified service.
 */
#define CONFGROUPNAME_SET_ENVIRONMENT "setenvironment"
#define CONFGROUPNAME_UNSET_ENVIRONMENT "unsetenvironment"

/*
 * END environment goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN gitray goodies.
 */

#define CONFGROUPNAME_GITRAY "gitray"

/*
 * END gitray goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN CarbonBlack helper plugin goodies.
 */

/**
 * Define the string used for cbhelper config file group
 */
#define CONFGROUPNAME_CBHELPER "cbhelper"

/**
 * Defines user-defined polling interval in seconds.
 */
#define CONFNAME_CBHELPER_POLLINTERVAL "poll-interval"

/*
 ******************************************************************************
 * END CarbonBlack helper plugin goodies.
 */

/*
 ******************************************************************************
 * BEGIN deviceHelper plugin goodies.
 */

/**
 * Defines the string used for the device helper config file group.
 */
#define CONFGROUPNAME_DEVICEHELPER "devicehelper"

/**
 * Defines the configuration to perform device helper or not.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * value.
 *
 * @param boolean Set to TRUE to disable device helper feature.
 *                Set to FALSE to enable device helper feature.
 */
#define CONFNAME_DEVICEHELPER_DISABLED "disabled"

/*
 * END deviceHelper goodies.
 ******************************************************************************
 */

/*
 ******************************************************************************
 * BEGIN gdp plugin goodies.
 */

/**
 * Defines the string used for the gdp config file group.
 */
#define CONFGROUPNAME_GDP "gdp"

/**
 * Defines a custom history cache buffer size limit (in bytes).
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * cache buffer size limit 1048576.
 *
 * @param int   User-defined cache buffer size limit within [262144, 4194304].
 *              Set 0 to disable caching.
 */
#define CONFNAME_GDP_CACHE_SIZE "cacheSize"

/**
 * Defines a custom history cache item count limit.
 *
 * @note Illegal values result in a @c g_warning and fallback to the default
 * cache item count limit 256.
 *
 * @param int   User-defined cache item count limit within [64, 1024].
 */
#define CONFNAME_GDP_CACHE_COUNT "cacheCount"

/*
 * END gdp goodies.
 ******************************************************************************
 */

#endif /* __CONF_H__ */
