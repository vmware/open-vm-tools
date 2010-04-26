
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
#include "vmware/tools/i18n.h"
#include "vmware/tools/utils.h"

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


/*
 * Sadly, our home-brewed implementation of getopt() doesn't come with an
 * implementation of getopt_long().
 */
#ifndef _WIN32
static struct option long_options[] = {
   /* quiet sets a flag */
   { "quiet", no_argument, 0, 'q' },
   /* These options don't set a flag.
      We distinguish them by their indices. */
   { "help", no_argument, 0, 'h' },
   { "version", no_argument, 0, 'v' },
   { 0, 0, 0, 0 } };
#endif

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
static CmdTable commands[] = {
   { "timesync",  TimeSync_Command, TRUE,    FALSE,   TimeSync_Help},
   { "script",    Script_Command,   FALSE,   TRUE,    Script_Help},
   { "disk",      Disk_Command,     TRUE,    TRUE,    Disk_Help},
   { "stat",      Stat_Command,     TRUE,    FALSE,   Stat_Help},
   { "device",    Device_Command,   TRUE,    FALSE,   Device_Help},
   { "help",      HelpCommand,      FALSE,   FALSE,   ToolboxCmdHelp},
};


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
                          "   device\n"
                          "   disk\n"
                          "   script\n"
                          "   stat\n"
                          "   timesync\n\n"
                          "For additional information please visit http://www.vmware.com/support/\n\n"),
           progName, progName, cmd, progName);
}


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
main(int argc,    // IN: length of command line arguments
     char **argv) // IN: Command line arguments
{
   Bool show_help = FALSE;
   Bool show_version = FALSE;
   CmdTable *cmd = NULL;
   int c;
   int retval;
   gboolean quiet = FALSE;

   setlocale(LC_ALL, "");
   VMTools_ConfigLogging("toolboxcmd", NULL, FALSE, FALSE);
   VMTools_BindTextDomain(VMW_TEXT_DOMAIN, NULL, NULL);

   /*
    * Check if we are in a VM
    */
   if (!VmCheck_IsVirtualWorld()) {
      g_printerr(SU_(error.novirtual, "%s must be run inside a virtual machine.\n"),
                 argv[0]);
      exit(EXIT_FAILURE);
   }

   /*
    * Parse the command line optional arguments
    */
   while (1) {
      int option_index = 0;

#ifdef _WIN32
      c = getopt(argc, argv, options);
#else
      c = getopt_long(argc, argv, options, long_options, &option_index);
#endif

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
         quiet = TRUE;
         break;

      case '?':
         /* getopt_long already printed an error message. */
         g_printerr(SU_(help.hint, "Try '%s %s%s%s' for more information.\n"),
                    argv[0], "-h", "", "");
         return EXIT_FAILURE;

      default:
         return EXIT_FAILURE;
      }
   }

   if (show_version) {
      printf("%s (%s)\n", TOOLBOXCMD_VERSION_STRING, BUILD_NUMBER);
   } else if (show_help) {
      ToolboxCmdHelp(argv[0], "help");
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
         retval = cmd->func(argv, argc, quiet);
      }

      if (retval == EX_USAGE && (cmd == NULL || strcmp(cmd->command, "help"))) {
         g_printerr(SU_(help.hint, "Try '%s %s%s%s' for more information.\n"),
                    argv[0], "help", cmd ? " " : "", cmd ? cmd->command : "");
      }

      return retval;
   }

   return EXIT_SUCCESS;
}
