/*********************************************************
 * Copyright (C) 2006-2019 VMware, Inc. All rights reserved.
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
 * linuxDeployment.c --
 *
 *      Implementation of libDeployPkg.so.
 */

#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "str.h"

#include "mspackWrapper.h"
#include "deployPkgFormat.h"
#include "deployPkg/linuxDeployment.h"
#include "imgcust-common/process.h"
#include "linuxDeploymentUtilities.h"
#include "mspackWrapper.h"
#include "vmware/guestrpc/deploypkg.h"
#include "vmware/guestrpc/guestcust-events.h"
#include "vmware/tools/guestrpc.h"
#include <file.h>
#include <strutil.h>
#include <util.h>

#include "embed_version.h"
#include "vm_version.h"

#if defined(OPEN_VM_TOOLS) || defined(USERWORLD)
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING "-vmtools");
#else
VM_EMBED_VERSION(SYSIMAGE_VERSION_EXT_STR);
#endif

/*
 * These are covered by #ifndef to give the ability to change these
 * variables from makefile (mostly planned for the forthcoming Solaris
 * customization)
 */

#define CLEANUPCMD  "/bin/rm -r -f "

#ifndef TMP_PATH_VAR
#define TMP_PATH_VAR "/tmp/.vmware/linux/deploy"
#endif

#ifndef IMC_TMP_PATH_VAR
#define IMC_TMP_PATH_VAR "@@IMC_TMP_PATH_VAR@@"
#endif

// Use it to create random name folder for extracting the package
#ifndef IMC_DIR_PATH_PATTERN
#define IMC_DIR_PATH_PATTERN "/.vmware-imgcust-dXXXXXX"
#endif

#ifndef BASEFILENAME
#define BASEFILENAME "/tmp/.vmware-deploy"
#endif

#ifndef CABCOMMANDLOG
#define CABCOMMANDLOG "/var/log/vmware-imc/toolsDeployPkg.log"
#endif

#define MAXSTRING 2048

/*
 * Constant definitions
 */

static const char  SPACECHAR       = ' ';
static const char  TABCHAR         = '\t';
static const char  QUOTECHAR       = '"';
static const char  BACKSLASH       = '\\';
static const char* INPROGRESS      = "INPROGRESS";
static const char* DONE            = "Done";
static const char* ERRORED         = "ERRORED";
#ifndef IMGCUST_UNITTEST
static const char* RUNDIR          = "/run";
static const char* VARRUNDIR       = "/var/run";
#endif
static const char* TMPDIR          = "/tmp";

// Possible return codes from perl script
static const int CUST_SUCCESS       = 0;
static const int CUST_GENERIC_ERROR = 255;
static const int CUST_NETWORK_ERROR = 254;
static const int CUST_NIC_ERROR     = 253;
static const int CUST_DNS_ERROR     = 252;

/*
 * Linked list definition
 *
 *    +---------+---------+
 *    | data    | Next ---+-->
 *    +---------+---------+
 *
 */

struct List {
   char*  data;
   struct List* next;
};

/*
 * Template function definition
 */

// Private functions
static Bool GetPackageInfo(const char* pkgName, char** cmd, uint8* type, uint8* flags);
static Bool ExtractZipPackage(const char* pkg, const char* dest);
static void Init(void);
static struct List* AddToList(struct List* head, const char* token);
static int ListSize(struct List* head);
static DeployPkgStatus Touch(const char*  state);
static DeployPkgStatus UnTouch(const char* state);
static DeployPkgStatus TransitionState(const char* stateFrom, const char* stateTo);
static bool CopyFileToDirectory(const char* srcPath, const char* destPath,
                                const char* fileName);
static DeployPkgStatus Deploy(const char* pkgName);
static char** GetFormattedCommandLine(const char* command);
int ForkExecAndWaitCommand(const char* command, bool ignoreStdErr);
static void SetDeployError(const char* format, ...);
static const char* GetDeployError(void);
static void NoLogging(int level, const char* fmtstr, ...);

/*
 * Globals
 */

static char* gDeployError = NULL;
LogFunction sLog = NoLogging;
static uint16 gProcessTimeout = DEPLOYPKG_PROCESSTIMEOUT_DEFAULT;
static bool gProcessTimeoutSetByLauncher = false;

// .....................................................................................

/*
 *------------------------------------------------------------------------------
 *
 * DeployPkg_SetTimeout --
 *
 *      Give the deploy package an application specific timeout value.
 *      Package deployment engines such as tools-deployPkg-plugin or standalone program
 * linuxDeployPkg can call this API to set gProcessTimeout.
 *      This API should be called before DeployPkg_DeployPackageFromFile or
 * DeployPkg_DeployPackageFromFileEx.
 *      If the package header includes valid 'timeout' value, then that value will be
 * ignored because 'timeout' value has been provided by the package deployment engines.
 *      If no valid 'timeout' value from both package header and deployment engine, then
 * default value 100s will be used.
 *
 * @param logger [in]
 *      timeout value to be used for process execution period control
 *
 *------------------------------------------------------------------------------
 */

void
DeployPkg_SetProcessTimeout(uint16 timeout)
{
   if (timeout > 0) {
      gProcessTimeout = timeout;
      sLog(log_debug, "Process timeout value from deployment launcher: %u.\n",
           gProcessTimeout);
      gProcessTimeoutSetByLauncher = true;
   }
}

// .....................................................................................

/**
 *
 * Panic
 *
 *    Used by VMware libraries pass PANIC signals to the parent application.
 *
 * @param   fmstr    [in]  Format to print the arguments in.
 * @param   args     [in]  Argument list.
 *
 **/
NORETURN void
Panic(const char *fmtstr, ...)
{
   va_list args;

   char *tmp = Util_SafeMalloc(MAXSTRING);

   va_start(args, fmtstr);
   Str_Vsnprintf(tmp, MAXSTRING, fmtstr, args);
   va_end(args);

   sLog(log_error, "Panic callback invoked: '%s'.\n", tmp);

   free(tmp);

   exit(1);
}

// .....................................................................................

/**
 *
 * Debug
 *
 *    Mechanism used by VMware libraries to pass debug messages to the parent.
 *
 * @param   fmstr    [in]  Format to print the arguments in.
 * @param   args     [in]  Argument list.
 *
 **/
void
Debug(const char *fmtstr, ...)
{
#ifdef VMX86_DEBUG
   va_list args;

   char *tmp = Util_SafeMalloc(MAXSTRING);

   va_start(args, fmtstr);
   Str_Vsnprintf(tmp, MAXSTRING, fmtstr, args);
   va_end(args);

   sLog(log_debug, "Debug callback invoked: '%s'.\n", tmp);

   free(tmp);
#endif
}

