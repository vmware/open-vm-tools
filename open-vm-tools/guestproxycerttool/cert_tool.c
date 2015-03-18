/*********************************************************
 * Copyright (C) 2014-2015 VMware, Inc. All rights reserved.
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
 * guestproxycerttool.c --
 *
 *    Utility to manage the certificates for 'rabbitmqproxy'
 *    plugin in 'VMware Tools'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include "cert_util.h"
#include "cert_key.h"
#include "cert_tool_version.h"

#include "vm_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(GUESTPROXYCERTTOOL_VERSION_STRING);

/*
 * The following variables are set up during the validation of system
 * environment. They are initialized only once and are not freed until
 * the program exits.
 */
static gboolean  initialized = FALSE;
static gchar    *guestProxyDir;
static gchar    *guestProxyServerDir;
static gchar    *guestProxyTrustedDir;
static gchar    *guestProxySslConf;

#define RSA_KEY_LENGTH       2048
#define CERT_EXPIRED_IN_DAYS (365 * 10)

/*
 *----------------------------------------------------------------------
 *
 * PrintUsage --
 *
 *    Print command usage information.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static void
PrintUsage(const gchar *cmd)                      // IN
{
   fprintf(stderr, "Guest Proxy Certificate Management Tool.\n");
   fprintf(stderr, "Usage: %s [OPTION] [ARGUMENTS]\n\n", cmd);
   fprintf(stderr, "Options\n"
           "  -h, --help                Prints the Usage information.\n\n"
           "  -g, --generate_key_cert   Regenerate the server key/cert, the old\n"
           "                            key/cert will be replaced.\n\n"
           "  -a, --add_trust_cert      <client_cert_pem_file>\n"
           "                            Adds the client cert to the trusted\n"
           "                            certificates directory.\n\n"
           "  -r, --remove_trust_cert   <client_cert_pem_file>\n"
           "                            Remove the client cert from the trusted\n"
           "                            certificates directory.\n\n"
           "  -d, --display_server_cert [<cert_pem_file>]\n"
           "                            Prints the server's certificate to the\n"
           "                            standard output. If the file path is\n"
           "                            specified, then the server's certificate\n"
           "                            is stored in the file.\n\n");
}


/*
 *----------------------------------------------------------------------
 *
 * ValidateEnvironment --
 *
 *    Initialize command and directory paths, which are used to hold
 *    key and certificate files, and find commands to operate on them.
 *
 * Results:
 *    TRUE if successfully find all the paths. Otherwise FALSE.
 *
 * Side effects:
 *    Path global variables are set accordingly and initialized is set
 *    to true so it is only initialized once.
 *
 *----------------------------------------------------------------------
 */

