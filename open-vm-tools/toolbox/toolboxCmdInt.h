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

/*
 * toolbox-cmd.h --
 *
 *     Common defines used by the toolbox-cmd.
 */
#ifndef _TOOLBOX_CMD_INT_H_
#define _TOOLBOX_CMD_INT_H_

#define G_LOG_MAIN "toolboxcmd"
#define VMW_TEXT_DOMAIN G_LOG_MAIN

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#   include "getoptwin32.h"
#else
#   include <getopt.h>
#   include <sysexits.h>
#   include <unistd.h>
#endif

#include "vmGuestLib.h"

/*
 * Some platforms (such as Win32) don't have sysexits.h and thus don't have
 * generic program exit codes.
 */

#ifndef EX_USAGE
#define EX_USAGE 64 /* command line usage error */
#endif

#ifndef EX_UNAVAILABLE
#define EX_UNAVAILABLE 69 /* service unavailable */
#endif

#ifndef EX_SOFTWARE
#define EX_SOFTWARE 70 /* internal software error */
#endif

#ifndef EX_OSERR
#define EX_OSERR 71 /* system error (e.g., can't fork) */
#endif

#ifndef EX_OSFILE
#define EX_OSFILE 72 /* critical OS file missing */
#endif

#ifndef EX_TEMPFAIL
#define EX_TEMPFAIL 75 /* temp failure; user is invited to retry */
#endif

#ifndef EX_NOPERM
#define EX_NOPERM 77 /* permission denied */
#endif

/*
 * We want to have commands and arguments on Windows to be
 * case-instensitive, everywhere else we expect lowercase
 * for commands and case-sensitivity for arguments.
 */
#ifdef _WIN32
#   define toolbox_strcmp  stricmp
#else
#   define toolbox_strcmp  strcmp
#endif

/*
 * Common functions.
 */

void
ToolsCmd_MissingEntityError(const char *name,
                            const char *entity);

void
ToolsCmd_UnknownEntityError(const char *name,
                            const char *entity,
                            const char *str);

void
ToolsCmd_Print(const char *fmt,
               ...) PRINTF_DECL(1, 2);

void
ToolsCmd_PrintErr(const char *fmt,
                  ...) PRINTF_DECL(1, 2);

gboolean
ToolsCmd_SendRPC(const char *rpc,
                 size_t rpcLen,
                 char **result,
                 size_t *resultLen);

void
ToolsCmd_FreeRPC(void *ptr);

/*
 * Command declarations.
 */

/**
 * A shorthand macro for declaring a command entry. This just declares
 * two functions ("foo_Command" and "foo_Help" for a command "foo").
 * The command implementation should provide those functions, which will
 * be used by the main dispatch table when executing commands.
 */

#define DECLARE_COMMAND(name)             \
   int name##_Command(char **argv,        \
                      int argc,           \
                      gboolean quiet);    \
   void name##_Help(const char *progName, \
                    const char *cmd);

DECLARE_COMMAND(Device);
DECLARE_COMMAND(Disk);
DECLARE_COMMAND(Script);
DECLARE_COMMAND(Stat);
DECLARE_COMMAND(TimeSync);
DECLARE_COMMAND(Logging);
DECLARE_COMMAND(Info);
DECLARE_COMMAND(Config);

#if defined(_WIN32) || \
   (defined(__linux__) && !defined(OPEN_VM_TOOLS) && !defined(USERWORLD))
DECLARE_COMMAND(Upgrade);
#endif

#endif /*_TOOLBOX_CMD_H_*/
