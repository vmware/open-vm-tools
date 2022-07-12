/*********************************************************
 * Copyright (C) 2006-2022 VMware, Inc. All rights reserved.
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

#include <ctype.h>
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

#ifndef STATE_FILE_PATH_BASENAME
#define STATE_FILE_PATH_BASENAME "/var/log/.vmware-deploy"
#endif

#ifndef CABCOMMANDLOG
#define CABCOMMANDLOG "/var/log/vmware-imc/toolsDeployPkg.log"
#endif

#define MAXSTRING 2048

// the minimum version that cloud-init support raw data
#define CLOUDINIT_SUPPORT_RAW_DATA_MAJOR_VERSION 21
#define CLOUDINIT_SUPPORT_RAW_DATA_MINOR_VERSION 1

// the maximum length of cloud-init version stdout
#define MAX_LENGTH_CLOUDINIT_VERSION 256

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
static const char* VARRUNIMCDIR    = "/var/run/vmware-imc";
#endif
static const char* TMPDIR          = "/tmp";

// Possible return codes from perl script
static const int CUST_SUCCESS       = 0;
static const int CUST_GENERIC_ERROR = 255;
static const int CUST_NETWORK_ERROR = 254;
static const int CUST_NIC_ERROR     = 253;
static const int CUST_DNS_ERROR     = 252;
static const int CUST_SCRIPT_DISABLED_ERROR = 6;

// the error code to use cloudinit workflow
typedef enum USE_CLOUDINIT_ERROR_CODE {
   USE_CLOUDINIT_OK = 0,
   USE_CLOUDINIT_INTERNAL_ERROR,
   USE_CLOUDINIT_WRONG_VERSION,
   USE_CLOUDINIT_NOT_INSTALLED,
   USE_CLOUDINIT_DISABLED,
   USE_CLOUDINIT_NO_CUST_CFG,
   USE_CLOUDINIT_IGNORE,
} USE_CLOUDINIT_ERROR_CODE;

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
int ForkExecAndWaitCommand(const char* command,
                           bool failIfStdErr,
                           char* forkOutput,
                           int maxOutputLen);
static void SetDeployError(const char* format, ...);
static const char* GetDeployError(void);
static void NoLogging(int level, const char* fmtstr, ...);
static Bool CheckFileExist(const char* dirPath, const char* fileName);
static Bool CopyFileIfExist(const char* sourcePath,
                            const char* targetPath,
                            const char* fileName);
static void GetCloudinitVersion(const char* versionOutput,
                                int* major,
                                int* minor);
static Bool IsTelinitASoftlinkToSystemctl(void);

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
      sLog(log_debug, "Process timeout value from deployment launcher: %u.",
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
   char *tmp = malloc(MAXSTRING);

   if (tmp != NULL) {
      va_list args;
      va_start(args, fmtstr);
      Str_Vsnprintf(tmp, MAXSTRING, fmtstr, args);
      va_end(args);

      sLog(log_error, "Panic callback invoked: '%s'.", tmp);

      free(tmp);
   } else {
      sLog(log_error, "Error allocating memory to log panic messages");
   }

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
   char *tmp = malloc(MAXSTRING);

   if (tmp != NULL) {
      va_list args;
      va_start(args, fmtstr);
      Str_Vsnprintf(tmp, MAXSTRING, fmtstr, args);
      va_end(args);

      sLog(log_debug, "Debug callback invoked: '%s'.", tmp);

      free(tmp);
   } else {
      sLog(log_error, "Error allocating memory to log debug messages");
   }
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

   if (errMsg != NULL) {
      int msg_size = strlen(CABCOMMANDLOG) + 1 + strlen(errMsg) + 1;
      msg = malloc(msg_size);
      if (msg == NULL) {
         sLog(log_error,
              "Error allocating memory to copy '%s' and '%s'.",
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
              "Error allocating memory to copy '%s'.",
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
   free(msg);

   if (vmxResponse != NULL) {
      if (response != NULL) {
         sLog(log_debug, "Got VMX response '%s'.", response);
         if (responseLength > responseBufferSize - 1) {
            sLog(log_warning,
                 "The VMX response is too long (only %d chars are allowed).",
                 responseBufferSize - 1);
            responseLength = responseBufferSize - 1;
         }
         memcpy(vmxResponse, response, responseLength);
         free(response);
      }
      else {
         sLog(log_debug, "Got no VMX response.");
         responseLength = 0;
      }
      vmxResponse[responseLength] = 0;
      if (vmxResponseLength != NULL) {
         *vmxResponseLength = responseLength;
      }
   }

   if (!success) {
      sLog(log_error, "Unable to set customization status in vmx.");
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
 *    Set the deployment error in a verbose style. Can be queried using
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
   char* tmp = malloc(MAXSTRING);

   if (tmp != NULL) {
      va_list args;
      va_start(args, format);
      Str_Vsnprintf(tmp, MAXSTRING, format, args);
      va_end(args);
   }

   if (gDeployError != NULL) {
      free(gDeployError);
      gDeployError = NULL;
   }

   sLog(log_debug, "Setting deploy error: '%s'.", tmp);
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
   sLog(log_debug, "Adding to list '%s'.", token);
#endif
   data = malloc(strlen(token) + 1);
   if (data == NULL) {
      SetDeployError("Error allocating memory. (%s)", strerror(errno));
      return NULL;
   }

   strcpy(data, token);

   l = malloc(sizeof(struct List));
   if (l == NULL) {
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

   if (tail != NULL) {
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
   sLog(log_debug, "Query: List size is %i.", sz);
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
   sLog(log_debug, "Cleaning the linked list.");
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
   sLog(log_info, "Cleaning old state files.");
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
   if (*command == NULL) {
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
   if (hdr.pkgProcessTimeout > 0) {
      if (!gProcessTimeoutSetByLauncher) {
          sLog(log_info, "Process timeout value %u in header will be used.",
             hdr.pkgProcessTimeout);
          gProcessTimeout = hdr.pkgProcessTimeout;
      } else {
          sLog(log_info, "Process timeout value %u in header is ignored.",
             hdr.pkgProcessTimeout);
      }
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
Touch(const char* state)
{
   int fileNameSize = strlen(STATE_FILE_PATH_BASENAME) + 1 /* For '.' */ +
                      strlen(state) + 1;
   char* fileName = malloc(fileNameSize);
   int fd;

   sLog(log_info, "ENTER STATE '%s'.", state);
   if (fileName == NULL) {
      SetDeployError("Error allocating memory.");
      return DEPLOYPKG_STATUS_ERROR;
   }

   Str_Snprintf(fileName, fileNameSize, "%s.%s", STATE_FILE_PATH_BASENAME,
                state);

   fd = open(fileName, O_WRONLY|O_CREAT|O_EXCL, 0644);

   if (fd < 0) {
      SetDeployError("Error creating lock file '%s'.(%s)", fileName,
                     strerror(errno));
      free(fileName);
      return DEPLOYPKG_STATUS_ERROR;
   }

   close(fd);
   free(fileName);

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
   int fileNameSize = strlen(STATE_FILE_PATH_BASENAME) + 1 /* For '.' */ +
                      strlen(state) + 1;
   char* fileName = malloc(fileNameSize);
   int result;

   sLog(log_info, "EXIT STATE '%s'.", state);
   if (fileName == NULL) {
      SetDeployError("Error allocating memory.");
      return DEPLOYPKG_STATUS_ERROR;
   }

   Str_Snprintf(fileName, fileNameSize, "%s.%s", STATE_FILE_PATH_BASENAME,
                state);

   result = remove(fileName);

   if (result < 0) {
      SetDeployError("Error removing lock '%s'.(%s)", fileName,
                     strerror(errno));
      free(fileName);
      return DEPLOYPKG_STATUS_ERROR;
   }

   free(fileName);
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
   sLog(log_info, "Transitioning from state '%s' to state '%s'.",
        stateFrom, stateTo);

   // Create a file to indicate state to
   if (stateTo != NULL) {
      if (Touch(stateTo) == DEPLOYPKG_STATUS_ERROR) {
         SetDeployError("Error creating new state '%s'.(%s)", stateTo, GetDeployError());
         return DEPLOYPKG_STATUS_ERROR;
      }
   }

   // Remove the old state file
   if (stateFrom != NULL) {
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

static char *
GetNicsToEnable(const char *dir)
{
   /*
    * The file nics.txt will list ordinal number of all nics to enable separated
    * by a ",". In current architecture we can have max 4 nics. So we just have
    * to read maximum of 7 characters. This code uses 1024 chars to make sure
    * any future needs are accomodated.
    */
   static const unsigned int NICS_SIZE = 1024;
   static const char *nicFile = "/nics.txt";

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
         SetDeployError("Error allocating memory to read nic file '%s'",
                        fileName);
         fclose(file);
         free(fileName);
         return ret;
      }
      if (fgets(ret, NICS_SIZE, file) == NULL) {
         sLog(log_warning, "fgets() failed or reached EOF.");
      }

      // Check various error condition
      if (ferror(file)) {
         SetDeployError("Error reading nic file '%s'.(%s)", fileName,
                        strerror(errno));
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
           "Trying to connect network interfaces, attempt %d.",
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
         sLog(log_warning, "VMX doesn't support NICs connection status query.");
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
                 "The network interfaces are connected on %d second.",
                 (attempt * enableNicsWaitCount + count) *
                 enableNicsWaitSeconds);
            return;
         }

         sleep(enableNicsWaitSeconds);
      }
   }

   sLog(log_error,
        "Can't connect network interfaces after %d attempts, giving up.",
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
   sLog(log_info, "Creating temp directory '%s' to copy customization files.",
        cloudInitTmpDirPath);
   snprintf(command, sizeof(command),
            "/bin/mkdir -p %s", cloudInitTmpDirPath);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command, true, NULL, 0);
   if (forkExecResult != 0) {
      SetDeployError("Error creating '%s' dir.(%s)",
                     cloudInitTmpDirPath,
                     strerror(errno));
      goto done;
   }

   cloudInitTmpDirCreated = TRUE;

   // Copy required files for cloud-init to a temp name initially and then
   // rename in order to avoid race conditions with partial writes.
   // Regarding to metadata and userdata, we don't parse cust.cfg to check
   // if they are mandatory. That is done by cloud-init.
   if (!CopyFileIfExist(imcDirPath, cloudInitTmpDirPath, "nics.txt")) {
      goto done;
   }

   if (!CopyFileIfExist(imcDirPath, cloudInitTmpDirPath, "metadata")) {
      goto done;
   }

   if (!CopyFileIfExist(imcDirPath, cloudInitTmpDirPath, "userdata")) {
      goto done;
   }

   // Get custom script name.
   customScriptName = GetCustomScript(imcDirPath);
   if (customScriptName != NULL) {
      char scriptPath[1024];

      sLog(log_info, "Custom script present.");
      sLog(log_info, "Copying script to execute post customization.");
      snprintf(scriptPath, sizeof(scriptPath), "%s/scripts", imcDirPath);
      scriptPath[sizeof(scriptPath) - 1] = '\0';
      if (!CopyFileToDirectory(scriptPath, cloudInitTmpDirPath,
                               "post-customize-guest.sh")) {
         goto done;
      }

      sLog(log_info, "Copying user uploaded custom script '%s'.",
           customScriptName);
      if (!CopyFileToDirectory(imcDirPath, cloudInitTmpDirPath,
                               customScriptName)) {
         goto done;
      }
   }

   sLog(log_info, "Copying main configuration file cust.cfg.");
   if (!CopyFileToDirectory(imcDirPath, cloudInitTmpDirPath, "cust.cfg")) {
      goto done;
   }

   deployPkgStatus = DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED;

