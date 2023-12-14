/*********************************************************
 * Copyright (c) 2020-2021 VMware, Inc. All rights reserved.
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
 * gdpPlugin.c --
 *
 * Publishes guest data to host side gdp daemon.
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <unistd.h>
#endif

#include "vm_basic_defs.h"
#include "vm_assert.h"
#include "vm_atomic.h"
#define  G_LOG_DOMAIN  "gdp"
#include "vmware/tools/log.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/threadPool.h"
#include "vmware/tools/gdp.h"
#include "vmci_defs.h"
#include "vmci_sockets.h"
#include "vmcheck.h"
#include "base64.h"
#include "util.h"
#include "str.h"
#include "jsmn.h"
#include "conf.h"

#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);


#if defined(_WIN32)
#   define GetSysErr() WSAGetLastError()

#   define SYSERR_EADDRINUSE    WSAEADDRINUSE
#   define SYSERR_EHOSTUNREACH  WSAEHOSTUNREACH
#   define SYSERR_EINTR         WSAEINTR
#   define SYSERR_EMSGSIZE      WSAEMSGSIZE
#   define SYSERR_WOULDBLOCK(e) (e == WSAEWOULDBLOCK)

    typedef int socklen_t;
#   define CloseSocket closesocket

    typedef WSAEVENT GdpEvent;
#   define GDP_INVALID_EVENT WSA_INVALID_EVENT
#else
#   define GetSysErr() errno

#   define SYSERR_EADDRINUSE    EADDRINUSE
#   define SYSERR_EHOSTUNREACH  EHOSTUNREACH
#   define SYSERR_EINTR         EINTR
#   define SYSERR_EMSGSIZE      EMSGSIZE
#   define SYSERR_WOULDBLOCK(e) (e == EAGAIN || e == EWOULDBLOCK)

    typedef int SOCKET;
#   define SOCKET_ERROR   (-1)
#   define INVALID_SOCKET ((SOCKET) -1)
#   define CloseSocket    close

    typedef int GdpEvent;
#   define GDP_INVALID_EVENT (-1)
#endif

#define GDP_TIMEOUT_AT_INFINITE (-1)
#define GDP_WAIT_INFINITE       (-1)
#define GDP_SEND_AFTER_ANY_TIME (-1)

#define USEC_PER_SECOND      (G_GINT64_CONSTANT(1000000))
#define USEC_PER_MILLISECOND (G_GINT64_CONSTANT(1000))

/*
 * Macros to read values from tools config
 */
#define GDP_CONFIG_GET_BOOL(key, defVal) \
   VMTools_ConfigGetBoolean(gPluginState.ctx->config, \
                            CONFGROUPNAME_GDP, key, defVal)

#define GDP_CONFIG_GET_INT(key, defVal) \
   VMTools_ConfigGetInteger(gPluginState.ctx->config, \
                            CONFGROUPNAME_GDP, key, defVal)

#define GDPD_RECV_PORT 7777
#define GDP_RECV_PORT  766

/*
 * The minimum rate limit is 1 pps, corresponding wait interval is 1 second.
 */
#define GDP_WAIT_RESULT_TIMEOUT 1500 // ms

#define GDP_PACKET_JSON_LINE_SUBSCRIBERS \
   "      \"subscribers\":[%s],\n"

#define GDP_PACKET_JSON \
   "{\n" \
   "   \"header\": {\n" \
   "      \"sequence\":%" G_GUINT64_FORMAT ",\n" \
   "%s" \
   "      \"createTime\":\"%s\",\n" \
   "      \"topic\":\"%s\",\n" \
   "      \"token\":\"%s\"\n" \
   "   },\n" \
   "   \"payload\":{\n" \
   "      \"category\":\"%s\",\n" \
   "      \"base64\":\"%s\"\n" \
   "   }\n" \
   "}"

#define GDP_RESULT_SEQUENCE   "sequence"  // Required, <uint64> e.g. 12345
#define GDP_RESULT_STATUS     "status"    // Required,  "ok", or "bad" for
                                          // malformed and rejected packet
#define GDP_RESULT_DIAGNOSIS  "diagnosis" // Optional
#define GDP_RESULT_RATE_LIMIT "rateLimit" // Required,  <int>
                                          // e.g. 2 packets per second

#define GDP_RESULT_STATUS_OK  "ok"
#define GDP_RESULT_STATUS_BAD "bad"

#define GDP_RESULT_REQUIRED_KEYS 3

#define GDP_HISTORY_REQUEST_PAST_SECONDS   "pastSeconds"   // <uint64>,
                                                           // Required
#define GDP_HISTORY_REQUEST_ID             "id"            // <uint64>,
                                                           // Subscription ID,
                                                           // Required
#define GDP_HISTORY_REQUEST_TOPIC_PREFIXES "topicPrefixes" // <String array>,
                                                           // Optional

#define GDP_HISTORY_REQUEST_REQUIRED_KEYS 2

/*
 * History guest data cache buffer size limit
 */
#define GDP_MAX_CACHE_SIZE_LIMIT     (1 << 22) // 4M
#define GDP_DEFAULT_CACHE_SIZE_LIMIT (1 << 20) // 1M
#define GDP_MIN_CACHE_SIZE_LIMIT     (1 << 18) // 256K

/*
 * History guest data cache item count limit
 */
#define GDP_MAX_CACHE_COUNT_LIMIT     (1 << 10) // 1024
#define GDP_DEFAULT_CACHE_COUNT_LIMIT (1 << 8)  // 256
#define GDP_MIN_CACHE_COUNT_LIMIT     (1 << 6)  // 64

#define GDP_STR_SIZE(s) ((guint32) (s != NULL ? (strlen(s) + 1) : 0))

#define GDP_TOKENS_PER_ALLOC 50

/*
 * GdpError message table.
 */
#define GDP_ERR_ITEM(a, b) b,
static const char * const gdpErrMsgs[] = {
GDP_ERR_LIST
};
#undef GDP_ERR_ITEM


typedef struct PluginState {
   ToolsAppCtx *ctx;       /* Tools application context */

   Atomic_Bool started;    /* TRUE : Guest data publishing is started
                              FALSE: otherwise
                              Transitions from FALSE to TRUE only */

#if defined(_WIN32)
   Bool wsaStarted;        /* TRUE : WSAStartup succeeded, WSACleanup required
                              FALSE: otherwise */
#endif
   int vmciFd;             /* vSocket address family value fd */
   int vmciFamily;         /* vSocket address family value */
   SOCKET sock;            /* Datagram socket for publishing guest data */
#if defined(_WIN32)
   GdpEvent eventNetwork;  /* The network event object:
                              To be associated with network
                              send/recv ready event */
#endif

   GdpEvent eventStop;     /* The stop event object:
                              Signalled to stop guest data publishing */
   Atomic_Bool stopped;    /* TRUE : Guest data publishing is stopped
                              FALSE: otherwise
                              Transitions from FALSE to TRUE only */

   GdpEvent eventConfig;   /* The config event object:
                              Signalled to update config */
} PluginState;

static PluginState gPluginState;


typedef struct PublishState {
   GMutex mutex; /* To sync incoming publish calls */

   /*
    * Data passed from the active incoming publish thread to
    * the gdp task thread.
    */
   gint64 createTime; /* Real wall-clock time,
                         in microseconds since January 1, 1970 UTC. */
   const gchar *topic;
   const gchar *token;
   const gchar *category;
   const gchar *data;
   guint32 dataLen;
   gboolean cacheData;

   /*
    * The publish event object:
    * The active incoming publish thread signals this event object
    * to notify the gdp task thread to publish new data.
    */
   GdpEvent eventPublish;

   /*
    * The get-result event object:
    * The gdp task thread signals this event object to notify
    * the active incoming publish thread to get publish result.
    */
   GdpEvent eventGetResult;

   GdpError gdpErr; /* The publish result */

} PublishState;

static PublishState gPublishState;


typedef enum GdpTaskEvent {
   GDP_TASK_EVENT_NONE,    /* No event */
   GDP_TASK_EVENT_STOP,    /* Stop event */
   GDP_TASK_EVENT_CONFIG,  /* Config event */
   GDP_TASK_EVENT_NETWORK, /* Network event */
   GDP_TASK_EVENT_PUBLISH, /* Publish event */
   GDP_TASK_EVENT_TIMEOUT, /* Wait timed out */
} GdpTaskEvent;

typedef enum GdpTaskMode {
   GDP_TASK_MODE_NONE,    /* Not publishing
                             valid with GDP_TASK_STATE_IDLE only */
   GDP_TASK_MODE_PUBLISH, /* Publishing new data */
   GDP_TASK_MODE_HISTORY, /* Publishing history data */
} GdpTaskMode;

typedef enum GdpTaskState {
   GDP_TASK_STATE_IDLE,             /* Not started
                                       valid with GDP_TASK_MODE_NONE only */
   GDP_TASK_STATE_WAIT_TO_SEND,     /* Wait to send JSON packet */
   GDP_TASK_STATE_WAIT_FOR_RESULT1, /* Wait for publish result from daemon */
   GDP_TASK_STATE_WAIT_FOR_RESULT2, /* Wait for publish result after re-send */
} GdpTaskState;

typedef struct PublishResult {
   guint64 sequence; /* Result for the packet with this sequence number */
   Bool statusOk;    /* TRUE: ok, FALSE: bad */
   gchar *diagnosis; /* Diagnosis message if statusOk is FALSE */
   gint32 rateLimit; /* VMCI peer rate limit */
} PublishResult;

typedef struct HistoryRequest {
   gint64 beginCacheTime;    /* Begin cacheTime */
   gint64 endCacheTime;      /* End cacheTime */
   guint64 id;               /* Subscription ID */
   GPtrArray *topicPrefixes; /* Topic prefixes */
} HistoryRequest;

typedef struct HistoryCacheItem {
   gint64 createTime; /* Guest data - begin */
   gchar *topic;
   gchar *token;
   gchar *category;
   gchar *data;
   guint32 dataLen;   /* Guest data - end */
   gint64 cacheTime;  /* Monotonic time point when item is cached */
   guint32 itemSize;  /* Item size in bytes */
} HistoryCacheItem;

typedef struct HistoryCache {
   GQueue queue;       /* Container for HistoryCacheItem */
   guint32 sizeLimit;  /* Cache buffer size limit */
   guint32 countLimit; /* Cache item count limit */
   guint32 size;       /* Current cache buffer size */
   GList *currentLink; /* Pointer to the history cache queue link
                          currently being published */
} HistoryCache;

typedef struct TaskContext {
   /*
    * Legal mode & state combination
    *
    * GDP_TASK_MODE_NONE   : GDP_TASK_STATE_IDLE
    *                        (Idle state will never time out)
    *
    * GDP_TASK_MODE_PUBLISH: GDP_TASK_STATE_WAIT_TO_SEND
    *                        GDP_TASK_STATE_WAIT_FOR_RESULT1
    *                        GDP_TASK_STATE_WAIT_FOR_RESULT2
    *                        (All the above wait states will time out)
    *
    * GDP_TASK_MODE_HISTORY: GDP_TASK_STATE_WAIT_TO_SEND
    *                        GDP_TASK_STATE_WAIT_FOR_RESULT1
    *                        GDP_TASK_STATE_WAIT_FOR_RESULT2
    *                        (All the above wait states will time out)
    */
   GdpTaskMode mode;
   GdpTaskState state;

   /*
    * Publish event object signalled while mode is GDP_TASK_MODE_HISTORY.
    */
   Bool publishPending;
   /*
    * History request can be received at any time,
    * non-empty requests queue means history request pending.
    */

   HistoryCache cache;  /* Container for history cache items */
   GQueue requests;     /* Container for HistoryRequest */

   guint64 sequence;    /* Sequence number */
   gchar *packet;       /* Formatted JSON packet */
   guint32 packetLen;   /* JSON packet length */

   gint64 timeoutAt;    /* Time out at this monotonic time point,
                           in microseconds. */
   gint64 sendAfter;    /* Send to daemon after this monotonic time point,
                           in microseconds. */
} TaskContext;


