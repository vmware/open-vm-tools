/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <stdbool.h>

#include "mspackWrapper.h"
#include "deployPkgFormat.h"
#include "deployPkg/linuxDeployment.h"
#include "imgcust-common/process.h"
#include "imgcust-guest/guestcust-events.h"
#include "linuxDeploymentUtilities.h"
#include "mspackWrapper.h"
#include "rpcout.h"
#include "toolsDeployPkg.h"

/*
 * These are covered by #ifndef to give the ability to change these
 * variables from makefile (mostly planned for the forthcoming Solaris
 * customization)
 */

#define CLEANUPCMD  "/bin/rm -r -f "

#ifndef EXTRACTPATH
#define EXTRACTPATH "/tmp/.vmware/linux/deploy"
#endif

#ifndef CLEANUPPATH
#define CLEANUPPATH "/tmp/.vmware"
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

static const char  ENDOFLINEMARKER = '\n';
static const char  SPACECHAR       = ' ';
static const char  TABCHAR         = '\t';
static const char  QUOTECHAR       = '"';
static const char  BACKSLASH       = '\\';
static const char* INPROGRESS      = "INPROGRESS";
static const char* DONE            = "Done";
static const char* ERRORED         = "ERRORED";

// Possible return codes from perl script
static const int CUST_SUCCESS       = 0;
static const int CUST_GENERIC_ERROR = 255;
static const int CUST_NETWORK_ERROR = 254;
static const int CUST_NIC_ERROR     = 253;
static const int CUST_DNS_ERROR     = 252;

// Common return values
static const int  DEPLOY_SUCCESS   = 0;
static const int  DEPLOY_ERROR     = -1;

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
static Bool CreateDir(const char *path);
static void Init(void);
static struct List* AddToList(struct List* head, const char* token);
static int ListSize(struct List* head);
static int Touch(const char*  state);
static int UnTouch(const char* state);
static int TransitionState(const char* stateFrom, const char* stateTo);
static int Deploy(const char* pkgName);
static char** GetFormattedCommandLine(const char* command);
static int ForkExecAndWaitCommand(const char* command);
static void SetDeployError(const char* format, ...);
static const char* GetDeployError(void);
static void NoLogging(int level, const char* fmtstr, ...);

/*
 * Globals
 */

