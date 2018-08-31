/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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
 * @file net.c
 *
 *	Parses assorted /proc/net nodes.
 */


#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "vmware.h"
#include "panic.h"
#include "slashProc.h"
#include "slashProcNetInt.h"


/*
 * Local data.
 */


#define PROC_NET_SNMP   "/proc/net/snmp"
#define PROC_NET_SNMP6  "/proc/net/snmp6"
#define PROC_NET_ROUTE  "/proc/net/route"
#define PROC_NET_ROUTE6 "/proc/net/ipv6_route"


/**
 * @brief Evaluate an expression with a regular expression match.
 *
 * Convenience wrapper to fetch a regular expression match result, evaluate
 * it as an argument in some expression, and then free it.
 *
 * @param[in] matchInfo  GMatchInfo RE context.
 * @param[in] matchIndex Match index.
 * @param[in] expr       Expression to evaluate.  Must contain @c MATCH as
 *                       placeholder where matched value will be inserted.
 */
#define MATCHEXPR(matchInfo, matchIndex, expr) do {                     \
   gchar *MATCH = g_match_info_fetch(matchInfo, matchIndex);            \
   expr;                                                                \
   g_free(MATCH);                                                       \
} while(0)


/**
 * Override-able @c /proc/net/snmp path.  Useful for debugging.
 */
static const char *pathToNetSnmp = PROC_NET_SNMP;


/**
 * Override-able @c /proc/net/snmp6 path.  Useful for debugging.
 */
static const char *pathToNetSnmp6 = PROC_NET_SNMP6;


/**
 * Override-able @c /proc/net/route path.  Useful for debugging.
 */
static const char *pathToNetRoute = PROC_NET_ROUTE;


/**
 * Override-able @c /proc/net/ipv6_route path.  Useful for debugging.
 */
static const char *pathToNetRoute6 = PROC_NET_ROUTE6;


/*
 * Private function prototypes.
 */

static void Ip6StringToIn6Addr(const char *ip6String,
                               struct in6_addr *in6_addr);
static guint64 MatchToGuint64(const GMatchInfo *matchInfo,
                              const gint matchIndex,
                              gint base);


/*
 * Library private functions.
 */


#ifdef VMX86_DEVEL
/*
 ******************************************************************************
 * SlashProcNetSetPathSnmp --                                           */ /**
 *
 * @brief Overrides @ref pathToNetSnmp.  Useful for debugging.
 *
 * @param[in] newPathToNetSnmp  New path to used in place of @ref PROC_NET_SNMP.
 *                              If @c NULL, @ref pathToNewSnmp reverts to @ref
 *                              PROC_NET_SNMP.
 *
 ******************************************************************************
 */

void
SlashProcNetSetPathSnmp(const char *newPathToNetSnmp)
{
   pathToNetSnmp = newPathToNetSnmp ? newPathToNetSnmp : PROC_NET_SNMP;
}


/*
 ******************************************************************************
 * SlashProcNetSetPathSnmp6 --                                          */ /**
 *
 * @brief Overrides @ref pathToNetSnmp6.  Useful for debugging.
 *
 * @sa SlashProcNetSetPathSnmp
 *
 ******************************************************************************
 */

void
SlashProcNetSetPathSnmp6(const char *newPathToNetSnmp6)
{
   pathToNetSnmp6 = newPathToNetSnmp6 ? newPathToNetSnmp6 : PROC_NET_SNMP6;
}


/*
 ******************************************************************************
 * SlashProcNetSetPathRoute --                                          */ /**
 *
 * @brief Overrides @ref pathToNetRoute.  Useful for debugging.
 *
 * @sa SlashProcNetSetPathSnmp
 *
 ******************************************************************************
 */

void
SlashProcNetSetPathRoute(const char *newPathToNetRoute)
{
   pathToNetRoute = newPathToNetRoute ? newPathToNetRoute : PROC_NET_ROUTE;
}


/*
 ******************************************************************************
 * SlashProcNetSetPathRoute6 --                                         */ /**
 *
 * @brief Overrides @ref pathToNetRoute6.  Useful for debugging.
 *
 * @sa SlashProcNetSetPathSnmp
 *
 ******************************************************************************
 */

