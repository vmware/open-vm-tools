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

#include "toolboxCmdInt.h"
#include "toolboxcmd_version.h"
#include "system.h"

#include "embed_version.h"
VM_EMBED_VERSION(TOOLBOXCMD_VERSION_STRING);


typedef int (*ToolboxCmdFunc)(char **argv, int argc);
typedef void (*ToolboxHelpFunc)(char *progName);


/*
 * Local Data
 */

const typedef struct CmdTable {
   const char *command;		/* The name of the command. */
   ToolboxCmdFunc func;		/* The function to execute. */
   Bool requireRoot;		/* Indicates whether root is required. */
   ToolboxHelpFunc helpFunc;	/* The help function associated with the command. */
} CmdTable;

static int quiet_flag; /* Flag set by `--quiet'. */

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
static void DeviceHelp(char *progName);
static void DiskHelp(char *progName);
static void ScriptHelp(char *progName);
static void StatHelp(char *progName);
static void TimeSyncHelp(char *progName);
static void ToolboxCmdHelp(char *progName);
static CmdTable *ParseCommand(char **argv, int argc);
static Bool CheckArgumentLength(char **argv, int argc);


/*
 * The commands table.
 * Must go after function declarations
 */
static CmdTable commands[] = {
   { "timesync", TimeSyncCommand, FALSE, TimeSyncHelp},
   { "script", ScriptCommand, TRUE, ScriptHelp},
   { "disk", DiskCommand, TRUE, DiskHelp},
   { "stat", StatCommand, FALSE, StatHelp},
   { "device", DeviceCommand, FALSE, DeviceHelp},
   { "help", HelpCommand, FALSE, ToolboxCmdHelp},
   { NULL, } };


/*
 *-----------------------------------------------------------------------------
 *
 * CheckArgumentLength --
 *
 *      Makes sure that the program receives at least one subcommand.
 *
 * Results:
 *      TRUE if there is at least one subcommand.
 *      FALSE otherwise.
 *
 * Side effects:
 *      Prints to stderr if the subcommand is missing.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CheckArgumentLength(char **argv, // IN: The command line arguments.
                    int argc)    // IN: The length of the command line argumensts
{
   if (++optind < argc) {
      return TRUE;
   } else {
      fprintf(stderr, "Missing command\n");
      return FALSE;
   }

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
DeviceHelp(char *progName) // IN: The name of the program obtained from argv[0]
{
   printf("device: functions related to the virtual machine's hardware devices\n"
          "Usage: %s device <subcommand> [args]\n"
          "    dev is the name of the device.\n\n"
          "Subcommands:\n"
          "   enable <dev>: enable the device dev\n"
          "   disable <dev>: disable the device dev\n"
          "   list: list all available devices\n"
          "   status <dev>: print the status of a device\n", progName);
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
ToolboxCmdHelp(char *progName)
{
   printf("Usage: %s <command> [options] [subcommand]\n"
          "Type \'%s help <command>\' for help on a specific command.\n"
          "Type \'%s --version' to see the Vmware Tools version.\n"
          "Use --quiet to suppress stdout output.\n"
          "Most commands take a subcommand.\n\n"
          "Available commands:\n"
          "   timesync\n"
          "   device\n"
          "   script\n"
          "   disk\n"
          "   stat\n"
          "\n"
          "For additional information please visit http://www.vmware.com/support/\n\n",
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
TimeSyncHelp(char *progName) // IN: The name of the program obtained from argv[0]
{
   printf("timesync: functions for controlling time synchronization on the guest OS\n"
          "Usage: %s timesync <subcommand>\n\n"
          "Subcommands\n"
          "   enable: enable time synchronization\n"
          "   disable: disable time synchronization\n"
          "   status: print the time synchronization status\n", progName);
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
ScriptHelp(char *progName) // IN: The name of the program obtained from argv[0]
{
   printf("script: control the scripts run in response to power operations\n"
          "Usage: %s script <power|resume|suspend|shutdown> <subcomamnd> [args]\n\n"
          "Subcommands:\n"
          "   enable: enable the given script and restore its path to the default\n"
          "   disable: disable the given script\n"
          "   set <full_path>: set the given script to the given path\n"
          "   default: print the default path of the given script\n"
          "   current: print the current path of the given script\n", progName);
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
DiskHelp(char *progName) // IN: The name of the program obtained from argv[0]
{
   printf("disk: perform disk shrink operations\n"
          "Usage: %s disk <subcommand> [args]\n\n"
          "Subcommands\n"
          "   list: list available mountpoints\n"
          "   shrink <mount-point>: shrinks a file system at the given mountpoint\n",
          progName);
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
StatHelp(char *progName) // IN: The name of the program obtained from argv[0]
{
   printf("stat: print useful guest and host information\n"
          "Usage: %s state <subcommand>\n\n"
          "Subcommands\n"
          "   hosttime: print the host time\n"
          "   memory: print the virtual machine memory in MBs\n"
          "   speed: print the CPU speed in MHz\n"
          "ESX guests only subcommands\n"
          "   sessionid: print the current session id\n"
          "   balloon: print memory ballooning information\n"
          "   swap: print memory swapping information\n"
          "   memlimit: print memory limit information\n"
          "   memres: print memory reservation information\n"
          "   cpures: print CPU reservation information\n"
          "   cpulimit: print CPU limit information\n", progName);
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
   if (CheckArgumentLength(argv, argc)) {
      int i = 0;
      while (commands[i].command != 0) {
	 if (strcmp(commands[i].command, argv[optind]) == 0) {
            commands[i].helpFunc(argv[0]);
            return EXIT_SUCCESS;
         }
	 i++;
      }
      fprintf(stderr, "Unknown subcommand\n");
   }
   ToolboxCmdHelp(argv[0]);
   return EX_USAGE;
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
 *      Returns ther exit code on errors.
 *
 * Side effects:
 *      Might enable or disable a device.
 *
 *-----------------------------------------------------------------------------
 */