static gboolean
ValidateEnvironment(gboolean requireRootPriv)     // IN
{
   gboolean ret = FALSE;

   if (requireRootPriv && geteuid() != 0) {
      Error("Please re-run this program as the super user to execute "
            "this operation.\n");
      goto exit;
   }

   ret = initialized;
   if (ret) {
      goto exit;
   }

   CertKey_InitOpenSSLLib();

   guestProxyDir = g_build_filename(CertUtil_GetToolDir(),
                                    "GuestProxyData", NULL);
   if (!g_file_test(guestProxyDir, G_FILE_TEST_IS_DIR)) {
      Error("Couldn't find the GuestProxy Directory at '%s'.\n",
            guestProxyDir);
      goto exit;
   }

   guestProxyServerDir = g_build_filename(guestProxyDir, "server", NULL);
   if (!g_file_test(guestProxyServerDir, G_FILE_TEST_IS_DIR)) {
      Error("Couldn't find the GuestProxy Certificate Directory at '%s'.\n",
            guestProxyServerDir);
      goto exit;
   }

   guestProxyTrustedDir = g_build_filename(guestProxyDir, "trusted", NULL);
   if (!g_file_test(guestProxyTrustedDir, G_FILE_TEST_IS_DIR)) {
      Error("Couldn't find the GuestProxy Certificate Store at '%s'.\n",
            guestProxyTrustedDir);
      goto exit;
   }

   guestProxySslConf = g_build_filename(CertUtil_GetToolDir(),
                                        "guestproxy-ssl.conf", NULL);
   if (!g_file_test(guestProxySslConf, G_FILE_TEST_IS_REGULAR)) {
      Error("Couldn't find the GuestProxy Config file at '%s'.\n",
            guestProxySslConf);
      goto exit;
   }

   initialized = ret = TRUE;

exit:
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * AddTrustCert --
 *
 *    Add the supplied certificate file (clientCertPemFile) to the
 *    trusted certificate directory.
 *
 * Results:
 *    TRUE if success, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
AddTrustCert(const gchar *clientCertPemFile)      // IN
{
   gboolean ret = FALSE;
   int last, num;
   gchar *hash = NULL;
   gchar *path = NULL;

   if (!ValidateEnvironment(TRUE)) {
      goto exit;
   }

   if (!g_file_test(clientCertPemFile, G_FILE_TEST_IS_REGULAR)) {
      Error("No certificate file found at %s.\n", clientCertPemFile);
      goto exit;
   }

   hash = CertKey_ComputeCertPemFileHash(clientCertPemFile);
   if (!hash) {
      goto exit;
   }

   if (CertUtil_FindCert(clientCertPemFile, guestProxyTrustedDir, hash,
                        &num, &last) && num >= 0) {
      Error("The specified certificate file already exists: %s.%d.\n",
            hash, num);
      goto exit;
   }

   path = CertUtil_CreateCertFileName(guestProxyTrustedDir, hash, last + 1);
   if (!CertUtil_CopyFile(clientCertPemFile, path)) {
      Error("Unable to add %s to the trusted certificate store.\n",
            clientCertPemFile);
      goto exit;
   }

   printf("Successfully added the %s to the trusted certificate store.\n",
          clientCertPemFile);
   ret = TRUE;

exit:
   g_free(hash);
   g_free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * DisplayServerCert --
 *
 *    Display the server certificate file to console. If a file name
 *    (serverCertPemFile) is supplied, write to that file instead.
 *
 * Results:
 *    TRUE if success, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
DisplayServerCert(const gchar *serverCertPemFile) // IN
{
   gboolean ret = FALSE;
   gchar *cert = NULL;
   FILE *file = NULL;
   GMappedFile *fmap = NULL;

   if (!ValidateEnvironment(FALSE)) {
      goto exit;
   }

   cert = g_build_filename(guestProxyServerDir, "cert.pem", NULL);
   if (!g_file_test(cert, G_FILE_TEST_IS_REGULAR)) {
      Error("Couldn't find the server certificate file: %s.\n", cert);
      goto exit;
   }

   if (serverCertPemFile && strlen(serverCertPemFile)) {
      printf("Copying the server certificate to %s.\n", serverCertPemFile);

      if (!CertUtil_CopyFile(cert, serverCertPemFile)) {
         Error("Failed to copy the certificate file to the file.\n");
         goto exit;
      }
      printf("Successfully copied the server certificate.\n");

   } else {

      fmap = g_mapped_file_new(cert, FALSE, NULL);
      if (fmap) {

         const gchar *content = g_mapped_file_get_contents(fmap);
         gsize length = g_mapped_file_get_length(fmap);

         if (fwrite(content, 1, length, stdout) < length) {
            Error("Failed to display %s: %s.\n", cert, strerror(errno));
            goto exit;
         }
      } else {
         Error("Couldn't open the server certificate file.\n");
         goto exit;
      }
   }

   ret = TRUE;

exit:
   g_free(cert);
   if (file) {
      fclose(file);
   }
   if (fmap) {
      g_mapped_file_unref(fmap);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * CreateKeyCert --
 *
 *    Create the server key and certificate files.
 *
 * Results:
 *    TRUE if success, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
CreateKeyCert(void)
{
   gboolean ret = FALSE;
   gchar *cert = NULL;
   gchar *key = NULL;

   if (!ValidateEnvironment(TRUE)) {
      goto exit;
   }

   key  = g_build_filename(guestProxyServerDir, "key.pem", NULL);
   cert = g_build_filename(guestProxyServerDir, "cert.pem", NULL);

   printf("Generating the key and certificate files.\n");

   if (!CertKey_GenerateKeyCert(RSA_KEY_LENGTH, CERT_EXPIRED_IN_DAYS,
                                guestProxySslConf, key, cert)) {
      goto exit;
   }

   ret = TRUE;
   printf("Successfully generated the key and certificate files.\n");

exit:
   g_free(key);
   g_free(cert);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * RemoveTrustCert --
 *
 *    Remove the specified certificate from the trusted certificate
 *    store.
 *
 * Results:
 *    TRUE if success, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
RemoveTrustCert(const gchar *clientCertPemFile)
{
   gboolean ret = FALSE;
   int last, num;
   gchar *hash = NULL;
   gchar *src = NULL;
   gchar *dst = NULL;

   if (!ValidateEnvironment(TRUE)) {
      goto exit;
   }

   if (!g_file_test(clientCertPemFile, G_FILE_TEST_IS_REGULAR)) {
      Error("No certificate file found at %s.\n", clientCertPemFile);
      goto exit;
   }

   hash = CertKey_ComputeCertPemFileHash(clientCertPemFile);
   if (!hash) {
      goto exit;
   }

   if (!CertUtil_FindCert(clientCertPemFile, guestProxyTrustedDir, hash,
                         &num, &last) || num < 0) {
      Error("Couldn't find any certificate in the trusted directory.\n");
      goto exit;
   }

   dst = CertUtil_CreateCertFileName(guestProxyTrustedDir, hash, num);
   if (last != num) {
      src = CertUtil_CreateCertFileName(guestProxyTrustedDir, hash, last);
      if (rename(src, dst) != 0) {
         Error("Failed to rename %s to %s with error: %s.",
               src, dst, strerror(errno));
         goto exit;
      }
   } else {
      if (unlink(dst) != 0) {
         Error("Failed to remove %s with error: %s.", dst, strerror(errno));
         goto exit;
      }
   }

   ret = TRUE;
   printf("Successfully removed the certificate.\n");

exit:
   g_free(hash);
   g_free(src);
   g_free(dst);

   return ret;
}


/*
 * Aggregation of command options.
 */
static struct _options {
   gchar        *addCert;
   gchar     *deleteCert;
   gchar     *outputCert;
   gboolean  displayCert;
   gboolean generateCert;
   gboolean        usage;
} options = { NULL, NULL, NULL, FALSE, FALSE, FALSE};


static gboolean
ParseDisplayCert(const gchar* name,
                 const gchar* value,
                 gpointer data,
                 GError **error)
{
   options.displayCert = TRUE;
   if (value) {
      options.outputCert = g_strdup(value);
   }

   return TRUE;
}


static GOptionEntry cmdOptions[] = {
   { "help",                'h', 0,
     G_OPTION_ARG_NONE,      &options.usage,        NULL, NULL },
   { "generate_key_cert",   'g', 0,
     G_OPTION_ARG_NONE,      &options.generateCert, NULL, NULL },
   { "add_trust_cert",      'a', 0,
     G_OPTION_ARG_FILENAME,  &options.addCert,      NULL, NULL },
   { "remove_trust_cert",   'r', 0,
     G_OPTION_ARG_FILENAME,  &options.deleteCert,   NULL, NULL },
   { "display_server_cert", 'd', G_OPTION_FLAG_OPTIONAL_ARG,
     G_OPTION_ARG_CALLBACK,  ParseDisplayCert,      NULL, NULL },
   { NULL }
};


/*
 *----------------------------------------------------------------------
 *
 * ParseOptions --
 *
 *    Parse command options.
 *
 * Results:
 *    Recognized options are saved into the options variable.
 *
 * Side effects:
 *    Both argc and argv are updated to remove those already
 *    recognized options.
 *
 *----------------------------------------------------------------------
 */

static void
ParseOptions(int *argc,                           // IN/OUT
             char ***argv)                        // IN/OUT
{
   GError *error = NULL;
   GOptionContext *context;

   context = g_option_context_new(NULL);
   g_option_context_add_main_entries(context, cmdOptions, NULL);
   /*
    * Turn off glib option parser help message in order to provide the
    * complete help messages compatible to those from the original perl
    * implementation script.
    */
   g_option_context_set_help_enabled(context, FALSE);

   if (!g_option_context_parse(context, argc, argv, &error)) {
      printf("Option parsing failed: %s\n", error->message);
      PrintUsage(g_get_prgname());
      exit(1);
   }

   g_option_context_free(context);
}

int
main(int argc, char **argv)
{
   ParseOptions(&argc, &argv);
   if (options.usage) {
      PrintUsage(g_get_prgname());
      exit(0);
   }

   if ((options.generateCert && !CreateKeyCert()) ||
       (options.displayCert && !DisplayServerCert(options.outputCert)) ||
       (options.addCert && !AddTrustCert(options.addCert)) ||
       (options.deleteCert && !RemoveTrustCert(options.deleteCert))) {
      return 1;
   } else {
      return 0;
   }
}