void
SlashProcNetSetPathRoute6(const char *newPathToNetRoute6)
{
   pathToNetRoute6 = newPathToNetRoute6 ? newPathToNetRoute6 : PROC_NET_ROUTE6;
}
#endif // ifdef VMX86_DEVEL


/*
 * Library public functions.
 */


/*
 ******************************************************************************
 * SlashProcNet_GetSnmp --                                              */ /**
 *
 * @brief Reads @ref pathToNetSnmp and returns contents as a
 *        <tt>GHashTable(gchar *key, guint64 *value)</tt>.
 *
 * Example usage:
 * @code
 * GHashTable *netSnmp = SlashProcNet_GetSnmp();
 * guint64 *inDiscards = g_hash_table_lookup(netSnmp, "IpInDiscards");
 * @endcode
 *
 * @note        Caller should free the returned @c GHashTable with
 *              @c g_hash_table_destroy.
 * @note        This routine creates persistent GLib GRegexs.
 *
 * @return      On failure, NULL.  On success, a valid @c GHashTable.
 * @todo        Provide a case-insensitive key comparison function.
 * @todo        Consider init/cleanup routines to not "leak" GRegexs.
 *
 ******************************************************************************
 */

GHashTable *
SlashProcNet_GetSnmp(void)
{
   GHashTable *myHashTable = NULL;
   GIOChannel *myChannel = NULL;
   GIOStatus keyIoStatus = G_IO_STATUS_ERROR;
   GIOStatus valIoStatus = G_IO_STATUS_ERROR;
   gchar *myKeyLine = NULL;
   gchar *myValLine = NULL;
   Bool parseError = FALSE;
   int fd = -1;

   static GRegex *myKeyRegex = NULL;
   static GRegex *myValRegex = NULL;

   if (myKeyRegex == NULL) {
      myKeyRegex = g_regex_new("^(\\w+): (\\w+ )*(\\w+)$", G_REGEX_OPTIMIZE,
                               0, NULL);
      myValRegex = g_regex_new("^(\\w+): (-?\\d+ )*(-?\\d+)$", G_REGEX_OPTIMIZE,
                               0, NULL);
      ASSERT(myKeyRegex);
      ASSERT(myValRegex);
   }

   if ((fd = g_open(pathToNetSnmp, O_RDONLY)) == -1) {
      return NULL;
   }

   myChannel = g_io_channel_unix_new(fd);

   myHashTable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

   /*
    * Expected format:
    *
    * pfx0: key0 key1 key2 ... keyN
    * pfx0: val0 val1 val2 ... valN
    * ...
    * pfxN: ...
    */

   while ((keyIoStatus = g_io_channel_read_line(myChannel, &myKeyLine, NULL, NULL,
                                                NULL)) == G_IO_STATUS_NORMAL &&
          (valIoStatus = g_io_channel_read_line(myChannel, &myValLine, NULL, NULL,
                                                NULL)) == G_IO_STATUS_NORMAL) {

      GMatchInfo *keyMatchInfo = NULL;
      GMatchInfo *valMatchInfo = NULL;

      gchar **myKeys = NULL;
      gchar **myVals = NULL;

      gchar **myKey = NULL;
      gchar **myVal = NULL;

      gchar *keyPrefix = NULL;
      gchar *valPrefix = NULL;

      /*
       * Per format above, we expect a pair of lines with a matching prefix.
       */
      {
         if (!g_regex_match(myKeyRegex, myKeyLine, 0, &keyMatchInfo) ||
             !g_regex_match(myValRegex, myValLine, 0, &valMatchInfo)) {
            parseError = TRUE;
            goto badIteration;
         }

         keyPrefix = g_match_info_fetch(keyMatchInfo, 1);
         valPrefix = g_match_info_fetch(valMatchInfo, 1);

         ASSERT(keyPrefix);
         ASSERT(valPrefix);

         if (strcmp(keyPrefix, valPrefix)) {
            parseError = TRUE;
            goto badIteration;
         }
      }

      myKeys = g_strsplit(myKeyLine, " ", 0);
      myVals = g_strsplit(myValLine, " ", 0);

      /*
       * Iterate over the columns, combining the column keys with the prefix
       * to form the new key name.  (I.e., "Ip: InDiscards" => "IpInDiscards".)
       */
      for (myKey = &myKeys[1], myVal = &myVals[1];
           *myKey && *myVal;
           myKey++, myVal++) {
         gchar *hashKey;
         guint64 *myIntVal = NULL;

         hashKey = g_strjoin(NULL, keyPrefix, *myKey, NULL);
         g_strstrip(hashKey);

         /*
          * By virtue of having matched the above regex, this conversion
          * must hold.
          */
         myIntVal = g_new(guint64, 1);
         *myIntVal = g_ascii_strtoull(*myVal, NULL, 10);

         /*
          * If our input contains duplicate keys, which I really don't see
          * happening, the latter value overrides the former.
          *
          * NB: myHashTable claims ownership of hashKey.
          */
         g_hash_table_insert(myHashTable, hashKey, myIntVal);
      }

      /*
       * Make sure the column counts matched.  If we succeeded, both pointers
       * should now be NULL.
       */
      if (*myKey || *myVal) {
         parseError = TRUE;
      }

badIteration:
      g_match_info_free(keyMatchInfo);
      g_match_info_free(valMatchInfo);

      g_free(keyPrefix);
      g_free(valPrefix);

      g_strfreev(myKeys);
      g_strfreev(myVals);

      g_free(myKeyLine);
      g_free(myValLine);
      myKeyLine = NULL;
      myValLine = NULL;

      if (parseError) {
         break;
      }
   }

   /*
    * Error conditions:
    *    Hash table empty:      Unable to parse any input.
    *    myKeyLine != NULL:     Failed to read "key" and "value" lines during
    *                           same loop iteration.
    *    parseError == TRUE:    See loop body above.
    */
   if (keyIoStatus == G_IO_STATUS_ERROR ||
       valIoStatus == G_IO_STATUS_ERROR ||
       g_hash_table_size(myHashTable) == 0 ||
       parseError) {
      g_hash_table_destroy(myHashTable);
      myHashTable = NULL;
   }

   g_free(myKeyLine);
   g_free(myValLine);
   myKeyLine = NULL;
   myValLine = NULL;

   close(fd);
   g_io_channel_unref(myChannel);

   return myHashTable;
}


