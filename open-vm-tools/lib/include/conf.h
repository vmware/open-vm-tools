/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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

#ifdef N_PLAT_NLM
#   define CONFVAL_POWERONSCRIPT_DEFAULT  "POWERON.NCF"
#   define CONFVAL_POWEROFFSCRIPT_DEFAULT "POWEROFF.NCF"
#   define CONFVAL_RESUMESCRIPT_DEFAULT   "RESUME.NCF"
#   define CONFVAL_SUSPENDSCRIPT_DEFAULT  "SUSPEND.NCF"
#elif ! defined(_WIN32)
#   define CONFNAME_HALT                  "halt-command"
#   define CONFNAME_REBOOT                "reboot-command"
#   ifdef sun
#      define CONFVAL_HALT_DEFAULT        "/usr/sbin/init 5"
#      define CONFVAL_REBOOT_DEFAULT      "/usr/sbin/init 6"
#   else
#      define CONFVAL_HALT_DEFAULT        "/sbin/shutdown -h now"
#      define CONFVAL_REBOOT_DEFAULT      "/sbin/shutdown -r now"
#   endif

#   define CONFVAL_POWERONSCRIPT_DEFAULT  "poweron-vm-default"
#   define CONFVAL_POWEROFFSCRIPT_DEFAULT "poweroff-vm-default"
#   define CONFVAL_RESUMESCRIPT_DEFAULT   "resume-vm-default"
#   define CONFVAL_SUSPENDSCRIPT_DEFAULT  "suspend-vm-default"

#   define CONFNAME_MOUNT_POINT           "mount-point"
#   define CONFVAL_MOUNT_POINT_DEFAULT    "/hgfs"
#else
#   define CONFVAL_POWERONSCRIPT_DEFAULT  "poweron-vm-default.bat"
#   define CONFVAL_POWEROFFSCRIPT_DEFAULT "poweroff-vm-default.bat"
#   define CONFVAL_RESUMESCRIPT_DEFAULT   "resume-vm-default.bat"
#   define CONFVAL_SUSPENDSCRIPT_DEFAULT  "suspend-vm-default.bat"
#endif

#define CONFNAME_MAX_WIPERSIZE            "max.wiperfile.size"
#define CONFNAME_POWERONSCRIPT            "poweron-script"
#define CONFNAME_POWEROFFSCRIPT           "poweroff-script"
#define CONFNAME_RESUMESCRIPT             "resume-script"
#define CONFNAME_SUSPENDSCRIPT            "suspend-script"
#define CONFNAME_LOG                      "log"
#define CONFNAME_LOGFILE                  "log.file"
#define CONFNAME_LOGLEVEL                 "log.level" 
#define CONFNAME_DISABLEQUERYDISKINFO     "disable-query-diskinfo"
/*
 * Without dnd = "true" in tools.conf, Linux DnD won't work. This will only be used 
 * to develop Linux DnD. Once the implementation is done, get rid of this (and the
 * code that checks it in toolbox-gtk.c and let dnd be enabled by default. 
 */
#define CONFNAME_HACKY_DND                "dnd"

/*
 * Minimum guest resolution size. Fix for bug 58681.
 */
#define CONFNAME_RESOLUTION_MIN_WIDTH         "resolution.min.width"
#define CONFNAME_RESOLUTION_MIN_HEIGHT        "resolution.min.height"
#define CONFVAL_RESOLUTION_MIN_WIDTH_DEFAULT  "640"
#define CONFVAL_RESOLUTION_MIN_HEIGHT_DEFAULT "480"

/*
 * This is the bin direcotry the user chose while installing the tools
 */
#define CONFNAME_BINDIR                       "bindir"

/* Default maximum size of wiper file in MB */
#define CONFVAL_MAX_WIPERSIZE_DEFAULT         "512"

/* Wait 5 seconds between polls to see if the conf file has changed */
#define CONF_POLL_TIME     500

GuestApp_Dict *Conf_Load(void);
Bool Conf_ReloadFile(GuestApp_Dict **pConfDict);

#endif /* __CONF_H__ */
