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
 * @file ticket.c --
 *
 *    Code to support ticket creation and sharing.
 */

#include "VGAuthLog.h"
#include "VGAuthUtil.h"
#include "serviceInt.h"
#ifdef _WIN32
#include "winDupHandle.h"
#endif

/*
 * XXX TODO
 *
 * There may be a bug in the ticket implementation.
 *
 * Right now, a ticket doesn't care about any expiration details
 * of the authn method used to create it.  This means a bearer
 * token that expires in 5 minutes could be used to create a ticket
 * with a far longer expiration time.
 *
 * This seems like a possible security problem, and it should instead
 * try to obey the auth time of the original authn method.
 *
 * This may be messy to implement securely, since we currently
 * lose the expiration date.  It could be passed around along with the
 * Subject and AliasInfo we already store, but a bad client could
 * cheat -- but a bad client can do other horrible things.  Since CreateTicket
 * can be called as a normal user, this seems hackable.
 *
 * Should CreateTicket be restricted to superUser?
 *
 * Also -- we currently would support making a ticket from a ticket.
 * Are there any security concerns here?
 */

/*
 * Debugging flags.
 */

/*
 * Set to spew the hashtable on add/remove.
 * Note that this allows secure information (tickets) to be seen.
 */
#define TICKET_TABLE_DEBUG 0


/*
 * Default to 24 hours, to match the SSPI ticket code in vixToolsWin32.c
 */
#define  TICKET_EXPIRE_TIME_IN_SECONDS_DEFAULT (24 * 60 * 60)

static void ServiceReapOldTickets(void);

typedef struct {
   gchar *ticket;
   gchar *userName;
#ifdef _WIN32
   HANDLE userToken;
#endif

   ServiceValidationResultsType type;
   /*
    * XXX may want to turn this into a union if we add data for other
    * validation types.
    */
   ServiceValidationResultsData *svData;

   // XXX For the expiration time, make it relative to the last
   // Validate, like the SSPI code.
   // This may want to be relative to the create time instead.

   // XXX I wanted to use GDateTime, but its in glib 2.26, and we're at 2.24.
   // Its probably a good idea to upgrade asap.
   GTimeVal lastUse;
} TicketInfo;

static GHashTable *ticketTable = NULL;
static int ticketExpireTime = TICKET_EXPIRE_TIME_IN_SECONDS_DEFAULT;
static int reapCheckTime;
static gboolean reapTimerRunning = FALSE;


/*
 ******************************************************************************
 * ServiceFreeValidationResultsData --                                   */ /**
 *
 * @param[in] svd   ServiceValidationResultsData to be freed.
 *
 ******************************************************************************
 */

void
ServiceFreeValidationResultsData(ServiceValidationResultsData *svd)
{
   if (NULL == svd) {
      return;
   }
   g_free(svd->samlSubject);
   ServiceAliasFreeAliasInfoContents(&(svd->aliasInfo));
   g_free(svd);
}


/*
 ******************************************************************************
 * TicketFreeTicketInfo --                                               */ /**
 *
 * @param[in] info   TicketInfo to be freed.
 *
 ******************************************************************************
 */

static void
TicketFreeTicketInfo(gpointer i)
{
   TicketInfo *info = (TicketInfo *) i;

   g_free(info->ticket);
   g_free(info->userName);
   ServiceFreeValidationResultsData(info->svData);
#ifdef _WIN32
   CloseHandle(info->userToken);
#endif
   g_free(info);
}


/*
 ******************************************************************************
 * TicketReapTimerCallback --                                            */ /**
 *
 * Callback for a timer which looks for old tickets to reap.
 *
 * @param[in] userData   Any user data (unused).
 *
 * @return TRUE if the timer is to be kept, FALSE if not.
 *
 ******************************************************************************
 */

static gboolean
TicketReapTimerCallback(gpointer userData)
{
   ServiceReapOldTickets();
   /*
    * Keep the timer running if we still have entries in the hashtable.
    */
   reapTimerRunning = (g_hash_table_size(ticketTable) > 0);

#if TICKET_TABLE_DEBUG
   Debug("%s: reapTimerRunning? %d\n", __FUNCTION__, reapTimerRunning);
#endif

   return reapTimerRunning;
}


