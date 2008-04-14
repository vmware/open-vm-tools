/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * foreignVMToolsDaemon.c --
 *
 * This implements the Vix tools using a socket to connect to the tools
 * from a client. This also assumes there is no VMX process, so the
 * tools takes commands directly from the client over the socket.
 *
 * In a VMware VM:
 *   Client ---socket--->  VMX  ---backdoor---> Tools
 *
 * In a foreign VM:
 *   Client ---socket--->  Tools
 *
 * So, this code performs operations on behalf of the VMX and the tools.
 * The tools functions are all implemented by the vixTools library, which
 * is the same code that runs in the VMware tools. This module then handles
 * socket communication and does functions that would otherwise be done by
 * the VMX.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#else
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#if defined(__FreeBSD__) || defined(sun)
#include <unistd.h>
#else
#include <linux/unistd.h>
#endif
#endif

#include "vmware.h"
#include "vm_version.h"
#include "vm_app.h"
#include "message.h"
#include "eventManager.h"
#include "debug.h"
#include "util.h"
#include "strutil.h"
#include "str.h"
#include "err.h"
#include "hostinfo.h"
#include "guest_os.h"
#include "conf.h"
#include "base64.h"
#include "hgfsServer.h"
#include "hgfs.h"
#include "system.h"
#include "codeset.h"

#include "vixOpenSource.h"
#include "syncEvent.h"
#include "foundryThreads.h"
#include "vixCommands.h"
#include "foreignVMToolsDaemon.h"

#include "vixTools.h"


VixLockType                         globalLock;
static struct FoundryWorkerThread   *selectThread;

ForeignVMToolsConnection            *activeConnectionList = NULL;
ForeignVMToolsCommand               *globalCommandList = NULL;

static struct GuestApp_Dict         *configDictionary = NULL;

static Bool ForeignToolsIsCommandAlive(ForeignVMToolsCommand *asyncCommand);

static ForeignVMToolsCommand *ForeignToolsGetActiveCommand(const char *name);

static void ForeignToolsSendRunProgramResponse(const char *requestName,
                                               VixError resultErr,
                                               int exitCode,
                                               int64 pid);

static VixError ForeignToolsGetUserCredentialForGuest(ForeignVMToolsConnection *connectionState,
                                                      ForeignVMToolsCommand *commandState);

static VixError ForeignToolsGetProperties(ForeignVMToolsCommand *asyncCommand,
                                          VixMsgTrivialRequest *requestMsg);

static VixError ForeignToolsSetProperties(ForeignVMToolsCommand *asyncCommand,
                                          VixMsgSetVMStateRequest *requestMsg);

static VixError ForeignToolsGetToolsState(ForeignVMToolsCommand *asyncCommand,
                                          VixMsgTrivialRequest *requestMsg);


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignTools_Initialize --
 *
 *      Start a worker thread.
 *
 * Results:
 *      FoundryWorkerThread *
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

Bool
ForeignTools_Initialize(GuestApp_Dict *configDictionaryParam)     // IN
{
   VixError err = VIX_OK;
   Bool success = TRUE;

   MessageStub_RegisterTransport();

   /*
    * Initialize the limited global state that protects us when 
    * client applications explicitly pump events.
    */
   err = VIX_INIT_LOCK(&globalLock);
   if (VIX_OK != err) {
      success = FALSE;
      goto abort;
   }

   if (NULL == configDictionaryParam) {
      success = FALSE;
      goto abort;
   }
   configDictionary = configDictionaryParam;

   VixTools_SetConsoleUserPolicy(TRUE); // allowConsoleUserOpsParam
   VixTools_SetRunProgramCallback(ForeignToolsSendRunProgramResponse);

   success = ForeignTools_InitializeNetworking();
   if (!success) {
      goto abort;
   }

   /*
    * Start the worker threads that will pump poll.
    */
   selectThread = FoundryThreads_StartThread(ForeignToolsSelectLoop, NULL);
   if (NULL == selectThread) {
      goto abort;
   }

   return(TRUE);

abort:
   return(FALSE);
} // ForeignTools_Initialize


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignTools_Shutdown --
 *
 *      Shutdown a thread and destroys its thread state.
 * 
 * Results:
 *      None.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