// .....................................................................................

/**
 *-----------------------------------------------------------------------------
 *
 * SetCustomizationStatusInVmxEx
 *
 *    Set the guest customization status in the VMX server, returning responses.
 *
 * @param   customizzationState  [in]  Customization state of the
 *                                     deployment/customization process
 * @param   errCode              [in]  Error code (can be success too)
 * @param   errMsg               [in]  Error message
 * @param   vmxResponse          [out] VMX response. Note that this is _not_ a
                                       success indicator but contains additional
                                       info target functionality may provide
 * @param   vmxResponseLength    [out] VMX response length
 * @param   responseBufferSize   [in]  VMX response buffer size
 *
 *-----------------------------------------------------------------------------
 **/

static Bool
SetCustomizationStatusInVmxEx(int customizationState,
                              int errCode,
                              const char* errMsg,
                              char   *vmxResponse,
                              size_t *vmxResponseLength,
                              size_t  responseBufferSize)
{
   char   *msg = NULL;
   char   *response = NULL;
   size_t  responseLength = 0;
   Bool success;

   if (errMsg) {
      int msg_size = strlen(CABCOMMANDLOG) + 1 + strlen(errMsg) + 1;
      msg = malloc(msg_size);
      if (msg == NULL) {
         sLog(log_error,
              "Error allocating memory to copy '%s' and '%s'.\n",
              CABCOMMANDLOG,
              errMsg);
         return false;
      }
      strcpy (msg, CABCOMMANDLOG);
      Str_Strcat(msg, "@", msg_size);
      Str_Strcat(msg, errMsg, msg_size);
   } else {
      msg = malloc(strlen(CABCOMMANDLOG) + 1);
      if (msg == NULL) {
         sLog(log_error,
              "Error allocating memory to copy '%s'.\n",
              CABCOMMANDLOG);
         return false;
      }
      strcpy (msg, CABCOMMANDLOG);
   }

   success = RpcChannel_SendOne(vmxResponse != NULL ? &response : NULL,
                                vmxResponse != NULL ? &responseLength : NULL,
                                "deployPkg.update.state %d %d %s",
                                customizationState,
                                errCode,
                                msg);
   free (msg);

   if (vmxResponse != NULL) {
      if (response != NULL) {
         sLog(log_debug, "Got VMX response '%s'.\n", response);
         if (responseLength > responseBufferSize - 1) {
            sLog(log_warning,
                 "The VMX response is too long (only %d chars are allowed).\n",
                 responseBufferSize - 1);
            responseLength = responseBufferSize - 1;
         }
         memcpy(vmxResponse, response, responseLength);
         free(response);
      }
      else {
         sLog(log_debug, "Got no VMX response.\n");
         responseLength = 0;
      }
      vmxResponse[responseLength] = 0;
      if (vmxResponseLength != NULL) {
         *vmxResponseLength = responseLength;
      }
   }

   if (!success) {
      sLog(log_error, "Unable to set customization status in vmx.\n");
   }

   return success;
}


/**
 *-----------------------------------------------------------------------------
 *
 * SetCustomizationStatusInVmx
 *
 *    Set the VMX customization status in the VMX server.
 *
 * @param   customizzationState  [in]  Customization state of the
 *                                     deployment/customization process
 * @param   errCode              [in]  Error code (can be success too)
 * @param   errMsg               [in]  Error message
 *
 *-----------------------------------------------------------------------------
 **/
static void
SetCustomizationStatusInVmx(int customizationState,
                            int errCode,
                            const char* errMsg)
{
   SetCustomizationStatusInVmxEx(customizationState, errCode, errMsg, NULL, NULL, 0);
}

// .....................................................................................

/**
 *
 * DeployPkg_SetLogger
 *
 *    Set the logging function.
 *
 * @param   [in]  log   The logging function to be used
 * @returns None
 *
 **/
void
DeployPkg_SetLogger(LogFunction log)
{
   sLog = log;
}

// .....................................................................................

/**
 *
 * NoLogging
 *
 *    NOP log function.
 *
 * @param   [in]  level    Log level
 * @param   [in]  fmtstr   Format string to format the variables.
 *
 **/
static void
NoLogging(int level, const char* fmtstr, ...)
{
   // No logging
}

// ......................................................................................

/**
 *
 * SetDeployError
 *
 *    Sets the deployment error in a verbose style. Can be queried using
 *    GetDeployError.
 *
 * @param   format   [in]  Format string to follow.
 * @param   ...      [in]  List of params to be formatted.
 *
 **/
static void
SetDeployError(const char* format, ...)
{
   /*
    * No Error check is employed since this is only an advisory service.
    */
   va_list args;

   char* tmp = malloc(MAXSTRING);

   if (tmp) {
      va_start(args, format);
      Str_Vsnprintf(tmp, MAXSTRING, format, args);
      va_end(args);
   }

   if (gDeployError) {
      free(gDeployError);
      gDeployError = NULL;
   }

   sLog(log_debug, "Setting deploy error: '%s'.\n", tmp);
   gDeployError = tmp;
}

// ......................................................................................

/**
 *
 * GetDeployError
 *
 *    Get the deploy error set using the SetDeployError method.
 *
 * @param   None
 * @returns Pointer to the deploy error string.
 *
 **/
static const char*
GetDeployError(void)
{
   return gDeployError;
}

// ......................................................................................

/**
 *
 * AddToList
 *
 *    Add an element to the specified linked list.
 *
 * List organization:
 * ------------------
 *  <<head>>
 *  +----+---+   +----+---+
 *  | D1 | x-+-> | D2 | x-+-> NULL
 *  +----+---+   +----+---+
 *               <<tail>>
 *
 * @param   head  [in]  Head of the linked list.
 * @param   token [in]  Token to be added.
 * @returns The head to the list.
 *
 **/
struct List*
AddToList(struct List* head, const char* token)
{
   struct List* l;
   struct List* tail;
   char* data;

#ifdef VMX86_DEBUG
   sLog(log_debug, "Adding to list '%s'.\n", token);
#endif
   data = malloc(strlen(token) + 1);
   if (!data) {
      SetDeployError("Error allocating memory. (%s)", strerror(errno));
      return NULL;
   }

   strcpy(data, token);

   l = malloc(sizeof(struct List));
   if (!l) {
      SetDeployError("Error allocating memory. (%s)", strerror(errno));
      // clear allocated resource
      free(data);
      return NULL;
   }

   l->data = data;
   l->next = NULL;

   tail = head;
   while (tail && tail->next) {
      tail = tail->next;
   }

   if (tail) {
      tail->next = l;
   }

   return head ? head : l;
}