static inline GdpEvent
GdpCreateEvent(void);

static inline void
GdpCloseEvent(GdpEvent *event); // IN/OUT

static inline void
GdpSetEvent(GdpEvent event); // IN

static inline void
GdpResetEvent(GdpEvent event); // IN

static GdpError
GdpWaitForEvent(GdpEvent event, // IN
                int timeout);   // IN

static Bool
GdpCreateSocket(void);

static void
GdpCloseSocket(void);

static GdpError
GdpSendTo(SOCKET sock,                         // IN
          const char *buf,                     // IN
          int bufLen,                          // IN
          const struct sockaddr_vm *destAddr); // IN

static GdpError
GdpRecvFrom(SOCKET sock,                  // IN
            char *buf,                    // OUT
            int *bufLen,                  // IN/OUT
            struct sockaddr_vm *srcAddr); // OUT

static inline void
GdpTopicPrefixFree(gpointer data); // IN

static inline void
GdpHistoryRequestFree(HistoryRequest *request); // IN

static void
GdpHistoryRequestFreeGFunc(gpointer item_data,  // IN
                           gpointer user_data); // IN

static inline void
GdpTaskClearHistoryRequestQueue(TaskContext *taskCtx); // IN/OUT

static guint32
GdpGetHistoryCacheSizeLimit();

static guint32
GdpGetHistoryCacheCountLimit();

static inline Bool
GdpTaskIsHistoryCacheEnabled(TaskContext *taskCtx); // IN

static void
GdpHistoryCacheItemFree(HistoryCacheItem *item); // IN

static void
GdpHistoryCacheItemFreeGFunc(gpointer item_data,  // IN
                             gpointer user_data); // IN

static inline void
GdpTaskClearHistoryCacheQueue(TaskContext *taskCtx); // IN/OUT

static void
GdpTaskDeleteHistoryCacheTail(TaskContext *taskCtx); // IN/OUT

static void
GdpTaskHistoryCachePushItem(TaskContext *taskCtx,  // IN/OUT
                            gint64 createTime,     // IN
                            const gchar *topic,    // IN
                            const gchar *token,    // IN
                            const gchar *category, // IN
                            const gchar *data,     // IN
                            guint32 dataLen);      // IN

static gchar *
GdpGetFormattedUtcTime(gint64 utcTime); // IN

static GdpError
GdpTaskBuildPacket(TaskContext *taskCtx,      // IN/OUT
                   gint64 createTime,         // IN
                   const gchar *topic,        // IN
                   const gchar *token,        // IN, OPTIONAL
                   const gchar *category,     // IN, OPTIONAL
                   const gchar *data,         // IN
                   guint32 dataLen,           // IN
                   const gchar *subscribers); // IN, OPTIONAL

static inline void
GdpTaskDestroyPacket(TaskContext *taskCtx); // IN/OUT

static inline Bool
GdpTaskOkToSend(TaskContext *taskCtx); // IN

static GdpError
GdpTaskSendPacket(TaskContext *taskCtx); // IN/OUT

static Bool
GdpMatchTopicPrefixes(const gchar *topic,              // IN
                      const GPtrArray *topicPrefixes); // IN

static gchar *
GdpTaskUpdateHistoryCachePointerAndGetSubscribers(
   TaskContext *taskCtx); // IN/OUT

static void
GdpTaskPublishHistory(TaskContext *taskCtx); // IN/OUT

static void
GdpTaskProcessConfigChange(TaskContext *taskCtx); // IN/OUT

static Bool
GdpJsonIsTokenOfKey(const char *json,       // IN
                    const jsmntok_t *token, // IN
                    const char *key);       // IN

static Bool
GdpJsonIsPublishResult(const char *json,        // IN
                       const jsmntok_t *tokens, // IN
                       int count,               // IN
                       PublishResult *result);  // OUT

static void
GdpTaskProcessPublishResult(TaskContext *taskCtx,         // IN/OUT
                            const PublishResult *result); // IN

static Bool
GdpJsonIsHistoryRequest(const char *json,         // IN
                        const jsmntok_t *tokens,  // IN
                        int count,                // IN
                        HistoryRequest *request); // OUT

static void
GdpTaskProcessHistoryRequest(TaskContext *taskCtx,     // IN/OUT
                             HistoryRequest *request); // IN/OUT

static void
GdpTaskProcessNetwork(TaskContext *taskCtx); // IN/OUT

static void
GdpTaskProcessPublish(TaskContext *taskCtx); // IN/OUT

static void
GdpTaskProcessTimeout(TaskContext *taskCtx); // IN/OUT

static int
GdpTaskGetTimeout(TaskContext *taskCtx); // IN

static void
GdpTaskProcessEvents(TaskContext *taskCtx,    // IN/OUT
                     GdpTaskEvent taskEvent); // IN

static GdpError
GdpTaskWaitForEvents(int timeout,              // IN
                     GdpTaskEvent *taskEvent); // OUT

static void
GdpTaskCtxInit(TaskContext *taskCtx); // OUT

static void
GdpTaskCtxDestroy(TaskContext *taskCtx); // IN/OUT

static void
GdpThreadTask(ToolsAppCtx *ctx, // IN
              void *data);      // IN

static void
GdpThreadInterrupt(ToolsAppCtx *ctx, // IN
                   void *data);      // IN

static void
GdpInit(ToolsAppCtx *ctx); // IN

static Bool
GdpStart(void);

static void
GdpDestroy(void);


/*
 ******************************************************************************
 * GdpCreateEvent --
 *
 * Creates a new event object/fd.
 *
 * @return The new event object handle or fd on success.
 * @return GDP_INVALID_EVENT otherwise.
 *
 ******************************************************************************
 */

static inline GdpEvent
GdpCreateEvent(void)
{
#if defined(_WIN32)
   return WSACreateEvent();
#else
   return eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#endif
}


/*
 ******************************************************************************
 * GdpCloseEvent --
 *
 * Closes the event object/fd.
 *
 * @param[in,out] event  The event object/fd pointer
 *
 ******************************************************************************
 */

static inline void
GdpCloseEvent(GdpEvent *event) // IN/OUT
{
   ASSERT(event != NULL);

   if (*event != GDP_INVALID_EVENT) {
#if defined(_WIN32)
      if (!WSACloseEvent(*event)) {
         g_warning("%s: WSACloseEvent failed: error=%d.\n",
                   __FUNCTION__, WSAGetLastError());
      }
#else
      if (close(*event) != 0) {
         g_warning("%s: close failed: error=%d.\n",
                   __FUNCTION__, errno);
      }
#endif
      *event = GDP_INVALID_EVENT;
   }
}


/*
 ******************************************************************************
 * GdpSetEvent --
 *
 * Signals the event object/fd.
 *
 * @param[in] event  The event object/fd
 *
 ******************************************************************************
 */

static inline void
GdpSetEvent(GdpEvent event) // IN
{
#if defined(_WIN32)
   ASSERT(event != GDP_INVALID_EVENT);
   if (!WSASetEvent(event)) {
      g_warning("%s: WSASetEvent failed: error=%d.\n",
                __FUNCTION__, WSAGetLastError());
   }
#else
   eventfd_t val = 1;
   ASSERT(event != GDP_INVALID_EVENT);
   if (eventfd_write(event, val) != 0) {
      g_warning("%s: eventfd_write failed: error=%d.\n", __FUNCTION__, errno);
   }
#endif
}


/*
 ******************************************************************************
 * GdpResetEvent --
 *
 * Resets the event object/fd.
 *
 * @param[in] event  The event object/fd
 *
 ******************************************************************************
 */

static inline void
GdpResetEvent(GdpEvent event) // IN
{
#if defined(_WIN32)
   ASSERT(event != GDP_INVALID_EVENT);
   if (!WSAResetEvent(event)) {
      g_warning("%s: WSAResetEvent failed: error=%d.\n",
                __FUNCTION__, WSAGetLastError());
   }
#else
   eventfd_t val;
   ASSERT(event != GDP_INVALID_EVENT);
   if (eventfd_read(event, &val) != 0) {
      g_warning("%s: eventfd_read failed: error=%d.\n", __FUNCTION__, errno);
   }
#endif
}


/*
 ******************************************************************************
 * GdpWaitForEvent --
 *
 * Waits for the event object/fd or timeout.
 *
 * @param[in] event    The event object/fd
 * @param[in] timeout  Timeout value in milliseconds,
 *                     negative value means an infinite timeout,
 *                     zero means no wait.
 *
 * @return GDP_ERROR_SUCCESS on event signalled.
 * @return GDP_ERROR_TIMEOUT on timeout.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpWaitForEvent(GdpEvent event, // IN
                int timeout)    // IN
{
#if defined(_WIN32)
   DWORD localTimeout;
   gint64 startTime;
   GdpError retVal;

   localTimeout = (DWORD)(timeout >= 0 ? timeout : WSA_INFINITE);
   if (timeout > 0) {
      startTime = g_get_monotonic_time();
   } else {
      startTime = 0; // Deals with [-Werror=maybe-uninitialized]
   }

   while (TRUE) {
      WSAEVENT eventObjects[] = { event };
      DWORD waitRes;

      waitRes = WSAWaitForMultipleEvents((DWORD) ARRAYSIZE(eventObjects),
                                         eventObjects,
                                         FALSE, localTimeout, TRUE);
      if (waitRes == WSA_WAIT_EVENT_0) {
         retVal = GDP_ERROR_SUCCESS;
         break;
      } else if (waitRes == WSA_WAIT_IO_COMPLETION) {
         gint64 curTime;
         gint64 passedTime;

         if (localTimeout == 0 ||
             localTimeout == WSA_INFINITE) {
            continue;
         }

         curTime = g_get_monotonic_time();
         passedTime = (curTime - startTime) / USEC_PER_MILLISECOND;
         if (passedTime >= localTimeout) {
            retVal = GDP_ERROR_TIMEOUT;
            break;
         }

         startTime = curTime;
         localTimeout -= (DWORD) passedTime;
         continue;
      } else if (waitRes == WSA_WAIT_TIMEOUT) {
         retVal = GDP_ERROR_TIMEOUT;
         break;
      } else { // WSA_WAIT_FAILED
         g_warning("%s: WSAWaitForMultipleEvents failed: error=%d.\n",
                   __FUNCTION__, WSAGetLastError());
         retVal = GDP_ERROR_GENERAL;
         break;
      }
   }

   return retVal;

#else

   gint64 startTime;
   GdpError retVal;

   if (timeout > 0) {
      startTime = g_get_monotonic_time();
   } else {
      startTime = 0; // Deals with [-Werror=maybe-uninitialized]
   }

   while (TRUE) {
      struct pollfd fds[1];
      int res;

      fds[0].fd = event;
      fds[0].events = POLLIN;
      fds[0].revents = 0;

      res = poll(fds, ARRAYSIZE(fds), timeout);
      if (res > 0) {
         if (fds[0].revents & POLLIN) {
            retVal = GDP_ERROR_SUCCESS;
         } else { // Not expected
            g_warning("%s: Unexpected event.\n", __FUNCTION__);
            retVal = GDP_ERROR_GENERAL;
         }

         break;
      } else if (res == -1) {
         int err = errno;
         if (err == EINTR) {
            gint64 curTime;
            gint64 passedTime;

            if (timeout <= 0) {
               continue;
            }

            curTime = g_get_monotonic_time();
            passedTime = (curTime - startTime) / USEC_PER_MILLISECOND;
            if (passedTime >= timeout) {
               retVal = GDP_ERROR_TIMEOUT;
               break;
            }

            startTime = curTime;
            timeout -= (int) passedTime;
            continue;
         } else {
            g_warning("%s: poll failed: error=%d.\n", __FUNCTION__, err);
            retVal = GDP_ERROR_GENERAL;
            break;
         }
      } else if (res == 0) {
         retVal = GDP_ERROR_TIMEOUT;
         break;
      } else {
         g_warning("%s: Unexpected poll return: %d.\n", __FUNCTION__, res);
         retVal = GDP_ERROR_GENERAL;
         break;
      }
   }

   return retVal;
#endif
}


/*
 ******************************************************************************
 * GdpCreateSocket --
 *
 * Creates a non-blocking datagram socket for guest data publishing.
 *
 * The socket is bound to the gdp guest receive port.
 *
 * @return TRUE on success.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
GdpCreateSocket(void)
{
   Bool retVal;
   struct sockaddr_vm localAddr;
#if defined(_WIN32)
   u_long nbMode = 1; // Non-blocking mode
#  define SOCKET_TYPE_PARAM SOCK_DGRAM
#else
   /*
    * Requires Linux kernel version >= 2.6.27.
    */
