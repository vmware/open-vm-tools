/*********************************************************
 * Copyright (c) 2012-2017, 2019-2021, 2023 VMware, Inc. All rights reserved.
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
 * @file proto.c
 *
 * Client/service protocol
 */
#ifndef _WIN32
#include <errno.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "VGAuthInt.h"
#include "VGAuthProto.h"
#include "VGAuthLog.h"
#include "VGAuthUtil.h"
#include "usercheck.h"

/* cranks up parser debugging */
#define VGAUTH_PROTO_TRACE 0

/*
 * Reply types
 */
typedef enum {
   PROTO_REQUEST_UNKNOWN,
   PROTO_REPLY_ERROR,
   PROTO_REPLY_SESSION_REQ,
   PROTO_REPLY_CONN,
   PROTO_REPLY_ADDALIAS,
   PROTO_REPLY_REMOVEALIAS,
   PROTO_REPLY_QUERYALIASES,
   PROTO_REPLY_QUERYMAPPEDALIASES,
   PROTO_REPLY_CREATETICKET,
   PROTO_REPLY_VALIDATETICKET,
   PROTO_REPLY_REVOKETICKET,
   PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN,
} ProtoReplyType;

/*
 * Possible parse states for replies.
 */
typedef enum {
   PARSE_STATE_NONE,

   PARSE_STATE_SEQ,

   PARSE_STATE_ERROR,
   PARSE_STATE_ERROR_CODE,
   PARSE_STATE_ERROR_MSG,

   PARSE_STATE_REPLY,

   PARSE_STATE_VERSION,
   PARSE_STATE_PIPENAME,

   PARSE_STATE_PEMCERT,
   PARSE_STATE_CERTCOMMENT,

   PARSE_STATE_ALIAS,
   PARSE_STATE_ALIASINFO,
   PARSE_STATE_NAMEDSUBJECT,
   PARSE_STATE_ANYSUBJECT,
   PARSE_STATE_COMMENT,

   PARSE_STATE_MAPPEDALIAS,
   PARSE_STATE_SUBJECTS,

   PARSE_STATE_TICKET,

   PARSE_STATE_USERHANDLEINFO,
   PARSE_STATE_USERHANDLETYPE,
   PARSE_STATE_USERHANDLESAMLINFO,
   PARSE_STATE_USERHANDLESAMLSUBJECT,

   PARSE_STATE_USERNAME,
   PARSE_STATE_TOKEN,
   PARSE_STATE_CHALLENGE_EVENT,
} ProtoParseState;


/*
 * The reply structure.
 */
struct ProtoReply {
   gboolean complete;
   int sequenceNumber;

   /*
    * The client knows what its expecting back, which is
    * used as a confidence check against what's actually read,
    * as well as telling us what to allocate for complex replies.
    */
   ProtoReplyType expectedReplyType;

   /*
    * If its an error, this will be set instead.
    */
   ProtoReplyType actualReplyType;

   ProtoParseState parseState;

   VGAuthError errorCode;

   union {
      struct {
         gchar *errorMsg;
      } error;
      struct {
         int version;
         gchar *pipeName;
      } sessionReq;
      struct {
         gchar *challengeEvent;
      } connect;
      struct {
         int num;
         VGAuthUserAlias *uaList;
      } queryUserAliases;
      struct {
         int num;
         VGAuthMappedAlias *maList;
      } queryMappedAliases;
      struct {
         gchar *ticket;
      } createTicket;
      struct {
         gchar *userName;
         gchar *token;
         VGAuthUserHandleType type;
         gchar *samlSubject;
         VGAuthAliasInfo aliasInfo;
      } validateTicket;
      struct {
         gchar *userName;
         char *comment;
         gchar *token;
         gchar *samlSubject;
         VGAuthAliasInfo aliasInfo;
      } validateSamlBToken;
   } replyData;

#if VGAUTH_PROTO_TRACE
   gchar *rawData;
#endif
};


typedef struct ProtoReply ProtoReply;


#if VGAUTH_PROTO_TRACE

/*
 ******************************************************************************
 * ProtoSubjectToString --                                               */ /**
 *
 * Debugging.  Returns the name of a VGAuthSubject.
 *
 * @param[in]  subj         The VGAuthSubject to dump.
 *
 ******************************************************************************
 */

static const gchar *
ProtoSubjectToString(const VGAuthSubject *subj)
{
   if (VGAUTH_SUBJECT_NAMED == subj->type) {
      return subj->val.name;
   } else if (VGAUTH_SUBJECT_ANY == subj->type) {
      return "<ANY>";
   } else {
      return "<UNKNOWN>";
   }
}


/*
 ******************************************************************************
 * Proto_DumpReply --                                                    */ /**
 *
 * Debugging.  Spews a ProtoReply to stdout.
 *
 * @param[in]  reply        The reply to dump.
 *
 ******************************************************************************
 */

static void
Proto_DumpReply(ProtoReply *reply)
{
   int i;
   int j;
   VGAuthUserAlias *ua;
   VGAuthAliasInfo *ai;

   printf("raw data: %s\n", reply->rawData ? reply->rawData : "<none>");
   printf("complete: %d\n", reply->complete);
   printf("sequenceNumber: %d\n", reply->sequenceNumber);
   printf("expectedReplyType: %d\n", reply->expectedReplyType);
   printf("actualReplyType: %d\n", reply->actualReplyType);
   printf("error code: "VGAUTHERR_FMT64X"\n", reply->errorCode);

   switch (reply->actualReplyType) {
   case PROTO_REPLY_ERROR:
      printf("error message: '%s'\n", reply->replyData.error.errorMsg ? reply->replyData.error.errorMsg :  "<none>");
      break;
   case PROTO_REPLY_SESSION_REQ:
      printf("version #: %d\n", reply->replyData.sessionReq.version);
      printf("pipeName: '%s'\n", reply->replyData.sessionReq.pipeName);
      break;
   case PROTO_REPLY_CONN:
   case PROTO_REPLY_ADDALIAS:
   case PROTO_REPLY_REMOVEALIAS:
   case PROTO_REPLY_REVOKETICKET:
      break;
   case PROTO_REPLY_QUERYALIASES:
      printf("#%d UserAliases:\n", reply->replyData.queryUserAliases.num);
      for (i = 0; i < reply->replyData.queryUserAliases.num; i++) {
         ua = &(reply->replyData.queryUserAliases.uaList[i]);
         printf("permCert: '%s'\n", ua->pemCert);
         for (j = 0; j < ua->numInfos; j++) {
            ai = &(ua->infos[j]);
            printf("\tsubject: '%s'\n", ProtoSubjectToString(&(ai->subject)));
            printf("\tcomment: '%s'\n", ai->comment);
         }
      }
      break;
   case PROTO_REPLY_QUERYMAPPEDALIASES:
      printf("#%d identities:\n", reply->replyData.queryMappedAliases.num);
      for (i = 0; i < reply->replyData.queryMappedAliases.num; i++) {
         printf("pemCert: '%s'\n", reply->replyData.queryMappedAliases.maList[i].pemCert);
         for (j = 0; j < reply->replyData.queryMappedAliases.maList[i].numSubjects; j++) {
            printf("subject #%d: '%s'\n", j, ProtoSubjectToString(&reply->replyData.queryMappedAliases.maList[i].subjects[j]));
         }
         printf("mapped user: '%s'\n", reply->replyData.queryMappedAliases.maList[i].userName);
      }
      break;
   case PROTO_REPLY_CREATETICKET:
      printf("ticket '%s'\n", reply->replyData.createTicket.ticket);
      break;
   case PROTO_REPLY_VALIDATETICKET:
      printf("username: '%s'\n", reply->replyData.validateTicket.userName);
      printf("validate type: %d\n", reply->replyData.validateTicket.type);
      if (VGAUTH_AUTH_TYPE_SAML == reply->replyData.validateTicket.type) {
         printf("SAML subject: '%s'\n",
                reply->replyData.validateTicket.samlSubject);
         ai = &(reply->replyData.validateTicket.aliasInfo);
         printf("\tsubject: '%s'\n", ProtoSubjectToString(&(ai->subject)));
         printf("\tcomment: '%s'\n", ai->comment);
      }
      break;
   case PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN:
      printf("username: '%s'\n", reply->replyData.validateSamlBToken.userName);
      printf("SAML subject: '%s'\n",
             reply->replyData.validateTicket.samlSubject);
      ai = &(reply->replyData.validateTicket.aliasInfo);
      printf("\tsubject: '%s'\n", ProtoSubjectToString(&(ai->subject)));
      printf("\tcomment: '%s'\n", ai->comment);
      break;
   default:
      printf("no reply specific data\n");
      break;
   }
}
#endif      // VGAUTH_PROTO_TRACE


