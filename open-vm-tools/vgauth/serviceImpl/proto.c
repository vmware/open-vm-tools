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
 * @file proto.c --
 *
 *    Service/client protocol interfaces
 */

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#include "VGAuthLog.h"
#include "serviceInt.h"
#include "VGAuthProto.h"
#ifdef _WIN32
#include "winToken.h"
#include "winDupHandle.h"
#endif

/* cranks up parser debugging */
#define VGAUTH_PROTO_TRACE 0

/*
 * Request types
 */
typedef enum {
   PROTO_REQUEST_UNKNOWN,
   PROTO_REQUEST_SESSION_REQ,
   PROTO_REQUEST_CONN,
   PROTO_REQUEST_ADDALIAS,
   PROTO_REQUEST_REMOVEALIAS,
   PROTO_REQUEST_QUERYALIASES,
   PROTO_REQUEST_QUERYMAPPEDALIASES,
   PROTO_REQUEST_CREATETICKET,
   PROTO_REQUEST_VALIDATETICKET,
   PROTO_REQUEST_REVOKETICKET,
   PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN,
} ProtoRequestType;

/*
 * Possible parse states for requests.
 */
typedef enum {
   PARSE_STATE_NONE,
   PARSE_STATE_REQUEST,
   PARSE_STATE_SEQNO,
   PARSE_STATE_REQNAME,
   PARSE_STATE_VERSION,
   PARSE_STATE_USERNAME,
   PARSE_STATE_TICKET,
   PARSE_STATE_ADDLINK,
   PARSE_STATE_PEMCERT,
   PARSE_STATE_ALIASINFO,
   PARSE_STATE_COMMENT,
   PARSE_STATE_SUBJECT,
   PARSE_STATE_ANYSUBJECT,
   PARSE_STATE_PID,
   PARSE_STATE_TOKEN,
   PARSE_STATE_SAMLTOKEN,
   PARSE_STATE_USERHANDLEINFO,
   PARSE_STATE_USERHANDLETYPE,
   PARSE_STATE_USERHANDLESAMLINFO,
   PARSE_STATE_USERHANDLESAMLSUBJECT,
   PARSE_STATE_SAML_VALIDATE_ONLY,
} ProtoParseState;

/*
 * The request structure
 */

struct ProtoRequest {
   gboolean complete;
   int sequenceNumber;

   ProtoRequestType reqType;

   ProtoParseState parseState;

   union {
      struct {
         int version;
         gchar *userName;
      } sessionReq;

      struct {
         /*
          * The client PID that the client declares
          * This field is only used on Windows
          */
         gchar *pid;
      } connect;

      struct {
         gchar *userName;
         gboolean addMapped;
         gchar *pemCert;
         ServiceAliasInfo aliasInfo;
      } addAlias;

      struct {
         gchar *userName;
         gchar *pemCert;
         ServiceSubject subject;
      } removeAlias;

      struct {
         gchar *userName;
      } queryAliases;

      struct {
         gchar *userName;
         /* The received client token HANDLE */
         gchar *token;
         ServiceValidationResultsType type;
         // only used if the type is VALIDATION_RESULTS_TYPE_SAML
         ServiceValidationResultsData samlData;
      } createTicket;

      struct {
         gchar *ticket;
      } validateTicket;

      struct {
         gchar *ticket;
      } revokeTicket;

      struct {
         gchar *samlToken;
         gchar *userName;
         gboolean validateOnly;
      } validateSamlBToken;

   } reqData;

#if VGAUTH_PROTO_TRACE
   gchar *rawData;
#endif
};


static VGAuthError ServiceProtoValidateSamlBearerToken(ServiceConnection *conn,
                                                       ProtoRequest *req);


/*
 ******************************************************************************
 * ProtoRequestTypeText --                                               */ /**
 *
 * Return the text representation of the protocol request type.
 *
 * @param[in]  t        A protocol request type
 *
 ******************************************************************************
 */

static const char *
ProtoRequestTypeText(ProtoRequestType t)
{
   switch (t) {
   case PROTO_REQUEST_UNKNOWN:
      return "UNKNOWN";
   case PROTO_REQUEST_SESSION_REQ:
      return "SESSION";
   case PROTO_REQUEST_CONN:
      return "CONNECT";
   case PROTO_REQUEST_ADDALIAS:
      return "ADDALIAS";
   case PROTO_REQUEST_REMOVEALIAS:
      return "REMOVEALIAS";
   case PROTO_REQUEST_QUERYALIASES:
      return "QUERYALIASES";
   case PROTO_REQUEST_QUERYMAPPEDALIASES:
      return "QUERYMAPPEDALIASES";
   case PROTO_REQUEST_CREATETICKET:
      return "CREATETICKET";
   case PROTO_REQUEST_VALIDATETICKET:
      return "VALIDATETICKET";
   case PROTO_REQUEST_REVOKETICKET:
      return "REVOKETICKET";
   case PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN:
      return "VALIDATE_SAML_BEARER_TOKEN";
   default:
      return "INVALID";
   }
}


/*
 ******************************************************************************
 * ProtoValidationTypeString --                                          */ /**
 *
 * Returns the XML wire name of a ServiceValidationResultsType.
 *
 * @param[in]  userHandl         The VGAuthUSerHandle.
 *
 ******************************************************************************
 */

static const gchar *
ProtoValidationTypeString(const ServiceValidationResultsType t)
{
   switch (t) {
   case VALIDATION_RESULTS_TYPE_NAMEPASSWORD:
      return VGAUTH_USERHANDLE_TYPE_NAMEPASSWORD;
   case VALIDATION_RESULTS_TYPE_SSPI:
      return VGAUTH_USERHANDLE_TYPE_SSPI;
   case VALIDATION_RESULTS_TYPE_SAML:
      return VGAUTH_USERHANDLE_TYPE_SAML;
   case VALIDATION_RESULTS_TYPE_SAML_INFO_ONLY:
      return VGAUTH_USERHANDLE_TYPE_SAML_INFO_ONLY;
   case VALIDATION_RESULTS_TYPE_UNKNOWN:
   default:
      ASSERT(0);
      Warning("%s: Tried to convert a validationType of %d to a string\n",
              __FUNCTION__, t);
      return "<UNKNOWN>";
   }
}


/*
 ******************************************************************************
 * Proto_DumpRequest --                                                  */ /**
 *
 * Debugging.  Spews a ProtoRequest to stdout.
 *
 * @param[in]  req        The request to dump.
 *
 ******************************************************************************
 */

