/*********************************************************
 * Copyright (c) 2008-2016,2020-2021 VMware, Inc. All rights reserved.
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
 * toolboxcmd-scripts.c --
 *
 *     The scripts functions for the linux toolbox-cmd
 */

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "conf.h"
#include "file.h"
#include "guestApp.h"
#include "system.h"
#include "toolboxCmdInt.h"
#include "vmware/tools/i18n.h"
#include "vmware/tools/utils.h"


#define SCRIPT_SUSPEND "suspend"
#define SCRIPT_RESUME  "resume"
#define SCRIPT_OFF     "shutdown"
#define SCRIPT_ON      "power"

typedef enum ScriptType {
   Default,
   Current
} ScriptType;


/*
 *-----------------------------------------------------------------------------
 *
 * GetConfName --
 *
 *      Gets the apm name.
 *
 * Results:
 *      The apm name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static const char *
GetConfName(const char *apm) // IN: apm name.
{
   if (toolbox_strcmp(apm, SCRIPT_SUSPEND) == 0) {
      return CONFNAME_SUSPENDSCRIPT;
   } else if (toolbox_strcmp(apm, SCRIPT_RESUME) == 0) {
      return CONFNAME_RESUMESCRIPT;
   } else if (toolbox_strcmp(apm, SCRIPT_OFF) == 0) {
      return CONFNAME_POWEROFFSCRIPT;
   } else if (toolbox_strcmp(apm, SCRIPT_ON) == 0) {
     return CONFNAME_POWERONSCRIPT;
   } else {
      return NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * LoadConfFile --
 *
 *      Loads the tools configuration file. If running as admin, tries to
 *      upgrade it if an old-style file is found.
 *
 * Results:
 *      A new GKeyFile *. If the conf file is not valid, it will be empty.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static GKeyFile *
LoadConfFile(void)
{
   GKeyFile *confDict = NULL;

   VMTools_LoadConfig(NULL,
                      G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                      &confDict,
                      NULL);

   if (confDict == NULL) {
      confDict = g_key_file_new();
   }

   return confDict;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetConfEntry --
 *
 *      Gets the entry in the config dictionary.
 *
 * Results:
 *      EXIT_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
GetConfEntry(const char *progName,  // IN: program name (argv[0])
             const char *apm,       // IN: apm name
             ScriptType type)       // IN: Script type (default or current)
{
   gchar *entry;
   GKeyFile *confDict;
   const char *confName;
   int len;
   int ret;

   confName = GetConfName(apm);
   if (!confName) {
      ToolsCmd_UnknownEntityError(progName,
                                  SU_(script.operation, "operation"),
                                  apm);
      return EX_USAGE;
   }

   confDict = LoadConfFile();
   TOOLBOXCMD_LOAD_GLOBALCONFIG(confDict)

   switch (type) {
   case Current:
      entry = g_key_file_get_string(confDict, "powerops", confName, NULL);
      if (entry) {
         break;
      }
      /* Fall through */

   default:
      entry = g_strdup(GuestApp_GetDefaultScript(confName));
      break;
   }

   len = strlen(entry);
   if (len > 0) {

      /* If script path is not absolute, assume the Tools install path. */
      if (!g_path_is_absolute(entry)) {
         char *defaultPath = GuestApp_GetInstallPath();
         char *tmp;
         Bool quoted;

         ASSERT(defaultPath != NULL);

         /* Cope with old configs that added quotes around script paths. */
         quoted = (entry[0] == '"' && entry[len - 1] == '"');
         tmp = g_strdup_printf("%s%c%.*s", defaultPath, DIRSEPC,
                                quoted ? len - 2 : len,
                                quoted ? entry + 1 : entry);

         vm_free(defaultPath);

         g_free(entry);
         entry = tmp;
      }

      g_print("%s\n", entry);
      ret = EXIT_SUCCESS;
   } else {
      ToolsCmd_PrintErr(SU_(script.unknownop, "No script for operation %s.\n"),
                        apm);
      ret = EX_TEMPFAIL;
   }

   g_free(entry);
   g_key_file_free(confDict);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScriptGetDefault  --
 *
 *      Gets the path to default script.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_USAGE on parse errors.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Print to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