void
ForeignTools_Shutdown(void)
{
   /*
    * Tell the select thread to exit and wait for it to stop.
    */
   selectThread->stopThread = TRUE;
   ForeignToolsWakeSelectThread();
   if (NULL != selectThread) {
      FoundryThreads_StopThread(selectThread);
      selectThread = NULL;
   }

   /*
    * Close every connection.
    */
   VIX_ENTER_LOCK(&globalLock);
   while (NULL != activeConnectionList) {
      ForeignToolsCloseConnection(activeConnectionList, SHUTDOWN_FOR_SYSTEM_SHUTDOWN);
   }
   VIX_LEAVE_LOCK(&globalLock);

   /*
    * Shut down the work queue.
    */
   VIX_DELETE_LOCK(&globalLock);
} // ForeignTools_Shutdown


/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsIsCommandAlive --
 *
 *      Returns TRUE if ForeignVMToolsCommand is still in the list of
 *      active commands.  Otherwise, return FALSE.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------------
 */

Bool
ForeignToolsIsCommandAlive(ForeignVMToolsCommand *asyncCommand) // IN
{
   ForeignVMToolsCommand *command;
   ForeignVMToolsCommand *nextCommand;
   Bool isAlive = FALSE;

   if (NULL == asyncCommand) {
      goto abort;
   }

   VIX_ENTER_LOCK(&globalLock);

   command = globalCommandList;
   while (NULL != command) {
      nextCommand = command->next;
      if (command == asyncCommand) {
         isAlive = TRUE;
         break;
      }
      command = nextCommand;
   }

   VIX_LEAVE_LOCK(&globalLock);

abort:
   return(isAlive);
} // ForeignToolsIsCommandAlive


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsDiscardCommand --
 *
 *      Record that we are executing an async command.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
ForeignToolsDiscardCommand(ForeignVMToolsCommand *command)   // IN
{
   ForeignVMToolsCommand *targetCommand;
   ForeignVMToolsCommand *prevCommand;

   if ((NULL == command) || (NULL == command->connection)) {
      return;
   }

   VIX_ENTER_LOCK(&globalLock);

   targetCommand = globalCommandList;
   prevCommand = NULL;
   while (NULL != targetCommand) {
      if (targetCommand == command) {
         break;
      }
      prevCommand = targetCommand;
      targetCommand = targetCommand->next;
   }

   /*
    * If the command is not in the active list, then somebody else
    * already freed it. Do not delete it again.
    */
   if (NULL == targetCommand) {
      goto abort;
   }

   if (NULL != prevCommand) {
      prevCommand->next = targetCommand->next;
   } else {
      globalCommandList = targetCommand->next;
   }

   free(command->responseBody);
   free(command->guestUserNamePassword);
   free(command->obfuscatedGuestUserNamePassword);
   free(command);

abort:
   VIX_LEAVE_LOCK(&globalLock);
} // ForeignToolsDiscardCommand


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsGetActiveCommand --
 *
 *       This gets the named active state.
 *
 * Results:
 *       The named active state.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

ForeignVMToolsCommand *
ForeignToolsGetActiveCommand(const char *name)  // IN
{
   ForeignVMToolsCommand *command = NULL;

   /*
    * Look for the test that corresponds to this name.
    */
   VIX_ENTER_LOCK(&globalLock);

   command = globalCommandList;
   while (NULL != command) {
      if (0 == Str_Strcasecmp(name, command->asyncOpName)) {
         break;
      }

      command = command->next;
   }

   VIX_LEAVE_LOCK(&globalLock);

   return command;
} // ForeignToolsGetActiveCommand



