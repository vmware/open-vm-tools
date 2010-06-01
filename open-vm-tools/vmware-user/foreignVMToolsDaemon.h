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
 * foreignVMToolsDaemon.h --
 *
 *
 */

#ifndef __FOREIGN_VM_TOOLS_DAEMON_H__
#define __FOREIGN_VM_TOOLS_DAEMON_H__

struct ForeignVMToolsConnection;



/*
 *-----------------------------------------------------------------------------
 *
 * Locks --
 *
 * This is the lock for each handle. We may have thousands of handles,
 * so these locks must be lightweight. SyncRecMutex creates a pair
 * of file handles on Linux, so it doesn't scale. That's ok, since we
 * don't need all the functionality of SyncRecMutex, like these locks
 * will not be cross process or passed to poll.
 *-----------------------------------------------------------------------------
 */
#ifdef _WIN32

typedef CRITICAL_SECTION VixLockType;
static INLINE VixError VIX_INIT_LOCK(VixLockType *lockPtr);
VixError VIX_INIT_LOCK(VixLockType *lockPtr) 
{
   InitializeCriticalSection(lockPtr); 
   return VIX_OK; 
}
#define VIX_DELETE_LOCK(lockPtr) DeleteCriticalSection(lockPtr)
#define VIX_ENTER_LOCK(lockPtr) EnterCriticalSection(lockPtr)
#define VIX_LEAVE_LOCK(lockPtr) LeaveCriticalSection(lockPtr)

#else // #elif !_WIN32

#ifdef __APPLE__
#define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#elif defined(__FreeBSD__) || defined(sun)
#include <unistd.h>
#define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#else
#include <linux/unistd.h>
#endif

#define _REENTRANT 1
#include <pthread.h>
#include <sys/types.h>

typedef pthread_mutex_t VixLockType;
static INLINE VixError VIX_INIT_LOCK(VixLockType *lockPtr);
VixError VIX_INIT_LOCK(VixLockType *lockPtr) 
{
   int result;
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);

   result = pthread_mutex_init(lockPtr, &attr);
   pthread_mutexattr_destroy(&attr);
   if (0 != result) {
      return VIX_E_OUT_OF_MEMORY;
   }
   return VIX_OK; 
}
#define VIX_DELETE_LOCK(lockPtr) pthread_mutex_destroy(lockPtr)
#define VIX_ENTER_LOCK(lockPtr) pthread_mutex_lock(lockPtr)
#define VIX_LEAVE_LOCK(lockPtr) pthread_mutex_unlock(lockPtr)
#define VIX_GET_THREADID() Util_GetCurrentThreadId()

#endif // #ifdef LINUX



/*
 * This represents one request we have received from the client
 * and are processing.
 */
typedef struct ForeignVMToolsCommand {
   struct ForeignVMToolsConnection     *connection;
   VixCommandRequestHeader             requestHeader;

   char                                asyncOpName[32];

   int                                 guestCredentialType;
   VixCommandNamePassword              *guestUserNamePassword;
   char                                *obfuscatedGuestUserNamePassword;
   int                                 obfuscatedCredentialType;

   VmTimeType                          programStartTime;
   int                                 runProgramOptions;

   char                                *responseBody;
   size_t                              responseBodyLength;

   struct ForeignVMToolsCommand       *next;
} ForeignVMToolsCommand;


/*
 * This represents one active connection to a Foundry client.
 */
typedef struct ForeignVMToolsConnection {
   int                                 socket;
   
   /*
    * This is the state of the message we are currently reading on 
    * the connection. At any given time, we always have an outstanding 
    * asynch read, which is waiting for the next request to arrive.
    */
   VixCommandRequestHeader             requestHeader;
   char                                *completeRequest;

   struct ForeignVMToolsConnection     *prev;
   struct ForeignVMToolsConnection     *next;
} ForeignVMToolsConnection;



typedef enum FoundryDisconnectReason {
   SHUTDOWN_FOR_SYSTEM_SHUTDOWN,
   SHUTDOWN_FOR_PEER_DISCONNECT,
} FoundryDisconnectReason;




Bool ForeignTools_InitializeNetworking(void);

void ForeignToolsSelectLoop(FoundryWorkerThread *threadState);

void ForeignToolsCloseConnection(ForeignVMToolsConnection *connectionState,
                                 FoundryDisconnectReason reason);

void ForeignToolsSendResponse(ForeignVMToolsConnection *connectionState,
                              VixCommandRequestHeader *requestHeader,
                              size_t responseBodyLength,
                              void *responseBody,
                              VixError error,
                              uint32 additionalError,
                              uint32 responseFlags);

void ForeignToolsSendResponseUsingTotalMessage(ForeignVMToolsConnection *connectionState,
                                               VixCommandRequestHeader *requestHeader,
                                               size_t totalMessageSize,
                                               void *totalMessage,
                                               VixError error,
                                               uint32 additionalError,
                                               uint32 responseFlags);

void ForeignToolsProcessMessage(ForeignVMToolsConnection *connectionState);

void ForeignToolsDiscardCommand(ForeignVMToolsCommand *command);

void ForeignToolsWakeSelectThread(void);


extern VixLockType                        globalLock;

extern struct ForeignVMToolsConnection    *activeConnectionList;
extern struct ForeignVMToolsCommand       *globalCommandList;

#endif /* __FOREIGN_VM_TOOLS_DAEMON_H__ */