static int
DeviceCommand(char **argv, // IN: Command line arguments
              int argc)    // IN: Length of command line argumenst
{
   if (CheckArgumentLength(argv, argc)) {
      if (strcmp(argv[optind], "list") == 0) {
         return Devices_ListDevices();
      } else {
	 char *subcommand = argv[optind++];
	 if (optind < argc) {
	    if (strcmp(subcommand, "status") == 0) {
	       return Devices_DeviceStatus(argv[optind]);
	    } else if (strcmp(subcommand, "enable") == 0) {
	       return Devices_EnableDevice(argv[optind], quiet_flag);
	    } else if (strcmp(subcommand, "disable") == 0) {
	       return Devices_DisableDevice(argv[optind], quiet_flag);
	    } else {
	       fprintf(stderr, "Unknown subcommand\n");

	    }
	 } else {
	    fprintf(stderr, "Missing device name\n");

	 }
      }
   }
   DeviceHelp(argv[0]);
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
   if (CheckArgumentLength(argv, argc)) {
      if (strcmp(argv[optind], "list") == 0) {
	 return Shrink_List();
      } else if (strcmp(argv[optind], "shrink") == 0) {
	 optind++; // Position optind at the mountpoint
	 if (optind < argc) {
	    return Shrink_DoShrink(argv[optind], quiet_flag);
	 } else {
	    fprintf(stderr, "Missing mount point\n");
	 }
      } else {
	 fprintf(stderr, "Unknown subcommand\n");

      }
   }
   DiskHelp(argv[0]);
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
StatCommand(char **argv, // IN: Comand line arguments
            int argc)    // IN: Length of command line arguments
{
   if (CheckArgumentLength(argv, argc)) {
      if (strcmp(argv[optind], "memory") == 0) {
	 return Stat_MemorySize();
      } else if (strcmp(argv[optind], "hosttime") == 0) {
	 return Stat_HostTime();
      } else if (strcmp(argv[optind], "sessionid") == 0) {
	 return Stat_GetSessionID();
      } else if (strcmp(argv[optind], "balloon") == 0) {
	 return Stat_GetMemoryBallooned();
      } else if (strcmp(argv[optind], "swap") == 0) {
	 return Stat_GetMemorySwapped();
      } else if (strcmp(argv[optind], "memlimit") == 0) {
	 return Stat_GetMemoryLimit();
      } else if (strcmp(argv[optind], "memres") == 0) {
	 return Stat_GetMemoryReservation();
      } else if (strcmp(argv[optind], "cpures") == 0) {
	 return Stat_GetCpuReservation();
      } else if (strcmp(argv[optind], "cpulimit") == 0) {
	 return Stat_GetCpuLimit();
      } else if (strcmp(argv[optind], "speed") == 0) {
	 return Stat_ProcessorSpeed();
      } else {
	 fprintf(stderr, "Unknown subcommand\n");
      }
   }
   StatHelp(argv[0]);
   return EX_USAGE;
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
   if (CheckArgumentLength(argv, argc)) {
      char* apm = argv[optind++];

      if (optind >= argc) {
	 fprintf(stderr, "Missing subcommand\n");
	 return EX_USAGE;
      }

      if (strcmp(argv[optind], "default") == 0) {
	 return Script_GetDefault(apm);
      } else if (strcmp(argv[optind], "current") == 0) {
	 return Script_GetCurrent(apm);
      } else if (strcmp(argv[optind], "set") == 0) {
	 optind++;
	 if (optind < argc) {
	    return Script_Set(apm, argv[optind], quiet_flag);
	 } else {
	    fprintf(stderr, "Missing script path\n");
	    ScriptHelp(argv[0]);
	    return EX_USAGE;
	 }
      } else if (strcmp(argv[optind], "enable") == 0) {
	 return Script_Enable(apm, quiet_flag);
      } else if (strcmp(argv[optind], "disable") == 0) {
	 return Script_Disable(apm, quiet_flag);
      } else {
	 fprintf(stderr, "Unknown subcommand");
      }
   }
   ScriptHelp(argv[0]);
   return EX_USAGE;
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
   if (CheckArgumentLength(argv, argc)) {
      if (strcmp(argv[optind], "enable") == 0) {
	 return TimeSync_Enable(quiet_flag);
      } else if (strcmp(argv[optind], "disable") == 0) {
	 return TimeSync_Disable(quiet_flag);
      } else if (strcmp(argv[optind], "status") == 0) {
	 return TimeSync_Status();
      } else {
	 fprintf(stderr, "Unknown subcommand");
      }
   }
   TimeSyncHelp(argv[0]);
   return EX_USAGE;
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
   if (optind < argc) {
      int i = 0;
      while (commands[i].command != 0) {
	 if (strcmp(commands[i].command, argv[optind]) == 0) {
	    return &commands[i];
	 }
	 i++;
      }
      return &commands[i];
   } else {
      fprintf(stderr, "Missing command\n");
      ToolboxCmdHelp(argv[0]);
      exit(EX_USAGE);
   }
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
   int c;
   /*
    * Check if we are in a VM
    */
   if (!VmCheck_IsVirtualWorld()) {
      fprintf(stderr, "%s must be run inside a virtual machine.\n", argv[0]);
      exit(EXIT_FAILURE);
   }

   if (argc < 2) {
      ToolboxCmdHelp(argv[0]);
      exit(EX_USAGE);
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
      if (c == -1)
	 break;

      switch (c) {
      case 'h':
	 ToolboxCmdHelp(argv[0]);
	 exit(EXIT_SUCCESS);

      case 'v':
	 printf("%s\n", TOOLBOXCMD_VERSION_STRING);
	 exit(EXIT_SUCCESS);

      case 'q':
	 quiet_flag = 1;
	 break;

      case '?':
	 /* getopt_long already printed an error message. */
	 ToolboxCmdHelp(argv[0]);
	 abort();
	 break;

      default:
	 abort();
      }
   }

   /* Process any remaining command line arguments (not options), and
    * execute corresponding command
    */
   if (optind < argc) {
      CmdTable *cmd = ParseCommand(argv, argc);
      if (cmd->command == NULL) {
         ToolboxCmdHelp(argv[0]);
	 return EX_USAGE;
      }
      if (cmd->requireRoot && !System_IsUserAdmin()) {
	 fprintf(stderr,"You must be root to perform %s operations\n",
                 cmd->command);
	 return EX_NOPERM;
      }
      return cmd->func(argv, argc);
   }
   return EXIT_SUCCESS;
}
