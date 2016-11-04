/*********************************************************
 * Copyright (C) 2016 VMware, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "linuxDeploymentUtilities.h"

/**
 *----------------------------------------------------------------------------
 *
 * IsCloudInitEnabled --
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
      return isEnabled;
   }

   // Read cloud.cfg file and find expected string.
   cloudFile = fopen(cloudFilePath, "r");
   if (cloudFile == NULL) {
     return isEnabled;
   }
   while(fgets(line, sizeof(line), cloudFile)) {
      if (!regexec(&regex, line, 0, NULL, 0)) {
         isEnabled = true;
         break;
      }
   }
   if (ferror(cloudFile)) {
      isEnabled = false;
   }
   fclose(cloudFile);

   return isEnabled;
}
