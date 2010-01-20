
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

#ifdef _WIN32
#include "resource.h"
#define MAX_STRING_BUFFER   2048
static char * GetString(unsigned int idString);
#define GETSTR(str)   GetString(ID##str)
#else
#include "messages.h"
#define GETSTR(str)   str
#endif

#include "toolboxCmdInt.h"
#include "toolboxcmd_version.h"
#include "system.h"

#include "embed_version.h"
VM_EMBED_VERSION(TOOLBOXCMD_VERSION_STRING);


typedef int (*ToolboxCmdFunc)(char **argv, int argc);
typedef void (*ToolboxHelpFunc)(const char *progName);


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

static Bool quiet_flag; /* Flag set by `--quiet'. */

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

static int HelpCommand(char **argv, int argc);
static int DeviceCommand(char **argv, int argc);
static int DiskCommand(char **argv, int argc);
static int StatCommand(char **argv, int argc);
static int ScriptCommand(char **argv, int argc);
static int TimeSyncCommand(char **argv, int argc);
static void DeviceHelp(const char *progName);
static void DiskHelp(const char *progName);
static void ScriptHelp(const char *progName);
static void StatHelp(const char *progName);
static void TimeSyncHelp(const char *progName);
static void ToolboxCmdHelp(const char *progName);
static CmdTable *ParseCommand(char **argv, int argc);


/*
 * The commands table.
 * Must go after function declarations
 */
static CmdTable commands[] = {
   { "timesync", TimeSyncCommand, TRUE, FALSE, TimeSyncHelp},
   { "script", ScriptCommand, FALSE /* We will handle argument checks ourselves */,
      TRUE, ScriptHelp},
   { "disk", DiskCommand, TRUE, TRUE, DiskHelp},
   { "stat", StatCommand, TRUE, FALSE, StatHelp},
   { "device", DeviceCommand, TRUE, FALSE, DeviceHelp},
   { "help", HelpCommand, FALSE, FALSE, ToolboxCmdHelp},
};

#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * GetString --
 *
 *      Gets a localized string based on an identifier.
 *
 * Results:
 *      A pointer to the string. I should not be freed, being static allocated.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