/*
 ******************************************************************************
 * Proto_ConcatXMLStrings --                                             */ /**
 *
 * Concatenates 2 XML strings and returns the new string.
 * g_free()s the two inputs.
 * Result must be g_free()d.
 *
 * @param[in]  str1     The first string.
 * @param[in]  str2     The second string.
 *
 * @return The new string.
 *
 ******************************************************************************
 */

static gchar *
Proto_ConcatXMLStrings(gchar *str1,
                       gchar *str2)
{
   gchar *newStr;

   newStr = g_strdup_printf("%s%s", str1, str2);
   g_free(str1);
   g_free(str2);

   return newStr;
}


/*
 ******************************************************************************
 * ProtoUserHandleTypeString --                                          */ /**
 *
 * Returns the type of a VGAuthUserHandle as a protocol string.
 *
 * @param[in]  userHandle        The VGAuthUSerHandle.
 *
 * @return The type as a string.
 *
 ******************************************************************************
 */

static const gchar *
ProtoUserHandleTypeString(const VGAuthUserHandle *userHandle)
{
   switch (userHandle->details.type) {
   case VGAUTH_AUTH_TYPE_NAMEPASSWORD:
      return VGAUTH_USERHANDLE_TYPE_NAMEPASSWORD;
   case VGAUTH_AUTH_TYPE_SSPI:
      return VGAUTH_USERHANDLE_TYPE_SSPI;
      break;
   case VGAUTH_AUTH_TYPE_SAML:
      return VGAUTH_USERHANDLE_TYPE_SAML;
   case VGAUTH_AUTH_TYPE_SAML_INFO_ONLY:
      return VGAUTH_USERHANDLE_TYPE_SAML_INFO_ONLY;
   case VGAUTH_AUTH_TYPE_UNKNOWN:
   default:
      ASSERT(0);
      Warning("%s: Unsupported handleType %d\n", __FUNCTION__, userHandle->details.type);
      return "<UNKNOWN>";
   }
}


