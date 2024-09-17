/*********************************************************
 * Copyright (C) 1998-2019, 2023 VMware, Inc. All rights reserved.
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
 * guestApp.c --
 *
 *    Utility functions common to all guest applications
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#include <windows.h>
#include <shlobj.h>
#include "productState.h"
#include "conf.h" // for tools install path regkey
#include "winregistry.h"
#include "windowsUtil.h"
#endif

#include "vmware.h"
#include "vm_version.h"
#include "vm_tools_version.h"
#include "guestApp.h"
#include "backdoor.h"
#include "backdoor_def.h"
#include "conf.h"
#include "rpcout.h"
#include "debug.h"
#include "strutil.h"
#include "str.h"
#include "msg.h"
#include "file.h"
#include "posix.h"
#include "vmware/guestrpc/tclodefs.h"

/*
 * For Netware/Linux/BSD/Solaris, the install path
 * is the hardcoded value below. For Windows, it is
 * determined dynamically in GuestApp_GetInstallPath(),
 * so the empty string here is just for completeness.
 */

#if defined _WIN32
#   define GUESTAPP_TOOLS_INSTALL_PATH ""
#elif defined __APPLE__
#   define GUESTAPP_TOOLS_INSTALL_PATH "/Library/Application Support/VMware Tools"
#else
#   define GUESTAPP_TOOLS_INSTALL_PATH "/usr/local/share/vmware-tools"
#endif

#if defined _WIN32
static char *GuestAppGetOrCreateConfPath(void);
static DWORD GuestAppCreateConfPathSecInfo(PSECURITY_ATTRIBUTES *confPathSecAttrs,
                                           PACL *confPathAcl);
static VOID GuestAppDestroyConfPathSecInfo(PSECURITY_ATTRIBUTES confPathSecAttrs,
                                           PACL confPathAcl);
static DWORD GuestAppCreateConfPathSD(PSECURITY_DESCRIPTOR *confPathSD,
                                      PACL *confPathAcl);
static VOID GuestAppDestroyConfPathSD(PSECURITY_DESCRIPTOR confPathSd,
                                      PACL confPathAcl);
static DWORD GuestAppCreateConfPathAcl(PACL *newAcl);
static VOID GuestAppDestroyConfPathAcl(PACL confPathAcl);
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_GetDefaultScript --
 *
 *    Returns the default power script for the given configuration option.
 *
 * Results:
 *    Script name on success, NULL of the option is not recognized.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

const char *
GuestApp_GetDefaultScript(const char *confName) // IN
{
   const char *value = NULL;
   if (strcmp(confName, CONFNAME_SUSPENDSCRIPT) == 0) {
      value = CONFVAL_SUSPENDSCRIPT_DEFAULT;
   } else if (strcmp(confName, CONFNAME_RESUMESCRIPT) == 0) {
      value = CONFVAL_RESUMESCRIPT_DEFAULT;
   } else if (strcmp(confName, CONFNAME_POWEROFFSCRIPT) == 0) {
      value = CONFVAL_POWEROFFSCRIPT_DEFAULT;
   } else if (strcmp(confName, CONFNAME_POWERONSCRIPT) == 0) {
      value = CONFVAL_POWERONSCRIPT_DEFAULT;
   }
   return value;
}

#if defined _WIN32

/*
 *------------------------------------------------------------------------------
 *
 * GuestApp_GetInstallPathW --
 *
 *    Returns the tools installation path as a UTF-16 encoded string, or NULL on
 *    error. The caller must deallocate the returned string using free.
 *
 * Results:
 *    See above.
 *
 * Side effects:
 *    None.
 *------------------------------------------------------------------------------
 */