static char* gDeployError = NULL;
static LogFunction sLog = NoLogging;

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
   /* Ignored */
   sLog(log_warning, "Panic callback invoked. \n");
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
Debug(const char *fmtstr,
      va_list args)
{
   /* Ignored */
#ifdef VMX86_DEBUG
   sLog(log_warning, "Debug callback invoked. \n");
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
      msg = malloc(strlen(CABCOMMANDLOG) + 1 + strlen(errMsg) + 1);
      strcpy (msg, CABCOMMANDLOG);
      strcat (msg, "@");
      strcat (msg, errMsg);
   } else {
      msg = malloc(strlen(CABCOMMANDLOG) + 1);
      strcpy (msg, CABCOMMANDLOG);
   }

   success = RpcOut_sendOne(vmxResponse != NULL ? &response : NULL,
                            vmxResponse != NULL ? &responseLength : NULL,
                            "deployPkg.update.state %d %d %s",
                            customizationState,
                            errCode,
                            msg);
   free (msg);

   if (vmxResponse != NULL) {
      if (response != NULL) {
         sLog(log_debug, "Got VMX response '%s'", response);
         if (responseLength > responseBufferSize - 1) {
            sLog(log_warning,
                 "The VMX response is too long (only %d chars are allowed)",
                 responseBufferSize - 1);
            responseLength = responseBufferSize - 1;
         }
         memcpy(vmxResponse, response, responseLength);
         free(response);
      }
      else {
         sLog(log_debug, "Got no VMX response");
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
      vsprintf(tmp, format, args);
   }

   if (gDeployError) {
      free(gDeployError);
      gDeployError = NULL;
   }

   sLog(log_debug, "Setting deploy error: %s \n", tmp);
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
   sLog(log_debug, "Adding to list %s. \n", token);
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
 * @preturns   DEPLOY_SUCCESS on success and DEPLOY_ERROR on failure.
 *
 **/
static int
ListSize(struct List* head)
{
   int sz = 0;
   struct List* l;

   for(l = head; l; ++sz, l = l->next);
#ifdef VMX86_DEBUG
   sLog(log_debug, "Query: List size is %i. \n", sz);
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
   sLog(log_debug, "Cleaning the linked list. \n");
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
   sLog(log_info, "Cleaning old state file from tmp directory. \n");
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

   return TRUE;
}

//......................................................................................

/**
 *
 * Create a lock file.
 *
 * @param   [IN]  state   The state of the system
 * @returns DEPLOY_SUCCESS on success and DEPLOY_ERROR on error
 *
 **/
static int
Touch(const char*  state)
{
   char* fileName = malloc(strlen(BASEFILENAME) + 1 + strlen(state) + 1);
   int fd;

   sLog(log_info, "ENTER STATE %s \n", state);
   if (!fileName) {
      SetDeployError("Error allocatin memory.");
      return DEPLOY_ERROR;
   }

   strcpy(fileName, BASEFILENAME);
   strcat(fileName, ".");
   strcat(fileName, state);

   fd = open(fileName, O_WRONLY|O_CREAT|O_EXCL, 0644);

   if (fd < 0) {
      SetDeployError("Error creating lock file %s.(%s)", fileName, strerror(errno));
      free (fileName);
      return DEPLOY_ERROR;
   }

   close(fd);
   free (fileName);

   return DEPLOY_SUCCESS;
}

//......................................................................................

/**
 *
 * Delete a lock file.
 *
 * @param   [IN]  state   The state of the system
 * @returns DEPLOY_SUCCESS on success and DEPLOY_ERROR on error
 *
 **/
static int
UnTouch(const char* state)
{
   char* fileName = malloc(strlen(BASEFILENAME) + 1 + strlen(state) + 1);
   int result;

   sLog(log_info, "EXIT STATE %s \n", state);
   if (!fileName) {
      SetDeployError("Error allocating memory.");
      return DEPLOY_ERROR;
   }

   strcpy(fileName, BASEFILENAME);
   strcat(fileName, ".");
   strcat(fileName, state);

   result = remove(fileName);

   if (result < 0) {
      SetDeployError("Error removing lock %s (%s)", fileName, strerror(errno));
      free (fileName);
      return DEPLOY_ERROR;
   }

   free (fileName);
   return DEPLOY_SUCCESS;
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
 * @returns DEPLOY_SUCCESS on success and DEPLOY_ERROR on error
 *
 **/
static int
TransitionState(const char* stateFrom, const char* stateTo)
{
   sLog(log_info, "Transitioning from state %s to state %s. \n", stateFrom, stateTo);

   // Create a file to indicate state to
   if (stateTo) {
      if (Touch(stateTo) == DEPLOY_ERROR) {
         SetDeployError("Error creating new state %s. (%s)", stateTo, GetDeployError());
         return DEPLOY_ERROR;
      }
   }

   // Remove the old state file
   if (stateFrom) {
      if (UnTouch(stateFrom) == DEPLOY_ERROR) {
         SetDeployError("Error deleting old state %s.(%s)", stateFrom, GetDeployError());
         return DEPLOY_ERROR;
      }
   }

   return DEPLOY_SUCCESS;
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
   char *fileName = malloc(strlen(dir) + strlen(nicFile) + 1);
   strcpy(fileName, dir);
   strcat(fileName, nicFile);

   file = fopen(fileName, "r");
   if (file) {
      ret = malloc(NICS_SIZE);
      if (fgets(ret, NICS_SIZE, file) == NULL) {
         sLog(log_warning, "fgets() failed or reached EOF");
      }

      // Check various error condition
      if (ferror(file)) {
         SetDeployError("Error reading nic file %s (%s)", fileName, strerror(errno));
         free(ret);
         ret = NULL;
      }

      if (!feof(file)) {
         SetDeployError("More than expected nics to enable. Nics: %s \n", ret);
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
           "Trying to connect network interfaces, attempt %d",
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
         sLog(log_warning, "VMX doesn't support NICs connection status query");
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
                 "The network interfaces are connected on %d second",
                 (attempt * enableNicsWaitCount + count) *
                 enableNicsWaitSeconds);
            return;
         }

         sleep(enableNicsWaitSeconds);
      }
   }

   sLog(log_error,
        "Can't connect network interfaces after %d attempts, giving up",
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
 * @param   [IN]  tmpDirPath  Path where nics.txt and cust.cfg exist
 * @returns DEPLOY_SUCCESS on success and DEPLOY_ERROR on error
 *
 *----------------------------------------------------------------------------
 * */
static int
CloudInitSetup(const char *tmpDirPath)
{
   int deployStatus = DEPLOY_ERROR;
   const char *cloudInitTmpDirPath = "/var/run/vmware-imc";
   int forkExecResult;
   char command[1024];
   Bool cloudInitTmpDirCreated = FALSE;

   snprintf(command, sizeof(command),
            "/bin/mkdir -p %s", cloudInitTmpDirPath);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command);
   if (forkExecResult != 0) {
      SetDeployError("Error creating %s dir: %s",
                     cloudInitTmpDirPath,
                     strerror(errno));
      goto done;
   }

   cloudInitTmpDirCreated = TRUE;

   snprintf(command, sizeof(command),
            "/bin/rm -f %s/cust.cfg %s/nics.txt",
            cloudInitTmpDirPath, cloudInitTmpDirPath);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command);

   snprintf(command, sizeof(command),
            "/bin/cp %s/cust.cfg %s/cust.cfg",
            tmpDirPath, cloudInitTmpDirPath);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command);

   if (forkExecResult != 0) {
      SetDeployError("Error copying cust.cfg file: %s",
                     strerror(errno));
      goto done;
   }

   snprintf(command, sizeof(command),
            "/usr/bin/test -f %s/nics.txt", tmpDirPath);
   command[sizeof(command) - 1] = '\0';

   forkExecResult = ForkExecAndWaitCommand(command);

   /*
    * /usr/bin/test -f returns 0 if the file exists
    * non zero is returned if the file does not exist.
    * We need to copy the nics.txt only if it exists.
    */
   if (forkExecResult == 0) {
      snprintf(command, sizeof(command),
               "/bin/cp %s/nics.txt %s/nics.txt",
               tmpDirPath, cloudInitTmpDirPath);
      command[sizeof(command) - 1] = '\0';

      forkExecResult = ForkExecAndWaitCommand(command);
      if (forkExecResult != 0) {
         SetDeployError("Error copying nics.txt file: %s",
                        strerror(errno));
         goto done;
      }
   }

   deployStatus = DEPLOY_SUCCESS;