/*
 ******************************************************************************
 * ServiceInitTicketPrefs --                                             */ /**
 *
 * Reads preferences used by the ticket code.
 *
 ******************************************************************************
 */

void
ServiceInitTicketPrefs(void)
{
   ticketExpireTime = Pref_GetInt(gPrefs,
                                  VGAUTH_PREF_NAME_TICKET_TTL,
                                  VGAUTH_PREF_GROUP_NAME_TICKET,
                                  TICKET_EXPIRE_TIME_IN_SECONDS_DEFAULT);
   if (ticketExpireTime <= 0) {
      Warning(VGAUTH_PREF_NAME_TICKET_TTL
              " set to invalid value of %d, using default of %d instead\n",
              ticketExpireTime, TICKET_EXPIRE_TIME_IN_SECONDS_DEFAULT);
      ticketExpireTime = TICKET_EXPIRE_TIME_IN_SECONDS_DEFAULT;
   }
   Debug("%s: ticket TTL set to %d seconds\n", __FUNCTION__, ticketExpireTime);

   /*
    * Compute the reapCheckTime based on the TTL.
    */
   reapCheckTime = ticketExpireTime / 10;
   if (reapCheckTime <= 0) {
      reapCheckTime = 1;
   }
   Debug("%s: computed reapCheckTime as %d seconds\n",
         __FUNCTION__, reapCheckTime);

}


/*
 ******************************************************************************
 * ServiceInitTickets --                                                 */ /**
 *
 * Sets up the service ticket table.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceInitTickets(void)
{
   ServiceInitTicketPrefs();

   /*
    * We use the ticket inside the TicketInfo struct as the key, so
    * there's no free function specified for it.
    */
   ticketTable = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       NULL,                 // key destroy func
                                       TicketFreeTicketInfo);// value destroy func

   return VGAUTH_E_OK;
}


#if TICKET_TABLE_DEBUG
/*
 ******************************************************************************
 * DumpTicketTable --                                                    */ /**
 *
 * Hashtable debug code to dump the entire table.
 *
 ******************************************************************************
 */

static void
DumpTicketTable()
{
   GHashTableIter iter;
   gpointer key, value;
   TicketInfo *info;

   g_hash_table_iter_init(&iter, ticketTable);
   while (g_hash_table_iter_next(&iter, &key, &value)) {
      info = (TicketInfo *) value;
      printf("key: %s (%p), val (%p) (ticket %s, user %s)\n",
             (gchar *) key, key, value, info->ticket, info->userName);
   }
}
#endif // TICKET_TABLE_DEBUG


/*
 ******************************************************************************
 * TicketGenerateTicket --                                               */ /**
 *
 * Creates a new string ticket.  The caller needs to double-check that it is
 * not already in use.
 *
 * @param[in]   userName      The userName associated with the ticket to be
 *                            created.
 *
 * @return The new ticket.  The caller is repsonsible for freeing it.
 *
 ******************************************************************************
 */

static gchar *
TicketGenerateTicket(const gchar *userName)
{
#define RAND_BUF_SIZE   8
   guchar rndBuf[RAND_BUF_SIZE];
   gchar *newTicket;
   gchar *b64rnd;
   VGAuthError err;

   err = ServiceRandomBytes(RAND_BUF_SIZE, rndBuf);
   if (VGAUTH_E_OK != err) {
      return NULL;
   }

   b64rnd = g_base64_encode(rndBuf, RAND_BUF_SIZE);

   /*
    * Use a constant string, the username, and a 256 bits
    * of base64'd random data.
    */
   newTicket = g_strdup_printf("Ticket-%s-%s", userName, b64rnd);

   g_free(b64rnd);

   return newTicket;
}


