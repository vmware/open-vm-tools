/*********************************************************
 * Copyright (C) 2016-2019 VMware, Inc. All rights reserved.
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
 * @file saml-xmlsec1.c
 *
 * Code for authenticating users based on SAML tokens.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#include <libxml/xmlschemas.h>

#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/templates.h>
#include <xmlsec/crypto.h>
#include <xmlsec/errors.h>

#include <glib.h>

#include "prefs.h"
#include "serviceInt.h"
#include "certverify.h"
#include "vmxlog.h"

static int gClockSkewAdjustment = VGAUTH_PREF_DEFAULT_CLOCK_SKEW_SECS;
static xmlSchemaPtr gParsedSchemas = NULL;
static xmlSchemaValidCtxtPtr gSchemaValidateCtx = NULL;

#define CATALOG_FILENAME            "catalog.xml"
#define SAML_SCHEMA_FILENAME        "saml-schema-assertion-2.0.xsd"

/*
 * Hack to test expired tokens and by-pass the time checks.
 *
 * Turning this on allows the VerifySAMLTokenFileTest() unit test
 * which reads a token from the file to be fed an old token (eg
 * from a log) and not have it fail because of the time-based
 * assertions.
 *
 * Note that setting this *will* cause negative tests looking for
 * time checks to fail.
 */
/* #define TEST_VERIFY_SIGN_ONLY 1 */

/*
 ******************************************************************************
 * XmlErrorHandler --                                                    */ /**
 *
 * Error handler for xml2.
 *
 * @param[in]  ctx           Context (unused).
 * @param[in]  msg           The error message in printf format.
 * @param[in]  ...           Any args for the msg.
 *
 ******************************************************************************
 */

static void
XmlErrorHandler(void *ctx,
                const char *msg,
                ...)
{
   gchar msgStr[1024];
   va_list argPtr;
   va_start(argPtr, msg);
   vsnprintf(msgStr, sizeof msgStr, msg, argPtr);
   va_end(argPtr);

   /*
    * Treat all as warning.
    */
   g_warning("XML Error: %s", msgStr);
   VMXLog_Log(VMXLOG_LEVEL_WARNING, "XML Error: %s", msgStr);
}


/*
 ******************************************************************************
 * XmlSecErrorHandler --                                                 */ /**
 *
 * Error handler for xmlsec.
 *
 * @param[in]  file          The name of the file generating the error.
 * @param[in]  line          The line number generating the error.
 * @param[in]  func          The function generating the error.
 * @param[in]  errorObject   The error specific object.
 * @param[in]  errorSubject  The error specific subject.
 * @param[in]  reason        The error code.
 * @param[in]  msg           The additional error message.
 *
 ******************************************************************************
 */

static void
XmlSecErrorHandler(const char *file,
                   int line,
                   const char *func,
                   const char *errorObject,
                   const char *errorSubject,
                   int reason,
                   const char *msg)
{
   /*
    * Treat all as warning.  */
   g_warning("XMLSec Error: %s:%s(line %d) object %s"
             " subject %s reason: %d, msg: %s",
             file, func, line,
             errorObject ? errorObject : "<UNSET>",
             errorSubject ? errorSubject : "<UNSET>",
             reason, msg);
   VMXLog_Log(VMXLOG_LEVEL_WARNING,
              "XMLSec Error: %s:%s(line %d) object %s"
              " subject %s reason: %d, msg: %s",
              file, func, line,
              errorObject ? errorObject : "<UNSET>",
              errorSubject ? errorSubject : "<UNSET>",
              reason, msg);
}


/*
 ******************************************************************************
 * LoadCatalogAndSchema --                                               */ /**
 *
 * Loads the schemas for validation.
 *
 * Using a catalog here ala xmllint.  Another option would be an
 * additional schema acting like a catalog.
 *
 * @param[in]   catPath       Path to the catalog file.
 * @param[in]   schemaPath    Path to the SAML schema file.
 *
 * return TRUE on success
 ******************************************************************************
 *
 */

