/*********************************************************
 * Copyright (c) 2016-2019, 2023 VMware, Inc. All rights reserved.
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "linuxDeploymentUtilities.h"
#include "str.h"

extern LogFunction sLog;

// The status code of flag 'disable_vmware_customization'
typedef enum DISABLE_VMWARE_CUSTIOMIZATION_FLAG_STATUS_CODE {
   DISABLE_VMWARE_CUSTOMIZATION_FLAG_UNSET = 0,
   DISABLE_VMWARE_CUSTOMIZATION_FLAG_SET_TRUE,
   DISABLE_VMWARE_CUSTOMIZATION_FLAG_SET_FALSE,
} DISABLE_VMWARE_CUSTIOMIZATION_FLAG_STATUS_CODE;

// Private functions
static DISABLE_VMWARE_CUSTIOMIZATION_FLAG_STATUS_CODE
GetDisableVMwareCustomizationFlagStatus(const char* cloudInitConfigFilePath);
static int
FilterCfgExt(const struct dirent *dir);

/**
 *----------------------------------------------------------------------------
 *
 * IsCloudInitCustomizationEnabled
 *
 * Function to determine if cloud-init customization workflow is enabled.
 * Essentially it does
 *  - Read all cloud-init configuration files under /etc/cloud/cloud.cfg.d/
 *  - Read the cloud-init configuration file /etc/cloud/cloud.cfg
 *  - Find if a particular flag is enabled or disabled
 *  - Particularly, the value of flag in files under /etc/cloud/cloud.cfg.d/
 *    has higher priority than the one in file /etc/cloud/cloud.cfg, and the
 *    value of flag in file listed behind in alphabetical sort under
 *    /etc/cloud/cloud.cfg.d/ has higher priority than the one in file listed
 *    in front
 *
 * @returns TRUE if value of the flag 'disable_vmware_customization' is false
 *          FALSE otherwise
 *
 *----------------------------------------------------------------------------
 **/
