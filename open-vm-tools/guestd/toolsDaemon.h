/*********************************************************
 * Copyright (C) 2001 VMware, Inc. All rights reserved.
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
 * toolsDaemon.h --
 *
 *    Platform independent methods used by the tools daemon/win32-service.
 *
 */


#ifndef __TOOLSDAEMON_H__
#   define __TOOLSDAEMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "vm_basic_types.h"
#include "vm_app.h"
#include "guestApp.h"
#include "procMgr.h"
#include "dbllnklst.h"
#include "file.h"

typedef Bool ToolsDaemon_Callback(void *clientData);

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX    "vmsvc"
#endif

/*
 * ToolsDaemon "member" data.
 */
typedef struct ToolsDaemon_Data {
   struct RpcIn *in;
   const char *execLogPath;
   Bool inError;
   int errorCount;
   GuestApp_Dict *optionsDict;         // the options we get from VMware
   GuestApp_Dict **pConfDict;          // the name/value pairs from the conf file
   struct Event *timeSyncEvent;
   uint32 timeSyncPeriod;
   struct Event *oldOptionsLoop;
   ToolsDaemon_Callback *haltCB;       // callback when we do a soft halt
   void *haltCBData;                   // its data
   ToolsDaemon_Callback *rebootCB;     // callback when we do a soft reboot
   void *rebootCBData;                 // its data
   ToolsDaemon_Callback *resetCB;      // callback when we receive a reset
   void *resetCBData;                  // its data
   ToolsDaemon_Callback *linkHgfsCB;   // callback to create hgfs link on desktop
   void *linkHgfsCBData;               // its data
   ToolsDaemon_Callback *unlinkHgfsCB; // callback to remove hgfs link on desktop
   void *unlinkHgfsCBData;             // its data
   GuestOsState stateChgInProgress;
   GuestOsState lastFailedStateChg;
   ProcMgr_AsyncProc *asyncProc;
   ProcMgr_Callback *asyncProcCb;
   void *asyncProcCbData;
} ToolsDaemon_Data;


ToolsDaemon_Data *
ToolsDaemon_Init(GuestApp_Dict **pConfDict,         // IN
                 const char *execLogPath,           // IN
                 ToolsDaemon_Callback haltCB,       // IN
                 void *haltCBData,                  // IN
                 ToolsDaemon_Callback rebootCB,     // IN
                 void *rebootCBData,                // IN
                 ToolsDaemon_Callback resetCB,      // IN
                 void *resetCBData,                 // IN
		 ToolsDaemon_Callback linkHgfsCB,   // IN
		 void *linkHgfsCBData,              // IN
		 ToolsDaemon_Callback unlinkHgfsCB, // IN
		 void *unlinkHgfsCBData);           // IN

Bool
ToolsDaemon_Init_Backdoor(ToolsDaemon_Data *data); // IN/OUT

void
ToolsDaemon_Cleanup(ToolsDaemon_Data *data); // IN/OUT

Bool
ToolsDaemon_CheckReset(ToolsDaemon_Data *data,  // IN/OUT
                       uint64 *sleepUsecs);     // IN/OUT

void
ToolsDaemon_Cleanup_Backdoor(ToolsDaemon_Data *data); // IN/OUT

Bool
ToolsDaemon_SyncTime(Bool syncBackward);

Bool
ToolsDaemon_SetOsPhase(Bool stateChangeSucceeded,
                       unsigned int cmdId);

void
ToolsDaemon_GetMinResolution(GuestApp_Dict *dict,   // IN
                             unsigned int *width,   // OUT
                             unsigned int *height); // OUT

const char *
ToolsDaemon_GetGuestTempDirectory(void);

#if !defined(N_PLAT_NLM) && !defined(sun)
Bool
ToolsDaemonHgfs(char const **result,     // OUT
                size_t *resultLen,       // OUT
                const char *name,        // IN
                const char *args,        // IN
                unsigned int argsSize,   // IN: Size of args
                void *clientData);       // Unused
#endif

Bool
ForeignTools_Initialize(GuestApp_Dict *configDictionaryParam);

void
ForeignTools_Shutdown(void);


/* Queue of timer events for the tools daemon. */
extern DblLnkLst_Links *ToolsDaemonEventQueue;

#ifdef __cplusplus
}
#endif

#endif /* __TOOLSDAEMON_H__ */
