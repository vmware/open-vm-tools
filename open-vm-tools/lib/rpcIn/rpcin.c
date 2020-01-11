/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
 * rpcin.c --
 *
 *    Remote Procedure Call between VMware and guest applications
 *    C implementation.
 *
 *    This module implements the guest=>host direction only.
 *    The in and out modules are separate since some applications (e.g.
 *    drivers that want to do RPC-based logging) only want/need/can have the
 *    out direction (the in direction is more complicated).
 */

#ifdef __KERNEL__
#   include "kernelStubs.h"
#else
#   include <stdio.h>
#   include <string.h>
#   include <stdlib.h>
#   include <stdarg.h>
#   if defined(_WIN32) && defined(_MSC_VER)
#      include <windows.h>
#   endif
#   include "debug.h"
#   include "str.h"
#   include "strutil.h"
#endif

#include "vm_basic_types.h"

#if ((defined(__linux__) && !defined(USERWORLD)) || defined(_WIN32)) && \
    defined(VMTOOLS_USE_GLIB)
#define VMTOOLS_USE_VSOCKET
#else
#undef  VMTOOLS_USE_VSOCKET
#endif

#include <ctype.h>

#if defined(VMTOOLS_USE_VSOCKET)
#  include <glib.h>
#  include "poll.h"
#  include "asyncsocket.h"
#  include "vmci_defs.h"
#include "dataMap.h"
#include "vmware/guestrpc/tclodefs.h"
#if defined(__linux__)
#include <arpa/inet.h>
#else
#include <winsock2.h>
#endif
#endif

#if defined(VMTOOLS_USE_GLIB)
#  include "vmware/tools/guestrpc.h"
#  include "vmware/tools/utils.h"
#endif

#include "vmware.h"
#include "message.h"
#include "rpcin.h"
#include "util.h"
#include "system.h"

#if !defined(VMTOOLS_USE_GLIB)
#include "eventManager.h"

/* Which event queue should RPC events be added to? */
static DblLnkLst_Links *gTimerEventQueue;

/*
 * The RpcIn object
 */

/* The list of TCLO command callbacks we support */
typedef struct RpcInCallbackList {
   const char *name;
   size_t length; /* Length of name so we don't have to strlen a lot */
   RpcIn_Callback callback;
   struct RpcInCallbackList *next;
   void *clientData;
} RpcInCallbackList;

#endif /* VMTOOLS_USE_GLIB */

#if defined(VMTOOLS_USE_VSOCKET)

#define RPCIN_HEARTBEAT_INTERVAL              1000             /* 1 second */
#define RPCIN_MIN_SEND_BUF_SIZE               (64 * 1024)
#define RPCIN_MIN_RECV_BUF_SIZE               (64 * 1024)

struct RpcIn;

/*  container for each vsocket connection details */
typedef struct _ConnInfo {
   AsyncSocket *asock;

   int32 packetLen;
   char *recvBuf;
   int recvBufLen;

   Bool connected;
   Bool shutDown;
   Bool recvStopped;
   int sendQueueLen;

   VmTimeType timestamp;

   struct RpcIn *in;
} ConnInfo;

static void RpcInConnRecvHeader(ConnInfo *conn);
static Bool RpcInConnRecvPacket(ConnInfo *conn, const char **errmsg);
#endif  /* VMTOOLS_USE_VSOCKET */


struct RpcIn {
#if defined(VMTOOLS_USE_GLIB)
   GSource *nextEvent;
   GMainContext *mainCtx;
   RpcIn_Callback dispatch;
   gpointer clientData;
#else
   RpcInCallbackList *callbacks;
   Event *nextEvent;
#endif

#if defined(VMTOOLS_USE_VSOCKET)
   ConnInfo *conn;
   GSource *heartbeatSrc;
#endif

   Message_Channel *channel;
   unsigned int delay;   /* The delay of the previous iteration of RpcInLoop */
   unsigned int maxDelay;  /* The maximum delay to schedule in RpcInLoop */
   RpcIn_ErrorFunc *errorFunc;
   void *errorData;

   /*
    * State of the result associated to the last TCLO request we received
    */

   /* Should we send the result back? */
   Bool mustSend;

   /* The result itself */
   char *last_result;

   /* The size of the result */
   size_t last_resultLen;

   /*
    * It's possible for a callback dispatched by RpcInLoop to call RpcIn_stop.
    * When this happens, we corrupt the state of the RpcIn struct, resulting in
    * a crash the next time RpcInLoop is called. To prevent corruption of the
    * RpcIn struct, we check inLoop when RpcIn_stop is called, and if it is
    * true, we set shouldStop to TRUE instead of actually stopping the
    * channel. When RpcInLoop exits, it will stop the channel if shouldStop is
    * TRUE.
    */
   Bool inLoop;     // RpcInLoop is running.
   Bool shouldStop; // Stop the channel the next time RpcInLoop exits.

   /*
    * RpcInConnErrorHandler called; cleared when a non "reset" reply has been
    * received.
    */
   Bool errStatus;
   RpcIn_ClearErrorFunc *clearErrorFunc;
};

static Bool RpcInSend(RpcIn *in, int flags);
static Bool RpcInScheduleRecvEvent(RpcIn *in);
static void RpcInStop(RpcIn *in);
static Bool RpcInExecRpc(RpcIn *in,            // IN
                         const char *reply,    // IN
                         size_t repLen,        // IN
                         const char **errmsg); // OUT
