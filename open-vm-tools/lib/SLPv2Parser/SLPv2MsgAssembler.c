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
#include "dynbuf.h"
#include "SLPv2.h"
#include "SLPv2Private.h"
#ifdef WIN32
#include <Winsock.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgAssemblerHeader --
 *
 *      Appends an SLPv2 header to a DynBuf.
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
SLPv2MsgAssemblerHeader(DynBuf *b,                 // IN
                        uint8 functionId,          // IN
                        uint32 length,             // IN
                        Bool overflowFlag,         // IN
                        Bool freshFlag,            // IN
                        Bool requestMulticastFlag, // IN
                        uint16 xid)                // IN
{
   struct SLPv2_Header header;
   uint8 *lengthArray = (uint8 *) &length;
   
   length = Portable_htonl(length);
   
   header.version = SLPV2_VERSION;
   header.functionId = functionId;
   header.length[0] = lengthArray[0];
   header.length[1] = lengthArray[1];
   header.length[2] = lengthArray[2];   
   header.flags = Portable_htons((overflowFlag << 15)
                             | (freshFlag << 14)
                             | (requestMulticastFlag << 13)); // flags
   header.extOffset[0] = 0;  // next extension offset
   header.extOffset[1] = 0;
   header.extOffset[2] = 0;
   header.xid = Portable_htons(xid);
   
   return DynBuf_Append(b, &header, sizeof header);
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgAssemblerUrl --
 *
 *      Appends an SLPv2 URL Entry to a DynBuf.
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
SLPv2MsgAssemblerUrl(DynBuf *b,       // IN
                     uint16 lifetime, // IN
                     char *url)       // IN
{
   static uint8 numberUrlAuths = 0;
   struct SLPv2_URL urlEntry;

   ASSERT(url);
   
   urlEntry.reserved = 0;
   urlEntry.lifetime = Portable_htons(lifetime);
   urlEntry.length   = Portable_htons(strlen(url));

   if (! DynBuf_Append(b, &urlEntry, sizeof urlEntry)) {
      goto abort;
   }
   if (! DynBuf_Append(b, &url, strlen(url))) {
      goto abort;
   }
   if (! DynBuf_Append(b, &numberUrlAuths, sizeof numberUrlAuths)) {
      goto abort;
   }
   return TRUE;

abort:
   return FALSE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgAssembler_ServiceRequest --
 *
 *      Appends an SLPv2 Service Request to a DynBuf.
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
SLPv2MsgAssembler_ServiceRequest(char **packet,             // OUT
                                 int *packetSize,           // OUT
                                 uint16 xid,                // IN
                                 Bool overflowFlag,         // IN
                                 Bool freshFlag,            // IN
                                 Bool requestMulticastFlag, // IN
                                 char *languageTag,         // IN
                                 char *prList,              // IN
                                 char *serviceType,         // IN
                                 char *scopeList,           // IN
                                 char *predicate,           // IN
                                 char *spi)                 // IN
{
   int len;
   static char *empty = "";
   uint16 languageTagLen;
   uint16 languageTagLenBE; /* big endian */
   uint16 prListLen;
   uint16 prListLenBE;
   uint16 serviceTypeLen;
   uint16 serviceTypeLenBE;
   uint16 scopeListLen;
   uint16 scopeListLenBE;
   uint16 predicateLen;
   uint16 predicateLenBE;
   uint16 spiLen;
   uint16 spiLenBE;
   DynBuf b;

   DynBuf_Init(&b);
   ASSERT(NULL != packet && NULL != packetSize);

   languageTag = (NULL == languageTag) ? empty : languageTag;
   prList      = (NULL == prList)      ? empty : prList;
   serviceType = (NULL == serviceType) ? empty : serviceType;
   scopeList   = (NULL == scopeList)   ? empty : scopeList;
   predicate   = (NULL == predicate)   ? empty : predicate;
   spi         = (NULL == spi)         ? empty : spi;

   if (strlen(languageTag)    > 65535 || strlen(prList)    > 65535
       || strlen(serviceType) > 65535 || strlen(scopeList) > 65535
       || strlen(predicate)   > 65535 || strlen(spi)       > 65535) {
      return FALSE;
   }

   languageTagLen   = strlen(languageTag);
   languageTagLenBE = Portable_htons(languageTagLen);
   prListLen        = strlen(prList);
   prListLenBE      = Portable_htons(prListLen);
   serviceTypeLen   = strlen(serviceType);
   serviceTypeLenBE = Portable_htons(serviceTypeLen);
   scopeListLen     = strlen(scopeList);
   scopeListLenBE   = Portable_htons(scopeListLen);
   predicateLen     = strlen(predicate);
   predicateLenBE   = Portable_htons(predicateLen);
   spiLen           = strlen(spi);
   spiLenBE         = Portable_htons(spiLen);

   len = sizeof (struct SLPv2_Header)
         + sizeof languageTagLen + languageTagLen
         + sizeof prListLen      + prListLen
         + sizeof serviceTypeLen + serviceTypeLen
         + sizeof scopeListLen   + scopeListLen
         + sizeof predicateLen   + predicateLen
         + sizeof spiLen         + spiLen;

   if (! SLPv2MsgAssemblerHeader(&b, SLPV2_SERVICEREQUEST, len,
                                     overflowFlag, freshFlag,
                                     requestMulticastFlag, xid)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &languageTagLenBE, sizeof languageTagLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, languageTag, languageTagLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &prListLenBE, sizeof prListLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, prList, prListLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &serviceTypeLenBE, sizeof serviceTypeLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, serviceType, serviceTypeLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &scopeListLenBE, sizeof scopeListLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, scopeList, scopeListLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &predicateLenBE, sizeof predicateLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, predicate, predicateLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &spiLenBE, sizeof spiLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, spi, spiLen)) {
      goto abort;
   }

   ASSERT(DynBuf_GetSize(&b) == len);
   DynBuf_Trim(&b);
   if (NULL != packetSize) {
      *packetSize = DynBuf_GetSize(&b);
   }
   if (NULL != packet) {
      *packet = DynBuf_Detach(&b);
   }
   DynBuf_Destroy(&b);
   return TRUE;

abort:
   DynBuf_Destroy(&b);
   return FALSE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgAssembler_ServiceReply --
 *
 *      Appends an SLPv2 Service Reply to a DynBuf.
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
SLPv2MsgAssembler_ServiceReply(char **packet,             // OUT
                               int *packetSize,           // OUT
                               uint16 xid,                // IN
                               char *languageTag,         // IN
                               uint16 errorCode,          // IN
                               uint16 urlEntryCount,      // IN
                               char **urls)               // IN
{
   int len;
   int urlTotalLength = 0;
   int i;
   static char *empty = "";
   uint16 languageTagLen;
   uint16 urlStringLength;
   DynBuf b;

   DynBuf_Init(&b);
   ASSERT(NULL != packet && NULL != packetSize);

   languageTag = (NULL == languageTag) ? empty : languageTag;

   if (strlen(languageTag) > 65535) {
      return FALSE;
   }

   languageTagLen = strlen(languageTag);

   /*
    * Compute the total length of all strings pointed to by "urls".
    */
   if (NULL != urls) {
      urlTotalLength = urlEntryCount * 2;
      for (i = 0; i < urlEntryCount; i++) {
         ASSERT(NULL != urls[i]);
         urlTotalLength += strlen(urls[i]);
      }
   } else {
      /*
       * If urls==NULL, then urlEntryCount better be zero!
       */
      ASSERT(0 == urlEntryCount);
   }

   len = sizeof (struct SLPv2_Header) + sizeof languageTagLen + languageTagLen
         + sizeof errorCode + sizeof urlEntryCount + urlTotalLength;

   languageTagLen = Portable_htons(languageTagLen);
   errorCode      = Portable_htons(errorCode);
   urlEntryCount  = Portable_htons(urlEntryCount);

   if (! SLPv2MsgAssemblerHeader(&b, SLPV2_SERVICEREPLY, len,
                                 FALSE, FALSE, FALSE, xid)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &languageTagLen, sizeof languageTagLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, languageTag, Portable_ntohs(languageTagLen))) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &errorCode, sizeof errorCode)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &urlEntryCount, sizeof urlEntryCount)) {
      goto abort;
   }

   /*
    * Append URLs.
    */
   urlEntryCount  = Portable_ntohs(urlEntryCount);
   for (i = 0 ; i < urlEntryCount ; i++) {
      urlStringLength = Portable_htons(strlen(urls[i]));
      if (! DynBuf_Append(&b, &urlStringLength, sizeof urlStringLength)) {
         goto abort;
      }
      if (! DynBuf_Append(&b, urls[i], Portable_ntohs(urlStringLength))) {
         goto abort;
      }
   }
   
  
   ASSERT(DynBuf_GetSize(&b) == len);
   DynBuf_Trim(&b);
   if (NULL != packetSize) {
      *packetSize = DynBuf_GetSize(&b);
   }
   if (NULL != packet) {
      *packet = DynBuf_Detach(&b);
   }
   DynBuf_Destroy(&b);
   return TRUE;

