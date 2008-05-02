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
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "vmware.h"
#include "vm_version.h"
#include "str.h"
#include "hostinfo.h"
#include "util.h"
#include "dbllnklst.h"
#include "eventManager.h"
#include "SLPv2Private.h"
#include "guestInfo.h"
#include "netutil.h"
#include "guestInfoInt.h"

#include "vixOpenSource.h"
#include "syncEvent.h"
#include "foundryThreads.h"
#include "vixCommands.h"
#include "foreignVMToolsDaemon.h"



#ifdef _WIN32
#define ASOCK_ENOTCONN          WSAENOTCONN
#define ASOCK_ENOTSOCK          WSAENOTSOCK
#define ASOCK_ECONNECTING       WSAEWOULDBLOCK
#define ASOCK_EWOULDBLOCK       WSAEWOULDBLOCK
#else
#define ASOCK_ENOTCONN          ENOTCONN
#define ASOCK_ENOTSOCK          ENOTSOCK
#define ASOCK_ECONNECTING       EINPROGRESS
#define ASOCK_EWOULDBLOCK       EWOULDBLOCK
#endif

#define SLPV2_DEFAULT_SCOPE_NAME "DEFAULT"

#if defined(__FreeBSD__) && !defined(_SOCKLEN_T_DECLARED)
typedef int socklen_t;
#endif




/*
 *-----------------------------------------------------------------------------
 *
 * Windows Procedure Pointers --
 *
 * We cannot link with winsock, since the DLL may not be present in all guests.
 * That would create a dll link problem that would prevent the tools from loading.
 * Instead, we try to get the procedure pointer ourselves at runtime, and quietly 
 * fail if it is not there.
 *-----------------------------------------------------------------------------
 */
#ifdef _WIN32
typedef int (WSAAPI *WSAStartupProcType)(WORD wVersionRequested,
                                         LPWSADATA lpWSAData);

typedef int (WSAAPI *WSAGetLastErrorProcType)(void);

typedef u_long (WSAAPI *NtohlProcType)(u_long netlong);

typedef u_long (WSAAPI *HtonlProcType)(u_long hostlong);

typedef u_short (WSAAPI *HtonsProcType)(u_short hostshort);

typedef int (WSAAPI *ClosesocketProcType)(SOCKET s);

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

typedef int (WSAAPI *SelectProcType)(int nfds,
                                     fd_set* readfds,
                                     fd_set* writefds,
                                     fd_set* exceptfds,
                                     const struct timeval* timeout);

typedef int (WSAAPI *IoctlsocketProcType)(SOCKET s,
                                          long cmd,
                                          u_long FAR * argp);

typedef int (WSAAPI *SendToProcType)(SOCKET s,
                                     const char* buf,
                                     int len,
                                     int flags,
                                     const struct sockaddr* to,
                                     int tolen);

typedef int (WSAAPI *SendSockOptProcType)(SOCKET s,
                                          int level, 
                                          int optname, 
                                          const void *optval,
                                          socklen_t optlen);