static void
Proto_DumpRequest(ProtoRequest *req)
{
#if VGAUTH_PROTO_TRACE
   printf("raw data: %s\n", req->rawData ? req->rawData : "<none>");
#endif
   Debug("complete: %d\n", req->complete);
   Debug("sequenceNumber: %d\n", req->sequenceNumber);
   Log("requestType: %d(%s REQ)\n", req->reqType,
       ProtoRequestTypeText(req->reqType));

   switch (req->reqType) {
   case PROTO_REQUEST_SESSION_REQ:
      Debug("version #: %d\n", req->reqData.sessionReq.version);
      Log("userName: '%s'\n", req->reqData.sessionReq.userName);
      break;
   case PROTO_REQUEST_CONN:
      // no details
      break;
   case PROTO_REQUEST_ADDALIAS:
      Log("userName: %s\n", req->reqData.addAlias.userName);
      Log("addMapped: %d\n", req->reqData.addAlias.addMapped);
      Debug("pemCert: %s\n", req->reqData.addAlias.pemCert);
      if (req->reqData.addAlias.aliasInfo.type == SUBJECT_TYPE_NAMED) {
         Log("Subject: %s\n", req->reqData.addAlias.aliasInfo.name);
      } else  if (req->reqData.addAlias.aliasInfo.type == SUBJECT_TYPE_ANY) {
         Log("ANY Subject\n");
      } else {
         Warning("*** UNKNOWN Subject type ***\n");
      }
      Log("comment: %s\n", req->reqData.addAlias.aliasInfo.comment);
      break;
   case PROTO_REQUEST_REMOVEALIAS:
      Log("userName: %s\n", req->reqData.removeAlias.userName);
      Debug("pemCert: %s\n", req->reqData.removeAlias.pemCert);
      if (req->reqData.removeAlias.subject.type == SUBJECT_TYPE_NAMED) {
         Log("Subject: %s\n", req->reqData.removeAlias.subject.name);
      } else  if (req->reqData.removeAlias.subject.type == SUBJECT_TYPE_ANY) {
         Log("ANY Subject\n");
      } else {
         Log("No Subject type specified (assuming removeAll case)\n");
      }
      break;
   case PROTO_REQUEST_QUERYALIASES:
      Log("userName: %s\n", req->reqData.queryAliases.userName);
      break;
   case PROTO_REQUEST_QUERYMAPPEDALIASES:
      // no details
      break;
   case PROTO_REQUEST_CREATETICKET:
      Log("userName '%s'\n", req->reqData.createTicket.userName);
      break;
   case PROTO_REQUEST_VALIDATETICKET:
      Log("ticket '%s'\n", req->reqData.validateTicket.ticket);
      break;
   case PROTO_REQUEST_REVOKETICKET:
      Log("ticket '%s'\n", req->reqData.revokeTicket.ticket);
      break;
   case PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN:
      Debug("token '%s'\n", req->reqData.validateSamlBToken.samlToken);
      Log("username '%s'\n", req->reqData.validateSamlBToken.userName);
      Log("validate Only '%s'\n",
            req->reqData.validateSamlBToken.validateOnly ? "TRUE" : "FALSE");
      break;
   default:
      Warning("Unknown request type -- no request specific data\n");
      break;
   }
}


/*
 ******************************************************************************
 * Proto_RequestNameToType --                                            */ /**
 *
 * Converts a request name to a ProtoRequestType
 *
 * @param[in]  name     The request name.
 *
 * @return the matching ProtoRequestType
 *
 ******************************************************************************
 */

typedef struct {
   ProtoRequestType type;
   const char *reqName;
} ReqName;

static const ReqName reqNameList[] = {
   { PROTO_REQUEST_SESSION_REQ, VGAUTH_REQUESTSESSION_ELEMENT_NAME },
   { PROTO_REQUEST_CONN, VGAUTH_REQUESTCONNECT_ELEMENT_NAME },
   { PROTO_REQUEST_ADDALIAS, VGAUTH_REQUESTADDALIAS_ELEMENT_NAME },
   { PROTO_REQUEST_REMOVEALIAS, VGAUTH_REQUESTREMOVEALIAS_ELEMENT_NAME },
   { PROTO_REQUEST_QUERYALIASES, VGAUTH_REQUESTQUERYALIASES_ELEMENT_NAME },
   { PROTO_REQUEST_QUERYMAPPEDALIASES, VGAUTH_REQUESTQUERYMAPPEDALIASES_ELEMENT_NAME },
   { PROTO_REQUEST_CREATETICKET, VGAUTH_REQUESTCREATETICKET_ELEMENT_NAME },
   { PROTO_REQUEST_VALIDATETICKET, VGAUTH_REQUESTVALIDATETICKET_ELEMENT_NAME },
   { PROTO_REQUEST_REVOKETICKET, VGAUTH_REQUESTREVOKETICKET_ELEMENT_NAME },
   { PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN,
     VGAUTH_REQUESTVALIDATESAMLBEARERTOKEN_ELEMENT_NAME },
};