static gboolean
LoadCatalogAndSchema(void)
{
   int ret;
   gboolean retVal = FALSE;
   xmlSchemaParserCtxtPtr ctx = NULL;
   gchar *catalogPath = NULL;
   gchar *schemaPath = NULL;
   gchar *schemaDir = NULL;

   schemaDir = Pref_GetString(gPrefs,
                              VGAUTH_PREF_SAML_SCHEMA_DIR,
                              VGAUTH_PREF_GROUP_NAME_SERVICE,
                              NULL);

   if (NULL == schemaDir) {
#ifdef _WIN32
      /*
       * To make life easier for the Windows installer, assume
       * the schema directory is next to the executable.  Also
       * check in ../ in case we're in a dev environment.
       */
      schemaDir = g_build_filename(gInstallDir, "schemas", NULL);
      if (!(g_file_test(schemaDir, G_FILE_TEST_EXISTS) &&
            g_file_test(schemaDir, G_FILE_TEST_IS_DIR))) {

         gchar *newDir = g_build_filename(gInstallDir, "..", "schemas", NULL);

         Debug("%s: schemas not found in Windows install loc '%s',"
               " trying dev location of '%s'\n", __FUNCTION__, schemaDir, newDir);

         g_free(schemaDir);
         schemaDir = newDir;
      }
#else
      /*
       * TODO -- clean this up to make a better default for Linux.
       */
      schemaDir = g_build_filename(gInstallDir, "..", "schemas", NULL);
#endif
   }
   Log("%s: Using '%s' for SAML schemas\n", __FUNCTION__, schemaDir);
   catalogPath = g_build_filename(schemaDir, CATALOG_FILENAME, NULL);
   schemaPath = g_build_filename(schemaDir, SAML_SCHEMA_FILENAME, NULL);

   xmlInitializeCatalog();

   /*
    * xmlLoadCatalog() just adds to the default catalog, and won't return an
    * error if it doesn't exist so long as a default catalog is set.
    *
    * So sanity check its existence.
    */
   if (!g_file_test(catalogPath, G_FILE_TEST_EXISTS)) {
      g_warning("Error: catalog file not found at \"%s\"\n", catalogPath);
      retVal = FALSE;
      goto done;
   }
   ret = xmlLoadCatalog(catalogPath);
   if (ret < 0) {
      g_warning("Error: Failed to load catalog at \"%s\"\n", catalogPath);
      retVal = FALSE;
      goto done;
   }

   ctx = xmlSchemaNewParserCtxt(schemaPath);
   if (NULL == ctx) {
      g_warning("Failed to create schema parser context\n");
      retVal = FALSE;
      goto done;
   }

   xmlSchemaSetParserErrors(ctx,
                            (xmlSchemaValidityErrorFunc) XmlErrorHandler,
                            (xmlSchemaValidityErrorFunc) XmlErrorHandler,
                            NULL);
   gParsedSchemas = xmlSchemaParse(ctx);
   if (NULL == gParsedSchemas) {
      /*
       * This shouldn't happen.  Means somebody mucked with our
       * schemas.
       */
      g_warning("Error: Failed to parse schemas\n");
      retVal = FALSE;
      goto done;
   }

   /*
    * Set up the validaton context for later use.
    */
   gSchemaValidateCtx = xmlSchemaNewValidCtxt(gParsedSchemas);
   if (NULL == gSchemaValidateCtx) {
      g_warning("Failed to create schema validation context\n");
      retVal = FALSE;
      goto done;
   }
   xmlSchemaSetValidErrors(gSchemaValidateCtx,
                           XmlErrorHandler,
                           XmlErrorHandler,
                           NULL);

   retVal = TRUE;
done:
   if (NULL != ctx) {
      xmlSchemaFreeParserCtxt(ctx);
   }
   g_free(catalogPath);
   g_free(schemaPath);
   g_free(schemaDir);

   return retVal;
}


/*
 ******************************************************************************
 * FreeSchemas --                                                        */ /**
 *
 * Frees global schema data.
 ******************************************************************************
 *
 */

static void
FreeSchemas(void)
{
   if (NULL != gSchemaValidateCtx) {
      xmlSchemaFreeValidCtxt(gSchemaValidateCtx);
      gSchemaValidateCtx = NULL;
   }
   if (NULL != gParsedSchemas) {
      xmlSchemaFree(gParsedSchemas);
      gParsedSchemas = NULL;
   }
}


/*
 ******************************************************************************
 * LoadPrefs --                                                          */ /**
 *
 * Loads any preferences SAML cares about.
 ******************************************************************************
 *
 */

static void
LoadPrefs(void)
{
   gClockSkewAdjustment = Pref_GetInt(gPrefs, VGAUTH_PREF_CLOCK_SKEW_SECS,
                                      VGAUTH_PREF_GROUP_NAME_SERVICE,
                                      VGAUTH_PREF_DEFAULT_CLOCK_SKEW_SECS);
    Log("%s: Allowing %d of clock skew for SAML date validation\n",
        __FUNCTION__, gClockSkewAdjustment);
}


/*
 ******************************************************************************
 * SAML_Init --                                                          */ /**
 *
 * Performs any initialization needed for SAML processing.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
SAML_Init(void)
{
   int ret;

   /*
    * Init the xml parser
    */
   xmlInitParser();

   /*
    * Verify the xml2 version -- if this is too old
    * its fatal, so we may want to use a different check.
    */
   LIBXML_TEST_VERSION

   /*
    * Tell libxml to do ID/REF lookups
    * Tell libxml to complete attributes with defaults from the DTDs
    */
   xmlLoadExtDtdDefaultValue = XML_DETECT_IDS | XML_COMPLETE_ATTRS;
   xmlSubstituteEntitiesDefault(1);


   /* set up the xml2 error handler */
   xmlSetGenericErrorFunc(NULL, XmlErrorHandler);

   /*
    * Load schemas
    */
   if (!LoadCatalogAndSchema()) {
      g_warning("Failed to load schemas\n");
      return VGAUTH_E_FAIL;
   }

   /* init xmlsec */
   ret = xmlSecInit();
   if (ret < 0) {
      g_warning("xmlSecInit() failed %d\n", ret);
      return VGAUTH_E_FAIL;
   }

   /*
    * set up the error callback
    */
   xmlSecErrorsSetCallback(XmlSecErrorHandler);

   /*
    * version check xmlsec1
    */
   if (xmlSecCheckVersion() != 1) {
      g_warning("Error: xmlsec1 lib version mismatch\n");
      return VGAUTH_E_FAIL;
   }