LPWSTR
GuestApp_GetInstallPathW(void)
{
   HKEY   key    = NULL;
   LONG   rc;
   DWORD  cbData = 0;
   DWORD  temp   = 0;
   PWCHAR data   = NULL;

   /*
    * We need to access the WOW3264Node Registry node for arm64 windows tools,
    * since the arm64 installer is currently built with x86 emulation mode.
    *
    * TODO: REMOVE the the KEY_WOW64_32KEY once the msi installer is running
    * in native arm64.
    */
   REGSAM samDesired = KEY_READ;
#ifdef TOOLS_ARM64
   samDesired |= KEY_WOW64_32KEY;
#endif
   rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, CONF_VMWARE_TOOLS_REGKEY_W, 0,
                      samDesired, &key);
   if (ERROR_SUCCESS != rc) {
      Debug("%s: Couldn't open key \"%S\".\n", __FUNCTION__,
            CONF_VMWARE_TOOLS_REGKEY_W);
      Debug("%s: RegOpenKeyExW error 0x%x.\n", __FUNCTION__, GetLastError());
      goto exit;
   }

   rc = RegQueryValueExW(key, CONF_VMWARE_TOOLS_INSTPATH_KEY_W, 0, NULL, NULL,
                         &cbData);
   if (ERROR_SUCCESS != rc) {
      Debug("%s: Couldn't get length of value \"%S\".\n", __FUNCTION__,
            CONF_VMWARE_TOOLS_INSTPATH_KEY_W);
      Debug("%s: RegQueryValueExW error 0x%x.\n", __FUNCTION__, GetLastError());
      goto exit;
   }

   /*
    * The data in the registry may not be null terminated. So allocate enough
    * space for one extra WCHAR and use that space to write our own NULL.
    */
   data = (LPWSTR) malloc(cbData + sizeof(WCHAR));
   if (NULL == data) {
      Debug("%s: Couldn't allocate %d bytes.\n", __FUNCTION__, cbData);
      goto exit;
   }

   temp = cbData;
   rc = RegQueryValueExW(key, CONF_VMWARE_TOOLS_INSTPATH_KEY_W, 0, NULL, (LPBYTE) data,
                         &temp);
   if (ERROR_SUCCESS != rc) {
      Debug("%s: Couldn't get data for value \"%S\".\n", __FUNCTION__,
            CONF_VMWARE_TOOLS_INSTPATH_KEY_W);
      Debug("%s: RegQueryValueExW error 0x%x.\n", __FUNCTION__, GetLastError());
      goto exit;
   }

   data[cbData / sizeof(WCHAR)] = L'\0';

exit:
   if (NULL != key) {
      RegCloseKey(key);
   }

   return data;
}

#endif

/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetInstallPath --
 *
 *      Get the tools installation path. The caller is responsible for
 *      freeing the memory allocated for the path.
 *
 * Results:
 *      The path in UTF-8 if successful.
 *      NULL otherwise.
 *
 * Side effects:
 *      Allocates memory.
 *
 *----------------------------------------------------------------------
 */

