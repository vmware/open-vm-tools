/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/


#ifndef _POLL_H_
#define _POLL_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vmware.h"
#include "userlock.h"

#ifdef _WIN32
#define HZ 100
#elif defined linux
#include <asm/param.h>
#elif __APPLE__
#include <TargetConditionals.h>
/*
 * Old SDKs don't define TARGET_OS_IPHONE at all.
 * New ones define it to 0 on Mac OS X, 1 on iOS.
 */
#if !defined(TARGET_OS_IPHONE) || TARGET_OS_IPHONE == 0
#include <sys/kernel.h>
#endif
#include <sys/poll.h>
#define HZ 100
#endif
#ifdef __ANDROID__
/*
 * <poll.h> of android should be included, but its name is same
 * with this file. So its content is put here to avoid conflict.
 */
#include <asm/poll.h>
#define HZ 100
typedef unsigned int  nfds_t;
int poll(struct pollfd *, nfds_t, long);
#endif


/*
 * Poll event types: each type has a different reason for firing,
 * or condition that must be met before firing.
 */

typedef enum {
   /*
    * Actual Poll queue types against which you can register callbacks.
    */
   POLL_VIRTUALREALTIME = -1, /* Negative because it doesn't have its own Q */
   POLL_VTIME = 0,
   POLL_REALTIME,
   POLL_DEVICE,
   POLL_MAIN_LOOP,
   POLL_NUM_QUEUES
} PollEventType;


/*
 * Classes of events
 *
 * These are the predefined classes.  More can be declared
 * with Poll_AllocClass().
 */

typedef enum PollClass {
   POLL_CLASS_MAIN,
   POLL_CLASS_PAUSE,
   POLL_CLASS_IPC,
   POLL_CLASS_CPT,
   POLL_CLASS_MKS,
   POLL_FIXED_CLASSES,
   POLL_MAX_CLASSES = 320 /* Size enum to maximum */
} PollClass;


/*
 * Each callback is registered in a set of classes
 */

typedef struct PollClassSet {
   /* Type is uintptr_t to give best 32/64-bit code. */
#define _POLL_ELEMSIZE (sizeof (uintptr_t) * 8)
   uintptr_t bits[CEILING(POLL_MAX_CLASSES, _POLL_ELEMSIZE)];
} PollClassSet;

/* An empty PollClassSet. */
static INLINE PollClassSet
PollClassSet_Empty(void)
{
   PollClassSet set = { { 0 } };
   return set;
}

/* A PollClassSet with the single member. */
static INLINE PollClassSet
PollClassSet_Singleton(PollClass c)
{
   PollClassSet s = PollClassSet_Empty();

   ASSERT_ON_COMPILE(sizeof s.bits[0] * 8 == _POLL_ELEMSIZE); /* Size correct */
   ASSERT_ON_COMPILE((_POLL_ELEMSIZE & (_POLL_ELEMSIZE - 1)) == 0); /* power of 2 */
   ASSERT_ON_COMPILE(POLL_MAX_CLASSES <= ARRAYSIZE(s.bits) * _POLL_ELEMSIZE);
   ASSERT(c < POLL_MAX_CLASSES);

   s.bits[c / _POLL_ELEMSIZE] = CONST3264U(1) << (c % _POLL_ELEMSIZE);
   return s;
}

/* Combine two PollClassSets. */
static INLINE PollClassSet
PollClassSet_Union(PollClassSet lhs, PollClassSet rhs)
{
   PollClassSet u;
   unsigned i;

   for (i = 0; i < ARRAYSIZE(u.bits); i++) {
      u.bits[i] = lhs.bits[i] | rhs.bits[i];
   }
   return u;
}

/* Add single class to PollClassSet. */
static INLINE PollClassSet
PollClassSet_Include(PollClassSet set, PollClass c)
{
   return PollClassSet_Union(set, PollClassSet_Singleton(c));
}


#define POLL_CS_MAIN    PollClassSet_Singleton(POLL_CLASS_MAIN)
#define POLL_CS_PAUSE   PollClassSet_Union(POLL_CS_MAIN,            \
                           PollClassSet_Singleton(POLL_CLASS_PAUSE))
#define POLL_CS_CPT     PollClassSet_Union(POLL_CS_PAUSE,           \
                           PollClassSet_Singleton(POLL_CLASS_CPT))
#define POLL_CS_IPC     PollClassSet_Union(POLL_CS_CPT,             \
                           PollClassSet_Singleton(POLL_CLASS_IPC))
#define POLL_CS_VMDB    POLL_CS_PAUSE /* POLL_CLASS_VMDB is retired */
#define POLL_CS_MKS	PollClassSet_Singleton(POLL_CLASS_MKS)
/* 
 * DANGER.  You don't need POLL_CS_ALWAYS.  Really.  So don't use it.
 */
#define POLL_CS_ALWAYS  PollClassSet_Union(POLL_CS_CPT, POLL_CS_IPC)