/*
 ******************************************************************************
 * Proto_StartElement --                                                 */ /**
 *
 * Called by the XML parser when it sees the start of a new
 * element.  Used to update the current parser state, and allocate
 * any space that may be needed for processing that state.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  elementName         The name of the element being started.
 * @param[in]  attributeNames      The names of any attributes on the element.
 * @param[in]  attributeValues     The values of any attributes on the element.
 * @param[in]  userData            The current ProtoReply as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
Proto_StartElement(GMarkupParseContext *parseContext,
                   const gchar *elementName,
                   const gchar **attributeNames,
                   const gchar **attributeValues,
                   gpointer userData,
                   GError **error)
{
   ProtoReply *reply = (ProtoReply *) userData;

#if VGAUTH_PROTO_TRACE
   Debug("%s: elementName '%s', parseState %d, cur reply type %d\n", __FUNCTION__, elementName, reply->parseState, reply->expectedReplyType);
#endif

   switch (reply->parseState) {
   case PARSE_STATE_NONE:
      /*
       * We're in 'idle' mode, expecting a fresh reply.
       */
      if (g_strcmp0(elementName, VGAUTH_REPLY_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_REPLY;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
      }
      break;
   case PARSE_STATE_REPLY:
      /*
       * We're in 'reply' mode, expecting some element inside the reply.
       */
      if (g_strcmp0(elementName, VGAUTH_SEQUENCENO_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_SEQ;
      } else if (g_strcmp0(elementName, VGAUTH_ERRORCODE_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_ERROR_CODE;
         reply->actualReplyType = PROTO_REPLY_ERROR;
      } else if (g_strcmp0(elementName, VGAUTH_ERRORMSG_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_ERROR_MSG;
         reply->actualReplyType = PROTO_REPLY_ERROR;
      } else if (g_strcmp0(elementName, VGAUTH_VERSION_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_VERSION;
         if (PROTO_REPLY_SESSION_REQ != reply->expectedReplyType) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
      } else if (g_strcmp0(elementName, VGAUTH_PIPENAME_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_PIPENAME;
         if (PROTO_REPLY_SESSION_REQ != reply->expectedReplyType) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
      } else if (g_strcmp0(elementName, VGAUTH_TOKEN_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_TOKEN;
         if ((PROTO_REPLY_VALIDATETICKET != reply->expectedReplyType) &&
            (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN !=
             reply->expectedReplyType)) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
      } else if (g_strcmp0(elementName, VGAUTH_USERHANDLEINFO_ELEMENT_NAME) == 0) {
         if (PROTO_REPLY_VALIDATETICKET != reply->expectedReplyType) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         } else {
            reply->parseState = PARSE_STATE_USERHANDLEINFO;
         }
      } else if (g_strcmp0(elementName, VGAUTH_CHALLENGE_EVENT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_CHALLENGE_EVENT;
         if ((PROTO_REPLY_CONN != reply->expectedReplyType)) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
      } else if (g_strcmp0(elementName, VGAUTH_USERNAME_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_USERNAME;
         if ((PROTO_REPLY_VALIDATETICKET != reply->expectedReplyType) &&
             (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN !=
              reply->expectedReplyType)) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
         reply->parseState = PARSE_STATE_USERNAME;
      } else if (g_strcmp0(elementName, VGAUTH_TICKET_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_TICKET;
         if (PROTO_REPLY_CREATETICKET != reply->expectedReplyType) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
      } else if (g_strcmp0(elementName, VGAUTH_COMMENT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_CERTCOMMENT;
         if (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN !=
             reply->expectedReplyType) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
      } else if (g_strcmp0(elementName, VGAUTH_ALIAS_ELEMENT_NAME) == 0) {
         VGAuthUserAlias *a;

         if (PROTO_REPLY_QUERYALIASES != reply->expectedReplyType) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         } else {
            reply->parseState = PARSE_STATE_ALIAS;

            a = reply->replyData.queryUserAliases.uaList;
            reply->replyData.queryUserAliases.num++;
            a = g_realloc_n(a,
                            reply->replyData.queryUserAliases.num,
                            sizeof(VGAuthUserAlias));
            reply->replyData.queryUserAliases.uaList = a;
            reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1].numInfos = 0;
            reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1].infos = NULL;
         }
      } else if (g_strcmp0(elementName, VGAUTH_MAPPEDALIASES_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_MAPPEDALIAS;
         reply->replyData.queryMappedAliases.num++;
         reply->replyData.queryMappedAliases.maList = g_realloc_n(reply->replyData.queryMappedAliases.maList,
                                        reply->replyData.queryMappedAliases.num,
                                        sizeof(VGAuthMappedAlias));
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].pemCert = NULL;
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].userName = NULL;
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].numSubjects = 0;
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].subjects = NULL;

      } else if (g_strcmp0(elementName, VGAUTH_USERHANDLESAMLINFO_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
      }
      break;
   case PARSE_STATE_ALIAS:
      if (g_strcmp0(elementName, VGAUTH_PEMCERT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_PEMCERT;
      } else if (g_strcmp0(elementName, VGAUTH_ALIASINFO_ELEMENT_NAME) == 0) {
         VGAuthAliasInfo *info;
         VGAuthUserAlias *ip = &(reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1]);

         reply->parseState = PARSE_STATE_ALIASINFO;

         // grow the AliasInfo array
         info = ip->infos;
         ip->numInfos++;

         info = g_realloc_n(info,
                            ip->numInfos,
                            sizeof(VGAuthAliasInfo));
         ip->infos = info;
         ip->infos[ip->numInfos - 1].subject.type = -1;
         ip->infos[ip->numInfos - 1].subject.val.name = NULL;
         ip->infos[ip->numInfos - 1].comment = NULL;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
      }
      break;
   case PARSE_STATE_USERHANDLEINFO:
      if (PROTO_REPLY_VALIDATETICKET != reply->expectedReplyType) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Got '%s' when expecting a reply of type %d",
                     elementName, reply->expectedReplyType);
      }
      if (g_strcmp0(elementName, VGAUTH_USERHANDLETYPE_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_USERHANDLETYPE;
      } else if (g_strcmp0(elementName, VGAUTH_USERHANDLESAMLINFO_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
      }
      break;
   case PARSE_STATE_USERHANDLESAMLINFO:
      if (g_strcmp0(elementName, VGAUTH_USERHANDLESAMLSUBJECT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_USERHANDLESAMLSUBJECT;
      } else if (g_strcmp0(elementName, VGAUTH_ALIASINFO_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_ALIASINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
      }
      break;
   case PARSE_STATE_ALIASINFO:
      if (g_strcmp0(elementName, VGAUTH_COMMENT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_COMMENT;
      } else if (g_strcmp0(elementName, VGAUTH_SUBJECT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_NAMEDSUBJECT;
      } else if (g_strcmp0(elementName, VGAUTH_ANYSUBJECT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_ANYSUBJECT;
         /*
          * Since this is an empty-element tag, the Contents code will
          * not be called, so do the work here.
          */
         if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
            VGAuthAliasInfo *info;
            VGAuthUserAlias *ip = &(reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1]);

            info = &(ip->infos[ip->numInfos - 1]);
            info->subject.type = VGAUTH_SUBJECT_ANY;
         } else if (PROTO_REPLY_VALIDATETICKET == reply->expectedReplyType) {
            reply->replyData.validateTicket.aliasInfo.subject.type = VGAUTH_SUBJECT_ANY;
         } else if (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN == reply->expectedReplyType) {
            reply->replyData.validateSamlBToken.aliasInfo.subject.type = VGAUTH_SUBJECT_ANY;
         } else {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Got '%s' when expecting a reply of type %d",
                        elementName, reply->expectedReplyType);
         }
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
      }
      break;
   case PARSE_STATE_MAPPEDALIAS:
      if (g_strcmp0(elementName, VGAUTH_USERNAME_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_USERNAME;
      } else if (g_strcmp0(elementName, VGAUTH_PEMCERT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_PEMCERT;
      } else if (g_strcmp0(elementName, VGAUTH_SUBJECTS_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_SUBJECTS;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
      }
      break;
   case PARSE_STATE_SUBJECTS:
      {
      int n;
      VGAuthSubject *subjs;
      VGAuthSubjectType sType = -1;

      if (g_strcmp0(elementName, VGAUTH_SUBJECT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_NAMEDSUBJECT;
         sType = VGAUTH_SUBJECT_NAMED;
      } else if (g_strcmp0(elementName, VGAUTH_ANYSUBJECT_ELEMENT_NAME) == 0) {
         reply->parseState = PARSE_STATE_ANYSUBJECT;
         sType = VGAUTH_SUBJECT_ANY;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, reply->parseState);
         break;
      }

      // got a new Subject or AnySubject, grow
      n = ++(reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].numSubjects);
      subjs = reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].subjects;
      subjs = g_realloc_n(subjs, n,  sizeof(VGAuthSubject));
      subjs[n - 1].type = sType;
      subjs[n - 1].val.name = NULL;
      reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].subjects = subjs;
      }
      break;
   default:
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Unexpected element '%s' in parse state %d",
                  elementName, reply->parseState);

      break;
   }
}


/*
 ******************************************************************************
 * Proto_EndElement --                                                   */ /**
 *
 * Called by the XML parser when the end of an element is reached.
 * Used here to pop the parse state.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  elementName         The name of the element being started.
 * @param[in]  userData            The current ProtoReply as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
Proto_EndElement(GMarkupParseContext *parseContext,
                 const gchar *elementName,
                 gpointer userData,
                 GError **error)
{
   ProtoReply *reply = (ProtoReply *) userData;

#if VGAUTH_PROTO_TRACE
   Debug("%s: elementName '%s'\n", __FUNCTION__, elementName);
#endif

   switch (reply->parseState) {
   case PARSE_STATE_SEQ:
   case PARSE_STATE_ERROR_CODE:
   case PARSE_STATE_ERROR_MSG:
   case PARSE_STATE_VERSION:
   case PARSE_STATE_PIPENAME:
   case PARSE_STATE_TICKET:
   case PARSE_STATE_TOKEN:
   case PARSE_STATE_CHALLENGE_EVENT:
   case PARSE_STATE_ALIAS:
   case PARSE_STATE_MAPPEDALIAS:
   case PARSE_STATE_USERHANDLEINFO:
      reply->parseState = PARSE_STATE_REPLY;
      break;
   case PARSE_STATE_USERNAME:
      if (PROTO_REPLY_QUERYMAPPEDALIASES == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_MAPPEDALIAS;
      } else {
         reply->parseState = PARSE_STATE_REPLY;
      }
      break;
   case PARSE_STATE_ALIASINFO:
      if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_ALIAS;
      } else if (PROTO_REPLY_VALIDATETICKET == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      } else if (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Bad parse state, popping aliasInfo in reply type %d",
                     reply->expectedReplyType);
      }
      break;
   case PARSE_STATE_SUBJECTS:
      reply->parseState = PARSE_STATE_MAPPEDALIAS;
      break;
   case PARSE_STATE_NAMEDSUBJECT:
   case PARSE_STATE_ANYSUBJECT:
      if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_ALIASINFO;
      } else if (PROTO_REPLY_QUERYMAPPEDALIASES == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_SUBJECTS;
      } else if (PROTO_REPLY_VALIDATETICKET == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_ALIASINFO;
      } else if (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_ALIASINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Bad parse state, popping subject in reply type %d",
                     reply->expectedReplyType);
      }
      break;
   case PARSE_STATE_COMMENT:
      reply->parseState = PARSE_STATE_ALIASINFO;
      break;
   case PARSE_STATE_PEMCERT:
      if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_ALIAS;
      } else if (PROTO_REPLY_QUERYMAPPEDALIASES == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_MAPPEDALIAS;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Bad parse state, popping pemCert in reply type %d",
                     reply->expectedReplyType);
      }
      break;
   case PARSE_STATE_CERTCOMMENT:
      reply->parseState = PARSE_STATE_REPLY;
      break;
   case PARSE_STATE_REPLY:
      reply->complete = TRUE;
      reply->parseState = PARSE_STATE_NONE;
      break;
   case PARSE_STATE_USERHANDLETYPE:
      reply->parseState = PARSE_STATE_USERHANDLEINFO;
      break;
   case PARSE_STATE_USERHANDLESAMLINFO:
      if (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN == reply->expectedReplyType) {
         reply->parseState = PARSE_STATE_REPLY;
      } else {
         reply->parseState = PARSE_STATE_USERHANDLEINFO;
      }
      break;
   case PARSE_STATE_USERHANDLESAMLSUBJECT:
      reply->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      break;
   default:
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Bad parse state, popping unknown parse state %d",
                  reply->parseState);
      ASSERT(0);
   }
}


/*
 ******************************************************************************
 * Proto_TextContents --                                                 */ /**
 *
 * Called by the parser with the contents of an element.
 * Used to store the values.
 *
 * @param[in]  parseContext        The XML parse context.
 * @param[in]  text                The contents of the current element
 *                                 (not NUL terminated)
 * @param[in]  textSize            The length of the text.
 * @param[in]  userData            The current ProtoReply as callback data.
 * @param[out] error               Any error.
 *
 ******************************************************************************
 */

static void
Proto_TextContents(GMarkupParseContext *parseContext,
                   const gchar *text,
                   gsize textSize,
                   gpointer userData,
                   GError **error)
{
   ProtoReply *reply = (ProtoReply *) userData;
   gchar *val;
   VGAuthUserHandleType t = VGAUTH_AUTH_TYPE_UNKNOWN;

#if VGAUTH_PROTO_TRACE
   Debug("%s: parseState %d, text '%*s'\n", __FUNCTION__, reply->parseState, (int) textSize, text);
#endif

   val = g_strndup(text, textSize);

   switch (reply->parseState) {
   case PARSE_STATE_SEQ:
      reply->sequenceNumber = atoi(val);
      g_free(val);
      break;

   case PARSE_STATE_ERROR_CODE:
      reply->errorCode = atoi(val);
      g_free(val);
      break;
   case PARSE_STATE_ERROR_MSG:
      reply->replyData.error.errorMsg = val;
      break;

   case PARSE_STATE_VERSION:
      reply->replyData.sessionReq.version = atoi(val);
      if (reply->expectedReplyType != PROTO_REPLY_SESSION_REQ) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found version number in reply type %d",
                     reply->expectedReplyType);
      }
      g_free(val);
      break;
   case PARSE_STATE_PIPENAME:
      if (reply->expectedReplyType != PROTO_REPLY_SESSION_REQ) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found pipeName in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      } else {
         reply->replyData.sessionReq.pipeName = val;
      }
      break;

   case PARSE_STATE_TICKET:
      if (reply->expectedReplyType != PROTO_REPLY_CREATETICKET) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found ticket in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      } else {
         reply->replyData.createTicket.ticket = val;
      }
      break;

   case PARSE_STATE_TOKEN:
      if (reply->expectedReplyType == PROTO_REPLY_VALIDATETICKET) {
         reply->replyData.validateTicket.token = val;
      } else if (reply->expectedReplyType ==
                 PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN) {
         reply->replyData.validateSamlBToken.token = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found token in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;

   case PARSE_STATE_CHALLENGE_EVENT:
      if (reply->expectedReplyType == PROTO_REPLY_CONN) {
         reply->replyData.connect.challengeEvent = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found token in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;

   case PARSE_STATE_USERNAME:
      if (reply->expectedReplyType == PROTO_REPLY_VALIDATETICKET) {
         reply->replyData.validateTicket.userName = val;
      } else if (reply->expectedReplyType ==
                 PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN) {
         reply->replyData.validateSamlBToken.userName = val;
      } else if (reply->expectedReplyType == PROTO_REPLY_QUERYMAPPEDALIASES) {
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].userName = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found username in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;

   case PARSE_STATE_PEMCERT:
      if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
         reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1].pemCert = val;
      } else if (reply->expectedReplyType == PROTO_REPLY_QUERYMAPPEDALIASES) {
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].pemCert = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found pemCert in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;
   case PARSE_STATE_CERTCOMMENT:
      if (reply->expectedReplyType == PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN) {
         reply->replyData.validateSamlBToken.comment = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found cert comment in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;

   case PARSE_STATE_REPLY:
   case PARSE_STATE_ALIAS:
   case PARSE_STATE_ALIASINFO:
   case PARSE_STATE_SUBJECTS:
   case PARSE_STATE_MAPPEDALIAS:
   case PARSE_STATE_USERHANDLEINFO:
   case PARSE_STATE_USERHANDLESAMLINFO:
      /*
       * Should just be whitespace, so drop it
       */
      g_free(val);
      break;
   case PARSE_STATE_USERHANDLESAMLSUBJECT:
      if (PROTO_REPLY_VALIDATETICKET == reply->expectedReplyType) {
         reply->replyData.validateTicket.samlSubject = val;
      } else if (PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN == reply->expectedReplyType) {
         reply->replyData.validateSamlBToken.samlSubject = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found SAMLSubject in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;
   case PARSE_STATE_USERHANDLETYPE:
      if (PROTO_REPLY_VALIDATETICKET == reply->expectedReplyType) {
         if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_NAMEPASSWORD) == 0) {
            t = VGAUTH_AUTH_TYPE_NAMEPASSWORD;
         } else if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_SSPI) == 0) {
            t = VGAUTH_AUTH_TYPE_SSPI;
         } else if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_SAML) == 0) {
            t = VGAUTH_AUTH_TYPE_SAML;
         } else if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_SAML_INFO_ONLY) == 0) {
            t = VGAUTH_AUTH_TYPE_SAML_INFO_ONLY;
         } else {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Found unrecognized userHandle type %s", val);
         }
         reply->replyData.validateTicket.type = t;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found UserHandleType in reply type %d",
                     reply->expectedReplyType);
      }
      g_free(val);
      break;
   case PARSE_STATE_NAMEDSUBJECT:
      if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
         VGAuthUserAlias *a;

         a = &(reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1]);
         a->infos[a->numInfos - 1].subject.val.name = val;
         a->infos[a->numInfos - 1].subject.type = VGAUTH_SUBJECT_NAMED;
      } else if (reply->expectedReplyType == PROTO_REPLY_QUERYMAPPEDALIASES) {
         int idx = reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].numSubjects;
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].subjects[idx - 1].val.name = val;
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].subjects[idx - 1].type = VGAUTH_SUBJECT_NAMED;
      } else if (reply->expectedReplyType == PROTO_REPLY_VALIDATETICKET) {
         reply->replyData.validateTicket.aliasInfo.subject.type = VGAUTH_SUBJECT_NAMED;
         reply->replyData.validateTicket.aliasInfo.subject.val.name = val;
      } else if (reply->expectedReplyType == PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN) {
         reply->replyData.validateSamlBToken.aliasInfo.subject.type = VGAUTH_SUBJECT_NAMED;
         reply->replyData.validateSamlBToken.aliasInfo.subject.val.name = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found NamedSubject in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;
   case PARSE_STATE_ANYSUBJECT:
      /*
       * Won't usually hit this code, since we use an empty-element tag.
       */
      if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
         VGAuthUserAlias *a;

         a = &(reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1]);
         a->infos[a->numInfos - 1].subject.type = VGAUTH_SUBJECT_ANY;
      } else if (reply->expectedReplyType == PROTO_REPLY_QUERYMAPPEDALIASES) {
         reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].subjects[reply->replyData.queryMappedAliases.maList[reply->replyData.queryMappedAliases.num - 1].numSubjects - 1].type = VGAUTH_SUBJECT_ANY;
      } else if (reply->expectedReplyType == PROTO_REPLY_VALIDATETICKET) {
         reply->replyData.validateTicket.aliasInfo.subject.type = VGAUTH_SUBJECT_ANY;
      } else if (reply->expectedReplyType == PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN) {
         reply->replyData.validateSamlBToken.aliasInfo.subject.type = VGAUTH_SUBJECT_ANY;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found AnySubject in reply type %d",
                     reply->expectedReplyType);
      }
      g_free(val);
      break;
   case PARSE_STATE_COMMENT:
      if (PROTO_REPLY_QUERYALIASES == reply->expectedReplyType) {
         VGAuthUserAlias *a;

         a = &(reply->replyData.queryUserAliases.uaList[reply->replyData.queryUserAliases.num - 1]);
         a->infos[a->numInfos - 1].comment = val;
      } else if (reply->expectedReplyType == PROTO_REPLY_VALIDATETICKET) {
         reply->replyData.validateTicket.aliasInfo.comment = val;
      } else if (reply->expectedReplyType == PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN) {
         reply->replyData.validateSamlBToken.aliasInfo.comment = val;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found comment in reply type %d",
                     reply->expectedReplyType);
         g_free(val);
      }
      break;
   default:
      g_warning("Unexpected value '%s' in unhandled parseState %d in %s\n",
                val, reply->parseState, __FUNCTION__);
      g_free(val);
      ASSERT(0);
   }
}


