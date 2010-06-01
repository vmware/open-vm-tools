/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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
 * socketMgr.h --
 *
 *    Socket management library.
 *
 */


#ifndef __SOCKETMGR_H__
#   define __SOCKETMGR_H__

#include "vm_basic_types.h"
#include "dbllnklst.h"


#ifdef _WIN32
typedef HANDLE SocketSelectable;
typedef SOCKET Socket;
#else    // _WIN32
typedef int SocketSelectable;
typedef int Socket;
#define INVALID_SOCKET     (-1)
#endif   // _WIN32


#define SOCKET_INFINITE_TIMEOUT   (-1)


typedef enum SocketMgrError {
   SOCKETMGR_ERROR_OK,
   SOCKETMGR_ERROR_DISCONNECTED,
   SOCKETMGR_ERROR_INVALID_ARG,
   SOCKETMGR_ERROR_OUT_OF_MEMORY,
   SOCKETMGR_ERROR_TIMEOUT,
   SOCKETMGR_ERROR_FAIL,
} SocketMgrError;

#define SOCKETMGR_IN    0x01
#define SOCKETMGR_OUT   0x02


typedef void (*SocketMgrConnectHandler)(Socket sock,
                                        int err,
                                        void *clientData);

typedef void (*SocketMgrSendHandler)(Socket sock,
                                     void *buf,
                                     int len,
                                     int err,
                                     int sentLen,
                                     void *clientData);

typedef void (*SocketMgrRecvHandler)(Socket sock,
                                     void *buf,
                                     int len,
                                     int err,
                                     void *clientData);


Bool SocketMgr_Init(DblLnkLst_Links *eventQueue);

Socket SocketMgr_Connect(const char *hostname,
                         unsigned short port);

Socket SocketMgr_ConnectIP(uint32 ip,
                           unsigned short port);

Socket SocketMgr_Listen(unsigned short port,
                        int backlog);

void SocketMgr_Accept(Socket sock,
                      SocketMgrConnectHandler onConnected,
                      void *clientData,
                      int timeoutInMilliSec);

void SocketMgr_Send(Socket sock,
                    void *buf,
                    int len,
                    SocketMgrSendHandler onSent,
                    void *clientData,
                    int timeoutInMilliSec);

void SocketMgr_Recv(Socket sock,
                    SocketMgrRecvHandler onReceived,
                    void *clientData,
                    int timeoutInMilliSec);

void SocketMgr_CloseSocket(Socket sock);

void SocketMgr_GetSelectables(int flags,
                              SocketSelectable **selectables,
                              int *count);

void SocketMgr_ProcessSelectable(SocketSelectable selectable,
                                 int flags);

#endif /* __SOCKETMGR_H__ */