typedef int (WSAAPI *WSAIoctlProcType)(SOCKET s,
                                          DWORD dwIoControlCode,
                                          LPVOID lpvInBuffer,
                                          DWORD cbInBuffer,
                                          LPVOID lpvOutBuffer,
                                          DWORD cbOutBuffer,
                                          LPDWORD lpcbBytesReturned,
                                          LPWSAOVERLAPPED lpOverlapped,
                                          LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

typedef int (WSAAPI *WSAFDIsSetProcType)(SOCKET s,
                                         fd_set* set);

typedef int (WSAAPI *RecvfromProcType)(SOCKET s,
                                       char* buf,
                                       int len,
                                       int flags,
                                       struct sockaddr* from,
                                       int* fromlen);


static WSAStartupProcType           VMToolsNet_WSAStartup = NULL;
static WSAGetLastErrorProcType      VMToolsNet_GetLastError = NULL;
static NtohlProcType                VMToolsNet_ntohl = NULL;
static HtonlProcType                VMToolsNet_htonl = NULL;
static HtonsProcType                VMToolsNet_htons = NULL;
static ClosesocketProcType          VMToolsNet_CloseSocket = NULL;
static SocketProcType               VMToolsNet_Socket = NULL;
static ListenProcType               VMToolsNet_Listen = NULL;
static BindProcType                 VMToolsNet_Bind = NULL;
static AcceptProcType               VMToolsNet_Accept = NULL;
static SendProcType                 VMToolsNet_Send = NULL;
static RecvProcType                 VMToolsNet_Recv = NULL;
static SelectProcType               VMToolsNet_Select = NULL;
static IoctlsocketProcType          VMToolsNet_IoctlSocket = NULL;
static SendToProcType               VMToolsNet_Sendto = NULL;
static SendSockOptProcType          VMToolsNet_SetSockOpt = NULL;
static WSAIoctlProcType             VMToolsNet_WSAIoctl = NULL;
static WSAFDIsSetProcType           VMToolsNet_FDIsSet = NULL;
static RecvfromProcType             VMToolsNet_Recvfrom = NULL;


// ifdef _WIN32
#else
#define VMToolsNet_GetLastError()           errno
#define VMToolsNet_ntohl                    ntohl
#define VMToolsNet_htonl                    htonl
#define VMToolsNet_htons                    htons
#define VMToolsNet_Socket                   socket
#define VMToolsNet_Listen                   listen
#define VMToolsNet_Bind                     bind
#define VMToolsNet_Accept                   accept
#define VMToolsNet_Send                     send
#define VMToolsNet_Recv                     recv
#define VMToolsNet_Select                   select
#define VMToolsNet_CloseSocket              close
#define VMToolsNet_IoctlSocket              ioctl
#define VMToolsNet_Sendto                   sendto
#define VMToolsNet_SetSockOpt               setsockopt
#define VMToolsNet_SetSockOpt               setsockopt
#define VMToolsNet_FDIsSet                  FD_ISSET
#define VMToolsNet_Recvfrom                 recvfrom

#endif 


Bool ForeignToolsMakeNonBlocking(int fd);

static Bool ForeignToolsSocketBind(int fd,
                                   unsigned int ip,
                                   unsigned short port);

static Bool ForeignToolsAcceptConnection(int tcpListenerSocket);

static void ForeignToolsProcessUDP(int tcpListenerSocket);

static void ForeignToolsReadRequest(ForeignVMToolsConnection *connectionState);

static int VixSocketListenerPort = VIX_TOOLS_SOCKET_PORT;
static int SLPv2SocketListenerPort = SLPV2_HIGHPORT; // SLPV2_PORT;

static char globalHostName[512];

static char globalSLPv2ServiceProperties[1024];

static int udpListenerSocket = -1;

#define VIX_SLPV2_PROPERTY_PORT "port"


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignTools_InitializeNetworking --
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
ForeignTools_InitializeNetworking(void)
{
   Bool success = TRUE;
   const char *globalHostInfoStr;
   char *ipAddressStr = NULL;
   char *destPtr;
   char *endDestPtr;
   NicInfo nicInfo;
#ifdef _WIN32
   HMODULE hWs2_32 = NULL;
   WSADATA wsaData;
   WORD versionRequested = MAKEWORD(2, 0);
#endif


   globalHostInfoStr = Hostinfo_NameGet();
   if (NULL == globalHostInfoStr) {
      globalHostName[0] = 0;
   } else {
      Str_Strcpy(globalHostName, globalHostInfoStr, sizeof(globalHostName));
   }

#ifdef _WIN32
   hWs2_32 = LoadLibrary(_T("ws2_32.dll"));
   if (NULL == hWs2_32) {
      goto abort;
   }

   VMToolsNet_WSAStartup = (WSAStartupProcType) GetProcAddress(hWs2_32, "WSAStartup");
   if (NULL == VMToolsNet_WSAStartup) {
      goto abort;
   }

   VMToolsNet_GetLastError = (WSAGetLastErrorProcType) GetProcAddress(hWs2_32, "WSAGetLastError");
   if (NULL == VMToolsNet_GetLastError) {
      goto abort;
   }

   VMToolsNet_ntohl = (NtohlProcType) GetProcAddress(hWs2_32, "ntohl");
   if (NULL == VMToolsNet_ntohl) {
      goto abort;
   }

   VMToolsNet_htonl = (HtonlProcType) GetProcAddress(hWs2_32, "htonl");
   if (NULL == VMToolsNet_htonl) {
      goto abort;
   }

   VMToolsNet_htons = (HtonsProcType) GetProcAddress(hWs2_32, "htons");
   if (NULL == VMToolsNet_htons) {
      goto abort;
   }

   VMToolsNet_CloseSocket = (ClosesocketProcType) GetProcAddress(hWs2_32, "closesocket");
   if (NULL == VMToolsNet_CloseSocket) {
      goto abort;
   }

   VMToolsNet_Socket = (SocketProcType) GetProcAddress(hWs2_32, "socket");
   if (NULL == VMToolsNet_Socket) {
      goto abort;
   }

   VMToolsNet_Listen = (ListenProcType) GetProcAddress(hWs2_32, "listen");
   if (NULL == VMToolsNet_Listen) {
      goto abort;
   }

   VMToolsNet_Bind = (BindProcType) GetProcAddress(hWs2_32, "bind");
   if (NULL == VMToolsNet_Bind) {
      goto abort;
   }

   VMToolsNet_Accept = (AcceptProcType) GetProcAddress(hWs2_32, "accept");
   if (NULL == VMToolsNet_Accept) {
      goto abort;
   }

   VMToolsNet_Send = (SendProcType) GetProcAddress(hWs2_32, "send");
   if (NULL == VMToolsNet_Send) {
      goto abort;
   }

   VMToolsNet_Recv = (RecvProcType) GetProcAddress(hWs2_32, "recv");
   if (NULL == VMToolsNet_Recv) {
      goto abort;
   }

   VMToolsNet_Select = (SelectProcType) GetProcAddress(hWs2_32, "select");
   if (NULL == VMToolsNet_Select) {
      goto abort;
   }

   VMToolsNet_IoctlSocket = (IoctlsocketProcType) GetProcAddress(hWs2_32, "ioctlsocket");
   if (NULL == VMToolsNet_IoctlSocket) {
      return FALSE;
   }

   VMToolsNet_Sendto = (SendToProcType) GetProcAddress(hWs2_32, "sendto");
   if (NULL == VMToolsNet_Sendto) {
      return FALSE;
   }

   VMToolsNet_SetSockOpt = (SendSockOptProcType) GetProcAddress(hWs2_32, "setsockopt");
   if (NULL == VMToolsNet_SetSockOpt) {
      return FALSE;
   }

   VMToolsNet_WSAIoctl = (WSAIoctlProcType) GetProcAddress(hWs2_32, "WSAIoctl");
   if (NULL == VMToolsNet_WSAIoctl) {
      return FALSE;
   }

   VMToolsNet_FDIsSet = (WSAFDIsSetProcType) GetProcAddress(hWs2_32, "__WSAFDIsSet");
   if (NULL == VMToolsNet_FDIsSet) {
      return FALSE;
   }

   VMToolsNet_Recvfrom = (RecvfromProcType) GetProcAddress(hWs2_32, "recvfrom");
   if (NULL == VMToolsNet_Recvfrom) {
      return FALSE;
   }


   if (0 != VMToolsNet_WSAStartup(versionRequested, &wsaData)) {
      goto abort;
   }

   NetUtil_LoadIpHlpApiDll();
#endif   // _WIN32

   /*
    * Build up a description of this host/guest that we can broadcast
    * over SLPv2.
    */
   destPtr = globalSLPv2ServiceProperties;
   endDestPtr = globalSLPv2ServiceProperties + sizeof(globalSLPv2ServiceProperties);

   ipAddressStr = NetUtil_GetPrimaryIP();
   if (NULL == ipAddressStr) {
      ipAddressStr = Util_SafeStrdup("");
   }
   destPtr += Str_Snprintf(destPtr,
                           endDestPtr - destPtr,
                           "%s=%s;",
                           VIX_SLPV2_PROPERTY_IP_ADDR,
                           ipAddressStr);
   destPtr += Str_Snprintf(destPtr,
                           endDestPtr - destPtr,
                           "%s=%d;",
                           VIX_SLPV2_PROPERTY_PORT,
                           VixSocketListenerPort);

   /*
    * Find out how big the IP_ADAPTER_INFO table needs to be. 
    */
   success = GuestInfoGetNicInfo(&nicInfo);
   if (success) {
      NicEntry *nicEntryPtr = NULL;
      DblLnkLst_Links *nicEntryLink;
    
      DblLnkLst_ForEach(nicEntryLink, &nicInfo.nicList) {
         nicEntryPtr = DblLnkLst_Container(nicEntryLink, NicEntry, links);
         destPtr += Str_Snprintf(destPtr,
                                 endDestPtr - destPtr,
                                 "%s=%s;",
                                 VIX_SLPV2_PROPERTY_MAC_ADDR,
                                 nicEntryPtr->nicEntryProto.macAddress);
      } 

      GuestInfo_FreeDynamicMemoryInNicInfo(&nicInfo);
   }

   free(ipAddressStr);
   return(TRUE);

#ifdef _WIN32
abort:
   return(FALSE);
#endif
} // ForeignTools_InitializeNetworking


/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsMakeNonBlocking --
 *
 *      Make the specified socket non-blocking if it isn't already.
 *
 * Results:
 *      TRUE if the operation succeeded, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
ForeignToolsMakeNonBlocking(int fd)  // IN
{
#ifdef _WIN32
   int retval;
   u_long argp = 1; /* non-zero => enable non-blocking mode */

   retval = VMToolsNet_IoctlSocket(fd, FIONBIO, &argp);
   if (retval != 0) {
      return FALSE;
   }

#else
   int flags;

   if ((flags = fcntl(fd, F_GETFL)) < 0) {
      return FALSE;
   }

   if (!(flags & O_NONBLOCK) && (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)) {
      return FALSE;
   }
#endif
   return TRUE;
} // ForeignToolsMakeNonBlocking



/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsSocketBind --
 *
 *      This is an internal routine that binds a socket to a port.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon failure.
 *
 * Side effects:
 *      Socket is bound to a particular port.
 *
 *----------------------------------------------------------------------------
 */

Bool
ForeignToolsSocketBind(int fd,                // IN
                       unsigned int ip,       // IN
                       unsigned short port)   // IN
{
   struct sockaddr_in local_addr = { 0 };
   int error = 0;
   int reuse = port != 0;

#ifndef _WIN32
   /*
    * Don't ever use SO_REUSEADDR on Windows; it doesn't mean what you think
    * it means.
    */
   if (VMToolsNet_SetSockOpt(fd, SOL_SOCKET, SO_REUSEADDR,
                  (const void *) &reuse, sizeof(reuse)) != 0) {
      error = VMToolsNet_GetLastError();
   }
#endif

#ifdef _WIN32
   /*
    * Always set SO_EXCLUSIVEADDRUSE on Windows, to prevent other applications
    * from stealing this socket. (Yes, Windows is that stupid).
    */
   {
      int exclusive = 1;
      if (VMToolsNet_SetSockOpt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     (const void *) &exclusive, sizeof(exclusive)) != 0) {
         error = VMToolsNet_GetLastError();
      }
   }
#endif

   /*
    * Bind to a port
    */
   local_addr.sin_family = AF_INET;
   local_addr.sin_addr.s_addr = VMToolsNet_htonl(ip);
   local_addr.sin_port = VMToolsNet_htons(port);

   if (VMToolsNet_Bind(fd, (struct sockaddr *) &local_addr, sizeof(local_addr)) != 0) {
      error = VMToolsNet_GetLastError();
      goto error;
   }

   return TRUE;

error:
   return FALSE;
} // ForeignToolsSocketBind