done:
   free(customScriptName);
   if (DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED == deployPkgStatus) {
      sLog(log_info, "Deployment for cloud-init succeeded.");
      TransitionState(INPROGRESS, DONE);
   } else {
      sLog(log_error, "Deployment for cloud-init failed.");
      if (cloudInitTmpDirCreated) {
         sLog(log_info, "Removing temporary folder '%s'.", cloudInitTmpDirPath);
         snprintf(command, sizeof(command),
                  "/bin/rm -rf %s",
                  cloudInitTmpDirPath);
         command[sizeof(command) - 1] = '\0';
         if (ForkExecAndWaitCommand(command, true, NULL, 0) != 0) {
            sLog(log_warning,
                 "Error while removing temporary folder '%s'. (%s)",
                 cloudInitTmpDirPath, strerror(errno));
         }
      }
      sLog(log_error, "Setting generic error status in vmx.");
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
   forkExecResult = ForkExecAndWaitCommand(command, true, NULL, 0);
   if (forkExecResult != 0) {
      SetDeployError("Error while copying file '%s'.(%s)", fileName,
                     strerror(errno));
      return false;
   }
   snprintf(command, sizeof(command), "/bin/mv -f %s/%s.tmp %s/%s", destPath,
            fileName, destPath, fileName);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command, true, NULL, 0);
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
 * @returns the error code to use cloud-init work flow
 *
 *----------------------------------------------------------------------------
 * */