static ProtoRequestType
Proto_RequestNameToType(const gchar *name)
{
   int i;

   for (i = 0; i < G_N_ELEMENTS(reqNameList); i++) {
      if (g_strcmp0(name, reqNameList[i].reqName) == 0) {
         return reqNameList[i].type;
      }
   }

   return PROTO_REQUEST_UNKNOWN;
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
 * @param[in]  userData            The current ProtoRequest as callback data.
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
   ProtoRequest *req = (ProtoRequest *) userData;

#if VGAUTH_PROTO_TRACE
   Debug("%s: elementName '%s', parseState %d, request type %d\n", __FUNCTION__, elementName, req->parseState, req->reqType);
#endif

   switch (req->parseState) {
   case PARSE_STATE_NONE:
      /*
       * We're in 'idle' mode, expecting a fresh request.
       */
      if (g_strcmp0(elementName, VGAUTH_REQUEST_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_REQUEST;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, req->parseState);
      }
      break;
   case PARSE_STATE_REQUEST:
      /*
       * We're in 'request' mode, expecting some element inside the request.
       */
      if (g_strcmp0(elementName, VGAUTH_REQUESTNAME_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_REQNAME;
      } else if (g_strcmp0(elementName, VGAUTH_SEQUENCENO_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_SEQNO;
      } else if (g_strcmp0(elementName, VGAUTH_USERNAME_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_USERNAME;
      } else if (g_strcmp0(elementName, VGAUTH_VERSION_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_VERSION;
      } else if (g_strcmp0(elementName, VGAUTH_TICKET_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_TICKET;
      } else if (g_strcmp0(elementName, VGAUTH_ADDMAPPEDLINK_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_ADDLINK;
      } else if (g_strcmp0(elementName, VGAUTH_PEMCERT_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_PEMCERT;
      } else if (g_strcmp0(elementName, VGAUTH_PID_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_PID;
      } else if (g_strcmp0(elementName, VGAUTH_TOKEN_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_TOKEN;
      } else if (g_strcmp0(elementName, VGAUTH_SAMLTOKEN_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_SAMLTOKEN;
      } else if (g_strcmp0(elementName, VGAUTH_VALIDATE_ONLY_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_SAML_VALIDATE_ONLY;
      } else if (g_strcmp0(elementName, VGAUTH_ALIASINFO_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_ALIASINFO;
      } else if (g_strcmp0(elementName, VGAUTH_SUBJECT_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_SUBJECT;
      } else if (g_strcmp0(elementName, VGAUTH_USERHANDLEINFO_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_USERHANDLEINFO;
      } else if (g_strcmp0(elementName, VGAUTH_ANYSUBJECT_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_ANYSUBJECT;
         /*
          * Proto_TextContents will never get called for an empty-element
          * tag, so set the value here.
          */
         req->parseState = PARSE_STATE_ANYSUBJECT;
         if (req->reqType != PROTO_REQUEST_REMOVEALIAS) {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Unexpected element '%s' in parse state %d",
                        elementName, req->parseState);
         } else {
            req->reqData.removeAlias.subject.type = SUBJECT_TYPE_ANY;
         }
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, req->parseState);
      }
      break;
   case PARSE_STATE_ALIASINFO:
      /*
       * We're parsing a AliasInfo, expecting one of its components.
       */
      if (g_strcmp0(elementName, VGAUTH_SUBJECT_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_SUBJECT;
      } else if (g_strcmp0(elementName, VGAUTH_ANYSUBJECT_ELEMENT_NAME) == 0) {
         /*
          * Proto_TextContents will never get called for an empty-element
          * tag, so set the value here.
          */
         req->parseState = PARSE_STATE_ANYSUBJECT;
         if (req->reqType == PROTO_REQUEST_ADDALIAS) {
            req->reqData.addAlias.aliasInfo.type = SUBJECT_TYPE_ANY;
         } else if (req->reqType == PROTO_REQUEST_CREATETICKET) {
            req->reqData.createTicket.samlData.aliasInfo.type = SUBJECT_TYPE_ANY;
         } else {
            g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                        "Unexpected element '%s' in parse state %d",
                        elementName, req->parseState);
         }
      } else if (g_strcmp0(elementName, VGAUTH_COMMENT_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_COMMENT;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, req->parseState);
      }
      break;
   case PARSE_STATE_USERHANDLEINFO:
      /*
       * We're parsing a UserHandleInfo, expecting one of its components.
       */
      if (g_strcmp0(elementName, VGAUTH_USERHANDLETYPE_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_USERHANDLETYPE;
      } else if (g_strcmp0(elementName, VGAUTH_USERHANDLESAMLINFO_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, req->parseState);
      }
      break;
   case PARSE_STATE_USERHANDLESAMLINFO:
      /*
       * We're parsing a UserHandleSamlInfo, expecting one of its components.
       */
      if (g_strcmp0(elementName, VGAUTH_USERHANDLESAMLSUBJECT_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_USERHANDLESAMLSUBJECT;
      } else if (g_strcmp0(elementName, VGAUTH_ALIASINFO_ELEMENT_NAME) == 0) {
         req->parseState = PARSE_STATE_ALIASINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Unexpected element '%s' in parse state %d",
                     elementName, req->parseState);
      }
      break;
   default:
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Unexpected element '%s' in parse state %d",
                  elementName, req->parseState);
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
 * @param[in]  elementName         The name of the element being finished.
 * @param[in]  userData            The current ProtoRequest as callback data.
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
   ProtoRequest *req = (ProtoRequest *) userData;

#if VGAUTH_PROTO_TRACE
   Debug("%s: elementName '%s'\n", __FUNCTION__, elementName);
#endif

   switch (req->parseState) {
   case PARSE_STATE_SEQNO:
   case PARSE_STATE_REQNAME:
   case PARSE_STATE_VERSION:
   case PARSE_STATE_USERNAME:
   case PARSE_STATE_ADDLINK:
   case PARSE_STATE_TICKET:
   case PARSE_STATE_PID:
   case PARSE_STATE_TOKEN:
   case PARSE_STATE_SAMLTOKEN:
   case PARSE_STATE_SAML_VALIDATE_ONLY:
   case PARSE_STATE_USERHANDLEINFO:
      req->parseState = PARSE_STATE_REQUEST;
      break;
   case PARSE_STATE_ALIASINFO:
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         req->parseState = PARSE_STATE_REQUEST;
      } else if (req->reqType == PROTO_REQUEST_CREATETICKET) {
         req->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Bad parse state, popping aliasInfo in req type %d",
                     req->reqType);
      }
      break;
   case PARSE_STATE_REQUEST:
      req->complete = TRUE;
      req->parseState = PARSE_STATE_NONE;
      break;
   case PARSE_STATE_PEMCERT:
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         req->parseState = PARSE_STATE_REQUEST;
      } else if (req->reqType == PROTO_REQUEST_REMOVEALIAS) {
         req->parseState = PARSE_STATE_REQUEST;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Bad parse state, popping pemCert in req type %d",
                     req->reqType);
      }
      break;
   case PARSE_STATE_COMMENT:
      req->parseState = PARSE_STATE_ALIASINFO;
      break;
   case PARSE_STATE_SUBJECT:
   case PARSE_STATE_ANYSUBJECT:
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         req->parseState = PARSE_STATE_ALIASINFO;
      } else if (req->reqType == PROTO_REQUEST_REMOVEALIAS) {
         req->parseState = PARSE_STATE_REQUEST;
      } else if (req->reqType == PROTO_REQUEST_CREATETICKET) {
         req->parseState = PARSE_STATE_ALIASINFO;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Bad parse state, popping (any)subject state %d",
                     req->parseState);
      }
      break;
   case PARSE_STATE_USERHANDLESAMLINFO:
   case PARSE_STATE_USERHANDLETYPE:
      req->parseState = PARSE_STATE_USERHANDLEINFO;
      break;
   case PARSE_STATE_USERHANDLESAMLSUBJECT:
      req->parseState = PARSE_STATE_USERHANDLESAMLINFO;
      break;
   default:
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Bad parse state, popping unknown parse state %d",
                  req->parseState);
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
 * @param[in]  userData            The current ProtoRequest as callback data.
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
   ProtoRequest *req = (ProtoRequest *) userData;
   gchar *val;
   int iVal;
   gboolean duplicate_found = FALSE;

#if VGAUTH_PROTO_TRACE
   Debug("%s: parseState %d, text '%*s'\n", __FUNCTION__, req->parseState, (int) textSize, text);
#endif

   /*
    * Simple string values should be set only once, but a malicious client
    * could send them multiple times, which could cause a leak if not
    * checked.
    */
#define SET_CHECK_DUP(var, val) \
   if ((var) != NULL) { duplicate_found = TRUE; goto done; } \
   else { (var) = (val); }

   val = g_strndup(text, textSize);

   switch (req->parseState) {
   case PARSE_STATE_SEQNO:
      req->sequenceNumber = atoi(val);
      break;
   case PARSE_STATE_REQNAME:
      if (req->reqType != PROTO_REQUEST_UNKNOWN) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Trying to handle new request of type %s when processing"
                     " a request of type %d",
                     val, req->reqType);
         goto done;
      }
      req->reqType = Proto_RequestNameToType(val);

      /*
       * Do any special init work once we've chosen the request type.
       */
      if (req->reqType == PROTO_REQUEST_REMOVEALIAS) {
         // init removeAlias to be UNSET, so that we handle the removeAll case
         req->reqData.removeAlias.subject.type = SUBJECT_TYPE_UNSET;
      }
      break;
   case PARSE_STATE_VERSION:
      if (req->reqType != PROTO_REQUEST_SESSION_REQ) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found version number in req type %d",
                     req->reqType);
         goto done;
      }
      req->reqData.sessionReq.version = atoi(val);
      break;
   case PARSE_STATE_USERNAME:
      switch (req->reqType) {
      case PROTO_REQUEST_SESSION_REQ:
         SET_CHECK_DUP(req->reqData.sessionReq.userName, val);
         break;
      case PROTO_REQUEST_ADDALIAS:
         SET_CHECK_DUP(req->reqData.addAlias.userName, val);
         break;
      case PROTO_REQUEST_REMOVEALIAS:
         SET_CHECK_DUP(req->reqData.removeAlias.userName, val);
         break;
      case PROTO_REQUEST_QUERYALIASES:
         SET_CHECK_DUP(req->reqData.queryAliases.userName, val);
         break;
      case PROTO_REQUEST_CREATETICKET:
         SET_CHECK_DUP(req->reqData.createTicket.userName, val);
         break;
      case PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN:
         SET_CHECK_DUP(req->reqData.validateSamlBToken.userName, val);
         break;
      default:
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found username in req type %d",
                     req->reqType);
         goto done;
      }
      val = NULL;
      break;
   case PARSE_STATE_TICKET:
      if (req->reqType == PROTO_REQUEST_VALIDATETICKET) {
         SET_CHECK_DUP(req->reqData.validateTicket.ticket, val);
      } else if (req->reqType == PROTO_REQUEST_REVOKETICKET) {
         SET_CHECK_DUP(req->reqData.revokeTicket.ticket, val);
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found ticket in req type %d",
                     req->reqType);
         goto done;
      }
      val = NULL;
      break;
   case PARSE_STATE_ADDLINK:
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         req->reqData.addAlias.addMapped = ((atoi(val) == 1) ? TRUE : FALSE);
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found addMappedLink in req type %d",
                     req->reqType);
         goto done;
      }
      break;
   case PARSE_STATE_REQUEST:
   case PARSE_STATE_ALIASINFO:
   case PARSE_STATE_USERHANDLEINFO:
   case PARSE_STATE_USERHANDLESAMLINFO:
      /*
       * Should just be whitespace, ignore
       */
      break;
   case PARSE_STATE_SUBJECT:
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         SET_CHECK_DUP(req->reqData.addAlias.aliasInfo.name, val);
         req->reqData.addAlias.aliasInfo.type = SUBJECT_TYPE_NAMED;
      } else if (req->reqType == PROTO_REQUEST_REMOVEALIAS) {
         SET_CHECK_DUP(req->reqData.removeAlias.subject.name, val);
         req->reqData.removeAlias.subject.type = SUBJECT_TYPE_NAMED;
      } else if (req->reqType == PROTO_REQUEST_CREATETICKET) {
         SET_CHECK_DUP(req->reqData.createTicket.samlData.aliasInfo.name, val);
         req->reqData.createTicket.samlData.aliasInfo.type = SUBJECT_TYPE_NAMED;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found SUBJECT in req type %d",
                     req->reqType);
         goto done;
      }
      val = NULL;
      break;
   case PARSE_STATE_ANYSUBJECT:
      /*
       * Don't expect to ever get here, but sombody may not use
       * an empty-element tag.
       */
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         req->reqData.addAlias.aliasInfo.type = SUBJECT_TYPE_ANY;
         req->reqData.addAlias.aliasInfo.name = NULL;
      } else if (req->reqType == PROTO_REQUEST_REMOVEALIAS) {
         req->reqData.removeAlias.subject.type = SUBJECT_TYPE_ANY;
         req->reqData.removeAlias.subject.name = NULL;
      } else if (req->reqType == PROTO_REQUEST_CREATETICKET) {
         req->reqData.createTicket.samlData.aliasInfo.type = SUBJECT_TYPE_ANY;
         req->reqData.createTicket.samlData.aliasInfo.name = NULL;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found ANYSUBJECT in req type %d",
                     req->reqType);
         goto done;
       }
      break;
   case PARSE_STATE_COMMENT:
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         SET_CHECK_DUP(req->reqData.addAlias.aliasInfo.comment, val);
      } else if (req->reqType == PROTO_REQUEST_CREATETICKET) {
         SET_CHECK_DUP(req->reqData.createTicket.samlData.aliasInfo.comment, val);
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found comment in req type %d",
                     req->reqType);
         goto done;
      }
      val = NULL;
      break;
   case PARSE_STATE_PEMCERT:
      if (req->reqType == PROTO_REQUEST_ADDALIAS) {
         SET_CHECK_DUP(req->reqData.addAlias.pemCert, val);
      } else if (req->reqType == PROTO_REQUEST_REMOVEALIAS) {
         SET_CHECK_DUP(req->reqData.removeAlias.pemCert, val);
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found pemCert in req type %d",
                     req->reqType);
         goto done;
      }
      val = NULL;
      break;
   case PARSE_STATE_PID:
      switch (req->reqType) {
      case PROTO_REQUEST_CONN:
         SET_CHECK_DUP(req->reqData.connect.pid, val);
         break;
      default:
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found pid in req type %d",
                     req->reqType);
         goto done;
      }
      val = NULL;
      break;
   case PARSE_STATE_TOKEN:
      switch (req->reqType) {
      case PROTO_REQUEST_CREATETICKET:
         SET_CHECK_DUP(req->reqData.createTicket.token, val);
         break;
      default:
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found token in req type %d",
                     req->reqType);
         goto done;
      }
      val = NULL;
      break;
   case PARSE_STATE_SAMLTOKEN:
      if (req->reqType != PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found SAML token in req type %d",
                     req->reqType);
         goto done;
      }
      SET_CHECK_DUP(req->reqData.validateSamlBToken.samlToken, val);
      val = NULL;
      break;
   case PARSE_STATE_SAML_VALIDATE_ONLY:

      if (req->reqType != PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found validateOnly option in req type %d",
                     req->reqType);
         goto done;
      }
      iVal = atoi(val);
      req->reqData.validateSamlBToken.validateOnly = (iVal) ? TRUE : FALSE;
      break;
   case PARSE_STATE_USERHANDLETYPE:
      {
      ServiceValidationResultsType t = VALIDATION_RESULTS_TYPE_UNKNOWN;

      if (req->reqType != PROTO_REQUEST_CREATETICKET) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found userHandle type in req type %d",
                     req->reqType);
         goto done;
      }
      if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_NAMEPASSWORD) == 0) {
         t = VALIDATION_RESULTS_TYPE_NAMEPASSWORD;
      } else if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_SSPI) == 0) {
         t = VALIDATION_RESULTS_TYPE_SSPI;
      } else if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_SAML) == 0) {
         t = VALIDATION_RESULTS_TYPE_SAML;
      } else if (g_strcmp0(val, VGAUTH_USERHANDLE_TYPE_SAML_INFO_ONLY) == 0) {
         t = VALIDATION_RESULTS_TYPE_SAML_INFO_ONLY;
      } else {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found unrecognized userHandle type %s", val);
         goto done;
      }
      req->reqData.createTicket.type = t;
      // let val be freed below
      }
      break;
   case PARSE_STATE_USERHANDLESAMLSUBJECT:
      if (req->reqType != PROTO_REQUEST_CREATETICKET) {
         g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                     "Found samlSubject in req type %d",
                     req->reqType);
         goto done;
      }
      SET_CHECK_DUP(req->reqData.createTicket.samlData.samlSubject, val);
      val = NULL;
      break;
   default:
      ASSERT(0);
      break;
   }