/*
 * Poll class-set taxonomy:
 * POLL_CS_MAIN
 *    - Unless you NEED another class, use POLL_CS_MAIN.
 * POLL_CS_PAUSE
 *    - For callbacks that must occur even if the guest is paused.
 *      Most VMDB or Foundry commands are in this category.
 * POLL_CS_CPT
 *    - Only for callbacks which can trigger intermediate Checkpoint 
 *      transitions.
 *      The ONLY such callback is Migrate.
 * POLL_CS_IPC
 *    - Only for callbacks which can contain Msg_(Post|Hint|Question) 
 *      responses, and for signal handlers (why)?
 *      Vigor, VMDB, and Foundry can contain Msg_* responses.
 * POLL_CS_MKS
 *    - Callback runs in MKS thread.
 * POLL_CS_ALWAYS
 *    - Only for events that must be processed immediately.
 *      The ONLY such callback is OvhdMemVmxSizeCheck.
 */


/*
 * Poll_Callback flags
 */

#define POLL_FLAG_PERIODIC		0x01    // keep after firing
#define POLL_FLAG_REMOVE_AT_POWEROFF	0x02  	// self-explanatory
#define POLL_FLAG_READ			0x04	// device is ready for reading
#define POLL_FLAG_WRITE			0x08	// device is ready for writing
#define POLL_FLAG_SOCKET                0x10    // device is a Windows socket
#define POLL_FLAG_NO_BULL               0x20    // callback does its own locking
#define POLL_FLAG_WINSOCK               0x40    // Winsock style write events
#define POLL_FLAG_FD                    0x80    // device is a Windows file descriptor.
#define POLL_FLAG_ACCEPT_INVALID_FDS    0x100   // For broken 3rd party libs, e.g. curl
#define POLL_FLAG_THUNK_TO_WND          0x200   // thunk callback to window message loop


/*
 * Advisory minimum time period.
 * Users that want the fastest running real-time poll
 * should use TICKS_TO_USECS(1).
 */

#define TICKS_TO_USECS(_x) ((_x) * (1000000 / HZ))
#define USECS_TO_TICKS(_x) ((_x) / (1000000 / HZ))


typedef void (*PollerFunction)(void *clientData);
typedef void (*PollerFireWrapper)(PollerFunction func,
                                  void *funcData,
                                  void *wrapperData);
typedef Bool (*PollerErrorFn)(const char *errorStr);

/*
 * Initialisers:
 *
 *      For the sake of convenience, we declare the initialisers
 *      for custom implmentations here, even though the actual
 *      implementations are distinct from the core poll code.
 */

typedef struct PollOptions {
   Bool locked;           // Use internal MXUser for locking
   Bool allowFullQueue;   // Don't assert when device event queue is full.
   VThreadID windowsMsgThread;       // thread that processes Windows messages
   PollerFireWrapper fireWrapperFn;  // optional; may be useful for stats
   void *fireWrapperData; // optional
   PollerErrorFn errorFn; // optional; called upon unrecoverable error
} PollOptions;


void Poll_InitDefault(void);
void Poll_InitDefaultEx(const PollOptions *opts);
void Poll_InitGtk(void); // On top of glib for Linux
void Poll_InitCF(void);  // On top of CoreFoundation for OSX


/*
 * Functions
 */
int Poll_SocketPair(Bool vmci, Bool stream, int fds[2]);
void Poll_Loop(Bool loop, Bool *exit, PollClass c);
void Poll_LoopTimeout(Bool loop, Bool *exit, PollClass c, int timeout);
Bool Poll_LockingEnabled(void);
void Poll_Exit(void);


/*
 * Poll_Callback adds a callback regardless of whether an identical one exists.
 *
 * Likewise, Poll_CallbackRemove removes exactly one callback.
 */

VMwareStatus Poll_Callback(PollClassSet classSet,
                           int flags,
                           PollerFunction f,
                           void *clientData,
                           PollEventType type,
                           PollDevHandle info, // fd/microsec delay
                           MXUserRecLock *lck);
Bool Poll_CallbackRemove(PollClassSet classSet,
                         int flags,
                         PollerFunction f,
                         void *clientData,
                         PollEventType type);
Bool Poll_CallbackRemoveOneByCB(PollClassSet classSet,
                                int flags,
                                PollerFunction f,
                                PollEventType type,
                                void **clientData);

void Poll_NotifyChange(PollClassSet classSet);

/*
 * Wrappers for Poll_Callback and Poll_CallbackRemove that present
 * simpler subsets of those interfaces.
 */

VMwareStatus Poll_CB_Device(PollerFunction f,
                            void *clientData,
                            PollDevHandle device,
                            Bool periodic);

Bool Poll_CB_DeviceRemove(PollerFunction f,
                          void *clientData,
                          Bool periodic);


VMwareStatus Poll_CB_RTime(PollerFunction f,
                           void *clientData,
                           int delay,	// microseconds
                           Bool periodic,
                           MXUserRecLock *lock);

Bool Poll_CB_RTimeRemove(PollerFunction f,
                         void *clientData,
                         Bool periodic);


#ifdef _WIN32
void Poll_SetWindowMessageRecipient(HWND hWnd, UINT msg, Bool alwaysThunk);
Bool Poll_FireWndCallback(void *lparam);
#endif

#endif // _POLL_H_