#ifdef XMLSEC_CRYPTO_DYNAMIC_LOADING
    /*
     * Load the openssl crypto engine if we are supporting dynamic
     * loading for xmlsec-crypto libraries.
     */
    if(xmlSecCryptoDLLoadLibrary("openssl") < 0) {
        g_warning("Error: unable to load openssl xmlsec-crypto library.\n "
                  "Make sure that you have xmlsec1-openssl installed and\n"
                  "check shared libraries path\n"
                  "(LD_LIBRARY_PATH) environment variable.\n");
        VMXLog_Log(VMXLOG_LEVEL_WARNING,
                   "Error: unable to load openssl xmlsec-crypto library.\n "
                   "Make sure that you have xmlsec1-openssl installed and\n"
                   "check shared libraries path\n"
                   "(LD_LIBRARY_PATH) environment variable.\n");
      return VGAUTH_E_FAIL;
    }
#endif /* XMLSEC_CRYPTO_DYNAMIC_LOADING */

   /*
    * init the xmlsec1 crypto app layer
    */
   ret = xmlSecCryptoAppInit(NULL);
   if (ret < 0) {
      g_warning("xmlSecCryptoAppInit() failed %d\n", ret);
      return VGAUTH_E_FAIL;
   }

   /*
    * Do crypto-engine specific initialization
    */
   ret = xmlSecCryptoInit();
   if (ret < 0) {
      g_warning("xmlSecCryptoInit() failed %d\n", ret);
      return VGAUTH_E_FAIL;
   }

   /*
    * Load prefs
    */
   LoadPrefs();

   Log("%s: Using xmlsec1 %d.%d.%d for XML signature support\n",
       __FUNCTION__, XMLSEC_VERSION_MAJOR, XMLSEC_VERSION_MINOR,
       XMLSEC_VERSION_SUBMINOR);
   VMXLog_Log(VMXLOG_LEVEL_WARNING,
              "%s: Using xmlsec1 %d.%d.%d for XML signature support\n",
              __FUNCTION__, XMLSEC_VERSION_MAJOR, XMLSEC_VERSION_MINOR,
              XMLSEC_VERSION_SUBMINOR);

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * SAML_Shutdown --                                                      */ /**
 *
 * Performs any clean-up of resources allocated by SAML code.
 *
 ******************************************************************************
 */

void
SAML_Shutdown()
{
   FreeSchemas();
   xmlSecCryptoShutdown();
   xmlSecCryptoAppShutdown();
   xmlSecShutdown();

#if 0
   /*
    * This is not thread safe:
    * http://0pointer.de/blog/projects/beware-of-xmlCleanupParser
    * and should only be called just before exit()
    * Because of this, our symbol-checker hates it:  See PR 407137
    */
   xmlCleanupParser();
#endif
}


/*
 ******************************************************************************
 * SAML_Reload --                                                        */ /**
 *
 * Reload any in-memory state used by the SAML module.
 *
 ******************************************************************************
 */

void
SAML_Reload()
{
   FreeSchemas();
   LoadPrefs();
   LoadCatalogAndSchema();
}


/*
 ******************************************************************************
 * FreeCertArray --                                                      */ /**
 *
 * Frees a simple array of pemCert.
 *
 * @param[in]  num      Number of certs in array.
 * @param[in]  certs    Array of certs to free.
 *
 ******************************************************************************
 */
static void
FreeCertArray(int num,
              gchar **certs)
{
   int i;

   for (i = 0; i < num; i++) {
      g_free(certs[i]);
   }
   g_free(certs);
}


/*
 ******************************************************************************
 * FindAttrValue --                                                      */ /**
 *
 * Returns the value of a attribute in an XML node.
 *
 * @param[in] node     XML subtree node.
 * @param[in] attrName Name of the attribute.
 *
 * @return Attribute value if exists.  The caller must free this with xmlFree().
 *
 ******************************************************************************
 */

static xmlChar *
FindAttrValue(const xmlNodePtr node,
              const gchar *attrName)
{
   xmlAttrPtr attr;
   xmlChar *name;

   /*
    * Find the attribute
    */
   attr = xmlHasProp(node, attrName);
   if ((attr == NULL) || (attr->children == NULL)) {
      return NULL;
   }

   /*
    * get the attribute value
    */
   name = xmlNodeListGetString(node->doc, attr->children, 1);

   return name;
}


/*
 ******************************************************************************
 * RegisterID --                                                         */ /**
 *
 * Register the document ID with the xml parser.
 *
 * This needs to be done if the document ID doesn't use the standard.
 * Otherwise the signing fails when setting up the reference.
 * SAML likes using 'ID' intead of the default 'xml:id', so
 * this is needed for both signing and verification.
 *
 * This is a no-op if the schemas have been loaded since they
 * set it up.
 *
 * See xmlsec1 FAQ 3.2
 *
 * Based on https://www.aleksey.com/pipermail/xmlsec/2003/001768.html
 *
 * @param[in]  node        The XML node on which to set the ID.
 * @param[in]  idName      The name of the ID.
 *
 * @return TRUE on success.
 ******************************************************************************
 */

static gboolean
RegisterID(xmlNodePtr node,
           const xmlChar *idName)
{
   xmlAttrPtr attr;
   xmlAttrPtr tmp;
   xmlChar *name;

   /*
    * find pointer to id attribute
    */
   attr = xmlHasProp(node, idName);
   if ((attr == NULL) || (attr->children == NULL)) {
      return FALSE;
   }

   /*
    * get the attribute (id) value
    */
   name = xmlNodeListGetString(node->doc, attr->children, 1);
   if (name == NULL) {
      return FALSE;
   }

   /*
    * check that we don't have the id already registered
    */
   tmp = xmlGetID(node->doc, name);
   if (tmp != NULL) {
      xmlFree(name);
      /* no-op if its already there */
      return TRUE;
   }

   /*
    * finally register id
    */
   xmlAddID(NULL, node->doc, name, attr);

   xmlFree(name);
   return TRUE;
}