/*
 ******************************************************************************
 * SlashProcNet_GetSnmp6 --                                             */ /**
 *
 * @brief Reads @ref pathToNetSnmp6 and returns contents as a
 *        <tt>GHashTable(gchar *key, guint64 *value)</tt>.
 *
 * Example usage:
 * @code
 * GHashTable *netSnmp6 = SlashProcNet_GetSnmp6();
 * guint64 *raw6Discards = g_hash_table_lookup(netSnmp6, "Ip6InDiscards");
 * @endcode
 *
 * @note        Caller should free the returned @c GHashTable with
 *              @c g_hash_table_destroy.
 * @note        This routine creates persistent GLib GRegexs.
 *
 * @return      On failure, NULL.  On success, a valid @c GHashTable.
 * @todo        Provide a case-insensitive key comparison function.
 * @todo        Consider init/cleanup routines to not "leak" GRegexs.
 *
 ******************************************************************************
 */

GHashTable *
SlashProcNet_GetSnmp6(void)
{
   GHashTable *myHashTable = NULL;
   GIOChannel *myChannel = NULL;
   GIOStatus ioStatus;
   gchar *myInputLine = NULL;
   Bool parseError = FALSE;
   int fd = -1;

   static GRegex *myRegex = NULL;

   if (myRegex == NULL) {
      myRegex = g_regex_new("^(\\w+)\\s+(-?\\d+)\\s*$", G_REGEX_OPTIMIZE,
                            0, NULL);
      ASSERT(myRegex);
   }

   if ((fd = g_open(pathToNetSnmp6, O_RDONLY)) == -1) {
      return NULL;
   }

   myChannel = g_io_channel_unix_new(fd);

   myHashTable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

   /*
    * Expected format:
    *
    * key1                              value1
    * key2                              value2
    * ...
    * keyN                              valueN
    */

   while ((ioStatus = g_io_channel_read_line(myChannel, &myInputLine, NULL,
                                             NULL, NULL)) == G_IO_STATUS_NORMAL) {
      GMatchInfo *matchInfo = NULL;

      if (g_regex_match(myRegex, myInputLine, 0, &matchInfo)) {
         gchar *myKey = NULL;
         gchar *myVal = NULL;
         guint64 *myIntVal = NULL;

         myKey = g_match_info_fetch(matchInfo, 1);
         myVal = g_match_info_fetch(matchInfo, 2);

         /*
          * By virtue of having matched the above regex, this conversion
          * must hold.
          */
         myIntVal = g_new(guint64, 1);
         *myIntVal = g_ascii_strtoull(myVal, NULL, 10);

         /*
          * The hash table will take ownership of myKey and myIntVal.  We're
          * still responsible for myVal.
          */
         g_hash_table_insert(myHashTable, myKey, myIntVal);
         g_free(myVal);
      } else {
         parseError = TRUE;
      }

      g_match_info_free(matchInfo);
      g_free(myInputLine);
      myInputLine = NULL;

      if (parseError) {
         break;
      }
   }

   if (ioStatus == G_IO_STATUS_ERROR ||
       g_hash_table_size(myHashTable) == 0 ||
       parseError) {
      g_hash_table_destroy(myHashTable);
      myHashTable = NULL;
   }

   close(fd);
   g_io_channel_unref(myChannel);

   return myHashTable;
}