/*
 ******************************************************************************
 * ServiceCreateTicketInfo --                                            */ /**
 *
 * Creates a TicketInfo object associated with user, adds it to the ticket
 * table and returns it.
 *
 * @param[in]   userName      The userName associated with the ticket to be
 * @param[in]   type          The type of validation used to create the
 *                            ticket.
 * @param[in]   svData        Any SAML validation data if the @a type is
 *                            VAIDATION_TYPE_SAML.
 * @param[out]  info          The ticket info object created for the new ticket
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
ServiceCreateTicketInfo(const char *userName,
                        const ServiceValidationResultsType type,
                        const ServiceValidationResultsData *svData,
                        TicketInfo **info)
{
   VGAuthError retCode = VGAUTH_E_FAIL;
   TicketInfo *newInfo;
   gchar *newTicket = NULL;

   /*
    * Get a new ticket, and be sure it's not a dup.
    */
   while (TRUE) {
      g_free(newTicket);
      newTicket = TicketGenerateTicket(userName);
      if (NULL == newTicket) {
         VGAUTH_LOG_WARNING("TicketGenerateTicket() failed, user = %s", userName);
         goto done;
      } else if (NULL == g_hash_table_lookup(ticketTable, newTicket)) {
         break;
      }
   }

   newInfo = g_malloc0(sizeof(TicketInfo));

   /*
    * GHashTable needs both key and value to be allocated outside of it,
    * so pass the ownership of newTicket to newInfo
    */
   newInfo->ticket = newTicket;
   newTicket = NULL;

   newInfo->userName = g_strdup(userName);
   g_get_current_time(&newInfo->lastUse);

   newInfo->type = type;
   if (VALIDATION_RESULTS_TYPE_SAML == type) {
      newInfo->svData = g_malloc0(sizeof(ServiceValidationResultsData));
      newInfo->svData->samlSubject = g_strdup(svData->samlSubject);
      ServiceAliasCopyAliasInfoContents(&(svData->aliasInfo),
                                        &(newInfo->svData->aliasInfo));
   }


   g_hash_table_insert(ticketTable, newInfo->ticket, newInfo);

#if TICKET_TABLE_DEBUG
   Debug("%s: dumping ticket table after add\n", __FUNCTION__);
   DumpTicketTable();
#endif

   /*
    * Set up a reap timer if it's not already running.
    */
   if (!reapTimerRunning) {
      (void) g_timeout_add_seconds(reapCheckTime,
                                   TicketReapTimerCallback,
                                   NULL);
      reapTimerRunning = TRUE;
   }

   *info = newInfo;

   retCode = VGAUTH_E_OK;

done:

   g_free(newTicket);

   return retCode;
}


#ifdef _WIN32
/*
 ******************************************************************************
 * ServiceCreateTicketWin --                                             */ /**
 *
 * Creates a ticket associated with user, and returns it.
 *
 * @param[in]   userName      The userName associated with the ticket to be
 *                            created.
 * @param[in]   type          The type of validation used to create the
 *                            ticket.
 * @param[in]   svData        Any SAML validation data if the @a type is
 *                            VALIDATION_RESULTS_TYPE_SAML.
 * @param[in]   clientProcHandle  The HANDLE that identifies the client process
 *                            where the access token is duplicated from.
 * @param[in]   token         The user's access token HANDLE from the client.
 * @param[out]  ticket        The new ticket.  To be freed by the caller.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceCreateTicketWin(const char *userName,
                       const ServiceValidationResultsType type,
                       ServiceValidationResultsData *svData,
                       HANDLE clientProcHandle,
                       const char *token,
                       char **ticket)
{
   VGAuthError retCode = VGAUTH_E_FAIL;
   TicketInfo *newInfo = NULL;
   HANDLE copyHandle = ServiceDupHandleFrom(clientProcHandle, token);

   if (!copyHandle) {
      VGAUTH_LOG_WARNING("ServiceDupHandleFrom() failed, user = %s", userName);
      goto done;
   }

   retCode = ServiceCreateTicketInfo(userName, type, svData, &newInfo);
   if (retCode != VGAUTH_E_OK) {
      VGAUTH_LOG_WARNING("ServiceCreateTicketInfo() failed");
      goto done;
   }

   newInfo->userToken = copyHandle;
   copyHandle = NULL;

   *ticket = g_strdup(newInfo->ticket);

done:

   if (copyHandle) {
      CloseHandle(copyHandle);
   }

   return retCode;
}
#else


/*
 ******************************************************************************
 * ServiceCreateTicketPosix --                                           */ /**
 *
 * Creates a ticket associated with user, and returns it.
 *
 * @param[in]   userName      The userName associated with the ticket to be
 *                            created.
 * @param[in]   type          The type of validation used to create the
 *                            ticket.
 * @param[in]   svData        Any SAML validation data if the @a type is
 *                            VALIDATION_RESULTS_TYPE_SAML.
 * @param[out]  ticket        The new ticket.  To be freed by the caller.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceCreateTicketPosix(const char *userName,
                         const ServiceValidationResultsType type,
                         ServiceValidationResultsData *svData,
                         char **ticket)
{
   VGAuthError retCode;
   TicketInfo *newInfo = NULL;

   retCode = ServiceCreateTicketInfo(userName, type, svData, &newInfo);
   if (retCode != VGAUTH_E_OK) {
      goto done;
   }

   *ticket = g_strdup(newInfo->ticket);

done:
   return retCode;
}
#endif


/*
 ******************************************************************************
 * TicketHashRemoveCheck --                                             */ /**
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
TicketHashRemoveCheck(gpointer key,
                      gpointer value,
                      gpointer userData)
{
   TicketInfo *info = (TicketInfo *) value;

   if (Util_CheckExpiration(&(info->lastUse), ticketExpireTime)) {
#if TICKET_TABLE_DEBUG
      Debug("%s: removing old ticket %s\n", __FUNCTION__, info->ticket);
#endif
      return TRUE;
   }

   return FALSE;
}


/*
 ******************************************************************************
 * ServiceReapOldTickets --                                              */ /**
 *
 * Looks for any expired tickets and removes them.
 *
 ******************************************************************************
 */