done:
   if (DEPLOY_SUCCESS == deployStatus) {
      TransitionState(INPROGRESS, DONE);
   } else {
      if (cloudInitTmpDirCreated) {
         snprintf(command, sizeof(command),
                  "/bin/rm -rf %s",
                  cloudInitTmpDirPath);
         command[sizeof(command) - 1] = '\0';
         ForkExecAndWaitCommand(command);
      }
      sLog(log_error, "Setting generic error status in vmx. \n");
      SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                  GUESTCUST_EVENT_CUSTOMIZE_FAILED,
                                  NULL);
      TransitionState(INPROGRESS, ERRORED);
   }

   return deployStatus;
}


//......................................................................................

/**
 *
 * Core function which takes care of deployment in Linux.
 * Essentially it does
 * - uncabing of the cabinet
 * - execution of the command embedded in the cabinet header
 *
 * @param   [IN[  packageName  Package file to be used for deployment
 * @returns DEPLOY_SUCCESS on success and DEPLOY_ERROR on error
 *
 **/
static int
Deploy(const char* packageName)
{
   int deployStatus = DEPLOY_SUCCESS;
   char* command = NULL;
   int deploymentResult = 0;
   char *nics;
   char* cleanupCommand;
   uint8 archiveType;
   uint8 flags;
   bool forceSkipReboot = false;
   Bool cloudInitEnabled = FALSE;
   const char *cloudInitConfigFilePath = "/etc/cloud/cloud.cfg";
   char cloudCommand[1024];
   int forkExecResult;

   TransitionState(NULL, INPROGRESS);

   // Notify the vpx of customization in-progress state
   SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                               TOOLSDEPLOYPKG_ERROR_SUCCESS,
                               NULL);

   sLog(log_info, "Reading cabinet file %s. \n", packageName);

   // Get the command to execute
   if (!GetPackageInfo(packageName, &command, &archiveType, &flags)) {
      SetDeployError("Error extracting package header information. (%s)",
                     GetDeployError());
      return DEPLOY_ERROR;
   }

   // Print the header command
#ifdef VMX86_DEBUG
   sLog(log_debug, "Header command: %s \n ", command);
