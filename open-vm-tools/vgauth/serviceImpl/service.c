/*********************************************************
 * Copyright (C) 2011-2016,2019 VMware, Inc. All rights reserved.
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

/**
 * @file service.c --
 *
 *    Creates and sets up ServiceConnection
 */

#include "VGAuthLog.h"
#include "serviceInt.h"
#include "VGAuthProto.h"
#include "VGAuthUtil.h"
#ifdef _WIN32
#include "winUtil.h"
#endif

static ServiceStartListeningForIOFunc startListeningIOFunc = NULL;
static ServiceStopListeningForIOFunc stopListeningIOFunc = NULL;

static GHashTable *listenConnectionMap = NULL;

static gboolean ServiceInitListenConnectionMap();
static ServiceConnection *ServiceLookupListenConnection(const char *userName);
static void ServiceMapListenConnection(const char *userName,
                                       ServiceConnection *userConn);

// Logs information about when connections are reaped.
#define LISTENCONN_TABLE_DEBUG 0
/*
 * Set to turn off listen connecton re-use.  Helps find issues with
 * user deletion.
 */
#define LISTENCONN_NO_REUSE 0

/*
 * Throw out idle listen connections after 30 minutes.
 */

#define  LISTENCONN_EXPIRE_TIME_IN_SECONDS_DEFAULT (30 * 60)

static int listenConnExpireTime = LISTENCONN_EXPIRE_TIME_IN_SECONDS_DEFAULT;
static int reapCheckTime;

PrefHandle gPrefs = NULL;
static gboolean reapTimerRunning = FALSE;

gboolean gVerboseLogging = FALSE;

/*
 * The directory where the service binary resides.
 */
char *gInstallDir;

/*
 * The data connection map that keeps tracks of the number of connection for
 * each user. PairType = <gchar *, int *>
 */
static GHashTable *dataConnectionMap = NULL;
static int dataConnectionMaxPerUser =
   VGAUTH_PREF_DEFAULT_MAX_DATA_CONNECTIONS_PER_USER;

static gboolean ServiceInitDataConnectionMap();
static void ServiceDataConnectionIncrement(const char *user);
static void ServiceDataConnectionDecrement(const char *user);
gboolean ServiceDataConnectionCheckLimit(const char *user);
gboolean ServiceIsSuperUser(const char *user);


/*
 ******************************************************************************
 * ServiceConnectionGetNextConnectionId --                               */ /**
 *
 * Get an unqiue connection Id
 *
 * @return a connection Id
 *
 ******************************************************************************
 */

static int
ServiceConnectionGetNextConnectionId(void)
{
   static int TheNextId = 0;
   return TheNextId++;
}


/*
 ******************************************************************************
 * ServiceRegisterIOFunctions --                                         */ /**
 *
 * Sets up IO function hooks, for when the service library needs to call
 * out to IO control.
 *
 * @param[in]   startFunc       The function called when we want to start
 *                              listening for IO on a connection.
 * @param[in]   stopFunc        The function called when we no longer
 *                              care about IO on a connection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceRegisterIOFunctions(ServiceStartListeningForIOFunc startFunc,
                           ServiceStopListeningForIOFunc stopFunc)
{
   startListeningIOFunc = startFunc;
   stopListeningIOFunc = stopFunc;

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceCreatePublicConnection --                                     */ /**
 *
 * Creates the connection that listens on the public pipe.
 *
 * @param[out]  returnConn        The new connection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceCreatePublicConnection(ServiceConnection **returnConn) // OUT
{
   ServiceConnection *newConn;
   VGAuthError err;

   if(!ServiceInitListenConnectionMap()) {
      VGAUTH_LOG_WARNING("%s", "Failed to initialize the listen connection map");
      return VGAUTH_E_FAIL;
   }

   if(!ServiceInitDataConnectionMap()) {
      VGAUTH_LOG_WARNING("%s", "Failed to initialize the data connection map");
      return VGAUTH_E_FAIL;
   }

   newConn = g_malloc0(sizeof(ServiceConnection));

#ifdef _WIN32
   newConn->hComm = INVALID_HANDLE_VALUE;
#endif

   newConn->connId = ServiceConnectionGetNextConnectionId();

   newConn->pipeName = g_strdup(SERVICE_PUBLIC_PIPE_NAME);
   newConn->userName = g_strdup(SUPERUSER_NAME);

   err = ServiceNetworkListen(newConn, FALSE);

   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to setup public listen channel\n", __FUNCTION__);
      ServiceConnectionShutdown(newConn);
      return err;
   }

   newConn->isPublic = TRUE;
   *returnConn = newConn;

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceEncodeUserName --                                              */ /**
 *
 * Esacape the backslash domain\username separator on Windows
 *
 * @param[in]   userName       The original user name
 *
 * @return   the result ascii string of the encoded user name
 *
 ******************************************************************************
 */