bool
IsCloudInitCustomizationEnabled()
{
   DISABLE_VMWARE_CUSTIOMIZATION_FLAG_STATUS_CODE flagStatus =
      DISABLE_VMWARE_CUSTOMIZATION_FLAG_UNSET;
   static const char cloudInitBaseConfigFilePath[] = "/etc/cloud/cloud.cfg";
   static const char cloudInitConfigDirPath[] = "/etc/cloud/cloud.cfg.d/";
   struct dirent **fileList;
   int i, fileCount;
   size_t filePathLength;
   char *filePath = NULL;

   sLog(log_info, "Checking if cloud-init customization is enabled.");
   fileCount =
      scandir(cloudInitConfigDirPath, &fileList, FilterCfgExt, alphasort);
   if (fileCount < 0) {
      sLog(log_warning, "Could not scan directory %s, error: %s.",
         cloudInitConfigDirPath, strerror(errno));
   } else {
      for (i = fileCount - 1; i >= 0; i--) {
         filePathLength = Str_Strlen(cloudInitConfigDirPath, PATH_MAX) +
            Str_Strlen(fileList[i]->d_name, FILENAME_MAX) + 1;
         filePath = malloc(filePathLength);
         if (filePath == NULL) {
            sLog(log_warning, "Error allocating memory to copy '%s'.",
               cloudInitConfigDirPath);
            break;
         }
         Str_Strcpy(filePath, cloudInitConfigDirPath, filePathLength);
         Str_Strcat(filePath, fileList[i]->d_name, filePathLength);
         flagStatus = GetDisableVMwareCustomizationFlagStatus(filePath);
         free(filePath);
         filePath = NULL;
         if (flagStatus != DISABLE_VMWARE_CUSTOMIZATION_FLAG_UNSET) {
            break;
         }
      }
      for (i = 0; i < fileCount; i++) {
         free(fileList[i]);
      }
   }
   free(fileList);

   if (flagStatus == DISABLE_VMWARE_CUSTOMIZATION_FLAG_UNSET) {
      flagStatus =
         GetDisableVMwareCustomizationFlagStatus(cloudInitBaseConfigFilePath);
   }

   return (flagStatus == DISABLE_VMWARE_CUSTOMIZATION_FLAG_SET_FALSE);
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
      sLog(log_warning, "Could not open directory %s: error: %s.", dirPath,
           strerror(errno));
      return scriptName;
   }

   regRet = regcomp(&scriptRegex, customScriptRegex, 0);
   if (regRet != 0) {
      char buf[256];

      regerror(regRet, &scriptRegex, buf, sizeof(buf));
      sLog(log_error, "Error compiling regex for custom script: %s.", buf);
      goto done;
   }

   while ((dir = readdir(tempDir)) != NULL) {
      if (regexec(&scriptRegex, dir->d_name, 0, NULL, 0) == 0) {
         scriptName = strdup(dir->d_name);
         if (scriptName == NULL) {
            sLog(log_warning, "Could not allocate memory for scriptName: %s.",
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

/**
 *----------------------------------------------------------------------------
 *
 * GetDisableVMwareCustomizationFlagStatus
 *
 * Function to get status code of the flag 'disable_vmware_customization' from
 * a cloud-init config file.
 * Essentially it does
 *  - Read a cloud-init config file
 *  - Get status code of the flag according to its value
 *
 * @param   [IN]   cloudInitConfigFilePath   path of a cloud-int config file
 * @returns The status code of this particular flag
 *
 *----------------------------------------------------------------------------
 **/
static DISABLE_VMWARE_CUSTIOMIZATION_FLAG_STATUS_CODE
GetDisableVMwareCustomizationFlagStatus(const char* cloudInitConfigFilePath)
{
   DISABLE_VMWARE_CUSTIOMIZATION_FLAG_STATUS_CODE flagStatus =
      DISABLE_VMWARE_CUSTOMIZATION_FLAG_UNSET;
   FILE *cloudInitConfigFile;
   char line[256];
   regex_t regex;
   size_t maxGroups = 2, flagValueLength = 0;
   regmatch_t groupArray[maxGroups];
   const char *flagPattern =
      "^\\s*disable_vmware_customization\\s*:\\s*(true|false)\\s*$";
   int reti;

   cloudInitConfigFile = fopen(cloudInitConfigFilePath, "r");
   if (cloudInitConfigFile == NULL) {
      sLog(log_warning, "Could not open file: %s.", strerror(errno));
      return flagStatus;
   }

   reti = regcomp(&regex, flagPattern, REG_EXTENDED);
   if (reti != 0) {
      char buf[256];
      regerror(reti, &regex, buf, sizeof(buf));
      sLog(log_error, "Error compiling regex for cloud-init flag: %s.", buf);
      goto done;
   }

   while (fgets(line, sizeof(line), cloudInitConfigFile) != NULL) {
      if (regexec(&regex, line, maxGroups, groupArray, 0) == 0) {
         flagValueLength = groupArray[1].rm_eo - groupArray[1].rm_so;
         if (flagValueLength > 0) {
            char flagValue[flagValueLength + 1];
            Str_Strncpy(flagValue, flagValueLength + 1,
               line + groupArray[1].rm_so, flagValueLength);
            sLog(log_info,
               "Flag 'disable_vmware_customization' set in %s with value: %s.",
               cloudInitConfigFilePath, flagValue);
            if (Str_Strequal(flagValue, "false")) {
               flagStatus = DISABLE_VMWARE_CUSTOMIZATION_FLAG_SET_FALSE;
            } else if (Str_Strequal(flagValue, "true")) {
               flagStatus = DISABLE_VMWARE_CUSTOMIZATION_FLAG_SET_TRUE;
            }
         }
      }
   }
   if (ferror(cloudInitConfigFile) != 0) {
      sLog(log_warning, "Error reading file: %s.", strerror(errno));
      flagStatus = DISABLE_VMWARE_CUSTOMIZATION_FLAG_UNSET;
   }
   regfree(&regex);

done:
   fclose(cloudInitConfigFile);
   return flagStatus;
}

/**
 *-----------------------------------------------------------------------------
 *
 * FilterCfgExt
 *
 * Filter files with .cfg extension when calling scandir.
 *
 * @param   [IN]   dir   struct dirent of a directory entry
 * @returns 1 if dir is a regular file and its file extension is .cfg
 *          0 otherwise
 *
 * ----------------------------------------------------------------------------
 **/
static int
FilterCfgExt(const struct dirent *dir)
{
   if (!dir)
      return 0;

   if (dir->d_type == DT_REG) {
      const char *ext = Str_Strrchr(dir->d_name, '.');
      if ((!ext) || (ext == dir->d_name)) {
         return 0;
      } else if (Str_Strequal(ext, ".cfg")) {
         return 1;
      }
   }

   return 0;
}
