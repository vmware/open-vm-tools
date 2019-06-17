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
 * toolbox-cmd.c --
 *
 *     The toolbox app with a command line interface.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <glib/gstdio.h>

#include "toolboxCmdInt.h"
#include "toolboxcmd_version.h"
#include "system.h"
#include "unicode.h"
#include "vmcheck.h"
#include "vmware/tools/guestrpc.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/log.h"
#include "vmware/tools/utils.h"
#if defined(_WIN32)
#include "vmware/tools/win32util.h"
#endif

#include "vm_version.h"
#include "vm_product_versions.h"

#include "vm_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(TOOLBOXCMD_VERSION_STRING);


typedef int (*ToolboxCmdFunc)(char **argv,
                              int argc,
                              gboolean quiet);

typedef void (*ToolboxHelpFunc)(const char *progName,
                                const char *cmd);


/*
 * Local Data
 */

const typedef struct CmdTable {
   const char *command;       /* The name of the command. */
   ToolboxCmdFunc func;       /* The function to execute. */
   Bool requireArguments;     /* The function requires arguments. */
   Bool requireRoot;          /* Indicates whether root is required. */
   ToolboxHelpFunc helpFunc;  /* The help function associated with the command. */
} CmdTable;


static struct option long_options[] = {
   /* quiet sets a flag */
   { "quiet", no_argument, 0, 'q' },
   /* These options don't set a flag.
      We distinguish them by their indices. */
   { "help", no_argument, 0, 'h' },
   { "version", no_argument, 0, 'v' },
   { 0, 0, 0, 0 } };

static gboolean gQuiet = FALSE;
static const char *options = "hqv";

/*
 * Local Functions
 */

static int
HelpCommand(char **argv,
            int argc,
            gboolean quiet);

static void
ToolboxCmdHelp(const char *progName,
               const char *cmd);


/*
 * The commands table.
 * Must go after function declarations
 */