done:
   if (duplicate_found) {
      g_set_error(error, G_MARKUP_ERROR_PARSE, VGAUTH_E_INVALID_ARGUMENT,
                  "Unexpected contents '%s' in parse state %d",
                  val, req->parseState);
   }
   g_free(val);
#undef SET_CHECK_DUP
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
 * Proto_NewRequest --                                                   */ /**
 *
 * Creates a new ProtoRequest object.
 *
 * @return the new ProtoRequest *
 *
 ******************************************************************************
 */

ProtoRequest *
Proto_NewRequest(void)
{
   ProtoRequest *req = NULL;

   req = g_malloc0(sizeof(ProtoRequest));
   req->parseState = PARSE_STATE_NONE;
   req->complete = FALSE;
#if VGAUTH_PROTO_TRACE
   req->rawData = NULL;
#endif
   req->reqType = PROTO_REQUEST_UNKNOWN;

   return req;
}


/*
 ******************************************************************************
 * Proto_FreeRequest --                                                  */ /**
 *
 * Frees a req.
 *
 * @param[in]  req         The request to free.
 *
 ******************************************************************************
 */

static void
Proto_FreeRequest(ProtoRequest *req)
{
   if (NULL == req) {
      return;
   }

#if VGAUTH_PROTO_TRACE
   g_free(req->rawData);
#endif

   switch (req->reqType) {
   case PROTO_REQUEST_UNKNOWN:
      // partial/empty request -- no-op
      break;
   case PROTO_REQUEST_SESSION_REQ:
      g_free(req->reqData.sessionReq.userName);
      break;
   case PROTO_REQUEST_CONN:
      g_free(req->reqData.connect.pid);
      break;
   case PROTO_REQUEST_ADDALIAS:
      g_free(req->reqData.addAlias.userName);
      g_free(req->reqData.addAlias.pemCert);
      // will be NULL if ANY, so should be safe
      g_free(req->reqData.addAlias.aliasInfo.name);
      g_free(req->reqData.addAlias.aliasInfo.comment);
      break;
   case PROTO_REQUEST_REMOVEALIAS:
      g_free(req->reqData.removeAlias.userName);
      g_free(req->reqData.removeAlias.pemCert);
      // wll be NULL if ANY or unset, so should be safe
      g_free(req->reqData.removeAlias.subject.name);
      break;
   case PROTO_REQUEST_QUERYALIASES:
      g_free(req->reqData.queryAliases.userName);
      break;
   case PROTO_REQUEST_QUERYMAPPEDALIASES:
      //empty
      break;
   case PROTO_REQUEST_CREATETICKET:
      g_free(req->reqData.createTicket.userName);
      g_free(req->reqData.createTicket.token);
      ServiceAliasFreeAliasInfoContents(&(req->reqData.createTicket.samlData.aliasInfo));
      g_free(req->reqData.createTicket.samlData.samlSubject);
      break;
   case PROTO_REQUEST_VALIDATETICKET:
      g_free(req->reqData.validateTicket.ticket);
      break;
   case PROTO_REQUEST_REVOKETICKET:
      g_free(req->reqData.revokeTicket.ticket);
      break;
   case PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN:
      g_free(req->reqData.validateSamlBToken.samlToken);
      g_free(req->reqData.validateSamlBToken.userName);
      break;
   default:
      Warning("%s: trying to free unknown request type %d\n",
              __FUNCTION__, req->reqType);
   }

   g_free(req);
}