/*
 * Describes the parser functions.
 */
static GMarkupParser wireParser = {
   Proto_StartElement,
   Proto_EndElement,
   Proto_TextContents,
   NULL,
   NULL,
};


/*
 ******************************************************************************
 * Proto_NewReply --                                                     */ /**
 *
 * Creates a new ProtoReply
 *
 * @param[in]  expectedReplyType         The type of the new reply.
 *
 * @return The new ProtoReply *.
 *
 ******************************************************************************
 */

ProtoReply *
Proto_NewReply(ProtoReplyType expectedReplyType)
{
   ProtoReply *reply = g_malloc0(sizeof(ProtoReply));
   reply->parseState = PARSE_STATE_NONE;
   reply->complete = FALSE;
   reply->errorCode = VGAUTH_E_OK;
   reply->expectedReplyType = expectedReplyType;
   reply->actualReplyType = expectedReplyType;
#if VGAUTH_PROTO_TRACE
   reply->rawData = NULL;
#endif

   return reply;
}


/*
 ******************************************************************************
 * Proto_FreeReply --                                                    */ /**
 *
 * Frees a reply.
 *
 * @param[in]  reply         The reply to free.
 *
 ******************************************************************************
 */

static void
Proto_FreeReply(ProtoReply *reply)
{
   if (NULL == reply) {
      return;
   }

#if VGAUTH_PROTO_TRACE
   g_free(reply->rawData);
#endif
   switch (reply->actualReplyType) {
   case PROTO_REQUEST_UNKNOWN:
      // partial/empty request -- no-op
      Debug("%s: Freeing an request of unknown type.\n", __FUNCTION__);
      break;
   case PROTO_REPLY_ERROR:
      g_free(reply->replyData.error.errorMsg);
      break;
   case PROTO_REPLY_SESSION_REQ:
      g_free(reply->replyData.sessionReq.pipeName);
      break;
   case PROTO_REPLY_CONN:
      g_free(reply->replyData.connect.challengeEvent);
      break;
   case PROTO_REPLY_ADDALIAS:
   case PROTO_REPLY_REMOVEALIAS:
   case PROTO_REPLY_REVOKETICKET:
      break;
   case PROTO_REPLY_QUERYMAPPEDALIASES:
      VGAuth_FreeMappedAliasList(reply->replyData.queryMappedAliases.num,
                                 reply->replyData.queryMappedAliases.maList);
      break;
   case PROTO_REPLY_QUERYALIASES:
      VGAuth_FreeUserAliasList(reply->replyData.queryUserAliases.num,
                                reply->replyData.queryUserAliases.uaList);
      break;
   case PROTO_REPLY_CREATETICKET:
      g_free(reply->replyData.createTicket.ticket);
      break;
   case PROTO_REPLY_VALIDATETICKET:
      g_free(reply->replyData.validateTicket.userName);
      g_free(reply->replyData.validateTicket.token);
      g_free(reply->replyData.validateTicket.samlSubject);
      VGAuth_FreeAliasInfoContents(&(reply->replyData.validateTicket.aliasInfo));
      break;
   case PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN:
      g_free(reply->replyData.validateSamlBToken.comment);
      g_free(reply->replyData.validateSamlBToken.userName);
      g_free(reply->replyData.validateSamlBToken.token);
      g_free(reply->replyData.validateSamlBToken.samlSubject);
      VGAuth_FreeAliasInfoContents(&(reply->replyData.validateSamlBToken.aliasInfo));
      break;
   }
   g_free(reply);
}