/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsSendRunProgramResponse --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
ForeignToolsSendRunProgramResponse(const char *requestName, // IN
                                   VixError resultErr,      // IN
                                   int exitCode,            // IN
                                   int64 pid)               // IN
{
   VixError err = VIX_OK;
   int additionalError = 0;
   ForeignVMToolsCommand *asyncCommand = NULL;
   VmTimeType programStopTime;
   VmTimeType deltaTime = 0;
   VixMsgRunProgramResponse responseMessage;

   Hostinfo_GetTimeOfDay(&programStopTime);

   asyncCommand = ForeignToolsGetActiveCommand(requestName);
   if (NULL == asyncCommand) {
      return;
   }
   /*
    * If all we wanted to do was start the program, then we are
    * done.
    */
   if (asyncCommand->runProgramOptions & VIX_RUNPROGRAM_RETURN_IMMEDIATELY) {
      return;
   }

   err = resultErr;
 
   /*
    * Find how long the program was running. Convert to seconds,
    * and report the result in VMDB.
    */
   deltaTime = programStopTime - asyncCommand->programStartTime;
   deltaTime = deltaTime / 1000000;

   responseMessage.exitCode = exitCode;
   responseMessage.deltaTime = deltaTime;
   responseMessage.pid = pid;
   responseMessage.stdOutLength = 0;
   responseMessage.stdErrLength = 0;

   ForeignToolsSendResponseUsingTotalMessage(asyncCommand->connection,
                                             &(asyncCommand->requestHeader),
                                             sizeof responseMessage,
                                             &responseMessage,
                                             err,
                                             additionalError,
                                             VIX_RESPONSE_EXTENDED_RESULT_V1);
   ForeignToolsDiscardCommand(asyncCommand);
} // ForeignToolsSendRunProgramResponse


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsGetUserCredentialForGuest -- 
 *
 *      Get the credentials we will pass into the guest.
 *      These may be passed in with the original command request, or else we 
 *      may use some default values. 
 *
 *      This also does limited checking, mainly to see if any credentials are
 *      even provided. It does NOT check to see if a user/name password is valid,
 *      or if a particular user is authorized for some operation. That will
 *      be done later in the guest when we actually execute each operation.
 *
 *      This leaves the actual credentials for this command packaged in 
 *      commandState->obfuscatedGuestUserNamePassword 
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
ForeignToolsGetUserCredentialForGuest(ForeignVMToolsConnection *connectionState,   // IN
                                      ForeignVMToolsCommand *commandState)         // IN
{
   VixError err = VIX_OK;
   //CryptoError cryptoErr = CRYPTO_ERROR_SUCCESS;
   Bool success;
   char *guestUserName = NULL;
   char *guestPassword = NULL;
   char *namePasswordMessage = NULL;
   char *endCredential;
   char *encryptedNamePassword = NULL;
   size_t encryptedNamePasswordLength = 0;
   char *decryptedBuffer = NULL;
   size_t decryptedBufferLength = 0;
   VixCommandNamePassword *guestUserNamePassword;
   VixCommandNamePassword newUserNamePassword;
   size_t newCredentialsLength;
   size_t newMessageLength;
   char *newNamePasswordBuffer;
   //CryptoKeyedHash *keyedHash;

   /*
    * If there was an optional userName and password sent, then parse it now.
    */
   if (VIX_USER_CREDENTIAL_NAME_PASSWORD == commandState->requestHeader.userCredentialType) {
      if (commandState->requestHeader.commonHeader.credentialLength <= 0) {
         err = VIX_E_INVALID_MESSAGE_HEADER;
         goto abort;
      }

      namePasswordMessage = connectionState->completeRequest 
                                 + commandState->requestHeader.commonHeader.headerLength 
                                 + commandState->requestHeader.commonHeader.bodyLength;
      endCredential = namePasswordMessage
                                 + commandState->requestHeader.commonHeader.credentialLength;
      /*
       * Make sure this is a valid NULL-terminated C string.
       */
      if (*(endCredential - 1)) {
         err = VIX_E_INVALID_MESSAGE_HEADER;
         goto abort;
      }

      success = Base64_EasyDecode(namePasswordMessage,
                                  (uint8 **) &encryptedNamePassword, 
                                  &encryptedNamePasswordLength);
      if (!success) {
         err = VIX_E_INVALID_MESSAGE_HEADER;
         goto abort;
      }

#if 0
      cryptoErr = CryptoKeyedHash_FromString(CryptoKeyedHashName_HMAC_SHA_1,
                                             &keyedHash);
      if (CryptoError_IsFailure(cryptoErr)) {
         err = Vix_TranslateCryptoError(cryptoErr);
         goto abort;
      }

      cryptoErr = CryptoKey_DecryptWithMAC(connectionState->sessionKey, 
                                           keyedHash,
                                           (uint8 *) encryptedNamePassword,
                                           encryptedNamePasswordLength,
                                           (uint8 **) &decryptedBuffer, 
                                           &decryptedBufferLength);
      if (CryptoError_IsFailure(cryptoErr)) {
         err = Vix_TranslateCryptoError(cryptoErr);
         goto abort;
      }
#else
      decryptedBuffer = encryptedNamePassword;
      decryptedBufferLength = encryptedNamePasswordLength;
#endif

      /*
       * Get the name/password fields from the credential data structure.
       */
      guestUserNamePassword = (VixCommandNamePassword *) decryptedBuffer;
      guestUserName = (char *) guestUserNamePassword;
      guestUserName += sizeof(VixCommandNamePassword);
      guestPassword = guestUserName;
      guestPassword += guestUserNamePassword->nameLength + 1;

      /*
       * If the client sent a valid userName/password, then this is OK.
       * Send it on to the tools and they will check permissions.
       * Allow an empty password string, that may be valid for some accounts.
       */
      if ((NULL == guestUserName) || !(guestUserName[0])) {
         err = VIX_E_GUEST_USER_PERMISSIONS;
         goto abort;
      }

      commandState->obfuscatedCredentialType = commandState->requestHeader.userCredentialType;
   /////////////////////////////////////////////////////////////////////////////
   } else if ((VIX_USER_CREDENTIAL_ANONYMOUS == commandState->requestHeader.userCredentialType)
         || (VIX_USER_CREDENTIAL_NONE == commandState->requestHeader.userCredentialType)) {
      err = VIX_E_MISSING_ANON_GUEST_ACCOUNT;
      goto abort;
   /////////////////////////////////////////////////////////////////////////////
   } else if (VIX_USER_CREDENTIAL_ROOT == commandState->requestHeader.userCredentialType) {
      err = VIX_E_ROOT_GUEST_OPERATIONS_PROHIBITED;
      goto abort;
   /////////////////////////////////////////////////////////////////////////////
   } else if (VIX_USER_CREDENTIAL_CONSOLE_USER == commandState->requestHeader.userCredentialType) {
      //<> For debug only. I need this until I package the tools as
      // an NT service. Otherwise, I cannot run a program.
      //<><>err = VIX_E_CONSOLE_GUEST_OPERATIONS_PROHIBITED;
      //<><>goto abort;
   } else {
      err = VIX_E_UNRECOGNIZED_COMMAND;
      goto abort;
   }

   /*
    * Now, package the name/password to be sent to the guest.
    */
   commandState->obfuscatedGuestUserNamePassword 
      = VixMsg_ObfuscateNamePassword(guestUserName, guestPassword);

   newCredentialsLength = sizeof(VixCommandNamePassword)
                           + strlen(commandState->obfuscatedGuestUserNamePassword) + 1;

   newMessageLength = commandState->requestHeader.commonHeader.totalMessageLength
                        - commandState->requestHeader.commonHeader.credentialLength
                        + newCredentialsLength;

   connectionState->completeRequest = Util_SafeRealloc(connectionState->completeRequest,
                                                       newMessageLength);
   if (NULL != guestUserName) {
      newUserNamePassword.nameLength = strlen(guestUserName);
   } else {
      newUserNamePassword.nameLength = 0;
   }
   if (NULL != guestPassword) {
      newUserNamePassword.passwordLength = strlen(guestPassword);
   } else {
      newUserNamePassword.passwordLength = 0;
   }
   newNamePasswordBuffer = connectionState->completeRequest 
                           + commandState->requestHeader.commonHeader.headerLength 
                           + commandState->requestHeader.commonHeader.bodyLength;

   memcpy(newNamePasswordBuffer,
          &newUserNamePassword,
          sizeof(newUserNamePassword));
   newNamePasswordBuffer += sizeof(newUserNamePassword);
   memcpy(newNamePasswordBuffer,
          commandState->obfuscatedGuestUserNamePassword,
          strlen(commandState->obfuscatedGuestUserNamePassword) + 1);

   commandState->requestHeader.commonHeader.totalMessageLength = newMessageLength;
   commandState->requestHeader.commonHeader.credentialLength = newCredentialsLength;

   connectionState->requestHeader.commonHeader.totalMessageLength = newMessageLength;
   connectionState->requestHeader.commonHeader.credentialLength = newCredentialsLength;


abort:
   free(encryptedNamePassword);
#if 0
   Crypto_Free(decryptedBuffer, decryptedBufferLength);
#endif

   return(err);
} // ForeignToolsGetUserCredentialForGuest


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsGetProperties --
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
ForeignToolsGetProperties(ForeignVMToolsCommand *asyncCommand,   // IN
                          VixMsgTrivialRequest *requestMsg)    // IN
{
   VixError err = VIX_OK;
   VixPropertyListImpl propList;
   char *serializedBufferBody = NULL;
   size_t serializedBufferLength = 0;
   char *responseMessage = NULL;
   size_t responseMessageLength = 0;
   int guestOSID;
   int guestOSFamily = GUEST_OS_FAMILY_ANY;   

   guestOSID = 0; // GuestOS_GetOSID(NULL);


   VixPropertyList_Initialize(&propList);

   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_VM_TOOLS_STATE,
                                    VIX_TOOLSSTATE_RUNNING);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_VMX_VERSION,
                                   "Foreign VM Tools");
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_FOREIGN_VM_TOOLS_VERSION,
                                   VIX_FOREIGN_VM_TOOLS_VMX_VERSION_STRING);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_VMX_PRODUCT_NAME,
                                   PRODUCT_NAME);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_VMX_VIX_FEATURES,
                                    VIX_TOOLSFEATURE_SUPPORT_GET_HANDLE_STATE);
   if (VIX_OK != err) {
      goto abort;
   }

   /*
    * *****************************************
    * Now, fill in default values for the tools.
    * Later, if the tools are running, they will have a chance to
    * provide correct values.
    */
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_TOOLS_PRODUCT_NAM,
                                   "");
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_TOOLS_VERSION,
                                   "");
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_GUEST_TOOLS_API_OPTIONS,
                                    0);
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetInteger(&propList,
                                    VIX_PROPERTY_GUEST_OS_FAMILY,
                                    guestOSFamily);

   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_NAME,
                                   "");
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_POWER_OFF_SCRIPT,
                                   "");
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_POWER_ON_SCRIPT,
                                   "");
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_SUSPEND_SCRIPT,
                                   "");
   if (VIX_OK != err) {
      goto abort;
   }
   err = VixPropertyList_SetString(&propList,
                                   VIX_PROPERTY_GUEST_RESUME_SCRIPT,
                                   "");
   if (VIX_OK != err) {
      goto abort;
   }

   /*
    * Serialize the property list to buffer
    */
   err = VixPropertyList_Serialize(&propList,
                                   FALSE,
                                   &serializedBufferLength,
                                   &serializedBufferBody);
   if (VIX_OK != err) {
      goto abort;
   }

   responseMessageLength = sizeof(VixMsgGetVMStateResponse) 
                                + serializedBufferLength;
   responseMessage = Util_SafeMalloc(responseMessageLength);
   memcpy(responseMessage + sizeof(VixMsgGetVMStateResponse), 
          serializedBufferBody,
          serializedBufferLength);
   ((VixMsgGetVMStateResponse *) responseMessage)->bufferSize = serializedBufferLength;

   ForeignToolsSendResponseUsingTotalMessage(asyncCommand->connection,
                                           &(asyncCommand->requestHeader),
                                           responseMessageLength,
                                           responseMessage,
                                           err,
                                           0, // additionalError,
                                           0); // responseFlags
   ForeignToolsDiscardCommand(asyncCommand);

   /*
    * ForeignToolsSendResponseUsingTotalMessage owns responseMessage now,
    * and it will deallocate it.
    */
   responseMessage = NULL;