/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsSelectLoop --
 *
 *       This is the main loop for the select thread.
 *
 * Results:
 *       None
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

void
ForeignToolsSelectLoop(FoundryWorkerThread *threadState)    // IN
{
   Bool success;
   int retval;
#ifdef _WIN32
   struct fd_set rwfds;
   struct fd_set exceptfds;
#else
   fd_set rwfds;
   fd_set exceptfds;
#endif
   ForeignVMToolsConnection *connectionState = NULL;
   int numFDs;
   int bcast = 1;
   int tcpListenerSocket = -1;


   if (NULL == threadState) {
      ASSERT(0);
      return;
   }

   /*
    * Create the socket we will listen on.
    */
   tcpListenerSocket = VMToolsNet_Socket(AF_INET, SOCK_STREAM, 0);
   if (tcpListenerSocket == -1) {
      goto abort;
   }
   //success = ForeignToolsMakeNonBlocking(tcpListenerSocket);
   success = ForeignToolsSocketBind(tcpListenerSocket, 
                                    INADDR_ANY, 
                                    VixSocketListenerPort);
   if (!success) {
      goto abort;
   }
   if (VMToolsNet_Listen(tcpListenerSocket, 5) != 0) {
      goto abort;
   }
   udpListenerSocket = VMToolsNet_Socket(AF_INET, SOCK_DGRAM, 0);
   if (udpListenerSocket == -1) {
      goto abort;
   }
   //success = ForeignToolsMakeNonBlocking(tcpListenerSocket);
   success = ForeignToolsSocketBind(udpListenerSocket, 
                                    INADDR_ANY, 
                                    SLPv2SocketListenerPort);
   if (!success) {
      goto abort;
   }
   VMToolsNet_SetSockOpt(udpListenerSocket, 
                         SOL_SOCKET, 
                         SO_BROADCAST,
                         (const void *) &bcast, 
                         sizeof(bcast));

#ifdef _WIN32
      {
         /*
          * On Windows, sending a UDP packet to a host may result in
          * a "connection reset by peer" message to be sent back by
          * the remote machine.  If that happens, our UDP socket becomes
          * useless.  We can disable this with the SIO_UDP_CONNRESET
          * ioctl option.
          */
         DWORD dwBytesReturned = 0;
         BOOL bNewBehavior = FALSE;
         DWORD status;
         status = VMToolsNet_WSAIoctl(udpListenerSocket, 
                                      SIO_UDP_CONNRESET,
                                      &bNewBehavior, 
                                      sizeof(bNewBehavior),
                                      NULL, 
                                      0, 
                                      &dwBytesReturned,
                                      NULL, 
                                      NULL);
         if (SOCKET_ERROR == status) {
         }
      }
#endif


   /*
    * This is the main select thread loop.
    */
   while (!(threadState->stopThread)) {
      /*
       * Listen for activity on any socket we care about.
       */
      FD_ZERO(&rwfds);
      FD_ZERO(&exceptfds);
      FD_SET(tcpListenerSocket, &rwfds);
      FD_SET(tcpListenerSocket, &exceptfds);
      FD_SET(udpListenerSocket, &rwfds);
      FD_SET(udpListenerSocket, &exceptfds);
      numFDs = 2;

      VIX_ENTER_LOCK(&globalLock);
      connectionState = activeConnectionList;
      while (NULL != connectionState) {
         FD_SET(connectionState->socket, &rwfds);
         FD_SET(connectionState->socket, &exceptfds);
         numFDs += 1;
         connectionState = connectionState->next;
      }
      VIX_LEAVE_LOCK(&globalLock);

      retval = VMToolsNet_Select(numFDs, &rwfds, NULL, &exceptfds, NULL);

      if (threadState->stopThread) {
         break;
      }

      /*
       * No sockets were ready or else we were interrupted by signal.
       * Loop and retry.
       */
      if (retval <= 0) {
         continue;
      }

      if (VMToolsNet_FDIsSet(tcpListenerSocket, &rwfds)) {
         success = ForeignToolsAcceptConnection(tcpListenerSocket);
         if (!success) {
            break;
         }
      }

      if (VMToolsNet_FDIsSet(udpListenerSocket, &rwfds)) {
         ForeignToolsProcessUDP(udpListenerSocket);
      }

      VIX_ENTER_LOCK(&globalLock);
      connectionState = activeConnectionList;
      while (NULL != connectionState) {
         ForeignVMToolsConnection *nextConnection;

         /*
          * Save the next connection in case we close this connection.
          */
         nextConnection = connectionState->next;

         if (VMToolsNet_FDIsSet(connectionState->socket, &rwfds)) {
            ForeignToolsReadRequest(connectionState);
         } else if (VMToolsNet_FDIsSet(connectionState->socket, &exceptfds)) {
            ForeignToolsCloseConnection(connectionState, SHUTDOWN_FOR_PEER_DISCONNECT);
         }

         connectionState = nextConnection;
      }
      VIX_LEAVE_LOCK(&globalLock);
   } // while (!(threadState->stopThread))

abort:
   if (tcpListenerSocket != -1) {
      VMToolsNet_CloseSocket(tcpListenerSocket);
   }
   if (udpListenerSocket != -1) {
      VMToolsNet_CloseSocket(udpListenerSocket);
      udpListenerSocket = -1;
   }   
} // ForeignToolsSelectLoop


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsAcceptConnection --
 *
 *
 * Results:
 *       None
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

