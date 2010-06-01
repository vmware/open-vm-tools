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
 * guestSocketMgr.c --
 *
 *    Implementation of the socket management lib
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include "str.h"

#if defined(_WIN32)
/*
 * Copied from asyncSocket.c:
 * We redefine strcpy/strcat because the Windows SDK uses it for getaddrinfo().
 * When we upgrade SDKs, this redefinition can go away.
 */
#define strcpy(dst,src) Str_Strcpy((dst), (src), sizeof (dst))
#define strcat(dst,src) Str_Strcpy((dst), (src), sizeof (dst))
#include <winsock2.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#else // defined(_WIN32)
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(sun)
/* For FIONREAD */
#include <sys/filio.h>
#endif // defined(sun)
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif // defined(_WIN32)


#include "vmware.h"
#include "vm_version.h"
#include "vm_basic_types.h"
#include "vm_assert.h"
#include "debug.h"
#include "msg.h"
#include "str.h"
#include "eventManager.h"
#include "socketMgr.h"


#ifdef _WIN32
#define SocketMgrLastError()        pfnWSAGetLastError()
#define EWOULDBLOCK                 WSAEWOULDBLOCK
#else
#define SocketMgrLastError()        errno
#define pfnNtohl                    ntohl
#define pfnHtonl                    htonl
#define pfnHtons                    htons
#define pfnGetaddrinfo              getaddrinfo
#define pfnFreeaddrinfo             freeaddrinfo
#define pfnConnect                  connect
#define pfnSocket                   socket
#define pfnListen                   listen
#define pfnBind                     bind
#define pfnAccept                   accept
#define pfnSend                     send
#define pfnRecv                     recv
#define pfnClosesocket              close
#define pfnIoctlsocket              ioctl
#endif


struct SocketState;

typedef struct SocketAcceptRequest {
   SocketMgrConnectHandler          onConnected;
   void                             *clientData;

   Event                            *timeoutEvent;
   struct SocketState               *socketState;

   struct SocketAcceptRequest       *next;
} SocketAcceptRequest;

typedef struct SocketRecvRequest {
   SocketMgrRecvHandler             onReceived;
   void                             *clientData;

   Event                            *timeoutEvent;
   struct SocketState               *socketState;

   struct SocketRecvRequest         *next;
} SocketRecvRequest;

typedef struct SocketSendRequest {
   char                             *buf;
   int                              len;
   int                              pos;
   SocketMgrSendHandler             onSent;
   void                             *clientData;

   Event                            *timeoutEvent;
   struct SocketState               *socketState;

   struct SocketSendRequest         *next;
} SocketSendRequest;


/*
 * Keeps track of the socket state.
 */

typedef struct SocketState {
   Socket                  socket;
   Bool                    isListening;

#ifdef _WIN32
   HANDLE                  event;
   long                    lNetworkEvents;
#endif

   SocketAcceptRequest     *acceptQueue;
   SocketRecvRequest       *recvQueue;
   SocketSendRequest       *sendQueue;

   struct SocketState      *next;
} SocketState;


static void SocketMgrOnAccept(SocketState *sockState);

static Bool SocketMgrAcceptTimeoutCallback(void *clientData);

static void SocketMgrOnSend(SocketState *sockState);

static Bool SocketMgrSendTimeoutCallback(void *clientData);

static void SocketMgrOnRecv(SocketState *sockState);

static Bool SocketMgrRecvTimeoutCallback(void *clientData);


static DblLnkLst_Links *socketMgrEventQueue = NULL;
static SocketState *globalSocketList = NULL;


#ifdef _WIN32
typedef int (WSAAPI *WSAStartupProcType)(WORD wVersionRequested,
                                         LPWSADATA lpWSAData);

typedef WSAEVENT (WSAAPI *WSACreateEventProcType)(void);

typedef int (WSAAPI *WSAGetLastErrorProcType)(void);

typedef BOOL (WSAAPI *WSACloseEventProcType)(WSAEVENT hEvent);

typedef int (WSAAPI *WSAEventSelectProcType)(SOCKET s,
                                             WSAEVENT hEventObject,
                                             long lNetworkEvents);

typedef BOOL (WSAAPI *WSAResetEventProcType)(WSAEVENT hEvent);

typedef int (WSAAPI *WSAEnumNetworkEventsProcType)(SOCKET s,
                                             WSAEVENT hEventObject,
                                             LPWSANETWORKEVENTS lpNetworkEvents);

typedef u_long (WSAAPI *NtohlProcType)(u_long netlong);

typedef u_long (WSAAPI *HtonlProcType)(u_long hostlong);

typedef u_short (WSAAPI *HtonsProcType)(u_short hostshort);

typedef int (WSAAPI *GetaddrinfoProcType)(const char FAR * nodename,
                                          const char FAR * servname,
                                          const struct addrinfo FAR * hints,
                                          struct addrinfo FAR * FAR * res);

typedef void (WSAAPI *FreeaddrinfoProcType)(LPADDRINFO pAddrInfo);

