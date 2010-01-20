/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * messages.h --
 *
 *     Common defines used by the toolbox-cmd.
 */
#ifndef _TOOLBOX_CMD_MESSAGES_H_
#define _TOOLBOX_CMD_MESSAGES_H_

#define S_WARNING_VWORLD        "%s must be run inside a virtual machine.\n"
#define S_WARNING_ADMIN         "%s: You must be root to perform %s operations"
#define S_WARNING_ADMIN_WIN     "%s: Administrator permissions are needed to perform %s operations. Use an administrator command prompt to complete these tasks."
#define S_HELP_MAIN             "Try '%s %s%s%s' for more information.\n"
#define S_HELP_TOOLBOXCMD       "Usage: %s <command> [options] [subcommand]\nType '%s help <command>' for help on a specific command.\nType '%s -v' to see the Vmware Tools version.\nUse '-q' option to suppress stdout output.\nMost commands take a subcommand.\n\nAvailable commands:\n  device\n  disk\n  script\n  stat\n  timesync\n\nFor additional information please visit http://www.vmware.com/support/\n\n"
#define S_HELP_DEVICE           "device: functions related to the virtual machine's hardware devices\nUsage: %s device <subcommand> [args]\n    device is the name of the device.\n\nSubcommands:\n   enable <dev>: enable the device dev\n   disable <dev>: disable the device dev\n   list: list all available devices\n   status <dev>: print the status of a device\n"
#define S_HELP_TIMESYNC         "timesync: functions for controlling time synchronization on the guest OS\nUsage: %s timesync <subcommand>\n\nSubcommands:\n   enable: enable time synchronization\n   disable: disable time synchronization\n   status: print the time synchronization status\n"
#define S_HELP_SCRIPT           "script: control the scripts run in response to power operations\nUsage: %s script <power|resume|suspend|shutdown> <subcommand> [args]\n\nSubcommands:\n   enable: enable the given script and restore its path to the default\n   disable: disable the given script\n   set <full_path>: set the given script to the given path\n   default: print the default path of the given script\n   current: print the current path of the given script\n"
#define S_HELP_DISK             "disk: perform disk shrink operations\nUsage: %s disk <subcommand> [args]\n\nSubcommands:\n   list: list available mountpoints\n   shrink <mount-point>: shrinks a file system at the given mountpoint\n"
#define S_HELP_STAT             "stat: print useful guest and host information\nUsage: %s stat <subcommand>\n\nSubcommands:\n   hosttime: print the host time\n   speed: print the CPU speed in MHz\nESX guests only subcommands:\n   sessionid: print the current session id\n   balloon: print memory ballooning information\n   swap: print memory swapping information\n   memlimit: print memory limit information\n   memres: print memory reservation information\n   cpures: print CPU reservation information\n   cpulimit: print CPU limit information\n"

#endif //_TOOLBOX_CMD_MESSAGES_H_