Bool
ForeignToolsAcceptConnection(int tcpListenerSocket)   // IN
{
   int retval;
   struct sockaddr remoteAddr;
   socklen_t remoteAddrLen = sizeof remoteAddr;
   ForeignVMToolsConnection *connectionState = NULL;


   retval = VMToolsNet_Accept(tcpListenerSocket, &remoteAddr, &remoteAddrLen);
   if (retval == -1) {
      retval = VMToolsNet_GetLastError();
      if (retval == ASOCK_EWOULDBLOCK) {
         return TRUE; // Ignore this error, so return success
#ifndef _WIN32
      } else if (retval == ECONNABORTED) {
         return TRUE; // Ignore this error, so return success
#endif
      } else {
         return FALSE; // OK, now this is an error.
      }
   }

         
   VIX_ENTER_LOCK(&globalLock);
         
   /*
    * Allocate some state for the connection
    */
   connectionState = Util_SafeCalloc(1, sizeof *connectionState);
   connectionState->socket = retval;

   connectionState->prev = NULL;
   connectionState->next = activeConnectionList;
   if (NULL != activeConnectionList) {
      activeConnectionList->prev = connectionState;
   }
   activeConnectionList = connectionState;

   VIX_LEAVE_LOCK(&globalLock);

   return(TRUE);
} // ForeignToolsAcceptConnection


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsReadRequest --
 *
 *
 * Results:
 *       None
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