ScriptGetDefault(const char *progName, // IN: program name (argv[0])
                 const char *apm)      // IN: APM name
{
   return GetConfEntry(progName, apm, Default);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScriptGetCurrent  --
 *
 *      Gets the path to Current script.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_USAGE on parse errors.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Print to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
ScriptGetCurrent(const char *progName, // IN: program name (argv[0])
                 const char *apm)      // IN: apm function name
{
   return GetConfEntry(progName, apm, Current);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScriptToggle  --
 *
 *      enables/disable script.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EX_USAGE on parse errors.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Enables/Disables a script
 *      Print to stderr on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
ScriptToggle(const char *progName,  // IN: program name (argv[0])
             const char *apm,       // IN: APM name
             Bool enable)           // IN: status
{
   const char *path;
   const char *confName;
   int ret = EXIT_SUCCESS;
   GKeyFile *confDict;
   GError *err = NULL;

   confName = GetConfName(apm);

   if (!confName) {
      ToolsCmd_UnknownEntityError(progName,
                                  SU_(script.operation, "operation"),
                                  apm);
      return EX_USAGE;
   }

   confDict = LoadConfFile();

   if (!enable) {
      path = "";
   } else {
      path = GuestApp_GetDefaultScript(confName);
   }

   g_key_file_set_string(confDict, "powerops", confName, path);
   if (!VMTools_WriteConfig(NULL, confDict, &err)) {
      ToolsCmd_PrintErr(SU_(script.write.error, "Error writing config: %s\n"),
                        err->message);
      g_clear_error(&err);
      ret = EX_TEMPFAIL;
   }

   g_key_file_free(confDict);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScriptSet  --
 *
 *      Sets a script to the given path.
 *
 * Results:
 *      EX_OSFILE if path doesn't exist.
 *      EXIT_SUCCESS on success.
 *      EX_USAGE on parse errors.
 *      EX_TEMPFAIL on failure.
 *
 * Side effects:
 *      Sets a script.
 *      Print to stderr and exit on error.
 *
 *-----------------------------------------------------------------------------
 */

static int
ScriptSet(const char *progName,  // IN: program name (argv[0])
          const char *apm,       // IN: APM name
          const char *path)      // IN: Verbosity flag
{
   const char *confName;
   int ret = EXIT_SUCCESS;
   GKeyFile *confDict = NULL;
   GError *err = NULL;

   if (!File_Exists(path)) {
      ToolsCmd_PrintErr(SU_(script.notfound, "%s doesn't exist.\n"), path);
      return EX_OSFILE;
   }

   confName = GetConfName(apm);
   if (!confName) {
      ToolsCmd_UnknownEntityError(progName,
                                  SU_(script.operation, "operation"),
                                  apm);
      return EX_USAGE;
   }

   confDict = LoadConfFile();
   g_key_file_set_string(confDict, "powerops", confName, path);

   if (!VMTools_WriteConfig(NULL, confDict, &err)) {
      ToolsCmd_PrintErr(SU_(script.write.error, "Error writing config: %s\n"),
                        err->message);
      g_clear_error(&err);
      ret = EX_TEMPFAIL;
   }

   g_key_file_free(confDict);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ScriptCheckName  --
 *
 *      Check if it is known script
 *
 * Results:
 *      TRUE if name is known, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
ScriptCheckName(const char *apm) // IN: script name
{
   return GetConfName(apm) != NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Script_Command --
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

int
Script_Command(char **argv,    // IN: command line arguments.
               int argc,       // IN: the length of the command line arguments.
               gboolean quiet) // IN
{
   const char *apm;

   if (++optind >= argc) {
      ToolsCmd_MissingEntityError(argv[0], SU_(arg.scripttype, "script type"));
      return EX_USAGE;
   }

   apm = argv[optind++];

   if (!ScriptCheckName(apm)) {
      ToolsCmd_UnknownEntityError(argv[0], SU_(arg.scripttype, "script type"), apm);
      return EX_USAGE;
   }

   if (optind >= argc) {
      ToolsCmd_MissingEntityError(argv[0], SU_(arg.subcommand, "subcommand"));
      return EX_USAGE;
   }

   if (toolbox_strcmp(argv[optind], "default") == 0) {
      return ScriptGetDefault(argv[0], apm);
   } else if (toolbox_strcmp(argv[optind], "current") == 0) {
      return ScriptGetCurrent(argv[0], apm);
   } else if (toolbox_strcmp(argv[optind], "set") == 0) {
      if (++optind >= argc) {
         ToolsCmd_MissingEntityError(argv[0], SU_(arg.scriptpath, "script path"));
         return EX_USAGE;
      }
      return ScriptSet(argv[0], apm, argv[optind]);
   } else if (toolbox_strcmp(argv[optind], "enable") == 0) {
      return ScriptToggle(argv[0], apm, TRUE);
   } else if (toolbox_strcmp(argv[optind], "disable") == 0) {
      return ScriptToggle(argv[0], apm, FALSE);
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
 * Script_Help --
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

void
Script_Help(const char *progName, // IN: The name of the program obtained from argv[0]
            const char *cmd)      // IN
{
   g_print(SU_(help.script,
               "%s: control the scripts run in response to power operations\n"
               "Usage: %s %s <power|resume|suspend|shutdown> <subcommand> [args]\n\n"
               "Subcommands:\n"
               "   enable: enable the given script and restore its path to the default\n"
               "   disable: disable the given script\n"
               "   set <full_path>: set the given script to the given path\n"
               "   default: print the default path of the given script\n"
               "   current: print the current path of the given script\n"
               "   NOTE: If the path is not present in tools.conf, its\n"
               "   value from the global configuration is returned if present\n"),
           cmd, progName, cmd);
}

