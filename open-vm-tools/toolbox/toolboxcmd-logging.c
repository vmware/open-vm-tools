/*********************************************************
 * Copyright (C) 2015-2016,2020-2021 VMware, Inc. All rights reserved.
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
 * toolboxcmd-logging.c --
 *
 *    Various logging operations for toolbox-cmd.
 *
 *    Note:
 *    Servicenames are not sanity checked.  This means it can report
 *    the default value for a bogus servicename, or setting values for
 *    an unsupported servicename.  But any sanity checking would require
 *    all possible servicenames to #define themselves.  Lack of
 *    a sanity check overrrides this complexity.
 *
 *    TODO:  This currently just modifies the tools.conf file, which means
 *    that if tools is running, it can talke up to 5 seconds to react
 *    to any changes.  It would be better if toolsd could be poked to
 *    shrink that delay.
 */

#include <time.h>

#include "conf.h"
#include "toolboxCmdInt.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/log.h"

#define LOGGING_CONF_SECTION "logging"

/*
 *-----------------------------------------------------------------------------
 *
 * LoggingCheckLevel --
 *
 *      Sanity check logging level.
 *
 * Results:
 *      Returns TRUE if its a valid logging level, FALSE if not.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
LoggingCheckLevel(char *level)           // logging level
{
   if ((strcmp("error", level) == 0) ||
       (strcmp("critical", level) == 0) ||
       (strcmp("warning", level) == 0) ||
       (strcmp("message", level) == 0) ||
       (strcmp("info", level) == 0) ||
       (strcmp("debug", level) == 0)) {
      return TRUE;
   } else {
      return FALSE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * LoggingSetLevel --
 *
 *      Set logging level
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

int
LoggingSetLevel(char *service,         // service
                char *level)           // logging level
{
   GKeyFile *confDict = NULL;
   gchar *confName;
   GError *err = NULL;
   int ret = EXIT_SUCCESS;

   VMTools_LoadConfig(NULL,
                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                      &confDict,
                      NULL);

   if (confDict == NULL) {
      confDict = g_key_file_new();
   }

   confName = g_strdup_printf("%s.level", service);

   g_key_file_set_string(confDict, LOGGING_CONF_SECTION,
                         confName, level);

   if (!VMTools_WriteConfig(NULL, confDict, &err)) {
      ToolsCmd_PrintErr(SU_(script.write.error, "Error writing config: %s\n"),
                        err->message);
      g_clear_error(&err);
      ret = EX_TEMPFAIL;
   }

   g_key_file_free(confDict);
   g_free(confName);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LoggingGetLevel --
 *
 *      Get current logging level
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

int
LoggingGetLevel(char *service)         // service
{
   GKeyFile *confDict = NULL;
   gchar *confName;
   int ret = EXIT_SUCCESS;
   gchar *level;

   VMTools_LoadConfig(NULL,
                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                      &confDict,
                      NULL);

   if (confDict == NULL) {
      confDict = g_key_file_new();
   }

   TOOLBOXCMD_LOAD_GLOBALCONFIG(confDict)

   confName = g_strdup_printf("%s.level", service);

   level = g_key_file_get_string(confDict, LOGGING_CONF_SECTION,
                                 confName, NULL);

   if (level) {
      g_print("%s = %s\n", confName, level);
   } else {
      g_print("%s = %s\n", confName, VMTOOLS_LOGGING_LEVEL_DEFAULT);
   }
   g_key_file_free(confDict);
   g_free(confName);
   g_free(level);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Logging_Command --
 *
 *      Handle and parse logging commands.
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

int
Logging_Command(char **argv,      // IN: Command line arguments
                int argc,         // IN: Length of command line arguments
                gboolean quiet)   // IN
{
   char *subcommand = argv[optind];
   char *op;

   if ((optind + 1) >= argc) {
      ToolsCmd_MissingEntityError(argv[0],
                                  SU_(arg.logging.subcommand, "logging operation"));
      return EX_USAGE;
   }
   if ((optind + 2) >= argc) {
      ToolsCmd_MissingEntityError(argv[0],
                                  SU_(arg.logging.service, "logging servicename"));
      return EX_USAGE;
   }

   op = argv[optind + 1];

   if (toolbox_strcmp(subcommand, "level") == 0) {
      if (toolbox_strcmp(op, "set") == 0) {
         if ((optind + 3) >= argc) {
            ToolsCmd_MissingEntityError(argv[0],
                                        SU_(arg.logging.level, "logging level"));
            return EX_USAGE;
         } else {
            if (!LoggingCheckLevel(argv[optind + 3])) {
               ToolsCmd_UnknownEntityError(argv[0],
                                           SU_(arg.logging.level, "logging level"),
                                           argv[optind + 3]);
               return EX_USAGE;
            }
            return LoggingSetLevel(argv[optind + 2], argv[optind + 3]);
         }
      } else if (toolbox_strcmp(op, "get") == 0) {
         return LoggingGetLevel(argv[optind + 2]);
      } else {
         ToolsCmd_UnknownEntityError(argv[0],
                                     SU_(arg.subcommand, "subcommand"),
                                     argv[optind + 1]);
         return EX_USAGE;
      }
   } else {
      ToolsCmd_UnknownEntityError(argv[0],
                                  SU_(arg.subcommand, "subcommand"),
                                  argv[optind]);
      return EX_USAGE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Logging_Help --
 *
 *      Prints the help for the logging command.
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
Logging_Help(const char *progName, // IN: The name of the program obtained from argv[0]
             const char *cmd)      // IN
{
   g_print(SU_(help.logging,
               "%s: modify tools logging\n"
               "Usage: %s %s level <subcommand> <servicename> <level>\n\n"
               "Subcommands:\n"
               "   get <servicename>: display current level\n"
               "   NOTE: If the level is not present in tools.conf, its\n"
               "   value from the global configuration is returned if present\n"
               "   set <servicename> <level>: set current level\n\n"
               "<servicename> can be any supported service, such as vmsvc or vmusr\n"
               "<level> can be one of error, critical, warning, info, message, debug\n"
               "   default is %s\n"),
           cmd, progName, cmd, VMTOOLS_LOGGING_LEVEL_DEFAULT);
}