void
ForeignToolsReadRequest(ForeignVMToolsConnection *connectionState)   // IN
{
   VixError err = VIX_OK;
   int result;

   /*
    * Read the message header.
    */
   result = VMToolsNet_Recv(connectionState->socket, 
                            (char *) &(connectionState->requestHeader), 
                            sizeof(connectionState->requestHeader),
                            0);
   if (result <= 0) {
      ForeignToolsCloseConnection(connectionState, SHUTDOWN_FOR_PEER_DISCONNECT);
      goto abort;
   }

   /*
    * Sanity check the request header.
    */
   err = VixMsg_ValidateRequestMsg(&(connectionState->requestHeader), result);
   if (VIX_OK != err) {
      goto abort;
   }

   connectionState->completeRequest 
       = Util_SafeMalloc(connectionState->requestHeader.commonHeader.totalMessageLength);
   memcpy(connectionState->completeRequest,
          &(connectionState->requestHeader),
          result);

   /*
    * If this request has a variable-sized part, like a body or a user credential,
    * then start reading that. Otherwise, if the request only has a fixed-size header, 
    * then we can start processing it now.
    */
   if (connectionState->requestHeader.commonHeader.totalMessageLength > result) {
      result = VMToolsNet_Recv(connectionState->socket, 
                              connectionState->completeRequest + result, 
                              connectionState->requestHeader.commonHeader.totalMessageLength - result,
                              0);
      if (result <= 0) {
         ForeignToolsCloseConnection(connectionState, SHUTDOWN_FOR_PEER_DISCONNECT);
         goto abort;
      }
   }

   ForeignToolsProcessMessage(connectionState);

abort:
   if (VIX_OK != err) {
      ForeignToolsSendResponse(connectionState,
                               &(connectionState->requestHeader),
                               0, // responseBodyLength
                               NULL, // responseBody
                               err,
                               0, // additionalError
                               0); // responseFlags;
   }
} // ForeignToolsReadRequest