abort:
   DynBuf_Destroy(&b);
   return FALSE;

}


/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgAssembler_AttributeRequest --
 *
 *      Appends an SLPv2 Attribute Request to a DynBuf.
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
SLPv2MsgAssembler_AttributeRequest(char **packet,             // OUT
                                   int *packetSize,           // OUT
                                   uint16 xid,                // IN
                                   Bool overflowFlag,         // IN
                                   Bool freshFlag,            // IN
                                   Bool requestMulticastFlag, // IN
                                   char *languageTag,         // IN
                                   char *prList,              // IN
                                   char *url,                 // IN
                                   char *scopeList,           // IN
                                   char *tagList,             // IN
                                   char *spi)                 // IN
{
   int len;
   static char *empty = "";
   uint16 languageTagLen;
   uint16 prListLen;
   uint16 urlLen;
   uint16 scopeListLen;
   uint16 tagListLen;
   uint16 spiLen;
   uint16 languageTagLenBE; /* big endian */
   uint16 prListLenBE;
   uint16 urlLenBE;
   uint16 scopeListLenBE;
   uint16 tagListLenBE;
   uint16 spiLenBE;
   DynBuf b;


   DynBuf_Init(&b);
   ASSERT(NULL != packet && NULL != packetSize);

   languageTag = (NULL == languageTag) ? empty : languageTag;
   prList      = (NULL == prList)      ? empty : prList;
   url         = (NULL == url)         ? empty : url;
   scopeList   = (NULL == scopeList)   ? empty : scopeList;
   tagList     = (NULL == tagList)     ? empty : tagList;
   spi         = (NULL == spi)         ? empty : spi;

   if (strlen(languageTag) > 65535 || strlen(prList)    > 65535
        || strlen(url)     > 65535 || strlen(scopeList) > 65535
        || strlen(tagList) > 65535 || strlen(spi)       > 65535) {
      return FALSE;
   }

   languageTagLen   = strlen(languageTag);
   languageTagLenBE = Portable_htons(languageTagLen);
   prListLen        = strlen(prList);
   prListLenBE      = Portable_htons(prListLen);
   urlLen           = strlen(url);
   urlLenBE         = Portable_htons(urlLen);
   scopeListLen     = strlen(scopeList);
   scopeListLenBE   = Portable_htons(scopeListLen);
   tagListLen       = strlen(tagList);
   tagListLenBE     = Portable_htons(tagListLen);
   spiLen           = strlen(spi);
   spiLenBE         = Portable_htons(spiLen);

   len = sizeof (struct SLPv2_Header)
         + sizeof languageTagLen + languageTagLen
         + sizeof prListLen      + prListLen
         + sizeof urlLen         + urlLen
         + sizeof scopeListLen   + scopeListLen
         + sizeof tagListLen     + tagListLen
         + sizeof spiLen         + spiLen;

   if (! SLPv2MsgAssemblerHeader(&b, SLPV2_ATTRIBUTEREQUEST, len,
                                     overflowFlag, freshFlag,
                                     requestMulticastFlag, xid)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &languageTagLenBE, sizeof languageTagLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, languageTag, languageTagLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &prListLenBE, sizeof prListLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, prList, prListLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &urlLenBE, sizeof urlLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, url, urlLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &scopeListLenBE, sizeof scopeListLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, scopeList, scopeListLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &tagListLenBE, sizeof tagListLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, tagList, tagListLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &spiLenBE, sizeof spiLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, spi, spiLen)) {
      goto abort;
   }

   ASSERT(DynBuf_GetSize(&b) == len);
   DynBuf_Trim(&b);
   if (NULL != packetSize) {
      *packetSize = DynBuf_GetSize(&b);
   }
   if (NULL != packet) {
      *packet = DynBuf_Detach(&b);
   }
   DynBuf_Destroy(&b);
   return TRUE;