// ......................................................................................

/**
 *
 * Listsize
 *
 *    Return the size of the specified linked list.
 *
 * @param      head  [in]  Head of the linked list.
 * @returns  The total size of the linked list.
 *
 **/
static int
ListSize(struct List* head)
{
   int sz = 0;
   struct List* l;

   for(l = head; l; ++sz, l = l->next);
#ifdef VMX86_DEBUG
   sLog(log_debug, "Query: List size is %i.\n", sz);
#endif
   return sz;
}

// ......................................................................................

/**
 *
 * DeleteList
 *
 *    Delete the complete list.
 *
 * @param   head  [in]  Head of the list to be deleted.
 * @returns None
 *
 **/
static void
DeleteList(struct List* head)
{
   struct List* t = head;
#ifdef VMX86_DEBUG
   sLog(log_debug, "Cleaning the linked list.\n");
#endif

   while(t) {
      struct List* tmp = t;
      t = t->next;

      // delete resource
      free(tmp->data);
      free(tmp);
   }
}

//......................................................................................

/**
 *
 * Initialize the deployment module.
 *
 * @param   None
 * @return  None
 *
 **/
static void
Init(void)
{
   // Clean up if there is any deployment locks/status before
   sLog(log_info, "Cleaning old state file from tmp directory.\n");
   UnTouch(INPROGRESS);
   UnTouch(DONE);
   UnTouch(ERRORED);

   /*
    * Set the error message as success. This will be replaced with an error
    * message when an error occours. Standard Linux practice.
    */
   SetDeployError("Success.");
}

//......................................................................................

/**
 *
 * Get the command to execute from the cab file.
 *
 * @param   [IN]  packageName   package file name
 * @param   [OUT] command       command line from header
 * @param   [OUT] archiveType   package archive format
 * @param   [OUT] flags         package header flags
 * @return  TRUE is success
 *
 **/
Bool
GetPackageInfo(const char* packageName,
               char** command,
               uint8* archiveType,
               uint8* flags)
{
   unsigned int sz;
   VMwareDeployPkgHdr hdr;
   int fd = open(packageName, O_RDONLY);

   // open errored ?
   if (fd < 0) {
      SetDeployError("Error opening file. (%s)", strerror(errno));
      return FALSE;
   }

   // read the custom header
   sz = read(fd, &hdr, sizeof(VMwareDeployPkgHdr));

   if (sz != sizeof(VMwareDeployPkgHdr)) {
      close(fd);

      SetDeployError("Error reading header. (%s)", strerror(errno));
      return FALSE;
   }

   // close the file
   close(fd);

   // Create space and copy the command
   *command = malloc(VMWAREDEPLOYPKG_CMD_LENGTH);
   if (!*command) {
      SetDeployError("Error allocating memory.");
      return FALSE;
   }

   memcpy(*command, hdr.command, VMWAREDEPLOYPKG_CMD_LENGTH);
   *archiveType = hdr.payloadType;
   *flags = hdr.reserved;

   //TODO hdr->command[VMWAREDEPLOYPKG_CMD_LENGTH - 1] = '\0';

   // Get process timeout value from client
   // If gProcessTimeout has been provided by deployment launcher, then
   // ignore the value from client.
   if (hdr.pkgProcessTimeout > 0 && hdr.pkgProcessTimeout <= MAX_UINT16) {
      if (!gProcessTimeoutSetByLauncher) {
          sLog(log_info, "Process timeout value %u in header will be used.\n",
             hdr.pkgProcessTimeout);
          gProcessTimeout = hdr.pkgProcessTimeout;
      } else {
          sLog(log_info, "Process timeout value %u in header is ignored.\n",
             hdr.pkgProcessTimeout);
      }
   } else if (hdr.pkgProcessTimeout != 0) {
      sLog(log_error, "Invalid process timeout value in header: %d.\n",
             hdr.pkgProcessTimeout);
   }

   return TRUE;
}

//......................................................................................

/**
 *
 * Create a lock file.
 *
 * @param   [IN]  state   The state of the system
 * @returns DEPLOYPKG_STATUS_SUCCESS on success
 *          DEPLOYPKG_STATUS_ERROR on error
 *
 **/
static DeployPkgStatus
Touch(const char*  state)
{
   int fileNameSize = strlen(BASEFILENAME) + 1 + strlen(state) + 1;
   char* fileName = malloc(fileNameSize);
   int fd;

   sLog(log_info, "ENTER STATE '%s'.\n", state);
   if (!fileName) {
      SetDeployError("Error allocatin memory.");
      return DEPLOYPKG_STATUS_ERROR;
   }

   strcpy(fileName, BASEFILENAME);
   Str_Strcat(fileName, ".", fileNameSize);
   Str_Strcat(fileName, state, fileNameSize);

   fd = open(fileName, O_WRONLY|O_CREAT|O_EXCL, 0644);

   if (fd < 0) {
      SetDeployError("Error creating lock file '%s'.(%s)", fileName, strerror(errno));
      free (fileName);
      return DEPLOYPKG_STATUS_ERROR;
   }

   close(fd);
   free (fileName);

   return DEPLOYPKG_STATUS_SUCCESS;
}

//......................................................................................

/**
 *
 * Delete a lock file.
 *
 * @param   [IN]  state   The state of the system
 * @returns DEPLOYPKG_STATUS_SUCCESS on success
 *          DEPLOYPKG_STATUS_ERROR on error
 *
 **/
static DeployPkgStatus
UnTouch(const char* state)
{
   int fileNameSize = strlen(BASEFILENAME) + 1 + strlen(state) + 1;
   char* fileName = malloc(fileNameSize);
   int result;

   sLog(log_info, "EXIT STATE '%s'.\n", state);
   if (!fileName) {
      SetDeployError("Error allocating memory.");
      return DEPLOYPKG_STATUS_ERROR;
   }

   strcpy(fileName, BASEFILENAME);
   Str_Strcat(fileName, ".", fileNameSize);
   Str_Strcat(fileName, state, fileNameSize);

   result = remove(fileName);

   if (result < 0) {
      SetDeployError("Error removing lock '%s'.(%s)", fileName, strerror(errno));
      free (fileName);
      return DEPLOYPKG_STATUS_ERROR;
   }

   free (fileName);
   return DEPLOYPKG_STATUS_SUCCESS;
}

//......................................................................................