typedef int (WSAAPI *ClosesocketProcType)(SOCKET s);

typedef int (WSAAPI *ConnectProcType)(SOCKET s,
                                      const struct sockaddr FAR * name,
                                      int namelen);

typedef SOCKET (WSAAPI *SocketProcType)(int af,
                                        int type,
                                        int protocol);

typedef int (WSAAPI *ListenProcType)(SOCKET s,
                                     int backlog);

typedef int (WSAAPI *BindProcType)(SOCKET s,
                                   const struct sockaddr FAR * name,
                                   int namelen);

typedef SOCKET (WSAAPI *AcceptProcType)(SOCKET s,
                                        struct sockaddr FAR * addr,
                                        int FAR * addrlen);

typedef int (WSAAPI *SendProcType)(SOCKET s,
                                   const char FAR * buf,
                                   int len,
                                   int flags);

typedef int (WSAAPI *RecvProcType)(SOCKET s,
                                   char FAR * buf,
                                   int len,
                                   int flags);

typedef int (WSAAPI *IoctlsocketProcType)(SOCKET s,
                                          long cmd,
                                          u_long FAR * argp);


static WSAStartupProcType           pfnWSAStartup = NULL;
static WSACreateEventProcType       pfnWSACreateEvent = NULL;
static WSAGetLastErrorProcType      pfnWSAGetLastError = NULL;
static WSACloseEventProcType        pfnWSACloseEvent = NULL;
static WSAEventSelectProcType       pfnWSAEventSelect = NULL;
static WSAResetEventProcType        pfnWSAResetEvent = NULL;
static WSAEnumNetworkEventsProcType pfnWSAEnumNetworkEvents = NULL;
static NtohlProcType                pfnNtohl = NULL;
static HtonlProcType                pfnHtonl = NULL;
static HtonsProcType                pfnHtons = NULL;
static GetaddrinfoProcType          pfnGetaddrinfo = NULL;
static FreeaddrinfoProcType         pfnFreeaddrinfo = NULL;
static ClosesocketProcType          pfnClosesocket = NULL;
static ConnectProcType              pfnConnect = NULL;
static SocketProcType               pfnSocket = NULL;
static ListenProcType               pfnListen = NULL;
static BindProcType                 pfnBind = NULL;
static AcceptProcType               pfnAccept = NULL;
static SendProcType                 pfnSend = NULL;
static RecvProcType                 pfnRecv = NULL;
static IoctlsocketProcType          pfnIoctlsocket = NULL;

#endif // ifdef _WIN32


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_Init --
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Bool
SocketMgr_Init(DblLnkLst_Links *eventQueue)
{
#ifdef _WIN32
   HMODULE hWs2_32 = NULL;
   WSADATA wsaData;
   WORD versionRequested = MAKEWORD(2, 0);

   hWs2_32 = LoadLibrary("ws2_32.dll");
   if (NULL == hWs2_32) {
      return FALSE;
   }

   pfnWSAStartup = (WSAStartupProcType)
                        GetProcAddress(hWs2_32, "WSAStartup");
   if (NULL == pfnWSAStartup) {
      return FALSE;
   }

   pfnWSACreateEvent = (WSACreateEventProcType)
                        GetProcAddress(hWs2_32, "WSACreateEvent");
   if (NULL == pfnWSACreateEvent) {
      return FALSE;
   }

   pfnWSAGetLastError = (WSAGetLastErrorProcType)
                        GetProcAddress(hWs2_32, "WSAGetLastError");
   if (NULL == pfnWSAGetLastError) {
      return FALSE;
   }

   pfnWSACloseEvent = (WSACloseEventProcType)
                        GetProcAddress(hWs2_32, "WSACloseEvent");
   if (NULL == pfnWSACloseEvent) {
      return FALSE;
   }

   pfnWSAEventSelect = (WSAEventSelectProcType)
                        GetProcAddress(hWs2_32, "WSAEventSelect");
   if (NULL == pfnWSAEventSelect) {
      return FALSE;
   }

   pfnWSAResetEvent = (WSAResetEventProcType)
                        GetProcAddress(hWs2_32, "WSAResetEvent");
   if (NULL == pfnWSAResetEvent) {
      return FALSE;
   }

   pfnWSAEnumNetworkEvents = (WSAEnumNetworkEventsProcType)
                        GetProcAddress(hWs2_32, "WSAEnumNetworkEvents");
   if (NULL == pfnWSAEnumNetworkEvents) {
      return FALSE;
   }

   pfnNtohl = (NtohlProcType)
                        GetProcAddress(hWs2_32, "ntohl");
   if (NULL == pfnNtohl) {
      return FALSE;
   }

   pfnHtonl = (HtonlProcType)
                        GetProcAddress(hWs2_32, "htonl");
   if (NULL == pfnHtonl) {
      return FALSE;
   }

   pfnHtons = (HtonsProcType)
                        GetProcAddress(hWs2_32, "htons");
   if (NULL == pfnHtons) {
      return FALSE;
   }

   pfnGetaddrinfo = (GetaddrinfoProcType)
                        GetProcAddress(hWs2_32, "getaddrinfo");
   if (NULL == pfnGetaddrinfo) {
      return FALSE;
   }

   pfnFreeaddrinfo = (FreeaddrinfoProcType)
                        GetProcAddress(hWs2_32, "freeaddrinfo");
   if (NULL == pfnFreeaddrinfo) {
      return FALSE;
   }

   pfnClosesocket = (ClosesocketProcType)
                        GetProcAddress(hWs2_32, "closesocket");
   if (NULL == pfnClosesocket) {
      return FALSE;
   }

   pfnConnect = (ConnectProcType)
                        GetProcAddress(hWs2_32, "connect");
   if (NULL == pfnConnect) {
      return FALSE;
   }

   pfnSocket = (SocketProcType)
                        GetProcAddress(hWs2_32, "socket");
   if (NULL == pfnSocket) {
      return FALSE;
   }

   pfnListen = (ListenProcType)
                        GetProcAddress(hWs2_32, "listen");
   if (NULL == pfnListen) {
      return FALSE;
   }

   pfnBind = (BindProcType)
                        GetProcAddress(hWs2_32, "bind");
   if (NULL == pfnBind) {
      return FALSE;
   }

   pfnAccept = (AcceptProcType)
                        GetProcAddress(hWs2_32, "accept");
   if (NULL == pfnAccept) {
      return FALSE;
   }

   pfnSend = (SendProcType)
                        GetProcAddress(hWs2_32, "send");
   if (NULL == pfnSend) {
      return FALSE;
   }

   pfnRecv = (RecvProcType)
                        GetProcAddress(hWs2_32, "recv");
   if (NULL == pfnRecv) {
      return FALSE;
   }

   pfnIoctlsocket = (IoctlsocketProcType)
                        GetProcAddress(hWs2_32, "ioctlsocket");
   if (NULL == pfnIoctlsocket) {
      return FALSE;
   }

   if (0 != pfnWSAStartup(versionRequested, &wsaData)) {
      return FALSE;
   }
#endif   // _WIN32

   if (NULL != socketMgrEventQueue) {
      /*
       * Should not call _Init more than once.
       */
      ASSERT(0);
      return FALSE;
   }
   socketMgrEventQueue = eventQueue;

   return TRUE;
} // SocketMgr_Init


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrSearchSocket --
 *
 *      This functions searchs the socket state list and returns the
 *      address pointer to the previous node's next field. This way
 *      deletion/insertion can be easily done without extra variables.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

