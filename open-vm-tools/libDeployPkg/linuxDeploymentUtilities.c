/*********************************************************
 * Copyright (C) 2016-2017 VMware, Inc. All rights reserved.
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

extern int ForkExecAndWaitCommand(const char* command);
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
   // Expected regex in cloud.cfg file
   const char *cloudInitRegex = "^\\s*disable_vmware_customization\\s*:\\s*false\\s*$";
   int reti = regcomp(&regex, cloudInitRegex, 0);
   if (reti) {
      char buf[256];
      regerror(reti, &regex, buf, sizeof(buf));
      sLog(log_warning, "Error compiling regex for cloud-init flag: %s", buf);
      return isEnabled;
   }

   sLog(log_info, "Checking if cloud.cfg exists and if cloud-init is enabled.");
   // Read cloud.cfg file and find expected string.
   cloudFile = fopen(cloudFilePath, "r");
   if (cloudFile == NULL) {
      sLog(log_info, "Could not open file: %s", strerror(errno));
      goto done;
   }
   while(fgets(line, sizeof(line), cloudFile)) {
      if (!regexec(&regex, line, 0, NULL, 0)) {
         isEnabled = true;
         break;
      }
   }
   if (ferror(cloudFile)) {
      sLog(log_warning, "Error reading file: %s", strerror(errno));
      isEnabled = false;
   }
   fclose(cloudFile);

done:
   regfree(&regex);
   return isEnabled;
}

/**
 *-----------------------------------------------------------------------------
 *
 * HasCustomScript
 *
 * Get custom script name if it exists.
 *
 * @param   [IN]      dirPath     path to extracted cab files
 * @param   [IN/OUT]  scriptName  name of the user uploaded custom script.
 *                                scriptName will be set only if custom script
 *                                exists.
 * @returns TRUE if custom script exists in dirPath
 *
 * ----------------------------------------------------------------------------
 **/
bool
HasCustomScript(const char* dirPath, char** scriptName)
{
   bool hasScript = false;
   size_t scriptSize;
   static const char *customScriptRegex = "^script[A-Za-z0-9]*\\.bat";
   DIR *tempDir;
   struct dirent *dir;
   regex_t scriptRegex;
   int ret = regcomp(&scriptRegex, customScriptRegex, 0);
   if (ret) {
      char buf[256];
      regerror(ret, &scriptRegex, buf, sizeof(buf));
      sLog(log_warning, "Error compiling regex for custom script: %s",
           buf);
      return hasScript;
   }
   sLog(log_info, "Check if custom script(pre/post customization) exists.");
   tempDir = opendir(dirPath);
   if (tempDir == NULL) {
      sLog(log_warning, "Could not open directory %s: error: %s", dirPath,
           strerror(errno));
      goto done;
   }
   while ((dir = readdir(tempDir)) != NULL) {
      if (!regexec(&scriptRegex, dir->d_name, 0, NULL, 0)) {
         scriptSize = strlen(dir->d_name);
         *scriptName = malloc(sizeof(char) * scriptSize + 1);
         if (*scriptName == NULL) {
            sLog(log_warning, "Could not allocate memory for scriptName: %s",
                 strerror(errno));
            closedir(tempDir);
            goto done;
         }
         **scriptName = '\0';
         strncat(*scriptName, dir->d_name, scriptSize);
         hasScript = true;
      }
   }
   closedir(tempDir);
done:
   regfree(&scriptRegex);
   return hasScript;
}