/**
 *
 * Depict a transitions from one state to another. the file corresponding to the
 * the old state is deleted and a new file corresponding to the new state is
 * created. This way it is ensured that the tmp directory is left is no status
 * entry at all. The other way to do this is to rename a given. I have opted for
 * deletion and creation to represent the physical transition.
 *
 * @param   [IN] stateFrom  The state from which the transition happens
 * @param   [IN] stateTo    The state to which the transition happens
 * @returns DEPLOYPKG_STATUS_SUCCESS on success
 *          DEPLOYPKG_STATUS_ERROR on error
 *
 **/
static DeployPkgStatus
TransitionState(const char* stateFrom, const char* stateTo)
{
   sLog(log_info, "Transitioning from state '%s' to state '%s'.\n", stateFrom, stateTo);

   // Create a file to indicate state to
   if (stateTo) {
      if (Touch(stateTo) == DEPLOYPKG_STATUS_ERROR) {
         SetDeployError("Error creating new state '%s'.(%s)", stateTo, GetDeployError());
         return DEPLOYPKG_STATUS_ERROR;
      }
   }

   // Remove the old state file
   if (stateFrom) {
      if (UnTouch(stateFrom) == DEPLOYPKG_STATUS_ERROR) {
         SetDeployError("Error deleting old state '%s'.(%s)", stateFrom, GetDeployError());
         return DEPLOYPKG_STATUS_ERROR;
      }
   }

   return DEPLOYPKG_STATUS_SUCCESS;
}

/**
 *-----------------------------------------------------------------------------
 *
 * GetNicsToEnable --
 *
 *      Returns ordinal numbers of nics to enable once customization is done.
 *      Ordinal numbers are read from a file in the deployment package and are
 *      separated by ",". Nics are disabled by VC before customization to avoid
 *      ip conflict on network while this vm is being customized.
 *
 *      This method allocates memory and then reutrns the nics to caller. The
 *      caller should call free to get rid of memory allocated.
 *
 * @param  dir [in] Directory where package files were expanded.
 * @return ordinal number of nics to enable separated by ",". If no nic file is
 *         found or no nics need re-enabling NULL is returned.
 *
 *-----------------------------------------------------------------------------
 */
static char*
GetNicsToEnable(const char* dir)
{
   /*
    * The file nics.txt will list ordinal number of all nics to enable separated by
    * a ",". In current architecture we can have max 4 nics. So we just have to read
    * maximum of 7 characters. This code uses 1024 chars to make sure any future
    * needs are accomodated.
    */
   static const unsigned int NICS_SIZE = 1024;
   static const char* nicFile = "/nics.txt";

   FILE *file;

   char *ret = NULL;
   int fileNameSize = strlen(dir) + strlen(nicFile) + 1;
   char *fileName = malloc(fileNameSize);
   if (fileName == NULL) {
      SetDeployError("Error allocating memory to copy '%s'", dir);
      return ret;
   }
   strcpy(fileName, dir);
   Str_Strcat(fileName, nicFile, fileNameSize);

   file = fopen(fileName, "r");
   if (file) {
      ret = malloc(NICS_SIZE);
      if (ret == NULL) {
         SetDeployError("Error allocating memory to read nic file '%s'", fileName);
         free(fileName);
         return ret;
      }
      if (fgets(ret, NICS_SIZE, file) == NULL) {
         sLog(log_warning, "fgets() failed or reached EOF.\n");
      }

      // Check various error condition
      if (ferror(file)) {
         SetDeployError("Error reading nic file '%s'.(%s)", fileName, strerror(errno));
         free(ret);
         ret = NULL;
      }

      if (!feof(file)) {
         SetDeployError("More than expected nics to enable. Nics: '%s'.", ret);
         free(ret);
         ret = NULL;
      }

      fclose(file);
   }

   free(fileName);
   return ret;
}

/**
 *------------------------------------------------------------------------------
 *
 * TryToEnableNics --
 *
 *      Sends a command to connect network interfaces and waits synchronously
 *      for its completion. If NICs are not connected in predefined time the
 *      command is send again several times.
 *
 *      Note that since guest has no direct visibility to NIC connection status
 *      we rely on VMX to get such info.
 *
 *      Use the enableNicsX constants to fine tune behavior, if needed.
 *
 *      @param nics List of nics that need to be activated.
 *
 *------------------------------------------------------------------------------
 */
static void
TryToEnableNics(const char *nics)
{
   static const int enableNicsRetries = 5;
   static const int enableNicsWaitCount = 5;
   static const int enableNicsWaitSeconds = 1;

   char vmxResponse[64];   // buffer for responses from VMX calls

   int attempt, count;

   for (attempt = 0; attempt < enableNicsRetries; ++attempt) {
      sLog(log_debug,
           "Trying to connect network interfaces, attempt %d.\n",
           attempt + 1);

      if (!SetCustomizationStatusInVmxEx(TOOLSDEPLOYPKG_RUNNING,
                                         GUESTCUST_EVENT_ENABLE_NICS,
                                         nics,
                                         vmxResponse,
                                         NULL,
                                         sizeof(vmxResponse)))
      {
         sleep(enableNicsWaitCount * enableNicsWaitSeconds);
         continue;
      }

      // Note that we are checking for 'query nics' functionality in the loop to
      // protect against potential vMotion during customization process in which
      // case the new VMX could be older, i.e. not that supportive :)
      if (strcmp(vmxResponse, QUERY_NICS_SUPPORTED) != 0) {
         sLog(log_warning, "VMX doesn't support NICs connection status query.\n");
         return;
      }

      for (count = 0; count < enableNicsWaitCount; ++count) {
         // vMotion is unlikely between check for support above and actual call here
         if (SetCustomizationStatusInVmxEx(TOOLSDEPLOYPKG_RUNNING,
                                           GUESTCUST_EVENT_QUERY_NICS,
                                           nics,
                                           vmxResponse,
                                           NULL,
                                           sizeof(vmxResponse)) &&
             strcmp(vmxResponse, NICS_STATUS_CONNECTED) == 0)
         {
            sLog(log_info,
                 "The network interfaces are connected on %d second.\n",
                 (attempt * enableNicsWaitCount + count) *
                 enableNicsWaitSeconds);
            return;
         }

         sleep(enableNicsWaitSeconds);
      }
   }

   sLog(log_error,
        "Can't connect network interfaces after %d attempts, giving up.\n",
        enableNicsRetries);
}

/**
 *-----------------------------------------------------------------------------
 *
 * _DeployPkg_SkipReboot --
 *
 *      Controls skipping the last reboot when customization package is deployed.
 *	XXX This is a UNDOCUMENTED function and is WORKAROUND solution to PR 536688
 *
 *      @param path IN: skip - whether to skip the final reboot after customization
 *
 *-----------------------------------------------------------------------------
 */

