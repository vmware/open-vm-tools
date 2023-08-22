/*********************************************************
 * Copyright (c) 2011-2017,2023 VMware, Inc. All rights reserved.
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
 * @file saml-xml-security-c.cpp
 *
 * Code for authenticating users based on SAML tokens.
 */

#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#undef WIN32_LEAN_AND_MEAN  // XSEC unconditionally redefines this
#include <xsec/dsig/DSIGKeyInfoX509.hpp>
#include <xsec/dsig/DSIGReference.hpp>
#include <xsec/dsig/DSIGReferenceList.hpp>
#include <xsec/framework/XSECEnv.hpp>
#include <xsec/framework/XSECException.hpp>
#include <xsec/framework/XSECProvider.hpp>
#include <xsec/utils/XSECDOMUtils.hpp>

#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/MemoryManager.hpp>
#include <xercesc/framework/XMLGrammarPool.hpp>
#include <xercesc/framework/XMLGrammarPoolImpl.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/sax/ErrorHandler.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/validators/common/Grammar.hpp>

/*
 * XXX
 *
 * Optimization idea: stash a hash (SHA512) of a valid token, and bypass
 * the full assertion process when we see that token again. The expiration
 * date of the token must also be saved off (and beware the time skew issue).
 *
 * Note that there's some extra complexity here:
 *
 * 1 - AddAlias sets up a cert/user mapping
 * 2 - a SAML token is used (and cached) using this cert/user combo
 * 3 - RemoveAlias removes the combo
 * 4 - the cached token still works
 *
 * So the cache should only bypass the token validation, not the certificate
 * check in ServiceVerifyAndCheckTrustCertChainForSubject()
 *
 * Also TBD is how much this buys us in the real world.  With short
 * token lifetimes, its less interesting.  Its also possible that
 * it will have no measureable affect because the token verification
 * will be lost in the noise of the API plumbing from VC->hostd->VMX->tools.
 *
 * The security folks have signed off on this, so long as we store only
 * in memory.
 *
 */

/*
 * XXX
 *
 * We should be a lot smarter about this, but this gets QE
 * moving.
 */
#define SAML_TOKEN_PREFIX "saml:"
#define SAML_TOKEN_SSO_PREFIX "saml2:"

extern "C" {
#include "prefs.h"
#include "serviceInt.h"
}
#include "samlInt.hpp"


/**
 * Error handler used to log warnings from the XML parser.
 */

class SAMLErrorHandler : public ErrorHandler {
public:
   static void
   printWarning(const SAXParseException &e,
                const char *msg)
   {
      SAMLStringWrapper nativeMsg(e.getMessage());

      /*
       * XXX
       *
       * These functions were inlined on older compilers but are exported
       * from libstdc++.so on newer compilers (4.4.3). Avoid using them to
       * avoid the newer dependency.
       *
       * _ZNSo9_M_insertIyEERSoT_@@GLIBCXX_3.4.9
       *    std::basic_ostream<char, std::char_traits<char> >&
       *       std::basic_ostream<char, std::char_traits<char> >::
       *       _M_insert<unsigned long long>(unsigned long long)
       *  aka: operator<<(uint64_t)
       *
       * _ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_i@@GLIBCXX_3.4.9
       *    std::basic_ostream<char, std::char_traits<char> >&
       *       std::__ostream_insert<char, std::char_traits<char> >
       *       (std::basic_ostream<char, std::char_traits<char> >&,
       *       char const*, int)
       *  aka: operator<<(std::string)
       *
       */
      Debug("SAML: %s: %s (line=%d, col=%d)\n",
            msg, nativeMsg.c_str(),
            (int) e.getLineNumber(), (int) e.getColumnNumber());

#ifdef avoid_this_usage
      /*
       * I'm tired of defining format modifier macros, so let's use
       * stringstream to handle e.getLineNumber()'s return type.
       */

      std::stringstream ss;

      ss << msg << ": " << nativeMsg.c_str() << "" << " (line=" <<
         e.getLineNumber() << ", col=" << e.getColumnNumber() << ")";

      Debug("SAML: %s.\n", ss.str().c_str());
#endif
   }

   void
   warning(const SAXParseException &e)
   {
      printWarning(e, "warning");
   }

   void
   error(const SAXParseException &e)
   {
      printWarning(e, "error");
   }

   void
   fatalError (const SAXParseException &e)
   {
      printWarning(e, "fatal error");
   }

   void
   resetErrors()
   {
   }
};



/**
 * The XML schema files needed to perform validating parsing of the
 * SAML assertions. Note: the order is important, since schemas need
 * to be loaded before any schema that depends on them, so don't change
 * the order.
 */