/*
 ******************************************************************************
 * Proto_ConfidenceCheckReply --                                             */ /**
 *
 * Verifies a reply is internally consistent and the type is what we expected.
 *
 * @param[in]  reply                       The reply to check.
 * @param[in]  expectedSequenceNumber      The sequence number that
 *                                         should be in the reply.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
Proto_ConfidenceCheckReply(ProtoReply *reply,
                       int expectedSequenceNumber)
{
#if VGAUTH_PROTO_TRACE
   ASSERT(strncmp(reply->rawData, VGAUTH_XML_PREAMBLE,
                  strlen(VGAUTH_XML_PREAMBLE)) == 0);
#endif
   if (PROTO_REPLY_ERROR != reply->actualReplyType) {
      if (reply->actualReplyType != reply->expectedReplyType) {
         Warning("%s: expected reply type %d doesn't match actual type %d\n",
                 __FUNCTION__, reply->expectedReplyType, reply->actualReplyType);
         return VGAUTH_E_COMM;
      }
   }

   if (-1 != expectedSequenceNumber) {
      if (reply->sequenceNumber != expectedSequenceNumber) {
         Warning("%s: sequence number check failed:  wanted %d, got %d\n",
                 __FUNCTION__, expectedSequenceNumber, reply->sequenceNumber);
         return VGAUTH_E_COMM;
      }
   }

   /*
    * If it's an error, kick out now.
    */
   if (PROTO_REPLY_ERROR == reply->actualReplyType) {
      return VGAUTH_E_OK;
   }

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * VGAuth_ReadAndParseResponse --                                        */ /**
 *
 * Reads the next reply off the wire and returns it in wireReply.
 *
 * @param[in]  ctx                       The VGAuthContext.
 * @param[in]  expectedReplyType         The expected reply type.
 * @param[out] wireReply                 The complete reply.  The caller
 *                                       should use Proto_FreeReply on
 *                                       it when finished.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_ReadAndParseResponse(VGAuthContext *ctx,
                            ProtoReplyType expectedReplyType,
                            ProtoReply **wireReply)
{
   VGAuthError err = VGAUTH_E_OK;
   GMarkupParseContext *parseContext;
   gsize len;
   ProtoReply *reply;
   gboolean bRet;
   GError *gErr = NULL;

   reply = Proto_NewReply(expectedReplyType);

   parseContext = g_markup_parse_context_new(&wireParser,
                                             0,
                                             reply,
                                             NULL);

   /*
    * May take multiple reads if reply is broken up by the underlying
    * transport.
    */
   while (!reply->complete) {
      gchar *rawReply = NULL;

      err = VGAuth_CommReadData(ctx, &len, &rawReply);
      if (0 == len) {      // EOF -- not expected
         err = VGAUTH_E_COMM;
         Warning("%s: EOF on datastream when trying to parse\n", __FUNCTION__);
         goto quit;
      }
      if (VGAUTH_E_OK != err) {
         goto quit;
      }
#if VGAUTH_PROTO_TRACE
      if (reply->rawData) {
         reply->rawData = g_strdup_printf("%s%s", reply->rawData, rawReply);
      } else {
         reply->rawData = g_strdup(rawReply);
      }
#endif
      bRet = g_markup_parse_context_parse(parseContext,
                                          rawReply,
                                          len,
                                          &gErr);
      g_free(rawReply);
      if (!bRet) {
         /*
          * XXX Could drain the wire here, but since this should
          * never happen, just treat it as fatal for this socket.
          */
         err = VGAUTH_E_COMM;
         Warning("%s: g_markup_parse_context_parse() failed: %s\n",
                 __FUNCTION__, gErr->message);
         g_error_free(gErr);
         goto quit;
      }
      /*
       * XXX need some way to break out if packet never completed
       * yet socket left valid.  timer?
       */
   }

#if VGAUTH_PROTO_TRACE
   Proto_DumpReply(reply);
#endif

   err = Proto_ConfidenceCheckReply(reply, ctx->comm.sequenceNumber);

   if (VGAUTH_E_OK != err) {
      Warning("%s: reply confidence check failed\n", __FUNCTION__);
      goto quit;
   }

   if (PROTO_REPLY_ERROR == reply->actualReplyType) {
      Debug("%s: service sent back error "VGAUTHERR_FMT64X" (%s)\n",
            __FUNCTION__,
            reply->errorCode, reply->replyData.error.errorMsg);
      err = reply->errorCode;
   }
   goto done;

quit:
   Proto_FreeReply(reply);
   reply = NULL;