#if defined(_WIN32)
#include "toolboxCmdTableWin32.h"
#else
static CmdTable commands[] = {
   { "timesync",   TimeSync_Command,   TRUE,  FALSE, TimeSync_Help},
   { "script",     Script_Command,     FALSE, TRUE,  Script_Help},
#if !defined(USERWORLD)
   { "disk",       Disk_Command,       TRUE,  TRUE,  Disk_Help},
#endif
   { "stat",       Stat_Command,       TRUE,  FALSE, Stat_Help},
   { "device",     Device_Command,     TRUE,  FALSE, Device_Help},
#if defined(__linux__) && !defined(OPEN_VM_TOOLS) && !defined(USERWORLD)
   { "upgrade",    Upgrade_Command,    TRUE,  TRUE,  Upgrade_Help},
#endif
   { "logging",    Logging_Command,    TRUE,  TRUE,  Logging_Help},
   { "info",       Info_Command,       TRUE,  TRUE,  Info_Help},
   { "config",     Config_Command,     TRUE,  TRUE,  Config_Help},
   { "help",       HelpCommand,        FALSE, FALSE, ToolboxCmdHelp},
};
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsCmd_MissingEntityError --
 *
 *      Print out error message regarding missing argument.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsCmd_MissingEntityError(const char *name,     // IN: command name (argv[0])
                            const char *entity)   // IN: what is missing
{
   g_printerr(SU_(error.missing, "%s: Missing %s\n"), name, entity);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsCmd_Print --
 *
 *      Prints a message to stdout unless quiet output was requested.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsCmd_Print(const char *fmt,
               ...)
{
   if (!gQuiet) {
      gchar *str;
      va_list args;

      va_start(args, fmt);
      g_vasprintf(&str, fmt, args);
      va_end(args);

      g_print("%s", str);
      g_free(str);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsCmd_PrintErr --
 *
 *      Prints a message to stderr unless quiet output was requested.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsCmd_PrintErr(const char *fmt,
                  ...)
{
   if (!gQuiet) {
      gchar *str;
      va_list args;

      va_start(args, fmt);
      g_vasprintf(&str, fmt, args);
      va_end(args);

      g_printerr("%s", str);
      g_free(str);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsCmd_SendRPC --
 *
 *    Sends an RPC message to the host.
 *
 * Results:
 *    The return value from the RPC.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
ToolsCmd_SendRPC(const char *rpc,      // IN
                 size_t rpcLen,        // IN
                 char **result,        // OUT
                 size_t *resultLen)    // OUT
{
   char *lrpc = (char *) rpc;
   RpcChannel *chan = RpcChannel_New();
   gboolean ret = RpcChannel_Start(chan);

   if (!ret) {
      g_warning("Error starting RPC channel.");
      goto exit;
   }

   ret = RpcChannel_Send(chan, lrpc, rpcLen, result, resultLen);

exit:
   RpcChannel_Destroy(chan);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsCmd_FreeRPC --
 *
 *    Free the memory allocated for the results from
 *    ToolsCmd_SendRPC calls.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsCmd_FreeRPC(void *ptr)      // IN
{
   RpcChannel_Free(ptr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsCmd_UnknownEntityError --
 *
 *      Print out error message regarding unknown argument.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsCmd_UnknownEntityError(const char *name,    // IN: command name (argv[0])
                            const char *entity,  // IN: what is unknown
                            const char *str)     // IN: errorneous string
{
   g_printerr(SU_(error.unknown, "%s: Unknown %s '%s'\n"), name, entity, str);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolboxCmdHelp --
 *
 *      Print out usage information to stdout.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(_WIN32)
#include "toolboxCmdHelpWin32.h"
#else
static void
ToolboxCmdHelp(const char *progName,   // IN
               const char *cmd)        // IN
{
   g_print(SU_(help.main, "Usage: %s <command> [options] [subcommand]\n"
                          "Type '%s %s <command>' for help on a specific command.\n"
                          "Type '%s -v' to see the VMware Tools version.\n"
                          "Use '-q' option to suppress stdout output.\n"
                          "Most commands take a subcommand.\n\n"
                          "Available commands:\n"
                          "   config\n"
                          "   device\n"
                          "   disk (not available on all operating systems)\n"
                          "   info\n"
                          "   logging\n"
                          "   script\n"
                          "   stat\n"
                          "   timesync\n"
                          "   upgrade (not available on all operating systems)\n"),
           progName, progName, cmd, progName);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HelpCommand --
 *
 *      Handle and parse help commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns other exit codes on errors.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HelpCommand(char **argv,      // IN: Command line arguments
            int argc,         // IN: Length of argv
            gboolean quiet)   // IN
{
   int retval = EXIT_SUCCESS;

   if (++optind < argc) {
      int i;

      for (i = 0; i < ARRAYSIZE(commands); i++) {
         if (toolbox_strcmp(commands[i].command, argv[optind]) == 0) {
            commands[i].helpFunc(argv[0], commands[i].command);
            return EXIT_SUCCESS;
         }
      }
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  argv[optind]);
      retval = EX_USAGE;
   }

   ToolboxCmdHelp(argv[0], argv[optind - 1]);
   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ParseCommand --
 *
 *      Parse the non optional command line arguments.
 *
 * Results:
 *      Returns the CmdTable pointer on success or calls exit on error.
 *
 * Side effects:
 *      Calls exit on parse errors.
 *
 *-----------------------------------------------------------------------------
 */

static CmdTable *
ParseCommand(char **argv, // IN: Command line arguments
             int argc)    // IN: Length of command line arguments
{
   int i;

   for (i = 0; i < ARRAYSIZE(commands); i++) {
      if (toolbox_strcmp(commands[i].command, argv[optind]) == 0) {
         return &commands[i];
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * main --
 *
 *      This is main
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns different exit code on failure.
 *
 * Side effects:
 *      The vmware-toolbox-cmd will run and do a variety of tricks for your
 *      amusement.
 *
 *-----------------------------------------------------------------------------
 */

int
#if defined(_WIN32)
wmain(int argc,         // IN: length of command line arguments
      wchar_t **wargv)  // IN: Command line arguments
#else
main(int argc,    // IN: length of command line arguments
     char **argv) // IN: Command line arguments
#endif
{
   Bool show_help = FALSE;
   Bool show_version = FALSE;
   CmdTable *cmd = NULL;
   GKeyFile *conf = NULL;
   int c;
   int retval = EXIT_FAILURE;

#if defined(_WIN32)
   char **argv;

   WinUtil_EnableSafePathSearching(TRUE);

   Unicode_InitW(argc, wargv, NULL, &argv, NULL);
#else
   Unicode_Init(argc, &argv, NULL);
#endif

   setlocale(LC_ALL, "");
   VMTools_LoadConfig(NULL, G_KEY_FILE_NONE, &conf, NULL);
   VMTools_ConfigLogging("toolboxcmd", conf, FALSE, FALSE);
   VMTools_BindTextDomain(VMW_TEXT_DOMAIN, NULL, NULL);

   /*
    * Check if we are in a VM
    *
    * Valgrind can't handle the backdoor check, so don't bother.
    */
#ifndef USE_VALGRIND
   if (!VmCheck_IsVirtualWorld()) {
      g_printerr(SU_(error.novirtual, "%s must be run inside a virtual machine.\n"),
                 argv[0]);
      goto exit;
   }
#endif

   /*
    * Parse the command line optional arguments
    */
   while (1) {
      int option_index = 0;

      c = getopt_long(argc, argv, options, long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1) {
         break;
      }

      switch (c) {
      case 'h':
         show_help = TRUE;
         break;

      case 'v':
         show_version = TRUE;
         break;

      case 'q':
         gQuiet = TRUE;
         break;

      case '?':
         /* getopt_long already printed an error message. */
         g_printerr(SU_(help.hint, "Try '%s %s%s%s' for more information.\n"),
                    argv[0], "-h", "", "");
         goto exit;

      default:
         goto exit;
      }
   }

   if (show_version) {
      g_print("%s (%s)\n", TOOLBOXCMD_VERSION_STRING, BUILD_NUMBER);
      retval = EXIT_SUCCESS;
   } else if (show_help) {
      ToolboxCmdHelp(argv[0], "help");
      retval = EXIT_SUCCESS;
   } else {
      /* Process any remaining command line arguments (not options), and
       * execute corresponding command
       */
      if (optind >= argc) {
         ToolsCmd_MissingEntityError(argv[0], SU_(arg.command, "command"));
         retval = EX_USAGE;
      } else if ((cmd = ParseCommand(argv, argc)) == NULL) {
         ToolsCmd_UnknownEntityError(argv[0], SU_(arg.command, "command"), argv[optind]);
         retval = EX_USAGE;
      } else if (cmd->requireRoot && !System_IsUserAdmin()) {
#if defined(_WIN32)
         g_printerr(SU_(error.noadmin.win,
                        "%s: Administrator permissions are needed to perform %s operations.\n"
                        "Use an administrator command prompt to complete these tasks.\n"),
                    argv[0], cmd->command);

#else
         g_printerr(SU_(error.noadmin.posix,
                        "%s: You must be root to perform %s operations.\n"),
                    argv[0], cmd->command);
#endif
         retval = EX_NOPERM;
      } else if (cmd->requireArguments && ++optind >= argc) {
         ToolsCmd_MissingEntityError(argv[0], SU_(arg.subcommand, "subcommand"));
         retval = EX_USAGE;
      } else {
         retval = cmd->func(argv, argc, gQuiet);
      }

      if (retval == EX_USAGE && (cmd == NULL || strcmp(cmd->command, "help"))) {
         g_printerr(SU_(help.hint, "Try '%s %s%s%s' for more information.\n"),
                    argv[0], "help", cmd ? " " : "", cmd ? cmd->command : "");
      }
   }

exit:
   if (conf != NULL) {
      g_key_file_free(conf);
   }

   return retval;
}