/*
 ******************************************************************************
 * Proto_SanityCheckRequest -                                            */ /**
 *
 * Verifies a request is internally consistent and the type is what we expected.
 *
 * @param[in]  request                 The request to check.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
Proto_SanityCheckRequest(ProtoRequest *request)
{
   /*
    * There's not much else to do here for now.  The parser does
    * most of the work, and we have no other rules.  The service doesn't
    * care about sequence numbers, or matching a request to a reply.
    */
#if VGAUTH_PROTO_TRACE
   ASSERT(strncmp(request->rawData, VGAUTH_XML_PREAMBLE,
                  strlen(VGAUTH_XML_PREAMBLE)) == 0);
#endif
   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * ServiceProtoReadAndProcessRequest --                                  */ /**
 *
 * Called when data is ready to be read from a client.  Reads that data,
 * parses it, and if it completes a request, process that request.
 *
 * @param[in]  conn                 The ServiceConnection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoReadAndProcessRequest(ServiceConnection *conn)
{
   ProtoRequest *req = conn->curRequest;
   VGAuthError err = VGAUTH_E_OK;
   gchar *data = NULL;
   gsize len;
   gboolean bRet;
   GError *gErr = NULL;

   /*
    * If nothing is currently being processed start a fresh one.
    */
   if (NULL == req) {
      req = Proto_NewRequest();
      conn->curRequest = req;

      conn->parseContext = g_markup_parse_context_new(&wireParser,
                                                      0,
                                                      req,
                                                      NULL);
   }

   /*
    * Suck some bytes, parse them.
    */
   if (!req->complete) {
      err = ServiceNetworkReadData(conn, &len, &data);

      if (conn->eof) { // EOF
         err = VGAUTH_E_COMM;
         Debug("%s: read EOF on Connection %d\n", __FUNCTION__, conn->connId);
         goto abort;
      }

      if (err != VGAUTH_E_OK) {
         goto abort;
      }
#if VGAUTH_PROTO_TRACE
      if (req->rawData) {
         req->rawData = g_strdup_printf("%s%s", req->rawData, data);
      } else {
         req->rawData = g_strdup(data);
      }
#endif
      bRet = g_markup_parse_context_parse(conn->parseContext,
                                          data,
                                          len,
                                          &gErr);
      g_free(data);
      if (!bRet) {
         err = VGAUTH_E_COMM;
         Warning("%s: g_markup_parse_context_parse() failed: %s\n",
                 __FUNCTION__, gErr->message);
         g_error_free(gErr);
         goto abort;
      }
   }

   /*
    * If the parser says we have a complete request, process it.
    */
   if (req->complete) {
      Proto_DumpRequest(req);
      err = Proto_SanityCheckRequest(req);
      if (err != VGAUTH_E_OK) {
         Warning("%s: request sanity check failed\n", __FUNCTION__);
      }

      // only try to handle it if the sanity check passed
      if (err == VGAUTH_E_OK) {
         err = ServiceProtoDispatchRequest(conn, req);
      }

      /*
       * Reset the protocol parser.
       */
      ServiceProtoCleanupParseState(conn);
   }

abort:
   /*
    * If something went wrong, clean up.  Any error means bad data coming
    * from the client, and we don't even try to recover -- just slam
    * the door.
    */
   if (err != VGAUTH_E_OK) {
      ServiceConnectionShutdown(conn);
   }

   return err;
}


/*
 ******************************************************************************
 * Proto_SecurityCheckRequest --                                         */ /**
 *
 * Verfies that superUser-only requests come over a superUser pipe,
 * and only superUser or the owner of a certstore can manipulate it.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The request to check.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
Proto_SecurityCheckRequest(ServiceConnection *conn,
                           ProtoRequest *req)
{
   VGAuthError err;
   gboolean isSecure = ServiceNetworkIsConnectionPrivateSuperUser(conn);

   switch (req->reqType) {
      /*
       * This comes over the public connection; alwsys let it through.
       */
   case PROTO_REQUEST_SESSION_REQ:
      err = VGAUTH_E_OK;
      break;
      /*
       * No security issues with Connect or QueryMappedCerts
       */
   case PROTO_REQUEST_CONN:
   case PROTO_REQUEST_QUERYMAPPEDALIASES:
      err = VGAUTH_E_OK;
      break;
      /*
       * These request can come over any user connection; always let
       * them through if they are coming from root or the owner of
       * the certstore being changed.
       */
   case PROTO_REQUEST_ADDALIAS:
   case PROTO_REQUEST_REMOVEALIAS:
   case PROTO_REQUEST_QUERYALIASES:
   case PROTO_REQUEST_CREATETICKET:
      if (isSecure) {
         err = VGAUTH_E_OK;
      } else {
         const gchar *connOwner = conn->userName;
         const gchar *reqUser = NULL;

         if (req->reqType == PROTO_REQUEST_ADDALIAS) {
            reqUser = req->reqData.addAlias.userName;
         } else if (req->reqType == PROTO_REQUEST_REMOVEALIAS) {
            reqUser = req->reqData.removeAlias.userName;
         } else if (req->reqType == PROTO_REQUEST_QUERYALIASES) {
            reqUser = req->reqData.queryAliases.userName;
         } else if (req->reqType == PROTO_REQUEST_CREATETICKET) {
            reqUser = req->reqData.createTicket.userName;
         } else {
            ASSERT(0);
         }

         if (Usercheck_CompareByName(connOwner, reqUser)) {
            err = VGAUTH_E_OK;
         } else {
            Audit_Event(FALSE,
                        SU_(proto.attack, "Possible security attack!  Request type %d has a "
                        "userName (%s) which doesn't match the pipe owner (%s)!"),
                        req->reqType, reqUser, connOwner);
            Warning("%s: Possible security attack!  Request type %d has a "
                    "userName (%s) which doesn't match the pipe owner (%s)!\n",
                    __FUNCTION__, req->reqType, reqUser, connOwner);
            err = VGAUTH_E_PERMISSION_DENIED;
         }
      }
      break;
      /*
       * These requests must come through a super-user owned private
       * connection.
       */
   case PROTO_REQUEST_VALIDATETICKET:
      err = (isSecure) ? VGAUTH_E_OK : VGAUTH_E_PERMISSION_DENIED;
      break;
   case PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN:
      /*
       * CAF wants to be able to validate as any user.
       */
      err = VGAUTH_E_OK;
      break;
   case PROTO_REQUEST_REVOKETICKET:
      /*
       * We want to allow just SUPERUSER and the ticket's owner to do the
       * Revoke.  But returning VGAUTH_E_PERMISSION_DENIED is also a hint
       * to an attacker that the ticket is valid.  So rather than
       * blow it off, we just ignore security at this layer,
       * and let the request fall through to ServiceRevokeTicket(),
       * which will turn a security issue into a no-op.
       */
      err = VGAUTH_E_OK;
      break;
   default:
      Warning("%s: Unrecognized request type '%d'\n",
              __FUNCTION__, req->reqType);
      err = VGAUTH_E_PERMISSION_DENIED;
      break;
   }

   return err;
}