gchar *
ServiceEncodeUserName(const char *userName)
{
#ifdef _WIN32
   char *user = NULL;
   char *domain = NULL;
   char *result = NULL;
   BOOL ok;

   ok = WinUtil_ParseUserName(userName, '\\', &user, &domain);
   ASSERT(ok);

   if (domain) {
      result = g_strdup_printf("%s+%s", domain, user);
   } else {
      result = g_strdup(userName);
   }

   g_free(user);
   g_free(domain);

   return result;
#else
   return g_strdup(userName);
#endif
}


/*
 ******************************************************************************
 * ServiceDecodeUserName --                                              */ /**
 *
 * Restore an already escaped name to the domain\user format on Windows
 *
 * @param[in]   userName       The encoded user name
 *
 * @return   the result ascii string of the original user name
 *
 ******************************************************************************
 */

gchar *
ServiceDecodeUserName(const char *userName)
{
#ifdef _WIN32
   char *user = NULL;
   char *domain = NULL;
   char *result = NULL;
   BOOL ok;

   ok = WinUtil_ParseUserName(userName, '+', &user, &domain);
   ASSERT(ok);

   if (domain) {
      result = g_strdup_printf("%s"DIRSEP"%s", domain, user);
   } else {
      result = g_strdup(userName);
   }

   g_free(user);
   g_free(domain);

   return result;
#else
   return g_strdup(userName);
#endif
}


/*
 ******************************************************************************
 * ServiceUserNameToPipeName --                                          */ /**
 *
 * Map a user name into its VGAuth pipe name, escaping the backslash on Windows
 *
 * @param[in]   userName       The user name
 *
 * @return   the result ascii string
 *
 ******************************************************************************
 */

static gchar *
ServiceUserNameToPipeName(const char *userName)
{
   gchar *escapedName = ServiceEncodeUserName(userName);
   gchar *pipeName = g_strdup_printf("%s-%s",
                                     SERVICE_PUBLIC_PIPE_NAME,
                                     escapedName);

   g_free(escapedName);
   return pipeName;
}


/*
 ******************************************************************************
 * ServiceCreateUserConnection --                                        */ /**
 *
 * Creates a connection that listens on user private pipe.
 *
 * @param[in]   userName          The name of the user for the pipe.
 * @param[out]  returnConn        The new connection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceCreateUserConnection(const char *userName,
                            ServiceConnection **returnConn) // OUT
{
   ServiceConnection *newConn;
   VGAuthError err;

   newConn = g_malloc0(sizeof(ServiceConnection));

#ifdef _WIN32
   newConn->hComm = INVALID_HANDLE_VALUE;
#endif

   newConn->connId = ServiceConnectionGetNextConnectionId();

   newConn->userName = g_strdup(userName);
   newConn->pipeName = ServiceUserNameToPipeName(userName);

   err = ServiceNetworkListen(newConn, TRUE);

   if (VGAUTH_E_OK != err) {
      ServiceConnectionShutdown(newConn);
      Warning("%s: failed to setup private listen channel\n", __FUNCTION__);
      return err;
   }

   newConn->isPublic = FALSE;
   newConn->isListener = TRUE;
   *returnConn = newConn;

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceConnectionClone --                                             */ /**
 *
 * Copies a ServiceConnection structure.
 *
 * @param[in]   parent          The ServiceConnection to be cloned.
 * @param[out]  clone           The new connection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceConnectionClone(ServiceConnection *parent,
                       ServiceConnection **clone)        // OUT
{
   ServiceConnection *newConn;

   ASSERT(parent);
   ASSERT(clone);

   newConn = g_malloc0(sizeof(ServiceConnection));

#ifdef _WIN32
   newConn->hComm = INVALID_HANDLE_VALUE;
#endif

   newConn->connId = ServiceConnectionGetNextConnectionId();

   newConn->pipeName = g_strdup(parent->pipeName);
   newConn->userName = g_strdup(parent->userName);

   newConn->isPublic = parent->isPublic;

   *clone = newConn;

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceConnectionShutdown --                                          */ /**
 *
 * Shuts down and frees a ServiceConnection.  Input is no
 * longer watched for on this connection, and any network resources
 * are closed.
 *
 * @param[in]   conn          The ServiceConnection to shut down.
 *
 ******************************************************************************
 */