/*
 ******************************************************************************
 * FindNodeByName --                                                     */ /**
 *
 * Searches under the specified node for one with a matching name.
 *
 * @param[in] root     XML subtree root under which to search.
 * @param[in] nodeName Name of node to find.
 *
 * @return matching xmlNodePtr or NULL.  Caller should not free this node.
 *
 ******************************************************************************
 */

static xmlNodePtr
FindNodeByName(xmlNodePtr root,
               char *nodeName)
{
   xmlNodePtr cur;

   cur = root->children;
   while (cur != NULL) {
      if (cur->type == XML_ELEMENT_NODE) {
         if (xmlStrEqual(nodeName, cur->name)) {
            break;
         }
      }
      cur = cur->next;
   }

   return cur;
}


/*
 ******************************************************************************
 * FindAllNodesByName --                                                 */ /**
 *
 * Searches under the specified node for all with a matching name.
 *
 * @param[in]  root     XML subtree root under which to search.
 * @param[in]  nodeName Name of node to find.
 * @param[out] nodeName Array of matches.
 *
 * @return Number of matching nodes.  Caller needs to free the array
 *         of Nodes, but not the nodes themselves.
 *
 ******************************************************************************
 */

static int
FindAllNodesByName(xmlNodePtr root,
                   char *nodeName,
                   xmlNodePtr **nodes)
{
   xmlNodePtr cur;
   xmlNodePtr *list = NULL;
   int count = 0;

   cur = root->children;
   while (cur != NULL) {
      if (cur->type == XML_ELEMENT_NODE) {
         if (xmlStrEqual(nodeName, cur->name)) {
            list = g_realloc_n(list,
                               sizeof(xmlNodePtr),
                               count + 1);
            list[count++] = cur;
         }
      }
      cur = cur->next;
   }
   *nodes = list;

   return count;
}


/*
 ******************************************************************************
 * ValidateDoc --                                                        */ /**
 *
 * Validates the XML document against the schema.
 *
 * @param[in]  doc         Parsed XML document.
 *
 ******************************************************************************
 */

static gboolean
ValidateDoc(xmlDocPtr doc)
{
   int ret;

   ret = xmlSchemaValidateDoc(gSchemaValidateCtx, doc);
   if (ret < 0) {
      g_warning("Failed to validate doc against schema\n");
   }

   return (ret == 0) ? TRUE : FALSE;
}


/*
 ******************************************************************************
 * CheckTimeAttr --                                                      */ /**
 *
 * Checks that the given attribute with the given name is a timestamp and
 * compares it against the current time.
 *
 * @param[in]  node         The node containing the attribute.
 * @param[in]  attrName     The name of the attribute.
 * @param[in]  notBefore    Whether the condition given by the attribute
 *                          should be in the past or 'now' (TRUE).
 *
 ******************************************************************************
 */

static gboolean
CheckTimeAttr(const xmlNodePtr node,
              const gchar *attrName,
              gboolean notBefore)
{
   xmlChar *timeAttr;
   GTimeVal attrTime;
   GTimeVal now;
   glong diff;
   gboolean retVal;

   timeAttr = FindAttrValue(node, attrName);
   if ((NULL == timeAttr) || (0 == *timeAttr)) {
      /*
       * The presence of all time restrictions in SAML are optional, so if
       * the attribute is not present, that is fine.
       */
      retVal = TRUE;
      goto done;
   }

   if (!g_time_val_from_iso8601(timeAttr, &attrTime)) {
      g_warning("%s: Could not parse %s value (%s).\n", __FUNCTION__, attrName,
                timeAttr);
      retVal = FALSE;
      goto done;
   }

   g_get_current_time(&now);

   /*
    * Check the difference, doing the math so that a positive
    * value is bad.  Ignore the micros field since precision
    * is unnecessary here because we see unsynced clocks in
    * the real world.
    */
   if (notBefore) {
      // expect time <= now
      diff = attrTime.tv_sec - now.tv_sec;
   } else {
      // expect now <= time
      diff = now.tv_sec - attrTime.tv_sec;
   }

   /*
    * A negative value is fine, a postive value
    * greater than the clock skew range is bad.
    */
   if (diff > gClockSkewAdjustment) {
      g_warning("%s: FAILED SAML assertion (timeStamp %s, delta %d) %s.\n",
                __FUNCTION__, timeAttr, (int) diff,
                notBefore ? "is not yet valid" : "has expired");
      VMXLog_Log(VMXLOG_LEVEL_WARNING,
                 "%s: FAILED SAML assertion (timeStamp %s, delta %d) %s.\n",
                __FUNCTION__, timeAttr, (int) diff,
                notBefore ? "is not yet valid" : "has expired");
      retVal = FALSE;
      goto done;
   }

   retVal = TRUE;

done:
   if (timeAttr) {
      xmlFree(timeAttr);
   }
   return retVal;
}