SocketState **
SocketMgrSearchSocket(Socket sock)     // IN
{
   SocketState **result = &globalSocketList;

   while (NULL != *result) {
      if (sock == (*result)->socket) {
         break;
      }

      result = &((*result)->next);
   }

   return result;
} // SocketMgrSearchSocket


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrSearchSelectable --
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

SocketState **
SocketMgrSearchSelectable(SocketSelectable selectable)      // IN
{
   SocketState **result = &globalSocketList;

   while (NULL != *result) {
#if defined(_WIN32)
      if (selectable == (*result)->event) {
#else
      if (selectable == (*result)->socket) {
#endif
         break;
      }

      result = &((*result)->next);
   }

   return result;
} // SocketMgrSearchSelectable


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrCreateSocketState --
 *
 *      Create a SocketState struct and add it to the global list.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

SocketState *
SocketMgrCreateSocketState(Socket sock)      // IN
{
   SocketState *sockState = NULL;

   ASSERT(INVALID_SOCKET != sock);

   sockState = (SocketState *) calloc(1, sizeof *sockState);
   if (NULL == sockState) {
      return NULL;
   }

   sockState->socket = sock;

#ifdef _WIN32
   sockState->event = pfnWSACreateEvent();
   if (WSA_INVALID_EVENT == sockState->event) {
      goto abort;
   }
   sockState->lNetworkEvents = 0;
#endif   // _WIN32

   /*
    * Insert the new state at the beginning of the list.
    */
   sockState->next = globalSocketList;
   globalSocketList = sockState;

   return sockState;

#ifdef _WIN32
abort:
   free(sockState);
   return NULL;
#endif   // _WIN32
} // SocketMgrCreateSocketState


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_Connect --
 *
 *      Connect to a hostname:port. This function is just a wrapper for
 *      ConnectIP that does hostname -> IP lookup.
 *
 * Results:
 *      
 *      If no error occurs, this function returns a valid Socket.
 *      Otherwise, it returns INVALID_SOCKET.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Socket
SocketMgr_Connect(const char *hostname,      // IN
                  unsigned short port)       // IN
{
   Socket sock = INVALID_SOCKET;
   struct sockaddr_in *addr = NULL;
   struct addrinfo hints;
   struct addrinfo *aiTop = NULL;
   struct addrinfo *aiIter = NULL;
   char portString[6];
   int retCode;

   if (NULL == hostname) {
      return INVALID_SOCKET;
   }

   Str_Sprintf(portString, sizeof portString, "%d", port);
   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;

   retCode = pfnGetaddrinfo(hostname, portString, &hints, &aiTop);
   if (0 != retCode) {
      goto abort;
   }

   for (aiIter = aiTop; NULL != aiIter; aiIter = aiIter->ai_next) {
      if (AF_INET == aiIter->ai_family) {
         addr = (struct sockaddr_in *) aiIter->ai_addr;
         break;
      }
   }

   if (NULL == addr) {
      goto abort;
   }

   sock = SocketMgr_ConnectIP(pfnNtohl(addr->sin_addr.s_addr), port);

abort:
   if (NULL != aiTop) {
      pfnFreeaddrinfo(aiTop);
   }

   return sock;
} // SocketMgr_Connect


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_ConnectIP --
 *
 *      Connect to IP:port.
 *
 * Results:
 *      
 *      If no error occurs, this function returns a valid Socket.
 *      Otherwise, it returns INVALID_SOCKET.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Socket
SocketMgr_ConnectIP(uint32 ip,               // IN
                    unsigned short port)     // IN
{
   Socket sock = INVALID_SOCKET;
   struct sockaddr_in tcpaddr = { 0 };
   SocketState *sockState = NULL;

   /*
    * Create a new IP socket.
    */
   sock = pfnSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (INVALID_SOCKET == sock) {
      goto abort;
   }

   /*
    * Create a address structure to pass to connect.
    */
   tcpaddr.sin_family = AF_INET;
   tcpaddr.sin_addr.s_addr = pfnHtonl(ip);
   tcpaddr.sin_port = pfnHtons(port);

   /*
    * Connect to the address.
    */
   if (0 != pfnConnect(sock, (struct sockaddr *) &tcpaddr, sizeof tcpaddr)) {
      goto abort;
   }

   sockState = SocketMgrCreateSocketState(sock);
   if (NULL == sockState) {
      goto abort;
   }
   sockState->isListening = FALSE;

   return sock;

abort:
   if (INVALID_SOCKET != sock) {
      pfnClosesocket(sock);
   }
   free(sockState);

   return INVALID_SOCKET;
} // SocketMgr_ConnectIP


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_Listen --
 *
 *      This function creates a socket and puts it to the listening
 *      mode on the port.
 *
 * Results:
 *      
 *      If no error occurs, this function returns a valid SOCKET.
 *      Otherwise, it returns INVALID_SOCKET.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Socket
SocketMgr_Listen(unsigned short port,     // IN
                 int backlog)             // IN
{
   Socket sock = INVALID_SOCKET;
   struct sockaddr_in tcpaddr = { 0 };
   SocketState *sockState = NULL;

   /*
    * Create a new IP socket.
    */
   sock = pfnSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (INVALID_SOCKET == sock) {
      goto abort;
   }

   /*
    * Create a address structure to pass to bind.
    */
   tcpaddr.sin_family = AF_INET;
   tcpaddr.sin_addr.s_addr = pfnHtonl(INADDR_ANY);
   tcpaddr.sin_port = pfnHtons(port);

   /*
    * Bind to the port.
    */
   if (0 != pfnBind(sock, (struct sockaddr *) &tcpaddr, sizeof tcpaddr)) {
      goto abort;
   }

   /*
    * Listen on the socket.
    */
   if (0 != pfnListen(sock, backlog)) {
      goto abort;
   }

   sockState = SocketMgrCreateSocketState(sock);
   if (NULL == sockState) {
      goto abort;
   }
   sockState->isListening = TRUE;

   return sock;

abort:
   if (INVALID_SOCKET != sock) {
      pfnClosesocket(sock);
   }
   free(sockState);

   return INVALID_SOCKET;
} // SocketMgr_Listen


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_Accept --
 *
 *      This function puts an accept request in the queue.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgr_Accept(Socket sock,                               // IN
                 SocketMgrConnectHandler onConnected,       // IN
                 void *clientData,                          // IN
                 int timeoutInMilliSec)                     // IN
{
   SocketState **sockState;
   SocketAcceptRequest *acceptReq = NULL;
   SocketAcceptRequest **lastReq;
   int error;

   ASSERT(NULL != onConnected);

   sockState = SocketMgrSearchSocket(sock);
   if (NULL == *sockState) {
      error = SOCKETMGR_ERROR_INVALID_ARG;
      goto abort;
   }

   acceptReq = (SocketAcceptRequest *) calloc(1, sizeof *acceptReq);
   if (NULL == acceptReq) {
      error = SOCKETMGR_ERROR_OUT_OF_MEMORY;
      goto abort;
   }

   acceptReq->onConnected = onConnected;
   acceptReq->clientData = clientData;
   acceptReq->next = NULL;
   acceptReq->socketState = *sockState;
   if (timeoutInMilliSec >= 0) {
      acceptReq->timeoutEvent = EventManager_Add(socketMgrEventQueue,
                                                 timeoutInMilliSec / 10,
                                                 SocketMgrAcceptTimeoutCallback,
                                                 acceptReq);
   }

   /*
    * Append it to acceptQueue.
    */
   lastReq = &((*sockState)->acceptQueue);
   while (NULL != *lastReq) {
      lastReq = &((*lastReq)->next);
   }
   *lastReq = acceptReq;

   return;

abort:
   free(acceptReq);
   onConnected(sock, error, clientData);
} // SocketMgr_Accept


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrOnAccept --
 *
 *      This is the function that gets called when there's a new
 *      connection pending.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgrOnAccept(SocketState *sockState)       // IN
{
   Socket newsock;
   SocketState *newsockState;
   SocketAcceptRequest *acceptReq;

   ASSERT(NULL != sockState);

   /*
    * The socket must be in listening mode and there are accept requests
    * pending.
    */
   ASSERT(sockState->isListening);
   ASSERT(NULL != sockState->acceptQueue);

   /*
    * Accept a new socket.
    */
   newsock = pfnAccept(sockState->socket, NULL, NULL);
   if (INVALID_SOCKET != newsock) {
      newsockState = SocketMgrCreateSocketState(newsock);
      if (NULL != newsockState) {
         newsockState->isListening = FALSE;
      } else {
         pfnClosesocket(newsock);
         newsock = INVALID_SOCKET;
      }
   }

   if (INVALID_SOCKET != newsock) {
      /*
       * Dequeue
       */
      acceptReq = sockState->acceptQueue;
      sockState->acceptQueue = acceptReq->next;

      if (NULL != acceptReq->timeoutEvent) {
         EventManager_Remove(acceptReq->timeoutEvent);
         acceptReq->timeoutEvent = NULL;
      }

      ASSERT(NULL != acceptReq->onConnected);
      (acceptReq->onConnected)(newsock,
                               SOCKETMGR_ERROR_OK,
                               acceptReq->clientData);

      free(acceptReq);
   }
} // SocketMgrOnAccept


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrAcceptTimeoutCallback --
 *
 *      This is the callback that gets called when the time for
 *      accepting the new connection is up.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Bool
SocketMgrAcceptTimeoutCallback(void *clientData)        // IN
{
   SocketAcceptRequest *acceptReq;
   SocketAcceptRequest **prevReq;

   acceptReq = (SocketAcceptRequest *) clientData;
   ASSERT(NULL != acceptReq);
   ASSERT(NULL != acceptReq->socketState);
   ASSERT(NULL != acceptReq->onConnected);

   (acceptReq->onConnected)(acceptReq->socketState->socket,
                            SOCKETMGR_ERROR_TIMEOUT,
                            acceptReq->clientData);

   /*
    * Remove the request from the queue.
    */
   prevReq = &(acceptReq->socketState->acceptQueue);
   while ((acceptReq != *prevReq) && (NULL != *prevReq)) {
      prevReq = &((*prevReq)->next);
   }

   /*
    * We must have found it in the queue. Otherwise, error.
    */
   ASSERT(acceptReq == *prevReq);
   *prevReq = acceptReq->next;

   free(acceptReq);

   return TRUE;
} // SocketMgrAcceptTimeoutCallback


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_Send --
 *
 *      This function puts a send request in the queue.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgr_Send(Socket sock,                              // IN
               void *buf,                                // IN
               int len,                                  // IN
               SocketMgrSendHandler onSent,         // IN
               void *clientData,                         // IN
               int timeoutInMilliSec)                    // IN
{
   SocketState **sockState;
   SocketSendRequest *sendReq = NULL;
   SocketSendRequest **lastReq;
   int error;
   Bool sendNow = FALSE;

   ASSERT(NULL != onSent);

   sockState = SocketMgrSearchSocket(sock);
   if (NULL == *sockState) {
      error = SOCKETMGR_ERROR_INVALID_ARG;
      goto abort;
   }

   ASSERT(NULL != socketMgrEventQueue);
   
   sendReq = (SocketSendRequest *) calloc(1, sizeof *sendReq);
   if (NULL == sendReq) {
      error = SOCKETMGR_ERROR_OUT_OF_MEMORY;
      goto abort;
   }

   sendReq->onSent = onSent;
   sendReq->buf = buf;
   sendReq->len = len;
   sendReq->pos = 0;
   sendReq->clientData = clientData;
   sendReq->next = NULL;
   sendReq->socketState = *sockState;
   if (timeoutInMilliSec >= 0) {
      sendReq->timeoutEvent = EventManager_Add(socketMgrEventQueue,
                                               timeoutInMilliSec / 10,
                                               SocketMgrSendTimeoutCallback,
                                               sendReq);
   }

   /*
    * Try to send now if this is the first request in the queue.
    */
   sendNow = (NULL == (*sockState)->sendQueue);

   /*
    * Append it to sendQueue.
    */
   lastReq = &((*sockState)->sendQueue);
   while (NULL != *lastReq) {
      lastReq = &((*lastReq)->next);
   }
   *lastReq = sendReq;

   if (sendNow) {
      SocketMgrOnSend(*sockState);
   }

   return;

abort:
   free(sendReq);
   onSent(sock, buf, len, error, 0, clientData);
} // SocketMgr_Send


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrOnSend --
 *
 *      This is the function that gets called when the socket is
 *      available to send data.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgrOnSend(SocketState *sockState)         // IN
{
   SocketSendRequest *sendReq;
   int sent;
   int error = 0;

   ASSERT(NULL != sockState);

   /*
    * The socket must NOT be in listening mode and there are send
    * requests pending.
    */
   ASSERT(!(sockState->isListening));
   ASSERT(NULL != sockState->sendQueue);

   /*
    * Try to process as many requests as possible.
    */
   while (NULL != sockState->sendQueue) {
      sendReq = sockState->sendQueue;

      sent = pfnSend(sockState->socket,
                     sendReq->buf + sendReq->pos,
                     sendReq->len - sendReq->pos,
                     0);
      sendReq->pos += sent;

      if (sendReq->pos == sendReq->len) {
         /*
          * Dequeue
          */
         sockState->sendQueue = sendReq->next;

         if (NULL != sendReq->timeoutEvent) {
            EventManager_Remove(sendReq->timeoutEvent);
            sendReq->timeoutEvent = NULL;
         }

         ASSERT(NULL != sendReq->onSent);
         (sendReq->onSent)(sockState->socket,
                           sendReq->buf,
                           sendReq->len,
                           SOCKETMGR_ERROR_OK,
                           sendReq->pos,
                           sendReq->clientData);
         free(sendReq);
      } else {
         error = SocketMgrLastError();
         break;
      }
   }

   if ((NULL != sockState->sendQueue) && (EWOULDBLOCK != error)) {
      SocketMgr_CloseSocket(sockState->socket);
   }
} // SocketMgrOnSend


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrSendTimeoutCallback --
 *
 *      This is the callback that gets called when the time for
 *      sending data is up.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Bool
SocketMgrSendTimeoutCallback(void *clientData)        // IN
{
   SocketSendRequest *sendReq;
   SocketSendRequest **prevReq;

   sendReq = (SocketSendRequest *) clientData;
   ASSERT(NULL != sendReq);
   ASSERT(NULL != sendReq->socketState);
   ASSERT(NULL != sendReq->onSent);

   (sendReq->onSent)(sendReq->socketState->socket,
                     sendReq->buf,
                     sendReq->len,
                     SOCKETMGR_ERROR_TIMEOUT,
                     sendReq->pos,
                     sendReq->clientData);

   /*
    * Remove the request from the queue.
    */
   prevReq = &(sendReq->socketState->sendQueue);
   while ((sendReq != *prevReq) && (NULL != *prevReq)) {
      prevReq = &((*prevReq)->next);
   }

   /*
    * We must have found it in the queue. Otherwise, error.
    */
   ASSERT(sendReq == *prevReq);
   *prevReq = sendReq->next;

   free(sendReq);

   return TRUE;
} // SocketMgrSendTimeoutCallback


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_Recv --
 *
 *      This function puts a receive data request in the queue.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgr_Recv(Socket sock,                              // IN
               SocketMgrRecvHandler onReceived,     // IN
               void *clientData,                         // IN
               int timeoutInMilliSec)                    // IN
{
   SocketState **sockState;
   SocketRecvRequest *recvReq = NULL;
   SocketRecvRequest **lastReq;
   int error;

   ASSERT(NULL != onReceived);

   sockState = SocketMgrSearchSocket(sock);
   if (NULL == *sockState) {
      error = SOCKETMGR_ERROR_INVALID_ARG;
      goto abort;
   }

   recvReq = (SocketRecvRequest *) calloc(1, sizeof *recvReq);
   if (NULL == recvReq) {
      error = SOCKETMGR_ERROR_OUT_OF_MEMORY;
      goto abort;
   }

   recvReq->onReceived = onReceived;
   recvReq->clientData = clientData;
   recvReq->next = NULL;
   recvReq->socketState = *sockState;
   if (timeoutInMilliSec >= 0) {
      recvReq->timeoutEvent = EventManager_Add(socketMgrEventQueue,
                                               timeoutInMilliSec / 10,
                                               SocketMgrRecvTimeoutCallback,
                                               recvReq);
   }

   /*
    * Append it to sendQueue.
    */
   lastReq = &((*sockState)->recvQueue);
   while (NULL != *lastReq) {
      lastReq = &((*lastReq)->next);
   }
   *lastReq = recvReq;

   return;

abort:
   free(recvReq);
   onReceived(sock, NULL, 0, error, clientData);
} // SocketMgr_Recv


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrOnRecv --
 *
 *      This is the function that gets called when there's incoming data
 *      pending on the socket.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgrOnRecv(SocketState *sockState)         // IN
{
   SocketRecvRequest *recvReq;
   char *buf = NULL;
   int len = 0;
   int error = SOCKETMGR_ERROR_OK;

   ASSERT(NULL != sockState);

   /*
    * The socket must NOT be in listening mode and there are recv
    * requests pending.
    */
   ASSERT(!(sockState->isListening));
   ASSERT(NULL != sockState->recvQueue);

   /*
    * Dequeue
    */
   recvReq = sockState->recvQueue;
   sockState->recvQueue = recvReq->next;

   if (NULL != recvReq->timeoutEvent) {
      EventManager_Remove(recvReq->timeoutEvent);
      recvReq->timeoutEvent = NULL;
   }

   /*
    * Get the buffer length first.
    */
   pfnIoctlsocket(sockState->socket, FIONREAD, &len);
   if (len > 0) {
      buf = malloc(len);
      if (NULL == buf) {
         len = 0;
         error = SOCKETMGR_ERROR_OUT_OF_MEMORY;
      } else {
         pfnRecv(sockState->socket, buf, len, 0);
      }
   }

   ASSERT(NULL != recvReq->onReceived);
   (recvReq->onReceived)(sockState->socket,
                         buf,
                         len,
                         error,
                         recvReq->clientData);

   free(buf);
   free(recvReq);
} // SocketMgrOnRecv


/*
 *----------------------------------------------------------------------
 *
 * SocketMgrRecvTimeoutCallback --
 *
 *      This is the callback that gets called when the time for
 *      receiving incoming data is up.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

Bool
SocketMgrRecvTimeoutCallback(void *clientData)        // IN
{
   SocketRecvRequest *recvReq;
   SocketRecvRequest **prevReq;

   recvReq = (SocketRecvRequest *) clientData;
   ASSERT(NULL != recvReq);
   ASSERT(NULL != recvReq->socketState);
   ASSERT(NULL != recvReq->onReceived);

   (recvReq->onReceived)(recvReq->socketState->socket,
                         NULL,
                         0,
                         SOCKETMGR_ERROR_TIMEOUT,
                         recvReq->clientData);

   /*
    * Remove the request from the queue.
    */
   prevReq = &(recvReq->socketState->recvQueue);
   while ((recvReq != *prevReq) && (NULL != *prevReq)) {
      prevReq = &((*prevReq)->next);
   }

   /*
    * We must have found it in the queue. Otherwise, error.
    */
   ASSERT(recvReq == *prevReq);
   *prevReq = recvReq->next;

   free(recvReq);

   return TRUE;
} // SocketMgrRecvTimeoutCallback


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_CloseSocket --
 *
 *      This function closes a socket and its associated data structure.
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgr_CloseSocket(Socket sock)             // IN
{
   SocketState **sockState;
   SocketState *tempSockState;

   if (INVALID_SOCKET == sock) {
      return;
   }

   /*
    * The socket that is passed to this function must be managed by
    * SocketMgr.
    */
   sockState = SocketMgrSearchSocket(sock);
   if (NULL == *sockState) {
      ASSERT(0);
      return;
   }

   /*
    * Remove from the list.
    */
   tempSockState = *sockState;
   *sockState = tempSockState->next;

   /*
    * Close the socket.
    */
   pfnClosesocket(tempSockState->socket);

   /*
    * Finish all the pending requests.
    */
   if (tempSockState->isListening) {
      /*
       * A socket cannot send/recv in the listening mode.
       */
      ASSERT(NULL == tempSockState->recvQueue);
      ASSERT(NULL == tempSockState->sendQueue);

      while (NULL != tempSockState->acceptQueue) {
         SocketAcceptRequest *acceptReq;

         acceptReq = tempSockState->acceptQueue;
         tempSockState->acceptQueue = acceptReq->next;

         EventManager_Remove(acceptReq->timeoutEvent);

         if (NULL != acceptReq->onConnected) {
            (acceptReq->onConnected)(tempSockState->socket,
                                     SOCKETMGR_ERROR_DISCONNECTED,
                                     acceptReq->clientData);
         }
         free(acceptReq);
      } // while (NULL != tempSockState->acceptQueue)
   } else {
      /*
       * A socket cannot send/recv in the listening mode.
       */
      ASSERT(NULL == tempSockState->acceptQueue);

      while (NULL != tempSockState->recvQueue) {
         SocketRecvRequest *recvReq;

         recvReq = tempSockState->recvQueue;
         tempSockState->recvQueue = recvReq->next;

         EventManager_Remove(recvReq->timeoutEvent);

         if (NULL != recvReq->onReceived) {
            (recvReq->onReceived)(tempSockState->socket,
                                  NULL,
                                  0,
                                  SOCKETMGR_ERROR_DISCONNECTED,
                                  recvReq->clientData);
         }
         free(recvReq);
      } // while (NULL != tempSockState->recvQueue)

      while (NULL != tempSockState->sendQueue) {
         SocketSendRequest *sendReq;

         sendReq = tempSockState->sendQueue;
         tempSockState->sendQueue = sendReq->next;

         EventManager_Remove(sendReq->timeoutEvent);

         if (NULL != sendReq->onSent) {
            (sendReq->onSent)(tempSockState->socket,
                              sendReq->buf,
                              sendReq->len,
                              SOCKETMGR_ERROR_DISCONNECTED,
                              sendReq->pos,
                              sendReq->clientData);
         }
         free(sendReq);
      } // while (NULL != tempSockState->sendQueue)
   }

#ifdef _WIN32
   pfnWSACloseEvent(tempSockState->event);
#endif

   free(tempSockState);
} // SocketMgr_CloseSocket


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_GetSelectables --
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgr_GetSelectables(int flags,                         // IN
                         SocketSelectable **selectables,    // OUT
                         int *count)                        // OUT
{
   SocketState *sockState;
#if defined(_WIN32)
   long lNetworkEvents;
#endif

   ASSERT((NULL != selectables) && (NULL != count));

   *selectables = NULL;
   *count = 0;

   sockState = globalSocketList;
   while (NULL != sockState) {
      if (((flags & SOCKETMGR_IN)
            && ((NULL != sockState->acceptQueue)
            || (NULL != sockState->recvQueue)))
            || ((flags & SOCKETMGR_OUT)
            && (NULL != sockState->sendQueue))) {
         *count += 1;
      }
      sockState = sockState->next;
   }

   if (0 == *count) {
      return;
   }

   *selectables = malloc(*count * sizeof(SocketSelectable));
   if (NULL == *selectables) {
      *count = 0;
      return;
   }

   *count = 0;
   sockState = globalSocketList;
   while (NULL != sockState) {
#if defined(_WIN32)
      /*
       * Always accept socket closure event.
       */
      lNetworkEvents = FD_CLOSE;
#endif

      if (((flags & SOCKETMGR_IN)
            && ((NULL != sockState->acceptQueue)
            || (NULL != sockState->recvQueue)))
            || ((flags & SOCKETMGR_OUT)
            && (NULL != sockState->sendQueue))) {
#if defined(_WIN32)
         if (sockState->isListening) {
            /*
            * A socket cannot send/recv while in the listening mode.
            */
            ASSERT(NULL == sockState->recvQueue);
            ASSERT(NULL == sockState->sendQueue);

            if (NULL != sockState->acceptQueue) {
               lNetworkEvents |= FD_ACCEPT;
            }
         } else {
            /*
            * A socket cannot send/recv while in the listening mode.
            */
            ASSERT(NULL == sockState->acceptQueue);

            if (NULL != sockState->recvQueue) {
               lNetworkEvents |= FD_READ;
            }
            if (NULL != sockState->sendQueue) {
               lNetworkEvents |= FD_WRITE;
            }
         }

         ASSERT(0 != lNetworkEvents);
         ASSERT(WSA_INVALID_EVENT != sockState->event);

         pfnWSAResetEvent(sockState->event);
         pfnWSAEventSelect(sockState->socket,
                           sockState->event,
                           lNetworkEvents);

         (*selectables)[*count] = sockState->event;
#else //  defined(_WIN32)
         (*selectables)[*count] = sockState->socket;
#endif

         *count += 1;
      }
      sockState = sockState->next;
   }
} // SocketMgr_GetSelectables


