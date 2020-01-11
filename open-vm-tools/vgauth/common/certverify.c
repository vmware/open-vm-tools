/*********************************************************
 * Copyright (C) 2011-2016,2018-2019 VMware, Inc. All rights reserved.
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
 * @file certverify.c
 *
 * Code to handle certficate verification with OpenSSL.
 */

#include "VGAuthError.h"
#include "VGAuthBasicDefs.h"
#include "VGAuthAuthentication.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/err.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "certverify.h"

VGAuthError CertVerify_CheckSignature(VGAuthHashAlg hash,
                                      EVP_PKEY *publicKey,
                                      size_t dataLen,
                                      const unsigned char *data,
                                      size_t signatureLen,
                                      const unsigned char *signature);



/*
 ******************************************************************************
 * VGAuthVerifyInit --                                                   */ /**
 *
 * Initializes OpenSSL for verify work.
 *
 ******************************************************************************
 */

void
CertVerify_Init(void)
{
   /*
    * XXX SSL init test -- loads err strings from both libs.
    * We may need just ERR_load_crypto_strings(); using this
    * to verify the make/deploy stuff doesn't leave libssl out.
    */
   SSL_load_error_strings();

   /*
    * XXX This may need tuning.  All we need for signature checks
    * are the digests.  We may want to add the ciphers or algorithms as
    * well, but OpenSSL_add_all_algorithms() and OpenSSL_add_all_ciphers()
    * can add a lot of bloat.
    */
   OpenSSL_add_all_digests();
}


/*
 ******************************************************************************
 * VerifyDumpSSLErrors --                                                */ /**
 *
 * Drains the ssl error stack and redirects them through glib.
 *
 ******************************************************************************
 */

static void
VerifyDumpSSLErrors(void)
{
   int flags;
   int line;
   const char *data;
   const char *file;
   unsigned long code;

   code = ERR_get_error_line_data(&file, &line, &data, &flags);
   while (code) {
      g_warning("SSL error: %lu (%s) in %s line %d\n",
                code, ERR_error_string(code, NULL), file, line);
      if (data && (flags & ERR_TXT_STRING)) {
         g_warning("SSL error data: %s\n", data);
      }
      /*
       * Note -- the docs mention the ERR_TXT_MALLOCED flag, but that doesn't
       * mean we got a copy of 'data'.  If 'data' is free'd here, it 'works'
       * until the SSL error buffer starts getting reused and a double
       * free happens.
       */
      code = ERR_get_error_line_data(&file, &line, &data, &flags);
   }
}


/*
 ******************************************************************************
 * VerifyCallback --                                                     */ /**
 *
 * Store verification callback.  Alows customization and error logging
 * during verification.
 *
 * @param[in]  ok      The current state of the verification.
 * @param[in]  ctx     The x509 store context.
 *
 * @return 1 on success, 0 on failure.
 *
 ******************************************************************************
 */

static int
VerifyCallback(int ok,
               X509_STORE_CTX *ctx)
{
   int ret = ok;
   int certErr = X509_STORE_CTX_get_error(ctx);
   X509 *curCert = X509_STORE_CTX_get_current_cert(ctx);
   char nameBuf[512];

   /*
    * XXX
    *
    * This is a legacy function that has some issues, but setting up a bio
    * just for a bit of debug seems overkill.
    */
   if (NULL != curCert) {
      X509_NAME_oneline(X509_get_subject_name(curCert), nameBuf, sizeof(nameBuf) - 1);
      nameBuf[sizeof(nameBuf)-1] = '\0';
   } else {
      g_strlcpy(nameBuf, "<NO CERT SUBJECT>", sizeof nameBuf);
   }
   g_debug("%s: name: %s ok: %d error %d at %d depth lookup:%s\n",
           __FUNCTION__,
           nameBuf,
           ok,
           certErr,
           X509_STORE_CTX_get_error_depth(ctx),
           X509_verify_cert_error_string(certErr));

   if (!ok) {
      switch (certErr) {
         // self-signed is ok
      case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
      case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
         g_debug("%s: allowing error %d\n", __FUNCTION__, certErr);
         ret = 1;
         break;
      default:
         g_warning("%s: error %d treated as failure\n", __FUNCTION__, certErr);
         break;
      }
   }

   return ret;
}