void
ServiceConnectionShutdown(ServiceConnection *conn)
{
   if (NULL == conn) {
      return;
   }

   ASSERT(stopListeningIOFunc);

   (* stopListeningIOFunc) (conn);

   ServiceNetworkCloseConnection(conn);

   ServiceProtoCleanupParseState(conn);

   if (conn->isListener) {
      ServiceNetworkRemoveListenPipe(conn);
   }

   if (conn->dataConnectionIncremented) {
      ServiceDataConnectionDecrement(conn->userName);
   }

   g_free(conn->pipeName);
   g_free(conn->userName);

   g_free(conn);
}


/*
 ******************************************************************************
 * ServiceHashConnectionShutdown --                                      */ /**
 *
 * Wrapper for ServiceConnectionShutdown() to be called by the hash table's free.
 *
 * @param[in]   userData        The ServiceConnection to shut down.
 *
 ******************************************************************************
 */

static void
ServiceHashConnectionShutdown(gpointer userData)
{
   ServiceConnection *conn = (ServiceConnection *) userData;

   ServiceConnectionShutdown(conn);
}


/*
 ******************************************************************************
 * ServiceStartUserConnection --                                         */ /**
 *
 * Creates a new listen connection for 'userName' if there is none.
 * If there is already the listen connection for 'userName' reuse it.
 *
 * @param[in]   userName          The user for whom to create a new listener.
 * @param[out]  pipeName          The name of the new pipe to be used.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceStartUserConnection(const char *userName,
                           char **pipeName)
{
   ServiceConnection *userConn;
   VGAuthError err = VGAUTH_E_OK;
   gboolean connReuse = FALSE;
   gboolean bRet;

   /* Check if we can find the listening connection in the map */
   userConn = ServiceLookupListenConnection(userName);
   if (userConn) {
      connReuse = TRUE;
   }
#if LISTENCONN_NO_REUSE
   // remove from hash table which will call ServiceConnectionShutdown()
   (void) g_hash_table_remove(listenConnectionMap, userName);
#else
   bRet = UsercheckUserExists(userName);
   if (userConn) {
      if (bRet) {
         /*
          * If we have a cached conn -- and the user is still around (seems
          * like a weird corner case, but the 'deleted user' unit test hits
          * this) -- reuse.
          */
         *pipeName = g_strdup(userConn->pipeName);
         goto done;
       } else {
         /*
          * Have a conn, but can't find the user -- clean up before we make
          * a new conn.  This can happen if the service is hit by a network
          * glitch or the LDAP bug (see usercheck.c for details).
          *
          * Throw out the old, let it (try) to make a new.  If its the
          * LDAP bug, the listen will succeed, and we don't want to
          * end up with two userConn's for the same user.
          */
          Debug("%s: Already have a connection for user '%s', but the user "
                "check failed, so tearing down the connection and trying "
                "to rebuild\n",
                __FUNCTION__, userName);
         (void) g_hash_table_remove(listenConnectionMap, userName);
         // fall thru and let a new conn get created
      }
   }
#endif

   err = ServiceCreateUserConnection(userName, &userConn);
   if (err != VGAUTH_E_OK) {
      goto done;
   }

   err = (* startListeningIOFunc) (userConn);
   if (err != VGAUTH_E_OK) {
      goto done;
   }

   *pipeName = g_strdup(userConn->pipeName);

   /* Insert the new connection into the map */
   ServiceMapListenConnection(userName, userConn);