#  define SOCKET_TYPE_PARAM SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC
#endif

   ASSERT(gPluginState.sock == INVALID_SOCKET);
   ASSERT(gPluginState.vmciFamily != -1);

   gPluginState.sock = socket(gPluginState.vmciFamily, SOCKET_TYPE_PARAM, 0);
#undef SOCKET_TYPE_PARAM

   if (gPluginState.sock == INVALID_SOCKET) {
      g_critical("%s: socket failed: error=%d.\n", __FUNCTION__, GetSysErr());
      return FALSE;
   }

   retVal = FALSE;

#if defined(_WIN32)
   /*
    * Sets socket to nonblocking mode.
    * Note: WSAEventSelect automatically does this if not done.
    */
   if (ioctlsocket(gPluginState.sock, FIONBIO, &nbMode) != 0) {
      g_critical("%s: ioctlsocket failed: error=%d.\n",
                 __FUNCTION__, GetSysErr());
      goto exit;
   }
#endif

   memset(&localAddr, 0, sizeof localAddr);
   localAddr.svm_family = gPluginState.vmciFamily;
   localAddr.svm_cid = VMCISock_GetLocalCID();
   localAddr.svm_port = GDP_RECV_PORT; // No htons

   if (bind(gPluginState.sock, (struct sockaddr *) &localAddr,
            (socklen_t) sizeof localAddr) != 0) {
      g_critical("%s: bind failed: error=%d.\n",
                 __FUNCTION__, GetSysErr());
      goto exit;
   }

   g_debug("%s: Socket created and bound to local port %d.\n",
           __FUNCTION__, localAddr.svm_port);

   retVal = TRUE;

exit:
   if (!retVal) {
      CloseSocket(gPluginState.sock);
      gPluginState.sock = INVALID_SOCKET;
   }

   return retVal;
}


/*
 ******************************************************************************
 * GdpCloseSocket --
 *
 * Closes the guest data publishing datagram socket.
 *
 ******************************************************************************
 */

static void
GdpCloseSocket(void)
{
   if (gPluginState.sock != INVALID_SOCKET) {
      g_debug("%s: Closing socket.\n", __FUNCTION__);
      if (CloseSocket(gPluginState.sock) != 0) {
         g_warning("%s: CloseSocket failed: fd=%d, error=%d.\n",
                   __FUNCTION__, gPluginState.sock, GetSysErr());
      }

      gPluginState.sock = INVALID_SOCKET;
   }
}


/*
 ******************************************************************************
 * GdpSendTo --
 *
 * Wrapper of sendto() for VMCI datagram socket.
 *
 * Datagram send is not buffered, it will not return EWOULDBLOCK.
 * If host daemon is not running, error EHOSTUNREACH is returned.
 *
 * @param[in] sock      VMCI datagram socket descriptor
 * @param[in] buf       Data buffer pointer
 * @param[in] bufLen    Data length
 * @param[in] destAddr  Destination VMCI datagram socket address
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpSendTo(SOCKET sock,                        // IN
          const char *buf,                    // IN
          int bufLen,                         // IN
          const struct sockaddr_vm *destAddr) // IN
{
   GdpError retVal;

   ASSERT(sock != INVALID_SOCKET);
   ASSERT(buf != NULL && bufLen > 0);
   ASSERT(destAddr != NULL);

   do {
      long res;
      int err;
      socklen_t destAddrLen = (socklen_t) sizeof *destAddr;

      res = sendto(sock, buf, bufLen, 0,
                   (const struct sockaddr *) destAddr, destAddrLen);
      if (res == bufLen) {
         retVal = GDP_ERROR_SUCCESS;
         break;
      } else if (res != SOCKET_ERROR) { // No partial send shall happen
         g_warning("%s: sendto returned unexpected value %d.\n",
                   __FUNCTION__, (int) res);
         retVal = GDP_ERROR_GENERAL;
         break;
      }

      err = GetSysErr();
      if (err == SYSERR_EINTR) {
         continue;
      } else if (err == SYSERR_EHOSTUNREACH) {
         g_info("%s: sendto failed: host daemon unreachable.\n", __FUNCTION__);
         retVal = GDP_ERROR_UNREACH;
      } else if (err == SYSERR_EMSGSIZE) {
         g_warning("%s: sendto failed: message too large.\n", __FUNCTION__);
         retVal = GDP_ERROR_DATA_SIZE;
      } else {
         g_warning("%s: sendto failed: error=%d.\n", __FUNCTION__, err);
         retVal = GDP_ERROR_GENERAL;
      }

      break;

   } while (TRUE);

   return retVal;
}


/*
 ******************************************************************************
 * GdpRecvFrom --
 *
 * Wrapper of recvfrom() for VMCI datagram socket.
 *
 * @param[in]     sock     VMCI datagram socket descriptor
 * @param[out]    buf      Buffer pointer
 * @param[in,out] bufLen   Buffer length on input,
 *                         received data length on output
 * @param[out]    srcAddr  Source VMCI datagram socket address
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpRecvFrom(SOCKET sock,                 // IN
            char *buf,                   // OUT
            int *bufLen,                 // IN/OUT
            struct sockaddr_vm *srcAddr) // OUT
{
   GdpError retVal;

   ASSERT(sock != INVALID_SOCKET);
   ASSERT(buf != NULL && bufLen != NULL && *bufLen > 0);
   ASSERT(srcAddr != NULL);

   do {
      long res;
      int err;
      socklen_t srcAddrLen = (socklen_t) sizeof *srcAddr;

      res = recvfrom(sock, buf, *bufLen, 0,
                     (struct sockaddr *) srcAddr, &srcAddrLen);
      if (res >= 0) {
         *bufLen = (int) res;
         retVal = GDP_ERROR_SUCCESS;
         break;
      }

      ASSERT(res == SOCKET_ERROR);
      err = GetSysErr();
      if (err == SYSERR_EINTR) {
         continue;
      } else if (err == SYSERR_EMSGSIZE) {
         g_warning("%s: recvfrom failed: buffer size too small.\n",
                   __FUNCTION__);
         retVal = GDP_ERROR_DATA_SIZE;
      } else { // Including EWOULDBLOCK
         g_warning("%s: recvfrom failed: error=%d.\n", __FUNCTION__, err);
         retVal = GDP_ERROR_GENERAL;
      }

      break;

   } while (TRUE);

   return retVal;
}


/*
 *****************************************************************************
 * GdpTopicPrefixFree --
 *
 * Frees topic prefix string buffer.
 *
 * @param[in] data  Topic prefix string buffer pointer
 *
 *****************************************************************************
 */

static inline void
GdpTopicPrefixFree(gpointer data) // IN
{
   g_debug("%s: Freeing buffer for topic prefix \"%s\".\n",
           __FUNCTION__, (const char *) data);
   free(data);
}


/*
 *****************************************************************************
 * GdpHistoryRequestFree --
 *
 * Frees history request resources.
 *
 * @param[in] request  History request pointer
 *
 *****************************************************************************
 */

static inline void
GdpHistoryRequestFree(HistoryRequest *request) // IN
{
   g_debug("%s: Entering ...\n", __FUNCTION__);

   if (request->topicPrefixes != NULL) {
      g_ptr_array_free(request->topicPrefixes, TRUE);
   }

   free(request);
}


/*
 *****************************************************************************
 * GdpHistoryRequestFreeGFunc --
 *
 * GFunc called by GdpTaskClearHistoryRequestQueue.
 *
 * @param[in] item_data  History request pointer
 * @param[in] user_data  Not used
 *
 *****************************************************************************
 */

static void
GdpHistoryRequestFreeGFunc(gpointer item_data, // IN
                           gpointer user_data) // IN
{
   GdpHistoryRequestFree((HistoryRequest *) item_data);
}


/*
 *****************************************************************************
 * GdpTaskClearHistoryRequestQueue --
 *
 * Removes all the elements in history request queue.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static inline void
GdpTaskClearHistoryRequestQueue(TaskContext *taskCtx) // IN/OUT
{
   g_queue_foreach(&taskCtx->requests, GdpHistoryRequestFreeGFunc, NULL);
   g_queue_clear(&taskCtx->requests);
}


/*
 ******************************************************************************
 * GdpGetHistoryCacheSizeLimit --
 *
 * Gets history cache buffer size limit from tools config.
 *
 ******************************************************************************
 */

static guint32
GdpGetHistoryCacheSizeLimit()
{
   gint sizeLimit;

   sizeLimit = GDP_CONFIG_GET_INT(CONFNAME_GDP_CACHE_SIZE,
                                  GDP_DEFAULT_CACHE_SIZE_LIMIT);
   if (sizeLimit != 0 &&
       (sizeLimit < GDP_MIN_CACHE_SIZE_LIMIT ||
        sizeLimit > GDP_MAX_CACHE_SIZE_LIMIT)) {
      g_warning("%s: Configured history cache buffer size limit %d "
                "exceeds range, set to default value %d.\n",
                __FUNCTION__, sizeLimit, GDP_DEFAULT_CACHE_SIZE_LIMIT);
      sizeLimit = GDP_DEFAULT_CACHE_SIZE_LIMIT;
   }

   return (guint32) sizeLimit;
}


/*
 ******************************************************************************
 * GdpGetHistoryCacheCountLimit --
 *
 * Gets history cache item count limit from tools config.
 *
 ******************************************************************************
 */

static guint32
GdpGetHistoryCacheCountLimit()
{
   gint countLimit;

   countLimit = GDP_CONFIG_GET_INT(CONFNAME_GDP_CACHE_COUNT,
                                   GDP_DEFAULT_CACHE_COUNT_LIMIT);
   if (countLimit < GDP_MIN_CACHE_COUNT_LIMIT ||
       countLimit > GDP_MAX_CACHE_COUNT_LIMIT) {
      g_warning("%s: Configured history cache item count limit %d "
                "exceeds range, set to default value %d.\n",
                __FUNCTION__, countLimit, GDP_DEFAULT_CACHE_COUNT_LIMIT);
      countLimit = GDP_DEFAULT_CACHE_COUNT_LIMIT;
   }

   return (guint32) countLimit;
}


/*
 ******************************************************************************
 * GdpTaskIsHistoryCacheEnabled --
 *
 * Checks if history cache is enabled.
 *
 * @param[in] taskCtx  The task context
 *
 * @return TRUE if history cache buffer size limit is not zero.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static inline Bool
GdpTaskIsHistoryCacheEnabled(TaskContext *taskCtx) // IN
{
   return taskCtx->cache.sizeLimit != 0 ? TRUE : FALSE;
}


/*
 *****************************************************************************
 * GdpHistoryCacheItemFree --
 *
 * Frees history cache item resources.
 *
 * @param[in] item  History cache item pointer
 *
 *****************************************************************************
 */

static void
GdpHistoryCacheItemFree(HistoryCacheItem *item) // IN
{
   g_debug("%s: Entering ...\n", __FUNCTION__);
   ASSERT(item != NULL);
   free(item->topic);
   free(item->token);
   free(item->category);
   free(item->data);
   free(item);
}


/*
 *****************************************************************************
 * GdpHistoryCacheItemFreeGFunc --
 *
 * GFunc called by GdpTaskClearHistoryCacheQueue.
 *
 * @param[in] item_data  History cache item pointer
 * @param[in] user_data  Not used
 *
 *****************************************************************************
 */