static void
ServiceReapOldTickets(void)
{
   int numRemoved;

#if TICKET_TABLE_DEBUG
   Debug("%s: dumping ticket table before reap\n", __FUNCTION__);
   DumpTicketTable();
#endif

   numRemoved = g_hash_table_foreach_remove(ticketTable,
                                            TicketHashRemoveCheck, NULL);

#if TICKET_TABLE_DEBUG
   Debug("%s: reaped %d tickets\n", __FUNCTION__, numRemoved);

   Debug("%s: dumping ticket table after reap\n", __FUNCTION__);
   DumpTicketTable();
#endif
}


#ifdef _WIN32
/*
 ******************************************************************************
 * ServiceValidateTicketWin --                                           */ /**
 *
 * Validates a ticket, returning the associated user if it's good.
 *
 * @param[in]   ticket        The ticket being validated.
 * @param[in]   clientProcHandle  The HANDLE that identifes the client process
 *                            where the access token is duplicated into.
 * @param[in]   type          The type of validation used to create the
 *                            ticket.
 * @param[in]   svData        Any SAML validation data if the @a type is
 *                            VALIDATION_RESULTS_TYPE_SAML.
 * @param[out]  userName      The userName associated with the ticket if valid.
 * @param[out]  token         The value in text of the access token that is
 *                            duplicated to the client process if the ticket is
 *                            valid.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceValidateTicketWin(const char *ticket,
                         HANDLE clientProcHandle,
                         char **userName,
                         ServiceValidationResultsType *type,
                         ServiceValidationResultsData **svData,
                         char **token)
{
   TicketInfo *info;
   VGAuthError err = VGAUTH_E_FAIL;
   char *dupTokenInText;
   ServiceValidationResultsData *retSvData = NULL;

   *userName = NULL;
   *token = NULL;
   *type = VALIDATION_RESULTS_TYPE_UNKNOWN;

   ServiceReapOldTickets();
   info = g_hash_table_lookup(ticketTable, ticket);

   if (!info) {
      err = VGAUTH_E_INVALID_TICKET;
      goto done;
   }

   dupTokenInText = ServiceDupHandleTo(clientProcHandle, info->userToken);
   if (!dupTokenInText) {
      VGAUTH_LOG_WARNING("ServiceDupHandleTo() failed, user = %s", info->userName);
      goto done;
   }

   /* all OK */

   g_get_current_time(&info->lastUse);

   *userName = g_strdup(info->userName);
   *token = dupTokenInText;
   if (VALIDATION_RESULTS_TYPE_SAML == info->type) {
      retSvData = g_malloc0(sizeof(ServiceValidationResultsData));
      retSvData->samlSubject = g_strdup(info->svData->samlSubject);
      ServiceAliasCopyAliasInfoContents(&(info->svData->aliasInfo),
                                        &(retSvData->aliasInfo));
   }
   *type = info->type;
   *svData = retSvData;

   err = VGAUTH_E_OK;