static bool sSkipReboot = false;

IMGCUST_API void
_DeployPkg_SkipReboot(bool skip)
{
   sSkipReboot = skip;
}

/**
 *----------------------------------------------------------------------------
 *
 * CloudInitSetup --
 *
 * Function which does the setup for cloud-init if it is enabled.
 * Essentially it copies
 * - nics.tx
 * - cust.cfg to a predefined location.
 *
 * @param   [IN]  imcDirPath Path where nics.txt and cust.cfg exist
 * @returns DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED on success
 *          DEPLOYPKG_STATUS_ERROR on error
 *
 *----------------------------------------------------------------------------
 * */
static DeployPkgStatus
CloudInitSetup(const char *imcDirPath)
{
   DeployPkgStatus deployPkgStatus = DEPLOYPKG_STATUS_ERROR;
   static const char *cloudInitTmpDirPath = "/var/run/vmware-imc";
   int forkExecResult;
   char command[1024];
   Bool cloudInitTmpDirCreated = FALSE;
   char* customScriptName = NULL;
   sLog(log_info, "Creating temp directory '%s' to copy customization files.\n",
        cloudInitTmpDirPath);
   snprintf(command, sizeof(command),
            "/bin/mkdir -p %s", cloudInitTmpDirPath);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command, false);
   if (forkExecResult != 0) {
      SetDeployError("Error creating '%s' dir.(%s)",
                     cloudInitTmpDirPath,
                     strerror(errno));
      goto done;
   }

   cloudInitTmpDirCreated = TRUE;

   // Copy required files for cloud-init to a temp name initially and then
   // rename in order to avoid race conditions with partial writes.
   sLog(log_info, "Check if nics.txt exists. Copy if exists, skip otherwise.\n");
   snprintf(command, sizeof(command),
            "/usr/bin/test -f %s/nics.txt", imcDirPath);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command, false);

   /*
    * /usr/bin/test -f returns 0 if the file exists
    * non zero is returned if the file does not exist.
    * We need to copy the nics.txt only if it exists.
    */
   if (forkExecResult == 0) {
      sLog(log_info, "nics.txt file exists. Copying...\n");
      if (!CopyFileToDirectory(imcDirPath, cloudInitTmpDirPath, "nics.txt")) {
         goto done;
       }
   }

   // Get custom script name.
   customScriptName = GetCustomScript(imcDirPath);
   if (customScriptName != NULL) {
      char scriptPath[1024];

      sLog(log_info, "Custom script present.\n");
      sLog(log_info, "Copying script to execute post customization.\n");
      snprintf(scriptPath, sizeof(scriptPath), "%s/scripts", imcDirPath);
      scriptPath[sizeof(scriptPath) - 1] = '\0';
      if (!CopyFileToDirectory(scriptPath, cloudInitTmpDirPath,
                               "post-customize-guest.sh")) {
         goto done;
      }

      sLog(log_info, "Copying user uploaded custom script '%s'.\n",
           customScriptName);
      if (!CopyFileToDirectory(imcDirPath, cloudInitTmpDirPath,
                               customScriptName)) {
         goto done;
      }
   }

   sLog(log_info, "Copying main configuration file cust.cfg.\n");
   if (!CopyFileToDirectory(imcDirPath, cloudInitTmpDirPath, "cust.cfg")) {
      goto done;
   }

   deployPkgStatus = DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED;

done:
   free(customScriptName);
   if (DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED == deployPkgStatus) {
      sLog(log_info, "Deployment for cloud-init succeeded.\n");
      TransitionState(INPROGRESS, DONE);
   } else {
      sLog(log_error, "Deployment for cloud-init failed.\n");
      if (cloudInitTmpDirCreated) {
         sLog(log_info, "Removing temporary folder '%s'.\n", cloudInitTmpDirPath);
         snprintf(command, sizeof(command),
                  "/bin/rm -rf %s",
                  cloudInitTmpDirPath);
         command[sizeof(command) - 1] = '\0';
         ForkExecAndWaitCommand(command, false);
      }
      sLog(log_error, "Setting generic error status in vmx.\n");
      SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                  GUESTCUST_EVENT_CUSTOMIZE_FAILED,
                                  NULL);
      TransitionState(INPROGRESS, ERRORED);
   }

   return deployPkgStatus;
}


//......................................................................................

static bool
CopyFileToDirectory(const char* srcPath, const char* destPath,
                    const char* fileName)
{
   char command[1024];
   int forkExecResult;
   snprintf(command, sizeof(command), "/bin/cp %s/%s %s/%s.tmp", srcPath,
            fileName, destPath, fileName);
   command[sizeof(command) - 1] = '\0';
   forkExecResult = ForkExecAndWaitCommand(command, false);
   if (forkExecResult != 0) {
      SetDeployError("Error while copying file '%s'.(%s)", fileName,
                     strerror(errno));
      return false;
   }
   snprintf(command, sizeof(command), "/bin/mv -f %s/%s.tmp %s/%s", destPath,
            fileName, destPath, fileName);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command, false);
   if (forkExecResult != 0) {
      SetDeployError("Error while renaming temp file '%s'.(%s)", fileName,
                     strerror(errno));
      return false;
   }
   return true;
}


//......................................................................................

/**
 *----------------------------------------------------------------------------
 *
 * UseCloudInitWorkflow --
 *
 * Function which checks if cloud-init should be used for customization.
 * Essentially it checks if
 * - customization specificaion file (cust.cfg) is present.
 * - cloud-init is installed
 * - cloud-init is enabled.
 *
 * @param   [IN]  dirPath  Path where the package is extracted.
 * @returns true if cloud-init should be used for guest customization.
 *
 *----------------------------------------------------------------------------
 * */

static bool
UseCloudInitWorkflow(const char* dirPath)
{
   char *cfgFullPath = NULL;
   int cfgFullPathSize;
   static const char cfgName[] = "cust.cfg";
   static const char cloudInitConfigFilePath[] = "/etc/cloud/cloud.cfg";
   static const char cloudInitCommand[] = "/usr/bin/cloud-init -v";
   int forkExecResult;

   if (NULL == dirPath) {
      return false;
   }

   sLog(log_debug, "Check if cust.cfg exists.\n");

   cfgFullPathSize = strlen(dirPath) + 1 /* For '/' */ + sizeof(cfgName);
   cfgFullPath = (char *) malloc(cfgFullPathSize);
   if (cfgFullPath == NULL) {
      sLog(log_error, "Failed to allocate memory. (%s)\n", strerror(errno));
      return false;
   }

   snprintf(cfgFullPath, cfgFullPathSize, "%s/%s", dirPath, cfgName);
   cfgFullPath[cfgFullPathSize - 1] = '\0';

   if (access(cfgFullPath, R_OK) != 0) {
      sLog(log_info, "cust.cfg is missing in '%s' directory. Error: (%s)\n",
           dirPath, strerror(errno));
      free(cfgFullPath);
      return false;
   } else {
      sLog(log_info, "cust.cfg is found in '%s' directory.\n", dirPath);
   }

   forkExecResult = ForkExecAndWaitCommand(cloudInitCommand, true);
   if (forkExecResult != 0) {
      sLog(log_info, "cloud-init is not installed.\n");
      free(cfgFullPath);
      return false;
   } else {
      sLog(log_info, "cloud-init is installed.\n");
   }

   free(cfgFullPath);
   return IsCloudInitEnabled(cloudInitConfigFilePath);
}