/*
 *----------------------------------------------------------------------
 *
 * SocketMgr_ProcessSelectable --
 *
 * Results:
 *      
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SocketMgr_ProcessSelectable(SocketSelectable selectable,    // IN
                            int flags)                      // IN
{
   SocketState **sockState;
#if defined(_WIN32)
   int error;
   WSANETWORKEVENTS wsaNetworkEvents;
#endif

   sockState = SocketMgrSearchSelectable(selectable);
   if (NULL == *sockState) {
      return;
   }

#if defined(_WIN32)
   pfnWSAResetEvent((*sockState)->event);
   error = pfnWSAEnumNetworkEvents((*sockState)->socket,
                                   selectable,
                                   &wsaNetworkEvents);
   if (0 != error) {
      return;
   }
#endif

#if defined(_WIN32)
   if ((FD_ACCEPT & wsaNetworkEvents.lNetworkEvents)
         || ((FD_CLOSE & wsaNetworkEvents.lNetworkEvents)
         && (NULL != (*sockState)->acceptQueue))) {
#else
   if ((flags & SOCKETMGR_IN)
         && (NULL != (*sockState)->acceptQueue)) {
#endif
      SocketMgrOnAccept(*sockState);
   }

#if defined(_WIN32)
   if ((FD_READ & wsaNetworkEvents.lNetworkEvents)
         || ((FD_CLOSE & wsaNetworkEvents.lNetworkEvents)
         && (NULL != (*sockState)->recvQueue))) {
#else
   if ((flags & SOCKETMGR_IN)
         && (NULL != (*sockState)->recvQueue)) {
#endif
      SocketMgrOnRecv(*sockState);
   }

#if defined(_WIN32)
   if ((FD_WRITE & wsaNetworkEvents.lNetworkEvents)
         || ((FD_CLOSE & wsaNetworkEvents.lNetworkEvents)
         && (NULL != (*sockState)->sendQueue))) {
#else
   if ((flags & SOCKETMGR_OUT)
         && (NULL != (*sockState)->sendQueue)) {
#endif
      SocketMgrOnSend(*sockState);
   }
} // SocketMgr_ProcessSelectable