char *
GuestApp_GetInstallPath(void)
{
   char *pathUtf8 = NULL;

#if defined _WIN32
   size_t pathLen = 0;

   if (WinReg_GetSZ(HKEY_LOCAL_MACHINE,
                    CONF_VMWARE_TOOLS_REGKEY,
                    CONF_VMWARE_TOOLS_INSTPATH_KEY,
                    &pathUtf8) != ERROR_SUCCESS) {
      Warning("%s: Unable to retrieve install path: %s\n",
               __FUNCTION__, Msg_ErrString());
      return NULL;
   }

   /* Strip off the trailing backslash, if present */

   pathLen = strlen(pathUtf8);
   if (pathLen > 0) {
      if (pathUtf8[pathLen - 1] == '\\') {
         pathUtf8[pathLen - 1] = '\0';
      }
   }

#else
   pathUtf8 = Str_Asprintf(NULL, "%s", GUESTAPP_TOOLS_INSTALL_PATH);
#endif

   return pathUtf8;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestApp_GetConfPath --
 *
 *      Get the path to the Tools configuration file.
 *
 *      The return conf path is a dynamically allocated UTF-8 encoded
 *      string that should be freed by the caller.
 *
 *      However, the function will also return NULL if we fail to create
 *      a "VMware/VMware Tools" directory. This can occur if we're not running
 *      as Administrator, which VMwareUser doesn't. But I believe that
 *      VMwareService will always come up before VMwareUser, so by the time
 *      a non-root user process calls this function, the directory exists.
 *
 * Results:
 *      The path in UTF-8, or NULL on failure.
 *
 * Side effects:
 *      Allocates memory.
 *
 *----------------------------------------------------------------------
 */

char *
GuestApp_GetConfPath(void)
{
   static char *confPath = NULL;

   if (confPath == NULL) {
#if defined _WIN32
      confPath = GuestAppGetOrCreateConfPath();
#else
      /* Just call into GuestApp_GetInstallPath. */
      confPath = GuestApp_GetInstallPath();
#endif
   }

   /*
    * Return a copy of the cached confPath.
    */
   return confPath ? Util_SafeStrdup(confPath) : NULL;
}

#if defined _WIN32

/*
 *----------------------------------------------------------------------
 *
 * GuestAppGetOrCreateConfPath --
 *
 *      Get the path to the Tools configuration file.
 *
 *      The return conf path is a dynamically allocated UTF-8 encoded
 *      string that should be freed by the caller.
 *      The directory will be created if it doesn't exist.
 *
 *      However, the function will also return NULL if we fail to create
 *      a "VMware/VMware Tools" directory. This can occur if we're not running
 *      as Administrator, which VMwareUser doesn't. But I believe that
 *      VMwareService will always come up before VMwareUser, so by the time
 *      a non-root user process calls this function, the directory exists.
 *
 * Results:
 *      The path in UTF-8, or NULL on failure.
 *
 * Side effects:
 *      Allocates memory and creates the directory if it does not exist.
 *
 *----------------------------------------------------------------------
 */

static char *
GuestAppGetOrCreateConfPath(void)
{
   PSECURITY_ATTRIBUTES confPathSecAttrs = NULL;
   PACL confPathAcl = NULL;
   char *path;
   char *tmp;
   DWORD status = ERROR_SUCCESS;

   path = W32Util_GetVmwareCommonAppDataFilePath(NULL);
   if (path == NULL) {
      goto exit;
   }

   tmp = Str_SafeAsprintf(NULL, "%s%c%s", path, DIRSEPC, ProductState_GetName());
   free(path);
   path = tmp;

   if (File_Exists(path)) {
      goto exit;
   }

   status = GuestAppCreateConfPathSecInfo(&confPathSecAttrs, &confPathAcl);
   if (ERROR_SUCCESS != status) {
      Warning("%s: Error: Get security info failed %u\n", __FUNCTION__, status);
      goto exit;
   }

   if (!CreateDirectoryA(path, confPathSecAttrs)) {
      status = GetLastError();
      if (ERROR_ALREADY_EXISTS == status) {
         Debug("%s: Error: CreateDirectory path exists %u\n", __FUNCTION__, status);
         status = ERROR_SUCCESS;
      } else {
         Warning("%s: Error: CreateDirectory failed %u\n", __FUNCTION__, status);
         goto exit;
      }
   }

exit:
   GuestAppDestroyConfPathSecInfo(confPathSecAttrs, confPathAcl);

   if (ERROR_SUCCESS != status) {
      free(path);
      path = NULL;
   }

   return path;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppCreateConfPathSecInfo --
 *
 *      Creates the user access security attributes.
 *
 * Results:
 *      ERROR_SUCCESS on success or appropriate failure code.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static DWORD
GuestAppCreateConfPathSecInfo(PSECURITY_ATTRIBUTES *confPathSecAttrs,   // OUT: security attrs
                              PACL *confPathAcl)                        // OUT: acl
{
   PSECURITY_ATTRIBUTES newSA = NULL;
   PSECURITY_DESCRIPTOR newSD = NULL;
   PACL newAcl = NULL;
   DWORD status = ERROR_SUCCESS;

   Debug("%s: entered\n", __FUNCTION__);

   ASSERT(NULL != confPathSecAttrs);
   ASSERT(NULL != confPathAcl);

   /* Initialize a security descriptor. */
   newSA = malloc(sizeof *newSA);
   if (NULL == newSA) {
      status = ERROR_NOT_ENOUGH_MEMORY;
      Warning("%s: Error: malloc failed %u\n", __FUNCTION__, status);
      goto exit;
   }
   memset(newSA, 0, sizeof *newSA);

   status = GuestAppCreateConfPathSD(&newSD, &newAcl);
   if (ERROR_SUCCESS != status) {
      Warning("%s: Error: SD creation failed %u\n", __FUNCTION__, status);
      goto exit;
   }

   newSA->nLength = sizeof *newSA;
   newSA->bInheritHandle = FALSE;
   newSA->lpSecurityDescriptor = newSD;

exit:
   if (ERROR_SUCCESS != status) {
      GuestAppDestroyConfPathSecInfo(newSA, newAcl);
      newSA = NULL;
      newAcl = NULL;
   }
   *confPathSecAttrs = newSA;
   *confPathAcl = newAcl;
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppDestroyConfPathSecInfo --
 *
 *      Destroys the security attributes and ACL.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static VOID
GuestAppDestroyConfPathSecInfo(PSECURITY_ATTRIBUTES confPathSecAttrs,  // IN/OUT: security attrs
                               PACL confPathAcl)                       // IN/OUT: acl
{
   if (NULL != confPathSecAttrs) {
      GuestAppDestroyConfPathSD(confPathSecAttrs->lpSecurityDescriptor,
                                confPathAcl);
      free(confPathSecAttrs);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppCreateConfPathSD --
 *
 *      Creates a security descriptor for the configuration path.
 *
 * Results:
 *      ERROR_SUCCESS on success or appropriate failure code.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static DWORD
GuestAppCreateConfPathSD(PSECURITY_DESCRIPTOR *confPathSD,   // OUT: security
                         PACL *confPathAcl)                  // OUT: acl
{
   DWORD status = ERROR_SUCCESS;
   PACL newConfPathAcl = NULL;
   PSECURITY_DESCRIPTOR newConfPathSD = NULL;

   ASSERT(NULL != confPathSD);
   ASSERT(NULL != confPathAcl);

   status = GuestAppCreateConfPathAcl(&newConfPathAcl);
   if (ERROR_SUCCESS != status) {
      Warning("%s: Error: Get Acl failed %u\n", __FUNCTION__, status);
      goto exit;
   }

   /* Initialize a security descriptor. */
   newConfPathSD = malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
   if (NULL == newConfPathSD) {
      status = ERROR_NOT_ENOUGH_MEMORY;
      Warning("%s: Error: malloc failed %u\n", __FUNCTION__, status);
      goto exit;
   }

    if (!InitializeSecurityDescriptor(newConfPathSD, SECURITY_DESCRIPTOR_REVISION)) {
      status = GetLastError();
      Warning("%s: Error: InitializeSecurityDescriptor failed %u\n", __FUNCTION__, status);
      goto exit;
    }

   /* Add the ACL to the security descriptor. */
   if (!SetSecurityDescriptorDacl(newConfPathSD,
                                  TRUE,
                                  newConfPathAcl,
                                  FALSE)) {
      status = GetLastError();
      Warning("%s: Error: SetSecurityDescriptorDacl failed %u\n", __FUNCTION__, status);
      goto exit;
   }

exit:
   if (ERROR_SUCCESS != status) {
      GuestAppDestroyConfPathSD(newConfPathSD, newConfPathAcl);
      newConfPathSD = NULL;
      newConfPathAcl = NULL;
   }
   *confPathSD = newConfPathSD;
   *confPathAcl = newConfPathAcl;
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppDestroyConfPathSD --
 *
 *      Destroys the security descriptor and access control list.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static VOID
GuestAppDestroyConfPathSD(PSECURITY_DESCRIPTOR confPathSd,   // IN/OUT: security
                          PACL confPathAcl)                  // IN/OUT: acl
{
   free(confPathSd);
   GuestAppDestroyConfPathAcl(confPathAcl);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppCreateConfPathAcl --
 *
 *      Create a new user access ACL.
 *
 * Results:
 *      ERROR_SUCCESS on success or appropriate failure code.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static DWORD
GuestAppCreateConfPathAcl(PACL *confPathAcl) // OUT: ACL
{
   PACL newAcl = NULL;
   PSID systemSID = NULL;
   PSID adminsGrpSID = NULL;
   PSID everyoneSID = NULL;
   SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;
   SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
   DWORD newAclSize;
   DWORD status = ERROR_SUCCESS;

   ASSERT(NULL != confPathAcl);

   /* Create a well-known SID for the Everyone group. */
   if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                                 SECURITY_WORLD_RID,
                                 0, 0, 0, 0, 0, 0, 0,
                                 &everyoneSID)) {
      status = GetLastError();
      Warning("%s: Error: AllocateAndInitializeSid Error %u\n", __FUNCTION__, status);
      goto exit;
   }

   /* Create a well-known SID for the Administrators group. */
   if (!AllocateAndInitializeSid(&SIDAuthNT, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &adminsGrpSID)) {
      status = GetLastError();
      Warning("%s: Error: AllocateAndInitializeSid Error %u\n", __FUNCTION__, status);
      goto exit;
   }

   /* Create a well-known SID for the System user. */
   if (!AllocateAndInitializeSid(&SIDAuthNT, 1,
                                 SECURITY_LOCAL_SYSTEM_RID,
                                 0, 0, 0, 0, 0, 0, 0,
                                 &systemSID)) {
      status = GetLastError();
      Warning("%s: Error: AllocateAndInitializeSid Error %u\n", __FUNCTION__, status);
      goto exit;
   }


   /*
    * Get the size of the new ACL. See the following link for this calculation.
    * http://msdn.microsoft.com/en-us/library/windows/desktop/aa378853(v=vs.85).aspx?
    */
   newAclSize = sizeof *newAcl;
   newAclSize += GetLengthSid(systemSID) + sizeof (ACCESS_ALLOWED_ACE) - sizeof (DWORD);
   newAclSize += GetLengthSid(adminsGrpSID) + sizeof (ACCESS_ALLOWED_ACE) - sizeof (DWORD);
   newAclSize += GetLengthSid(everyoneSID) + sizeof (ACCESS_ALLOWED_ACE) - sizeof (DWORD);

   /* Align newAclSize to a DWORD. */
   newAclSize = (newAclSize + (sizeof (DWORD) - 1)) & 0xfffffffc;

   /* Allocate the new ACL. */
   newAcl = malloc(newAclSize);
   if (NULL == newAcl) {
      status = ERROR_NOT_ENOUGH_MEMORY;
      Warning("%s: Error: malloc Acl failed %u\n", __FUNCTION__, status);
      goto exit;
   }

   /* Initialize the new DACL. */
   if (!InitializeAcl(newAcl, newAclSize, ACL_REVISION)) {
      status = GetLastError();
      Warning("%s: Error: Init Acl failed %u\n", __FUNCTION__, status);
      goto exit;
   }

   /* Add the ACE to the DACL for the sid. Note: no inheritence. */
   if (!AddAccessAllowedAceEx(newAcl,
                              ACL_REVISION,
                              CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
                              GENERIC_READ | GENERIC_EXECUTE,
                              everyoneSID)) {
      status = GetLastError();
      Warning("%s: Error: Add Everyone Ace to Acl failed %u\n", __FUNCTION__, status);
      goto exit;
   }

   /* Add the ACE to the ACL for the sid. Note: no inheritence. */
   if (!AddAccessAllowedAceEx(newAcl,
                              ACL_REVISION,
                              CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
                              GENERIC_ALL,
                              adminsGrpSID)) {
      status = GetLastError();
      Warning("%s: Error: Add admins Grp Ace to Acl failed %u\n", __FUNCTION__, status);
      goto exit;
   }

   /* Add the ACE to the ACL for the sid. Note: no inheritence. */
   if (!AddAccessAllowedAceEx(newAcl,
                              ACL_REVISION,
                              CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
                              GENERIC_ALL,
                              systemSID)) {
      status = GetLastError();
      Warning("%s: Error: Add system Ace to Acl failed %u\n", __FUNCTION__, status);
      goto exit;
   }

exit:
   /* Free the allocated SIDs. */
   if (NULL != everyoneSID) {
      FreeSid(everyoneSID);;
   }

   if (NULL != adminsGrpSID) {
      FreeSid(adminsGrpSID);
   }

   if (NULL != systemSID) {
      FreeSid(systemSID);
   }

   if (ERROR_SUCCESS != status) {
      GuestAppDestroyConfPathAcl(newAcl);
      newAcl = NULL;
   }
   *confPathAcl = newAcl;

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppDestroyConfPathAcl --
 *
 *      Destroys the ACL.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static VOID
GuestAppDestroyConfPathAcl(PACL confPathAcl)     // IN/OUT: security
{
   free(confPathAcl);
}
#endif