abort:
   free(responseMessage);
   free(serializedBufferBody);
   VixPropertyList_RemoveAllWithoutHandles(&propList);

   return err;
} // ForeignToolsGetProperties


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsSetProperties --
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
ForeignToolsSetProperties(ForeignVMToolsCommand *asyncCommand,      // IN
                          VixMsgSetVMStateRequest *requestMsg)    // IN
{
   VixError err = VIX_OK;
   VixPropertyListImpl propList;
   size_t serializedBufferLength = 0;
   char *serializedBuffer = NULL;

   /*
    * Do some validation
    */
   if ((NULL == asyncCommand)
         || (NULL == requestMsg)
         || (0 > requestMsg->bufferSize)) {
      err = VIX_E_INVALID_ARG;
      goto abort;
   }

   serializedBufferLength = requestMsg->bufferSize;
   serializedBuffer = ((char*)requestMsg + sizeof(*requestMsg));

   /*
    * Create a temporary property list and deserialize the buffer into.
    */
   err = VixPropertyList_Deserialize(&propList, 
                                     serializedBuffer, 
                                     serializedBufferLength);
   if (VIX_OK != err) {
      goto abort;
   }

   /*
    * To Do: <>
    * Change any VMX properties here.
    */

abort:
   VixPropertyList_RemoveAllWithoutHandles(&propList);

   return err;
} // ForeignToolsSetProperties


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsGetToolsState --
 *
 * Results:
 *      VixError
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VixError
ForeignToolsGetToolsState(ForeignVMToolsCommand *asyncCommand,   // IN
                          VixMsgTrivialRequest *requestMsg)    // IN
{
   VixError err = VIX_OK;
   VixPropertyListImpl propList;
   char *decodedStr = NULL;
   size_t decodedLength;
   Bool success;
   char *serializedBufferBody = NULL;
   size_t serializedBufferLength = 0;
   Bool deleteResultValue = FALSE;
   char *base64Buffer = NULL;
   size_t base64BufferLength;
   char *responseMessage = NULL;
   size_t responseMessageLength = 0;

   VixPropertyList_Initialize(&propList);

   err = VixTools_ProcessVixCommand((VixCommandRequestHeader *) asyncCommand,
                                    asyncCommand->asyncOpName,
                                    1024 * 1024, // maxResultBufferSize,
                                    &configDictionary,
                                    &base64Buffer,
                                    &base64BufferLength,
                                    &deleteResultValue);
   if (VIX_OK != err) {
      goto abort;
   }

   /*
    * If we got a string back from the guest, then decode it and 
    * convert it into a list of properties.
    */
   if (NULL != base64Buffer) {
      decodedStr = Util_SafeMalloc(base64BufferLength);
      success = Base64_Decode(base64Buffer, 
                              decodedStr, 
                              base64BufferLength,
                              &decodedLength);
      if (success) {
         err = VixPropertyList_Deserialize(&propList, decodedStr, decodedLength);
         err = VIX_OK;
      }

      (void) VixPropertyList_SetInteger(&propList,
                                        VIX_PROPERTY_VM_TOOLS_STATE,
                                        VIX_TOOLSSTATE_RUNNING);
   } else { // if (NULL == base64Buffer)
      (void) VixPropertyList_SetInteger(&propList,
                                        VIX_PROPERTY_VM_TOOLS_STATE,
                                        VIX_TOOLSSTATE_NOT_INSTALLED);
   }

   /*
    * Serialize the property list to buffer
    */
   err = VixPropertyList_Serialize(&propList,
                                   FALSE,
                                   &serializedBufferLength,
                                   &serializedBufferBody);
   if (VIX_OK != err) {
      goto abort;
   }

   responseMessageLength = sizeof(VixMsgGetVMStateResponse) 
                                + serializedBufferLength;
   responseMessage = Util_SafeMalloc(responseMessageLength);
   memcpy(responseMessage + sizeof(VixMsgGetVMStateResponse), 
          serializedBufferBody,
          serializedBufferLength);
   ((VixMsgGetVMStateResponse *) responseMessage)->bufferSize = serializedBufferLength;

   ForeignToolsSendResponseUsingTotalMessage(asyncCommand->connection,
                                             &(asyncCommand->requestHeader),
                                             responseMessageLength,
                                             responseMessage,
                                             err,
                                             0, // additionalError,
                                             0);  // responseFlags
   ForeignToolsDiscardCommand(asyncCommand);

   /*
    * VMAutomation_SendResponseUsingTotalMessage owns responseMessage now,
    * and it will deallocate it.
    */
   responseMessage = NULL;

abort:
   VixPropertyList_RemoveAllWithoutHandles(&propList);
   free(decodedStr);
   free(serializedBufferBody);
   free(responseMessage);

   return err;
} // ForeignToolsGetToolsState