/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsSendResponse --
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
ForeignToolsSendResponse(ForeignVMToolsConnection *connectionState,  // IN
                         VixCommandRequestHeader *requestHeader,     // IN
                         size_t responseBodyLength,                  // IN
                         void *responseBody,                         // IN
                         VixError error,                             // IN
                         uint32 additionalError,                     // IN
                         uint32 responseFlags)                       // IN
{
   int result = 0;
   VixCommandResponseHeader *responseHeader = NULL;
   size_t totalMessageSize;

   responseHeader = VixMsg_AllocResponseMsg(requestHeader,
                                            error,
                                            additionalError,
                                            responseBodyLength,
                                            responseBody,
                                            &totalMessageSize);

   responseHeader->responseFlags |= responseFlags;

   result = VMToolsNet_Send(connectionState->socket, 
                            (const char *) responseHeader,
                            totalMessageSize, 
                            0);
   if (result <= 0) {
   }

   free(responseHeader);
} // ForeignToolsSendResponse


/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsSendResponseUsingTotalMessage --
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
ForeignToolsSendResponseUsingTotalMessage(ForeignVMToolsConnection *connectionState, // IN
                                          VixCommandRequestHeader *requestHeader,    // IN
                                          size_t totalMessageSize,                      // IN
                                          void *totalMessage,                        // IN
                                          VixError error,                            // IN
                                          uint32 additionalError,                    // IN
                                          uint32 responseFlags)                      // IN
{
   int result = 0;
   VixCommandResponseHeader *responseHeader;

   if ((totalMessageSize <= 0) || (NULL == totalMessage)) {
      return;
   }

   responseHeader = (VixCommandResponseHeader *) totalMessage;
   VixMsg_InitResponseMsg(responseHeader,
                          requestHeader,
                          error,
                          additionalError,
                          totalMessageSize);

   responseHeader->responseFlags |= responseFlags;

   result = VMToolsNet_Send(connectionState->socket, 
                            (const char *) responseHeader,
                            totalMessageSize, 
                            0);
   if (result <= 0) {
   }
} // ForeignToolsSendResponseUsingTotalMessage