done:
   *wireReply = reply;
   g_markup_parse_context_free(parseContext);
   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendSessionRequest --                                          */ /**
 *
 * Sends the sessionRequest message and verifies the returning
 * reply.  The pipeName member of the ctx->comm is filled in.
 *
 * @param[in]  ctx                       The VGAuthContext.
 * @param[in]  userName                  The name of the user.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendSessionRequest(VGAuthContext *ctx,
                          const char *userName,
                          char **pipeName)                  // OUT
{
   VGAuthError err;
   gchar *packet;
   ProtoReply *reply = NULL;

   packet = g_markup_printf_escaped(VGAUTH_SESSION_REQUEST_FORMAT,
                                    ctx->comm.sequenceNumber,
                                    userName);

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to send packet\n", __FUNCTION__);
      goto quit;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_SESSION_REQ, &reply);
   if (VGAUTH_E_OK != err) {
      Warning("%s: read & parse reply failed\n", __FUNCTION__);
      goto quit;
   }

   /* version # check */
   if (reply->replyData.sessionReq.version != atoi(VGAUTH_PROTOCOL_VERSION)) {
      Warning("%s: version mismatch client is %d, service %d\n",
              __FUNCTION__, atoi(VGAUTH_PROTOCOL_VERSION),
              reply->replyData.sessionReq.version);
      /* XXX error out, or pretend?  */
   }

   *pipeName = g_strdup(reply->replyData.sessionReq.pipeName);

   ctx->comm.sequenceNumber++;

quit:
   Proto_FreeReply(reply);
   g_free(packet);
   return err;
}


/*
 ******************************************************************************
 * VGAuthErrorPipeClosed --                                              */ /**
 *
 * Check if the error code contains a system error that
 * the other end closed the pipe
 *
 * @param[in]  err       A VGAuthError code
 *
 * @return TRUE if the error code contains a system error that the other end
 *         closed the pipe. FALSE otherwise.
 *
 ******************************************************************************
 */

static gboolean
VGAuthErrorPipeClosed(VGAuthError err)
{
#ifdef _WIN32
   return VGAUTH_ERROR_EXTRA_ERROR(err) == ERROR_NO_DATA;
#else
   return VGAUTH_ERROR_EXTRA_ERROR(err) == EPIPE;
#endif
}


/*
 ******************************************************************************
 * VGAuth_SendConnectRequest --                                          */ /**
 *
 * Sends the connect message and verifies the returning reply.
 *
 * @param[in]  ctx                       The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendConnectRequest(VGAuthContext *ctx)
{
   VGAuthError err = VGAUTH_E_OK;
   VGAuthError err2;
   gchar *packet;
   ProtoReply *reply = NULL;
   char *pid = NULL;
#ifdef _WIN32
   unsigned int challengeEventValue;
   HANDLE hChallengeEvent;
   DWORD dwPid = GetCurrentProcessId();

   pid = Convert_UnsignedInt32ToText(dwPid);
#endif

   /* Value of pid is always NULL on non-Windows platforms */
   /* coverity[dead_error_line] */
   packet = g_markup_printf_escaped(VGAUTH_CONNECT_REQUEST_FORMAT,
                                    ctx->comm.sequenceNumber,
                                    pid ? pid : "");

   err = VGAuth_CommSendData(ctx, packet);
   /*
    * Bail out if failed.
    * However, continue to read the service response
    * if the service closed the pipe prematurely.
    */
   if (VGAUTH_FAILED(err) && !VGAuthErrorPipeClosed(err)) {
      VGAUTH_LOG_WARNING("failed to send packet, %s", packet);
      goto done;
   }

   err2 = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_CONN, &reply);
   if (VGAUTH_E_OK != err2) {
      VGAUTH_LOG_WARNING("read & parse reply failed, as user %s",
         ctx->comm.userName);
      err = err2;
      goto done;
   }

#ifdef _WIN32
   err = VGAUTH_E_FAIL;
   CHK_TEXT_TO_UINT32(challengeEventValue,
                      reply->replyData.connect.challengeEvent,
                      goto done);
   hChallengeEvent = (HANDLE)(size_t)challengeEventValue;
   if (!SetEvent(hChallengeEvent)) {
      VGAUTH_LOG_ERR_WIN("SetEvent() failed, pipe = %s", ctx->comm.pipeName);
      CloseHandle(hChallengeEvent);
      goto done;
   }
   CloseHandle(hChallengeEvent);
   err = VGAUTH_E_OK;
#endif

   ctx->comm.sequenceNumber++;

done:
   Proto_FreeReply(reply);
   g_free(packet);
   g_free(pid);
   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendAddAliasRequest --                                         */ /**
 *
 * Sends the AddAlias message and verifies the returning reply.
 *
 * @param[in]  ctx                       The VGAuthContext.
 * @param[in]  userName                  The user of the identity store
 *                                       being changed.
 * @param[in]  addMappedLink             If TRUE, adds an entry to the
 *                                       mapping file.
 * @param[in]  pemCert                   The certificate to add.
 * @param[in]  ai                        The associated AliasInfo.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendAddAliasRequest(VGAuthContext *ctx,
                           const char *userName,
                           gboolean addMappedLink,
                           const char *pemCert,
                           VGAuthAliasInfo *ai)
{
   VGAuthError err = VGAUTH_E_OK;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;
   gchar *aiPacket = NULL;

   if (!VGAuth_IsConnectedToServiceAsUser(ctx, userName)) {
      err = VGAuth_ConnectToServiceAsUser(ctx, userName);
      if (VGAUTH_E_OK != err) {
         goto quit;
      }
   }

   packet = g_markup_printf_escaped(VGAUTH_ADDALIAS_REQUEST_FORMAT_START,
                                    ctx->comm.sequenceNumber,
                                    userName,
                                    addMappedLink,
                                    pemCert);

   if (VGAUTH_SUBJECT_NAMED == ai->subject.type) {
      aiPacket = g_markup_printf_escaped(VGAUTH_NAMEDALIASINFO_FORMAT,
                                         ai->subject.val.name,
                                         ai->comment);
   } else {
      aiPacket = g_markup_printf_escaped(VGAUTH_ANYALIASINFO_FORMAT,
                                         ai->comment);
   }
   packet = Proto_ConcatXMLStrings(packet, aiPacket);
   packet = Proto_ConcatXMLStrings(packet,
                                   g_strdup(VGAUTH_ADDALIAS_REQUEST_FORMAT_END));

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to send packet\n", __FUNCTION__);
      goto quit;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_ADDALIAS, &reply);
   if (VGAUTH_E_OK != err) {
      Warning("%s: read & parse reply failed\n", __FUNCTION__);
      goto quit;
   }

   ctx->comm.sequenceNumber++;

quit:
   Proto_FreeReply(reply);
   g_free(packet);
   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendRemoveAliasRequest --                                      */ /**
 *
 * Sends the RemoveAlias message and verifies the returning reply.
 *
 * @param[in]  ctx          The VGAuthContext.
 * @param[in]  userName     The user of the identity store being changed.
 * @param[in]  pemCert      The certifcate to be removed, in PEM format.
 * @param[in]  subj         The subject to be removed (NULL if all).
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendRemoveAliasRequest(VGAuthContext *ctx,
                              const char *userName,
                              const char *pemCert,
                              VGAuthSubject *subj)
{
   VGAuthError err = VGAUTH_E_OK;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;
   gchar *sPacket;

   /*
    * Try connecting as user if we can, otherwise try root.
    * This allows for removing entries from deleted users.
    */
   if (UsercheckUserExists(userName)) {
      if (!VGAuth_IsConnectedToServiceAsUser(ctx, userName)) {
         err = VGAuth_ConnectToServiceAsUser(ctx, userName);
         if (VGAUTH_E_OK != err) {
            goto quit;
         }
      }
   } else {
      if (!VGAuth_IsConnectedToServiceAsUser(ctx, SUPERUSER_NAME)) {
         err = VGAuth_ConnectToServiceAsUser(ctx, SUPERUSER_NAME);
         if (VGAUTH_E_OK != err) {
            goto quit;
         }
      }
   }

   packet = g_markup_printf_escaped(VGAUTH_REMOVEALIAS_REQUEST_FORMAT_START,
                                    ctx->comm.sequenceNumber,
                                    userName,
                                    pemCert);

   if (subj) {
      if (VGAUTH_SUBJECT_NAMED == subj->type) {
         sPacket = g_markup_printf_escaped(VGAUTH_SUBJECT_FORMAT,
                                           subj->val.name);
      } else {
         sPacket = g_strdup(VGAUTH_ANYSUBJECT_FORMAT);
      }
      packet = Proto_ConcatXMLStrings(packet, sPacket);
   }
   packet = Proto_ConcatXMLStrings(packet,
                                   g_strdup(VGAUTH_REMOVEALIAS_REQUEST_FORMAT_END));

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to send packet\n", __FUNCTION__);
      goto quit;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_REMOVEALIAS, &reply);
   if (VGAUTH_E_OK != err) {
      Warning("%s: read & parse reply failed\n", __FUNCTION__);
      goto quit;
   }

   ctx->comm.sequenceNumber++;

