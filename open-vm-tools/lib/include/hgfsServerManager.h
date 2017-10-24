/*********************************************************
 * Copyright (C) 2006-2017 VMware, Inc. All rights reserved.
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

#ifndef _HGFS_SERVER_MANAGER_H_
# define _HGFS_SERVER_MANAGER_H_

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * hgfsServerManager.h --
 *
 *    Common routines needed to register an HGFS server.
 */


#ifndef VMX86_TOOLS
#include "device_shared.h" // For DeviceLock and functions

Bool Hgfs_PowerOn(void);

void HgfsServerManager_GetDeviceLock(DeviceLock **lock);
Bool HgfsServerManager_ChangeState(Bool enable);

#else  /* VMX86_TOOLS */
//#include "hgfsServer.h" // For HgfsReceiveFlags

typedef struct HgfsServerMgrData {
   const char  *appName;         // Application name to register
   void        *rpc;             // RpcChannel unused
   void        *rpcCallback;     // RpcChannelCallback unused
   void        *connection;      // Connection object returned on success
} HgfsServerMgrData;


#define HgfsServerManager_DataInit(mgr, _name, _rpc, _rpcCallback) \
   do {                                                            \
      (mgr)->appName       = (_name);                              \
      (mgr)->rpc           = (_rpc);                               \
      (mgr)->rpcCallback   = (_rpcCallback);                       \
      (mgr)->connection    = NULL;                                 \
   } while (0)

Bool HgfsServerManager_Register(HgfsServerMgrData *data);
void HgfsServerManager_Unregister(HgfsServerMgrData *data);
Bool HgfsServerManager_ProcessPacket(HgfsServerMgrData *mgrData,
                                     char const *packetIn,
                                     size_t packetInSize,
                                     char *packetOut,
                                     size_t *packetOutSize);
uint32 HgfsServerManager_InvalidateInactiveSessions(HgfsServerMgrData *mgrData);
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _HGFS_SERVER_MANAGER_H_