/*
 ******************************************************************************
 * CheckAudience --                                                      */ /**
 *
 * Checks whether the given audience URI refers to this machine.
 *
 * @param[in]  audience   An audience URI that a token is targetted for.
 *
 * @return TRUE if the audience URI refers to this machine, FALSE otherwise.
 *
 ******************************************************************************
 */

static gboolean
CheckAudience(const xmlChar *audience)
{
   gboolean ret;

   /*
    * Our SSO server doesn't set Recipient, so this only gets used by test code
    * whch uses a simple hostname check.
    *
    * Something like a VC UUID might be more accurate in a virtual
    * machine.
    */

   ret = strstr(audience, g_get_host_name()) != NULL;
   g_debug("%s: audience check: token: '%s', host: '%s' ? %d\n",
           __FUNCTION__,
           audience, g_get_host_name(), ret);
   return ret;
}


/*
 ******************************************************************************
 * VerifySubject --                                                      */ /**
 *
 * Extracts the name of the subject and enforces any conditions in
 * SubjectConfirmation elements.
 * Subjects are described in section 2.4 of the SAML Core specification.
 *
 * Example Subject XML:
 * <saml:Subject>
 *    <saml:NameID Format="urn:oasis:names:tc:SAML:1.1:nameid-format:emailAddress">
 *       scott@example.org
 *    </saml:NameID>
 *    <saml:SubjectConfirmation Method="urn:oasis:names:tc:SAML:2.0:cm:bearer">
 *       <saml:SubjectConfirmationData NotOnOrAfter="2011-12-08T00:42:10Z">
 *       </saml:SubjectConfirmationData>
 *    </saml:SubjectConfirmation>
 * </saml:Subject>
 *
 * @param[in]     doc         The parsed SAML token.
 * @param[out]    subjectRet  The Subject NameId.  Should be g_free()d by
 *                            caller.
 *
 * @return TRUE if the conditions in at least one SubjectConfirmation is met,
 *         FALSE otherwise.
 *
 ******************************************************************************
 */

static gboolean
VerifySubject(xmlDocPtr doc,
              gchar **subjectRet)
{
   xmlNodePtr subjNode;
   xmlNodePtr nameIDNode;
   xmlNodePtr child;
   gchar *subjectVal = NULL;
   gboolean retCode = FALSE;
   gboolean validSubjectFound = FALSE;
   xmlChar *tmp;

   if (NULL != subjectRet) {
      *subjectRet = NULL;
   }

   subjNode = FindNodeByName(xmlDocGetRootElement(doc), "Subject");
   if (NULL == subjNode) {
      g_warning("No Subject node found\n");
      goto done;
   }

   /*
    * Pull out the NameID for later checks elsewhere.
    */
   nameIDNode = FindNodeByName(subjNode, "NameID");
   if (NULL == nameIDNode) {
      g_warning("%s: NameID not found in Subject\n", __FUNCTION__);
      goto done;
   }
   tmp = xmlNodeGetContent(nameIDNode);
   subjectVal = g_strdup(tmp);
   xmlFree(tmp);

   /*
    * Find all the SubjectConfirmation nodes and see if at least one
    * can be validated.
    */
   for (child = subjNode->children; child != NULL; child = child->next) {
      xmlChar *method;
      xmlNodePtr subjConfirmData;

      if (child->type == XML_ELEMENT_NODE) {
         if (!xmlStrEqual(child->name, "SubjectConfirmation")) {
            continue;
         }
         method = FindAttrValue(child, "Method");
         if ((NULL == method) || (0 == *method)) {
            // should not happen since this is required
            g_warning("%s: Missing SubjectConfirmation method\n", __FUNCTION__);
            xmlFree(method);
            goto done;
         }
         if (!xmlStrEqual(method, "urn:oasis:names:tc:SAML:2.0:cm:bearer")) {
            g_warning("%s: method %s not bearer\n", __FUNCTION__, method);
            xmlFree(method);
            continue;
         }
         xmlFree(method);

         subjConfirmData = FindNodeByName(child, "SubjectConfirmationData");
         if (NULL != subjConfirmData) {
            xmlChar *recipient;

            if (!CheckTimeAttr(subjConfirmData, "NotBefore", TRUE) ||
                !CheckTimeAttr(subjConfirmData, "NotOnOrAfter", FALSE)) {
               g_warning("%s: subjConfirmData time check failed\n",
                         __FUNCTION__);
               continue;
            }

            /*
             * Recipient isn't always there.
             */
            recipient = FindAttrValue(subjConfirmData, "Recipient");
            if ((NULL != recipient) && (0 != *recipient) &&
                !CheckAudience(recipient)) {
               g_debug("%s: failed recipient check\n", __FUNCTION__);
               xmlFree(recipient);
               continue;
            }
            xmlFree(recipient);
         }

         /*
          * passed all the checks, we have a match so kick out
          */
         validSubjectFound = TRUE;
         break;
      }
   }

   if (validSubjectFound && (NULL != subjectRet)) {
      *subjectRet = subjectVal;
   } else {
      g_free(subjectVal);
   }
   retCode = validSubjectFound;
done:
   return retCode;
}