static USE_CLOUDINIT_ERROR_CODE
UseCloudInitWorkflow(const char* dirPath)
{
   static const char cfgName[] = "cust.cfg";
   static const char metadataName[] = "metadata";
   static const char cloudInitConfigFilePath[] = "/etc/cloud/cloud.cfg";
   static const char cloudInitCommand[] = "/usr/bin/cloud-init -v";
   char cloudInitCommandOutput[MAX_LENGTH_CLOUDINIT_VERSION];
   int forkExecResult;

   if (NULL == dirPath) {
      return USE_CLOUDINIT_INTERNAL_ERROR;
   }

   // check if cust.cfg file exists
   if (!CheckFileExist(dirPath, cfgName)) {
      return USE_CLOUDINIT_NO_CUST_CFG;
   }

   forkExecResult = ForkExecAndWaitCommand(cloudInitCommand,
                                           false,
                                           cloudInitCommandOutput,
                                           sizeof(cloudInitCommandOutput));
   if (forkExecResult != 0) {
      sLog(log_info, "cloud-init is not installed.");
      return USE_CLOUDINIT_NOT_INSTALLED;
   } else {
      sLog(log_info, "cloud-init is installed.");
   }

   // If cloud-init metadata exists, check if cloud-init support to handle
   // cloud-init raw data.
   // In this case, the guest customization must be delegated to cloud-init,
   // no need to check if cloud-init is enabled in cloud.cfg.
   if (CheckFileExist(dirPath, metadataName)) {
      int major, minor;
      GetCloudinitVersion(cloudInitCommandOutput, &major, &minor);
      sLog(log_info, "metadata exists, check cloud-init version...");
      if (major < CLOUDINIT_SUPPORT_RAW_DATA_MAJOR_VERSION ||
          (major == CLOUDINIT_SUPPORT_RAW_DATA_MAJOR_VERSION &&
           minor < CLOUDINIT_SUPPORT_RAW_DATA_MINOR_VERSION)) {
          sLog(log_info,
               "cloud-init version %d.%d is older than required version %d.%d",
               major,
               minor,
               CLOUDINIT_SUPPORT_RAW_DATA_MAJOR_VERSION,
               CLOUDINIT_SUPPORT_RAW_DATA_MINOR_VERSION);
          return USE_CLOUDINIT_WRONG_VERSION;
      } else {
         return USE_CLOUDINIT_OK;
      }
   } else {
      if (IsCloudInitEnabled(cloudInitConfigFilePath)) {
         return USE_CLOUDINIT_OK;
      } else {
         return USE_CLOUDINIT_DISABLED;
      }
   }
}


