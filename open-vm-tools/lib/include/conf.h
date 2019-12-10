/*********************************************************
 * Copyright (C) 2002-2019 VMware, Inc. All rights reserved.
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

#include "guestApp.h"

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

/*
 * END AppInfo goodies.
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

#endif /* __CONF_H__ */