#endif

   // create the destination directory
   if (!CreateDir(EXTRACTPATH "/")) {
      free(command);
      return DEPLOY_ERROR;
   }

   if (archiveType == VMWAREDEPLOYPKG_PAYLOAD_TYPE_CAB) {
      if (!ExtractCabPackage(packageName, EXTRACTPATH)) {
         free(command);
         return DEPLOY_ERROR;
      }
   } else if (archiveType == VMWAREDEPLOYPKG_PAYLOAD_TYPE_ZIP) {
      if (!ExtractZipPackage(packageName, EXTRACTPATH)) {
         free(command);
         return DEPLOY_ERROR;
      }
   }

   // check if cloud-init installed
   snprintf(cloudCommand, sizeof(cloudCommand),
            "/usr/bin/cloud-init -h");
   cloudCommand[sizeof(cloudCommand) - 1] = '\0';
   forkExecResult = ForkExecAndWaitCommand(cloudCommand);
   // if cloud-init is installed, check if it is enabled
   if (forkExecResult == 0 && IsCloudInitEnabled(cloudInitConfigFilePath)) {
      cloudInitEnabled = TRUE;
      sSkipReboot = TRUE;
      free(command);
      deployStatus =  CloudInitSetup(EXTRACTPATH);
   } else {
      // Run the deployment command
      sLog(log_info, "Launching deployment %s.  \n", command);
      deploymentResult = ForkExecAndWaitCommand(command);
      free(command);

      if (deploymentResult != 0) {
         sLog(log_error, "Customization process returned with error. \n");
         sLog(log_debug, "Deployment result = %d \n", deploymentResult);

         if (deploymentResult == CUST_NETWORK_ERROR ||
             deploymentResult == CUST_NIC_ERROR) {
            sLog(log_info, "Setting network error status in vmx. \n");
            SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                        GUESTCUST_EVENT_NETWORK_SETUP_FAILED,
                                        NULL);
         } else {
            sLog(log_info, "Setting generic error status in vmx. \n");
            SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_RUNNING,
                                        GUESTCUST_EVENT_CUSTOMIZE_FAILED,
                                        NULL);
         }

         TransitionState(INPROGRESS, ERRORED);

         deployStatus = DEPLOY_ERROR;
         SetDeployError("Deployment failed. "
                        "The forked off process returned error code.");
         sLog(log_error, "Deployment failed. "
                         "The forked off process returned error code. \n");
      } else {
         SetCustomizationStatusInVmx(TOOLSDEPLOYPKG_DONE,
                                     TOOLSDEPLOYPKG_ERROR_SUCCESS,
                                     NULL);

         TransitionState(INPROGRESS, DONE);

         deployStatus = DEPLOY_SUCCESS;
         sLog(log_info, "Deployment succeded. \n");
      }
   }

   if (!cloudInitEnabled || DEPLOY_SUCCESS != deployStatus) {
      /*
       * Read in nics to enable from the nics.txt file. We do it irrespective
       * of the sucess/failure of the customization so that at the end of it we
       * always have nics enabled.
       */
      nics = GetNicsToEnable(EXTRACTPATH);
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
   }

   cleanupCommand = malloc(strlen(CLEANUPCMD) + strlen(CLEANUPPATH) + 1);
   if (!cleanupCommand) {
      SetDeployError("Error allocating memory.");
      return DEPLOY_ERROR;
   }

   strcpy(cleanupCommand, CLEANUPCMD);
   strcat(cleanupCommand, CLEANUPPATH);

   sLog(log_info, "Launching cleanup. \n");
   if (ForkExecAndWaitCommand(cleanupCommand) != 0) {
      sLog(log_warning, "Error while clean up. Error removing directory %s. (%s)",
           EXTRACTPATH, strerror (errno));
      //TODO: What should be done if cleanup fails ??
   }
   free (cleanupCommand);

   if (flags & VMWAREDEPLOYPKG_HEADER_FLAGS_SKIP_REBOOT) {
      forceSkipReboot = true;
   }
   sLog(log_info,
        "sSkipReboot: %s, forceSkipReboot %s\n",
        sSkipReboot ? "true" : "false",
        forceSkipReboot ? "true" : "false");
   sSkipReboot |= forceSkipReboot;

   //Reset the guest OS
   if (!sSkipReboot && !deploymentResult) {
      pid_t pid = fork();
      if (pid == -1) {
         sLog(log_error, "Failed to fork: %s", strerror(errno));
      } else if (pid == 0) {
         // We're in the child

         // Repeatedly try to reboot to workaround PR 530641 where
         // telinit 6 is overwritten by a telinit 2
         int rebootComandResult = 0;
         do {
            sLog(log_info, "Rebooting\n");
            rebootComandResult = ForkExecAndWaitCommand("/sbin/telinit 6");
            sleep(1);
         } while (rebootComandResult == 0);
         sLog(log_error, "telinit returned error %d\n", rebootComandResult);

         exit (127);
      }
   }

   return deployStatus;
}

/**
 * Extract all files into the destination folder.
 */