done:
   if (err != VGAUTH_E_OK && userConn) {
      if (connReuse) {
         VGAUTH_LOG_DEBUG("%s: removing dead userConn for %s from hashtable",
                          __FUNCTION__, userName);
         (void) g_hash_table_remove(listenConnectionMap, userName);
      } else {
         VGAUTH_LOG_DEBUG("%s: removing failed userConn for %s",
                          __FUNCTION__, userName);
         ServiceConnectionShutdown(userConn);
      }
   } else if (NULL != userConn) {
      g_get_current_time(&userConn->lastUse);
   }

   return err;
}


/*
 ******************************************************************************
 * ServiceNetworkIsConnectionPrivateSuperUser --                         */ /**
 *
 * Checks to see if the connection is private to superUser or a member
 * of the Administrators group.
 *
 * @param[int]  conn        The connection.
 *
 * @return TRUE if the connection is private to superUser
 *
 ******************************************************************************
 */

gboolean
ServiceNetworkIsConnectionPrivateSuperUser(ServiceConnection *conn)
{
   if (conn->isPublic) {
      return FALSE;
   } else {
      return ServiceIsSuperUser(conn->userName);
   }
}


/*
 ******************************************************************************
 * ServiceListenHashRemoveCheck --                                       */ /**
 *
 * Hash table remove check function.
 *
 * @param[in]  key       Hash table key.
 * @param[in]  value     Hash table value.
 * @param[in]  userData  Any user data (unused)
 *
 * @returns TRUE if the entry is to be removed from the table.
 *
 ******************************************************************************
 */

static gboolean
ServiceListenHashRemoveCheck(gpointer key,
                             gpointer value,
                             gpointer userData)
{
   ServiceConnection *conn = (ServiceConnection *) value;;

   if (Util_CheckExpiration(&(conn->lastUse), listenConnExpireTime)) {
      Debug("%s: removing old listen conn for user %s\n", __FUNCTION__, conn->userName);
      return TRUE;
   }

   return FALSE;
}


/*
 ******************************************************************************
 * ServiceListenReapTimerCallback --                                     */ /**
 *
 * Callback for a timer which looks for old user listen connections to reap.
 *
 * @param[in] userData   Any user data (unused).
 *
 * @return TRUE if the timer is to be kept, FALSE if not.
 *
 ******************************************************************************
 */

static gboolean
ServiceListenReapTimerCallback(gpointer userData)
{
   int numRemoved;

#if LISTENCONN_TABLE_DEBUG
   Debug("%s: looking for listen connection to reap\n", __FUNCTION__);
#endif

   numRemoved = g_hash_table_foreach_remove(listenConnectionMap,
                                            ServiceListenHashRemoveCheck, NULL);

#if LISTENCONN_TABLE_DEBUG
   Debug("%s: reaped %d listen connection\n", __FUNCTION__, numRemoved);
#endif

   /*
    * Keep the timer running if we still have entries in the hashtable.
    */
   reapTimerRunning = (g_hash_table_size(listenConnectionMap) > 0);
#if LISTENCONN_TABLE_DEBUG
   Debug("%s: reapTimerRunning? %d\n", __FUNCTION__, reapTimerRunning);
#endif

   return reapTimerRunning;
}


/*
 ******************************************************************************
 * ServiceInitListenConnectionPrefs --                                    */ /**
 *
 * Reads any preferences for the listen connection.
 *
 ******************************************************************************
 */

void
ServiceInitListenConnectionPrefs(void)
{
   listenConnExpireTime = Pref_GetInt(gPrefs,
                                      VGAUTH_PREF_NAME_LISTEN_TTL,
                                      VGAUTH_PREF_GROUP_NAME_SERVICE,
                                      LISTENCONN_EXPIRE_TIME_IN_SECONDS_DEFAULT);
   if (listenConnExpireTime <= 0) {
      Warning(VGAUTH_PREF_NAME_LISTEN_TTL
              " set to invalid value of %d, using default of %d instead\n",
              listenConnExpireTime, LISTENCONN_EXPIRE_TIME_IN_SECONDS_DEFAULT);
      listenConnExpireTime = LISTENCONN_EXPIRE_TIME_IN_SECONDS_DEFAULT;
   }
   Debug("%s: listen conn TTL set to %d seconds\n",
         __FUNCTION__, listenConnExpireTime);

   /*
    * Compute the reapCheckTime based on the TTL.
    */
   reapCheckTime = listenConnExpireTime / 10;
   if (reapCheckTime <= 0) {
      reapCheckTime = 1;
   }
   Debug("%s: computed reapCheckTime as %d seconds\n",
         __FUNCTION__, reapCheckTime);

}


