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

#ifndef _SLPV2_PRIVATE_H_
#define _SLPV2_PRIVATE_H_

#define SLPV2_PORT 427
#define SLPV2_HIGHPORT 61526 /* vmware internal */

/*
 * SLPv2 header constants
 */
#define SLPV2_VERSION              2

/*
 * SLPv2 Function IDs
 */
#define SLPV2_SERVICEREQUEST       1
#define SLPV2_SERVICEREPLY         2
#define SLPV2_ATTRIBUTEREQUEST     6
#define SLPV2_ATTRIBUTEREPLY       7


/*
 * These are procedures on windows. But, networking is not available on
 * all guests (I'm looking at you, Win95), so we cannot link with htons
 * or similar functions.
 */
#define Portable_ntohl(in) ((in >> 24) & 0x000000ff) | ((in >> 8) & 0x0000ff00) |  \
                                  ((in << 8) & 0x00ff0000) | ((in << 24) & 0xff000000)
#define Portable_htonl(in) Portable_ntohl(in)
#define Portable_ntohs(in) ((in >> 8) & 0x00ff) | ((in << 8) & 0xff00)
#define Portable_htons(in) Portable_ntohs(in)


/*
 *  From RFC 2608, Section 8.  SLPv2 Header:
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|    Version    |  Function-ID  |            Length             |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *| Length, contd.|O|F|R|       reserved          |Next Ext Offset|
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|  Next Extension Offset, contd.|              XID              |
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *|      Language Tag Length      |         Language Tag          \
 *+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
#include "vmware_pack_begin.h"
struct SLPv2_Header {
   uint8 version;
   uint8 functionId;
   uint8 length[3];  // "uint32 length:24" doesn't pack correctly w/Win32.
   uint16 flags;
   uint8 extOffset[3]; // "uint32 extOffset:24" doesn't pack correctly w/Win32.
   uint16 xid;
}
#include "vmware_pack_end.h"
;

/*
 *  From RFC 2608, Section 4.3.  SLPv2 URL Entry:
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |   Reserved    |          Lifetime             |   URL Length  |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |URL len, contd.|            URL (variable length)              \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |# of URL auths |            Auth. blocks (if any)              \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */


#include "vmware_pack_begin.h"
struct SLPv2_URL {
   uint8 reserved;
   uint16 lifetime;
   uint16 length;
}
#include "vmware_pack_end.h"
;

struct SLPv2_ServiceRequest {
   char *prList;       /* Previous Responder List */
   char *serviceType;
   char *scope;
   char *predicate;    /* LDAPv3 search filter, optional */
   char *spi;          /* SLP Security Parameter Index */
};

struct SLPv2_ServiceReply {
   uint16 error;
   uint16 urlCount;
   char **url;
};

struct SLPv2_AttributeRequest {
   char *prList;       /* Previous Responder List */
   char *url;
   char *scope;
   char *tagList;
   char *spi;          /* SLP Security Parameter Index */
};

struct SLPv2_AttributeReply {
   uint16 error;
   char *attributeList;
};

struct SLPv2_Parse {
   struct SLPv2_Header *header;
   uint16 languageTagLength;
   char *languageTag;
   struct SLPv2_ServiceRequest   serviceRequest;
   struct SLPv2_ServiceReply     serviceReply;
   struct SLPv2_AttributeRequest attributeRequest;
   struct SLPv2_AttributeReply   attributeReply;
};


