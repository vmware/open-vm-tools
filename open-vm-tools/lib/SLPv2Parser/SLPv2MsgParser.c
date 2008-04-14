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
#include "vmware.h"

#ifdef WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <stdlib.h>

#include "str.h"
#include "util.h"
#include "SLPv2.h"
#include "SLPv2Private.h"

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserStringValid -
 *
 *      Returns TRUE if the string at 'offset' bytes into 'packet'
 *      actually fits inside the packet, which is 'len' bytes.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon memory error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParserStringValid(char *packet,  // IN
                          int len,       // IN
                          int offset)    // IN
{
   uint16 stringLength = Portable_ntohs(*((uint16 *) (packet + len)));
   return (offset + stringLength > len) ? FALSE : TRUE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserGetString -
 *
 *      Returns a C string, given the (16 bit length) Pascal string at
 *      packet+offset.
 *
 * Results:
 *      Returns a C string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
SLPv2MsgParserGetString(char *packet,     // IN
                        int packetLength, // IN
                        int offset,       // IN
                        Bool *ok)         // OUT
{
   uint16 stringLength = Portable_ntohs(*((uint16 *) (packet + offset)));
   char *string = NULL;
   Bool myOk = TRUE;

   /* make sure the string actually fits in the packet */
   if (offset + stringLength > packetLength) {
      if (NULL != ok) {
         myOk = FALSE;
      }
      goto bye;
   }

   string = Util_SafeMalloc(stringLength + 1);

   /*
    * We use memcpy() because Str_Strcpy()  doesn't handle non-NULL
    * terminated strings.
    */
   memcpy(string, packet + offset + 2, stringLength);
   string[stringLength] = '\0';

bye:
   if (NULL != ok) {
      if (*ok == FALSE && myOk == TRUE) {
         myOk = FALSE;
      }
      *ok = myOk;
   }
   return string;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParser_Init -
 *
 *      Initializes a SLPv2_Parse structure.
 *
 * Results:
 *      Returns a pointer to a SLPv2_Parse structure or NULL upon error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

struct SLPv2_Parse *
SLPv2MsgParser_Init()
{
   struct SLPv2_Parse *parse;

   parse = (struct SLPv2_Parse *) Util_SafeMalloc(sizeof (struct SLPv2_Parse));
   parse->header = NULL;
   parse->languageTag = NULL;

   parse->serviceRequest.prList = NULL;
   parse->serviceRequest.serviceType = NULL;
   parse->serviceRequest.scope = NULL;
   parse->serviceRequest.predicate = NULL;
   parse->serviceRequest.spi = NULL;

   parse->serviceReply.error = 0;
   parse->serviceReply.urlCount = 0;
   parse->serviceReply.url = NULL;

   parse->attributeRequest.prList   = NULL;
   parse->attributeRequest.url      = NULL;
   parse->attributeRequest.scope    = NULL;
   parse->attributeRequest.tagList  = NULL;
   parse->attributeRequest.spi      = NULL;

   parse->attributeReply.error         = 0;
   parse->attributeReply.attributeList = NULL;

   return parse;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserGetHeader -
 *
 *      Populates the SLPv2_Parse structure with SLPv2 header data.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon memory error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParserGetHeader(char *packet,               // IN
                        int len,                    // IN
                        struct SLPv2_Parse *parse)  // IN
{
   uint16 languageTagOffset = sizeof(struct SLPv2_Header);
   Bool parseOk = TRUE;
   uint32 lengthTemp;
   uint8 *lengthArrayTemp = (uint8 *) &lengthTemp;
   
   parse->header = (struct SLPv2_Header *) packet;

   if (len < sizeof(struct SLPv2_Header)) {
      return FALSE;
   }

   if (SLPV2_VERSION != parse->header->version) {
      return FALSE;
   }

   parse->languageTagLength = Portable_ntohs(*((uint16 *) (packet + languageTagOffset)));

   parse->languageTag = SLPv2MsgParserGetString(packet,
                                                len,
                                                languageTagOffset,
                                                &parseOk);
   if (! parseOk) {
      return FALSE;
   }

   
   lengthArrayTemp[0] = parse->header->length[0];
   lengthArrayTemp[1] = parse->header->length[1];
   lengthArrayTemp[2] = parse->header->length[2];
   lengthTemp = Portable_ntohl(lengthTemp);
   parse->header->length[0] = lengthArrayTemp[0];
   parse->header->length[1] = lengthArrayTemp[1];
   parse->header->length[2] = lengthArrayTemp[2];
   
   parse->header->xid = Portable_ntohs(parse->header->xid);

   return TRUE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserParseServiceRequest -
 *
 *      Populates the SLPv2_Parse structure with SLPv2 Service Request data.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon memory error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParserParseServiceRequest(char *packet,               // IN
                                int len,                    // IN
                                struct SLPv2_Parse *parse)  // IN
{
   Bool parseOkay = TRUE;

   /* previous responder list */
   uint16 prOffset = sizeof(struct SLPv2_Header) + parse->languageTagLength + 2;
   uint16 prLength = Portable_ntohs(*((uint16 *) (packet + prOffset)));

   /* service type */
   uint16 stOffset = prOffset + prLength + 2;
   uint16 stLength = Portable_ntohs(*((uint16 *) (packet + stOffset)));

   /* scope list */
   uint16 slOffset = stOffset + stLength + 2;
   uint16 slLength = Portable_ntohs(*((uint16 *) (packet + slOffset)));

   /* predicate */
   uint16 predicateOffset = slOffset + slLength + 2;
   uint16 predicateLength = Portable_ntohs(*((uint16 *) (packet + predicateOffset)));

   /* security parameter index */
   uint16 spiOffset = predicateOffset + predicateLength + 2;

   parse->serviceRequest.prList = SLPv2MsgParserGetString(packet, len,
                                                          prOffset, &parseOkay);
   parse->serviceRequest.serviceType = SLPv2MsgParserGetString(packet, len,
                                                          stOffset, &parseOkay);
   parse->serviceRequest.scope = SLPv2MsgParserGetString(packet, len,
                                                          slOffset, &parseOkay);
   parse->serviceRequest.predicate = SLPv2MsgParserGetString(packet, len,
                                                   predicateOffset, &parseOkay);
   parse->serviceRequest.spi = SLPv2MsgParserGetString(packet, len,
                                                         spiOffset, &parseOkay);
   if (! parseOkay) {
      return FALSE;
   }

   return TRUE;
}

                                  
/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserParseServiceReply -
 *
 *      Populates the SLPv2_Parse structure with SLPv2 Service Reply data.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon memory error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParserParseServiceReply(char *packet,               // IN
                              int len,                    // IN
                              struct SLPv2_Parse *parse)  // IN
{
   uint16 errorOffset;
   uint16 urlCountOffset;
   uint16 urlOffset;
   Bool   parseOk = TRUE;
   int    i;

   /* error code */
   errorOffset = sizeof(struct SLPv2_Header) + parse->languageTagLength + 2;
   parse->serviceReply.error = Portable_ntohs(*((uint16 *) (packet + errorOffset)));

   urlCountOffset = errorOffset + 2;
   parse->serviceReply.urlCount = Portable_ntohs(*((uint16 *) (packet
                                                           + urlCountOffset)));
   parse->serviceReply.url = Util_SafeMalloc(sizeof(char *)
                                         * parse->serviceReply.urlCount);
   urlOffset = urlCountOffset + 2;

   /*
    * Zero out the URL array so if we fail in the middle of
    * populating it, * SLPv2MsgParser_Destroy will free a bunch
    * of NULLs instead of dangling pointers.
    */
   for (i = 0; i < parse->serviceReply.urlCount; i++) {
      parse->serviceReply.url[i] = NULL;
   }

   for (i = 0; i < parse->serviceReply.urlCount; i++) {
      uint16 urlLength = Portable_ntohs(*((uint16 *) (packet + urlOffset)));

      parse->serviceReply.url[i] = SLPv2MsgParserGetString(packet, len,
                                                         urlOffset, &parseOk);
      if (! parseOk) {
         return FALSE;
      }
      urlOffset = urlOffset + urlLength + 2;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserParseAttributeRequest -
 *
 *      Populates the SLPv2_Parse structure with SLPv2 Attribute Request data.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon memory error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParserParseAttributeRequest(char *packet,               // IN
                                  int len,                    // IN
                                  struct SLPv2_Parse *parse)  // IN
{
   Bool parseOkay = TRUE;

   /* previous responder list */
   uint16 prOffset = sizeof(struct SLPv2_Header) + parse->languageTagLength + 2;
   uint16 prLength = Portable_ntohs(*((uint16 *) (packet + prOffset)));

   /* url */
   uint16 urlOffset = prOffset + prLength + 2;
   uint16 urlLength = Portable_ntohs(*((uint16 *) (packet + urlOffset)));

   /* scope list */
   uint16 slOffset = urlOffset + urlLength + 2;
   uint16 slLength = Portable_ntohs(*((uint16 *) (packet + slOffset)));

   /* tag list */
   uint16 tagOffset = slOffset + slLength + 2;
   uint16 tagLength = Portable_ntohs(*((uint16 *) (packet + tagOffset)));

   /* security parameter index */
   uint16 spiOffset = tagOffset + tagLength + 2;


   parse->attributeRequest.prList  = SLPv2MsgParserGetString(packet, len,
                                                          prOffset, &parseOkay);
   parse->attributeRequest.url     = SLPv2MsgParserGetString(packet, len,
                                                         urlOffset, &parseOkay);
   parse->attributeRequest.scope   = SLPv2MsgParserGetString(packet, len,
                                                          slOffset, &parseOkay);
   parse->attributeRequest.tagList = SLPv2MsgParserGetString(packet, len,
                                                         tagOffset, &parseOkay);
   parse->attributeRequest.spi     = SLPv2MsgParserGetString(packet, len,
                                                         spiOffset, &parseOkay);

   if (! parseOkay) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParserParseAttributeReply -
 *
 *      Populates the SLPv2_Parse structure with SLPv2 Attribute Reply data.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon memory error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParserParseAttributeReply(char *packet,               // IN
                                int len,                    // IN
                                struct SLPv2_Parse *parse)  // IN
{
   uint16 errorOffset;
   uint16 attributeOffset;
   Bool parseOkay = TRUE;
   /* error code */
   errorOffset = sizeof(struct SLPv2_Header) + parse->languageTagLength + 2;
   parse->attributeReply.error = Portable_ntohs(*((uint16 *) (packet + errorOffset)));

   /* attribute list */
   attributeOffset = errorOffset + 2;
   parse->attributeReply.attributeList = SLPv2MsgParserGetString(packet, len,
                                                   attributeOffset, &parseOkay);

   if (! parseOkay) {
      return FALSE;
   }

   return TRUE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParser_Parse -
 *
 *      Returns TRUE if the packet parses as a SLPv2 message.
 *
 * Results:
 *      Returns TRUE upon success, FALSE upon memory error.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SLPv2MsgParser_Parse(struct SLPv2_Parse *parse,     // IN
                     char *packet,                  // IN
                     int len)                       // IN
{
   Bool parseOk = TRUE;
   ASSERT(NULL != parse);

   parseOk = SLPv2MsgParserGetHeader(packet, len, parse);

   if (parseOk) {
      switch (parse->header->functionId) {
      case SLPV2_SERVICEREQUEST:
         parseOk = SLPv2MsgParserParseServiceRequest(packet, len, parse);
         break;
      case SLPV2_SERVICEREPLY:
         parseOk = SLPv2MsgParserParseServiceReply(packet, len, parse);
         break;
      case SLPV2_ATTRIBUTEREQUEST:
         parseOk = SLPv2MsgParserParseAttributeRequest(packet, len, parse);
         break;
      case SLPV2_ATTRIBUTEREPLY:
         parseOk = SLPv2MsgParserParseAttributeReply(packet, len, parse);
         break;
      default:
         parseOk = FALSE;
      } // switch
   }  // if parseOk

   return parseOk;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgParser_Destroy -
 *
 *      Returns TRUE if the
 *
 * Results:
 *      Disposes of a SLPv2_Parse structure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
SLPv2MsgParser_Destroy(struct SLPv2_Parse *parse) // IN
{
   int i;

   ASSERT(NULL != parse);

   /*
    * header.  We don't free(parse->header) because that's up to the caller
    * to manage, since the caller allocated the buffer that contains the
    * packet.
    */ 

   parse->header = NULL;
   free(parse->languageTag);
   parse->languageTag = NULL;

   /*
    * service request strings.
    */
   free(parse->serviceRequest.prList);
   free(parse->serviceRequest.serviceType);
   free(parse->serviceRequest.scope);
   free(parse->serviceRequest.predicate);
   free(parse->serviceRequest.spi);
   parse->serviceRequest.prList = NULL;
   parse->serviceRequest.serviceType = NULL;
   parse->serviceRequest.scope = NULL;
   parse->serviceRequest.predicate = NULL;
   parse->serviceRequest.spi = NULL;
   
   /*
    * service response.
    */
   for (i=0; i < parse->serviceReply.urlCount; i++) {
      free(parse->serviceReply.url[i]);
      parse->serviceReply.url[i] = NULL;
   }
   free(parse->serviceReply.url);

   /*
    * attribute request.
    */
   free(parse->attributeRequest.prList);
   free(parse->attributeRequest.url);
   free(parse->attributeRequest.scope);
   free(parse->attributeRequest.tagList);
   free(parse->attributeRequest.spi);
   parse->attributeRequest.prList = NULL;
   parse->attributeRequest.url = NULL;
   parse->attributeRequest.scope = NULL;
   parse->attributeRequest.tagList = NULL;
   parse->attributeRequest.spi = NULL;

   /*
    * attribute reply.
    */
   free(parse->attributeReply.attributeList);
   parse->attributeReply.attributeList = NULL;

   free(parse);
} // SLPv2MsgParser_Destroy