/**
 *
 * Function which cleans up the deployment directory imcDirPath.
 * This function is called when customization deployment is completed or
 * any unexpected error happens before deployment begins.
 *
 * @param   [IN] imcDirPath  The deployment directory.
 * @returns true if cleaning up succeeds, false if cleaning up fails.
 *
 **/
static bool
DeleteTempDeploymentDirectory(const char* imcDirPath)
{
   int cleanupCommandSize;
   char* cleanupCommand;

   cleanupCommandSize = strlen(CLEANUPCMD) + strlen(imcDirPath) + 1;
   cleanupCommand = malloc(cleanupCommandSize);
   if (cleanupCommand == NULL) {
      SetDeployError("Error allocating memory."
                     "Failed to clean up imc directory '%s'", imcDirPath);
      return false;
   }

   strcpy(cleanupCommand, CLEANUPCMD);
   Str_Strcat(cleanupCommand, imcDirPath, cleanupCommandSize);

   sLog(log_info, "Launching cleanup.");
   if (ForkExecAndWaitCommand(cleanupCommand, true, NULL, 0) != 0) {
      sLog(log_warning, "Error while cleaning up imc directory '%s'. (%s)",
           imcDirPath, strerror(errno));
      free(cleanupCommand);
      return false;
   }
   free(cleanupCommand);
   return true;
}