/**
 *
 * Core function which takes care of deployment in Linux.
 * Essentially it does
 * - uncabing of the cabinet
 * - execution of the command embedded in the cabinet header
 *
 * @param   [IN[  packageName  Package file to be used for deployment
 * @returns DEPLOYPKG_STATUS_SUCCESS on success
 *          DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED if customization task is
 *          delegated to cloud-init.
 *          DEPLOYPKG_STATUS_ERROR on error
 *
 **/
static DeployPkgStatus
Deploy(const char* packageName)
{
   DeployPkgStatus deployPkgStatus = DEPLOYPKG_STATUS_SUCCESS;
   char* pkgCommand = NULL;
   char* command = NULL;
   int deploymentResult = 0;
   char *nics;
   char* cleanupCommand;
   uint8 archiveType;
   uint8 flags;
   bool forceSkipReboot = false;
   const char *baseDirPath = NULL;
   char *imcDirPath = NULL;
   bool useCloudInitWorkflow = false;
   int imcDirPathSize = 0;
   int cleanupCommandSize = 0;
   TransitionState(NULL, INPROGRESS);

   // Notify the vpx of customization in-progress state
   SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                               TOOLSDEPLOYPKG_ERROR_SUCCESS,
                               NULL);

   // Add this macro definition to enable using '/tmp' instead of '/var/run' as
   // the cab file deployment directory in unit test.
#ifdef IMGCUST_UNITTEST
   baseDirPath = TMPDIR;
#else
   // PR 2127543, Use /var/run or /run but /tmp firstly
   if (File_IsDirectory(VARRUNDIR)) {
      baseDirPath = VARRUNDIR;
   } else if (File_IsDirectory(RUNDIR)) {
      baseDirPath = RUNDIR;
   } else {
      baseDirPath = TMPDIR;
   }
#endif

   // Create a random name dir under base dir path
   imcDirPathSize = strlen(baseDirPath) + strlen(IMC_DIR_PATH_PATTERN) + 1;
   imcDirPath = malloc(imcDirPathSize);
   if (imcDirPath == NULL) {
      SetDeployError("Error allocating memory to create imc dir.");
      return DEPLOYPKG_STATUS_ERROR;
   }
   strcpy(imcDirPath, baseDirPath);
   Str_Strcat(imcDirPath, IMC_DIR_PATH_PATTERN, imcDirPathSize);
   if (mkdtemp(imcDirPath) == NULL) {
      free(imcDirPath);
      SetDeployError("Error creating imc dir. (%s)", strerror(errno));
      return DEPLOYPKG_STATUS_ERROR;
   }

   sLog(log_info,
        "Reading cabinet file '%s' and will extract it to '%s'.\n",
         packageName,
         imcDirPath);

   // Get the command to execute
   if (!GetPackageInfo(packageName, &pkgCommand, &archiveType, &flags)) {
      SetDeployError("Error extracting package header information. (%s)",
                     GetDeployError());
      free(imcDirPath);
      return DEPLOYPKG_STATUS_CAB_ERROR;
   }

   sLog(log_info, "Flags in the header: %d.\n", (int) flags);

   sLog(log_info, "Original deployment command: '%s'.\n", pkgCommand);
   if (strstr(pkgCommand, IMC_TMP_PATH_VAR) != NULL) {
      command = StrUtil_ReplaceAll(pkgCommand, IMC_TMP_PATH_VAR, imcDirPath);
   } else {
      command = StrUtil_ReplaceAll(pkgCommand, TMP_PATH_VAR, imcDirPath);
   }
   free(pkgCommand);

   sLog(log_info, "Actual deployment command: '%s'.\n", command);

   if (archiveType == VMWAREDEPLOYPKG_PAYLOAD_TYPE_CAB) {
      if (!ExtractCabPackage(packageName, imcDirPath)) {
         free(imcDirPath);
         free(command);
         return DEPLOYPKG_STATUS_CAB_ERROR;
      }
   } else if (archiveType == VMWAREDEPLOYPKG_PAYLOAD_TYPE_ZIP) {
      if (!ExtractZipPackage(packageName, imcDirPath)) {
         free(imcDirPath);
         free(command);
         return DEPLOYPKG_STATUS_CAB_ERROR;
      }
   }

   if (!(flags & VMWAREDEPLOYPKG_HEADER_FLAGS_IGNORE_CLOUD_INIT)) {
      useCloudInitWorkflow = UseCloudInitWorkflow(imcDirPath);
   } else {
      sLog(log_info, "Ignoring cloud-init.\n");
   }

   if (useCloudInitWorkflow) {
      sLog(log_info, "Executing cloud-init workflow.\n");
      sSkipReboot = TRUE;
      free(command);
      deployPkgStatus = CloudInitSetup(imcDirPath);
   } else {
      sLog(log_info, "Executing traditional GOSC workflow.\n");
      deploymentResult = ForkExecAndWaitCommand(command, false);
      free(command);

      if (deploymentResult != CUST_SUCCESS) {
         sLog(log_error, "Customization process returned with error.\n");
         sLog(log_debug, "Deployment result = %d.\n", deploymentResult);

         if (deploymentResult == CUST_NETWORK_ERROR ||
             deploymentResult == CUST_NIC_ERROR ||
             deploymentResult == CUST_DNS_ERROR) {
            sLog(log_info, "Setting network error status in vmx.\n");
            SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                        GUESTCUST_EVENT_NETWORK_SETUP_FAILED,
                                        NULL);
         } else {
            sLog(log_info, "Setting '%s' error status in vmx.\n",
                 deploymentResult == CUST_GENERIC_ERROR ? "generic" : "unknown");
            SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                        GUESTCUST_EVENT_CUSTOMIZE_FAILED,
                                        NULL);
         }

         TransitionState(INPROGRESS, ERRORED);

         deployPkgStatus = DEPLOYPKG_STATUS_ERROR;
         SetDeployError("Deployment failed."
                        "The forked off process returned error code.");
         sLog(log_error, "Deployment failed."
                         "The forked off process returned error code.\n");
      } else {
         nics = GetNicsToEnable(imcDirPath);
         if (nics) {
            // XXX: Sleep before the last SetCustomizationStatusInVmx
            //      This is a temporary-hack for PR 422790
            sleep(5);
            sLog(log_info, "Wait before set enable-nics stats in vmx.\n");

            TryToEnableNics(nics);

            free(nics);
         } else {
            sLog(log_info, "No nics to enable.\n");
         }

         SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_DONE,
                                     TOOLSDEPLOYPKG_ERROR_SUCCESS,
                                     NULL);

         TransitionState(INPROGRESS, DONE);

         deployPkgStatus = DEPLOYPKG_STATUS_SUCCESS;
         sLog(log_info, "Deployment succeeded.\n");
      }
   }
   cleanupCommandSize = strlen(CLEANUPCMD) + strlen(imcDirPath) + 1;
   cleanupCommand = malloc(cleanupCommandSize);
   if (!cleanupCommand) {
      SetDeployError("Error allocating memory.");
      free(imcDirPath);
      return DEPLOYPKG_STATUS_ERROR;
   }

   strcpy(cleanupCommand, CLEANUPCMD);
   Str_Strcat(cleanupCommand, imcDirPath, cleanupCommandSize);

   sLog(log_info, "Launching cleanup.\n");
   if (ForkExecAndWaitCommand(cleanupCommand, false) != 0) {
      sLog(log_warning, "Error while cleaning up imc directory '%s'. (%s)\n",
           imcDirPath, strerror (errno));
   }
   free (cleanupCommand);
   free(imcDirPath);

   if (flags & VMWAREDEPLOYPKG_HEADER_FLAGS_SKIP_REBOOT) {
      forceSkipReboot = true;
   }
   sLog(log_info,
        "sSkipReboot: '%s', forceSkipReboot '%s'.\n",
        sSkipReboot ? "true" : "false",
        forceSkipReboot ? "true" : "false");
   sSkipReboot |= forceSkipReboot;

   //Reset the guest OS
   if (!sSkipReboot && !deploymentResult) {
      pid_t pid = fork();
      if (pid == -1) {
         sLog(log_error, "Failed to fork: '%s'.\n", strerror(errno));
      } else if (pid == 0) {
         // We're in the child

         // Repeatedly try to reboot to workaround PR 530641 where
         // telinit 6 is overwritten by a telinit 2
         int rebootCommandResult;
         bool isRebooting = false;
         sLog(log_info, "Trigger reboot.\n");
         do {
            if (isRebooting) {
               sLog(log_info, "Rebooting.\n");
            }
            rebootCommandResult =
               ForkExecAndWaitCommand("/sbin/telinit 6", false);
            isRebooting = (rebootCommandResult == 0) ?
			   true : isRebooting;
            sleep(1);
         } while (rebootCommandResult == 0);
         if (!isRebooting) {
            sLog(log_error,
                 "Failed to reboot, telinit returned error %d.\n",
                 rebootCommandResult);
            exit (127);
         } else {
            sLog(log_info, "Reboot has been triggered.\n");
         }
      }
   }

   return deployPkgStatus;
}