/*
 ******************************************************************************
 * Proto_MakeErrorReply --                                               */ /**
 * ProtoMakeErrorReplyInt --                                             */ /**
 *
 * Generates an error reply string.  Must be g_free()d by caller.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   err           The error code.
 * @param[in]   errMsg        The error message.
 *
 * #return A new string containing the error message.
 *
 ******************************************************************************
 */

static gchar *
ProtoMakeErrorReplyInt(ServiceConnection *conn,
                       int reqSeqno,
                       VGAuthError err,
                       const char *errMsg)
{
   gchar *packet;
   gchar *escapedErrMsg = g_markup_escape_text(errMsg, -1);

   /*
    * g_markup_printf_escaped() is broken when the printf format
    * contains the Windows FMT64 format string %I64
    */

   packet = g_strdup_printf(VGAUTH_ERROR_FORMAT,
                            reqSeqno,
                            err,
                            escapedErrMsg);
   g_free(escapedErrMsg);

   Log("Returning error message '%s'\n", packet);

   return packet;
}


static gchar *
Proto_MakeErrorReply(ServiceConnection *conn,
                     ProtoRequest *req,
                     VGAuthError err,
                     const char *errMsg)
{
   return ProtoMakeErrorReplyInt(conn, req->sequenceNumber, err, errMsg);
}


/*
 ******************************************************************************
 * ServiceProtoDispatchRequest --                                        */ /**
 *
 * Dispatches and executes a request.  The function doing the processing will
 * generate any replies.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoDispatchRequest(ServiceConnection *conn,
                            ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;


   /*
    * Many requests must come across a superUser owned pipe.
    * Verify that here.
    */
   err = Proto_SecurityCheckRequest(conn, req);
   if (err != VGAUTH_E_OK) {
      Warning("%s: security check failed for request type %d\n",
              __FUNCTION__, req->reqType);
      packet = Proto_MakeErrorReply(conn,
                                    req,
                                    err,
                                    "Security check failed");
      goto sendError;
   }

#ifdef _WIN32
   /*
    * Check if we need to complete an earlier pid verification
    */
   if (conn->pidVerifyState == PID_VERIFY_PENDING) {
      err = ServiceEndVerifyPid(conn);
      if (err != VGAUTH_E_OK) {
         VGAUTH_LOG_WARNING("ServiceEndVerifyPid() failed, pipe = %s", conn->pipeName);
         packet = Proto_MakeErrorReply(conn,
                                       req,
                                       err,
                                       "Pid verification failed");
         goto sendError;
      }
   }

   /*
    * Check that we have the client proc handle to process the following
    * requests.
    */
   switch(req->reqType) {
   case PROTO_REQUEST_CREATETICKET:
   case PROTO_REQUEST_VALIDATETICKET:
   case PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN:
      if (conn->hProc == NULL) {
         VGAUTH_LOG_WARNING("Invalid client process HANDLE, possibly missing Connect, "
                            "pipe = %s", conn->pipeName);
         err = VGAUTH_E_FAIL;
         packet = Proto_MakeErrorReply(conn,
                                       req,
                                       err,
                                       "Client process handle check failed");
         goto sendError;
      }

      break;
   default:
      break;
   }
#endif

   switch (req->reqType) {
   case PROTO_REQUEST_SESSION_REQ:
      err = ServiceProtoHandleSessionRequest(conn, req);
      break;
   case PROTO_REQUEST_CONN:
      err = ServiceProtoHandleConnection(conn, req);
      break;
   case PROTO_REQUEST_ADDALIAS:
      err = ServiceProtoAddAlias(conn, req);
      break;
   case PROTO_REQUEST_REMOVEALIAS:
      err = ServiceProtoRemoveAlias(conn, req);
      break;
   case PROTO_REQUEST_QUERYALIASES:
      err = ServiceProtoQueryAliases(conn, req);
      break;
   case PROTO_REQUEST_QUERYMAPPEDALIASES:
      err = ServiceProtoQueryMappedAliases(conn, req);
      break;
   case PROTO_REQUEST_CREATETICKET:
      err = ServiceProtoCreateTicket(conn, req);
      break;
   case PROTO_REQUEST_VALIDATETICKET:
      err = ServiceProtoValidateTicket(conn, req);
      break;
   case PROTO_REQUEST_REVOKETICKET:
      err = ServiceProtoRevokeTicket(conn, req);
      break;
   case PROTO_REQUEST_VALIDATE_SAML_BEARER_TOKEN:
      err = ServiceProtoValidateSamlBearerToken(conn, req);
      break;
   default:
      /*
       * Be polite, send an error, and then fail cleanly
       */
      err = VGAUTH_E_NOTIMPLEMENTED;
      packet = Proto_MakeErrorReply(conn,
                                    req,
                                    err,
                                    "Unrecognized request");
sendError:
      /*
       * Don't really care if it works since we're about to
       * shut it down anyways.
       */
      (void) ServiceNetworkWriteData(conn, strlen(packet), packet);
      g_free(packet);
      break;
   }

   // 'err' is from ServiceNetworkWriteData(), not from the operation
   Log("%s: processed reqType %d(%s REQ), returning "
       VGAUTHERR_FMT64" on connection %d\n", __FUNCTION__,
       req->reqType, ProtoRequestTypeText(req->reqType), err, conn->connId);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoHandleSessionRequest --                                   */ /**
 *
 * Handles a SessionRequest request.  Creates a new listener pipe
 * for the incoming user, and replies to the caller.  Also does
 * any version negotiation.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The SessionRequest request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoHandleSessionRequest(ServiceConnection *conn,
                                 ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;
   gchar *pipeName = NULL;

   /*
    * Do any argument checking.  For now, the version number must
    * match.
    */

   if (req->reqData.sessionReq.version != atoi(VGAUTH_PROTOCOL_VERSION)) {
      err = VGAUTH_E_VERSION_MISMATCH;
      Warning("%s: version mismatch.  Client is %d, want %d\n",
              __FUNCTION__, req->reqData.sessionReq.version,
              atoi(VGAUTH_PROTOCOL_VERSION));
      packet = Proto_MakeErrorReply(conn, req, err,
                                    "sessionRequest failed; version mismatch");
      goto send_err;
   }

   err = ServiceStartUserConnection(req->reqData.sessionReq.userName,
                                    &pipeName);
   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "sessionRequest failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_SESSION_REPLY_FORMAT,
                                       req->sequenceNumber,
                                       pipeName);
   }

send_err:
   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to send SessionReq reply\n", __FUNCTION__);
   }

   g_free(pipeName);
   g_free(packet);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoHandleConnection --                                       */ /**
 *
 * Handles a Connect request -- just a simple reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The Connect request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoHandleConnection(ServiceConnection *conn,
                             ProtoRequest *req)
{
   VGAuthError err = VGAUTH_E_OK;
   VGAuthError err2;
   gchar *packet;
   char *event = NULL;

#ifdef _WIN32
   err = ServiceStartVerifyPid(conn, req->reqData.connect.pid, &event);
#endif

   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "connect failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_CONNECT_REPLY_FORMAT,
                                       req->sequenceNumber,
                                       event ? event : "");
   }

   err2 = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err2 != VGAUTH_E_OK) {
      Warning("%s: failed to send Connect reply\n", __FUNCTION__);
      if (err == VGAUTH_E_OK) {
         err = err2;
      }
   }
   g_free(packet);
   g_free(event);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoAddAlias --                                               */ /**
 *
 * Protocol layer for AddAlias.  Calls to alias code
 * to save the data, sends a reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The AddAlias request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoAddAlias(ServiceConnection *conn,
                     ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;

   /*
    * The alias code will do argument validation.
    */
   err = ServiceAliasAddAlias(conn->userName,
                              req->reqData.addAlias.userName,
                              req->reqData.addAlias.addMapped,
                              req->reqData.addAlias.pemCert,
                              &(req->reqData.addAlias.aliasInfo));

   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "addAlias failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_ADDALIAS_REPLY_FORMAT,
                                       req->sequenceNumber);
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to send AddSubject reply\n", __FUNCTION__);
   }

   g_free(packet);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoRemoveAlias --                                            */ /**
 *
 * Protocol layer for RemoveAlias.  Calls to alias code
 * to remove the cert, sends reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The RemoveAlias request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoRemoveAlias(ServiceConnection *conn,
                        ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;

   /*
    * The alias code will do argument validation.
    */
   err = ServiceAliasRemoveAlias(conn->userName,
                                 req->reqData.removeAlias.userName,
                                 req->reqData.removeAlias.pemCert,
                                 &(req->reqData.removeAlias.subject));

   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "removeAlias failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_REMOVEALIAS_REPLY_FORMAT,
                                       req->sequenceNumber);
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to send RemoveAlias reply\n", __FUNCTION__);
   }

   g_free(packet);

   return err;
}


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

   if (NULL == str2) {
      return str1;
   }
   newStr = g_strdup_printf("%s%s", str1, str2);
   g_free(str1);
   g_free(str2);

   return newStr;
}