quit:
   Proto_FreeReply(reply);
   g_free(packet);
   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendQueryUserAliasesRequest --                                 */ /**
 *
 * Sends the QueryAliases message and verifies the returning reply.
 *
 * @param[in]  ctx                 The VGAuthContext.
 * @param[in]  userName            The user of the identity store
 *                                 being queried.
 * @param[out] num                 The number of VGAuthUserAlias being returned.
 * @param[out] uaList              The resulting UserAliases.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendQueryUserAliasesRequest(VGAuthContext *ctx,
                                   const char *userName,
                                   int *num,
                                   VGAuthUserAlias **uaList)
{
   VGAuthError err = VGAUTH_E_OK;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;

   *uaList = NULL;
   *num = 0;

   /*
    * Try connecting as user if we can, otherwise try root.
    * This allows for querying certs for deleted users.
    */
   if (UsercheckUserExists(userName)) {
      if (!VGAuth_IsConnectedToServiceAsUser(ctx, userName)) {
         err = VGAuth_ConnectToServiceAsUser(ctx, userName);
         if (VGAUTH_E_OK != err) {
            goto quit;
         }
      }
   } else {
      if (!VGAuth_IsConnectedToServiceAsUser(ctx, SUPERUSER_NAME)) {
         err = VGAuth_ConnectToServiceAsUser(ctx, SUPERUSER_NAME);
         if (VGAUTH_E_OK != err) {
            goto quit;
         }
      }
   }

   packet = g_markup_printf_escaped(VGAUTH_QUERYALIASES_REQUEST_FORMAT,
                                    ctx->comm.sequenceNumber,
                                    userName);

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to send packet\n", __FUNCTION__);
      goto quit;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_QUERYALIASES, &reply);
   if (VGAUTH_E_OK != err) {
      Warning("%s: read & parse reply failed\n", __FUNCTION__);
      goto quit;
   }

   // just copy the reply data
   *num = reply->replyData.queryUserAliases.num;
   *uaList = reply->replyData.queryUserAliases.uaList;

   // clear out reply before free
   reply->replyData.queryUserAliases.num = 0;
   reply->replyData.queryUserAliases.uaList = NULL;

   ctx->comm.sequenceNumber++;

quit:
   Proto_FreeReply(reply);
   g_free(packet);
   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendQueryMappedAliasesRequest --                               */ /**
 *
 * Sends the QueryMappedAliases message and verifies the returning reply.
 *
 * @param[in]  ctx              The VGAuthContext.
 * @param[out] num              The number of identities.
 * @param[out] maList           The VGAuthMappedAliases being returned.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendQueryMappedAliasesRequest(VGAuthContext *ctx,
                                     int *num,
                                     VGAuthMappedAlias **maList)
{
   VGAuthError err = VGAUTH_E_OK;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;

   *num = 0;
   *maList = NULL;

   /*
    * QueryMappedCerts has no security restrictions, so we don't care
    * what user is used.
    */
   if (!VGAuth_IsConnectedToServiceAsAnyUser(ctx)) {
      err = VGAuth_ConnectToServiceAsCurrentUser(ctx);
      if (VGAUTH_E_OK != err) {
         goto quit;
      }
   }

   packet = g_markup_printf_escaped(VGAUTH_QUERYMAPPEDALIASES_REQUEST_FORMAT,
                                    ctx->comm.sequenceNumber);

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to send packet\n", __FUNCTION__);
      goto quit;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_QUERYMAPPEDALIASES, &reply);
   if (VGAUTH_E_OK != err) {
      Warning("%s: read & parse reply failed\n", __FUNCTION__);
      goto quit;
   }

   // just copy the reply data
   *num = reply->replyData.queryMappedAliases.num;
   *maList = reply->replyData.queryMappedAliases.maList;

   // clear out reply before free
   reply->replyData.queryMappedAliases.num = 0;
   reply->replyData.queryMappedAliases.maList = NULL;

   ctx->comm.sequenceNumber++;

quit:
   Proto_FreeReply(reply);
   g_free(packet);
   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendCreateTicketRequest --                                     */ /**
 *
 * Sends the CreateTicket message and verifies the returning reply.
 *
 * @param[in]  ctx                       The VGAuthContext.
 * @param[in]  userHandle                The VGAuthUserHandle.
 * @param[out] ticket                    The new ticket.
 *
 * @note   token is optional on Windows, ignored on other platforms
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendCreateTicketRequest(VGAuthContext *ctx,
                               VGAuthUserHandle *userHandle,
                               char **ticket)
{
   VGAuthError err = VGAUTH_E_OK;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;
   char *tokenInText = NULL;
   char *sPacket;
   VGAuthAliasInfo *ai;

   *ticket = NULL;

   if (!VGAuth_IsConnectedToServiceAsUser(ctx, userHandle->userName)) {
      err = VGAuth_ConnectToServiceAsUser(ctx, userHandle->userName);
      if (VGAUTH_E_OK != err) {
         goto quit;
      }
   }

#ifdef _WIN32
   ASSERT(Check_Is32bitNumber((size_t)userHandle->token));
   tokenInText =
      Convert_UnsignedInt32ToText((unsigned int)(size_t)userHandle->token);
#endif

   /* Value of tokenInText is always NULL on non-Windows platforms */
   /* coverity[dead_error_line] */
   packet = g_markup_printf_escaped(VGAUTH_CREATETICKET_REQUEST_FORMAT_START,
                                    ctx->comm.sequenceNumber,
                                    userHandle->userName,
                                    tokenInText ? tokenInText : "",
                                    ProtoUserHandleTypeString(userHandle));

   if (VGAUTH_AUTH_TYPE_SAML == userHandle->details.type) {
      sPacket = g_markup_printf_escaped(VGAUTH_USERHANDLESAMLINFO_FORMAT_START,
                                        userHandle->details.val.samlData.subject);
      packet = Proto_ConcatXMLStrings(packet, sPacket);

      ai = &(userHandle->details.val.samlData.aliasInfo);
      if (VGAUTH_SUBJECT_NAMED == ai->subject.type) {
         sPacket = g_markup_printf_escaped(VGAUTH_NAMEDALIASINFO_FORMAT,
                                           ai->subject.val.name,
                                           ai->comment);
      } else {
         sPacket = g_markup_printf_escaped(VGAUTH_ANYALIASINFO_FORMAT,
                                           ai->comment);
      }
      packet = Proto_ConcatXMLStrings(packet, sPacket);
      packet = Proto_ConcatXMLStrings(packet,
                                      g_strdup(VGAUTH_USERHANDLESAMLINFO_FORMAT_END));
   }
   packet = Proto_ConcatXMLStrings(packet,
                                   g_strdup(VGAUTH_CREATETICKET_REQUEST_FORMAT_END));

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to send packet\n", __FUNCTION__);
      goto quit;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_CREATETICKET, &reply);
   if (VGAUTH_E_OK != err) {
      Warning("%s: read & parse reply failed\n", __FUNCTION__);
      goto quit;
   }

   *ticket = g_strdup(reply->replyData.createTicket.ticket);

   ctx->comm.sequenceNumber++;