/**
 *
 * Core function which takes care of deployment in Linux.
 * Essentially it does
 * - uncabing of the cabinet
 * - execution of the command embedded in the cabinet header
 *
 * @param   [IN]  packageName  Package file to be used for deployment
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
   uint8 archiveType;
   uint8 flags;
   bool forceSkipReboot = false;
   const char *baseDirPath = NULL;
   char *imcDirPath = NULL;
   USE_CLOUDINIT_ERROR_CODE useCloudInitWorkflow = USE_CLOUDINIT_IGNORE;
   int imcDirPathSize = 0;
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
   // PR 2942062, Use /var/run/vmware-imc if the directory exists
   // PR 2127543, Use /var/run or /run but /tmp firstly
   if (File_IsDirectory(VARRUNIMCDIR)) {
      baseDirPath = VARRUNIMCDIR;
   } else if (File_IsDirectory(VARRUNDIR)) {
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
        "Reading cabinet file '%s' and will extract it to '%s'.",
         packageName,
         imcDirPath);

   // Get the command to execute
   if (!GetPackageInfo(packageName, &pkgCommand, &archiveType, &flags)) {
      SetDeployError("Error extracting package header information. (%s)",
                     GetDeployError());
      // Possible errors have been logged inside DeleteTempDeploymentDirectory.
      // So no need to check its return value and log error here.
      DeleteTempDeploymentDirectory(imcDirPath);
      free(imcDirPath);
      return DEPLOYPKG_STATUS_CAB_ERROR;
   }

   sLog(log_info, "Flags in the header: %d.", (int) flags);

   sLog(log_info, "Original deployment command: '%s'.", pkgCommand);
   if (strstr(pkgCommand, IMC_TMP_PATH_VAR) != NULL) {
      command = StrUtil_ReplaceAll(pkgCommand, IMC_TMP_PATH_VAR, imcDirPath);
   } else {
      command = StrUtil_ReplaceAll(pkgCommand, TMP_PATH_VAR, imcDirPath);
   }
   free(pkgCommand);

   sLog(log_info, "Actual deployment command: '%s'.", command);

   if (archiveType == VMWAREDEPLOYPKG_PAYLOAD_TYPE_CAB) {
      if (!ExtractCabPackage(packageName, imcDirPath)) {
         DeleteTempDeploymentDirectory(imcDirPath);
         free(imcDirPath);
         free(command);
         return DEPLOYPKG_STATUS_CAB_ERROR;
      }
   } else if (archiveType == VMWAREDEPLOYPKG_PAYLOAD_TYPE_ZIP) {
      if (!ExtractZipPackage(packageName, imcDirPath)) {
         DeleteTempDeploymentDirectory(imcDirPath);
         free(imcDirPath);
         free(command);
         return DEPLOYPKG_STATUS_CAB_ERROR;
      }
   }

   if (!(flags & VMWAREDEPLOYPKG_HEADER_FLAGS_IGNORE_CLOUD_INIT)) {
      useCloudInitWorkflow = UseCloudInitWorkflow(imcDirPath);
   } else {
      sLog(log_info, "Ignoring cloud-init.");
   }

   sLog(log_info, "UseCloudInitWorkflow return: %d", useCloudInitWorkflow);

   if (useCloudInitWorkflow == USE_CLOUDINIT_OK) {
      sLog(log_info, "Executing cloud-init workflow.");
      sSkipReboot = TRUE;
      free(command);
      deployPkgStatus = CloudInitSetup(imcDirPath);
   } else if (useCloudInitWorkflow == USE_CLOUDINIT_WRONG_VERSION ||
              useCloudInitWorkflow == USE_CLOUDINIT_INTERNAL_ERROR) {
      int errCode = (useCloudInitWorkflow == USE_CLOUDINIT_WRONG_VERSION) ?
         TOOLSDEPLOYPKG_ERROR_CLOUDINIT_NOT_SUPPORT_RAWDATA :
         GUESTCUST_EVENT_CUSTOMIZE_FAILED;
      TransitionState(INPROGRESS, ERRORED);

      SetDeployError("Deployment failed. use cloud-init work flow return: %d",
                     useCloudInitWorkflow);
      sLog(log_error, "Deployment failed. use cloud-init work flow return: %d",
           useCloudInitWorkflow);
      SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                  errCode,
                                  "Deployment failed");
      DeleteTempDeploymentDirectory(imcDirPath);
      free(imcDirPath);
      free(command);
      return DEPLOYPKG_STATUS_ERROR;
   } else {
      sLog(log_info, "Executing traditional GOSC workflow.");
      deploymentResult = ForkExecAndWaitCommand(command, true, NULL, 0);
      free(command);

      if (deploymentResult != CUST_SUCCESS) {
         sLog(log_error, "Customization process returned with error.");
         sLog(log_debug, "Deployment result = %d.", deploymentResult);

         if (deploymentResult == CUST_NETWORK_ERROR ||
             deploymentResult == CUST_NIC_ERROR ||
             deploymentResult == CUST_DNS_ERROR) {
            sLog(log_info, "Setting network error status in vmx.");
            SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                        GUESTCUST_EVENT_NETWORK_SETUP_FAILED,
                                        NULL);
         } else if (deploymentResult == CUST_SCRIPT_DISABLED_ERROR) {
            sLog(log_info,
                 "Setting custom script disabled error status in vmx.");
            SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
               TOOLSDEPLOYPKG_ERROR_CUST_SCRIPT_DISABLED, NULL);
         } else {
            sLog(log_info, "Setting '%s' error status in vmx.",
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
                         "The forked off process returned error code.");
      } else {
         char *nics = GetNicsToEnable(imcDirPath);

         if (nics != NULL) {
            // XXX: Sleep before the last SetCustomizationStatusInVmx
            //      This is a temporary-hack for PR 422790
            sleep(5);
            sLog(log_info, "Wait before set enable-nics stats in vmx.");

            TryToEnableNics(nics);

            free(nics);
         } else {
            sLog(log_info, "No nics to enable.");
         }

         SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_DONE,
                                     TOOLSDEPLOYPKG_ERROR_SUCCESS,
                                     NULL);

         TransitionState(INPROGRESS, DONE);

         deployPkgStatus = DEPLOYPKG_STATUS_SUCCESS;
         sLog(log_info, "Deployment succeeded.");
      }
   }

   if (!DeleteTempDeploymentDirectory(imcDirPath)) {
      free(imcDirPath);
      return DEPLOYPKG_STATUS_ERROR;
   }
   free(imcDirPath);

   if (flags & VMWAREDEPLOYPKG_HEADER_FLAGS_SKIP_REBOOT) {
      forceSkipReboot = true;
   }
   sLog(log_info,
        "sSkipReboot: '%s', forceSkipReboot '%s'.",
        sSkipReboot ? "true" : "false",
        forceSkipReboot ? "true" : "false");
   sSkipReboot |= forceSkipReboot;

   //Reset the guest OS
   if (!sSkipReboot && !deploymentResult) {
      pid_t pid = fork();
      if (pid == -1) {
         sLog(log_error, "Failed to fork: '%s'.", strerror(errno));
      } else if (pid == 0) {
         // We're in the child
         int rebootCommandResult;
         bool isRebooting = false;
         // Retry reboot until telinit 6 succeeds to workaround PR 2716292 where
         // telinit is a soft(symbolic) link to systemctl and it could exit
         // abnormally due to systemd sends SIGTERM
         bool retryReboot = IsTelinitASoftlinkToSystemctl();
         sLog(log_info, "Trigger reboot.");
         // Repeatedly try to reboot to workaround PR 530641 where
         // telinit 6 is overwritten by a telinit 2
         do {
            if (isRebooting) {
               sLog(log_info, "Rebooting.");
            }
            rebootCommandResult =
               ForkExecAndWaitCommand("/sbin/telinit 6", true, NULL, 0);
            isRebooting = (rebootCommandResult == 0) ? true : isRebooting;
            sleep(1);
         } while (rebootCommandResult == 0 || (retryReboot && !isRebooting));
         if (!isRebooting) {
            sLog(log_error,
                 "Failed to reboot, reboot command returned error %d.",
                 rebootCommandResult);
            exit (127);
         } else {
            sLog(log_info, "Reboot has been triggered.");
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

   sLog(log_info, "Extracting package files.");

   // Set log function
   MspackWrapper_SetLogger(sLog);

   // Self check library compatibility
   if ((error = SelfTestMspack()) != LINUXCAB_SUCCESS) {
      SetDeployError("mspack self test failed. (%s)", GetLinuxCabErrorMsg(error));
      return FALSE;
   }

   // check if cab file is set
   if (cabFileName == NULL) {
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
      sLog(log_error, "Failed to open package file '%s' for read. (%s)",
           pkgName, strerror(errno));
      return FALSE;
   }
   if ((zipFd = open(zipName, O_CREAT | O_WRONLY | O_TRUNC, 0700)) < 0) {
      sLog(log_error, "Failed to create temporary zip file '%s'. (%s)", zipName,
           strerror(errno));
      close(pkgFd);
      return FALSE;;
   }
   if (lseek(pkgFd, sizeof(VMwareDeployPkgHdr), 0) == (off_t) -1) {
      sLog(log_error,
           "Failed to set the offset for the package file '%s'. (%s)",
           pkgName, strerror(errno));
      close(pkgFd);
      close(zipFd);
      ret = FALSE;
      goto done;
   }
   while ((rdCount = read(pkgFd, copyBuf, sizeof copyBuf)) > 0) {
      if (write(zipFd, copyBuf, rdCount) < 0) {
         sLog(log_error, "Failed to write the zip file '%s'. (%s)", zipName,
              strerror(errno));
         close(pkgFd);
         close(zipFd);
         ret = FALSE;
         goto done;
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

   sLog(log_info, "unzip output: '%s'.", Process_GetStdout(h));

   // Assume zip failed if it wrote to stderr
   stderr = Process_GetStderr(h);
   if (strlen(stderr) > 0) {
      sLog(log_error, "Package unzip failed: '%s'.", stderr);
      ret = FALSE;
   }

   Process_Destroy(h);
done:
   // Clean up the temporary zip file
   if (remove(zipName) != 0) {
      sLog(log_warning, "Failed to remove the temporary zip file '%s'. (%s)",
           zipName, strerror(errno));
   }

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
   if (args == NULL) {
      SetDeployError("Error allocating memory.");
      // clear resources
      DeleteList(commandTokens);
      return NULL;
   }

   for(l = commandTokens, i = 0; l; l = l->next, i++) {
      char* arg = malloc(strlen(l->data) + 1);
      if (arg == NULL) {
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
      sLog(log_debug, "Arg (address & value) : %p '%s'.", args[i], args[i]);
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
 * @param   [IN]  failIfStdErr  Whether to treat stderr as command failed when
 *                              command's return code is 0.
 * @param   [OUT] forkOutput    Return the command stdout. If stdout is empty,
 *                              return the command stderr.
 * @param   [IN]  maxOutputLen  The maximum length to return from command
 *                              output
 * @return  Return code from the process (or -1)
 *
 **/