/*
 *-----------------------------------------------------------------------------
 *
 * ForeignToolsProcessUDP --
 *
 *
 * Results:
 *       None
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */

void
ForeignToolsProcessUDP(int udpListenerSocket)   // IN
{
   struct sockaddr_in clientAddr;
   int clientAddrLen = sizeof(clientAddr);
   int actualPacketLength = 0;
   char receiveBuffer[2048];
   struct SLPv2_Parse *parser = NULL;
   Bool success;
   uint16 xid;
   Bool match;
   char urlBuffer[1024];
   char *urlList[] = { urlBuffer };
   char *replyPacket = NULL;
   int replyPacketSize = 0;

   actualPacketLength = VMToolsNet_Recvfrom(udpListenerSocket, 
                                             receiveBuffer, 
                                             sizeof(receiveBuffer),
                                             0,
                                             (struct sockaddr *) &clientAddr, 
                                             &clientAddrLen);
   if (actualPacketLength <= 0) {
      goto abort;
   }


   parser = SLPv2MsgParser_Init();
   success = SLPv2MsgParser_Parse(parser, receiveBuffer, actualPacketLength);
   if (!success) {
      goto abort;
   }

   /*
    * First, check if this is a general request to find all hosts
    * runn a particular type of service.
    */
   match = SLPv2MsgParser_ServiceRequestMatch(parser,
                                              "", // previous responder list
                                              VIX_SLPV2_SERVICE_NAME_TOOLS_SERVICE, // service
                                              SLPV2_DEFAULT_SCOPE_NAME, // scope
                                              "", // LDAPv3 predicate
                                              &xid);
   if (match) {
       Str_Sprintf(urlBuffer, 
                   sizeof urlBuffer, 
                   "%s://%s/", 
                   VIX_SLPV2_SERVICE_NAME_TOOLS_SERVICE,
                   globalHostName);
       success = SLPv2MsgAssembler_ServiceReply(&replyPacket,
                                                &replyPacketSize,
                                                xid,   // xid from service request msg
                                                "en",  // language
                                                0,     // error code
                                                1,     // URL count
                                                urlList);
      /*
       * Otherwise, check if this is a specific request to find all attributes
       * of this specific service instance.
       */
      } else {
         match = SLPv2MsgParser_AttributeRequestMatch(parser,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      NULL,
                                                      &xid);
         if (match) {
            success = SLPv2MsgAssembler_AttributeReply(&replyPacket,
                                                       &replyPacketSize,
                                                       xid,         // xid from request
                                                       "en",        // language
                                                       0,           // error code
                                                       globalSLPv2ServiceProperties); // attributes
         } // if (match)
      }

      /*
       * If we have a response to the request, then send it and stop looking
       * for more responses.
       */
      if (NULL != replyPacket) {
         VMToolsNet_Sendto(udpListenerSocket, 
                           replyPacket, 
                           replyPacketSize,
                           0, // flags
                           (struct sockaddr *) &clientAddr, 
                           clientAddrLen);
      }

abort:
   free(replyPacket);
   SLPv2MsgParser_Destroy(parser);
} // ForeignToolsProcessUDP