abort:
   DynBuf_Destroy(&b);
   return FALSE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SLPv2MsgAssembler_AttributeReply --
 *
 *      Appends an SLPv2 Attribute Reply to a DynBuf.
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
SLPv2MsgAssembler_AttributeReply(char **packet,        // OUT
                                 int *packetSize,      // OUT
                                 uint16 xid,           // IN
                                 char *languageTag,    // IN
                                 uint16 errorCode,     // IN
                                 char *attributeList)  // IN
{
   int len;
   static char *empty = "";
   uint16 languageTagLen;
   uint16 languageTagLenBE; /* big endian */
   uint16 attributeListLen;
   uint16 attributeListLenBE;
   uint16 errorCodeBE;
   DynBuf b;

   DynBuf_Init(&b);
   ASSERT(NULL != packet && NULL != packetSize);

   languageTag   = (NULL == languageTag)   ? empty : languageTag;
   attributeList = (NULL == attributeList) ? empty : attributeList;

   if (strlen(languageTag) > 65535 || strlen(attributeList) > 65535) {
      return FALSE;
   }

   errorCodeBE        = Portable_htons(errorCode);
   languageTagLen     = strlen(languageTag);
   languageTagLenBE   = Portable_htons(languageTagLen);
   attributeListLen   = strlen(attributeList);
   attributeListLenBE = Portable_htons(attributeListLen);

   len = sizeof (struct SLPv2_Header)
         + sizeof languageTagLen   + languageTagLen
         + sizeof errorCode
         + sizeof attributeListLen + attributeListLen;

   if (! SLPv2MsgAssemblerHeader(&b, SLPV2_ATTRIBUTEREPLY, len,
                                     FALSE, FALSE, FALSE, xid)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &languageTagLenBE, sizeof languageTagLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, languageTag, languageTagLen)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &errorCodeBE, sizeof errorCodeBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, &attributeListLenBE, sizeof attributeListLenBE)) {
      goto abort;
   }
   if (! DynBuf_Append(&b, attributeList, attributeListLen)) {
      goto abort;
   }

   ASSERT(DynBuf_GetSize(&b) == len);
   DynBuf_Trim(&b);
   if (NULL != packetSize) {
      *packetSize = DynBuf_GetSize(&b);
   }
   if (NULL != packet) {
      *packet = DynBuf_Detach(&b);
   }
   DynBuf_Destroy(&b);
   return TRUE;

abort:
   DynBuf_Destroy(&b);
   return FALSE;
}