/**
 * Extract all files into the destination folder.
 */
Bool
ExtractCabPackage(const char* cabFileName,
                  const char* destDir)
{
   unsigned int error;

   sLog(log_info, "Extracting package files.\n");

   // Set log function
   MspackWrapper_SetLogger(sLog);

   // Self check library compatibility
   if ((error = SelfTestMspack()) != LINUXCAB_SUCCESS) {
      SetDeployError("mspack self test failed. (%s)", GetLinuxCabErrorMsg(error));
      return FALSE;
   }

   // check if cab file is set
   if (!cabFileName) {
      SetDeployError("Cab file not set.");
      return FALSE;
   }

   // uncab the cabinet file
   if ((error = ExpandAllFilesInCab(cabFileName, destDir)) != LINUXCAB_SUCCESS) {
      SetDeployError("Error expanding cabinet. (%s)", GetLinuxCabErrorMsg(error));
      return FALSE;
   }
   return TRUE;
}

/**
 * Extract all files into the destination folder.
 */
static Bool
ExtractZipPackage(const char* pkgName,
                  const char* destDir)
{
   ProcessHandle h;
   char* args[32];
   const char* stderr;

   int pkgFd, zipFd;
   char zipName[1024];
   char copyBuf[4096];
   ssize_t rdCount;
   char* destCopy;

   Bool ret = TRUE;

   // strip the header from the file
   snprintf(zipName, sizeof zipName, "%s/%x", destDir, (unsigned int)time(0));
   zipName[(sizeof zipName) - 1] = '\0';
   if ((pkgFd = open(pkgName, O_RDONLY)) < 0) {
      sLog(log_error, "Failed to open package file '%s' for read. (%s)\n", pkgName,
           strerror(errno));
      return FALSE;
   }
   if ((zipFd = open(zipName, O_CREAT | O_WRONLY | O_TRUNC, 0700)) < 0) {
      sLog(log_error, "Failed to create temporary zip file '%s'. (%s)\n", zipName,
           strerror(errno));
      close(pkgFd);
      return FALSE;;
   }
   lseek(pkgFd, sizeof(VMwareDeployPkgHdr), 0);
   while((rdCount = read(pkgFd, copyBuf, sizeof copyBuf)) > 0) {
      if (write(zipFd, copyBuf, rdCount) < 0) {
         sLog(log_warning, "write() failed.\n");
      }
   }

   close(pkgFd);
   close(zipFd);

   destCopy = strdup(destDir);  // destDir is const
   args[0] = "/usr/bin/unzip";
   args[1] = "-o";
   args[2] = zipName;
   args[3] = "-d";
   args[4] = destCopy;
   args[5] = NULL;
   Process_Create(&h, args, sLog);
   free(destCopy);
   Process_RunToComplete(h, gProcessTimeout);

   sLog(log_info, "unzip output: '%s'.\n", Process_GetStdout(h));

   // Assume zip failed if it wrote to stderr
   stderr = Process_GetStderr(h);
   if (strlen(stderr) > 0) {
      sLog(log_error, "Package unzip failed: '%s'.\n", stderr);
      ret = FALSE;
   }

   Process_Destroy(h);

   return ret;
}

//......................................................................................