/*
 * From RFC 2608, Section 8.1.  Service Request:
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |       Service Location header (function = SrvRqst = 1)        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |      length of <PRList>       |        <PRList> String        \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |   length of <service-type>    |    <service-type> String      \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |    length of <scope-list>     |     <scope-list> String       \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  length of predicate string   |  Service Request <predicate>  \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  length of <SLP SPI> string   |       <SLP SPI> String        \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/*
 * From RFC 2608, Section 8.2.  Service Reply:
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |        Service Location header (function = SrvRply = 2)       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |        Error Code             |        URL Entry count        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |       <URL Entry 1>          ...       <URL Entry N>          \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/*
 * From RFC 2608, Section 10.3.  Attribute Request:
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |       Service Location header (function = AttrRqst = 6)       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |       length of PRList        |        <PRList> String        \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |         length of URL         |              URL              \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |    length of <scope-list>     |      <scope-list> string      \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  length of <tag-list> string  |       <tag-list> string       \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |   length of <SLP SPI> string  |        <SLP SPI> string       \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/*
 * From RFC 2608, Section 10.4.  Attribute Reply:
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |       Service Location header (function = AttrRply = 7)       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |         Error Code            |      length of <attr-list>    |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         <attr-list>                           \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |# of AttrAuths |  Attribute Authentication Block (if present)  \
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */


/*
 * SLPv2 Parsing prototypes.
 */
struct SLPv2_Parse *SLPv2MsgParser_Init(void);
Bool SLPv2MsgParser_Parse(struct SLPv2_Parse *parse, char *packet, int len);
void SLPv2MsgParser_Destroy(struct SLPv2_Parse *parse);


/*
 * Matching.
 */

Bool SLPv2MsgParser_ServiceRequestMatch(struct SLPv2_Parse *parse, // IN
                                        char *myIpsList,           // UN
                                        char *myServiceType,       // IN
                                        char *myScope,             // IN
                                        char *myPredicate,         // IN
                                        uint16 *xid);              // OUT

Bool SLPv2MsgParser_ServiceReplyMatch(struct SLPv2_Parse *parse, // IN
                                      int *urlCount,             // OUT
                                      char ***urlArray,          // OUT
                                      uint16 *xid);              // OUT

Bool SLPv2MsgParser_AttributeRequestMatch(struct SLPv2_Parse *parse, // IN
                                          char *myIpsList,           // UN
                                          char *url,                 // IN
                                          char *myScope,             // IN
                                          char *tagList,             // IN
                                          uint16 *xid);              // OUT

Bool SLPv2MsgParser_AttributeReplyMatch(struct SLPv2_Parse *parse, // IN
                                        char **attributeList,      // OUT
                                        uint16 *xid);              // OUT



/*
 * SLPv2 packet generation prototypes.
 */
Bool SLPv2MsgAssembler_ServiceRequest(char **packet,             // OUT
                                      int  *packetSize,          // OUT
                                      uint16 xid,                // IN
                                      Bool overflowFlag,         // IN
                                      Bool freshFlag,            // IN
                                      Bool requestMulticastFlag, // IN
                                      char *languageTag,         // IN
                                      char *prList,              // IN
                                      char *serviceType,         // IN
                                      char *scopeList,           // IN
                                      char *predicate,           // IN
                                      char *spi);                // IN

Bool SLPv2MsgAssembler_ServiceReply(char **packet,             // OUT
                                    int  *packetSize,          // OUT
                                    uint16 xid,                // IN
                                    char *languageTag,         // IN
                                    uint16 errorCode,          // IN
                                    uint16 urlEntryCount,      // IN
                                    char **urls);              // IN

Bool SLPv2MsgAssembler_AttributeRequest(char **packet,             // OUT
                                        int  *packetSize,          // OUT
                                        uint16 xid,                // IN
                                        Bool overflowFlag,         // IN
                                        Bool freshFlag,            // IN
                                        Bool requestMulticastFlag, // IN
                                        char *languageTag,         // IN
                                        char *prList,              // IN
                                        char *url,                 // IN
                                        char *scopeList,           // IN
                                        char *tagList,             // IN
                                        char *spi);                // IN

Bool SLPv2MsgAssembler_AttributeReply(char **packet,        // OUT
                                      int  *packetSize,     // OUT
                                      uint16 xid,           // IN
                                      char *languageTag,    // IN
                                      uint16 errorCode,     // IN
                                      char *attributeList); // IN




#endif // _SLPV2_PRIVATE_H_