/*
 ******************************************************************************
 * CertVerify_StripPEMCert --                                            */ /**
 *
 * Cleans off any leading or trailing delimiters from a PEM certificate.
 * The resulting string needs to be g_free()d.
 *
 * Assumes the data is in the openssl form, but allows for some fudge
 * factor in the way the '---' are handled in case of hand-editing.
 * This may be overkill, but since we're currently thinking people can
 * hand-edit things, and its not that much harder, lets try it.
 * Of course, if we get a test case that tries to do this, I'm sure
 * they can beat it if they try hard enough.
 *
 * @param[in]   pemCert     The pemCert to clean.
 *
 * @return The cleaned PEM cert.
 *
 ******************************************************************************
 */

gchar *
CertVerify_StripPEMCert(const gchar *pemCert)
{
   gchar *result = g_strdup(pemCert);
   gchar *b;
   gchar *e;
   gsize len;

   /*
    * Find the the -----END CERTIFICATE----- or a variant.
    */
   e = g_strrstr(result, "\n--");
   if (NULL != e) {
      *(e+1) = '\0';
   }

   /*
    * Find the the -----BEGIN CERTIFICATE----- or a variant.
    */
   b = g_strstr_len(result, strlen(result), "--\n");
   if (NULL != b) {
      b += 3;
      len = strlen(b) + 1;
      memmove(result, b, len);
   }

   return result;
}


static gchar *sslCertHeader = "-----BEGIN CERTIFICATE-----\n";
static gchar *sslCertFooter = "-----END CERTIFICATE-----\n";


/*
 ******************************************************************************
 * CertVerify_EncodePEMForSSL --                                         */ /**
 *
 * OpenSSL is absurdly picky about PEM.  It must have the proper header
 * and footer.  It also insists on having newlines every 64 chars.
 * When we pull the PEM out of something like a SAML token, its
 * not good enough, so this code will convert base64 to something SSL
 * likes.
 *
 * @param[in]   pemCert     The pemCert to clean.
 *
 * @return The OpenSSL-safe PEM cert.
 *
 ******************************************************************************
 */

gchar *
CertVerify_EncodePEMForSSL(const gchar *pemCert)
{
   gchar *tmpCertStr = NULL;
   gchar *result;
   char *t;
   guchar *binCert;
   gsize len;
   int cnt;
   gsize strLen;
   gchar *cleanCertStr = NULL;

   /*
    * Make sure its just base64 data.
    */
   tmpCertStr = CertVerify_StripPEMCert(pemCert);

   /*
    * Decode
    */
   binCert = g_base64_decode(tmpCertStr, &len);
   g_free(tmpCertStr);

   /*
    * Now re-encode -- this way we flush any whitespace out of the original.
    */
   cleanCertStr = g_base64_encode(binCert, len);

   /*
    * rebuild, with a newline every 64 chars.
    */
   len = strlen(cleanCertStr);
   strLen = len +
      strlen(sslCertHeader) +
      strlen(sslCertFooter) +
      len/64 +                   // newline every 64 chars
      1 +                        // +1 for any leftover
      1;                         // final NUL

   result = g_malloc0(strLen);

   /*
    * Now rebuild it, with the SSL wrapper and newlines every 64 chars.
    */
   memcpy(result, sslCertHeader, strlen(sslCertHeader));
   tmpCertStr = result + strlen(sslCertHeader);

   t = cleanCertStr;
   cnt = 0;
   while (*t) {
      *tmpCertStr++ = *t++;
      if (++cnt == 64) {
         *tmpCertStr++ = '\n';
         cnt = 0;
      }
   }
   if (cnt != 0) {      // don't add an double newline
      *tmpCertStr++ = '\n';
   }
   memcpy(tmpCertStr, sslCertFooter, strlen(sslCertFooter));

   g_free(cleanCertStr);
   g_free(binCert);

   return result;
}