static const char *schemas[] = {
   "xml.xsd",
   "XMLSchema.xsd",
   "xmldsig-core-schema.xsd",
   "xenc-schema.xsd",
   "saml-schema-assertion-2.0.xsd",
};


/**
 * An in-memory cache for XML schemas.
 */
static XMLGrammarPool *pool = NULL;

static int clockSkewAdjustment = VGAUTH_PREF_DEFAULT_CLOCK_SKEW_SECS;

static bool SAMLLoadSchema(XercesDOMParser &parser,
                           const SAMLGlibString &schemaDir,
                           const char *filename);
static DOMDocument *SAMLValidateSchemaAndParse(XercesDOMParser &parser,
                                               const char *xmlText);

static bool SAMLCheckSubject(const DOMDocument *doc,
                             SAMLTokenData &token);

static bool SAMLCheckConditions(const DOMDocument *doc,
                                SAMLTokenData &token);

static bool SAMLCheckTimeAttr(const DOMElement *elem, const char *attrName,
                              bool beforeNow);

static bool SAMLCheckAudience(const XMLCh *audience);

static bool SAMLCheckSignature(DOMDocument *doc,
                               gboolean hostVerified,
                               vector<string> &certs);

static bool SAMLCheckReference(const DOMDocument *doc, DSIGSignature *sig);

static DOMElement *SAMLFindChildByName(const DOMElement *elem,
                                       const char *name);

static auto_ptr<DSIGKeyInfoX509> SAMLFindKey(const XSECEnv &secEnv,
                                             const DOMElement *sigElem);


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
SAML_Init()
{
   try {
      XMLPlatformUtils::Initialize();
      XSECPlatformUtils::Initialise();

      auto_ptr<XMLGrammarPool> myPool = SAMLCreateAndPopulateGrammarPool();
      if (NULL == myPool.get()) {
         return VGAUTH_E_FAIL;
      }

      pool = myPool.release();

      clockSkewAdjustment = Pref_GetInt(gPrefs, VGAUTH_PREF_CLOCK_SKEW_SECS,
                                        VGAUTH_PREF_GROUP_NAME_SERVICE,
                                        VGAUTH_PREF_DEFAULT_CLOCK_SKEW_SECS);
      Log("%s: Allowing %d of clock skew for SAML date validation\n",
          __FUNCTION__, clockSkewAdjustment);

      return VGAUTH_E_OK;
   } catch (const XMLException& e) {
      SAMLStringWrapper msg(e.getMessage());

      Warning("Failed to initialize Xerces: %s.\n", msg.c_str());
      return VGAUTH_E_FAIL;
   } catch (...) {
      // We're called from C code, so don't let any exceptions out.
      Warning("%s: Unexpected exception.\n", __FUNCTION__);
      return VGAUTH_E_FAIL;
   }
}


/*
 ******************************************************************************
 * SAMLCreateAndPopulateGrammarPool --                                   */ /**
 *
 * Creates a grammar pool that is populates with cached grammars representing
 * the XML schemas needed for SAML validation.
 *
 * @return A heap allocated grammar pool (must be freed with operator
 *         delete) or NULL on failure.
 *
 ******************************************************************************
 */

auto_ptr<XMLGrammarPool>
SAMLCreateAndPopulateGrammarPool()
{
   auto_ptr<XMLGrammarPool> newPool(new XMLGrammarPoolImpl(XMLPlatformUtils::fgMemoryManager));

   /*
    * Create a parser instance to load all the schemas, so they can
    * be cached for later. In addition to making parsing faster, we
    * need to cache them so that Xerces does not try to download
    * schemas from the web when one is referenced or imported by another
    * schema.
    */
   XercesDOMParser parser(NULL, XMLPlatformUtils::fgMemoryManager,
                          newPool.get());

   gchar *dir = Pref_GetString(gPrefs, VGAUTH_PREF_SAML_SCHEMA_DIR,
                               VGAUTH_PREF_GROUP_NAME_SERVICE, NULL);
   if (NULL == dir) {
#ifdef _WIN32
      /*
       * To make life easier for the Windows installer, assume
       * the schema directory is next to the executable.  Also
       * check in ../ in case we're in a dev environment.
       */
      dir = g_build_filename(gInstallDir, "schemas", NULL);
      if (!(g_file_test(dir, G_FILE_TEST_EXISTS) &&
            g_file_test(dir, G_FILE_TEST_IS_DIR))) {

         gchar *newDir = g_build_filename(gInstallDir, "..", "schemas", NULL);

         Debug("%s: schemas not found in Windows install loc '%s',"
               " trying dev location of '%s'\n", __FUNCTION__, dir, newDir);

         g_free(dir);
         dir = newDir;
      }
#else
      /*
       * XXX -- clean this up to make a better default for Linux.
       */
      dir = g_build_filename(gInstallDir, "..", "schemas", NULL);
#endif
   }
   Log("%s: Using '%s' for SAML schemas\n", __FUNCTION__, dir);
   SAMLGlibString schemaDir(dir);

   for (unsigned int i = 0; i < G_N_ELEMENTS(schemas); i++) {
      if (!SAMLLoadSchema(parser, schemaDir, schemas[i])) {
         return auto_ptr<XMLGrammarPool>(NULL);
      }
   }

   return newPool;
}