/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsProcessMessage --
 *
 *      Calls the correct handler for a particular message type,
 *      and determines whether to queue more receives.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
ForeignToolsProcessMessage(ForeignVMToolsConnection *connectionState)     //IN
{
   VixError err = VIX_OK;
   ForeignVMToolsCommand *commandState = NULL;
   uint32 additionalError = 0;
   Bool sendResponse = FALSE;
   Bool deleteResultValue = FALSE;
   char *dummyResponse;
   size_t dummyResponseLength;

   ASSERT(NULL != connectionState);

   /*
    * Allocate state for the command. 
    */
   commandState = Util_SafeCalloc(1, sizeof *commandState);
   commandState->connection = connectionState;
   commandState->requestHeader = connectionState->requestHeader;
   commandState->guestCredentialType = connectionState->requestHeader.userCredentialType;
   commandState->guestUserNamePassword = NULL;
   commandState->obfuscatedGuestUserNamePassword = NULL;
   commandState->obfuscatedCredentialType = 0;
   commandState->runProgramOptions = 0;
   commandState->responseBody = NULL;
   commandState->responseBodyLength = 0;

   VIX_ENTER_LOCK(&globalLock);
   commandState->next = globalCommandList;
   globalCommandList = commandState;
   VIX_LEAVE_LOCK(&globalLock);

   switch (connectionState->requestHeader.opCode) {
      ///////////////////////////////////
      case VIX_COMMAND_GET_HANDLE_STATE:
         err = ForeignToolsGetProperties(commandState, 
                                       (VixMsgTrivialRequest *) connectionState->completeRequest);
         break;

      ///////////////////////////////////
      case VIX_COMMAND_INSTALL_TOOLS:
      case VIX_COMMAND_WAIT_FOR_TOOLS:
         err = VIX_OK;
         sendResponse = TRUE;
         break;

      ///////////////////////////////////
      case VIX_COMMAND_GET_DISK_PROPERTIES:
      case VIX_COMMAND_CAPTURE_SCREEN:
      case VIX_COMMAND_MOUSE_EVENTS:
      case VIX_COMMAND_KEYSTROKES:
      case VIX_COMMAND_LIST_USB_DEVICES:
         err = VIX_E_NOT_SUPPORTED;
         break;

      ///////////////////////////////////
      case VIX_CREATE_SESSION_KEY_COMMAND:
         err = VIX_E_NOT_SUPPORTED;
         break;

      ///////////////////////////////////
      case VIX_COMMAND_SET_HANDLE_STATE:
         err = ForeignToolsSetProperties(commandState, 
                                         (VixMsgSetVMStateRequest *) (connectionState->completeRequest));
         sendResponse = TRUE;
         break;

      ///////////////////////////////////
      case VIX_COMMAND_RUN_PROGRAM:
         err = ForeignToolsGetUserCredentialForGuest(connectionState, commandState);
         if (VIX_OK != err) {
            goto abort;
         }

         Hostinfo_GetTimeOfDay(&(commandState->programStartTime));
         Str_Snprintf(commandState->asyncOpName, 
                      sizeof(commandState->asyncOpName),
                      "%p", 
                      commandState);
         commandState->responseBody = NULL;

         err = VixTools_ProcessVixCommand((VixCommandRequestHeader *) connectionState->completeRequest,
                                          commandState->asyncOpName,
                                          1024 * 1024, // maxResultBufferSize,
                                          &configDictionary,
                                          &dummyResponse,
                                          &dummyResponseLength,
                                          &deleteResultValue);

         /*
          * We don't complete the command until the program exits.
          */
         commandState->responseBody = NULL;
         commandState->responseBodyLength = 0;
         break;

#if 0
      ///////////////////////////////////
      //<><><>
      // I can add these.
      case VIX_COMMAND_VM_POWEROFF:
      case VIX_COMMAND_VM_RESET:
      case VIX_COMMAND_RELOAD_VM:
         break;

      ///////////////////////////////////
      //<><><>
      // These will not be supported.
      case VIX_COMMAND_VM_SUSPEND:
      case VIX_COMMAND_SET_NIC_BANDWIDTH:
      case VIX_COMMAND_UPGRADE_VIRTUAL_HARDWARE:
      case VIX_COMMAND_CREATE_RUNNING_VM_SNAPSHOT:
      case VIX_COMMAND_CONSOLIDATE_RUNNING_VM_SNAPSHOT:
         break;
#endif

      ///////////////////////////////////
      case VIX_COMMAND_GET_TOOLS_STATE:
         err = ForeignToolsGetToolsState(commandState,
                                         (VixMsgTrivialRequest *) (connectionState->completeRequest));
         break;

      ////////////////////////////////////
      case VIX_COMMAND_CHECK_USER_ACCOUNT:
      case VIX_COMMAND_LOGOUT_IN_GUEST:
         err = ForeignToolsGetUserCredentialForGuest(connectionState, commandState);
         if (VIX_OK != err) {
            goto abort;
         }

         if (VIX_USER_CREDENTIAL_NAME_PASSWORD 
                  == commandState->requestHeader.userCredentialType) {
            err = VixTools_ProcessVixCommand((VixCommandRequestHeader *) connectionState->completeRequest,
                                             commandState->asyncOpName,
                                             1024 * 1024, // maxResultBufferSize,
                                             &configDictionary,
                                             &(commandState->responseBody),
                                             &(commandState->responseBodyLength),
                                             &deleteResultValue);
         } else {
            commandState->responseBody = NULL;
            commandState->responseBodyLength = 0;
         }

         sendResponse = TRUE;
         break;


      ///////////////////////////////////
      // By default, most tools commands do require authentication.
      default:
         err = ForeignToolsGetUserCredentialForGuest(connectionState, commandState);
         if (VIX_OK != err) {
            goto abort;
         }

         err = VixTools_ProcessVixCommand((VixCommandRequestHeader *) connectionState->completeRequest,
                                          commandState->asyncOpName,
                                          1024 * 1024, // maxResultBufferSize,
                                          &configDictionary,
                                          &(commandState->responseBody),
                                          &(commandState->responseBodyLength),
                                          &deleteResultValue);

         sendResponse = TRUE;
         break;
   } // switch


abort:
   if ((VIX_OK != err) || (sendResponse)) {
      if (ForeignToolsIsCommandAlive(commandState)) {
         ForeignToolsSendResponse(connectionState,
                                &(connectionState->requestHeader),
                                commandState->responseBodyLength,
                                commandState->responseBody,
                                err,
                                additionalError,
                                0); // responseFlags

         if (!deleteResultValue) {
            commandState->responseBody = NULL;
            commandState->responseBodyLength = 0;
         }

         ForeignToolsDiscardCommand(commandState);
      }
   }
} // ForeignToolsProcessMessage