Bool
ExtractCabPackage(const char* cabFileName,
                  const char* destDir)
{
   unsigned int error;

   sLog(log_info, "Extracting package files. \n");

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
      sLog(log_error, "Failed to open package file %s for read: %s", pkgName,
           strerror(errno));
      return FALSE;
   }
   if ((zipFd = open(zipName, O_CREAT | O_WRONLY | O_TRUNC, 0700)) < 0) {
      sLog(log_error, "Failed to create temporary zip file %s: %s", zipName,
           strerror(errno));
      close(pkgFd);
      return FALSE;;
   }
   lseek(pkgFd, sizeof(VMwareDeployPkgHdr), 0);
   while((rdCount = read(pkgFd, copyBuf, sizeof copyBuf)) > 0) {
      if (write(zipFd, copyBuf, rdCount) < 0) {
         sLog(log_warning, "write() failed");
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
   Process_RunToComplete(h, 100);

   sLog(log_info, "unzip output: %s\n", Process_GetStdout(h));

   // Assume zip failed if it wrote to stderr
   stderr = Process_GetStderr(h);
   if (strlen(stderr) > 0) {
      sLog(log_error, "Package unzip failed: %s\n", stderr);
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
      return NULL;
   }

   for(l = commandTokens, i = 0; l; l = l->next, i++) {
      char* arg = malloc(strlen(l->data) + 1);
      if (!arg) {
         SetDeployError("Error allocating memory.(%s)", strerror(errno));
         return NULL;
      }

      strcpy (arg, l->data);
      args[i] = arg;

#ifdef VMX86_DEBUG
      sLog(log_debug, "Arg (address & value) : %p %s \n", args[i], args[i]);
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
 * @param   [IN]  command  Command to execute
 * @return  Return code from the process (or DEPLOY_ERROR)
 *
 **/
static int
ForkExecAndWaitCommand(const char* command)
{
   ProcessHandle hp;
   int retval;
   int i;
   char** args = GetFormattedCommandLine(command);

   sLog(log_debug, "Command to exec : %s \n", args[0]);
   Process_Create(&hp, args, sLog);

   // Free args array as Process_Create has its own private copy now.
   for (i = 0; args[i] != NULL; i++) {
      free(args[i]);
   }
   free(args);

   Process_RunToComplete(hp, 100);
   sLog(log_info, "Customization command output: %s\n", Process_GetStdout(hp));

   if(Process_GetExitCode(hp) == 0 && strlen(Process_GetStderr(hp)) > 0) {
      // Assume command failed if it wrote to stderr, even if exitCode is 0
      sLog(log_error, "Customization command failed: %s\n", Process_GetStderr(hp));
      retval = -1;
   } else {
      retval = Process_GetExitCode(hp);
   }
   Process_Destroy(hp);
   return retval;
}

/**
 * Sets up the path for exracting file. For e.g. if the file is /a/b/c/d.abc
 * then it creates /a/b/c (skips if any of the directories along the path
 * exists). If the the path ends in '/', then the the entire input is assumed
 * to be a directory and is created as such.
 *
 * @param path  IN: Complete path of the file
 * @return TRUE on success
 */

Bool
CreateDir(const char* path)
{
   struct stat stats;
   char* token;
   char* copy;

   // make a copy we can modify
   copy = strdup(path);

   // walk through the path (it employs in string replacement)
   for (token = copy + 1; *token; ++token) {

      // find
      if (*token != '/') {
         continue;
      }

      /*
       * cut it off here for e.g. on first iteration /a/b/c/d.abc will have
       * token /a, on second /a/b etc
       */
      *token = 0;

      sLog(log_debug, "Creating directory %s", copy);

      // ignore if the directory exists
      if (!((stat(copy, &stats) == 0) && S_ISDIR(stats.st_mode))) {
         // make directory and check error (accessible only by owner)
         if (mkdir(copy, 0700) == -1) {
            sLog(log_error, "Unable to create directory %s (%s)", copy,
                 strerror(errno));
            free(copy);
            return FALSE;
         }
      }

      // restore the token
      *token = '/';
   }

   free(copy);
   return TRUE;
}

//......................................................................................

/**
 *
 * The only public function in this shared library, and the only
 * part of the DeployPkg_ interface implemented in Linux.
 * Decodes a package from a file, extracts its payload,
 * expands the payload into a temporary directory, and then executes
 * the command specified in the package.
 *
 * @param   [IN]  file  The package file
 * @retutns DEPLOY_SUCCESS on success and DEPLOY_ERROR on error
 *
 **/
int
DeployPkg_DeployPackageFromFile(const char* file)
{
   int retStatus;

   sLog(log_info, "Initializing deployment module. \n");
   Init();

   sLog(log_info, "Deploying cabinet file %s. \n", file);
   retStatus = Deploy(file);

   if (retStatus != DEPLOY_SUCCESS) {
      sLog(log_error, "Deploy error: %s \n", GetDeployError());
   }

   free(gDeployError);
   gDeployError = NULL;

   return retStatus;
}