static Bool RpcInOpenChannel(RpcIn *in, Bool useBackdoorOnly);

/*
 * The following functions are only needed in the non-glib version of the
 * library. The glib version of the library only deals with the transport
 * aspects of the code - RPC dispatching and other RPC-layer concerns are
 * handled by the rpcChannel abstraction library, or by the application.
 */

#if !defined(VMTOOLS_USE_GLIB)

/*
 *-----------------------------------------------------------------------------
 *
 * RpcInPingCallback --
 *
 *      Replies to a ping message from the VMX.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInPingCallback(char const **result,     // OUT
                  size_t *resultLen,       // OUT
                  const char *name,        // IN
                  const char *args,        // IN
                  size_t argsSize,         // IN
                  void *clientData)        // IN
{
   return RpcIn_SetRetVals(result, resultLen, "", TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_Construct --
 *
 *      Constructor for the RpcIn object.
 *
 * Results:
 *      New RpcIn object.
 *
 * Side effects:
 *      Sets the current timer event queue, allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

RpcIn *
RpcIn_Construct(DblLnkLst_Links *eventQueue)
{
   RpcIn *result;
   result = (RpcIn *)calloc(1, sizeof(RpcIn));

   gTimerEventQueue = result? eventQueue: NULL;
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInLookupCallback --
 *
 *      Lookup a callback struct in our list.
 *
 * Results:
 *      The callback if found
 *      NULL if not found
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static RpcInCallbackList *
RpcInLookupCallback(RpcIn *in,        // IN
                    const char *name) // IN
{
   RpcInCallbackList *p;

   ASSERT(in);
   ASSERT(name);

   for (p = in->callbacks; p; p = p->next) {
      if (strcmp(name, p->name) == 0) {
         return p;
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_RegisterCallback --
 *
 *      Register an old-style callback to happen when a TCLO message is
 *      received. When a TCLO message beginning with 'name' is
 *      sent, the callback will be called with: the cmd name, the args
 *      (starting with the char directly after the cmd name; that's why
 *      it's helpful to add a space to the name if arguments are expected),
 *      and a pointer to the result.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_RegisterCallback(RpcIn *in,               // IN
                       const char *name,        // IN
                       RpcIn_Callback cb,       // IN
                       void *clientData)        // IN
{
   RpcInCallbackList *p;

   Debug("RpcIn: Registering callback '%s'\n", name);

   ASSERT(in);
   ASSERT(name);
   ASSERT(cb);
   ASSERT(RpcInLookupCallback(in, name) == NULL); // not there yet

   p = (RpcInCallbackList *) malloc(sizeof(RpcInCallbackList));
   ASSERT_NOT_IMPLEMENTED(p);

   p->length = strlen(name);
   p->name = strdup(name);
   p->callback = cb;
   p->clientData = clientData;

   p->next = in->callbacks;

   in->callbacks = p;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_UnregisterCallback --
 *
 *      Unregisters an RpcIn callback by name.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_UnregisterCallback(RpcIn *in,               // IN
                         const char *name)        // IN
{
   RpcInCallbackList *cur, *prev;

   ASSERT(in);
   ASSERT(name);

   Debug("RpcIn: Unregistering callback '%s'\n", name);

   for (cur = in->callbacks, prev = NULL; cur && strcmp(cur->name, name);
        prev = cur, cur = cur->next);

   /*
    * If we called UnregisterCallback on a name that doesn't exist, we
    * have a problem.
    */
   ASSERT(cur != NULL);

   if (prev == NULL) {
      in->callbacks = cur->next;
   } else {
      prev->next = cur->next;
   }
   free((void *)cur->name);
   free(cur);
}


#else /* VMTOOLS_USE_GLIB */


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_Construct --
 *
 *      Constructor for the RpcIn object. Ties the RpcIn loop to the given
 *      glib main loop, and uses the given callback to dispatch incoming
 *      RPC messages.
 *
 *      The dispatch callback receives data in a slightly different way than
 *      the regular RPC callbacks. Basically, the raw data from the backdoor
 *      is provided in the "args" field of the RpcInData struct, and "name"
 *      is NULL. So the dispatch function is responsible for parsing the RPC
 *      message, and preparing the RpcInData instance for proper use by the
 *      final consumer.
 *
 * Results:
 *      New RpcIn object.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

RpcIn *
RpcIn_Construct(GMainContext *mainCtx,    // IN
                RpcIn_Callback dispatch,  // IN
                gpointer clientData)      // IN
{
   RpcIn *result;

#if defined(VMTOOLS_USE_VSOCKET)
   Poll_InitGtk();
#endif

   ASSERT(mainCtx != NULL);
   ASSERT(dispatch != NULL);

   result = calloc(1, sizeof *result);
   if (result != NULL) {
      result->mainCtx = mainCtx;
      result->clientData = clientData;
      result->dispatch = dispatch;
   }
   return result;
}

#endif /* VMTOOLS_USE_GLIB */


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_Destruct --
 *
 *      Destructor for the RpcIn object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees all memory associated with the RpcIn object, resets the global
 *      timer event queue.
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_Destruct(RpcIn *in) // IN
{
   ASSERT(in);
   ASSERT(in->channel == NULL);
   ASSERT(in->nextEvent == NULL);
   ASSERT(in->mustSend == FALSE);

#if defined(VMTOOLS_USE_VSOCKET)
   ASSERT(in->conn == NULL);
#endif

#if !defined(VMTOOLS_USE_GLIB)
   while (in->callbacks) {
      RpcInCallbackList *p;

      p = in->callbacks->next;
      free((void *) in->callbacks->name);
      free(in->callbacks);
      in->callbacks = p;
   }

   gTimerEventQueue = NULL;
#endif

   free(in);
}