int
ForkExecAndWaitCommand(const char* command,
                       bool failIfStdErr,
                       char* forkOutput,
                       int maxOutputLen)
{
   ProcessHandle hp;
   int retval;
   int i;
   char** args = GetFormattedCommandLine(command);
   const char* processStdOut;
   Bool isPerlCommand = (strcmp(args[0], "/usr/bin/perl") == 0) ? true : false;
   Bool isTelinitCommand =
      (strcmp(args[0], "/sbin/telinit") == 0) ? true : false;

   sLog(log_debug, "Command to exec : '%s'.", args[0]);
   Process_Create(&hp, args, sLog);

   // Free args array as Process_Create has its own private copy now.
   for (i = 0; args[i] != NULL; i++) {
      free(args[i]);
   }
   free(args);

   Process_RunToComplete(hp, gProcessTimeout);

   processStdOut = Process_GetStdout(hp);

   if (forkOutput != NULL) {
      // Copy the command stdout. If stdout is empty, copy the command stderr.
      if (strlen(processStdOut) > 0) {
         Str_Strncpy(forkOutput, maxOutputLen, processStdOut,
                     maxOutputLen - 1);
      } else {
         Str_Strncpy(forkOutput, maxOutputLen, Process_GetStderr(hp),
                     maxOutputLen - 1);
      }
   }

   if (isPerlCommand) {
      sLog(log_info, "Customization command output:\n%s\n%s\n%s",
         "=================== Perl script log start =================",
         processStdOut,
         "=================== Perl script log end =================");
   } else {
      sLog(log_info, "Customization command output:\n'%s'.",
         processStdOut);
   }
   retval = Process_GetExitCode(hp);

   if (retval == 0) {
      const char* processStdErr = Process_GetStderr(hp);
      if (strlen(processStdErr) > 0) {
         if (failIfStdErr) {
            // Assume command failed if it wrote to stderr, although exitCode
            // is 0.
            sLog(log_error,
                 "Customization command failed with stderr: '%s'.",
                 processStdErr);
            retval = -1;
         } else {
            // Assume command succeeded if exitCode is 0, although it wrote to
            // stderr. e.g, PR2148977, "cloud-init -v" will return 0
            // even there is output in stderr
            sLog(log_info, "Command succeeded despite of stderr output: '%s'.",
                 processStdErr);
         }
      }
   } else {
      if (isTelinitCommand) {
         sLog(log_info,
              "Telinit command failed with exitcode: %d, stderr: '%s'.",
              retval,
              Process_GetStderr(hp));
      } else {
         sLog(log_error,
              "Customization command failed with exitcode: %d, stderr: '%s'.",
              retval,
              Process_GetStderr(hp));
      }
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

#if !defined(OPEN_VM_TOOLS) && !defined(USERWORLD)
   sLog(log_info, "libDeployPkg.so version: %s (%s)",
        SYSIMAGE_VERSION_EXT_STR, BUILD_NUMBER);
#else
   /*
    * For OPEN_VM_TOOLS and USERWORLD, the vmtoolsd version is logged in
    * function DeployPkgDeployPkgInGuest from
    * services/plugins/deployPkg/deployPkg.c
    */
#endif
   sLog(log_info, "Initializing deployment module.");
   Init();

   sLog(log_info, "Deploying cabinet file '%s'.", file);
   retStatus = Deploy(file);

   if (retStatus != DEPLOYPKG_STATUS_SUCCESS &&
       retStatus != DEPLOYPKG_STATUS_CLOUD_INIT_DELEGATED) {
      sLog(log_error, "Deploy error: '%s'.", GetDeployError());
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
              "Deployment delegated to Cloud-init. Returning success.");
      case DEPLOYPKG_STATUS_SUCCESS:
         retStatus = 0;
         break;
      default:
         retStatus = -1;
         break;
   }

   return retStatus;
}

