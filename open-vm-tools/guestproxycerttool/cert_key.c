/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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
 * certKey.c --
 *
 *    Utilities to handle key and certificate generation.
 */

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/rand.h>
#include <openssl/safestack.h>
#include <openssl/evp.h>
#include <string.h>
#include <errno.h>
#include "cert_util.h"
#include "cert_key.h"

#define BSIZE 4096

/*
 *----------------------------------------------------------------------
 *
 * GetSSLError --
 *
 *    Get the latest human readable error message from SSL library.
 *
 * Results:
 *    Return the error message. Callers should free the returned
 *    error message.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gchar *
GetSSLError(gchar **errorStr)                    // OUT
{
   unsigned long code;
   GString *str = g_string_new(NULL);
   gboolean first = TRUE;
   char buf[BSIZE];

   while ((code = ERR_get_error()) != 0) {
      ERR_error_string_n(code, buf, sizeof buf);
      g_string_append_printf(str, "%s%s", first ? "" : ", ", buf);
      first = FALSE;
   }

   *errorStr = g_string_free(str, FALSE);

   return *errorStr;
}


/*
 *----------------------------------------------------------------------
 *
 * CertKey_InitOpenSSLLib --
 *
 *    Initialize OpenSSL for key and certificate generation.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

void
CertKey_InitOpenSSLLib(void)
{
   ERR_load_crypto_strings();
   OpenSSL_add_all_digests();
}


/*
 *----------------------------------------------------------------------
 *
 * CertKey_ComputeCertPemFileHash --
 *
 *    Compute the certificate subject name hash.
 *
 * Results:
 *    Return the computed hash string. Callers should free the returned
 *    string.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gchar *
CertKey_ComputeCertPemFileHash(const gchar *certPemFile) // IN
{
   FILE *file;
   gchar *hash = NULL;
   X509 *cert = NULL;
   gchar *err = NULL;

   file = g_fopen(certPemFile, "r");
   if (!file) {
      Error("Failed to open %s: %s.\n", certPemFile, strerror(errno));
      goto exit;
   }

   cert = PEM_read_X509(file, NULL, NULL, NULL);
   if (!cert) {
      Error("Error reading certificate file %s: %s.\n",
            certPemFile, GetSSLError(&err));
      goto exit;
   }

   hash = g_strdup_printf("%08lx", X509_subject_name_hash(cert));

exit:
   if (file) {
      fclose(file);
   }
   X509_free(cert);
   g_free(err);

   return hash;
}


/*
 *----------------------------------------------------------------------
 *
 * SetCertSerialNumber --
 *
 *    Set the certificate serial number.
 *
 * Results:
 *    Return TRUE if success, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
SetCertSerialNumber(X509 *cert)                  // IN
{
   BIGNUM *btmp = NULL;
   ASN1_INTEGER *sno;
   gboolean ret = FALSE;
   gchar *err = NULL;

   sno = ASN1_INTEGER_new();
   if (!sno) {
      Error("Failed to allocate an ASN1 integer.\n");
      goto exit;
   }

   btmp = BN_new();
   if (!btmp) {
      Error("Failed to allocate a BIGNUM structure.\n");
      goto exit;
   }

   if (!BN_rand(btmp, 64, 0, 0)) {
      Error("Failed to generate random number: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   if (!BN_to_ASN1_INTEGER(btmp, sno)) {
      Error("Failed to convert from BIGNUM to ASN1_INTEGER: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   if (!X509_set_serialNumber(cert, sno)) {
      Error("Failed to set the certificate serial number: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   ret = TRUE;

exit:
   BN_free(btmp);
   ASN1_INTEGER_free(sno);
   g_free(err);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * LoadOpenSSLConf --
 *
 *    Loading the OpenSSL configuration file.
 *
 * Results:
 *    Return the configuration structure if success, otherwise NULL.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static CONF *
LoadOpenSSLConf(const gchar *fname)              // IN
{
   CONF *config;
   const char *mask;
   gboolean ret = FALSE;
   gchar *err = NULL;

   config = NCONF_new(NULL);
   if (!config) {
      Error("Failed to allocate the OpenSSL config.\n");
      goto exit;
   }

   if (!NCONF_load(config, fname, NULL)) {
      Error("Failed to load the configuration file %s: %s.\n",
            fname, GetSSLError(&err));
      goto exit;
   }

   OPENSSL_load_builtin_modules();

   if (!CONF_modules_load(config, NULL, 0)) {
      Error("Error configuring OpenSSL modules: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   mask = NCONF_get_string(config, "req", "string_mask");
   if (mask) {
      ASN1_STRING_set_default_mask_asc(mask);
   }

   ret = TRUE;

exit:
   if (!ret) {
      NCONF_free(config);
      config = NULL;
   }
   g_free(err);

   return config;
}


/*
 *----------------------------------------------------------------------
 *
 * GenerateRSAKeyPair --
 *
 *    Interface to OpenSSL library to create RSA key pair.
 *
 * Results:
 *    Return the generated RSA key structure if success,
 *    otherwise NULL.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static RSA *
GenerateRSAKeyPair(int bits)                     // IN
{
   BIGNUM *bn;
   RSA *rsa = NULL;
   gchar *err = NULL;

   bn = BN_new();
   if (!bn) {
      Error("Failed to allocate a BIGNUM.\n");
      goto exit;
   }

   if (!BN_set_word(bn, RSA_F4)) {
      Error("Failed to assign a value to BIGNUM: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   rsa = RSA_new();
   if (!rsa) {
      Error("Failed to allocate a RSA structure.\n");
      goto exit;
   }

   if (!RSA_generate_key_ex(rsa, bits, bn, NULL)) {
      Error("Error generating RSA key pair: %s.\n",
            GetSSLError(&err));
      RSA_free(rsa);
      rsa = NULL;
   }

exit:
   BN_free(bn);
   g_free(err);

   return rsa;
}


/*
 *----------------------------------------------------------------------
 *
 * GenerateRSAPrivateKey --
 *
 *    Create the RSA private key structure.
 *
 * Results:
 *    Return the generated RSA private key structure if success,
 *    otherwise NULL.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static EVP_PKEY *
GenerateRSAPrivateKey(int bits)                  // IN
{
   RSA *rsa;
   EVP_PKEY *pkey = NULL;

   rsa = GenerateRSAKeyPair(bits);
   if (!rsa) {
      goto exit;
   }

   pkey = EVP_PKEY_new();
   if (!pkey) {
      Error("Failed to allocate a private key structure.\n");
      goto exit;
   }

   EVP_PKEY_assign_RSA(pkey, rsa);
   rsa = NULL;

exit:
   RSA_free(rsa);

   return pkey;
}


/*
 *----------------------------------------------------------------------
 *
 * ConfigX509CertReq --
 *
 *    Configure the X509 certificate request.
 *
 * Results:
 *    Return TRUE if success, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
ConfigX509CertReq(X509_REQ *req,                 // OUT
                  CONF *config)                  // IN
{
   int idx;
   X509_NAME *subject;
   gboolean ret = FALSE;
   const char *dname;
   gchar *err = NULL;

   if (!X509_REQ_set_version(req, 0L)) {
      Error("Failed to set the certificate request version: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   subject = X509_REQ_get_subject_name(req);
   if (!subject) {
      Error("Failed to get the certificate request subject name: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   dname = NCONF_get_string(config, "req", "distinguished_name");
   if (dname) {
      STACK_OF(CONF_VALUE) *dn_sk = NCONF_get_section(config, dname);
      if (!dn_sk) {
         Error("Failed to get section %s: %s.\n",
               dname, GetSSLError(&err));
         goto exit;
      }

      for (idx = 0; idx < sk_CONF_VALUE_num(dn_sk); idx++) {
         CONF_VALUE *v = sk_CONF_VALUE_value(dn_sk, idx);

         if (!X509_NAME_add_entry_by_txt(subject, v->name, MBSTRING_ASC,
                                         v->value, -1, -1, 0)) {
            Error("Failed to set certificate request pair %s/%s: %s.\n",
                  v->name, v->value, GetSSLError(&err));
            goto exit;
         }
      }
   }

   ret = TRUE;

exit:
   g_free(err);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * GenerateX509CertReq --
 *
 *    Create X509 certificate request.
 *
 * Results:
 *    Return the generated X509 request if success, otherwise NULL.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static X509_REQ *
GenerateX509CertReq(EVP_PKEY **pkey,             // OUT
                    CONF *config,                // IN
                    int bits)                    // IN
{
   X509_REQ *req = NULL;
   gboolean ret = FALSE;
   gchar *err = NULL;

   *pkey = GenerateRSAPrivateKey(bits);
   if (!*pkey) {
      goto exit;
   }

   req = X509_REQ_new();
   if (!req) {
      Error("Failed to allocate a X509 certificate request.\n");
      goto exit;
   }

   if (!ConfigX509CertReq(req, config)) {
      goto exit;
   }

   if (!X509_REQ_set_pubkey(req, *pkey)) {
      Error("Failed to set certificate request public key: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   ret = TRUE;

exit:
   if (!ret) {
      X509_REQ_free(req);
      req = NULL;
   }
   g_free(err);

   return req;
}


/*
 *----------------------------------------------------------------------
 *
 * GenerateX509Cert --
 *
 *    Generate a X509 certificate.
 *
 * Results:
 *    Return a X509 structure if success, otherwise NULL.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static X509 *
GenerateX509Cert(X509_REQ *req,                  // IN
                 CONF *config,                   // IN
                 int days)                       // IN
{
   X509 *cert;
   X509V3_CTX ctx;
   char *extensions;
   gboolean ret = FALSE;
   gchar *err = NULL;

   cert = X509_new();
   if (!cert) {
      Error("Failed to allocate a X509 certificate: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   if (!SetCertSerialNumber(cert)) {
      goto exit;
   }

   if (!X509_set_issuer_name(cert, X509_REQ_get_subject_name(req))    ||
       !X509_gmtime_adj(X509_get_notBefore(cert), 0)                  ||
       !X509_gmtime_adj(X509_get_notAfter(cert), (long)60*60*24*days) ||
       !X509_set_subject_name(cert, X509_REQ_get_subject_name(req))) {
      Error("Failed to configure the X509 certificate: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
   X509V3_set_nconf(&ctx, config);

   extensions = NCONF_get_string(config, "req", "x509_extensions");
   if (extensions) {
      if (!X509_set_version(cert, 2)) {
         Error("Failed to set the certificate version: %s.\n",
               GetSSLError(&err));
         goto exit;
      }

      if (!X509V3_EXT_add_nconf(config, &ctx, extensions, cert)) {
         Error("Error loading extension section %s: %s.\n",
               extensions, GetSSLError(&err));
         goto exit;
      }
   }

   ret = TRUE;

exit:
   if (!ret) {
      X509_free(cert);
      cert = NULL;
   }
   g_free(err);

   return cert;
}


#ifndef _WIN32
/*
 *----------------------------------------------------------------------
 *
 * WritePemFile --
 *
 *    Output RSA private key and X509 certificate in PEM format.
 *
 * Results:
 *    TRUE if success, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gboolean
WritePemFile(EVP_PKEY *pkey,                     // IN
             const gchar *keyFile,               // IN
             X509 *cert,                         // IN
             const gchar *certFile)              // IN
{
   FILE *file;
   gboolean ret = FALSE;
   gchar *err = NULL;
   mode_t mode;

   mode = umask(066);
   file = g_fopen(keyFile, "w");
   if (!file) {
      Error("Failed to open %s: %s.\n", keyFile, strerror(errno));
      goto exit;
   }

   if (!PEM_write_PrivateKey(file, pkey, NULL, NULL, 0, NULL, NULL)) {
      Error("Failed to write the private key file %s: %s.\n",
            keyFile, GetSSLError(&err));
      goto exit;
   }

   fclose(file);

   umask(022);
   file = g_fopen(certFile, "w");
   if (!file) {
      Error("Failed to open %s: %s.\n", certFile, strerror(errno));
      goto exit;
   }

   if (!PEM_write_X509(file, cert)) {
      Error("Failed to write the certificate file %s: %s.\n",
            certFile, GetSSLError(&err));
      goto exit;
   }

   ret = TRUE;

exit:
   if (file) {
      fclose(file);
   }
   g_free(err);
   umask(mode);

   return ret;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * CertKey_GenerateKeyCert --
 *
 *    Generate the server key and certificate files.
 *
 * Results:
 *    TRUE if success, otherwise FALSE. When success, key and
 *    certificate files are generated.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gboolean
CertKey_GenerateKeyCert(int bits,                 // IN
                        int days,                 // IN
                        const gchar *confFile,    // IN
                        const gchar *keyFile,     // IN
                        const gchar *certFile)    // IN
{
   gboolean ret = FALSE;
   X509 *cert = NULL;
   X509_REQ *req = NULL;
   EVP_PKEY *pkey = NULL;
   CONF *config;
   gchar *err = NULL;

   config = LoadOpenSSLConf(confFile);
   if (!config) {
      goto exit;
   }

   req = GenerateX509CertReq(&pkey, config, bits);
   if (!req) {
      goto exit;
   }

   cert = GenerateX509Cert(req, config, days);
   if (!cert) {
      goto exit;
   }

   if (!X509_set_pubkey(cert, pkey)) {
      Error("Failed to set certificate public key: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   if (!X509_sign(cert, pkey, EVP_sha256())) {
      Error("Failed to sign the X509 certificate: %s.\n",
            GetSSLError(&err));
      goto exit;
   }

   /*
    * Write private key and certificate PEM files.
    */
   if (WritePemFile(pkey, keyFile, cert, certFile)) {
      ret = TRUE;
   }

exit:
   g_free(err);
   NCONF_free(config);
   EVP_PKEY_free(pkey);
   X509_REQ_free(req);
   X509_free(cert);

   return ret;
}