/*
 ******************************************************************************
 * SlashProcNet_GetRoute --                                             */ /**
 *
 * @brief Reads the first @c maxRoutes lines of @ref pathToNetRoute and
 *        returns a @c GPtrArray of <tt>struct rtentry</tt>s.
 *
 * Example usage:
 * @code
 * GPtrArray *rtArray;
 * guint i;
 * rtArray = SlashProcNet_GetRoute(NICINFO_MAX_ROUTES, RTF_UP);
 * for (i = 0; i < rtArray->len; i++) {
 *    struct rtentry *myRoute = g_ptr_array_index(rtArray, i);
 *    // Do something with myRoute->rt_dst.
 * }
 * SlashProcNet_FreeRoute(rtArray);
 * @endcode
 *
 * @note        Caller is responsible for freeing the @c GPtrArray with
 *              SlashProcNet_FreeRoute.
 * @note        This routine creates persistent GLib GRegexs.
 *
 * @param[in]   maxRoutes       Max routes to gather.
 * @param[in]   rtFilterFlags   Route flags used to filter out what we want.
 *                              Set ~0 if want everything.
 *
 * @return      On failure, NULL.  On success, a valid @c GPtrArray.
 * @todo        Consider init/cleanup routines to not "leak" GRegexs.
 * @todo        Consider rewriting, integrating with libdnet.
 *
 ******************************************************************************
 */