/**
 *
 * Coverts the string into array of "C" string with NULL as the last
 * element in the array. This is the way an "exec" in Linux expects its
 * parameters.
 *
 * @param   [IN]  command  Command to execute
 * @return  2-Dimensional array of "C" string or NULL on error
 *
 **/
static char**
GetFormattedCommandLine(const char* command)
{
   // tokenize it into program that is needed to be executed and arguments for it
   struct List* commandTokens = NULL;

   char token[strlen(command) + 1];
   unsigned int wToken = 0; // write head for token

   char** args;
   struct List* l;

   unsigned int i;
   for (i = 0; i < strlen(command); ++i) {
      if (command[i] == BACKSLASH) {// if backslash skip next char
         token[wToken++] = command[i++];
         if (i < (strlen(command) - 1)) {
            token[wToken++] = command[i++];
         }
         continue;
      } else if (command[i] == QUOTECHAR) {// if quote skip till next quote
         token[wToken++] = QUOTECHAR;
         for (++i; command[i] && (command[i] != QUOTECHAR); ++i) {
            token[wToken++] = command[i];
         }
         token[wToken++] = QUOTECHAR;
         continue;
      } else if (command[i] == SPACECHAR || command[i] == TABCHAR) {// tab or space
         token[wToken++] = 0;
         commandTokens = AddToList(commandTokens, token);

         memset(token, 0, strlen(command));
         wToken = 0;

         // seek to the next char position that is not a space or a tab
         for (;command[i] != SPACECHAR && command[i] != TABCHAR; ++i);
      } else {// add it to token
         token[wToken++] = command[i];
      }
   }

   // last token -- check and insert
   /* if (token) { */
      commandTokens = AddToList(commandTokens, token);
   /* } */

   // prefixing the start path for the commands
   args = malloc((ListSize(commandTokens) + 1) * sizeof(char*));
   if (!args) {
      SetDeployError("Error allocating memory.");
      // clear resources
      DeleteList(commandTokens);
      return NULL;
   }

   for(l = commandTokens, i = 0; l; l = l->next, i++) {
      char* arg = malloc(strlen(l->data) + 1);
      if (!arg) {
         unsigned int j;
         SetDeployError("Error allocating memory. (%s)", strerror(errno));
         // free allocated memories in previous iterations if any
         for (j = 0; j < i; j++) {
            free(args[j]);
         }
         free(args);
         // clear resources
         DeleteList(commandTokens);
         return NULL;
      }

      strcpy (arg, l->data);
      args[i] = arg;

#ifdef VMX86_DEBUG
      sLog(log_debug, "Arg (address & value) : %p '%s'.\n", args[i], args[i]);
#endif
   }

   // marks the end of params
   args[ListSize(commandTokens)] = NULL;

   // clear resources
   DeleteList(commandTokens);

   return args;
}

//......................................................................................

/**
 *
 * Fork off the command and wait for it to finish. Classical Linux/Unix
 * fork-and-exec.
 *
 * @param   [IN]  command       Command to execute
 * @param   [IN]  ignoreStdErr  If we ignore stderr when cmd's return code is 0
 * @return  Return code from the process (or -1)
 *
 **/
int
ForkExecAndWaitCommand(const char* command, bool ignoreStdErr)
{
   ProcessHandle hp;
   int retval;
   int i;
   char** args = GetFormattedCommandLine(command);

   sLog(log_debug, "Command to exec : '%s'.\n", args[0]);
   Process_Create(&hp, args, sLog);

   // Free args array as Process_Create has its own private copy now.
   for (i = 0; args[i] != NULL; i++) {
      free(args[i]);
   }
   free(args);

   Process_RunToComplete(hp, gProcessTimeout);
   sLog(log_info, "Customization command output: '%s'.\n", Process_GetStdout(hp));
   retval = Process_GetExitCode(hp);

   if (retval == 0) {
      if (strlen(Process_GetStderr(hp)) > 0) {
         if (!ignoreStdErr) {
            // Assume command failed if it wrote to stderr, even if exitCode is 0
            sLog(log_error,
                 "Customization command failed with stderr: '%s'.\n",
                 Process_GetStderr(hp));
            retval = -1;
         } else {
            // If we choose to ignore stderr, we do not return -1 when return
            // code is 0. e.g, PR2148977, "cloud-init -v" will return 0
            // even there is output in stderr
            sLog(log_info, "Ignoring stderr output: '%s'.\n", Process_GetStderr(hp));
         }
      }
   } else {
      sLog(log_error,
           "Customization command failed with exitcode: %d, stderr: '%s'.\n",
           retval,
           Process_GetStderr(hp));
   }

   Process_Destroy(hp);
   return retval;
}

//.............................................................................

/**
 *
 * Decodes a package from a file, extracts its payload,
 * expands the payload into a temporary directory, and then executes
 * the command specified in the package.
 *
 * @param   [IN]  file  The package file
 * @returns DEPLOYPKG_STATUS_SUCCESS on success
 *          DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED if customization task is
 *          delegated to cloud-init.
 *          DEPLOYPKG_STATUS_ERROR on error
 *
 **/
DeployPkgStatus
DeployPkg_DeployPackageFromFileEx(const char* file)
{
   DeployPkgStatus retStatus;

   sLog(log_info, "Initializing deployment module.\n");
   Init();

   sLog(log_info, "Deploying cabinet file '%s'.\n", file);
   retStatus = Deploy(file);

   if (retStatus != DEPLOYPKG_STATUS_SUCCESS &&
       retStatus != DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED) {
      sLog(log_error, "Deploy error: '%s'.\n", GetDeployError());
   }

   free(gDeployError);
   gDeployError = NULL;

   return retStatus;
}

//.............................................................................

/**
 *
 * Decodes a package from a file, extracts its payload,
 * expands the payload into a temporary directory, and then executes
 * the command specified in the package.
 *
 * @param   [IN]  file  The package file
 * @returns 0 on success and -1 on error
 *
 **/
int
DeployPkg_DeployPackageFromFile(const char* file)
{
   DeployPkgStatus deployPkgStatus = DeployPkg_DeployPackageFromFileEx(file);
   int retStatus;

   switch (deployPkgStatus) {
      case DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED:
         /*
          * The return code from DeployPkg_DeployPackageFromFile should
          * be either 0 (for success) or -1 (for failure).
          * DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED should be treated as
          * success. So fallback to DEPLOYPKG_STATUS_SUCCESS.
          */
         sLog(log_info,
              "Deployment delegated to Cloud-init. Returning success.\n");
      case DEPLOYPKG_STATUS_SUCCESS:
         retStatus = 0;
         break;
      default:
         retStatus = -1;
         break;
   }

   return retStatus;
}