/*
 ******************************************************************************
 * ServiceProtoQueryAliases --                                           */ /**
 *
 * Protocol layer for QueryAliases.  Calls to alias code
 * for the list of certs and associated aliasInfos, sends reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The QueryAliases request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoQueryAliases(ServiceConnection *conn,
                         ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;
   gchar *endPacket;
   int num;
   ServiceAlias *aList;
   int i;
   int j;

   /*
    * The alias code will do argument validation.
    */
   err = ServiceAliasQueryAliases(req->reqData.queryAliases.userName,
                                  &num,
                                  &aList);

   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "queryAliases failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_QUERYALIASES_REPLY_FORMAT_START,
                                       req->sequenceNumber);
      // now the aliases
      for (i = 0; i < num; i++) {
         gchar *certPacket;

         certPacket = g_markup_printf_escaped(VGAUTH_ALIAS_FORMAT_START,
                                              aList[i].pemCert);
         packet = Proto_ConcatXMLStrings(packet, certPacket);
         for (j = 0; j < aList[i].num; j++) {
            gchar *aiPacket;
            ServiceAliasInfo *ai = &(aList[i].infos[j]);

            if (ai->type == SUBJECT_TYPE_ANY) {
               aiPacket = g_markup_printf_escaped(VGAUTH_ANYALIASINFO_FORMAT,
                                                  ai->comment);
            } else if (ai->type == SUBJECT_TYPE_NAMED) {
               aiPacket = g_markup_printf_escaped(VGAUTH_NAMEDALIASINFO_FORMAT,
                                                  ai->name,
                                                  ai->comment);
            } else {
               aiPacket = NULL;
               ASSERT(0);
            }
            packet = Proto_ConcatXMLStrings(packet, aiPacket);
         }
         packet = Proto_ConcatXMLStrings(packet,
                                         g_markup_printf_escaped(VGAUTH_ALIAS_FORMAT_END));
      }

      // now the end of the reply
      endPacket = g_markup_printf_escaped(VGAUTH_QUERYALIASES_REPLY_FORMAT_END);
      packet = Proto_ConcatXMLStrings(packet, endPacket);


      ServiceAliasFreeAliasList(num, aList);
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to send QueryAliases reply\n", __FUNCTION__);
   }

   g_free(packet);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoQueryMappedAliases --                                     */ /**
 *
 * Protocol layer for QueryMappedAliases.  Calls to alias code
 * for the list of certs and subjects, sends reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The QueryMappedAliases request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoQueryMappedAliases(ServiceConnection *conn,
                               ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;
   gchar *endPacket;
   int num;
   int i;
   int j;
   ServiceMappedAlias *maList;

   /*
    * The alias code will do argument validation.
    */
   err = ServiceAliasQueryMappedAliases(&num,
                                        &maList);

   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "queryMappedIds failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_QUERYMAPPEDALIASES_REPLY_FORMAT_START,
                                       req->sequenceNumber);
      for (i = 0; i < num; i++) {
         gchar *tPacket;

         tPacket = g_markup_printf_escaped(VGAUTH_MAPPEDALIASES_FORMAT_START,
                                           maList[i].userName,
                                           maList[i].pemCert);
         packet = Proto_ConcatXMLStrings(packet, tPacket);
         for (j = 0; j < maList[i].num; j++) {
            if (maList[i].subjects[j].type == SUBJECT_TYPE_ANY) {
               tPacket = g_markup_printf_escaped(VGAUTH_ANYSUBJECT_FORMAT);
            } else if (maList[i].subjects[j].type == SUBJECT_TYPE_NAMED) {
               tPacket = g_markup_printf_escaped(VGAUTH_SUBJECT_FORMAT,
                                                 maList[i].subjects[j].name);
            } else {
               tPacket = NULL;
               ASSERT(0);
            }
            packet = Proto_ConcatXMLStrings(packet, tPacket);
         }
         packet = Proto_ConcatXMLStrings(packet,
                                         g_markup_printf_escaped(VGAUTH_MAPPEDALIASES_FORMAT_END));
      }

      // now the end of the reply
      endPacket = g_markup_printf_escaped(VGAUTH_QUERYMAPPEDALIASES_REPLY_FORMAT_END);
      packet = Proto_ConcatXMLStrings(packet, endPacket);

      ServiceAliasFreeMappedAliasList(num, maList);
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to send QueryAliases reply\n", __FUNCTION__);
   }

   g_free(packet);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoCreateTicket --                                           */ /**
 *
 * Protocol layer for CreateTicket.  Calls to ticket code
 * for the new ticket, sends reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The CreateTicket request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoCreateTicket(ServiceConnection *conn,
                         ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;
   char *ticket;

   /*
    * The ticket code will do argument validation.
    */
#ifdef _WIN32
   err = ServiceCreateTicketWin(req->reqData.createTicket.userName,
                                req->reqData.createTicket.type,
                                &(req->reqData.createTicket.samlData),
                                conn->hProc,
                                req->reqData.createTicket.token,
                                &ticket);
#else
   err = ServiceCreateTicketPosix(req->reqData.createTicket.userName,
                                  req->reqData.createTicket.type,
                                  &(req->reqData.createTicket.samlData),
                                  &ticket);
#endif

   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "createTicket failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_CREATETICKET_REPLY_FORMAT,
                                       req->sequenceNumber,
                                       ticket);
      g_free(ticket);
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      Warning("%s: failed to send CreateTicket reply\n", __FUNCTION__);
   }

   g_free(packet);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoValidateTicket --                                         */ /**
 *
 * Protocol layer for ValidateTicket.  Calls to ticket code
 * to validate the ticket, sends reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The ValidateTicket request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoValidateTicket(ServiceConnection *conn,
                           ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;
   gchar *sPacket;
   char *userName = NULL;
   char *token = NULL;
   ServiceValidationResultsType type;
   ServiceValidationResultsData *svd = NULL;

   /*
    * The ticket code will do argument validation.
    */
#ifdef _WIN32
   err = ServiceValidateTicketWin(req->reqData.validateTicket.ticket,
                                  conn->hProc,
                                  &userName,
                                  &type,
                                  &svd,
                                  &token);
#else
   err = ServiceValidateTicketPosix(req->reqData.validateTicket.ticket,
                                    &userName,
                                    &type,
                                    &svd);