static void
GdpHistoryCacheItemFreeGFunc(gpointer item_data, // IN
                             gpointer user_data) // IN
{
   GdpHistoryCacheItemFree((HistoryCacheItem *) item_data);
}


/*
 *****************************************************************************
 * GdpTaskClearHistoryCacheQueue --
 *
 * Removes all the elements in history cache queue.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static inline void
GdpTaskClearHistoryCacheQueue(TaskContext *taskCtx) // IN/OUT
{
   g_queue_foreach(&taskCtx->cache.queue, GdpHistoryCacheItemFreeGFunc, NULL);
   g_queue_clear(&taskCtx->cache.queue);

}


/*
 *****************************************************************************
 * GdpTaskDeleteHistoryCacheTail --
 *
 * Deletes the tail of history cache queue and removes its reference.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskDeleteHistoryCacheTail(TaskContext *taskCtx) // IN/OUT
{
   HistoryCacheItem *item;

   ASSERT(!g_queue_is_empty(&taskCtx->cache.queue));

   if (taskCtx->cache.currentLink ==
       g_queue_peek_tail_link(&taskCtx->cache.queue)) {
      taskCtx->cache.currentLink = NULL;
   }

   item = (HistoryCacheItem *) g_queue_pop_tail(&taskCtx->cache.queue);
   ASSERT(item != NULL);
   taskCtx->cache.size -= item->itemSize;
   GdpHistoryCacheItemFree(item);
}


/*
 ******************************************************************************
 * GdpTaskHistoryCachePushItem --
 *
 * Pushes the published guest data item into history cache queue.
 *
 * @param[in,out]      taskCtx     The task context
 * @param[in]          createTime  UTC timestamp, in number of micro-
 *                                 seconds since January 1, 1970 UTC.
 * @param[in]          topic       Topic
 * @param[in,optional] token       Token, can be NULL
 * @param[in,optional] category    Category, can be NULL that defaults to
 *                                 "application"
 * @param[in]          data        Buffer containing data to publish
 * @param[in]          dataLen     Buffer length
 *
 ******************************************************************************
 */

static void
GdpTaskHistoryCachePushItem(TaskContext *taskCtx,  // IN/OUT
                            gint64 createTime,     // IN
                            const gchar *topic,    // IN
                            const gchar *token,    // IN
                            const gchar *category, // IN
                            const gchar *data,     // IN
                            guint32 dataLen)       // IN
{
   HistoryCacheItem *item;

   ASSERT(topic != NULL);
   ASSERT(data != NULL && dataLen > 0);
   ASSERT(GdpTaskIsHistoryCacheEnabled(taskCtx));

   item = (HistoryCacheItem *) Util_SafeMalloc(sizeof(HistoryCacheItem));
   item->createTime = createTime;
   item->topic = Util_SafeStrdup(topic);
   item->token = Util_SafeStrdup(token);
   item->category = Util_SafeStrdup(category);
   item->data = (gchar *) Util_SafeMalloc(dataLen);
   Util_Memcpy(item->data, data, dataLen);
   item->dataLen = dataLen;

   item->cacheTime = g_get_monotonic_time();
   item->itemSize = ((guint32) sizeof(HistoryCacheItem))
                    + GDP_STR_SIZE(item->topic)
                    + GDP_STR_SIZE(item->token)
                    + GDP_STR_SIZE(item->category)
                    + item->dataLen;

   while (taskCtx->cache.size + item->itemSize >
             taskCtx->cache.sizeLimit ||
          g_queue_get_length(&taskCtx->cache.queue) >=
             taskCtx->cache.countLimit) {
      GdpTaskDeleteHistoryCacheTail(taskCtx);
   }

   g_queue_push_head(&taskCtx->cache.queue, item);
   taskCtx->cache.size += item->itemSize;

   g_debug("%s: Current history cache size in bytes: %u, item count: %u.\n",
           __FUNCTION__, taskCtx->cache.size,
           g_queue_get_length(&taskCtx->cache.queue));
}


/*
 ******************************************************************************
 * GdpGetFormattedUtcTime --
 *
 * Converts UTC time to string in format like "2018-02-22T21:17:38.517Z".
 *
 * The caller must free the return value using g_free.
 *
 * @param[in] utcTime  Real wall-clock time
 *                     Number of microseconds since January 1, 1970 UTC.
 *
 * @return Formatted timestamp string on success.
 * @return NULL otherwise.
 *
 ******************************************************************************
 */

static gchar *
GdpGetFormattedUtcTime(gint64 utcTime) // IN
{
   gchar *retVal = NULL;
   GDateTime *utcDateTime;

   utcDateTime = g_date_time_new_from_unix_utc(utcTime / USEC_PER_SECOND);
   if (utcDateTime != NULL) {
      gchar *dateToSecond; // YYYY-MM-DDTHH:MM:SS

      dateToSecond = g_date_time_format(utcDateTime, "%FT%T");
      if (dateToSecond != NULL) {
         gint msec; // milliseconds

         msec = (utcTime % USEC_PER_SECOND) / USEC_PER_MILLISECOND;
         retVal = g_strdup_printf("%s.%03dZ", dateToSecond, msec);
         g_free(dateToSecond);
      }

      g_date_time_unref(utcDateTime);
   }

   return retVal;
}


/*
 ******************************************************************************
 * GdpTaskBuildPacket --
 *
 * Builds JSON packet in the task context.
 *
 * @param[in,out]      taskCtx      The task context
 * @param[in]          createTime   UTC timestamp, in number of micro-
 *                                  seconds since January 1, 1970 UTC.
 * @param[in]          topic        Topic
 * @param[in,optional] token        Token, can be NULL
 * @param[in,optional] category     Category, can be NULL that defaults to
 *                                  "application"
 * @param[in]          data         Buffer containing data to publish
 * @param[in]          dataLen      Buffer length
 * @param[in,optional] subscribers  For history data only, NULL for new data
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpTaskBuildPacket(TaskContext *taskCtx,     // IN/OUT
                   gint64 createTime,        // IN
                   const gchar *topic,       // IN
                   const gchar *token,       // IN, OPTIONAL
                   const gchar *category,    // IN, OPTIONAL
                   const gchar *data,        // IN
                   guint32 dataLen,          // IN
                   const gchar *subscribers) // IN, OPTIONAL
{
   gchar base64Data[GDP_MAX_PACKET_LEN + 1]; // Add a space for NULL
   gchar *subscribersLine = NULL;
   gchar *formattedTime;
   GdpError gdpErr;

   ASSERT(topic != NULL);
   ASSERT(data != NULL && dataLen > 0);

   if (!Base64_Encode(data, dataLen, base64Data, sizeof base64Data, NULL)) {
      g_info("%s: Base64_Encode failed, data length is %u.\n",
             __FUNCTION__, dataLen);
      return GDP_ERROR_DATA_SIZE;
   }

   if (subscribers != NULL && *subscribers != '\0') {
      subscribersLine = g_strdup_printf(GDP_PACKET_JSON_LINE_SUBSCRIBERS,
                                        subscribers);
   }

   formattedTime = GdpGetFormattedUtcTime(createTime);

   ASSERT(taskCtx->packet == NULL);
   taskCtx->packet = g_strdup_printf(GDP_PACKET_JSON,
                                     ++taskCtx->sequence,
                                     subscribersLine != NULL ?
                                        subscribersLine : "",
                                     formattedTime != NULL ?
                                        formattedTime : "",
                                     topic,
                                     token != NULL ?
                                        token : "",
                                     category != NULL ?
                                        category : "application",
                                     base64Data);
   taskCtx->packetLen = (guint32) strlen(taskCtx->packet);
   if (taskCtx->packetLen > GDP_MAX_PACKET_LEN) {
      g_info("%s: Packet length (%u) exceeds maximum limit (%u).\n",
             __FUNCTION__, taskCtx->packetLen, GDP_MAX_PACKET_LEN);
      GdpTaskDestroyPacket(taskCtx);
      gdpErr = GDP_ERROR_DATA_SIZE;
   } else {
      gdpErr = GDP_ERROR_SUCCESS;
   }

   g_free(formattedTime);
   g_free(subscribersLine);
   return gdpErr;
}


/*
 *****************************************************************************
 * GdpTaskDestroyPacket --
 *
 * Destroys JSON packet in the task context.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static inline void
GdpTaskDestroyPacket(TaskContext *taskCtx) // IN/OUT
{
   g_free(taskCtx->packet);
   taskCtx->packet = NULL;
   taskCtx->packetLen = 0;
}


/*
 ******************************************************************************
 * GdpTaskOkToSend --
 *
 * Checks if current time is OK to send JSON packet to host side gdp daemon.
 *
 * @param[in] taskCtx  The task context
 *
 * @return TRUE if current time has passed the task context sendAfter time.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static inline Bool
GdpTaskOkToSend(TaskContext *taskCtx) // IN
{
   return g_get_monotonic_time() >= taskCtx->sendAfter ?
             TRUE : FALSE;
}


/*
 ******************************************************************************
 * GdpTaskSendPacket --
 *
 * Sends JSON packet in the task context to host side gdp daemon.
 *
 * Datagram send is not buffered, it will not return EWOULDBLOCK.
 * If host daemon is not running, error EHOSTUNREACH is returned.
 *
 * @param[in,out] taskCtx  The task context
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpTaskSendPacket(TaskContext *taskCtx) // IN/OUT
{
   struct sockaddr_vm destAddr;
   GdpError gdpErr;

   ASSERT(gPluginState.sock != INVALID_SOCKET);
   ASSERT(taskCtx->packet != NULL && taskCtx->packetLen > 0);

   memset(&destAddr, 0, sizeof destAddr);
   destAddr.svm_family = gPluginState.vmciFamily;
   destAddr.svm_cid = VMCI_HOST_CONTEXT_ID;
   destAddr.svm_port = GDPD_RECV_PORT; // No htons

   gdpErr = GdpSendTo(gPluginState.sock, taskCtx->packet,
                      (int) taskCtx->packetLen, &destAddr);
   if (gdpErr == GDP_ERROR_SUCCESS) {
      /*
       * Updates sendAfter for next send, phase 1 / 2.
       */
      taskCtx->sendAfter = g_get_monotonic_time();
      taskCtx->timeoutAt = taskCtx->sendAfter +
                           GDP_WAIT_RESULT_TIMEOUT * USEC_PER_MILLISECOND;
   } else {
      GdpTaskDestroyPacket(taskCtx);
      taskCtx->timeoutAt = GDP_TIMEOUT_AT_INFINITE;
   }

   return gdpErr;
}


/*
 *****************************************************************************
 * GdpMatchTopicPrefixes --
 *
 * Matches a topic against prefixes.
 *
 * @param[in] topic          Topic
 * @param[in] topicPrefixes  Topic prefixes
 *
 * @return TRUE if topic matches any prefix.
 * @return FALSE otherwise.
 *
 *****************************************************************************
 */

static Bool
GdpMatchTopicPrefixes(const gchar *topic,             // IN
                      const GPtrArray *topicPrefixes) // IN
{
   guint index;

   ASSERT(topic != NULL);
   ASSERT(topicPrefixes != NULL);

   for (index = 0; index < topicPrefixes->len; index++) {
      const gchar *prefix = g_ptr_array_index(topicPrefixes, index);
      guint prefixLen = (guint) strlen(prefix);

      if (strncmp(topic, prefix, prefixLen) == 0) {
         if (topic[prefixLen] == '.' || topic[prefixLen] == '\0') {
            return TRUE;
         }
      }
   }

   return FALSE;
}


/*
 *****************************************************************************
 * GdpTaskUpdateHistoryCachePointerAndGetSubscribers --
 *
 * Updates history cache pointer and gets subscribers of the pointed item.
 *
 * @param[in,out] taskCtx  The task context
 *
 * @return A string of subscription IDs separated by comma
           (to be freed by caller),
 *         or NULL if no match.
 *
 *****************************************************************************
 */