/*
 ******************************************************************************
 * VerifyConditions --                                                   */ /**
 *
 * Enforces conditions specified by the "saml:Conditions" element
 * under the root element.
 * Conditions are described in section 2.5 of the SAML Core specification.
 *
 * Example Conditions XML:
 *    <saml:Conditions NotBefore="2011-12-08T00:41:10Z"
 *     NotOnOrAfter="2011-12-08T00:42:10Z">
 *       <saml:AudienceRestriction>
 *          <saml:Audience>https://sp.example.com/SAML2</saml:Audience>
 *       </saml:AudienceRestriction>
 *    </saml:Conditions>
 *
 * @param[in]  doc  The parsed SAML token.
 *
 * @return TRUE if the conditions are met; FALSE otherwise.
 *
 ******************************************************************************
 */

static gboolean
VerifyConditions(xmlDocPtr doc)
{
   xmlNodePtr condNode;

   /*
    * There should be at most one Conditions element and the schema checking
    * done by the parser should enforce that.
    */
   condNode = FindNodeByName(xmlDocGetRootElement(doc), "Conditions");
   if (NULL == condNode) {
      // Conditions are optional.
      g_debug("%s: No Conditions found, accepting\n", __FUNCTION__);
      return TRUE;
   }

   if (!CheckTimeAttr(condNode, "NotBefore", TRUE) ||
       !CheckTimeAttr(condNode, "NotOnOrAfter", FALSE)) {
      g_warning("%s: Time Conditions failed!\n", __FUNCTION__);
      return FALSE;
   }

   /*
    * <Condition> is a generic element, intended as an extension point.
    * We don't know about any. According to the general processng rules, if
    * we find a condition we don't know about, the result of the validation
    * is "indeterminate" and we should reject the assertion.
    */
   if (FindNodeByName(condNode, "Condition") != NULL) {
      g_warning("%s: Unrecognized condition found!\n", __FUNCTION__);
      return FALSE;
   }

   /*
    * <AudienceRestriction> defines a set a URIs that describe what
    * audience the assertioned is addressed to or intended for.
    * But it's very generic. From the spec (section 2.5.1.4):
    *    A URI reference that identifies an intended audience. The URI
    *    reference MAY identify a document that describes the terms and
    *    conditions of audience membership. It MAY also contain the unique
    *    identifier URI from a SAML name identifier that describes a system
    *    entity.
    *
    * Our SSO server doesn't set it, so no point in checking it.
    */

#if 0
   // TODO nothing looks at this
   /*
    * <OneTimeUse> element is specified to disallow caching. We don't
    * cache, so it doesn't affect our validation.
    * However, we need to communicate it to clients so they do not cache.
    */
   oneTimeUse = (FindChildByName(condNode, "OneTimeUse")
                       != NULL);
#endif

   /*
    * <ProxyRestriction> only applies if a service wants to make their own
    * assertions based on a SAML assertion. That should not apply here.
    */

   return TRUE;
}


/*
 ******************************************************************************
 * BuildCertChain --                                                     */ /**
 *
 * Pulls the certs out of the parsed SAML token, adds them to the
 * key manager, and returns them as a list.
 *
 * @param[in]  x509Node     x509 data node.
 * @param[in]  mgr          KeyManager
 * @param[out] numCerts     Number of certs being returned.
 * @param[out] certChain    Array containing the certs in OpenSSL PEM
 *                          format.  Array and contents must be g_free()d
 *                          by caller.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

static gboolean
BuildCertChain(xmlNodePtr x509Node,
               xmlSecKeysMngrPtr mgr,
               int *numCerts,
               gchar ***certChain)
{
   gboolean bRet = FALSE;
   xmlNodePtr *x509CertNodes = NULL;
   int num;
   int i;
   int ret;
   gchar **certList = NULL;

   num = FindAllNodesByName(x509Node,
                            (char *) xmlSecNodeX509Certificate,
                            &x509CertNodes);
   if (num == 0) {
      g_warning("Missing x509 certificate node(s)\n");
      goto done;
   }

   certList = g_malloc0_n(num + 1, sizeof(gchar *));

   for (i = 0; i < num; i++) {
      gchar *pemCert = NULL;
      xmlChar *base64Cert;

      base64Cert = xmlNodeGetContent(x509CertNodes[i]);
      if (NULL == base64Cert) {
         g_warning("Missing x509 certificate base64 data\n");
         goto done;
      }

      /*
       * Turn the raw base64 into PEM.  Thanks for being so anal,
       * OpenSSL.
       */
      pemCert = CertVerify_EncodePEMForSSL(base64Cert);
      xmlFree(base64Cert);

      /*
       * Add cert to the keymanager.
       */
      ret = xmlSecCryptoAppKeysMngrCertLoadMemory(mgr,
                                                  pemCert,
                                                  strlen(pemCert),
                                                  xmlSecKeyDataFormatPem,
                                                  xmlSecKeyDataTypeTrusted);
      if (ret < 0) {
         g_warning("%s: Failed to add cert to key manager\n", __FUNCTION__);
         g_warning("PEM cert: %s\n", pemCert);
         VMXLog_Log(VMXLOG_LEVEL_WARNING,
                    "%s: Failed to add cert to key manager\n", __FUNCTION__);
         /*
          * XXX
          *
          * Certificates can have data (eg email addresses)
          * we don't want to log those to the VMX due to privacy concerns.
          * So let's not log to VMX at all until we have a reliable way to
          * cleanse them -- assuming that doesn't make them worthless
          * since the data won't match anything in the aliasStore
          * or a SAML token.
          */
#if 0
           VMXLog_Log(VMXLOG_LEVEL_WARNING, "PEM cert: %s\n", pemCert);
#endif
         goto done;
      }

      /*
       * add pemCert to the returned list
       */
      certList[i] = pemCert;
   }

   bRet = TRUE;
   *numCerts = num;
   *certChain = certList;