/*
 ******************************************************************************
 * CertStringToX509 --                                                   */ /**
 *
 * Creates an openssl x509 object from a pemCert string.
 *
 * @param[in]  pemCert      The certificate in PEM format.
 *
 * @return a new X509 object containing the cert.
 *
 ******************************************************************************
 */

static X509 *
CertStringToX509(const char *pemCert)
{
   BIO *bio;
   X509 *newCert = NULL;
   char *sslCertStr = NULL;
   gsize len;

   ASSERT(pemCert);

   /*
    * Don't blow up if fed junk.
    */
   len = strlen(pemCert);
   if (len < strlen(sslCertHeader)) {
      return NULL;
   }

   /*
    * XXX This assums that if the header is there, the footer is too.
    * It'd safer but more wasteful to always force it to be added.
    */
   if (memcmp(sslCertHeader, pemCert, strlen(sslCertHeader)) != 0) {
      sslCertStr = CertVerify_EncodePEMForSSL(pemCert);
   } else {
      sslCertStr = g_strdup(pemCert);
   }

   // create a BIO from the string
   bio = BIO_new_mem_buf((void *) sslCertStr, -1);
   if (NULL == bio) {
      VerifyDumpSSLErrors();
      g_warning("%s: unable to convert string to BIO\n", __FUNCTION__);
      goto done;
   }

   // read the x509 object from it
   newCert = PEM_read_bio_X509(bio, NULL, 0, NULL);
   if (NULL == newCert) {
      VerifyDumpSSLErrors();
      g_warning("%s: unable to convert string to x509\n", __FUNCTION__);
   }

   // make sure the data isn't free()d with the BIO
   // XXX is this necessary?
//   BIO_set_close(bio, BIO_NOCLOSE);
   BIO_free(bio);

done:

   g_free(sslCertStr);

   return newCert;
}


/*
 ******************************************************************************
 * CertVerifyX509ToString --                                             */ /**
 *
 * Debug support for X509 certs; convert them to human readable text.
 *
 * @param[in]  x  An X509 structure containing a cert.
 *
 * @return Allocated string containing the cert in human-readable text.
 *
 ******************************************************************************
 */

static gchar *
CertVerifyX509ToString(X509 *x)
{
   BIO *mem;
   char *str;
   gchar *retVal;
   int len;

   mem = BIO_new(BIO_s_mem());
   if (!mem) {
      g_warning("%s: out of memory creating BIO\n", __FUNCTION__);
      return NULL;
   }

   X509_print(mem, x);

   len = BIO_get_mem_data(mem, &str);

   retVal = g_strndup(str, len);

   BIO_set_close(mem, BIO_CLOSE);
   BIO_free(mem);

   return retVal;
}


/*
 ******************************************************************************
 * CertVerify_CertToX509String --                                        */ /**
 *
 * Debug support for certs; convert them to human readable text.
 *
 * @param[in]  pemCert A certficate in PEM format.
 *
 * @return Allocated string containing the cert in human-readable text.
 *
 ******************************************************************************
 */

gchar *
CertVerify_CertToX509String(const gchar *pemCert)
{
   X509 *x = NULL;
   gchar *retVal = NULL;

   x = CertStringToX509(pemCert);
   if (x) {
      retVal = CertVerifyX509ToString(x);
   }
   X509_free(x);

   return retVal;
}


/*
 ******************************************************************************
 * CertVerify_IsWellFormedPEMCert --                                     */ /**
 *
 * Checks to see if a PEM cert string can be converted into a x509
 * object.  Note that it does not verify the contents of the cert for
 * proper contents, expiration, revocation, etc; just
 * if the string can be converted into an x509 cert.
 *
 * @param[in]  pemCert      The certificate in PEM format.
 *
 * @return TRUE if the string contains a PEM cert.
 *
 ******************************************************************************
 */

gboolean
CertVerify_IsWellFormedPEMCert(const char *pemCert)
{
   X509 *x509Cert;

   if (NULL == pemCert) {
      return FALSE;
   }

   x509Cert = CertStringToX509(pemCert);
   if (NULL == x509Cert) {
      return FALSE;
   }
   X509_free(x509Cert);
   return TRUE;
}