static gchar *
GdpTaskUpdateHistoryCachePointerAndGetSubscribers(
   TaskContext *taskCtx) // IN/OUT
{
   gchar *subscribers = NULL;

   if (taskCtx->cache.currentLink == NULL) {
      /*
       * Starts with the earliest history cache link.
       */
      taskCtx->cache.currentLink = g_queue_peek_tail_link(
                                      &taskCtx->cache.queue);
   }

   while (taskCtx->cache.currentLink != NULL &&
          !g_queue_is_empty(&taskCtx->requests)) {
      HistoryCacheItem *item;
      GList *requestList;

      item = (HistoryCacheItem *) taskCtx->cache.currentLink->data;
      requestList = g_queue_peek_tail_link(&taskCtx->requests);
      while (requestList != NULL) {
         HistoryRequest *request = (HistoryRequest *) requestList->data;
         Bool requestDone = FALSE;

         if (item->cacheTime > request->endCacheTime) {
            requestDone = TRUE;
         } else if (request->beginCacheTime < item->cacheTime &&
                    item->cacheTime <= request->endCacheTime) {
            if (request->topicPrefixes == NULL ||
                GdpMatchTopicPrefixes(item->topic, request->topicPrefixes)) {
               if (subscribers == NULL) {
                  subscribers = g_strdup_printf("%" G_GUINT64_FORMAT,
                                                request->id);
               } else {
                  gchar *subscribersNew;
                  subscribersNew = g_strdup_printf("%s,%" G_GUINT64_FORMAT,
                                                   subscribers, request->id);
                  g_free(subscribers);
                  subscribers = subscribersNew;
               }
            }

            if (item->cacheTime == request->endCacheTime) {
               requestDone = TRUE;
            } else {
               request->beginCacheTime = item->cacheTime;
            }
         }

         if (requestDone) {
            GList *requestListPrev = requestList->prev;
            GdpHistoryRequestFree(request);
            g_queue_delete_link(&taskCtx->requests, requestList);
            requestList = requestListPrev;
         } else {
            requestList = requestList->prev;
         }
      }

      if (subscribers != NULL) {
         break;
      }

      taskCtx->cache.currentLink = taskCtx->cache.currentLink->prev;
   }

   return subscribers;
}


/*
 *****************************************************************************
 * GdpTaskPublishHistory --
 *
 * Publishes cached history guest data.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskPublishHistory(TaskContext *taskCtx) // IN/OUT
{
   GdpError gdpErr;
   gchar *subscribers;
   HistoryCacheItem *item;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   ASSERT(taskCtx->mode == GDP_TASK_MODE_NONE &&
          taskCtx->state == GDP_TASK_STATE_IDLE);

   subscribers = GdpTaskUpdateHistoryCachePointerAndGetSubscribers(taskCtx);
   if (subscribers == NULL) {
      g_debug("%s: No history data to publish now.\n",
              __FUNCTION__);
      goto cleanup;
   }

   ASSERT(taskCtx->cache.currentLink != NULL);
   item = (HistoryCacheItem *) taskCtx->cache.currentLink->data;
   /*
    * Updates history cache pointer.
    */
   taskCtx->cache.currentLink = taskCtx->cache.currentLink->prev;

   gdpErr = GdpTaskBuildPacket(taskCtx,
                               item->createTime,
                               item->topic,
                               item->token,
                               item->category,
                               item->data,
                               item->dataLen,
                               subscribers);
   if (gdpErr != GDP_ERROR_SUCCESS) {
      /*
       * Theoretically speaking, too many subscribers could cause JSON packet
       * length exceed maximum limit and fail GdpTaskBuildPacket with
       * GDP_ERROR_DATA_SIZE.
       */
      g_info("%s: Failed to build JSON packet for subscribers: [%s].\n",
             __FUNCTION__, subscribers);
      g_free(subscribers);
      goto cleanup;
   }
   g_free(subscribers);

   if (GdpTaskOkToSend(taskCtx)) {
      gdpErr = GdpTaskSendPacket(taskCtx);
      if (gdpErr != GDP_ERROR_SUCCESS) {
         g_info("%s: Failed to send history JSON packet.\n",
                __FUNCTION__);
         goto cleanup;
      }

      taskCtx->state = GDP_TASK_STATE_WAIT_FOR_RESULT1;
   } else {
      taskCtx->timeoutAt = taskCtx->sendAfter;
      taskCtx->state = GDP_TASK_STATE_WAIT_TO_SEND;
   }

   taskCtx->mode = GDP_TASK_MODE_HISTORY;
   g_debug("%s: Updated mode=%d, state=%d.\n",
           __FUNCTION__, taskCtx->mode, taskCtx->state);
   return;

cleanup:
   taskCtx->cache.currentLink = NULL;
   GdpTaskClearHistoryRequestQueue(taskCtx);
}


/*
 *****************************************************************************
 * GdpTaskProcessConfigChange --
 *
 * Processes config change.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskProcessConfigChange(TaskContext *taskCtx) // IN/OUT
{
   guint32 sizeLimit = GdpGetHistoryCacheSizeLimit();
   guint32 countLimit = GdpGetHistoryCacheCountLimit();

   if (taskCtx->cache.sizeLimit == sizeLimit &&
       taskCtx->cache.countLimit == countLimit) {
      return;
   }

   g_debug("%s: Current history cache buffer size limit: %u, new value: %u.\n",
           __FUNCTION__, taskCtx->cache.sizeLimit, sizeLimit);
   taskCtx->cache.sizeLimit = sizeLimit;

   g_debug("%s: Current history cache item count limit: %u, new value: %u.\n",
           __FUNCTION__, taskCtx->cache.countLimit, countLimit);
   taskCtx->cache.countLimit = countLimit;

   while (taskCtx->cache.size > taskCtx->cache.sizeLimit ||
          g_queue_get_length(&taskCtx->cache.queue) >
             taskCtx->cache.countLimit) {
      GdpTaskDeleteHistoryCacheTail(taskCtx);
   }
}


/*
 ******************************************************************************
 * GdpJsonIsTokenOfKey --
 *
 * Checks if the JSON token represents the key.
 *
 * @param[in]  json   The JSON text
 * @param[in]  token  Token
 * @param[in]  key    Key name
 *
 * @return TRUE if token represents the key.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
GdpJsonIsTokenOfKey(const char *json,       // IN
                    const jsmntok_t *token, // IN
                    const char *key)        // IN
{
   int tokenLen = token->end - token->start;
   if (token->type == JSMN_STRING &&
       token->size == 1 && // The token represents a key
       (int) strlen(key) == tokenLen &&
       strncmp(json + token->start, key, tokenLen) == 0) {
      return TRUE;
   }
   return FALSE;
}


/*
 ******************************************************************************
 * GdpJsonIsPublishResult --
 *
 * Checks if the JSON text represents a publish result.
 *
 * @param[in]  json     The JSON text
 * @param[in]  tokens   Token array
 * @param[in]  count    Token array count
 * @param[out] request  Pointer to publish result
 *
 * @return TRUE if json represents a publish result.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
GdpJsonIsPublishResult(const char *json,        // IN
                       const jsmntok_t *tokens, // IN
                       int count,               // IN
                       PublishResult *result)   // OUT
{
   int index;
   int requiredKeys = GDP_RESULT_REQUIRED_KEYS;
   gchar *diagnosis = NULL;

   /*
    * Loops over all keys of the root object.
    */
   for (index = 1; index < count; index++) {
      int tokenLen;

      if (GdpJsonIsTokenOfKey(json, &tokens[index], GDP_RESULT_SEQUENCE)) {
         requiredKeys--;
         index++;
         result->sequence = g_ascii_strtoull(json + tokens[index].start,
                                             NULL, 10);
      } else if (GdpJsonIsTokenOfKey(json, &tokens[index],
                                     GDP_RESULT_STATUS)) {
         requiredKeys--;
         index++;
         tokenLen = tokens[index].end - tokens[index].start;
         if ((int) strlen(GDP_RESULT_STATUS_OK) == tokenLen &&
             strncmp(json + tokens[index].start,
                     GDP_RESULT_STATUS_OK, tokenLen) == 0) {
            result->statusOk = TRUE;
         } else {
            result->statusOk = FALSE;
         }
      } else if (GdpJsonIsTokenOfKey(json, &tokens[index],
                                     GDP_RESULT_DIAGNOSIS)) {
         index++;
         ASSERT(diagnosis == NULL);
         tokenLen = tokens[index].end - tokens[index].start;
         diagnosis = g_strndup(json + tokens[index].start, tokenLen);
      } else if (GdpJsonIsTokenOfKey(json, &tokens[index],
                                     GDP_RESULT_RATE_LIMIT)) {
         requiredKeys--;
         index++;
         result->rateLimit = atoi(json + tokens[index].start);
      }
   }

   if (requiredKeys == 0) {
      result->diagnosis = diagnosis;
      return TRUE;
   } else {
      g_free(diagnosis);
      return FALSE;
   }
}


/*
 *****************************************************************************
 * GdpTaskProcessPublishResult --
 *
 * Processes publish result.
 *
 * @param[in,out] taskCtx  The task context
 * @param[in]     result   Publish result
 *
 *****************************************************************************
 */

static void
GdpTaskProcessPublishResult(TaskContext *taskCtx,        // IN/OUT
                            const PublishResult *result) // IN
{
   g_debug("%s: Entering ...\n", __FUNCTION__);

   if (taskCtx->mode == GDP_TASK_MODE_NONE ||
       (taskCtx->state != GDP_TASK_STATE_WAIT_FOR_RESULT1 &&
        taskCtx->state != GDP_TASK_STATE_WAIT_FOR_RESULT2)) {
      g_info("%s: Publish result not expected at mode=%d, state=%d.\n",
             __FUNCTION__, taskCtx->mode, taskCtx->state);
      return;
   }

   if (taskCtx->sequence != result->sequence) {
      g_info("%s: Publish result sequence number not match.\n",
             __FUNCTION__);
      return;
   }

   if (!result->statusOk) {
      g_info("%s: Publish failed: %s\n", __FUNCTION__,
             result->diagnosis ? result->diagnosis : "");
   }

   if (taskCtx->mode == GDP_TASK_MODE_PUBLISH) {
      if (result->statusOk) {
         if (GdpTaskIsHistoryCacheEnabled(taskCtx) &&
             gPublishState.cacheData) {
            GdpTaskHistoryCachePushItem(taskCtx,
                                        gPublishState.createTime,
                                        gPublishState.topic,
                                        gPublishState.token,
                                        gPublishState.category,
                                        gPublishState.data,
                                        gPublishState.dataLen);
         }

         gPublishState.gdpErr = GDP_ERROR_SUCCESS;
      } else {
         gPublishState.gdpErr = GDP_ERROR_INVALID_DATA;
      }

      GdpSetEvent(gPublishState.eventGetResult);
   }

   GdpTaskDestroyPacket(taskCtx);
   if (result->rateLimit > 0) {
      /*
       * Updates sendAfter for next send, phase 2 / 2.
       */
      taskCtx->sendAfter += (USEC_PER_SECOND / result->rateLimit);
   }
   taskCtx->timeoutAt = GDP_TIMEOUT_AT_INFINITE;
   taskCtx->mode = GDP_TASK_MODE_NONE;
   taskCtx->state = GDP_TASK_STATE_IDLE;
   g_debug("%s: Reset mode=%d, state=%d.\n",
           __FUNCTION__, taskCtx->mode, taskCtx->state);
}