/**
 *
 * Check if the given file exists or not
 *
 * @param  [IN]  dirPath     The dir path of the given file
 * @param  [IN]  fileName    The file name of the given file
 * @returns  TRUE if file exists.
 *           FALSE if file doesn't exist or an error occured.
 *
 **/
static Bool
CheckFileExist(const char* dirPath, const char* fileName)
{
   Bool ret;
   int fullPathSize = strlen(dirPath) + strlen(fileName) + 2 /* '/' and \0 */;
   char *fullPath = (char *) malloc(fullPathSize);
   if (fullPath == NULL) {
      sLog(log_error, "Failed to allocate memory. (%s)", strerror(errno));
      return FALSE;
   }

   snprintf(fullPath, fullPathSize, "%s/%s", dirPath, fileName);
   ret = File_Exists(fullPath);
   free(fullPath);
   return ret;
}

/**
 *
 * Copy the given file to target directory if it exists
 *
 * @param  [IN]  sourcePath     The dir path to copy the file from
 * @param  [IN]  targetPath     The dir path to copy the file to
 * @param  [IN]  fileName       The file name to copy
 * @returns  TRUE if file is copied or not exist.
 *           FALSE if any error occurs.
 *
 **/
static Bool
CopyFileIfExist(const char* sourcePath,
                const char* targetPath,
                const char* fileName)
{
   sLog(log_info, "Copy file %s/%s to directory %s, return if not exist.",
        sourcePath, fileName, targetPath);

   if (CheckFileExist(sourcePath, fileName)) {
      sLog(log_info, "file %s exists. Copying...", fileName);
      if (!CopyFileToDirectory(sourcePath, targetPath, fileName)) {
         return FALSE;
       }
   } else {
      sLog(log_info, "file %s doesn't exist, skipped.", fileName);
   }
   return TRUE;
}