#if 0
/*
 * ssl stack test code.
 *
 * ssl stacks act as expected, but the terminology in the docs is
 * confusing.  'pop' is doc'd to return the last element on the
 * stack, but it does do the expected LIFO.
 * 'shift' claims it returns the first element, but it actually
 * returns the first thing added.
 *
 * this bit of code helped make sense of it, so leaving behind
 * as a framework to experiment with other stack operations.
 */
static void
VerifyTstSSLStacks(void)
{
   STACK_OF(ASN1_INTEGER) *istack;
   ASN1_INTEGER *aint;

   istack = sk_ASN1_INTEGER_new_null();
   for (i = 0; i < 10; i++) {
      aint = ASN1_INTEGER_new();
      ASN1_INTEGER_set(aint, i);
      sk_ASN1_INTEGER_push(istack, aint);
   }

   // 'shift' does FIFO
   while ((aint = sk_ASN1_INTEGER_shift(istack)) != NULL) {
      printf("shifted value: %ld\n", ASN1_INTEGER_get(aint));
   }
   // 'pop' does LIFO
   while ((aint = sk_ASN1_INTEGER_pop(istack)) != NULL) {
      printf("popped value: %ld\n", ASN1_INTEGER_get(aint));
   }
}
#endif


/*
 ******************************************************************************
 * PEMChainToStack --                                                    */ /**
 *
 * Converts an array of PEM certificates into a stack of x509 objects.
 *
 * @param[in]  numCerts      The number of certs to convert.
 * @param[in]  pemCerts      The certs.
 * @param[out] newChain      The resulting chain.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure.
 *
 ******************************************************************************
 */

static VGAuthError
PEMChainToStack(int numCerts,
                const char **pemCerts,
                STACK_OF(X509) **newChain)
{
   STACK_OF(X509) *chain = NULL;
   int i;
   X509 *x509Cert;
   VGAuthError err = VGAUTH_E_OK;

   *newChain = NULL;
   if (numCerts > 0) {
      chain = sk_X509_new_null();
      if (NULL == chain) {
         err = VGAUTH_E_FAIL;
         g_warning("%s: failed to create X509 stack\n", __FUNCTION__);
         goto done;
      }

      for (i = 0; i < numCerts; i++) {
         x509Cert = CertStringToX509(pemCerts[i]);
         if (NULL == x509Cert) {
            err = VGAUTH_E_INVALID_CERTIFICATE;
            g_warning("%s: failed to convert PEM cert to X509\n", __FUNCTION__);
            goto done;
         }

         if (0 == sk_X509_push(chain, x509Cert)) {
            VerifyDumpSSLErrors();
            err = VGAUTH_E_FAIL;
            X509_free(x509Cert);
            g_warning("%s: failed to add cert to stack\n", __FUNCTION__);
            goto done;
         }
      }
      ASSERT(sk_X509_num(chain) == numCerts);
   }
   *newChain = chain;

done:
   if (VGAUTH_E_OK != err) {
      sk_X509_pop_free(chain, X509_free);
   }

   return err;
}


