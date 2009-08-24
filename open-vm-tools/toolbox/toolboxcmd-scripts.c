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
 * toolboxcmd-scripts.c --
 *
 *     The scripts functions for the linux toolbox-cmd
 */

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "system.h"
#include "toolboxCmdInt.h"
#include "vmtools.h"


#define SCRIPT_SUSPEND "suspend"
#define SCRIPT_RESUME  "resume"
#define SCRIPT_OFF     "shutdown"
#define SCRIPT_ON      "power"

typedef enum ScriptType {
   Default,
   Current
} ScriptType;

static int ScriptToggle(const char *apm, Bool enable, int quiet_flag);
static const char* GetConfName(const char *apm);
static int GetConfEntry(const char *apm, ScriptType type);


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
   gchar *confPath;
   GKeyFile *confDict;

   confPath = VMTools_GetToolsConfFile();
   confDict = VMTools_LoadConfig(confPath,
                                 G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                                 System_IsUserAdmin());

   if (confDict == NULL) {
      confDict = g_key_file_new();
   }

   g_free(confPath);
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
GetConfEntry(const char *apm,  // IN: apm name
             ScriptType type)  // IN: Script type (default or current)
{
   gchar *entry = NULL;
   GKeyFile *confDict = NULL;
   const char *confName;
   int ret;

   confName = GetConfName(apm);
   if (!confName) {
      fprintf(stderr, "Unknown operation\n");
      return EX_USAGE;
   }

   confDict = LoadConfFile();

   if (type == Default) {
      entry = g_strdup(GuestApp_GetDefaultScript(confName));
   } else if (type == Current) {
      entry = g_key_file_get_string(confDict, "powerops", confName, NULL);
      if (entry == NULL) {
         entry = g_strdup(GuestApp_GetDefaultScript(confName));
      }
   }

   if (strlen(entry) > 0) {
      printf("%s\n", entry);
      ret = EXIT_SUCCESS;
   } else {
      fprintf(stderr, "No script for operation %s\n", apm);
      ret = EX_TEMPFAIL;
   }

   g_free(entry);
   g_key_file_free(confDict);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Script_GetDefault  --
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

int
Script_GetDefault(const char *apm) // IN: APM name
{
   return GetConfEntry(apm, Default);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Script_GetCurrent  --
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

int
Script_GetCurrent(const char *apm) // IN: apm function name
{
   return GetConfEntry(apm, Current);
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
ScriptToggle(const char *apm, // IN: APM name
             Bool enable,     // IN: status
             int quiet_flag)  // IN: Verbosity flag
{
   const char *path;
   const char *confName;
   gchar *confPath;
   int ret = EXIT_SUCCESS;
   GKeyFile *confDict;
   GError *err = NULL;

   confName = GetConfName(apm);

   if (!confName) {
      fprintf(stderr, "Unknown operation\n");
      return EX_USAGE;
   }

   confDict = LoadConfFile();

   if (!enable) {
      path = "";
   } else {
      path = GuestApp_GetDefaultScript(confName);
   }

   g_key_file_set_string(confDict, "powerops", confName, path);
   confPath = VMTools_GetToolsConfFile();
   if (!VMTools_WriteConfig(confPath, confDict, &err)) {
      fprintf(stderr, "Error writing config: %s\n", err->message);
      g_clear_error(&err);
      ret = EX_TEMPFAIL;
   }

   g_key_file_free(confDict);
   g_free(confPath);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Script_Enable  --
 *
 *      enables script.
 *
 * Results:
 *      Same as ScriptToggle.
 *
 * Side effects:
 *      Same as ScriptToggle.
 *
 *-----------------------------------------------------------------------------
 */

int
Script_Enable(const char *apm,   // IN: APM name
              int quiet_flag)    // IN: Verbosity flag
{
   return ScriptToggle(apm, TRUE, quiet_flag);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Script_Disable  --
 *
 *      disable script
 *
 * Results:
 *      Same as ScriptToggle.
 *
 * Side effects:
 *      Same as ScriptToggle.
 *
 *-----------------------------------------------------------------------------
 */

int
Script_Disable(const char *apm,  // IN: APM name
               int quiet_flag)   // IN: Verbosity Flag
{
   return ScriptToggle(apm, FALSE, quiet_flag);
}


/*
 *-----------------------------------------------------------------------------
 *
 * sets a script to the given path  --
 *
 *      disable script.
 *
 * Results:
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

int
Script_Set(const char *apm,   // IN: APM name
           const char *path,  // IN: path to script
           int quiet_flag)    // IN: Verbosity flag
{
   const char *confName;
   int ret = EXIT_SUCCESS;
   gchar *confPath = NULL;
   GKeyFile *confDict = NULL;
   GError *err = NULL;

   if (!File_Exists(path)) {
      fprintf(stderr, "%s doesn't exist\n", path);
      return EX_OSFILE;
   }

   confName = GetConfName(apm);
   if (!confName) {
      fprintf(stderr, "Unknown operation\n");
      return EX_USAGE;
   }

   confPath = VMTools_GetToolsConfFile();
   confDict = LoadConfFile();

   g_key_file_set_string(confDict, "powerops", confName, path);

   if (!VMTools_WriteConfig(confPath, confDict, &err)) {
      fprintf(stderr, "Error writing config: %s\n", err->message);
      g_clear_error(&err);
      ret = EX_TEMPFAIL;
   }

   g_key_file_free(confDict);
   g_free(confPath);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Script_CheckName  --
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

Bool
Script_CheckName(const char *apm) // IN: script name
{
   return GetConfName(apm) != NULL;
}