quit:
   Proto_FreeReply(reply);
   g_free(packet);
   g_free(tokenInText);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendValidateTicketRequest --                                   */ /**
 *
 * Sends the ValidateTicket message and verifies the returning reply.
 *
 * @param[in]  ctx                       The VGAuthContext.
 * @param[in]  ticket                    The ticket to validate.
 * @param[out] userHandle                The new VGAuthUserHandle based on
 *                                       the ticket.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendValidateTicketRequest(VGAuthContext *ctx,
                                 const char *ticket,
                                 VGAuthUserHandle **userHandle)
{
   VGAuthError err;
   VGAuthError retCode = VGAUTH_E_FAIL;
   VGAuthUserHandle *newHandle = NULL;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;
   HANDLE token = NULL;
#ifdef _WIN32
   unsigned int tokenValue;
#endif

   *userHandle = NULL;

   /*
    * Note that only root can validate a ticket.
    */
   if (!VGAuth_IsConnectedToServiceAsUser(ctx, SUPERUSER_NAME)) {
      err = VGAuth_ConnectToServiceAsUser(ctx, SUPERUSER_NAME);
      if (VGAUTH_E_OK != err) {
         retCode = err;
         goto done;
      }
   }

   packet = g_markup_printf_escaped(VGAUTH_VALIDATETICKET_REQUEST_FORMAT,
                                    ctx->comm.sequenceNumber,
                                    ticket);

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      retCode = err;
      VGAUTH_LOG_WARNING("%s", "VGAuth_CommSendData() failed");
      goto done;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_VALIDATETICKET, &reply);
   if (VGAUTH_E_OK != err) {
      retCode = err;
      VGAUTH_LOG_WARNING("%s", "VGAuth_ReadAndParseResponse() failed");
      goto done;
   }

#ifdef _WIN32
   CHK_TEXT_TO_UINT32(tokenValue, reply->replyData.validateTicket.token,
                      goto done);
   token = (HANDLE)(size_t)tokenValue;
#endif

   err = VGAuth_CreateHandleForUsername(ctx,
                                        reply->replyData.validateTicket.userName,
                                        reply->replyData.validateTicket.type,
                                        token, &newHandle);
   if (err != VGAUTH_E_OK) {
#ifdef _WIN32
      CloseHandle(token);
#endif
      goto done;
   }

   if (VGAUTH_AUTH_TYPE_SAML == reply->replyData.validateTicket.type) {
      err = VGAuth_SetUserHandleSamlInfo(ctx,
                                         newHandle,
                                         reply->replyData.validateTicket.samlSubject,
                                         &(reply->replyData.validateTicket.aliasInfo));
      if (err != VGAUTH_E_OK) {
#ifdef _WIN32
         CloseHandle(token);
#endif
         goto done;
      }

   }

   *userHandle = newHandle;

   ctx->comm.sequenceNumber++;

   retCode = VGAUTH_E_OK;

done:

   Proto_FreeReply(reply);
   g_free(packet);

   return retCode;
}


/*
 ******************************************************************************
 * VGAuth_SendRevokeTicketRequest --                                     */ /**
 *
 * Sends the RevokeTicket message.
 *
 * @param[in]  ctx                       The VGAuthContext.
 * @param[in]  ticket                    The ticket to revoke.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendRevokeTicketRequest(VGAuthContext *ctx,
                               const char *ticket)
{
   VGAuthError err;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;

   /*
    * Note that only root or the owner can revoke a ticket.
    *
    * If we're root, fine.  Otherwise, try to connect as current
    * user, which may also be root.
    */
   if (!VGAuth_IsConnectedToServiceAsUser(ctx, SUPERUSER_NAME)) {
      err = VGAuth_ConnectToServiceAsCurrentUser(ctx);
      if (VGAUTH_E_OK != err) {
         goto done;
      }
   }

   packet = g_markup_printf_escaped(VGAUTH_REVOKETICKET_REQUEST_FORMAT,
                                    ctx->comm.sequenceNumber,
                                    ticket);

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      VGAUTH_LOG_WARNING("%s", "VGAuth_CommSendData() failed");
      goto done;
   }

   err = VGAuth_ReadAndParseResponse(ctx, PROTO_REPLY_REVOKETICKET, &reply);
   if (VGAUTH_E_OK != err) {
      VGAUTH_LOG_WARNING("%s", "VGAuth_ReadAndParseResponse() failed");
      goto done;
   }

   ctx->comm.sequenceNumber++;

done:

   Proto_FreeReply(reply);
   g_free(packet);

   return err;
}


/*
 ******************************************************************************
 * VGAuth_SendValidateSamlBearerTokenRequest --                          */ /**
 *
 * Sends the ValidateSamlToken message and verifies the returning reply.
 *
 * @param[in]  ctx                       The VGAuthContext.
 * @param[in]  validateOnly              If set, only validation should
 *                                       occur, not access token creation.
 * @param[in]  samlToken                 The SAML token.
 * @param[in]  userName                  The user to authenticate as.
 * @param[out] userHandle                The resulting new userHandle.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_SendValidateSamlBearerTokenRequest(VGAuthContext *ctx,
                                          gboolean validateOnly,
                                          gboolean hostVerified,
                                          const char *samlToken,
                                          const char *userName,
                                          VGAuthUserHandle **userHandle)
{
   VGAuthError err = VGAUTH_E_OK;
   VGAuthUserHandle *newHandle = NULL;
   gchar *packet = NULL;
   ProtoReply *reply = NULL;
   HANDLE token = NULL;
#ifdef _WIN32
   unsigned int tokenValue;
#endif
   VGAuthUserHandleType hType;

   *userHandle = NULL;

   /*
    * ValidateSAMLBearerToken has no security restrictions, so we don't care
    * what user is used.
    */
   if (!VGAuth_IsConnectedToServiceAsAnyUser(ctx)) {
      err = VGAuth_ConnectToServiceAsCurrentUser(ctx);
      if (VGAUTH_E_OK != err) {
         goto quit;
      }
   }

   packet = g_markup_printf_escaped(VGAUTH_VALIDATESAMLBEARERTOKEN_REQUEST_FORMAT,
                                    ctx->comm.sequenceNumber,
                                    samlToken,
                                    userName ? userName : "",
                                    validateOnly ? "1" : "0",
                                    hostVerified ? "1" : "0");

   err = VGAuth_CommSendData(ctx, packet);
   if (VGAUTH_E_OK != err) {
      Warning("%s: failed to send packet\n", __FUNCTION__);
      goto quit;
   }

   err = VGAuth_ReadAndParseResponse(ctx,
                                     PROTO_REPLY_VALIDATE_SAML_BEARER_TOKEN,
                                     &reply);
   if (VGAUTH_E_OK != err) {
      Warning("%s: read & parse reply failed\n", __FUNCTION__);
      goto quit;
   }


   if (!validateOnly) {
      hType = VGAUTH_AUTH_TYPE_SAML;
#ifdef _WIN32
      CHK_TEXT_TO_UINT32(tokenValue, reply->replyData.validateSamlBToken.token,
                         goto quit);
      token = (HANDLE)(size_t)tokenValue;
#endif
   } else {
      hType = VGAUTH_AUTH_TYPE_SAML_INFO_ONLY;
   }

   err = VGAuth_CreateHandleForUsername(ctx,
                                        reply->replyData.validateSamlBToken.userName,
                                        hType,
                                        token, &newHandle);
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to create userHandle\n", __FUNCTION__);
      goto quit;
   }

   /*
    * Pull the rest of the userHandle info out of packet and add it
    * to userHandle
    */
   err = VGAuth_SetUserHandleSamlInfo(ctx,
                                      newHandle,
                                      reply->replyData.validateSamlBToken.samlSubject,
                                      &(reply->replyData.validateSamlBToken.aliasInfo));
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to set the SAML info on the userHandle\n", __FUNCTION__);
      goto quit;
   }

   *userHandle = newHandle;

   ctx->comm.sequenceNumber++;

quit:
   Proto_FreeReply(reply);
   g_free(packet);

   return err;
}