/*
 ******************************************************************************
 * CertVerify_CertChain --                                               */ /**
 *
 * @brief Verifies a complete certificate chain.
 *
 * Verifies that all certs are properly signed, in the proper date range, etc.
 * The pemLeafCert is the cert being validated.
 * The pemUntrustedCertChain contains the certs passed in which are
 * not trusted (eg, those not found in the certstore).
 * The pemTrustedCerts contains all certificates that are in the certstore.
 *
 * @param[in]  pemLeafCert              The leaf cert in PEM format.
 * @param[in]  numUntrustedCerts        The size of the untrusted chain.
 * @param[in]  pemUntrustedCertChain    The chain of untrusted certificates.
 * @param[in]  numTrustedCerts          The size of the trusted chain.
 * @param[in]  pemTrustedCertChain      The chain of trusted certificates.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
CertVerify_CertChain(const char *pemLeafCert,
                     int numUntrustedCerts,
                     const char **pemUntrustedCertChain,
                     int numTrustedCerts,
                     const char **pemTrustedCertChain)
{
   VGAuthError err = VGAUTH_E_OK;
   int ret;
   STACK_OF(X509) *trustedChain = NULL;
   STACK_OF(X509) *untrustedChain = NULL;
   X509_STORE *store = NULL;
   X509_STORE_CTX *verifyCtx = NULL;
   X509 *leafCert;


   /*
    * Turn the leaf cert into an x509 object.
    */
   leafCert = CertStringToX509(pemLeafCert);
   if (NULL == leafCert) {
      err = VGAUTH_E_INVALID_CERTIFICATE;
      g_warning("%s: failed to convert PEM cert to X509\n", __FUNCTION__);
      goto done;
   }

   err = PEMChainToStack(numUntrustedCerts,
                         pemUntrustedCertChain,
                         &untrustedChain);
   if (VGAUTH_E_OK != err) {
      g_warning("%s: failed to convert untrusted chain\n", __FUNCTION__);
      goto done;
   }
   err = PEMChainToStack(numTrustedCerts,
                         pemTrustedCertChain,
                         &trustedChain);
   if (VGAUTH_E_OK != err) {
      g_warning("%s: failed to convert trusted chain\n", __FUNCTION__);
      goto done;
   }

   /*
    * Build X509 store.
    */
   store = X509_STORE_new();
   if (NULL == store) {
      err = VGAUTH_E_FAIL;
      VerifyDumpSSLErrors();
      g_warning("%s: unable to create x509 store\n", __FUNCTION__);
      goto done;
   }

   /*
    * Set the callback.
    *
    * XXX OpenSSL v1.0 has X509_STORE_set_verify_cb()
    */
   X509_STORE_set_verify_cb_func(store, VerifyCallback);

   /*
    * Do the verification.
    *
    * Note that the verify code is not happy seeing a self-signed (CA)
    * cert in the untrusted list.
    */
   verifyCtx = X509_STORE_CTX_new();
   if (NULL == verifyCtx) {
      err = VGAUTH_E_FAIL;
      VerifyDumpSSLErrors();
      g_warning("%s: unable to create x509 store context\n", __FUNCTION__);
      goto done;
   }

   /*
    * Set up the verify to look at leafCert.  We pass no additional
    * untrusted certs.
    */
   ret = X509_STORE_CTX_init(verifyCtx, store, leafCert, untrustedChain);
   if (1 != ret) {
      err = VGAUTH_E_FAIL;
      VerifyDumpSSLErrors();
      g_warning("%s: unable to init x509 store context\n", __FUNCTION__);
      goto done;
   }

   /*
    * Set the trusted list.  Anything self-signed needs to be here.
    */
   X509_STORE_CTX_trusted_stack(verifyCtx, trustedChain);

   /*
    * XXX
    *
    * Add CRLs
    */


   /*
    * And now check it
    */
   ret = X509_verify_cert(verifyCtx);
   if (ret <= 0) {
      err = VGAUTH_E_INVALID_CERTIFICATE;
      VerifyDumpSSLErrors();
      g_warning("%s: unable to verify x509 certificate (ret = %d)\n", __FUNCTION__, ret);
      goto done;
   }

done:
   sk_X509_pop_free(trustedChain, X509_free);
   sk_X509_pop_free(untrustedChain, X509_free);
   X509_free(leafCert);

   /*
    * SSL 'free' code doesn't seem to believe in doing NULL checks
    */
   if (verifyCtx) {
      X509_STORE_CTX_free(verifyCtx);
   }
   if (store) {
      X509_STORE_free(store);
   }

   return err;
}