/**
 *
 * Get the cloudinit version from "cloud-init -v" output.
 *
 * The "cloud-init -v" output is something like:
 *    /usr/bin/cloud-init 20.3-2-g371b392c-0ubuntu1~20.04.1
 *    or
 *    cloud-init 0.7.9
 *
 * @param [IN] version    The output of command "cloud-init -v"
 * @param [OUT] major     The major version of cloud-init
 * @param [OUT] minor     The minor version of cloud-init
 *
 * examples:
 *    /usr/bin/cloud-init 20.3-2-g371b392c-0ubuntu1~20.04.1
 *       major: 20, minor: 3
 *    cloud-init 0.7.9
 *       major: 0, minor: 7
 **/
static void
GetCloudinitVersion(const char* version, int* major, int* minor)
{
   *major = *minor = 0;
   if (version == NULL || strlen(version) == 0) {
      sLog(log_warning, "Invalid cloud-init version.");
      return;
   }
   sLog(log_info, "Parse cloud-init version from :%s", version);

   if (isdigit(version[0])) {
      sscanf(version, "%d%*[-.]%d", major, minor);
   } else {
      sscanf(version, "%*[^0123456789]%d%*[-.]%d", major, minor);
   }
   sLog(log_info, "Cloud-init version major: %d, minor: %d", *major, *minor);
}

/**
 *
 * Check if "telinit" command is a soft(symbolic) link to "systemctl" command
 *
 * The fullpath of "systemctl" command could be:
 *    /bin/systemctl
 *    or
 *    /usr/bin/systemctl
 *
 * @returns TRUE if "telinit" command is a soft link to "systemctl" command
 *          FALSE if "telinit" command is not a soft link to "systemctl" command
 *
 **/
static Bool
IsTelinitASoftlinkToSystemctl(void)
{
   static const char systemctlBinPath[] = "/bin/systemctl";
   static const char readlinkCommand[] = "/bin/readlink /sbin/telinit";
   char readlinkCommandOutput[256];
   int forkExecResult;

   forkExecResult = ForkExecAndWaitCommand(readlinkCommand,
                                           true,
                                           readlinkCommandOutput,
                                           sizeof(readlinkCommandOutput));
   if (forkExecResult != 0) {
      sLog(log_debug, "readlink command result = %d.", forkExecResult);
      return FALSE;
   }

   if (strstr(readlinkCommandOutput, systemctlBinPath) != NULL) {
      sLog(log_debug, "/sbin/telinit is a soft link to systemctl");
      return TRUE;
   } else {
      sLog(log_debug, "/sbin/telinit is not a soft link to systemctl");
   }

   return FALSE;
}