done:
   if (!bRet) {
      FreeCertArray(num, certList);
   }
   g_free(x509CertNodes);

   return bRet;
}


/*
 ******************************************************************************
 * VerifySignature --                                                    */ /**
 *
 * Verifies the signature on an XML document.
 *
 * @param[in]  doc       Parsed XML document.
 * @param[out] numCerts  Number of certs in the token.
 * @param[out] certChain Certs in the token. Caller should g_free() array and
 *                       contents.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

static gboolean
VerifySignature(xmlDocPtr doc,
                int *numCerts,
                gchar ***certChain)
{
   xmlNodePtr dsigNode;
   xmlNodePtr keyInfoNode;
   xmlNodePtr x509Node;
   xmlSecDSigCtxPtr dsigCtx = NULL;
   xmlSecKeysMngrPtr mgr = NULL;
   int ret;
   int num = 0;
   gchar **certList = NULL;
   gboolean bRet;
   gboolean retCode = FALSE;

   *numCerts = 0;
   *certChain = NULL;

   /*
    * First pull out the signature to get to the x509 cert.
    */
   dsigNode = xmlSecFindNode(xmlDocGetRootElement(doc),
                             xmlSecNodeSignature, xmlSecDSigNs);
   if (NULL == dsigNode) {
      g_warning("Missing signature node\n");
      goto done;
   }

   keyInfoNode = xmlSecFindNode(dsigNode, xmlSecNodeKeyInfo,
                                xmlSecDSigNs);
   if (NULL == keyInfoNode) {
      g_warning("Missing KeyInfo node\n");
      goto done;
   }

   x509Node = xmlSecFindNode(keyInfoNode, xmlSecNodeX509Data,
                             xmlSecDSigNs);
   if (NULL == x509Node) {
      g_warning("Missing x509 node\n");
      goto done;
   }

   /*
    * Make a key manager to hold the certs.
    */
   mgr = xmlSecKeysMngrCreate();
   if (mgr == NULL) {
      g_warning("Failed to create key manager");
      goto done;
   }

   ret = xmlSecCryptoAppDefaultKeysMngrInit(mgr);
   if (ret < 0) {
      g_warning("Failed to init key manager\n");
      goto done;
   }


   /*
    * Get the cert chain from the token.
    *
    * Unlike xml-security-c, xmlsec1 wants to validate the cert
    * chain in the token so it needs the full chain, not just
    * the public key from the first cert.
    *
    * Also save it off for later use by the alias store check.
    */
   bRet = BuildCertChain(x509Node, mgr, &num, &certList);
   if (FALSE == bRet) {
      g_warning("Failed to add cert to key manager\n");
      goto done;
   }

   /*
    * Create a signature context with the key manager
    */
   dsigCtx = xmlSecDSigCtxCreate(mgr);
   if (NULL == dsigCtx) {
      g_warning("Missing signature node\n");
      goto done;
   }

   /*
    * The vgauth service code expects the id to be "ID".  xmlSec
    * won't handle the URI ref in the signature unless we
    *
    * a) use 'xml:id' (the default) instead of "ID"
    * or
    * b) register the ID
    *
    * We can't control what the SSO server does, so its "b".
    */
   bRet = RegisterID(xmlDocGetRootElement(doc), "ID");
   if (bRet == FALSE) {
      g_warning("failed to register ID\n");
      goto done;
   }

   /*
    * Verify signature.  This just returns if the signature code worked
    * or not, not if the signature is correct.
    */
   ret = xmlSecDSigCtxVerify(dsigCtx, dsigNode);
   if (ret < 0) {
      g_warning("Signature verify failed\n");
      goto done;
   }

   /*
    * The xml-security-c verifies the Reference explicitly; this
    * isn't needed for xmlsec1 because the library does it.
    */

   /*
    * Check status to verify the signature is correct.
    *
    */
   if (dsigCtx->status != xmlSecDSigStatusSucceeded) {
      g_warning("Signature is INVALID\n");
      VMXLog_Log(VMXLOG_LEVEL_WARNING,
                 "%s: signature is invalid\n", __FUNCTION__);
      goto done;
   }

   retCode = TRUE;
   *numCerts = num;
   *certChain = certList;
done:
   if (!retCode) {
      FreeCertArray(num, certList);
   }
   if (dsigCtx) {
      xmlSecDSigCtxDestroy(dsigCtx);
   }
   if (mgr) {
      xmlSecKeysMngrDestroy(mgr);
   }

   return retCode;
}


/*
 ******************************************************************************
 * VerifySAMLToken --                                                    */ /**
 *
 * Verifies a XML text as a SAML token.
 * Parses the XML, then verifies Subject, Conditions and Signature.
 *
 * @param[in]  token     Text of SAML token.
 * @param[out] subject   Subject of SAML token,  Caller must g_free().
 * @param[out] numCerts  Number of certs in the token.
 * @param[out] certChain Certs in the token. Caller should g_free() array and
 *                       contents.
 *
 * @return matching TRUE on success.
 *
 ******************************************************************************
 */

