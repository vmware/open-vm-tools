/*********************************************************
 * Copyright (C) 2016-2019 VMware, Inc. All rights reserved.
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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "linuxDeploymentUtilities.h"

extern LogFunction sLog;

/**
 *----------------------------------------------------------------------------
 *
 * IsCloudInitEnabled
 *
 * Function to determine if cloud-init is enabled.
 * Essentially it does
 *  - read a cloud-init config file
 *  - Find if a particular flag is enabled or disabled.
 *
 *  @param   [IN]  cloudFilePath path of the cloud-init config file
 *  @returns TRUE if disable_vmware_customization is false and FALSE otherwise.
 *
 *----------------------------------------------------------------------------
 **/
bool
IsCloudInitEnabled(const char *cloudFilePath)
{
   bool isEnabled = false;
   FILE *cloudFile;
   char line[256];
   regex_t regex;
   const char *cloudInitRegex =
               "^\\s*disable_vmware_customization\\s*:\\s*false\\s*$";
   int reti;

   sLog(log_info, "Checking if cloud.cfg exists and if cloud-init is enabled.");
   cloudFile = fopen(cloudFilePath, "r");
   if (cloudFile == NULL) {
      sLog(log_info, "Could not open file: %s", strerror(errno));
      return isEnabled;
   }

   reti = regcomp(&regex, cloudInitRegex, 0);
   if (reti != 0) {
      char buf[256];
      regerror(reti, &regex, buf, sizeof(buf));
      sLog(log_error, "Error compiling regex for cloud-init flag: %s", buf);
      goto done;
   }

   while (fgets(line, sizeof(line), cloudFile) != NULL) {
      if (regexec(&regex, line, 0, NULL, 0) == 0) {
         isEnabled = true;
         break;
      }
   }
   if (ferror(cloudFile) != 0) {
      sLog(log_warning, "Error reading file: %s", strerror(errno));
      isEnabled = false;
   }
   regfree(&regex);

done:
   fclose(cloudFile);
   return isEnabled;
}

/**
 *-----------------------------------------------------------------------------
 *
 * GetCustomScript
 *
 * Get custom script name if it exists.  Returns the first script found.
 *
 * @param   [IN]      dirPath     path to extracted cab files
 *
 * @returns the script name of the user uploaded custom script if it
 *          is found in dirPath.  Must be freed by caller.
 *
 *          NULL on failure or if the script does not exist
 *
 * ----------------------------------------------------------------------------
 **/
char *
GetCustomScript(const char* dirPath)
{
   char *scriptName = NULL;
   static const char *customScriptRegex = "^script[A-Za-z0-9]*\\.bat";
   DIR *tempDir;
   struct dirent *dir;
   regex_t scriptRegex;
   int regRet;

   sLog(log_info, "Check if custom script(pre/post customization) exists.");
   tempDir = opendir(dirPath);
   if (tempDir == NULL) {
      sLog(log_warning, "Could not open directory %s: error: %s", dirPath,
           strerror(errno));
      return scriptName;
   }

   regRet = regcomp(&scriptRegex, customScriptRegex, 0);
   if (regRet != 0) {
      char buf[256];

      regerror(regRet, &scriptRegex, buf, sizeof(buf));
      sLog(log_error, "Error compiling regex for custom script: %s", buf);
      goto done;
   }

   while ((dir = readdir(tempDir)) != NULL) {
      if (regexec(&scriptRegex, dir->d_name, 0, NULL, 0) == 0) {
         scriptName = strdup(dir->d_name);
         if (scriptName == NULL) {
            sLog(log_warning, "Could not allocate memory for scriptName: %s",
                 strerror(errno));
            break;
         }
         break;
      }
   }
   regfree(&scriptRegex);

done:
   closedir(tempDir);
   return scriptName;
}