/*
 ******************************************************************************
 * ServiceInitListenConnectionMap --                                     */ /**
 *
 * Initialize the listen connection map.
 *
 * @return  TRUE if successful. FALSE otherwise.
 *
 ******************************************************************************
 */

gboolean
ServiceInitListenConnectionMap(void)
{
   ServiceInitListenConnectionPrefs();

   ASSERT(listenConnectionMap == NULL);

   listenConnectionMap = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, ServiceHashConnectionShutdown);

   return (listenConnectionMap != NULL);
}


/*
 ******************************************************************************
 * ServiceLookupListenConnection --                                      */ /**
 *
 * Checks to see if the listening connection for the userName is started.
 *
 * @param[in]  userName        The user name of the listen connection.
 *
 * @return  The listen connection of the user if it is already created.
 *          NULL otherwise.
 *
 ******************************************************************************
 */

ServiceConnection *
ServiceLookupListenConnection(const char *userName)
{
   ASSERT(listenConnectionMap);

   return (ServiceConnection *)
      g_hash_table_lookup(listenConnectionMap, userName);
}


/*
 ******************************************************************************
 * ServiceMapListenConnection --                                         */ /**
 *
 * Insert the listen connection of the user into the listen connecton map.
 *
 * @param[in]  userName        The user name of the listen connection.
 * @param[in]  userConn        The listen connection.
 *
 ******************************************************************************
 */

void
ServiceMapListenConnection(const char *userName,
                           ServiceConnection *userConn)
{
   ASSERT(listenConnectionMap);
   ASSERT(g_hash_table_lookup(listenConnectionMap, userName) == NULL);

   g_hash_table_insert(listenConnectionMap, g_strdup(userName), userConn);

   /*
    * Set up a reap timer if it's not already running.
    */
   if (!reapTimerRunning) {
      (void) g_timeout_add_seconds(reapCheckTime,
                                   ServiceListenReapTimerCallback,
                                   NULL);
      reapTimerRunning = TRUE;
   }
}


/*
 ******************************************************************************
 * Service_ReloadPrefs --                                                */ /**
 *
 * Reload any preferences used by the service implementation.
 *
 ******************************************************************************
 */

void
Service_ReloadPrefs(void)
{
   ServiceInitTicketPrefs();
   ServiceInitListenConnectionPrefs();
   SAML_Reload();
}


/*
 ******************************************************************************
 * Service_Shutdown --                                                */ /**
 *
 * Shutdown the service implementation.
 *
 ******************************************************************************
 */

void
Service_Shutdown(void)
{
   SAML_Shutdown();
}


/*
 ******************************************************************************
 * ServiceAcceptConnection --                                            */ /**
 *
 * Accepts a connection on a socket/pipe
 *
 * @param[in,out] connIn  The connection that owns the socket being accept()ed.
 * @param[in,out] onnOut  The new connection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceAcceptConnection(ServiceConnection *connIn,
                        ServiceConnection *connOut)
{
   VGAuthError err = ServiceNetworkAcceptConnection(connIn, connOut);

   if (err != VGAUTH_E_OK) {
      return err;
   }

   ServiceDataConnectionIncrement(connOut->userName);
   connOut->dataConnectionIncremented = TRUE;

   // Check the per user connection limit
   if (!ServiceDataConnectionCheckLimit(connOut->userName)) {
      ServiceReplyTooManyConnections(connOut, dataConnectionMaxPerUser);
      return VGAUTH_E_TOO_MANY_CONNECTIONS;
   } else {
      return VGAUTH_E_OK;
   }
}

/*
 ******************************************************************************
 * ServiceInitDataConnectionPrefs --                                     */ /**
 *
 * Reads any preferences for data connections.
 *
 ******************************************************************************
 */

