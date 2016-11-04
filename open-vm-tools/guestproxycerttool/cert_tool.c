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
 * cert_tool.c --
 *
 *    Utility to manage the certificates for 'rabbitmqproxy'
 *    plugin in 'VMware Tools'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#include "cert_util.h"
#include "cert_key.h"
#include "cert_tool_version.h"
#ifdef _WIN32
#include "common_win.h"
#endif

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


gboolean gIsLogEnabled = FALSE;

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
           "  -h, --help                Prints the usage information.\n\n"
           "  -v, --verbose             Prints additional log messages.\n\n"
           "  -f, --force               Forces to regenerate the new server key/cert\n"
           "                            when used with -g.\n\n"
           "  -g, --generate_key_cert   Generates the server key/cert if they don't\n"
           "                            exist. Use with -f to force the regeneration.\n\n"
           "  -a, --add_trust_cert      <client_cert_pem_file>\n"
           "                            Adds the client cert to the trusted\n"
           "                            certificates directory.\n\n"
           "  -r, --remove_trust_cert   <client_cert_pem_file>\n"
           "                            Removes the client cert from the trusted\n"
           "                            certificates directory.\n\n"
           "  -d, --display_server_cert [<cert_pem_file>]\n"
           "                            Prints the server's certificate to the\n"
           "                            standard output. If the file path is\n"
           "                            specified, then the server's certificate\n"
           "                            is stored in the file.\n\n");
}


static void
InitProxyPaths(const gchar *toolDir)
{
   guestProxyDir        = g_build_filename(toolDir, "GuestProxyData", NULL);
   guestProxyServerDir  = g_build_filename(guestProxyDir, "server", NULL);
   guestProxyTrustedDir = g_build_filename(guestProxyDir, "trusted", NULL);
   guestProxySslConf    = g_build_filename(toolDir, "guestproxy-ssl.conf", NULL);
}


#ifndef _WIN32
/*
 *----------------------------------------------------------------------
 *
 * CheckRootPriv --
 *
 *    Check if the effect user id is root.
 *
 * Results:
 *    TRUE if it is, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gboolean
CheckRootPriv(void)
{
   if (geteuid() != 0) {
      Error("Please re-run this program as the super user to execute "
            "this operation.\n");
      return FALSE;
   }
   return TRUE;
}
#endif


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
 *    Path global variables are set accordingly and guest proxy data
 *    directories are created on-demand.
 *
 *----------------------------------------------------------------------
 */

static gboolean
ValidateEnvironment(gboolean requireRootPriv)     // IN
{
   gboolean ret = FALSE;

   if (requireRootPriv && !CheckRootPriv()) {
      Error("Current user has insufficient privileges.\n");
      goto exit;
   }

   if (!initialized) {
      CertKey_InitOpenSSLLib();
      initialized = TRUE;
   }

   if (!g_file_test(guestProxySslConf, G_FILE_TEST_IS_REGULAR)) {
      Error("Couldn't find the GuestProxy Config file at '%s'.\n",
            guestProxySslConf);
      goto exit;
   }

   /* Create guest proxy data directories on-demand */
   if (!g_file_test(guestProxyDir, G_FILE_TEST_IS_DIR)) {
      if (g_mkdir(guestProxyDir, 0755) < 0) {
         Error("Couldn't create the directory '%s'.\n", guestProxyDir);
         goto exit;
      }
   }

   if (!g_file_test(guestProxyServerDir, G_FILE_TEST_IS_DIR)) {
      if (g_mkdir(guestProxyServerDir, 0755) < 0) {
         Error("Couldn't create the directory '%s'.\n", guestProxyServerDir);
         goto exit;
      }
   }

   if (!g_file_test(guestProxyTrustedDir, G_FILE_TEST_IS_DIR)) {
      if (g_mkdir(guestProxyTrustedDir, 0700) < 0) {
         Error("Couldn't create the directory '%s'.\n", guestProxyTrustedDir);
         goto exit;
      }
   }

   ret = TRUE;

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
CreateKeyCert(gboolean force)
{
   gboolean ret = FALSE;
   gchar *cert = NULL;
   gchar *key = NULL;

   if (!ValidateEnvironment(TRUE)) {
      goto exit;
   }

   key  = g_build_filename(guestProxyServerDir, "key.pem", NULL);
   cert = g_build_filename(guestProxyServerDir, "cert.pem", NULL);

   /*
    * If both server key and certificate files already exist and the
    * program is not asked to create them by force, print an warning
    * message about server key/cert files not regenerating.
    */
   if (g_file_test(key, G_FILE_TEST_IS_REGULAR) &&
       g_file_test(cert, G_FILE_TEST_IS_REGULAR) && !force) {
      printf("\nNOTE: both %s and \n      %s already exist.\n"
             "      They are not generated again. To regenerate "
             "them by force,\n      use the \"%s -g -f\" command.\n\n",
             key, cert, g_get_prgname());
      ret = TRUE;
      goto exit;
   }

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
  "  -e, --erase_proxy_data   Erases the trusted and server directories,\n"
  "                           and their contents including server key/cert.\n\n"
*/

/*
 *----------------------------------------------------------------------
 *
 * EraseProxyData --
 *
 *    Delete the guest proxy data diectory and its contents.
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
EraseProxyData(void)
{
   gboolean ret = FALSE;

   if (!CheckRootPriv()) {
      goto exit;
   }

   if (g_file_test(guestProxyDir, G_FILE_TEST_IS_DIR)) {
      if (!CertUtil_RemoveDir(guestProxyDir)) {
         Error("Fail to remove the directory '%s'.\n", guestProxyDir);
         goto exit;
      }
   }

   ret = TRUE;

exit:
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
   gboolean        verbose;
   gboolean        force;
   gboolean        erase;
} options = { NULL, NULL, NULL, FALSE, FALSE, FALSE, FALSE};


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
   { "verbose",             'v', 0,
     G_OPTION_ARG_NONE,      &options.verbose,      NULL, NULL },
   { "erase_proxy_data",    'e', 0,
     G_OPTION_ARG_NONE,      &options.erase,        NULL, NULL },
   { "force",               'f', 0,
     G_OPTION_ARG_NONE,      &options.force,        NULL, NULL },
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

   if (options.verbose) {
      gIsLogEnabled = TRUE;
   }

   InitProxyPaths(CertUtil_GetToolDir());

   if ((options.generateCert && !CreateKeyCert(options.force)) ||
       (options.displayCert && !DisplayServerCert(options.outputCert)) ||
       (options.addCert && !AddTrustCert(options.addCert)) ||
       (options.deleteCert && !RemoveTrustCert(options.deleteCert)) ||
       (options.erase && !EraseProxyData())) {
      return 1;
   } else {
      return 0;
   }
}