/*
 ******************************************************************************
 * SAML_Shutdown --                                                      */ /**
 *
 * Performs any clean-up of resources needed for SAML processing.
 *
 ******************************************************************************
 */

void
SAML_Shutdown()
{
   try {
      delete pool;
      pool = NULL;
      XSECPlatformUtils::Terminate();
      XMLPlatformUtils::Terminate();
   } catch (...) {
      // We're called from C code, so don't let any exceptions out.
      Warning("%s: Unexpected exception.\n", __FUNCTION__);
   }
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
   ASSERT(pool != NULL);

   auto_ptr<XMLGrammarPool> myPool = SAMLCreateAndPopulateGrammarPool();
   if (NULL == myPool.get()) {
      Warning("%s: Failed to reload SAML state. Using old settings.\n",
              __FUNCTION__);
      return;
   }

   delete pool;
   pool = myPool.release();
}


/*
 ******************************************************************************
 * SAMLLoadSchema --                                                     */ /**
 *
 * Loads a schema into the grammar pool used by the given parser.
 *
 * @param[in]  parser    The parser to load the schema with.
 * @param[in]  schemaDir The full path to the directory containing the schema.
 * @param[in]  filename  The name of the XML schema file.
 *
 * @return true if the schema file was successfully loaded, false otherwise.
 *
 ******************************************************************************
 */