GPtrArray *
SlashProcNet_GetRoute(unsigned int maxRoutes,
                      unsigned short rtFilterFlags)
{
   GIOChannel *myChannel = NULL;
   GIOStatus myIoStatus;
   GPtrArray *myArray = NULL;
   gchar *myLine = NULL;
   int fd = -1;
   unsigned int lineCount = 0;

   static GRegex *myFieldsRE = NULL;
   static GRegex *myValuesRE = NULL;

   ASSERT(maxRoutes > 0);

   if (myFieldsRE == NULL) {
      myFieldsRE = g_regex_new("^Iface\\s+Destination\\s+Gateway\\s+Flags\\s+"
                               "RefCnt\\s+Use\\s+Metric\\s+Mask\\s+MTU\\s+"
                               "Window\\s+IRTT\\s*$", 0, 0, NULL);
      myValuesRE = g_regex_new("^(\\S+)\\s+([[:xdigit:]]{8})\\s+"
                               "([[:xdigit:]]{8})\\s+([[:xdigit:]]{4})\\s+"
                               "\\d+\\s+\\d+\\s+(\\d+)\\s+"
                               "([[:xdigit:]]{8})\\s+(\\d+)\\s+\\d+\\s+(\\d+)\\s*$",
                               0, 0, NULL);
      ASSERT(myFieldsRE);
      ASSERT(myValuesRE);
   }

   /*
    * 1.  Open pathToNetRoute, associate it with a GIOChannel.
    */

   if ((fd = g_open(pathToNetRoute, O_RDONLY)) == -1) {
      Warning("%s: open(%s): %s\n", __func__, pathToNetRoute,
              g_strerror(errno));
      return NULL;
   }

   myChannel = g_io_channel_unix_new(fd);

   /*
    * 2.  Sanity check the header, making sure it matches what we expect.
    *     (It's -extremely- unlikely this will change, but we should check
    *     anyway.)
    */

   myIoStatus = g_io_channel_read_line(myChannel, &myLine, NULL, NULL, NULL);
   if (myIoStatus != G_IO_STATUS_NORMAL ||
       g_regex_match(myFieldsRE, myLine, 0, NULL) == FALSE) {
      goto out;
   }

   g_free(myLine);
   myLine = NULL;

   myArray = g_ptr_array_new();

   /*
    * 3.  For each line...
    */

   while (lineCount < maxRoutes &&
             (myIoStatus = g_io_channel_read_line(myChannel,
                  &myLine, NULL, NULL, NULL)) == G_IO_STATUS_NORMAL) {
      GMatchInfo *myMatchInfo = NULL;
      struct rtentry *myEntry = NULL;
      struct sockaddr_in *sin = NULL;
      Bool parseError = FALSE;

      /*
       * 3a. Validate with regex.
       */
      if (!g_regex_match(myValuesRE, myLine, 0, &myMatchInfo)) {
         parseError = TRUE;
         goto iterationCleanup;
      }

      /*
       * 3b. Allocate new rtentry, add to array.  This simplifies the cleanup
       *     code path.
       */
      myEntry = g_new0(struct rtentry, 1);

      /*
       * 3c. Copy contents to new struct rtentry.
       */
      myEntry->rt_dev = g_match_info_fetch(myMatchInfo, 1);

      sin = (struct sockaddr_in *)&myEntry->rt_dst;
      sin->sin_family = AF_INET;
      sin->sin_addr.s_addr = MatchToGuint64(myMatchInfo, 2, 16);

      sin = (struct sockaddr_in *)&myEntry->rt_gateway;
      sin->sin_family = AF_INET;
      sin->sin_addr.s_addr = MatchToGuint64(myMatchInfo, 3, 16);

      sin = (struct sockaddr_in *)&myEntry->rt_genmask;
      sin->sin_family = AF_INET;
      sin->sin_addr.s_addr = MatchToGuint64(myMatchInfo, 6, 16);

      myEntry->rt_flags = MatchToGuint64(myMatchInfo, 4, 16);
      myEntry->rt_metric = MatchToGuint64(myMatchInfo, 5, 10);
      myEntry->rt_mtu = MatchToGuint64(myMatchInfo, 7, 10);
      myEntry->rt_irtt = MatchToGuint64(myMatchInfo, 8, 10);

      if (rtFilterFlags == (unsigned short)~0 ||
          (myEntry->rt_flags & rtFilterFlags) != 0) {
         g_ptr_array_add(myArray, myEntry);
         lineCount++;
      } else {
         g_free(myEntry->rt_dev);
         g_free(myEntry);
      }

iterationCleanup:
      g_free(myLine);
      myLine = NULL;

      g_match_info_free(myMatchInfo);
      myMatchInfo = NULL;

      if (parseError) {
         /* Return NULL to signal parsing error */
         if (myArray) {
            SlashProcNet_FreeRoute(myArray);
            myArray = NULL;
         }
         break;
      }
   }

out:
   g_free(myLine);
   close(fd);
   g_io_channel_unref(myChannel);

   return myArray;
}