/*
 ******************************************************************************
 * GdpJsonIsHistoryRequest --
 *
 * Checks if the JSON text represents a history request.
 *
 * @param[in]  json     The JSON text
 * @param[in]  tokens   Token array
 * @param[in]  count    Token array count
 * @param[out] request  Pointer to history request
 *
 * @return TRUE if json represents a history request.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
GdpJsonIsHistoryRequest(const char *json,        // IN
                        const jsmntok_t *tokens, // IN
                        int count,               // IN
                        HistoryRequest *request) // OUT
{
   int index;
   int requiredKeys = GDP_HISTORY_REQUEST_REQUIRED_KEYS;
   guint64 pastSeconds = 0;

   request->topicPrefixes = NULL;

   /*
    * Loops over all keys of the root object
    */
   for (index = 1; index < count; index++) {
      if (GdpJsonIsTokenOfKey(json, &tokens[index],
                              GDP_HISTORY_REQUEST_PAST_SECONDS)) {
         requiredKeys--;
         index++;
         pastSeconds = g_ascii_strtoull(json + tokens[index].start, NULL, 10);
      } else if (GdpJsonIsTokenOfKey(json, &tokens[index],
                                     GDP_HISTORY_REQUEST_ID)) {
         requiredKeys--;
         index++;
         request->id = g_ascii_strtoull(json + tokens[index].start, NULL, 10);
      } else if (GdpJsonIsTokenOfKey(json, &tokens[index],
                                     GDP_HISTORY_REQUEST_TOPIC_PREFIXES) &&
                 tokens[index + 1].type == JSMN_ARRAY) {
         int prefixCount;
         int prefixIndex;

         prefixCount = tokens[index + 1].size;
         request->topicPrefixes = g_ptr_array_new_full(prefixCount,
                                                       GdpTopicPrefixFree);
         index += 2;
         for (prefixIndex = 0; prefixIndex < prefixCount; prefixIndex++) {
            const jsmntok_t *token = &tokens[index + prefixIndex];
            g_ptr_array_add(request->topicPrefixes,
                            Util_SafeStrndup(json + token->start,
                                             token->end - token->start));
         }

         index += (prefixCount - 1);
      }
   }

   if (requiredKeys == 0) {
      request->endCacheTime = g_get_monotonic_time();
      request->beginCacheTime = request->endCacheTime -
                                (gint64)pastSeconds * USEC_PER_SECOND;
      return TRUE;
   }

   if (request->topicPrefixes != NULL) {
      g_ptr_array_free(request->topicPrefixes, TRUE);
      request->topicPrefixes = NULL;
   }
   return FALSE;
}


/*
 *****************************************************************************
 * GdpTaskProcessHistoryRequest --
 *
 * Validates new history request, pushes the request to queue and
 * resets history cache pointer.
 *
 * @param[in,out] taskCtx  The task context
 * @param[in,out] request  New history request
 *
 *****************************************************************************
 */

static void
GdpTaskProcessHistoryRequest(TaskContext *taskCtx,    // IN/OUT
                             HistoryRequest *request) // IN/OUT
{
   HistoryRequest *requestCopy;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   if (!GdpTaskIsHistoryCacheEnabled(taskCtx)) {
      g_info("%s: History cache not enabled.\n", __FUNCTION__);
      goto fail;
   }

   if (request->beginCacheTime >= request->endCacheTime) {
      g_info("%s: Invalid history request.\n", __FUNCTION__);
      goto fail;
   }

   if (request->beginCacheTime < 0) {
      request->beginCacheTime = 0;
   }

   requestCopy = (HistoryRequest *) Util_SafeMalloc(sizeof *request);
   requestCopy->beginCacheTime = request->beginCacheTime;
   requestCopy->endCacheTime = request->endCacheTime;
   requestCopy->id = request->id;
   requestCopy->topicPrefixes = request->topicPrefixes;
   request->topicPrefixes = NULL;

   /*
    * Note: each request comes with a unique subscription ID.
    */
   g_queue_push_head(&taskCtx->requests, requestCopy);
   /*
    * Resets history cache pointer.
    */
   taskCtx->cache.currentLink = NULL;

   return;

fail:
   if (request->topicPrefixes != NULL) {
      g_ptr_array_free(request->topicPrefixes, TRUE);
      request->topicPrefixes = NULL;
   }
}


/*
 *****************************************************************************
 * GdpTaskProcessNetwork --
 *
 * Processes the network event.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskProcessNetwork(TaskContext *taskCtx) // IN/OUT
{
   GdpError gdpErr;
   char buf[GDP_MAX_PACKET_LEN + 1]; // Adds a space for NULL
   int bufLen = (int) sizeof buf - 1;
   struct sockaddr_vm srcAddr;
   unsigned int numTokens;
   jsmntok_t *tokens;
   jsmn_parser parser;
   int retVal;
   Bool isPublishResult;
   Bool isHistoryRequest;
   PublishResult result = { 0 };
   HistoryRequest request = { 0 };

   g_debug("%s: Entering ...\n", __FUNCTION__);

   gdpErr = GdpRecvFrom(gPluginState.sock, buf, &bufLen, &srcAddr);
   if (gdpErr != GDP_ERROR_SUCCESS || bufLen <= 0) {
      return;
   }

   if (srcAddr.svm_cid != VMCI_HOST_CONTEXT_ID) {
      g_info("%s: Unexpected source svm_cid: %u.\n",
             __FUNCTION__, srcAddr.svm_cid);
      return;
   }

   buf[bufLen] = '\0';

   jsmn_init(&parser);

   numTokens = GDP_TOKENS_PER_ALLOC;
   tokens = Util_SafeMalloc(numTokens * sizeof *tokens);
   while ((retVal = jsmn_parse(&parser, buf, bufLen, tokens, numTokens))
          == JSMN_ERROR_NOMEM) {
      numTokens += GDP_TOKENS_PER_ALLOC;
      tokens = Util_SafeRealloc(tokens, numTokens * sizeof *tokens);
   }

   if (retVal < 0) {
      g_info("%s: Error %d while parsing JSON:\n%s\n",
             __FUNCTION__, retVal, buf);
      free(tokens);
      return;
   }

   /*
    * The top-level element should be an object.
    */
   if (retVal < 1 || tokens[0].type != JSMN_OBJECT) {
      g_info("%s: Invalid JSON:\n%s\n", __FUNCTION__, buf);
      free(tokens);
      return;
   }

   isPublishResult = FALSE;
   isHistoryRequest = FALSE;
   if (srcAddr.svm_port == GDPD_RECV_PORT) {
      isPublishResult = GdpJsonIsPublishResult(buf, tokens,
                                               retVal, &result);
      if (!isPublishResult) {
         isHistoryRequest = GdpJsonIsHistoryRequest(buf, tokens,
                                                    retVal, &request);
      }
   } else {
      isHistoryRequest = GdpJsonIsHistoryRequest(buf, tokens,
                                                 retVal, &request);
      if (!isHistoryRequest) {
         isPublishResult = GdpJsonIsPublishResult(buf, tokens,
                                                  retVal, &result);
      }
   }

   if (isPublishResult) {
      GdpTaskProcessPublishResult(taskCtx, &result);
      g_free(result.diagnosis);
   } else if (isHistoryRequest) {
      g_debug("%s: Received history request:\n%s\n", __FUNCTION__, buf);
      GdpTaskProcessHistoryRequest(taskCtx, &request);
   } else {
      g_info("%s: Unknown JSON:\n%s\n", __FUNCTION__, buf);
   }

   free(tokens);
}


/*
 *****************************************************************************
 * GdpTaskProcessPublish --
 *
 * Processes the publish event.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskProcessPublish(TaskContext *taskCtx) // IN/OUT
{
   GdpError gdpErr;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   ASSERT(taskCtx->mode != GDP_TASK_MODE_PUBLISH);

   if (taskCtx->mode != GDP_TASK_MODE_NONE) {
      g_debug("%s: Set publish pending.\n", __FUNCTION__);
      ASSERT(!taskCtx->publishPending);
      taskCtx->publishPending = TRUE;
      return;
   }

   ASSERT(taskCtx->state == GDP_TASK_STATE_IDLE);

   gdpErr = GdpTaskBuildPacket(taskCtx,
                               gPublishState.createTime,
                               gPublishState.topic,
                               gPublishState.token,
                               gPublishState.category,
                               gPublishState.data,
                               gPublishState.dataLen,
                               NULL);
   if (gdpErr != GDP_ERROR_SUCCESS) {
      goto fail;
   }

   if (GdpTaskOkToSend(taskCtx)) {
      gdpErr = GdpTaskSendPacket(taskCtx);
      if (gdpErr != GDP_ERROR_SUCCESS) {
         goto fail;
      }

      taskCtx->state = GDP_TASK_STATE_WAIT_FOR_RESULT1;
   } else {
      taskCtx->timeoutAt = taskCtx->sendAfter;
      taskCtx->state = GDP_TASK_STATE_WAIT_TO_SEND;
   }

   taskCtx->mode = GDP_TASK_MODE_PUBLISH;
   g_debug("%s: Updated mode=%d, state=%d.\n",
           __FUNCTION__, taskCtx->mode, taskCtx->state);
   return;

fail:
   gPublishState.gdpErr = gdpErr;
   GdpSetEvent(gPublishState.eventGetResult);
}


/*
 *****************************************************************************
 * GdpTaskProcessTimeout --
 *
 * Processes wait timeout.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskProcessTimeout(TaskContext *taskCtx) // IN/OUT
{
   GdpError gdpErr;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   ASSERT(taskCtx->mode != GDP_TASK_MODE_NONE &&
          taskCtx->state != GDP_TASK_STATE_IDLE);

   if (taskCtx->state == GDP_TASK_STATE_WAIT_TO_SEND ||
       taskCtx->state == GDP_TASK_STATE_WAIT_FOR_RESULT1) {
      gdpErr = GdpTaskSendPacket(taskCtx);
      if (gdpErr != GDP_ERROR_SUCCESS) {
         goto fail;
      }

      if (taskCtx->state == GDP_TASK_STATE_WAIT_TO_SEND) {
         taskCtx->state = GDP_TASK_STATE_WAIT_FOR_RESULT1;
      } else {
         taskCtx->state = GDP_TASK_STATE_WAIT_FOR_RESULT2;
      }

      g_debug("%s: Updated mode=%d, state=%d.\n",
              __FUNCTION__, taskCtx->mode, taskCtx->state);
   } else if (taskCtx->state == GDP_TASK_STATE_WAIT_FOR_RESULT2) {
      g_warning("%s: Wait for publish result timed out.\n", __FUNCTION__);
      GdpTaskDestroyPacket(taskCtx);
      taskCtx->timeoutAt = GDP_TIMEOUT_AT_INFINITE;
      gdpErr = GDP_ERROR_TIMEOUT;
      goto fail;
   } else {
      NOT_REACHED();
   }

   return;

fail:
   if (taskCtx->mode == GDP_TASK_MODE_PUBLISH) {
      gPublishState.gdpErr = gdpErr;
      GdpSetEvent(gPublishState.eventGetResult);
   }

   taskCtx->state = GDP_TASK_STATE_IDLE;
   taskCtx->mode = GDP_TASK_MODE_NONE;
   g_debug("%s: Reset mode=%d, state=%d.\n",
           __FUNCTION__, taskCtx->mode, taskCtx->state);
}


/*
 *****************************************************************************
 * GdpTaskGetTimeout --
 *
 * Gets timeout value for the next wait call.
 *
 * @param[in] taskCtx  The task context
 *
 *****************************************************************************
 */

static int
GdpTaskGetTimeout(TaskContext *taskCtx) // IN
{
   gint64 curTime;
   gint64 timeout;

   if (taskCtx->timeoutAt == GDP_TIMEOUT_AT_INFINITE) {
      return GDP_WAIT_INFINITE;
   }

   curTime = g_get_monotonic_time();
   if (curTime >= taskCtx->timeoutAt) {
      return 0;
   }

   timeout = (taskCtx->timeoutAt - curTime) / USEC_PER_MILLISECOND;
   return (int)timeout;
}


/*
 *****************************************************************************
 * GdpTaskProcessEvents --
 *
 * Processes task events.
 *
 * @param[in,out] taskCtx    The task context
 * @param[in]     taskEvent  Task event to process
 *
 *****************************************************************************
 */

