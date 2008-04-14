/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#ifdef WIN32
#include <Winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "vmware.h"
#include "util.h"
#include "str.h"
#include "SLPv2.h"
#include "SLPv2Private.h"


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserMatchStringInList -
 *
 *      Returns TRUE if attribute is contained in list, where list
 *      is a comma separated list of attributes.
 *
 * Results:
 *      Returns TRUE upon match.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParserMatchStringInList(char *list,      // IN
                                char *attribute) // IN
{
   char *start;
   char *end;
   size_t length;

   /*
    * If there is no attribute to match against, it's a match.
    */
   if (NULL == list || NULL == attribute || 0 == strlen(attribute)) {
      return TRUE;
   }

   /*
    * Look at every attribute in the list that is followed by a comma.
    */
   start = list;
   end   = Str_Strchr(start, ',');
   length = (NULL != end) ? (end - start) : strlen(start);

   while (end != NULL) {
      if (0 == Str_Strncasecmp(start, attribute, length)) {
         return TRUE;
      }

      start = end + 1;
      end   = Str_Strchr(start, ',');
      length = (NULL != end) ? (end - start) : strlen(start);
   }

   /*
    * At this point we're at the last item in list.
    */
   if (0 == Str_Strncasecmp(start, attribute, length)) {
     return TRUE;
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParser_ServiceRequestMatch -
 *
 *      Returns TRUE if the packet matches the SLPv2 request parameters
 *      passed to this function.
 *
 * Results:
 *      Returns TRUE upon match.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParser_ServiceRequestMatch(struct SLPv2_Parse *parse, // IN
                                   char *myIpsList,           // UN
                                   char *myServiceType,       // IN
                                   char *myScope,             // IN
                                   char *myPredicate,         // IN
                                   uint16 *xid)               // OUT
{
   ASSERT(NULL != parse);

   if (SLPV2_SERVICEREQUEST != parse->header->functionId) {
      return FALSE;
   }

   /*
    * TODO: ip address matching.
    */

   if (! SLPv2MsgParserMatchStringInList(parse->serviceRequest.serviceType,
                                                              myServiceType)) {
      return FALSE;
   }

   if (! SLPv2MsgParserMatchStringInList(parse->serviceRequest.scope,
                                                                    myScope)) {
      return FALSE;
   }

   // TODO: implement LDAPv3 predicate match

   if (NULL != xid) {
      *xid = Portable_ntohs(parse->header->xid);
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParser_ServiceReplyMatch -
 *
 *      Returns TRUE if the packet matches the SLPv2 parameters
 *      passed to this function.
 *
 * Results:
 *      Returns TRUE upon match.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParser_ServiceReplyMatch(struct SLPv2_Parse *parse, // IN
                                 int *urlCount,             // OUT
                                 char ***urlArray,          // OUT
                                 uint16 *xid)               // OUT
{
   ASSERT(NULL != parse);

   if (SLPV2_SERVICEREPLY != parse->header->functionId) {
      return FALSE;
   }

   if (NULL != urlCount) {
      *urlCount = parse->serviceReply.urlCount;
   }

   if (NULL != urlArray) {
      int i;
      *urlArray = Util_SafeMalloc(sizeof(char *)*parse->serviceReply.urlCount);

      for (i = 0; i < parse->serviceReply.urlCount; i++) {
         *urlArray[i] = Util_SafeStrdup(parse->serviceReply.url[i]);
      }
   }

   if (NULL != xid) {
      *xid = Portable_ntohs(parse->header->xid);
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParser_AttributeRequestMatch -
 *
 *      Returns TRUE if the packet matches the SLPv2 request parameters
 *      passed to this function.
 *
 * Results:
 *      Returns TRUE upon match.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParser_AttributeRequestMatch(struct SLPv2_Parse *parse, // IN
                                     char *myIpsList,           // UN
                                     char *url,                 // IN
                                     char *myScope,             // IN
                                     char *tagList,             // IN
                                     uint16 *xid)               // OUT
{
   ASSERT(NULL != parse);

   if (SLPV2_ATTRIBUTEREQUEST != parse->header->functionId) {
      return FALSE;
   }

   /*
    * TODO: ip address matching.
    */

   if ((url != NULL) && (0 != strcmp(url, parse->attributeRequest.url))) {
      return FALSE;
   }

   if (! SLPv2MsgParserMatchStringInList(parse->serviceRequest.scope,
                                                                    myScope)) {
      return FALSE;
   }

   /*
    * TODO: tag list and LDAPv3 predicate matching.
    */

   if (NULL != xid) {
      *xid = Portable_ntohs(parse->header->xid);
   }

   return TRUE;
}



/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParser_AttributeReplyMatch -
 *
 *      Returns TRUE if the packet matches the SLPv2 parameters
 *      passed to this function.
 *
 * Results:
 *      Returns TRUE upon match.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParser_AttributeReplyMatch(struct SLPv2_Parse *parse, // IN
                                   char **attributeList,      // OUT
                                   uint16 *xid)               // OUT
{
   ASSERT(NULL != parse);

   if (SLPV2_ATTRIBUTEREPLY != parse->header->functionId) {
      return FALSE;
   }

   if (attributeList != NULL) {
      *attributeList = Util_SafeStrdup(parse->attributeReply.attributeList);
   }

   if (NULL != xid) {
      *xid = Portable_ntohs(parse->header->xid);
   }

   return TRUE;
}