/*
 ******************************************************************************
 * SlashProcNet_FreeRoute --                                            */ /**
 *
 * @brief Frees memory associated with a GPtrArray allocated by
 *        SlashProcNet_GetRoute.
 *
 * @param[in]   routeArray      Array to free.
 *
 ******************************************************************************
 */

void
SlashProcNet_FreeRoute(GPtrArray *routeArray)
{
   int i;

   if (routeArray == NULL) {
      return;
   }

   for (i = 0; i < routeArray->len; i++) {
      struct rtentry *myEntry = g_ptr_array_index(routeArray, i);
      ASSERT(myEntry->rt_dev);
      g_free(myEntry->rt_dev);
      g_free(myEntry);
   }

   g_ptr_array_free(routeArray, TRUE);
}


/*
 ******************************************************************************
 * SlashProcNet_GetRoute6 --                                            */ /**
 *
 * @brief Reads the first @c maxRoutes lines of @ref pathToNetRoute6 and
 *        returns a @c GPtrArray of <tt>struct in6_rtmsg</tt>s.
 *
 * Example usage:
 * @code
 * GPtrArray *rtArray;
 * guint i;
 * rtArray = SlashProcNet_GetRoute6(NICINFO_MAX_ROUTES, RTF_UP);
 * for (i = 0; i < rtArray->len; i++) {
 *    struct in6_rtmsg *myRoute = g_ptr_array_index(rtArray, i);
 *    // Do something with myRoute->rtmsg_dst.
 * }
 * SlashProcNet_FreeRoute6(rtArray);
 * @endcode
 *
 * @note        Caller is responsible for freeing the @c GPtrArray with
 *              SlashProcNet_FreeRoute6.
 * @note        This routine creates persistent GLib GRegexs.
 *
 * @param[in]   maxRoutes       Max routes to gather.
 * @param[in]   rtFilterFlags   Route flags used to filter out what we want.
 *                              Set ~0 if want everything.
 *
 * @return      On failure, NULL.  On success, a valid @c GPtrArray.
 * @todo        Consider init/cleanup routines to not "leak" GRegexs.
 * @todo        Consider rewriting, integrating with libdnet.
 *
 ******************************************************************************
 */

GPtrArray *
SlashProcNet_GetRoute6(unsigned int maxRoutes,
                       unsigned int rtFilterFlags)
{
   GIOChannel *myChannel = NULL;
   GIOStatus myIoStatus;
   GPtrArray *myArray = NULL;
   gchar *myLine = NULL;
   Bool parseError = FALSE;
   int fd = -1;
   unsigned int lineCount = 0;

   static GRegex *myValuesRE = NULL;

   ASSERT(maxRoutes > 0);

   if (myValuesRE == NULL) {
      myValuesRE = g_regex_new("^([[:xdigit:]]{32}) ([[:xdigit:]]{2}) "
                                "([[:xdigit:]]{32}) ([[:xdigit:]]{2}) "
                                "([[:xdigit:]]{32}) ([[:xdigit:]]{8}) "
                                "[[:xdigit:]]{8} [[:xdigit:]]{8} "
                                "([[:xdigit:]]{8})\\s+(\\S+)\\s*$", 0, 0,
                                NULL);
      ASSERT(myValuesRE);
   }

   /*
    * 1.  Open pathToNetRoute6, associate it with a GIOChannel.
    */

   if ((fd = g_open(pathToNetRoute6, O_RDONLY)) == -1) {
      Warning("%s: open(%s): %s\n", __func__, pathToNetRoute6,
              g_strerror(errno));
      return NULL;
   }

   myChannel = g_io_channel_unix_new(fd);

   myArray = g_ptr_array_new();

   while (lineCount < maxRoutes &&
               (myIoStatus = g_io_channel_read_line(myChannel,
                     &myLine, NULL, NULL, NULL)) == G_IO_STATUS_NORMAL) {
      struct in6_rtmsg *myEntry = NULL;
      GMatchInfo *myMatchInfo = NULL;

      if (!g_regex_match(myValuesRE, myLine, 0, &myMatchInfo)) {
         parseError = TRUE;
         goto iterationCleanup;
      }

      myEntry = g_new0(struct in6_rtmsg, 1);

      MATCHEXPR(myMatchInfo, 1, Ip6StringToIn6Addr(MATCH, &myEntry->rtmsg_dst));
      MATCHEXPR(myMatchInfo, 3, Ip6StringToIn6Addr(MATCH, &myEntry->rtmsg_src));
      MATCHEXPR(myMatchInfo, 5, Ip6StringToIn6Addr(MATCH, &myEntry->rtmsg_gateway));

      myEntry->rtmsg_dst_len = MatchToGuint64(myMatchInfo, 2, 16);
      myEntry->rtmsg_src_len = MatchToGuint64(myMatchInfo, 4, 16);
      myEntry->rtmsg_metric = MatchToGuint64(myMatchInfo, 6, 16);
      myEntry->rtmsg_flags = MatchToGuint64(myMatchInfo, 7, 16);

      MATCHEXPR(myMatchInfo, 8, myEntry->rtmsg_ifindex = if_nametoindex(MATCH));

      if (rtFilterFlags == (uint)~0 ||
          (myEntry->rtmsg_flags & rtFilterFlags) != 0) {
         g_ptr_array_add(myArray, myEntry);
         lineCount++;
      } else {
         g_free(myEntry);
      }

iterationCleanup:
      g_free(myLine);
      myLine = NULL;

      g_match_info_free(myMatchInfo);
      myMatchInfo = NULL;

      if (parseError) {
         /* Return NULL to signal parsing error */
         if (myArray) {
            SlashProcNet_FreeRoute6(myArray);
            myArray = NULL;
         }
         break;
      }
   }

   g_free(myLine);
   myLine = NULL;

   close(fd);
   g_io_channel_unref(myChannel);

   return myArray;
}