static void
GdpTaskProcessEvents(TaskContext *taskCtx,   // IN/OUT
                     GdpTaskEvent taskEvent) // IN
{
   switch (taskEvent) {
      case GDP_TASK_EVENT_CONFIG:
         GdpResetEvent(gPluginState.eventConfig);
         GdpTaskProcessConfigChange(taskCtx);
         break;
      case GDP_TASK_EVENT_NETWORK:
         GdpTaskProcessNetwork(taskCtx);
         break;
      case GDP_TASK_EVENT_PUBLISH:
         GdpResetEvent(gPublishState.eventPublish);
         GdpTaskProcessPublish(taskCtx);
         break;
      case GDP_TASK_EVENT_TIMEOUT:
         GdpTaskProcessTimeout(taskCtx);
         break;
      default:
         //GDP_TASK_EVENT_NONE
         //GDP_TASK_EVENT_STOP
         NOT_REACHED();
   }
}


/*
 ******************************************************************************
 * GdpTaskWaitForEvents --
 *
 * Waits for the following events or timeout:
 *    The stop event
 *    The config event
 *    The network event
 *    The publish event
 *
 * @param[in]  timeout    Timeout value in milliseconds,
 *                        negative value means an infinite timeout,
 *                        zero means no wait.
 * @param[out] taskEvent  Return which event is signalled
 *                        if no error happens.
 *
 * @return GDP_ERROR_SUCCESS if no error happens.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpTaskWaitForEvents(int timeout,             // IN
                     GdpTaskEvent *taskEvent) // OUT
{
#if defined(_WIN32)
   int res;
   DWORD localTimeout;
   gint64 startTime;
   GdpError retVal;

   ASSERT(Atomic_ReadBool(&gPluginState.started));

   /*
    * Resets the network event object.
    */
   WSAResetEvent(gPluginState.eventNetwork);

   /*
    * Associates the network event object with FD_READ network event
    * in the socket.
    */
   res = WSAEventSelect(gPluginState.sock, gPluginState.eventNetwork, FD_READ);
   if (res != 0) {
      g_warning("%s: WSAEventSelect failed: error=%d.\n",
                __FUNCTION__, WSAGetLastError());
      return GDP_ERROR_GENERAL;
   }

   retVal = GDP_ERROR_SUCCESS;

   localTimeout = (timeout >= 0 ? (DWORD) timeout : WSA_INFINITE);
   if (timeout > 0) {
      startTime = g_get_monotonic_time();
   } else {
      startTime = 0; // Deals with [-Werror=maybe-uninitialized]
   }

   while (TRUE) {
      WSAEVENT eventObjects[] = { gPluginState.eventStop,
                                  gPluginState.eventConfig,
                                  gPluginState.eventNetwork,
                                  gPublishState.eventPublish };
      DWORD waitRes;

      waitRes = WSAWaitForMultipleEvents((DWORD)ARRAYSIZE(eventObjects),
                                         eventObjects,
                                         FALSE, localTimeout, TRUE);
      if (waitRes == WSA_WAIT_EVENT_0) {
         *taskEvent = GDP_TASK_EVENT_STOP;
         break;
      } else if (waitRes == (WSA_WAIT_EVENT_0 + 1)) {
         *taskEvent = GDP_TASK_EVENT_CONFIG;
         break;
      } else if (waitRes == (WSA_WAIT_EVENT_0 + 2)) {
         WSANETWORKEVENTS networkEvents;
         res = WSAEnumNetworkEvents(gPluginState.sock, NULL, &networkEvents);
         if (res != 0) {
            g_warning("%s: WSAEnumNetworkEvents failed: error=%d.\n",
                      __FUNCTION__, WSAGetLastError());
            retVal = GDP_ERROR_GENERAL;
            break;
         }

         /*
          * Not checking networkEvents.iErrorCode[FD_READ_BIT]/
          * networkEvents.iErrorCode[FD_WRITE_BIT] for WSAENETDOWN,
          * since WSAEnumNetworkEvents should have returned WSAENETDOWN
          * if the error condition exists.
          */
         if (networkEvents.lNetworkEvents & FD_READ) {
            *taskEvent = GDP_TASK_EVENT_NETWORK;
         } else { // Not expected
            g_warning("%s: Unexpected network event.\n",
                      __FUNCTION__);
            retVal = GDP_ERROR_GENERAL;
         }

         break;
      } else if (waitRes == (WSA_WAIT_EVENT_0 + 3)) {
         *taskEvent = GDP_TASK_EVENT_PUBLISH;
         break;
      } else if (waitRes == WSA_WAIT_IO_COMPLETION) {
         gint64 curTime;
         gint64 passedTime;

         if (localTimeout == 0 ||
             localTimeout == WSA_INFINITE) {
            continue;
         }

         curTime = g_get_monotonic_time();
         passedTime = (curTime - startTime) / USEC_PER_MILLISECOND;
         if (passedTime >= localTimeout) {
            *taskEvent = GDP_TASK_EVENT_TIMEOUT;
            break;
         }

         startTime = curTime;
         localTimeout -= (DWORD)passedTime;
         continue;
      } else if (waitRes == WSA_WAIT_TIMEOUT) {
         *taskEvent = GDP_TASK_EVENT_TIMEOUT;
         break;
      } else { // WSA_WAIT_FAILED
         g_warning("%s: WSAWaitForMultipleEvents failed: error=%d.\n",
                   __FUNCTION__, WSAGetLastError());
         retVal = GDP_ERROR_GENERAL;
         break;
      }
   }

   /*
    * Cancels the association.
    */
   WSAEventSelect(gPluginState.sock, NULL, 0);

   return retVal;

#else

   gint64 startTime;
   GdpError retVal = GDP_ERROR_SUCCESS;

   ASSERT(Atomic_ReadBool(&gPluginState.started));

   if (timeout > 0) {
      startTime = g_get_monotonic_time();
   } else {
      startTime = 0; // Deals with [-Werror=maybe-uninitialized]
   }

   while (TRUE) {
      struct pollfd fds[4];
      int res;

      fds[0].fd = gPluginState.eventStop;
      fds[0].events = POLLIN;
      fds[0].revents = 0;
      fds[1].fd = gPluginState.eventConfig;
      fds[1].events = POLLIN;
      fds[1].revents = 0;
      fds[2].fd = gPluginState.sock;
      fds[2].events = POLLIN;
      fds[2].revents = 0;
      fds[3].fd = gPublishState.eventPublish;
      fds[3].events = POLLIN;
      fds[3].revents = 0;

      res = poll(fds, ARRAYSIZE(fds), timeout);
      if (res > 0) {
         if (fds[0].revents & POLLIN) {
            *taskEvent = GDP_TASK_EVENT_STOP;
         } else if (fds[1].revents & POLLIN) {
            *taskEvent = GDP_TASK_EVENT_CONFIG;
         } else if (fds[2].revents & POLLIN) {
            *taskEvent = GDP_TASK_EVENT_NETWORK;
         } else if (fds[3].revents & POLLIN) {
            *taskEvent = GDP_TASK_EVENT_PUBLISH;
         } else { // Not expected
            g_warning("%s: Unexpected event.\n", __FUNCTION__);
            retVal = GDP_ERROR_GENERAL;
         }

         break;
      } else if (res == -1) {
         int err = errno;
         if (err == EINTR) {
            gint64 curTime;
            gint64 passedTime;

            if (timeout <= 0) {
               continue;
            }

            curTime = g_get_monotonic_time();
            passedTime = (curTime - startTime) / USEC_PER_MILLISECOND;
            if (passedTime >= timeout) {
               *taskEvent = GDP_TASK_EVENT_TIMEOUT;
               break;
            }

            startTime = curTime;
            timeout -= (int) passedTime;
            continue;
         } else {
            g_warning("%s: poll failed: error=%d.\n", __FUNCTION__, err);
            retVal = GDP_ERROR_GENERAL;
            break;
         }
      } else if (res == 0) {
         *taskEvent = GDP_TASK_EVENT_TIMEOUT;
         break;
      } else {
         g_warning("%s: Unexpected poll return: %d.\n", __FUNCTION__, res);
         retVal = GDP_ERROR_GENERAL;
         break;
      }
   }

   return retVal;
#endif
}


/*
 *****************************************************************************
 * GdpTaskCtxInit --
 *
 * Initializes the task context states and resources.
 *
 * @param[out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskCtxInit(TaskContext *taskCtx) // OUT
{
   taskCtx->mode = GDP_TASK_MODE_NONE;
   taskCtx->state = GDP_TASK_STATE_IDLE;
   taskCtx->publishPending = FALSE;

   g_queue_init(&taskCtx->cache.queue);
   taskCtx->cache.sizeLimit = GdpGetHistoryCacheSizeLimit();
   taskCtx->cache.countLimit = GdpGetHistoryCacheCountLimit();
   taskCtx->cache.size = 0;
   taskCtx->cache.currentLink = NULL;

   g_queue_init(&taskCtx->requests);

   taskCtx->sequence = 0;
   taskCtx->packet = NULL;
   taskCtx->packetLen = 0;

   taskCtx->timeoutAt = GDP_TIMEOUT_AT_INFINITE;
   taskCtx->sendAfter = GDP_SEND_AFTER_ANY_TIME;
}


/*
 *****************************************************************************
 * GdpTaskCtxDestroy --
 *
 * Destroys the task context resources.
 *
 * @param[in,out] taskCtx  The task context
 *
 *****************************************************************************
 */

static void
GdpTaskCtxDestroy(TaskContext *taskCtx) // IN/OUT
{
   GdpTaskClearHistoryCacheQueue(taskCtx);
   GdpTaskClearHistoryRequestQueue(taskCtx);
   GdpTaskDestroyPacket(taskCtx);
}


/*
 *****************************************************************************
 * GdpThreadTask --
 *
 * The gdp task thread routine.
 *
 * @param[in] ctx   The application context
 * @param[in] data  Data pointer, not used
 *
 *****************************************************************************
 */

static void
GdpThreadTask(ToolsAppCtx *ctx, // IN
              void *data)       // IN
{
   TaskContext taskCtx;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   GdpTaskCtxInit(&taskCtx);

   do {
      int timeout;
      GdpError gdpErr;
      GdpTaskEvent taskEvent = GDP_TASK_EVENT_NONE;

      /*
       * Part 1 inside the loop:
       * Checks and processes one pending event at idle state.
       */

      if (taskCtx.mode == GDP_TASK_MODE_NONE) {
         ASSERT(taskCtx.state == GDP_TASK_STATE_IDLE);
         ASSERT(taskCtx.timeoutAt == GDP_TIMEOUT_AT_INFINITE);

         if (taskCtx.publishPending) { // Higher priority
            taskCtx.publishPending = FALSE;
            GdpTaskProcessPublish(&taskCtx);
         } else if (!g_queue_is_empty(&taskCtx.requests)) {
            /*
             * History request pending.
             *
             * GdpTaskPublishHistory clears history request queue if it fails
             * to kick off the state machine.
             */
            GdpTaskPublishHistory(&taskCtx);
         }
      }

      /*
       * Part 2 inside the loop:
       * Gets and processes one event at a time, then loops back to part 1
       * except for the stop event.
       */

      timeout = GdpTaskGetTimeout(&taskCtx);
      if (timeout == 0) {
         GdpTaskProcessEvents(&taskCtx, GDP_TASK_EVENT_TIMEOUT);
         continue;
      }

      /*
       * timeout == GDP_WAIT_INFINITE means taskCtx.mode == GDP_TASK_MODE_NONE
       * and taskCtx.state == GDP_TASK_STATE_IDLE. There should be no pending
       * publish event or history request in this case.
       */
      ASSERT(timeout != GDP_WAIT_INFINITE ||
             (!taskCtx.publishPending &&
              g_queue_is_empty(&taskCtx.requests)));

      gdpErr = GdpTaskWaitForEvents(timeout, &taskEvent);
      if (gdpErr != GDP_ERROR_SUCCESS) {
         continue;
      }

      if (taskEvent == GDP_TASK_EVENT_STOP) {
         /*
          * In case the publish event comes at the same time,
          * only the stop event is handled, so do not check
          * (taskCtx.mode == GDP_TASK_MODE_PUBLISH) here.
          */
         gPublishState.gdpErr = GDP_ERROR_STOP;
         GdpSetEvent(gPublishState.eventGetResult);
         break;
      }

      GdpTaskProcessEvents(&taskCtx, taskEvent);

   } while (!Atomic_ReadBool(&gPluginState.stopped));

   GdpTaskCtxDestroy(&taskCtx);

   g_debug("%s: Exiting ...\n", __FUNCTION__);
}