void
ServiceInitDataConnectionPrefs(void)
{
   dataConnectionMaxPerUser =
      Pref_GetInt(gPrefs,
                  VGAUTH_PREF_NAME_MAX_DATA_CONNECTIONS_PER_USER,
                  VGAUTH_PREF_GROUP_NAME_SERVICE,
                  VGAUTH_PREF_DEFAULT_MAX_DATA_CONNECTIONS_PER_USER);

   if (dataConnectionMaxPerUser <= 0) {
      Warning(VGAUTH_PREF_NAME_MAX_DATA_CONNECTIONS_PER_USER
              " set to invalid value of %d, using default of %d instead\n",
              dataConnectionMaxPerUser,
              VGAUTH_PREF_DEFAULT_MAX_DATA_CONNECTIONS_PER_USER);
      dataConnectionMaxPerUser =
         VGAUTH_PREF_DEFAULT_MAX_DATA_CONNECTIONS_PER_USER;
   }

   VGAUTH_LOG_DEBUG("Maximum number of data connections per user set to %d",
                    dataConnectionMaxPerUser);
}


/*
 ******************************************************************************
 * ServiceInitDataConnectionMap --                                       */ /**
 *
 * Initialize the data connection map.
 *
 * @return  TRUE if successful. FALSE otherwise.
 *
 ******************************************************************************
 */

gboolean
ServiceInitDataConnectionMap(void)
{
   ServiceInitDataConnectionPrefs();

   ASSERT(dataConnectionMap == NULL);

   dataConnectionMap = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             g_free, g_free);

   return (dataConnectionMap != NULL);
}


/*
 ******************************************************************************
 * ServiceDataConnectionIncrement --                                     */ /**
 *
 * Increment the connection count for the user
 *
 * @param[in]  user   The user for whom we increment the count
 *
 ******************************************************************************
 */

void
ServiceDataConnectionIncrement(const char *user)
{
   int *pCount = g_hash_table_lookup(dataConnectionMap, user);

   if (pCount) {
      (*pCount)++;
   } else {
      pCount = g_malloc0(sizeof *pCount);
      (*pCount)++;
      g_hash_table_insert(dataConnectionMap, g_strdup(user), pCount);
   }
}


/*
 ******************************************************************************
 * ServiceDataConnectionDecrement --                                     */ /**
 *
 * Decrement the connection count for the user
 *
 * @param[in]  user   The user for whom we decrement the count
 *
 ******************************************************************************
 */

void
ServiceDataConnectionDecrement(const char *user)
{
   int *pCount = g_hash_table_lookup(dataConnectionMap, user);

   ASSERT(pCount);
   ASSERT(pCount > 0);
   (*pCount)--;
}


/*
 ******************************************************************************
 * ServiceDataConnectionDecrement --                                     */ /**
 *
 * Check if the user has exceeded its maximum connection limit
 *
 * @param[in]  user   The user for whom we check its connection limit
 *
 * @return  TRUE   if user can still have more connection(s)
 *          FALSE  otherwise
 *
 ******************************************************************************
 */

gboolean
ServiceDataConnectionCheckLimit(const char *user)
{
   int *pCount = g_hash_table_lookup(dataConnectionMap, user);

   if (!pCount) {
      return TRUE;
   }

   if (*pCount <= dataConnectionMaxPerUser) {
      return TRUE;
   }

   // no limit for the super user
   return ServiceIsSuperUser(user);
}


/*
 ******************************************************************************
 * ServiceIsSuperUser --                                                 */ /**
 *
 * Checks to see if a user is the super user or a member of the Administrator
 * group.
 *
 * @param[int]  user   The user name
 *
 * @return TRUE if the user is the super user
 *         FALSE otherwise
 *
 ******************************************************************************
 */

gboolean
ServiceIsSuperUser(const char *user)
{
#ifdef _WIN32
   /*
    * For Windows, accept either superUser or a member of the Administrator
    * group.  These may overlap, but check both to be safe.
    */
   gboolean bRet;

   bRet = Usercheck_CompareByName(user, SUPERUSER_NAME);
   if (bRet) {
      return TRUE;
   }
   return Usercheck_IsAdminMember(user);
#else
   // On linux, we only care about root
   return Usercheck_CompareByName(user, SUPERUSER_NAME);
#endif
}