static bool
SAMLLoadSchema(XercesDOMParser &parser,
               const SAMLGlibString &schemaDir,
               const char *filename)
{
   SAMLGlibString schemaPath(g_build_filename(schemaDir.c_str(), filename,
                                              NULL));
   Grammar *g = parser.loadGrammar(schemaPath.c_str(),
                                   Grammar::SchemaGrammarType, true);
   if (g == NULL) {
      /*
       * The parser complains even with official schemas, so we don't
       * normally set an error handler. However, this should not fail since
       * we control these files, so try again with logging, so we can see
       * what went wrong.
       */
      SAMLErrorHandler errorHandler;
      parser.setErrorHandler(&errorHandler);

      g = parser.loadGrammar(schemaPath.c_str(), Grammar::SchemaGrammarType,
                             true);

      Warning("Failed to load XML Schema from %s.\n", schemaPath.c_str());
      return false;
   }

   return true;
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
 * @param[out] subjNameOut The subject in the token.
 * @param[out] verifySi    The subjectInfo associated with the entry
 *                         in the ID provider store used to verify the
 *                         SAML cert.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
SAML_VerifyBearerToken(const char *xmlText,
                       const char *userName,
                       char **userNameOut,
                       char **subjNameOut,
                       ServiceAliasInfo **verifyAi)
{
   try {
      vector<string> certs;
      VGAuthError err;
      SAMLTokenData token;

      err = SAMLVerifyAssertion(xmlText,
                                FALSE, // use original mode
                                token, certs);
      if (VGAUTH_E_OK != err) {
         return err;
      }

      return err;
   } catch (XSECException &e) {
      SAMLStringWrapper msg(e.getMsg());

      Warning("XSec exception while verifying assertion: %s.\n", msg.c_str());
      return VGAUTH_E_FAIL;
   } catch (const XMLException& e) {
      SAMLStringWrapper msg(e.getMessage());

      Warning("Xerces exception while verifying assertion: %s.\n",
              msg.c_str());
      return VGAUTH_E_FAIL;
   } catch (...) {
      // We're called from C code, so don't let any exceptions out.
      Warning("Unexpected exception.\n");
      return VGAUTH_E_FAIL;
   }
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
 * @param[in]  xmlText      The text of the SAML assertion.
 * @param[in]  userName     Optional username to authenticate as.
 * @param[in]  hostVerified If true, skip signature verification.
 * @param[out] userNameOut  The user that the token has authenticated as.
 * @param[out] subjNameOut  The subject in the token.
 * @param[out] verifySi     The subjectInfo associated with the entry
 *                          in the ID provider store used to verify the
 *                          SAML cert.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
SAML_VerifyBearerTokenAndChain(const char *xmlText,
                               const char *userName,
                               gboolean hostVerified,
                               char **userNameOut,
                               char **subjNameOut,
                               ServiceAliasInfo **verifyAi)
{
   *userNameOut = NULL;
   *subjNameOut = NULL;
   *verifyAi = NULL;

   try {
      vector<string> certs;
      VGAuthError err;
      SAMLTokenData token;
      char **pemCerts;
      ServiceSubject subj;
      int i;

      err = SAMLVerifyAssertion(xmlText,
                                hostVerified,
                                token, certs);
      if (VGAUTH_E_OK != err) {
         return err;
      }

      pemCerts = (char **) g_malloc0(sizeof(char *) * certs.size());
      for (i = 0; i < (int) certs.size(); i++) {
         pemCerts[i] = g_strdup(certs[i].c_str());
      }
      subj.type = SUBJECT_TYPE_NAMED;
      if (subjNameOut) {
         *subjNameOut = g_strdup(token.subjectName.c_str());
      }
      subj.name = g_strdup(token.subjectName.c_str());
      err = ServiceVerifyAndCheckTrustCertChainForSubject((int) certs.size(),
                                                          (const char **) pemCerts,
                                                          userName,
                                                          &subj,
                                                          userNameOut,
                                                          verifyAi);
      Debug("%s: ServiceVerifyAndCheckTrustCertChainForSubject() returned " VGAUTHERR_FMT64 "\n", __FUNCTION__, err);

      for (i = 0; i < (int) certs.size(); i++) {
         g_free(pemCerts[i]);
      }
      g_free(pemCerts);
      g_free(subj.name);
      return err;
   } catch (XSECException &e) {
      SAMLStringWrapper msg(e.getMsg());

      Warning("XSec exception while verifying assertion: %s.\n", msg.c_str());
      return VGAUTH_E_FAIL;
   } catch (const XMLException& e) {
      SAMLStringWrapper msg(e.getMessage());

      Warning("Xerces exception while verifying assertion: %s.\n",
              msg.c_str());
      return VGAUTH_E_FAIL;
   } catch (...) {
      // We're called from C code, so don't let any exceptions out.
      Warning("Unexpected exception.\n");
      return VGAUTH_E_FAIL;
   }
}


/*
 ******************************************************************************
 * SAMLVerifyAssertion --                                                */ /**
 *
 * Performs the following checks to validate a SAML assertion.
 * 1) Checks that the XML document is well formed according to the SAML 2.0
 *    Assertion XML schema.
 * 2) Check that the assertion is signed by a certificate contained within
 *    the assertion.
 * 3) TODO: Check that the assertion contains a Subject element, and that
 *    Subject element should contain a SubjectConfirmation element. The
 *    SubjectConfirmation method must be "bearer"
 *    ("urn:oasis:names:tc:SAML:2.0:cm:bearer").
 * 4) The Conditions element for the assertion must be met in terms of
 *    any "NotBefore" or "NotOnOrAfter" information.
 * The chain of certs used to verify the signature will be returned via @a
 * certs.
 *
 * @param[in]  xmlText
 * @param[in]  hostVerified If true, skip signature verification.
 * @param[out] token     The interesting bits extracted from the xmlText.
 * @param[out] certs     If the SAML assertion is verified, then this will
 *                       contain the certificate chain for the issuer.
 *                       Each certificate will be base64 encoded (but without
 *                       the PEM-style bookends), with the issuer's cert
 *                       at element 0.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
SAMLVerifyAssertion(const char *xmlText,
                    gboolean hostVerified,
                    SAMLTokenData &token,
                    vector<string> &certs)
{
   XercesDOMParser parser(NULL, XMLPlatformUtils::fgMemoryManager, pool);
   SAMLErrorHandler errorHandler;
   SecurityManager sm;

   parser.setErrorHandler(&errorHandler);

   // prevent the billion laughs attack -- put a limit on entity expansions
   sm.setEntityExpansionLimit(100);
   parser.setSecurityManager(&sm);

   DOMDocument *doc = SAMLValidateSchemaAndParse(parser, xmlText);
   if (NULL == doc) {
      return VGAUTH_E_AUTHENTICATION_DENIED;
   }

   const DOMElement *s = SAMLFindChildByName(doc->getDocumentElement(),
                                             SAML_TOKEN_PREFIX"Subject");
   if (NULL == s) {
      Debug("Couldn't find " SAML_TOKEN_PREFIX " in token\n");
      s = SAMLFindChildByName(doc->getDocumentElement(),
                                             SAML_TOKEN_SSO_PREFIX"Subject");
      if (NULL == s) {
         Debug("Couldn't find " SAML_TOKEN_SSO_PREFIX " in token\n");
         Warning("No recognized tags in token; punting\n");
         return VGAUTH_E_AUTHENTICATION_DENIED;
      } else {
         Debug("Found " SAML_TOKEN_SSO_PREFIX " in token\n");
         token.isSSOToken = true;
         token.ns = SAML_TOKEN_SSO_PREFIX;
      }
   } else {
      Debug("Found " SAML_TOKEN_PREFIX " in token\n");
      token.isSSOToken = false;
      token.ns = SAML_TOKEN_PREFIX;
   }

   if (!SAMLCheckSubject(doc, token)) {
      return VGAUTH_E_AUTHENTICATION_DENIED;
   }

   if (!SAMLCheckConditions(doc, token)) {
      return VGAUTH_E_AUTHENTICATION_DENIED;
   }

   if (!SAMLCheckSignature(doc,
                           hostVerified,
                           certs)) {
      return VGAUTH_E_AUTHENTICATION_DENIED;
   }

   return VGAUTH_E_OK;
}


/*
 ******************************************************************************
 * SAMLValidateSchemaAndParse --                                         */ /**
 *
 * Checks that the XML document is well formed according to the SAML 2.0
 * Assertion XML schema.
 *
 * @param[in]  parser    The parser to use with the XML document.
 * @param[in]  xmlText   The text of the SAML assertion.
 *
 * @return A pointer to a DOMDocument instance that represents the parsed
 *         SAML assertion or NULL if the document was not valid. The memory
 *         used by the DOMDocument is owned by the parser.
 *
 ******************************************************************************
 */

static DOMDocument *
SAMLValidateSchemaAndParse(XercesDOMParser &parser,
                           const char *xmlText)
{
   parser.setDoNamespaces(true);
   parser.setDoSchema(true);
   parser.setValidationScheme(AbstractDOMParser::Val_Always);
   parser.useCachedGrammarInParse(true);

   MemBufInputSource in(reinterpret_cast<const XMLByte *>(xmlText),
                        strlen(xmlText), "VGAuthSamlAssertion");

   parser.parse(in);

   xsecsize_t errorCount = parser.getErrorCount();
   if (errorCount > 0) {
      Debug("Encountered %u errors while parsing SAML assertion.\n",
            (unsigned int) errorCount);
      return NULL;
   }

   DOMDocument *doc = parser.getDocument();
   ASSERT(doc != NULL);

   return doc;
}


/*
 ******************************************************************************
 * SAMLCheckSubject --                                                */ /**
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
 * @param[in]     doc     The DOM representation of the SAML assertions.
 * @param[in/out] token   Information about the token to be populated.
 *
 * @return true if the conditions in at least one SubjectConfirmation is met,
 *         false otherwise.
 *
 ******************************************************************************
 */

static bool
SAMLCheckSubject(const DOMDocument *doc,
                 SAMLTokenData &token)
{
   const DOMElement *subject;
   char *name = g_strdup_printf("%sSubject",
                                token.ns.c_str());
   subject = SAMLFindChildByName(doc->getDocumentElement(), name);
   g_free(name);

   if (NULL == subject) {
      // Should not happen, since this is required element in the schema.
      Log("%s: Missing subject element!\n", __FUNCTION__);
//      ASSERT(0);
      return false;
   }

   const DOMElement *nameID;
   name = g_strdup_printf("%sNameID", token.ns.c_str());
   nameID = SAMLFindChildByName(subject, name);
   g_free(name);
   if (NULL == nameID) {
      /*
       * The schema allows BaseID, NameID, or EncryptedID. The library code
       * for the SSO server only supports NameID. EncryptedID is really
       * complicated (and we don't have decryption keys, so let's not
       * support it for now.
       */

      Log("%s: No NameID element for the subject.\n", __FUNCTION__);
      return false;
   }

   token.subjectName = SAMLStringWrapper(nameID->getTextContent()).c_str();
   Debug("%s: subjectName: '%s'\n", __FUNCTION__, token.subjectName.c_str());

   /*
    * TODO: Investigate: NameID elements can have a NameQualifier attribute.
    * This smells like a domain name, and we might want to include it with
    * subject name (<NameQualifier>\subjectName).
    */

   /*
    * Find all the SubjectConfirmation nodes and see if at least one can be
    * verified.
    */

   name = g_strdup_printf("%sSubjectConfirmation", token.ns.c_str());
   XMLT scName(name);
   g_free(name);
   for (DOMElement *child = subject->getFirstElementChild(); child != NULL;
        child = child->getNextElementSibling()) {

      if (!XMLString::equals(child->getNodeName(), scName.getUnicodeStr())) {
         continue;
      }

      const XMLCh *method = child->getAttribute(MAKE_UNICODE_STRING("Method"));
      if ((NULL == method) || (0 == *method)) {
         // Should not happen, since this is a required attribute.
         ASSERT(0);
         Debug("%s: Missing confirmation method.\n", __FUNCTION__);
         continue;
      }

      if (!XMLString::equals(
             MAKE_UNICODE_STRING("urn:oasis:names:tc:SAML:2.0:cm:bearer"),
             method)) {
         Debug("%s: Non-bearer confirmation method in token", __FUNCTION__);
         continue;
      }

      const DOMElement *subjConfirmData;
      name = g_strdup_printf("%sSubjectConfirmationData", token.ns.c_str());
      subjConfirmData = SAMLFindChildByName(child, name);
      g_free(name);
      if (NULL != subjConfirmData) {
         if (!SAMLCheckTimeAttr(subjConfirmData, "NotBefore", true) ||
             !SAMLCheckTimeAttr(subjConfirmData, "NotOnOrAfter", false)) {
            Warning("%s: subjConfirmData time check failed\n", __FUNCTION__);
            continue;
         }

         const XMLCh *recipient;
         recipient = subjConfirmData->getAttribute(
            MAKE_UNICODE_STRING("Recipient"));
         /*
          * getAttribute() returns a 0-length string, not NULL, if it can't
          * find what it wants.
          */
         if ((0 != XMLString::stringLen(recipient)) &&
             !SAMLCheckAudience(recipient)) {
            Debug("%s: failed recipient check\n", __FUNCTION__);
            continue;
         }
      }

      return true;
   }

   Debug("%s: Could not verify using any SubjectConfirmation elements\n",
         __FUNCTION__);
   return false;
}


/*
 ******************************************************************************
 * SAMLCheckConditions --                                                */ /**
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
 * @param[in]  doc  The DOM representation of the SAML assertions.
 *
 * @return true if the conditions are met; false otherwise.
 *
 ******************************************************************************
 */

static bool
SAMLCheckConditions(const DOMDocument *doc,
                    SAMLTokenData &token)
{
   /*
    * There should be at most one Conditions element and the schema checking
    * done by the parser should enforce that.
    */
   char *name = g_strdup_printf("%sConditions", token.ns.c_str());
   const DOMElement *conditions = SAMLFindChildByName(doc->getDocumentElement(),
                                                      name);
   g_free(name);
   if (NULL == conditions) {
      // Conditions are optional.
      return true;
   }

   if (!SAMLCheckTimeAttr(conditions, "NotBefore", true) ||
       !SAMLCheckTimeAttr(conditions, "NotOnOrAfter", false)) {
      return false;
   }

   /*
    * <Condition> is a generic element, intended as an extension point.
    * We don't know about any. According to the general processng rules, if
    * we find a condition we don't know about, the result of the validation
    * is "indeterminate" and we should reject the assertion.
    */
   name = g_strdup_printf("%sCondition", token.ns.c_str());
   if (SAMLFindChildByName(conditions, name) != NULL) {
      Log("%s: Unrecognized condition found!\n", __FUNCTION__);
      g_free(name);
      return false;
   }
   g_free(name);

   /*
    * <AudienceRestriction> defines a set a URIs that describe what
    * audience the assertioned is addressed to or intended for.
    * But it's very generic. From the spec (section 2.5.1.4):
    *    A URI reference that identifies an intended audience. The URI
    *    reference MAY identify a document that describes the terms and
    *    conditions of audience membership. It MAY also contain the unique
    *    identifier URI from a SAML name identifier that describes a system
    *    entity.
    * Some searching online shows people using http://<hostname>/ as the
    * URI, but let's wait until we get some feedback from the SSO team.
    * TODO: Validate it using SAMLCheckAudience().
    */

   /*
    * <OneTimeUse> element is specified to disallow caching. We don't
    * cache, so it doesn't affect out validation.
    * However, we need to communicate it to clients so they do not cache.
    */
   name = g_strdup_printf("%sOneTimeUse", token.ns.c_str());
   token.oneTimeUse = (SAMLFindChildByName(conditions, name)
                       != NULL);
   g_free(name);

   /*
    * <ProxyRestriction> only applies if a service wants to make their own
    * assertions based on a SAML assertion. That should not apply here.
    */

   return true;
}


/*
 ******************************************************************************
 * SAMLCheckTimeAttr --                                                */ /**
 *
 * Checks that the given attribute with the given name is a timestamp and
 * compares it against the current time.
 *
 * @param[in]  elem         The element containing the attribute.
 * @param[in]  attrName     The name of the attribute.
 * @param[in]  notBefore    Whether the condition given by the attribute
 *                          should be in the past or 'now' (true).
 *
 ******************************************************************************
 */

static bool
SAMLCheckTimeAttr(const DOMElement *elem,
                  const char *attrName,
                  bool notBefore)
{
   const XMLCh *timeAttr = elem->getAttribute(MAKE_UNICODE_STRING(attrName));
   if ((NULL == timeAttr) || (0 == *timeAttr)) {
      /*
       * The presence of all time restrictions in SAML are optional, so if
       * the attribute is not present, that is fine.
       */
      return true;
   }

   SAMLStringWrapper timeStr(timeAttr);
   GTimeVal attrTime;

   if (!g_time_val_from_iso8601(timeStr.c_str(), &attrTime)) {
      Log("%s: Could not parse %s value (%s).\n", __FUNCTION__, attrName,
          timeStr.c_str());
      return false;
   }

   GTimeVal now;
   g_get_current_time(&now);

   glong diff;

   /*
    * Check the difference, doing the math so that a positive
    * value is bad.  Ignore the micros since we're letting clock
    * skew add a fudge-factor.
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
   if (diff > clockSkewAdjustment) {
      Warning("%s: FAILED SAML assertion (timeStamp %s, delta %d) %s.\n",
              __FUNCTION__, timeStr.c_str(), (int) diff,
              notBefore ? "is not yet valid" : "has expired");
      return false;
   }

   return true;
}


/*
 ******************************************************************************
 * SAMLCheckAudience --                                                  */ /**
 *
 * Checks whether the given audience URI refers to this machine.
 *
 * @param[in]  audience   An audience URI that a token is targetted for.
 *
 * @return True if the audience URI refers to this machine, false otherwise.
 *
 ******************************************************************************
 */

static bool
SAMLCheckAudience(const XMLCh *audience)
{
   bool ret;

   /*
    * XXX This should be much better. Ideally it should check that it refers
    * to the hostname of a URL or matches some kind of URN. Also, this is
    * where the VC UUID can be used when running in a VM.
    * We should accept:
    *   URL: <scheme_name>://<host_name>[/stuff]
    *   URN: urn:vmware:vgauth:<vc_domain_name>:<vc_vm_uuid>:[vgauth_client_app_name]
    * Glib has a basic URL and we should use it.
    * TODO: Need a RpcIn call into the VMX to get the VC UUID, since it is not
    * currently exposed. (Could be NamespaceDB, but then need to make a separate
    * workflow for pushing the VC UUIDs out to VMs.)
    */

   ret = strstr(SAMLStringWrapper(audience).c_str(),
                 g_get_host_name()) != NULL;
   Debug("%s: audience check: token: '%s', host: '%s' ? %d\n",
         __FUNCTION__,
         SAMLStringWrapper(audience).c_str(),
                          g_get_host_name(), ret);
   return ret;
}


/*
 ******************************************************************************
 * SAMLCheckSignature --                                                 */ /**
 *
 * Finds the signature in the SAML assertion, then extracts the X509
 * from that, then checks that the signature is valid.
 *
 * @param[in]  doc     The document of which to check the signature.
 * @param[in]  hostVerified If true, skip signature verification.
 * @param[out] certs   The base64 encoded certificates present in the
 *                     signature.
 *
 * @return true if the signature if valid, false otherwise.
 *
 ******************************************************************************
 */

static bool
SAMLCheckSignature(DOMDocument *doc,
                   gboolean hostVerified,
                   vector<string> &certs)
{
   DOMElement *sigElem = SAMLFindChildByName(doc->getDocumentElement(),
                                             "ds:Signature");
   if (NULL == sigElem) {
      Warning("%s: No top level signature found.\n", __FUNCTION__);
      return false;
   }

   XSECEnv secEnv(doc);

   auto_ptr<DSIGKeyInfoX509> keyInfo = SAMLFindKey(secEnv, sigElem);
   if (keyInfo.get() == NULL) {
      Warning("%s: No X509 data found as part of the signature.\n",
            __FUNCTION__);
      return false;
   }

   if (keyInfo->getCertificateListSize() == 0) {
      Warning("%s: No X509 certificates found in the signature\n",
              __FUNCTION__);
      return false;
   }
   if (hostVerified) {
      Debug("hostVerified is set, skipping signtaure check");
   } else {

   const XSECCryptoX509 *x509 = keyInfo->getCertificateCryptoItem(0);
   ASSERT(NULL != x509);

   XSECProvider prov;
   DSIGSignature *sig = prov.newSignatureFromDOM(doc, sigElem);

   sig->load();
   sig->setSigningKey(x509->clonePublicKey());

   if (!SAMLCheckReference(doc, sig)) {
      return false;
   }

   if (!sig->verify()) {
      Warning("%s: Signature check failed: %s.\n", __FUNCTION__,
              SAMLStringWrapper(sig->getErrMsgs()).c_str());
      return false;
   }

   }
   for (int i = 0; i < keyInfo->getCertificateListSize(); i++) {
      const XSECCryptoX509 *cert = keyInfo->getCertificateCryptoItem(i);
      certs.push_back(string(cert->getDEREncodingSB().rawCharBuffer()));
   }

   return true;
}


/*
 ******************************************************************************
 * SAMLCheckReference --                                                 */ /**
 *
 * Checks that the given signature refers to (and thus was computed over)
 * the root element of the document. This ensures that the entire document
 * is protected/endorsed by the signature.
 * See the SAML Core specification, section 5.4.2.
 *
 * @param[in]  doc   The document in which contains the signature.
 * @param[in]  sig   The signature
 *
 * @return true if the signature refers to the whole document, or false
 *         otherwise.
 *
 ******************************************************************************
 */

static bool
SAMLCheckReference(const DOMDocument *doc,
                   DSIGSignature *sig)
{
   DOMElement *rootElem = doc->getDocumentElement();

   const XMLCh *id = rootElem->getAttribute(MAKE_UNICODE_STRING("ID"));
   if (NULL == id) {
      Debug("%s: NULL ID attribute.\n", __FUNCTION__);
      return false;
   }

   XMLSize_t idLen = XMLString::stringLen(id);
   if (0 == idLen) {
      Debug("%s: Root element has no or an empty ID attribute.\n",
            __FUNCTION__);
      return false;
   }

   /*
    * At least one reference should contain a URI that refers to the root
    * element. To do so, that URI should be "#" followed by the value of
    * the ID element of the root node; for example if the ID is "SAML" the
    * URI must be "#SAML".
    *
    * TODO: The vmacore implementation of SAML parsing, used by clients
    * validating tokens, allows for multiple references and considers if
    * at least one matches. However, the SAML spec (section 5.4.2) requires
    * that there be only one reference element in the signature. Currently
    * we follow the vmacore behavior.
    */

   XMLT uriPrefix("#");
   XMLSize_t prefixLen = XMLString::stringLen(uriPrefix.getUnicodeStr());

   DSIGReferenceList *references = sig->getReferenceList();
   DSIGReferenceList::size_type numReferences = references->getSize();
   for (DSIGReferenceList::size_type i = 0; i < numReferences; i++) {
      DSIGReference *ref = references->item(i);
      const XMLCh *uri = ref->getURI();

      if (uri != NULL &&
          XMLString::startsWith(uri, uriPrefix.getUnicodeStr()) &&
          XMLString::equals(id, uri + prefixLen)) {
         return true;
      }
   }

   Debug("%s: No matching reference found in the signature for ID '%s'.\n",
         __FUNCTION__, SAMLStringWrapper(id).c_str());
   return false;
}


/*
 ******************************************************************************
 * SAMLFindChildByName --                                                */ /**
 *
 * Finds the first element that is a child of the given element which
 * matches the given node name.
 *
 * TODO: Investigate using getLocalName() and getNamespaceURI() to
 * identify the child, since in "ds:Signature" "ds" is an alias to as longer
 * URI, and that URI should be used instead (it's more stable).
 *
 * @param[in]  elem  The element to search the children of.
 * @param[in]  name  The name of the child element
 *
 * @return A pointer to the DOMElement matching the name, or NULL if
 *         no such element is found.
 *
 ******************************************************************************
 */

static DOMElement *
SAMLFindChildByName(const DOMElement *elem,
                    const char *name)
{
   XMLT sigNodeName(name);
   DOMElement *childElem;

   for (childElem = elem->getFirstElementChild();
        childElem != NULL; childElem = childElem->getNextElementSibling()) {
      if (XMLString::equals(childElem->getNodeName(),
                            sigNodeName.getUnicodeStr())) {
         break;
      }
   }

   return childElem;
}


/*
 ******************************************************************************
 * SAMLFindKey --                                                        */ /**
 *
 * Finds the first ds:X509Data element under the given ds:Signature element.
 *
 * @param[in]  secEnv    A XSEC environment to create the object from.
 * @param[in]  sigElem   The root element of the signuture.
 *
 * @return A pointer to a DSIGKeyInfoX509 object, which must be freed using
 *         operator delete, or NULL if no ds:X509Data element is found.
 *
 ******************************************************************************
 */

static auto_ptr<DSIGKeyInfoX509>
SAMLFindKey(const XSECEnv &secEnv,
            const DOMElement *sigElem)
{
   DOMNodeList *keyInfos =
      sigElem->getElementsByTagName(MAKE_UNICODE_STRING("ds:X509Data"));

   if (keyInfos->getLength() == 0) {
      return auto_ptr<DSIGKeyInfoX509>(NULL);
   }

   auto_ptr<DSIGKeyInfoX509> keyInfo(new DSIGKeyInfoX509(&secEnv,
                                                         keyInfos->item(0)));

   keyInfo->load();

   return keyInfo;
}