#if defined(VMTOOLS_USE_VSOCKET)

/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnStopRecv --
 *
 *    Stop recving from the vsocket connection.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInConnStopRecv(ConnInfo *conn)   // IN
{
   if (!(conn->recvStopped)) {
      int res = AsyncSocket_CancelRecvEx(conn->asock, NULL, NULL, NULL, TRUE);
      if (res != ASOCKERR_SUCCESS) {
         /* just log an error, we are closing the socket anyway */
         Debug("RpcIn: error in stopping recv for conn %d\n",
               AsyncSocket_GetFd(conn->asock));
      }
      conn->recvStopped = TRUE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInCloseConn --
 *
 *      Close vsocket connection.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInCloseConn(ConnInfo *conn) // IN
{
   int fd = AsyncSocket_GetFd(conn->asock);

   if (conn->in != NULL) {
      conn->in->conn = NULL;
      conn->in = NULL;
   }

   if (conn->sendQueueLen > 0) {
      Debug("RpcIn: Shutting down vsocket connection %d.\n", fd);
      conn->shutDown = TRUE;
      RpcInConnStopRecv(conn);
   } else {
      Debug("RpcIn: Closing vsocket connection %d\n", fd);
      AsyncSocket_Close(conn->asock);
      free(conn->recvBuf);
      free(conn);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnSendDoneCb --
 *
 *    AsyncSocket callback function for a send completion.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInConnSendDoneCb(void *buf,            // IN
                    int len,              // IN
                    AsyncSocket *asock,   // IN
                    void *clientData)     // IN
{
   ConnInfo *conn = (ConnInfo *)clientData;

   free(buf);

   if (AsyncSocket_GetState(asock) == AsyncSocketClosed) {
      /* the connection is closed or being closed. */
      return;
   }

   conn->sendQueueLen -= len;
   ASSERT(conn->sendQueueLen >= 0);

   if (conn->sendQueueLen == 0 && conn->shutDown) {
      Debug("RpcIn: Closing connection %d as sendbuffer is now empty.\n",
            AsyncSocket_GetFd(conn->asock));
      RpcInCloseConn(conn);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInPackSendData --
 *
 *    Helper function for building send packet and serialize it.
 *
 * Result:
 *    TRUE on sucess, FALSE otherwise.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInPackSendData(int fd,                      // IN
                  const char *buf,             // IN
                  int len,                     // IN
                  int flags,                   // IN
                  char **serBuf,               // OUT
                  int32 *serBufLen)            // OUT
{
   DataMap map;
   ErrorCode res;
   char *newBuf;
   gboolean mapCreated = FALSE;
   int64 pktType = (flags & RPCIN_TCLO_PING) ?
                   GUESTRPCPKT_TYPE_PING : GUESTRPCPKT_TYPE_DATA;

   res = DataMap_Create(&map);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   mapCreated = TRUE;
   res = DataMap_SetInt64(&map, GUESTRPCPKT_FIELD_TYPE,
                          pktType, TRUE);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   if (buf != NULL && len > 0) {
      newBuf = malloc(len);
      if (newBuf == NULL) {
         Debug("RpcIn: Error in allocating memory for conn %d.\n", fd);
         goto quit;
      }
      memcpy(newBuf, buf, len);
      res = DataMap_SetString(&map, GUESTRPCPKT_FIELD_PAYLOAD, newBuf,
                              len, TRUE);
      if (res != DMERR_SUCCESS) {
         free(newBuf);
         goto quit;
      }
   }

   res = DataMap_Serialize(&map, serBuf, serBufLen);
   if (res != DMERR_SUCCESS) {
      goto quit;
   }

   DataMap_Destroy(&map);
   return TRUE;

quit:
   if (mapCreated) {
      DataMap_Destroy(&map);
   }
   Debug("RpcIn: Error in dataMap encoding for conn %d.\n", fd);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnSend --
 *
 *    Helper function for writing data to a socket.
 *    ownership of buffer is untouched.
 *
 * Result:
 *    TRUE on sucess, FALSE otherwise.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInConnSend(ConnInfo *conn,              // IN
              const char *buf,             // IN
              int len,                     // IN
              int flags)                   // IN
{
   int res;
   int32 packetLen;
   char *packetBuf;
   int fd = AsyncSocket_GetFd(conn->asock);

   Debug("RpcIn: sending msg to conn %d: len=%d\n",
         AsyncSocket_GetFd(conn->asock), len);

   if (!RpcInPackSendData(fd, buf, len, flags, &packetBuf, &packetLen)) {
      return FALSE;
   }

   res = AsyncSocket_Send(conn->asock, packetBuf, packetLen,
                          RpcInConnSendDoneCb, conn);
   if (res != ASOCKERR_SUCCESS) {
      Debug("RpcIn: error in AsyncSocket_Send for socket %d: %s\n",
            AsyncSocket_GetFd(conn->asock), AsyncSocket_Err2String(res));
      free(packetBuf);
      return FALSE;
   } else {
      conn->sendQueueLen += packetLen;
      return TRUE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInCloseChannel --
 *
 *      Close channel on error.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInCloseChannel(RpcIn *in,               // IN
                  char const *errmsg)      // IN
{
   /* Call the error routine */
   (*in->errorFunc)(in->errorData, errmsg);
   RpcInStop(in);
   in->shouldStop = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInHeartbeatCallback --
 *
 *      Callback function to send a heartbeat message to VMX.
 *
 * Result:
 *      TRUE to keep the callback, FALSE otherwise.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
RpcInHeartbeatCallback(void *clientData)      // IN
{
   RpcIn *in = (RpcIn *)clientData;
   ASSERT(in);
   if (in->conn) {
      ASSERT(!in->mustSend);
      ASSERT(in->last_result == NULL);
      ASSERT(in->last_resultLen == 0);

      in->mustSend = TRUE;
      if (RpcInSend(in, RPCIN_TCLO_PING)) {
         return TRUE;
      } else {
         char *errmsg = "RpcIn: Unable to send";
         RpcInCloseChannel(in, errmsg);
         return FALSE;
      }
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInRegisterHeartbeatCallback --
 *
 *      Register a callback so we can send heartbeat messages periodically, HA
 *      monitoring depends on this.
 *
 * Result:
 *      None.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInRegisterHeartbeatCallback(RpcIn *in)      // IN
{
   ASSERT(in->heartbeatSrc == NULL);
   in->heartbeatSrc = VMTools_CreateTimer(RPCIN_HEARTBEAT_INTERVAL);
   if (in->heartbeatSrc != NULL) {
      g_source_set_callback(in->heartbeatSrc, RpcInHeartbeatCallback, in, NULL);
      g_source_attach(in->heartbeatSrc, in->mainCtx);
   } else {
      Debug("RpcIn: error in scheduling heartbeat callback.\n");
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInDecodePacket --
 *
 *    Helper function to decode received packet in DataMap encoding format.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
RpcInDecodePacket(ConnInfo *conn,       // IN
                  char **payload,       // OUT
                  int32 *payloadLen)    // OUT
{
   ErrorCode res;
   DataMap map;
   int fd = AsyncSocket_GetFd(conn->asock);
   int fullPacketLen = conn->packetLen + sizeof conn->packetLen;
   char *buf;
   int32 len;


   /* decoding the packet */
   res = DataMap_Deserialize(conn->recvBuf, fullPacketLen, &map);
   if (res != DMERR_SUCCESS) {
      Debug("RpcIn: Error in dataMap decoding for conn %d, error=%d\n",
            fd, res);
      return FALSE;
   }

   res = DataMap_GetString(&map, GUESTRPCPKT_FIELD_PAYLOAD, &buf, &len);
   if (res == DMERR_SUCCESS) {
      char *tmpPtr = (char *)malloc(len + 1);
      if (tmpPtr == NULL) {
         Debug("RpcIn: Error in allocating memory for conn %d\n", fd);
         goto exit;
      }
      memcpy(tmpPtr, buf, len);
      /* add a trailing 0 for backward compatible */
      tmpPtr[len] = '\0';

      *payload = tmpPtr;
      *payloadLen = len;
   } else {
      Debug("RpcIn: Empty payload for conn %d\n", fd);
      goto exit;
   }

   DataMap_Destroy(&map);
   return TRUE;

exit:
   DataMap_Destroy(&map);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnRecvedCb --
 *
 *    AsyncSocket callback function after data is recved.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInConnRecvedCb(void *buf,            // IN
                  int len,              // IN
                  AsyncSocket *asock,   // IN
                  void *clientData)     // IN
{
   ConnInfo *conn = (ConnInfo *)clientData;
   const char *errmsg = NULL;

   ASSERT(conn != NULL);

   if (buf == &conn->packetLen) {
      /* We just received the packet header*/
      conn->packetLen = ntohl(conn->packetLen);
      Debug("RpcIn:: Got packet length %d from conn %d.\n",
            conn->packetLen, AsyncSocket_GetFd(conn->asock));
      if (!RpcInConnRecvPacket(conn, &errmsg)) {
         RpcInCloseChannel(conn->in, errmsg);
      }
   } else {
      char *payload = NULL;
      int32 payloadLen = 0;

      ASSERT(buf == conn->recvBuf + sizeof conn->packetLen);
      ASSERT(len <= conn->recvBufLen - sizeof conn->packetLen);

      if (!RpcInDecodePacket(conn, &payload, &payloadLen)) {
         errmsg = "RpcIn: packet error";
         RpcInCloseChannel(conn->in, errmsg);
         return;
      }

      Debug("RpcIn: Got msg from conn %d: [%s]\n",
            AsyncSocket_GetFd(conn->asock), payload);

      if (RpcInExecRpc(conn->in, payload, payloadLen, &errmsg)) {
         conn->in->mustSend = TRUE;
         if (RpcInSend(conn->in, 0)) {
            if (conn->in->heartbeatSrc == NULL) {
               /* Register heartbeat callback after the first successful send
                * so we do not mess with TCLO protocol. */
               RpcInRegisterHeartbeatCallback(conn->in);
            }
            RpcInConnRecvHeader(conn);
            free(payload);
            return;
         } else {
            errmsg = "RpcIn: Unable to send";
         }
      }

      RpcInCloseChannel(conn->in, errmsg);  /* on error */
      free(payload);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnRecvHeader --
 *
 *    Register header recv callback for vsocket connection.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInConnRecvHeader(ConnInfo *conn)   // IN
{
   int res;
   res = AsyncSocket_Recv(conn->asock, &conn->packetLen,
                          sizeof conn->packetLen,
                          RpcInConnRecvedCb, conn);

   conn->recvStopped = res != ASOCKERR_SUCCESS;
   if (res != ASOCKERR_SUCCESS) {
      Debug("RpcIn: error in recving packet header for conn: %d\n",
            AsyncSocket_GetFd(conn->asock));
      RpcInCloseChannel(conn->in, "RpcIn: error in recv");
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnRecvPacket --
 *
 *    Register packet recv callback for vsocket connection.
 *
 * Result:
 *    TRUE on success, FALSE on failure.
 *
 * Side-effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInConnRecvPacket(ConnInfo *conn,         // IN
                    const char **errmsg)    // OUT
{
   int res;
   int32 pktLen = conn->packetLen;
   int fullPktLen = pktLen + sizeof pktLen;

   if (conn->recvBuf == NULL || conn->recvBufLen < fullPktLen) {
      // allocate buffer if needed.
      conn->recvBufLen = fullPktLen;
      free(conn->recvBuf);
      conn->recvBuf = malloc(conn->recvBufLen);
      if (conn->recvBuf == NULL) {
         Debug("RpcIn: Could not allocate recv buffer for socket %d, "
               "closing connection.\n", AsyncSocket_GetFd(conn->asock));
         *errmsg = "Couldn't allocate enough memory";
         return FALSE;
      }
   }

   *((int32 *)(conn->recvBuf)) = htonl(pktLen);
   res = AsyncSocket_Recv(conn->asock, conn->recvBuf + sizeof pktLen,
                          pktLen, RpcInConnRecvedCb, conn);

   conn->recvStopped = res != ASOCKERR_SUCCESS;
   if (res != ASOCKERR_SUCCESS) {
      Debug("RpcIn: error in recving packet for conn %d, "
            "closing connection.\n", AsyncSocket_GetFd(conn->asock));
      *errmsg = "RpcIn: error in recv";
   }
   return res == ASOCKERR_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnErrorHandler --
 *
 *      Connection error handler for asyncsocket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInConnErrorHandler(int err,             // IN
                      AsyncSocket *asock,  // IN
                      void *clientData)    // IN
{
   ConnInfo *conn = (ConnInfo *)clientData;
   char const *errmsg ="RpcIn: vsocket connection error";
   RpcIn *in = conn->in;

   Debug("RpcIn: Error in socket %d, closing connection: %s.\n",
         AsyncSocket_GetFd(asock), AsyncSocket_Err2String(err));

   in->errStatus = TRUE;

   if (conn->connected) {
      RpcInCloseChannel(conn->in, errmsg);
   } else { /* the connection never gets connected */
      RpcInCloseConn(conn);
      Debug("RpcIn: falling back to use backdoor ...\n");
      RpcInOpenChannel(in, TRUE);  /* fall back on backdoor */
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInConnectDone --
 *
 *      Callback function for AsyncSocket connect.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInConnectDone(AsyncSocket *asock,   // IN
                 void *clientData)     // IN
{
   ConnInfo *conn = (ConnInfo *)clientData;
   RpcIn *in = conn->in;

   if (AsyncSocket_GetState(asock) != AsyncSocketConnected) {
      goto exit;
   }

   if (!AsyncSocket_EstablishMinBufferSizes(asock, RPCIN_MIN_SEND_BUF_SIZE,
                                            RPCIN_MIN_RECV_BUF_SIZE)) {
      goto exit;
   }

   conn->connected = TRUE;
   RpcInConnRecvHeader(conn);
   return;

exit:
   Debug("RpcIn: failed to create vsocket connection, using backdoor.\n");
   RpcInCloseConn(conn);
   RpcInOpenChannel(in, TRUE);  /* fall back on backdoor */
}

#endif  /* VMTOOLS_USE_VSOCKET */


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInSend --
 *
 *      Send the last result back to VMware
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side-effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInSend(RpcIn *in,   // IN
          int flags)   // IN
{
   Bool status = FALSE;
   Bool useBackdoor = TRUE;

   ASSERT(in);
   ASSERT(in->mustSend);

#if defined(VMTOOLS_USE_VSOCKET)
   if (in->conn != NULL) {
      useBackdoor = FALSE;
      status = RpcInConnSend(in->conn, in->last_result, in->last_resultLen,
                             flags);
   }
#endif

   if (useBackdoor) {
      ASSERT(in->channel);
      if (in->last_resultLen) {
         Debug("RpcIn: sending %"FMTSZ"u bytes\n", in->last_resultLen);
      }
      status = Message_Send(in->channel, (unsigned char *)in->last_result,
                            in->last_resultLen);
   }

   if (status == FALSE) {
      Debug("RpcIn: couldn't send back the last result\n");
   }

   free(in->last_result);
   in->last_result = NULL;
   in->last_resultLen = 0;
   in->mustSend = FALSE;

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInStop --
 *
 *      Stop the RPC channel.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Sends the last result back to the host.
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInStop(RpcIn *in) // IN
{
   ASSERT(in);
   if (in->nextEvent) {
      /* The loop is started. Stop it */
#if defined(VMTOOLS_USE_GLIB)
      if (!in->inLoop) {
         g_source_destroy(in->nextEvent);
      }

      g_source_unref(in->nextEvent);
#else
      EventManager_Remove(in->nextEvent);
#endif
      in->nextEvent = NULL;
   }

   if (in->channel) {
      /* The channel is open */
      if (in->mustSend) {
         /* There is a final result to send back. Try to send it */
         RpcInSend(in, 0);
         ASSERT(in->mustSend == FALSE);
      }

      /* Try to close the channel */
      if (Message_Close(in->channel) == FALSE) {
         Debug("RpcIn: couldn't close channel\n");
      }

      in->channel = NULL;
   }

#if defined(VMTOOLS_USE_VSOCKET)
   if (in->conn != NULL) {
      if (in->mustSend) {
         /* There is a final result to send back. Try to send it */
         RpcInSend(in, 0);
      }
      RpcInCloseConn(in->conn);
   }

   if (in->heartbeatSrc != NULL) {
      g_source_destroy(in->heartbeatSrc);
      g_source_unref(in->heartbeatSrc);
      in->heartbeatSrc = NULL;
   }
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_stop --
 *
 *      Stop the RPC channel.
 *
 * Results:
 *      None
 *
 * Side-effects:
 *      Sends the last result to the host, if one exists.
 *
 *-----------------------------------------------------------------------------
 */

void
RpcIn_stop(RpcIn *in) // IN
{
   if (in->inLoop) {
      in->shouldStop = TRUE;
   } else {
      RpcInStop(in);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInExecRpc --
 *
 *      Call dispatcher to run the RPC.
 *
 * Result:
 *      TRUE on success, FALSE on error.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInExecRpc(RpcIn *in,            // IN
             const char *reply,    // IN
             size_t repLen,        // IN
             const char **errmsg)  // OUT
{
   unsigned int status;
   const char *statusStr;
   unsigned int statusLen;
   char *result;
   size_t resultLen;
   Bool freeResult = FALSE;

   /*
    * Execute the RPC
    */

#if defined(VMTOOLS_USE_GLIB)
   RpcInData data = { NULL, reply, repLen, NULL, 0, FALSE, NULL, in->clientData };

   status = in->dispatch(&data);
   result = data.result;
   resultLen = data.resultLen;
   freeResult = data.freeResult;
#else
   char *cmd;
   unsigned int index = 0;
   RpcInCallbackList *cb = NULL;

   cmd = StrUtil_GetNextToken(&index, reply, " ");
   if (cmd != NULL) {
      cb = RpcInLookupCallback(in, cmd);
      if (cb) {
         result = NULL;
         status = cb->callback((char const **) &result, &resultLen, cb->name,
                               reply + cb->length, repLen - cb->length,
                               cb->clientData);
         ASSERT(result);
      } else {
         Debug("RpcIn: Unknown Command '%s': No matching callback\n", cmd);
         status = FALSE;
         result = "Unknown Command";
         resultLen = strlen(result);
      }
      free(cmd);
   } else {
      Debug("RpcIn: Bad command (null) received\n");
      status = FALSE;
      result = "Bad command";
      resultLen = strlen(result);
   }
#endif

   statusStr = status ? "OK " : "ERROR ";
   statusLen = strlen(statusStr);

   in->last_result = (char *)malloc(statusLen + resultLen);
   if (in->last_result == NULL) {
      *errmsg = "RpcIn: Not enough memory";
      return FALSE;
   }
   memcpy(in->last_result, statusStr, statusLen);
   memcpy(in->last_result + statusLen, result, resultLen);
   in->last_resultLen = statusLen + resultLen;

   if (freeResult) {
      free(result);
   }

   /*
    * Run the event pump (in case VMware sends a long sequence of RPCs and
    * perfoms a time-consuming job) and continue to loop immediately
    */
   in->delay = 0;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInUpdateDelayTime --
 *
 *      Calculate new delay time.
 *      Use an exponential back-off, doubling the time to wait each time up to
 *      the max delay.
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
RpcInUpdateDelayTime(RpcIn *in)            // IN
{
   if (in->delay < in->maxDelay) {
      if (in->delay > 0) {
         /*
          * Catch overflow.
          */
         in->delay = ((in->delay * 2) > in->delay) ? (in->delay * 2) : in->maxDelay;
      } else {
         in->delay = 1;
      }
      in->delay = MIN(in->delay, in->maxDelay);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ByteDump --
 *
 *      Return a \0 terminated string that keeps ascii characters
 *      but escapes non ascii ones.
 *
 *      The output may be truncated if an internal buffer limit is reached.
 *
 * Result:
 *      Return a string that the caller should not free
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
ByteDump(const char *buf,
        size_t size)
{
#define BYTE_DUMP_LIMIT 128
   static const char truncationTag[] = "...";
   static char dumpBuffer[BYTE_DUMP_LIMIT + sizeof truncationTag];
   size_t i, count, nPrintable, nBinary;

   count = 0;
   nPrintable = 0;
   nBinary = 0;

   if (! size) {
      goto exit;
   }

   for (i = 0; i < size; ++i) {
      unsigned char c = buf[i];
      if (c == '\\') {
         if ((BYTE_DUMP_LIMIT - count) < 2) {
            break;
         }
         dumpBuffer[count++] = c;
         dumpBuffer[count++] = c;
         ++nPrintable;
      } else if (isprint(c)) {
         if ((BYTE_DUMP_LIMIT - count) < 1) {
            break;
         }
         dumpBuffer[count++] = c;
         ++nPrintable;
      } else {
         if ((BYTE_DUMP_LIMIT - count) < 3) {
            break;
         }
         dumpBuffer[count++] = '\\';
         Str_Snprintf(&dumpBuffer[count], 3, "%02x", c);
         count += 2;
         ++nBinary;
      }
   }

   if (nBinary > nPrintable) {
      return "(assumed/dropped binary data)";
   }

   if (i < size) {
      /* Data is truncated */
      int n = Str_Snprintf(&dumpBuffer[count], sizeof dumpBuffer - count,
                           "%s", truncationTag);
      ASSERT(n);
      count += n;
   }

exit:

   dumpBuffer[count] = 0;

   return dumpBuffer;
}


/*
 * RpcInClearErrorStatus --
 *
 *      Clear the errStatus indicator and if a callback has been registered,
 *      notify the RpcChannel layer that an error condition has been cleared.
 */

static void
RpcInClearErrorStatus(RpcIn *in) // IN
{
   if (in->errStatus) {
      Debug("RpcIn: %s: Clearing errStatus\n", __FUNCTION__);
      in->errStatus = FALSE;
      if (in->clearErrorFunc != NULL) {
         in->clearErrorFunc(in->errorData);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInLoop --
 *
 *      Receives an RPC from the host.
 *
 * Result:
 *      For the Event Manager implementation, always TRUE.
 *
 *      For the glib implementation, returns FALSE if the timer was rescheduled
 *      so that g_main_loop will unregister the old timer, or TRUE otherwise.
 *
 * Side-effects:
 *      Stops the RPC channel on error.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMTOOLS_USE_GLIB)
static gboolean
#else
static Bool
#endif
RpcInLoop(void *clientData) // IN
{
   RpcIn *in;
   char const *errmsg = NULL;
   char const *reply;
   size_t repLen;
   Bool resched = FALSE;

#if defined(VMTOOLS_USE_GLIB)
   unsigned int current;
#endif

   in = (RpcIn *)clientData;
   ASSERT(in);
   ASSERT(in->nextEvent);
   ASSERT(in->channel);
   ASSERT(in->mustSend);

#if defined(VMTOOLS_USE_GLIB)
   current = in->delay;
#else
   /*
    * The event has fired: it is no longer valid. Note that this is
    * not true in the glib case!
    */
   in->nextEvent = NULL;
#endif

   in->inLoop = TRUE;

   /*
    * Workaround for bug 780404. Remove if we ever figure out the root cause.
    * Note that the ASSERT above catches this on non-release builds.
    */
   if (in->channel == NULL) {
      errmsg = "RpcIn: Channel is not active";
      goto error;
   }

   /*
    * This is very important: this is the only way to signal the existence of
    * this guest application to VMware.
    */
   if (RpcInSend(in, 0) == FALSE) {
      errmsg = "RpcIn: Unable to send";
      goto error;
   }

   if (Message_Receive(in->channel, (unsigned char **)&reply, &repLen) == FALSE) {
      errmsg = "RpcIn: Unable to receive";
      goto error;
   }

   if (repLen) {
      char *s = ByteDump(reply, repLen);
      Debug("RpcIn: received %d bytes, content:\"%s\"\n", (int) repLen, s);

      /* If reply is not a "reset", the channel is functioning. */
      if (in->errStatus && strcmp(s, "reset") != 0) {
         RpcInClearErrorStatus(in);
      }

      if (!RpcInExecRpc(in, reply, repLen, &errmsg)) {
         goto error;
      }
   } else {
      static uint64 lastPrintMilli = 0;
      uint64 now = System_GetTimeMonotonic() * 10;
      if ((now - lastPrintMilli) > 5000) {
         /*
          * Throttle the log to write one entry every 5 seconds
          * this allow us to figure that tools side is polling for TCLO.
          */
         Debug("RpcIn: received 0 bytes, empty TCLO poll\n");
         lastPrintMilli = now;
      }

      /* RpcIn connection is working - receiving. */
      if (in->errStatus) {
         RpcInClearErrorStatus(in);
      }

      /*
       * Nothing to execute
       */

      /* No request -> No result */
      ASSERT(in->last_result == NULL);
      ASSERT(in->last_resultLen == 0);

      RpcInUpdateDelayTime(in);
   }

   ASSERT(in->mustSend == FALSE);
   in->mustSend = TRUE;

   if (!in->shouldStop) {
      Bool needResched = TRUE;
#if defined(VMTOOLS_USE_GLIB)
      resched = in->delay != current;
      needResched = resched;
#endif
      if (needResched && !RpcInScheduleRecvEvent(in)) {
         errmsg = "RpcIn: Unable to run the loop";
         goto error;
      }
   }

exit:
   if (in->shouldStop) {
      RpcInStop(in);
      in->shouldStop = FALSE;
#if defined(VMTOOLS_USE_GLIB)
      /* Force the GMainContext to unref the GSource that runs the RpcIn loop. */
      resched = TRUE;
#endif
   }

   in->inLoop = FALSE;

   return !resched;

error:
   /* Call the error routine */
   (*in->errorFunc)(in->errorData, errmsg);
   in->shouldStop = TRUE;
   goto exit;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInScheduleRecvEvent --
 *
 *      Setup corresponding callback functions to recv data.
 *
 * Result:
 *      TRUE on success, FALSE on error.
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInScheduleRecvEvent(RpcIn *in)      // IN
{
#if defined(VMTOOLS_USE_GLIB)
   if (in->nextEvent != NULL) {
      g_source_unref(in->nextEvent);
   }
   in->nextEvent = VMTools_CreateTimer(in->delay * 10);
   if (in->nextEvent != NULL) {
      g_source_set_callback(in->nextEvent, RpcInLoop, in, NULL);
      g_source_attach(in->nextEvent, in->mainCtx);
   }
#else
   in->nextEvent = EventManager_Add(gTimerEventQueue, in->delay, RpcInLoop, in);
#endif
   return in->nextEvent != NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcInOpenChannel --
 *
 *    Create backdoor or vsocket channel.
 *
 * Result
 *    TRUE on success
 *    FALSE on failure
 *
 * Side-effects
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
RpcInOpenChannel(RpcIn *in,                 // IN
                 Bool useBackdoorOnly)      // IN
{
#if defined(VMTOOLS_USE_VSOCKET)
   static Bool first = TRUE;
   static Bool initOk = TRUE;
   AsyncSocket *asock;
   int res;

   ASSERT(in->conn == NULL);

   while (TRUE) {  /* one pass loop */
      if (useBackdoorOnly) {
         break;
      }

      if (first) {
         first = FALSE;
         res = AsyncSocket_Init();
         initOk = (res == ASOCKERR_SUCCESS);
         if (!initOk) {
            Debug("RpcIn: Error in socket initialization: %s\n",
                  AsyncSocket_Err2String(res));
            break;
         }
      }

      if (!initOk) {
         break;
      }

      in->conn = calloc(1, sizeof *(in->conn));
      if (in->conn == NULL) {
         Debug("RpcIn: Error in allocating memory for vsocket connection.\n");
         break;
      }
      in->conn->in = in;
      asock = AsyncSocket_ConnectVMCI(VMCI_HYPERVISOR_CONTEXT_ID,
                                      GUESTRPC_TCLO_VSOCK_LISTEN_PORT,
                                      RpcInConnectDone,
                                      in->conn, 0, NULL, &res);
      if (asock == NULL) {
         Debug("RpcIn: Error in creating vsocket connection: %s\n",
               AsyncSocket_Err2String(res));
      } else {
         res = AsyncSocket_SetErrorFn(asock, RpcInConnErrorHandler, in->conn);
         if (res != ASOCKERR_SUCCESS) {
            Debug("RpcIn: Error in setting error handler for vsocket %d\n",
                  AsyncSocket_GetFd(asock));
            AsyncSocket_Close(asock);
         } else {
            Debug("RpcIn: successfully created vsocket connection %d.\n",
                  AsyncSocket_GetFd(asock));
            in->conn->asock = asock;
            return TRUE;
         }
      }
      break;
   }

   if (in->conn != NULL) {
      free(in->conn);
      in->conn = NULL;
   }

#endif

   ASSERT(in->channel == NULL);
   in->channel = Message_Open(0x4f4c4354);
   if (in->channel == NULL) {
      Debug("RpcIn: couldn't open channel with TCLO protocol\n");
      goto error;
   }

   if (!RpcInScheduleRecvEvent(in)) {
      Debug("RpcIn_start: couldn't start the loop\n");
      goto error;
   }

   in->mustSend = TRUE;
   return TRUE;

error:
   RpcInStop(in);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_start --
 *
 *    Start the background loop that receives RPC from VMware
 *
 * Result
 *    TRUE on success
 *    FALSE on failure
 *
 * Side-effects
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMTOOLS_USE_GLIB)
Bool
RpcIn_start(RpcIn *in,                                // IN
            unsigned int delay,                       // IN
            RpcIn_ErrorFunc *errorFunc,               // IN
            RpcIn_ClearErrorFunc *clearErrorFunc,     // IN
            void *errorData)                          // IN

#else
Bool
RpcIn_start(RpcIn *in,                                // IN
            unsigned int delay,                       // IN
            RpcIn_Callback resetCallback,             // IN
            void *resetClientData,                    // IN
            RpcIn_ErrorFunc *errorFunc,               // IN
            RpcIn_ClearErrorFunc *clearErrorFunc,     // IN
            void *errorData)                          // IN
#endif
{
   ASSERT(in);

   in->delay = 0;
   in->maxDelay = delay;
   in->errorFunc = errorFunc;
   in->clearErrorFunc = clearErrorFunc;
   in->errorData = errorData;

   /* No initial result */
   ASSERT(in->last_result == NULL);
   ASSERT(in->last_resultLen == 0);
   ASSERT(in->mustSend == FALSE);
   ASSERT(in->nextEvent == NULL);

#if !defined(VMTOOLS_USE_GLIB)
   /* Register the 'reset' handler */
   if (resetCallback) {
      RpcIn_RegisterCallback(in, "reset", resetCallback, resetClientData);
   }

   RpcIn_RegisterCallback(in, "ping", RpcInPingCallback, NULL);
#endif

   return RpcInOpenChannel(in, FALSE);
}


#if !defined(VMTOOLS_USE_GLIB)
/*
 *-----------------------------------------------------------------------------
 *
 * RpcIn_SetRetVals --
 *
 *      Utility method to set the return values of a tclo command.
 *      Example:
 *          return RpcIn_SetRetVals(result, resultLen,
 *                                  "Message", FALSE);
 *
 * Results:
 *      retVal
 *
 * Side effects:
 *	Sets *result to resultVal & resultLen to strlen(*result).
 *
 *-----------------------------------------------------------------------------
 */

unsigned int
RpcIn_SetRetVals(char const **result,   // OUT
                 size_t *resultLen,     // OUT
                 const char *resultVal, // IN
                 Bool retVal)           // IN
{
   ASSERT(result);
   ASSERT(resultLen);
   ASSERT(resultVal);

   *result = resultVal;
   *resultLen = strlen(*result);

   return retVal;
}
#endif