/*
 ******************************************************************************
 * CertVerify_CheckSignatureUsingCert --                                 */ /**
 *
 * @brief Verifies the signature of binary data.
 *
 * Verifies that 'data' has been correctly signed using the private key
 * associated with the public key in the certificate given by pemCert.
 *
 * Does not make any checks on the validity of the certificate.
 *
 * @param[in] hash         Identifies the hash algorithm to used when computing
 *                         the signature.
 * @param[in] pemCert      The certificate containing the public key to use
 *                         when validating the signature.
 * @param[in] dataLen      The length of 'data' in bytes.
 * @param[in] data         The data that has been signed.
 * @param[in] signatureLen The length of 'signature' in bytes.
 * @param[in] signature    The signature of data.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
CertVerify_CheckSignatureUsingCert(VGAuthHashAlg hash,
                                   const char *pemCert,
                                   size_t dataLen,
                                   const unsigned char *data,
                                   size_t signatureLen,
                                   const unsigned char *signature)
{
   VGAuthError err;
   X509 *cert;
   X509_PUBKEY *x509PubKey;
   EVP_PKEY *publicKey;

   cert = CertStringToX509(pemCert);
   if (NULL == cert) {
      err = VGAUTH_E_INVALID_CERTIFICATE;
      g_warning("%s: failed to convert PEM cert to X509.\n", __FUNCTION__);
      goto done;
   }

   x509PubKey = X509_get_X509_PUBKEY(cert);
   publicKey = X509_PUBKEY_get(x509PubKey);
   if (NULL == publicKey) {
      VerifyDumpSSLErrors();
      g_warning("%s: unable to get the public key from the cert.\n", __FUNCTION__);
      err = VGAUTH_E_FAIL;
      goto done;
   }

   err = CertVerify_CheckSignature(hash, publicKey, dataLen, data,
                                   signatureLen, signature);
   EVP_PKEY_free(publicKey);

done:
   X509_free(cert);

   return err;
}


/*
 ******************************************************************************
 * CertVerify_CheckSignature --                                          */ /**
 *
 * @brief Verifies the signature of binary data.
 *
 * Verifies that 'data' has been correctly signed using the private key
 * associated with 'publicKey'.
 *
 * @param[in] hash         Identifies the hash algorithm to used when computing
 *                         the signature.
 * @param[in] pemCert      The public key to use when validating the signature.
 * @param[in] dataLen      The length of 'data' in bytes.
 * @param[in] data         The data that has been signed.
 * @param[in] signatureLen The length of 'signature' in bytes.
 * @param[in] signature    The signature of data.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
CertVerify_CheckSignature(VGAuthHashAlg hash,
                          EVP_PKEY *publicKey,
                          size_t dataLen,
                          const unsigned char *data,
                          size_t signatureLen,
                          const unsigned char *signature)
{
   VGAuthError err = VGAUTH_E_FAIL;
   EVP_MD_CTX *mdCtx = NULL;
   const EVP_MD *hashAlg;
   int ret;

   mdCtx = EVP_MD_CTX_new();
   if (mdCtx == NULL) {
      g_warning("%s: unable to allocate a message digest.\n", __FUNCTION__);
      return(VGAUTH_E_OUT_OF_MEMORY);
   }

   switch (hash) {
   case VGAUTH_HASH_ALG_SHA256:
      hashAlg = EVP_sha256();
      break;
   default:
      g_warning("%s: unrecognized hash algorithm %d.\n", __FUNCTION__, hash);
      err = VGAUTH_E_INVALID_ARGUMENT;
      goto done;
   }

   ret = EVP_VerifyInit(mdCtx, hashAlg);
   if (ret <= 0) {
      VerifyDumpSSLErrors();
      g_warning("%s: unable to initialize verificatation context (ret = %d)\n",
                __FUNCTION__, ret);
      goto done;
   }

   /*
    * Since we are synchronous, just compute the hash over all the data in
    * one shot. We probably should put some upper bound on the size of the
    * data.
    */
   ret = EVP_VerifyUpdate(mdCtx, data, dataLen);
   if (ret <= 0) {
      VerifyDumpSSLErrors();
      g_warning("%s: unable to update verificatation context (ret = %d)\n",
                __FUNCTION__, ret);
      goto done;
   }

   ret = EVP_VerifyFinal(mdCtx, signature, (unsigned int) signatureLen, publicKey);
   if (0 == ret) {
      g_debug("%s: verification failed!\n", __FUNCTION__);
      err = VGAUTH_E_AUTHENTICATION_DENIED;
      goto done;
   } else if (ret < 0) {
      VerifyDumpSSLErrors();
      g_warning("%s: error while verifying signature (ret = %d)\n",
                __FUNCTION__, ret);
      goto done;
   }

   err = VGAUTH_E_OK;

done:
   EVP_MD_CTX_free(mdCtx);

   return err;
}