static gboolean
VerifySAMLToken(const gchar *token,
                gchar **subject,
                int *numCerts,
                gchar ***certChain)
{
   xmlDocPtr doc = NULL;
   int retCode = FALSE;
   gboolean bRet;
   /*
    * If we want to set extra options, use this path.
    */
#if PARSE_WITH_OPTIONS
   xmlParserCtxtPtr parseCtx = NULL;

   parseCtx = xmlCreateDocParserCtxt(token);

   /*
    * Don't allow extra stuff to be pulled off the net.
    * The schema validation should prevent this from getting
    * through, but it might still be nice to prevent network issues
    * from slowing things down.
    */
   xmlCtxtUseOptions(parseCtx, XML_PARSE_NONET);
   doc = xmlCtxtReadMemory(parseCtx,
                           token,
                           strlen(token),
                           NULL, NULL, 0);
#else
   doc = xmlParseMemory(token, (int)strlen(token));
#endif
   if ((NULL == doc) || (xmlDocGetRootElement(doc) == NULL)) {
      g_warning("Failed to parse document\n");
      goto done;
   }

   bRet = ValidateDoc(doc);
   if (FALSE == bRet) {
      g_warning("Failed to validate token against schema\n");
      goto done;
   }

   bRet = VerifySubject(doc, subject);
#ifndef TEST_VERIFY_SIGN_ONLY
   if (FALSE == bRet) {
      g_warning("Failed to verify Subject node\n");
      goto done;
   }
#endif

   bRet = VerifyConditions(doc);
#ifndef TEST_VERIFY_SIGN_ONLY
   if (FALSE == bRet) {
      g_warning("Failed to verify Conditions\n");
      goto done;
   }
#endif

   bRet = VerifySignature(doc, numCerts, certChain);
   if (FALSE == bRet) {
      g_warning("Failed to verify Signature\n");
      // XXX Can we log the token at this point without risking security?
      goto done;
   }

   retCode = TRUE;
done:
#if PARSE_WITH_OPTIONS
   if (NULL != parseCtx) {
      xmlFreeParserCtxt(parseCtx);
   }
#endif
   if (!retCode && (NULL != subject)) {
      g_free(*subject);
      *subject = NULL;
   }
   if (doc) {
      xmlFreeDoc(doc);
   }

   return retCode;
}


/*
 ******************************************************************************
 * SAML_VerifyBearerToken --                                             */ /**
 *
 * Determines whether the SAML bearer token can be used to authenticate.
 * A token consists of a single SAML assertion.
 *
 * This is currently only used from the test code.
 *
 * @param[in]  xmlText     The text of the SAML assertion.
 * @param[in]  userName    Optional username to authenticate as.
 * @param[out] userNameOut The user that the token has authenticated as.
 * @param[out] subjNameOut The subject in the token.  Caller must g_free().
 * @param[out] verifyAi    The alias info associated with the entry
 *                         in the alias store used to verify the
 *                         SAML cert.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
SAML_VerifyBearerToken(const char *xmlText,
                       const char *userName,                // UNUSED
                       char **userNameOut,                  // UNUSED
                       char **subjNameOut,
                       ServiceAliasInfo **verifyAi)         // UNUSED
{
   gboolean ret;
   gchar **certChain = NULL;
   int num = 0;

   ret = VerifySAMLToken(xmlText,
                         subjNameOut,
                         &num,
                         &certChain);

   // clean up -- this code doesn't look at the chain
   FreeCertArray(num, certChain);

   return (ret == TRUE) ? VGAUTH_E_OK : VGAUTH_E_AUTHENTICATION_DENIED;
}


/*
 ******************************************************************************
 * SAML_VerifyBearerTokenAndChain --                                     */ /**
 *
 * Determines whether the SAML bearer token can be used to authenticate.
 * A token consists of a single SAML assertion.
 * The token must first be verified, then the certificate chain used
 * verify it must be checked against the appropriate certificate store.
 *
 * @param[in]  xmlText     The text of the SAML assertion.
 * @param[in]  userName    Optional username to authenticate as.
 * @param[out] userNameOut The user that the token has authenticated as.
 * @param[out] subjNameOut The subject in the token.  Caller must g_free().
 * @param[out] verifyAi    The alias info associated with the entry
 *                         in the alias store used to verify the
 *                         SAML cert.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
SAML_VerifyBearerTokenAndChain(const char *xmlText,
                               const char *userName,
                               char **userNameOut,
                               char **subjNameOut,
                               ServiceAliasInfo **verifyAi)
{
   VGAuthError err;
   gboolean bRet;
   int num;
   gchar **certChain = NULL;
   ServiceSubject subj;

   *userNameOut = NULL;
   *subjNameOut = NULL;
   *verifyAi = NULL;

   bRet = VerifySAMLToken(xmlText,
                          subjNameOut,
                          &num,
                          &certChain);

   if (FALSE == bRet) {
      return VGAUTH_E_AUTHENTICATION_DENIED;
   }

   subj.type = SUBJECT_TYPE_NAMED;
   subj.name = *subjNameOut;
   err = ServiceVerifyAndCheckTrustCertChainForSubject(num,
                                                       (const char **) certChain,
                                                       userName,
                                                       &subj,
                                                       userNameOut,
                                                       verifyAi);
   g_debug("%s: ServiceVerifyAndCheckTrustCertChainForSubject() "
           "returned "VGAUTHERR_FMT64"\n", __FUNCTION__, err);
   FreeCertArray(num, certChain);

   return err;
}