/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsCloseConnection --
 *
 *      Closes the socket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------------
 */

void
ForeignToolsCloseConnection(ForeignVMToolsConnection *connectionState,  // IN
                            FoundryDisconnectReason reason)             // IN
{
   ForeignVMToolsConnection *targetConnection = NULL;
   Bool holdingLock = FALSE;
   ForeignVMToolsCommand *command;
   ForeignVMToolsCommand *nextCommand;


   if (NULL == connectionState) {
      return;
   }

   VIX_ENTER_LOCK(&globalLock);
   holdingLock = TRUE;

   /*
    * It's possible that data on a connection arrives late, after we have
    * closed it. In that case, VMAutomationReceiveMessage will correctly 
    * not process the data. But, be careful that we don't try to close
    * the connection again.
    */
   targetConnection = activeConnectionList;
   while (NULL != targetConnection) {
       if (targetConnection == connectionState) {
           break;
       }
      targetConnection = targetConnection->next;
   }
   if (NULL == targetConnection) {
      goto abort;
   }

   /*
    * Discard any commands we received on this connection.
    */
   command = globalCommandList;
   while (NULL != command) {
      nextCommand = command->next;
      if (connectionState == command->connection) {
         if (SHUTDOWN_FOR_PEER_DISCONNECT != reason) {
            ForeignToolsSendResponse(connectionState,
                                     &(connectionState->requestHeader),
                                     0, // responseBodyLength
                                     NULL, // responseBody
                                     VIX_OK, // err,
                                     0, // additionalError
                                     0); // responseFlags
         }

         ForeignToolsDiscardCommand(command);
      }
      
      command = nextCommand;
   } // while (NULL != command)

   /*
    * Remove the connection from the global list.
    */
   if (NULL != connectionState->prev) {
      connectionState->prev->next = connectionState->next;
   } else {
      activeConnectionList = connectionState->next;
   }
   if (NULL != connectionState->next) {
      connectionState->next->prev = connectionState->prev;
   }

   VIX_LEAVE_LOCK(&globalLock);
   holdingLock = FALSE;

   if (connectionState->socket >= 0) {
      VMToolsNet_CloseSocket(connectionState->socket);
   }

   //if (NULL != connectionState->sessionKey) {
     // CryptoKey_Free(connectionState->sessionKey);
   //}

   free(connectionState);

abort:
   if (holdingLock) {
      VIX_LEAVE_LOCK(&globalLock);
   }
} // ForeignToolsCloseConnection


/*
 *----------------------------------------------------------------------------
 *
 * ForeignToolsWakeSelectThread --
 *
 *      Closes the socket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------------
 */

void
ForeignToolsWakeSelectThread(void)    // IN
{
   struct sockaddr_in localAddr = { 0 };
   char packet[2];

   if (-1 == udpListenerSocket) {
      return;
   }

   localAddr.sin_family = AF_INET;
   localAddr.sin_addr.s_addr = VMToolsNet_htonl(INADDR_ANY);
   localAddr.sin_port = VMToolsNet_htons(SLPv2SocketListenerPort);

   VMToolsNet_Sendto(udpListenerSocket, 
                     packet, 
                     sizeof(packet),
                     0, // flags
                     (struct sockaddr *) &localAddr, 
                     sizeof(localAddr));
} // ForeignToolsWakeSelectThread