#endif

   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "validateTicket failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_VALIDATETICKET_REPLY_FORMAT_START,
                                       req->sequenceNumber,
                                       userName,
                                       token ? token : "",
                                       ProtoValidationTypeString(type));
      if (VALIDATION_RESULTS_TYPE_SAML == type) {
         sPacket = g_markup_printf_escaped(VGAUTH_USERHANDLESAMLINFO_FORMAT_START,
                                           svd->samlSubject);
         packet = Proto_ConcatXMLStrings(packet, sPacket);
         if (SUBJECT_TYPE_NAMED == svd->aliasInfo.type) {
               sPacket = g_markup_printf_escaped(VGAUTH_NAMEDALIASINFO_FORMAT,
                                                  svd->aliasInfo.name,
                                                  svd->aliasInfo.comment);
         } else {
               sPacket = g_markup_printf_escaped(VGAUTH_ANYALIASINFO_FORMAT,
                                                  svd->aliasInfo.comment);
         }
         packet = Proto_ConcatXMLStrings(packet, sPacket);
         packet = Proto_ConcatXMLStrings(packet,
                                         g_strdup(VGAUTH_USERHANDLESAMLINFO_FORMAT_END));
      }
      packet = Proto_ConcatXMLStrings(packet, g_strdup(VGAUTH_VALIDATETICKET_REPLY_FORMAT_END));
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      VGAUTH_LOG_WARNING("ServiceNetWorkWriteData() failed, pipe = %s",
                         conn->pipeName);
      goto done;
   }

done:

   g_free(userName);
   g_free(token);
   g_free(packet);
   ServiceFreeValidationResultsData(svd);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoRevokeTicket --                                           */ /**
 *
 * Protocol layer for RevokeTicket.  Calls to ticket code
 * to revoke the ticket, sends reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The RevokeTicket request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceProtoRevokeTicket(ServiceConnection *conn,
                         ProtoRequest *req)
{
   VGAuthError err;
   gchar *packet;

   err = ServiceRevokeTicket(conn, req->reqData.revokeTicket.ticket);
   if (err != VGAUTH_E_OK) {
      packet = Proto_MakeErrorReply(conn, req, err, "revokeTicket failed");
   } else {
      packet = g_markup_printf_escaped(VGAUTH_REVOKETICKET_REPLY_FORMAT,
                                       req->sequenceNumber);
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      VGAUTH_LOG_WARNING("ServiceNetWorkWriteData() failed, pipe = %s",
                         conn->pipeName);
   }

   g_free(packet);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoValidateSamlBearerToken --                                */ /**
 *
 * Protocol layer for ValidateSamlBearerToken.  Calls to validate code
 * to validate the token, sends reply.
 *
 * @param[in]   conn          The ServiceConnection.
 * @param[in]   req           The ValidateSamlToken request to process.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
ServiceProtoValidateSamlBearerToken(ServiceConnection *conn,
                                    ProtoRequest *req)
{
   VGAuthError err = VGAUTH_E_FAIL;
   gchar *packet;
   gchar *sPacket;
   char *userName = NULL;
   char *subjectName = NULL;
   char *comment = NULL;
   char *tokenStr = NULL;
   ServiceAliasInfo *ai = NULL;

   /*
    * The validate code will do argument validation.
    */
   err = SAML_VerifyBearerTokenAndChain(req->reqData.validateSamlBToken.samlToken,
                                        req->reqData.validateSamlBToken.userName,
                                        &userName,
                                        &subjectName,
                                        &ai);
#ifdef _WIN32
   /*
    * Only create a token in the non-info-only mode
    */
   if ((err == VGAUTH_E_OK) &&
       !req->reqData.validateSamlBToken.validateOnly) {
      HANDLE userToken = NULL;

      err = WinToken_GenerateTokenForUser(userName, &userToken);
      if (err == VGAUTH_E_OK) {
         tokenStr = ServiceDupHandleTo(conn->hProc, userToken);
         if (!tokenStr) {
            VGAUTH_LOG_WARNING("ServiceDupHandleTo() failed, user = %s",
                               userName);
            err = VGAUTH_E_FAIL;
         } else {
            // close our copy after duping into client process
            CloseHandle(userToken);
         }
      } else {
         VGAUTH_LOG_WARNING("WinToken_GenerateTokenForUser() failed, user = %s",
                            userName);
      }
   } else {
      Debug("%s: skipping token creation\n", __FUNCTION__);
   }
#endif
   if (err != VGAUTH_E_OK) {
      Audit_Event(FALSE,
                  SU_(validate.samlBearer.fail,
                      "Validation of SAML bearer token failed: %d"),
                  (int) err);    // localization code can't deal with
                                 // differing types of uint64


      /*
       * Rewrite some errors to hide any data that could be useful to an
       * attacker.  Do this at this stage so that we still have
       * useful debug and possibly auditing reasons.
       */
      if (err ==  VGAUTH_E_INVALID_CERTIFICATE) {
         err = VGAUTH_E_AUTHENTICATION_DENIED;
      }
      packet = Proto_MakeErrorReply(conn, req, err,
                                    "validateSamlToken failed");
   } else {
      Audit_Event(FALSE,
                  SU_(validate.samlBearer.success,
                      "Validated SAML bearer token for user '%s'"),
                  userName);
      packet = g_markup_printf_escaped(VGAUTH_VALIDATESAMLBEARERTOKEN_REPLY_FORMAT_START,
                                       req->sequenceNumber,
                                       userName ? userName : "",
                                       tokenStr ? tokenStr : "",
                                       subjectName ? subjectName : "");

      if (SUBJECT_TYPE_NAMED == ai->type) {
            sPacket = g_markup_printf_escaped(VGAUTH_NAMEDALIASINFO_FORMAT,
                                               ai->name,
                                               ai->comment);
      } else {
            sPacket = g_markup_printf_escaped(VGAUTH_ANYALIASINFO_FORMAT,
                                              ai->comment);
      }
      packet = Proto_ConcatXMLStrings(packet, sPacket);
      packet = Proto_ConcatXMLStrings(packet,
                                      g_strdup(VGAUTH_VALIDATESAMLBEARERTOKEN_REPLY_FORMAT_END));
   }

   err = ServiceNetworkWriteData(conn, strlen(packet), packet);
   if (err != VGAUTH_E_OK) {
      VGAUTH_LOG_WARNING("ServiceNetWorkWriteData() failed, pipe = %s", conn->pipeName);
      goto done;
   }

done:
   g_free(userName);
   g_free(subjectName);
   g_free(packet);
   g_free(comment);
   g_free(tokenStr);
   ServiceAliasFreeAliasInfo(ai);

   return err;
}


/*
 ******************************************************************************
 * ServiceProtoCleanupParseState --                                      */ /**
 *
 * Resets the current parse state.
 *
 * @param[in]  conn                       The connection.
 *
 ******************************************************************************
 */

void
ServiceProtoCleanupParseState(ServiceConnection *conn)
{
   // g_markup_parse_context_free() whines if passed a NULL
   if (NULL != conn->parseContext) {
      g_markup_parse_context_free(conn->parseContext);
      conn->parseContext = NULL;
   }

   Proto_FreeRequest(conn->curRequest);
   conn->curRequest = NULL;
}


/*
 ******************************************************************************
 * ServiceReplyTooManyConnections --                                     */ /**
 *
 * Send the too many connection error message to the client
 *
 * @param[in]  conn                       The connection.
 * @param[in]  connLimit                  The concurrent connection limit
 *
 ******************************************************************************
 */

void
ServiceReplyTooManyConnections(ServiceConnection *conn,
                               int connLimit)
{
   gchar *packet =
      ProtoMakeErrorReplyInt(conn, 0,
                             VGAUTH_E_TOO_MANY_CONNECTIONS,
                            "The user exceeded its max number of connections");

   (void) ServiceNetworkWriteData(conn, strlen(packet), packet);

   VGAUTH_LOG_WARNING("User %s exceeding concurrent connection limit of "
                      "%d connections (connection ID is %d)",
                      conn->userName, connLimit, conn->connId);

   g_free(packet);
}