/*
 ******************************************************************************
 * SlashProcNet_FreeRoute6 --                                           */ /**
 *
 * @brief Frees memory associated with a GPtrArray allocated by
 *        SlashProcNet_GetRoute6.
 *
 * @param[in]   routeArray      Array to free.
 *
 ******************************************************************************
 */

void
SlashProcNet_FreeRoute6(GPtrArray *routeArray)
{
   int i;

   if (routeArray == NULL) {
      return;
   }

   for (i = 0; i < routeArray->len; i++) {
      struct rtentry *myEntry = g_ptr_array_index(routeArray, i);
      g_free(myEntry);
   }

   g_ptr_array_free(routeArray, TRUE);
}


/*
 * Private functions.
 */


/*
 ******************************************************************************
 * Ip6StringToIn6Addr --                                                */ /**
 *
 * @brief Parses a @c /proc/net/ipv6_route hexadecimal IPv6 address and
 *        records it in a <tt>struct in6_addr</tt>.
 *
 * @param[in]   ip6String       Source string.
 * @param[out]  in6_addr        Output struct.
 *
 ******************************************************************************
 */

static void
Ip6StringToIn6Addr(const char *ip6String,
                   struct in6_addr *in6_addr)
{
   unsigned int i;

   ASSERT(strlen(ip6String) == 32);

   for (i = 0; i < 16; i++) {
      int nmatched;
      nmatched = sscanf(&ip6String[2 * i], "%2hhx", &in6_addr->s6_addr[i]);
      ASSERT(nmatched == 1);
   }
}


/*
 ******************************************************************************
 * MatchToGuint64 --                                                    */ /**
 *
 * @brief Wrapper around @c g_ascii_strtoull and @c g_match_info_fetch.
 *
 * @param[in]   matchInfo       Source regular expression match context.
 * @param[in]   matchIndex      Match number to fetch.
 * @param[in]   base            Base represented by matched string.
 *                              (See @c g_ascii_strtoull docs.)
 *
 ******************************************************************************
 */

static guint64
MatchToGuint64(const GMatchInfo *matchInfo,
               const gint matchIndex,
               gint base)
{
   guint64 retval;
   MATCHEXPR(matchInfo, matchIndex, retval = g_ascii_strtoull(MATCH, NULL, base));
   return retval;
}