done:
   return err;
}
#else


/*
 ******************************************************************************
 * ServiceValidateTicketPosix --                                         */ /**
 *
 * Validates a ticket, returning the associated user if it's good.
 *
 * @param[in]   ticket        The ticket being validated.
 * @param[out]  userName      The userName associated with the ticket if valid.
 * @param[in]   type          The type of validation used to create the
 *                            ticket.
 * @param[in]   svData        Any SAML validation data if the @a type is
 *                            VAIDATION_TYPE_SAML.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceValidateTicketPosix(const char *ticket,
                           char **userName,
                           ServiceValidationResultsType *type,
                           ServiceValidationResultsData **svData)
{
   TicketInfo *info;
   ServiceValidationResultsData *retSvData = NULL;
   VGAuthError err;

   ServiceReapOldTickets();
   info = g_hash_table_lookup(ticketTable, ticket);

   if (NULL != info) {
      // update the last access time
      g_get_current_time(&info->lastUse);

      *userName = g_strdup(info->userName);
      *type = info->type;
      if (VALIDATION_RESULTS_TYPE_SAML == info->type) {
         retSvData = g_malloc0(sizeof(ServiceValidationResultsData));
         retSvData->samlSubject = g_strdup(info->svData->samlSubject);
         ServiceAliasCopyAliasInfoContents(&(info->svData->aliasInfo),
                                           &(retSvData->aliasInfo));
      }

      err = VGAUTH_E_OK;
   } else {
      *userName = NULL;
      *type = VALIDATION_RESULTS_TYPE_UNKNOWN;
      err = VGAUTH_E_INVALID_TICKET;
   }
   *svData = retSvData;

   return err;
}
#endif


/*
 ******************************************************************************
 * ServiceRevokeTicket --                                                */ /**
 *
 * Revokes a ticket.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   ticket        The ticket being validated.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceRevokeTicket(ServiceConnection *conn,
                    const char *ticket)
{
   TicketInfo *info;
   VGAuthError err;

   ServiceReapOldTickets();
   info = g_hash_table_lookup(ticketTable, ticket);

   if (NULL != info) {
      /*
       * Security check.  Allow only SUPERUSER or the ticket's owner to
       * Revoke it.  We do it here instead of in
       * Proto_SecurityCheckRequest() because we want to treat this as a
       * no-op, since otherwise an attacker can confirm the existance of a
       * ticket by getting back a permission error.
       */
      if (!(ServiceNetworkIsConnectionPrivateSuperUser(conn) ||
          Usercheck_CompareByName(conn->userName, info->userName))) {
         /*
          * Both an auditing event and debug noise may be visible
          * to the attacker, so don't spew.
          */
         err = VGAUTH_E_OK;
         goto done;
      }
      if (!g_hash_table_remove(ticketTable, ticket)) {
         // this shouldn't happen, since we just found it
         ASSERT(0);
         err = VGAUTH_E_FAIL;
      } else {
         err = VGAUTH_E_OK;
      }
   } else {
      /*
       * If a bad/old ticket is revoked, we'll just pretend it worked
       * anyways.  This makes it hard to test, but its better from an
       * dev point of view, since otherwise the dev has to ignore
       * INVALID_TICKET, and assume the ticket got reaped or otherwise
       * flushed.
       */
      err = VGAUTH_E_OK;
   }

done:
   return err;
}


/*
 ******************************************************************************
 * ServiceLookupTicketOwner --                                           */ /**
 *
 * Returns the owner of a ticket.
 *
 * @param[in]   ticket        The ticket being validated.
 * @param[out]  userName      The owner of the ticket.  Must be g_free()d.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceLookupTicketOwner(const char *ticket,
                         char **userName)
{
   TicketInfo *info;
   VGAuthError err;

   ServiceReapOldTickets();
   info = g_hash_table_lookup(ticketTable, ticket);

   if (NULL != info) {
      *userName = g_strdup(info->userName);
      err = VGAUTH_E_OK;
   } else {
      err = VGAUTH_E_INVALID_TICKET;
   }

   return err;
}