/*
 *****************************************************************************
 * GdpThreadInterrupt --
 *
 * Interrupts the gdp task thread to exit.
 *
 * @param[in] ctx   The application context
 * @param[in] data  Data pointer, not used
 *
 *****************************************************************************
 */

static void
GdpThreadInterrupt(ToolsAppCtx *ctx, // IN
                   void *data)       // IN
{
   g_debug("%s: Entering ...\n", __FUNCTION__);
   ASSERT(Atomic_ReadBool(&gPluginState.started));
   Atomic_WriteBool(&gPluginState.stopped, TRUE);
   GdpSetEvent(gPluginState.eventStop);
}


/*
 ******************************************************************************
 * GdpInit --
 *
 * Initializes plugin state data and resources.
 *
 * @param[in] ctx  The application context
 *
 ******************************************************************************
 */

static void
GdpInit(ToolsAppCtx *ctx) // IN
{
   gPluginState.ctx = ctx;
   Atomic_WriteBool(&gPluginState.started, FALSE);

#if defined(_WIN32)
   gPluginState.wsaStarted = FALSE;
#endif

   gPluginState.vmciFd = -1;
   gPluginState.vmciFamily = -1;
   gPluginState.sock = INVALID_SOCKET;
#if defined(_WIN32)
   gPluginState.eventNetwork = GDP_INVALID_EVENT;
#endif

   gPluginState.eventStop = GDP_INVALID_EVENT;
   Atomic_WriteBool(&gPluginState.stopped, FALSE);

   gPluginState.eventConfig = GDP_INVALID_EVENT;

   gPublishState.eventPublish = GDP_INVALID_EVENT;

   gPublishState.eventGetResult = GDP_INVALID_EVENT;
}


/*
 ******************************************************************************
 * GdpStart --
 *
 * Creates plugin resources and starts the task thread for data publishing.
 *
 * @return TRUE on success.
 * @return FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
GdpStart(void)
{
   Bool retVal = FALSE;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   ASSERT(!Atomic_ReadBool(&gPluginState.started));

#if defined(_WIN32)
   {
      WSADATA wsaData;
      int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
      if (res != 0) {
         g_critical("%s: WSAStartup failed: error=%d.\n",
                    __FUNCTION__, res);
         return FALSE;
      }

      gPluginState.wsaStarted = TRUE;
   }
#endif

   gPluginState.vmciFamily = VMCISock_GetAFValueFd(&gPluginState.vmciFd);
   if (gPluginState.vmciFamily == -1) {
      g_critical("%s: Failed to get vSocket address family value.\n",
                 __FUNCTION__);
      goto exit;
   }

   if (!GdpCreateSocket()) {
      g_critical("%s: Failed to create VMCI datagram socket.\n",
                 __FUNCTION__);
      goto exit;
   }

#if defined(_WIN32)
   gPluginState.eventNetwork = GdpCreateEvent();
   if (gPluginState.eventNetwork == GDP_INVALID_EVENT) {
      g_critical("%s: GdpCreateEvent for network failed: error=%d.\n",
                 __FUNCTION__, GetSysErr());
      goto exit;
   }
#endif

   gPluginState.eventStop = GdpCreateEvent();
   if (gPluginState.eventStop == GDP_INVALID_EVENT) {
      g_critical("%s: GdpCreateEvent for stop failed: error=%d.\n",
                 __FUNCTION__, GetSysErr());
      goto exit;
   }

   gPluginState.eventConfig = GdpCreateEvent();
   if (gPluginState.eventConfig == GDP_INVALID_EVENT) {
      g_critical("%s: GdpCreateEvent for config failed: error=%d.\n",
                 __FUNCTION__, GetSysErr());
      goto exit;
   }

   gPublishState.eventPublish = GdpCreateEvent();
   if (gPublishState.eventPublish == GDP_INVALID_EVENT) {
      g_critical("%s: GdpCreateEvent for publish failed: error=%d.\n",
                 __FUNCTION__, GetSysErr());
      goto exit;
   }

   gPublishState.eventGetResult = GdpCreateEvent();
   if (gPublishState.eventGetResult == GDP_INVALID_EVENT) {
      g_critical("%s: GdpCreateEvent for get-result failed: error=%d.\n",
                 __FUNCTION__, GetSysErr());
      goto exit;
   }

   if (!ToolsCorePool_StartThread(gPluginState.ctx, "GdpThread",
                                  GdpThreadTask,
                                  GdpThreadInterrupt,
                                  NULL, NULL)) {
      g_critical("%s: Failed to start the gdp task thread.\n", __FUNCTION__);
      goto exit;
   }

   Atomic_WriteBool(&gPluginState.started, TRUE);
   retVal = TRUE;

exit:
   if (!retVal) {
      GdpDestroy();
   }

   return retVal;
}


/*
 ******************************************************************************
 * GdpDestroy --
 *
 * Destroys plugin resources.
 *
 ******************************************************************************
 */

static void
GdpDestroy(void)
{
   g_debug("%s: Entering ...\n", __FUNCTION__);

   GdpCloseSocket();

   if (gPluginState.vmciFd != -1) {
      VMCISock_ReleaseAFValueFd(gPluginState.vmciFd);
      gPluginState.vmciFd = -1;
   }

#if defined(_WIN32)
   GdpCloseEvent(&gPluginState.eventNetwork);
#endif

   GdpCloseEvent(&gPluginState.eventStop);

   GdpCloseEvent(&gPluginState.eventConfig);

   GdpCloseEvent(&gPublishState.eventPublish);

   GdpCloseEvent(&gPublishState.eventGetResult);

#if defined(_WIN32)
   if (gPluginState.wsaStarted) {
      WSACleanup();
      gPluginState.wsaStarted = FALSE;
   }
#endif
}


/*
 ******************************************************************************
 * GdpPublish --
 *
 * Publishes guest data to host side gdp daemon.
 *
 * @param[in]          createTime  UTC timestamp, in number of micro-
 *                                 seconds since January 1, 1970 UTC.
 * @param[in]          topic       Topic
 * @param[in,optional] token       Token, can be NULL
 * @param[in,optional] category    Category, can be NULL that defaults to
 *                                 "application"
 * @param[in]          data        Buffer containing data to publish
 * @param[in]          dataLen     Buffer length
 * @param[in]          cacheData   Cache the data if TRUE
 *
 * @return GDP_ERROR_SUCCESS on success.
 * @return Other GdpError code otherwise.
 *
 ******************************************************************************
 */

static GdpError
GdpPublish(gint64 createTime,     // IN
           const gchar *topic,    // IN
           const gchar *token,    // IN, OPTIONAL
           const gchar *category, // IN, OPTIONAL
           const gchar *data,     // IN
           guint32 dataLen,       // IN
           gboolean cacheData)    // IN
{
   GdpError gdpErr;

   g_debug("%s: Entering ...\n", __FUNCTION__);

   if (topic == NULL || *topic == '\0') {
      g_info("%s: Missing topic.\n", __FUNCTION__);
      return GDP_ERROR_INVALID_DATA;
   }

   if (data == NULL || dataLen == 0) {
      g_info("%s: Topic '%s' has no data.\n", __FUNCTION__, topic);
      return GDP_ERROR_INVALID_DATA;
   }

   if (token != NULL && *token == '\0') {
      token = NULL;
   }

   if (category != NULL && *category == '\0') {
      category = NULL;
   }

   g_mutex_lock(&gPublishState.mutex);

   if (Atomic_ReadBool(&gPluginState.stopped)) {
      gdpErr = GDP_ERROR_STOP;
      goto exit;
   }

   if (!Atomic_ReadBool(&gPluginState.started) &&
       !GdpStart()) {
      gdpErr = GDP_ERROR_GENERAL;
      goto exit;
   }

   gPublishState.createTime = createTime;
   gPublishState.topic = topic;
   gPublishState.token = token;
   gPublishState.category = category;
   gPublishState.data = data;
   gPublishState.dataLen = dataLen;
   gPublishState.cacheData = cacheData;

   GdpSetEvent(gPublishState.eventPublish);

   do {
      gdpErr = GdpWaitForEvent(gPublishState.eventGetResult,
                               GDP_WAIT_INFINITE);
      if (gdpErr == GDP_ERROR_SUCCESS) {
         gdpErr = gPublishState.gdpErr;
         break;
      }
   } while (!Atomic_ReadBool(&gPluginState.stopped));

   GdpResetEvent(gPublishState.eventGetResult);

exit:
   g_mutex_unlock(&gPublishState.mutex);
   g_debug("%s: Exiting with gdp error: %s.\n",
           __FUNCTION__, gdpErrMsgs[gdpErr]);
   return gdpErr;
}


/*
 *-----------------------------------------------------------------------------
 * GdpConfReload --
 *
 * Handles gdp config change.
 *
 * @param[in] src   The source object, unused
 * @param[in] ctx   The application context
 * @param[in] data  Unused
 *
 *-----------------------------------------------------------------------------
 */

static void
GdpConfReload(gpointer src,     // IN
              ToolsAppCtx *ctx, // IN
              gpointer data)    // IN
{
   if (!Atomic_ReadBool(&gPluginState.started)) {
      return;
   }

   GdpSetEvent(gPluginState.eventConfig);
}


/*
 ******************************************************************************
 * GdpShutdown --
 *
 * Cleans up on shutdown.
 *
 * @param[in] src   The source object, unused
 * @param[in] ctx   The application context
 * @param[in] data  Unused
 *
 ******************************************************************************
 */

static void
GdpShutdown(gpointer src,     // IN
            ToolsAppCtx *ctx, // IN
            gpointer data)    // IN
{
   g_debug("%s: Entering ...\n", __FUNCTION__);
   ASSERT(!Atomic_ReadBool(&gPluginState.started) ||
          Atomic_ReadBool(&gPluginState.stopped));
   g_object_set(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GDP, NULL, NULL);
   GdpDestroy();
}


/*
 ******************************************************************************
 * ToolsOnLoad --
 *
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in] ctx  The application context
 *
 * @return The registration data.
 *
 ******************************************************************************
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx) // IN
{
   /*
    * Returns NULL to disable the plugin if not running in vmsvc daemon.
    */
   if (!TOOLS_IS_MAIN_SERVICE(ctx)) {
      g_info("%s: Not running in vmsvc daemon: container name='%s'.\n",
             __FUNCTION__, ctx->name);
      return NULL;
   }

   /*
    * Returns NULL to disable the plugin if not running in a VMware VM.
    */
   if (!ctx->isVMware) {
      g_info("%s: Not running in a VMware VM.\n", __FUNCTION__);
      return NULL;
   }

   /*
    * Returns NULL to disable the plugin if VM is not running on ESX host.
    */
   {
      uint32 vmxVersion = 0;
      uint32 vmxType = VMX_TYPE_UNSET;
      if (!VmCheck_GetVersion(&vmxVersion, &vmxType) ||
          vmxType != VMX_TYPE_SCALABLE_SERVER) {
         g_info("%s: VM is not running on ESX host.\n", __FUNCTION__);
         return NULL;
      }
   }

   GdpInit(ctx);

   {
      static ToolsPluginSvcGdp svcGdp = { GdpPublish };
      static ToolsPluginData regData = { "gdp", NULL, NULL, NULL };

      ToolsServiceProperty propGdp = { TOOLS_PLUGIN_SVC_PROP_GDP };

      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CONF_RELOAD, GdpConfReload, NULL },
         { TOOLS_CORE_SIG_SHUTDOWN, GdpShutdown, NULL },
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS,
           VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
      };

      ctx->registerServiceProperty(ctx->serviceObj, &propGdp);
      g_object_set(ctx->serviceObj, TOOLS_PLUGIN_SVC_PROP_GDP, &svcGdp, NULL);

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
      return &regData;
   }
}
