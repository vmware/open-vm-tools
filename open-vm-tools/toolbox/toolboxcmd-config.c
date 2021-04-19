/*********************************************************
 * Copyright (C) 2016,2020-2021 VMware, Inc. All rights reserved.
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
 * toolboxcmd-config.c --
 *
 *    VMTools config operations.
 *
 *    Supports a basic set/get of individual tools.conf key-value pairs.
 *
 */

#include <time.h>

#include "conf.h"
#include "toolboxCmdInt.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/log.h"


/*
 *-----------------------------------------------------------------------------
 *
 * ConfigSet --
 *
 *      Set a config entry.
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
ConfigSet(const char *section,         // config section
          const char *key,             // key
          const char *value)           // value
{
   GKeyFile *confDict = NULL;
   GError *err = NULL;
   int ret = EXIT_SUCCESS;

   VMTools_LoadConfig(NULL,
                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                      &confDict,
                      NULL);

   if (confDict == NULL) {
      confDict = g_key_file_new();
   }

   g_key_file_set_string(confDict, section,
                         key, value);

   if (!VMTools_WriteConfig(NULL, confDict, &err)) {
      ToolsCmd_PrintErr(SU_(script.write.error, "Error writing config: %s\n"),
                        err ? err->message : "");
      g_clear_error(&err);
      ret = EX_TEMPFAIL;
   }

   g_key_file_free(confDict);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ConfigGet --
 *
 *      Get config value.
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
ConfigGet(const char *section,      // section
          const char *key)          // key
{
   GKeyFile *confDict = NULL;
   int ret = EXIT_SUCCESS;
   gchar *value = NULL;

   VMTools_LoadConfig(NULL,
                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                      &confDict,
                      NULL);

   if (confDict) {
      TOOLBOXCMD_LOAD_GLOBALCONFIG(confDict)
      value = g_key_file_get_string(confDict, section,
                                    key, NULL);
   } else {
      ret = EX_UNAVAILABLE;
   }

   if (value) {
      g_print("[%s] %s = %s\n", section, key, value);
   } else {
      g_print("[%s] %s UNSET\n", section, key);
   }

   g_key_file_free(confDict);
   g_free(value);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ConfigRemove --
 *
 *      Remove config key.
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
ConfigRemove(const char *section,      // section
             const char *key)          // key
{
   GKeyFile *confDict = NULL;
   int ret = EXIT_SUCCESS;
   GError *err = NULL;

   VMTools_LoadConfig(NULL,
                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                      &confDict,
                      NULL);

   // be idempotent -- ignore any error about non-existant config or key
   if (confDict) {
      /*
       * our ancient FreeBSD glib expects g_key_file_remove_key()
       * to return void, and since we don't care anyways, ignore the
       * return so it builds everywhere.
       */
      (void) g_key_file_remove_key(confDict, section,
                                   key, NULL);
   } else {
      return EX_UNAVAILABLE;
   }

   if (!VMTools_WriteConfig(NULL, confDict, &err)) {
      ToolsCmd_PrintErr(SU_(script.write.error, "Error writing config: %s\n"),
                        err ? err->message : "");
      g_clear_error(&err);
      ret = EX_TEMPFAIL;
   }

   g_key_file_free(confDict);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Config_Command --
 *
 *      Handle and parse config commands.
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
Config_Command(char **argv,      // IN: Command line arguments
               int argc,         // IN: Length of command line arguments
               gboolean quiet)   // IN
{
   const char *op;
   const char *section;
   const char *key;

   if (optind >= argc) {
      ToolsCmd_MissingEntityError(argv[0],
                                  SU_(arg.config.operation, "config operation"));
      return EX_USAGE;
   }

   if ((optind + 1) >= argc) {
      ToolsCmd_MissingEntityError(argv[0],
                                  SU_(arg.config.section, "config section"));
      return EX_USAGE;
   }

   if ((optind + 2) >= argc) {
      ToolsCmd_MissingEntityError(argv[0],
                                  SU_(arg.config.key, "config key"));
      return EX_USAGE;
   }

   op = argv[optind];
   section = argv[optind + 1];
   key = argv[optind + 2];

   if (toolbox_strcmp(op, "set") == 0) {
      const char *value;

      if ((optind + 3) >= argc) {
         ToolsCmd_MissingEntityError(argv[0],
                                     SU_(arg.config.value, "config value"));
         return EX_USAGE;
      }
      value = argv[optind + 3];

      return ConfigSet(section, key, value);
   } else if (toolbox_strcmp(op, "get") == 0) {
      return ConfigGet(section, key);
   } else if (toolbox_strcmp(op, "remove") == 0) {
      return ConfigRemove(section, key);
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
 * Config_Help --
 *
 *      Prints the help for the config command.
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
Config_Help(const char *progName, // IN: The name of the program obtained from argv[0]
            const char *cmd)      // IN
{
   g_print(SU_(help.config,
               "%s: modify Tools configuration\n"
               "Usage: %s %s <subcommand>\n\n"
               "Subcommands:\n"
               "   get <section> <key>: display current value for <key>\n"
               "   NOTE: If the <key> is not present in tools.conf, its\n"
               "   value from the global configuration is returned if present\n"
               "   set <section> <key> <value>: set <key> to <value>\n\n"
               "   remove <section> <key>: remove <key>\n\n"
               "<section> can be any supported section, such as logging, guestoperations or guestinfo.\n"
               "<key> can be any configuration key.\n"
               "<value> can be any value.\n"),
           cmd, progName, cmd);
}

