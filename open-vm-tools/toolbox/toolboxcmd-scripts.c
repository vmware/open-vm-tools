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

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "toolboxCmdInt.h"


#define SCRIPT_SUSPEND "suspend"
#define SCRIPT_RESUME  "resume"
#define SCRIPT_OFF     "shutdown"
#define SCRIPT_ON      "power"

typedef enum ScriptType {
   Default,
   Current
} ScriptType;

static int ScriptToggle(char *apm, Bool enable, int quiet_flag);
static char* GetConfName(char *apm);
static int GetConfEntry(char *apm, ScriptType type);
static int WriteDict(GuestApp_Dict *confDict, int quiet_flag);


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

static char *
GetConfName(char *apm) // IN: apm name.
{
   if (strcmp(apm, SCRIPT_SUSPEND) == 0) {
      return CONFNAME_SUSPENDSCRIPT;
   }else if (strcmp(apm, SCRIPT_RESUME) == 0) {
      return CONFNAME_RESUMESCRIPT;
   } else if (strcmp(apm, SCRIPT_OFF) == 0) {
      return CONFNAME_POWEROFFSCRIPT;
   } else if (strcmp(apm, SCRIPT_ON) == 0) {
     return CONFNAME_POWERONSCRIPT;
   } else {
      return NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetConfEntry --
 *
 *      Gets the entry in the ConfDict.
 *
 * Results:
 *      The conf entry.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
GetConfEntry(char *apm,        // IN: apm name
             ScriptType type)  // IN: Script type (default or current)
{
   const char *entry = NULL;
   GuestApp_Dict *confDict;
   char *confName;
   confDict = Conf_Load();
   confName = GetConfName(apm);
   if (!confName) {
      fprintf(stderr, "Unknown operation\n");
      return EX_USAGE;
   }
   if (type == Default) {
      entry = GuestApp_GetDictEntryDefault(confDict, confName);
   } else if (type == Current) {
      entry = GuestApp_GetDictEntry(confDict, confName);
   }
   if (entry) {
      printf("%s\n", entry);
      return EXIT_SUCCESS;
   } else {
      fprintf(stderr, "Error retreiving the path for script %s\n", apm);
      return EX_TEMPFAIL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * WriteDict --
 *
 *      Writes a ConfDict to the hard disk.
 *
 * Results:
 *      EXIT_SUCCESS on success.
 *      EXIT_TEMPFAIL on failure
 *
 * Side effects:
 *      Writes the Dict to the hard disk.
 *
 *-----------------------------------------------------------------------------
 */

static int
WriteDict(GuestApp_Dict *confDict, // IN: The Dict to be writen
          int quiet_flag)          // IN: Verbosity flag.
{
   if (!GuestApp_WriteDict(confDict)) {
      fprintf(stderr, "Unable to write dictionary\n");
      GuestApp_FreeDict(confDict);
      return EX_TEMPFAIL;
   } else {
      if (!quiet_flag) {
         printf("Script Set\n");
      }
      GuestApp_FreeDict(confDict);
      return EXIT_SUCCESS;
   }
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
Script_GetDefault(char *apm) // IN: APM name
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
Script_GetCurrent(char *apm) // IN: apm function name
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
ScriptToggle(char *apm,       // IN: APM name
              Bool enable,    // IN: status
              int quiet_flag) // IN: Verbosity flag
{
   const char *path;
   char *confName;
   GuestApp_Dict *confDict;
   confDict = Conf_Load();
   confName = GetConfName(apm);

   if (!confName) {
      fprintf(stderr, "Unknown operation\n");
      return EX_USAGE;
   }

   if (!enable) {
      path = "";
   } else {
      path = GuestApp_GetDictEntryDefault (confDict, confName);
   }

   GuestApp_SetDictEntry(confDict, confName, path);

   return WriteDict(confDict, quiet_flag);
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
Script_Enable(char *apm,      // IN: APM name
              int quiet_flag) // IN: Verbosity flag
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
Script_Disable(char *apm,      // IN: APM name
               int quiet_flag) // IN: Verbosity Flag
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
Script_Set(char *apm,	   // IN: APM name
           char *path,	   // IN: path to script
           int quiet_flag) // IN: Verbosity flag
{
   char *confName;
   GuestApp_Dict *confDict;
   if (!File_Exists(path)) {
      fprintf(stderr, "%s doesn't exists\n", path);
      return EX_OSFILE;
   }
   confDict = Conf_Load();
   confName = GetConfName(apm);
   if (!confName) {
      fprintf(stderr, "Unknown operation\n");
      return EX_USAGE;
   }

   GuestApp_SetDictEntry(confDict, confName, path);
   return WriteDict(confDict, quiet_flag);

}