GetString(unsigned int idString)     // IN: the identifier of the string
{
   static char szResourceString[MAX_STRING_BUFFER];
   const int sizeResourceString = ARRAYSIZE(szResourceString);

   szResourceString[0] = '\0';

   LoadString(GetModuleHandle(NULL), idString,
              szResourceString, sizeResourceString);

   return szResourceString;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * ToolboxMissingEntityError --
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

static void
ToolboxMissingEntityError(const char *name,     // IN: command name (argv[0])
                          const char *entity)   // IN: what is missing
{
   fprintf(stderr, "%s: Missing %s\n", name, entity);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ToolboxUnknownEntityError --
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

static void
ToolboxUnknownEntityError(const char *name,    // IN: command name (argv[0])
                          const char *entity,  // IN: what is unknown
                          const char *str)     // IN: errorneous string
{
   fprintf(stderr, "%s: Unknown %s '%s'\n", name, entity, str);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DeviceHelp --
 *
 *      Prints the help for device commands.
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
DeviceHelp(const char *progName) // IN: The name of the program obtained from argv[0]
{
   printf(GETSTR(S_HELP_DEVICE), progName);
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
ToolboxCmdHelp(const char *progName)
{
   printf(GETSTR(S_HELP_TOOLBOXCMD),
          progName, progName, progName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * TimeSyncHelp --
 *
 *      Prints the help for timesync command.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
TimeSyncHelp(const char *progName) // IN: The name of the program obtained from argv[0]
{
   printf(GETSTR(S_HELP_TIMESYNC), progName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScriptHelp --
 *
 *      Prints the help for the script command.
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
ScriptHelp(const char *progName) // IN: The name of the program obtained from argv[0]
{
   printf(GETSTR(S_HELP_SCRIPT), progName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DiskHelp --
 *
 *      Prints the help for the disk command.
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
DiskHelp(const char *progName) // IN: The name of the program obtained from argv[0]
{
   printf(GETSTR(S_HELP_DISK), progName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatHelp --
 *
 *      Prints the help for the stat command.
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
StatHelp(const char *progName) // IN: The name of the program obtained from argv[0]
{
   printf(GETSTR(S_HELP_STAT), progName);
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
HelpCommand(char **argv, // IN: Command line arguments
            int argc)    // IN: Length of argv
{
   int retval = EXIT_SUCCESS;

   if (++optind < argc) {
      int i;

      for (i = 0; i < ARRAYSIZE(commands); i++) {
         if (toolbox_strcmp(commands[i].command, argv[optind]) == 0) {
            commands[i].helpFunc(argv[0]);
            return EXIT_SUCCESS;
         }
      }
      ToolboxUnknownEntityError(argv[0], "subcommand", argv[optind]);
      retval = EX_USAGE;
   }

   ToolboxCmdHelp(argv[0]);
   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DeviceCommand --
 *
 *      Handle and parse device commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the exit code on errors.
 *
 * Side effects:
 *      Might enable or disable a device.
 *
 *-----------------------------------------------------------------------------
 */

static int
DeviceCommand(char **argv, // IN: Command line arguments
              int argc)    // IN: Length of command line arguments
{
   char *subcommand = argv[optind];
   Bool haveDeviceArg = optind + 1 < argc;

   if (toolbox_strcmp(subcommand, "list") == 0) {
      return Devices_ListDevices();
   } else if (toolbox_strcmp(subcommand, "status") == 0) {
      if (haveDeviceArg) {
         return Devices_DeviceStatus(argv[optind + 1]);
      }
   } else if (toolbox_strcmp(subcommand, "enable") == 0) {
      if (haveDeviceArg) {
         return Devices_EnableDevice(argv[optind + 1], quiet_flag);
      }
   } else if (toolbox_strcmp(subcommand, "disable") == 0) {
      if (haveDeviceArg) {
         return Devices_DisableDevice(argv[optind + 1], quiet_flag);
      }
   } else {
      ToolboxUnknownEntityError(argv[0], "subcommand", subcommand);
      return EX_USAGE;
   }

   ToolboxMissingEntityError(argv[0], "device name");
   return EX_USAGE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DiskCommand --
 *
 *      Handle and parse disk commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit code on errors.
 *
 * Side effects:
 *      Might shrink disk
 *
 *-----------------------------------------------------------------------------
 */

static int
DiskCommand(char **argv, // IN: command line arguments
            int argc)    // IN: The length of the command line arguments
{
   if (toolbox_strcmp(argv[optind], "list") == 0) {
      return Shrink_List();
   } else if (toolbox_strcmp(argv[optind], "shrink") == 0) {
      if (++optind >= argc) {
         ToolboxMissingEntityError(argv[0], "mount point");
      } else {
         return Shrink_DoShrink(argv[optind], quiet_flag);
      }
   } else {
      ToolboxUnknownEntityError(argv[0], "subcommand", argv[optind]);
   }
   return EX_USAGE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StatCommand --
 *
 *      Handle and parse stat commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit codes on errors.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
StatCommand(char **argv, // IN: Command line arguments
            int argc)    // IN: Length of command line arguments
{
   if (toolbox_strcmp(argv[optind], "hosttime") == 0) {
      return Stat_HostTime();
   } else if (toolbox_strcmp(argv[optind], "sessionid") == 0) {
      return Stat_GetSessionID();
   } else if (toolbox_strcmp(argv[optind], "balloon") == 0) {
      return Stat_GetMemoryBallooned();
   } else if (toolbox_strcmp(argv[optind], "swap") == 0) {
      return Stat_GetMemorySwapped();
   } else if (toolbox_strcmp(argv[optind], "memlimit") == 0) {
      return Stat_GetMemoryLimit();
   } else if (toolbox_strcmp(argv[optind], "memres") == 0) {
      return Stat_GetMemoryReservation();
   } else if (toolbox_strcmp(argv[optind], "cpures") == 0) {
      return Stat_GetCpuReservation();
   } else if (toolbox_strcmp(argv[optind], "cpulimit") == 0) {
      return Stat_GetCpuLimit();
   } else if (toolbox_strcmp(argv[optind], "speed") == 0) {
      return Stat_ProcessorSpeed();
   } else {
      ToolboxUnknownEntityError(argv[0], "subcommand", argv[optind]);
      return EX_USAGE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScriptCommand --
 *
 *      Handle and parse script commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the exit code on errors.
 *
 * Side effects:
 *      Might enables, disables, or change APM scripts.
 *
 *-----------------------------------------------------------------------------
 */

static int
ScriptCommand(char **argv, // IN: command line arguments.
              int argc)    // IN: the length of the command line arguments.
{
   const char *apm;

   if (++optind >= argc) {
      ToolboxMissingEntityError(argv[0], "script type");
      return EX_USAGE;
   }

   apm = argv[optind++];

   if (!Script_CheckName(apm)) {
      ToolboxUnknownEntityError(argv[0], "script type", apm);
      return EX_USAGE;
   }

   if (optind >= argc) {
      ToolboxMissingEntityError(argv[0], "subcommand");
      return EX_USAGE;
   }

   if (toolbox_strcmp(argv[optind], "default") == 0) {
      return Script_GetDefault(apm);
   } else if (toolbox_strcmp(argv[optind], "current") == 0) {
      return Script_GetCurrent(apm);
   } else if (toolbox_strcmp(argv[optind], "set") == 0) {
      if (++optind >= argc) {
         ToolboxMissingEntityError(argv[0], "script path");
         return EX_USAGE;
      }
      return Script_Set(apm, argv[optind], quiet_flag);
   } else if (toolbox_strcmp(argv[optind], "enable") == 0) {
      return Script_Enable(apm, quiet_flag);
   } else if (toolbox_strcmp(argv[optind], "disable") == 0) {
      return Script_Disable(apm, quiet_flag);
   } else {
      ToolboxUnknownEntityError(argv[0], "subcommand", argv[optind]);
      return EX_USAGE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * TimeSyncCommand --
 *
 *      Parse and Handle timesync commands.
 *
 * Results:
 *      Returns EXIT_SUCCESS on success.
 *      Returns the appropriate exit code errors.
 *
 * Side effects:
 *      Might enable time sync, which would change the time in the guest os.
 *
 *-----------------------------------------------------------------------------
 */

static int
TimeSyncCommand(char **argv, // IN: command line arguments
                int argc)    // IN: The length of the command line arguments
{
   if (toolbox_strcmp(argv[optind], "enable") == 0) {
      return TimeSync_Enable(quiet_flag);
   } else if (toolbox_strcmp(argv[optind], "disable") == 0) {
      return TimeSync_Disable(quiet_flag);
   } else if (toolbox_strcmp(argv[optind], "status") == 0) {
      return TimeSync_Status();
   } else {
      ToolboxUnknownEntityError(argv[0], "subcommand", argv[optind]);
      return EX_USAGE;
   }
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

   setlocale(LC_ALL, "");

   /*
    * Check if we are in a VM
    */
   if (!VmCheck_IsVirtualWorld()) {
      fprintf(stderr, GETSTR(S_WARNING_VWORLD), argv[0]);
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
         quiet_flag = TRUE;
         break;

      case '?':
         /* getopt_long already printed an error message. */
         fprintf(stderr, GETSTR(S_HELP_MAIN), argv[0], "-h", "", "");
         return EXIT_FAILURE;

      default:
         return EXIT_FAILURE;
      }
   }

   if (show_version) {
      printf("%s (%s)\n", TOOLBOXCMD_VERSION_STRING, BUILD_NUMBER);
   } else if (show_help) {
      ToolboxCmdHelp(argv[0]);
   } else {
      /* Process any remaining command line arguments (not options), and
       * execute corresponding command
       */
      if (optind >= argc) {
         ToolboxMissingEntityError(argv[0], "command");
         retval = EX_USAGE;
      } else if ((cmd = ParseCommand(argv, argc)) == NULL) {
         ToolboxUnknownEntityError(argv[0], "command", argv[optind]);
         retval = EX_USAGE;
      } else if (cmd->requireRoot && !System_IsUserAdmin()) {
         fprintf(stderr, GETSTR(S_WARNING_ADMIN), argv[0], cmd->command);
         retval = EX_NOPERM;
      } else if (cmd->requireArguments && ++optind >= argc) {
         ToolboxMissingEntityError(argv[0], "subcommand");
         retval = EX_USAGE;
      } else {
         retval = cmd->func(argv, argc);
      }

      if (retval == EX_USAGE && (cmd == NULL || strcmp(cmd->command, "help"))) {
         fprintf(stderr, GETSTR(S_HELP_MAIN),
                 argv[0], "help", cmd ? " " : "", cmd ? cmd->command : "");
      }

      return retval;
   }

   return EXIT_SUCCESS;
}
