/*********************************************************
 * Copyright (C) 2006-2021 VMware, Inc. All rights reserved.
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
 * xferlogs.c --
 *
 *      Simple program based on rpctool to dump the vm-support output to
 *      the vmx log file base64 encoded, it can dump any file supplied on the
 *      commandline to the vmx log. It also does the decoding part of it
 *      it can read the vmware.log file decode and write the encoded files in
 *      the directory it was invoked.
 *
 *      example of a transfer found in the vmx log file.
 *      Aug 24 18:48:09: vcpu-0| Guest: >Logfile Begins : /root/install.log: ver - 1
 *      Aug 24 18:48:09: vcpu-0| Guest: >SW5zdGFsbGluZyA0NDEgcGFja2FnZXMKCkluc3RhbGxpbmcgZ2xpYmMtY29tbW9uLTIuMi41LTM0
 *      Aug 24 18:48:09: vcpu-0| Guest: >LgpJbnN0YWxsaW5nIGh3ZGF0YS0wLjE0LTEuCkluc3RhbGxpbmcgaW5kZXhodG1sLTcuMy0zLgpJ
 *      Aug 24 18:48:09: vcpu-0| Guest: >bnN0YWxsaW5nIG1haWxjYXAtMi4xLjktMi4KSW5zdGFsbGluZyBtYW4tcGFnZXMtMS40OC0yLgpJ
 *      ....
 *      ....
 *      Aug 24 18:48:10: vcpu-0| Guest: >Mi4K
 *      Aug 24 18:48:10: vcpu-0| Guest: >Logfile Ends
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <glib.h>

#include "vmware.h"
#include "vmsupport.h"
#include "vmcheck.h"
#include "debug.h"
#include "rpcvmx.h"
#include "rpcout.h"
#include "base64.h"
#include "str.h"
#include "strutil.h"

#include "xferlogs_version.h"
#include "vm_version.h"
#ifdef _WIN32
#include "vmware/tools/win32util.h"
#endif
#include "embed_version.h"
VM_EMBED_VERSION(XFERLOGS_VERSION_STRING);

/*
 * "The resultant base64-encoded data exceeds the original in length by the
 * ratio 4:3, and typically appears to consist of seemingly random characters.
 * As newlines, represented by a CR+LF pair, are inserted in the encoded data
 * every 76 characters, the actual length of the encoded data is approximately
 * 136.8% of the original." - Base64 Wiki
 * And just so that it produces 80 char output.
 */

#define BUF_BASE64_SIZE        57
#define BUF_OUT_SIZE           256
#define LOG_GUEST_MARK         "Guest: >"
#define LOG_START_MARK         ">Logfile Begins "
#define LOG_END_MARK           ">Logfile Ends "

typedef enum {
   NOT_IN_GUEST_LOGGING,
   IN_GUEST_LOGGING
} extractMode;

#define LOG_VERSION            1


/*
 *--------------------------------------------------------------------------
 *
 * xmitFile --
 *
 *       This function transfers a file using the rpc channel in base64
 *       encoding to the vmx logs.
 *
 * Results:
 *       None.
 *
 * Side effects:
 *       The program would exit if an error occurs in Base64_Encode.
 *       Output is added to the vmx log file.
 *
 *--------------------------------------------------------------------------
 */

static void
xmitFile(char *filename) //IN : file to be transmitted.
{
   FILE *fp;
   size_t readLen;
   char buf[BUF_BASE64_SIZE];

   /*
    * We have a unique identifier saying that this is guest dumping the
    * output of logs and not any other logging information from the guest.
    */
   char base64B[BUF_BASE64_SIZE * 2] = ">";
   char *base64Buf = base64B + 1;

   if (!(fp = fopen(filename, "rb"))) {
      Warning("Unable to open file %s with errno %d\n", filename, errno);
      exit(-1);
   }

   //XXX the format below is hardcoded and used by extractFile
   RpcVMX_Log("%s: %s: ver - %d", LOG_START_MARK, filename, LOG_VERSION);
   while ((readLen = fread(buf, 1, sizeof buf, fp)) > 0 ) {
      if (Base64_Encode(buf, readLen, base64Buf, sizeof base64B - 1, NULL)) {
         RpcVMX_Log("%s", base64B);
      } else {
         Warning("Error in Base64_Encode\n");
         goto exit;
      }
   }
exit:
   RpcVMX_Log(LOG_END_MARK);
   fclose(fp);
}


/*
 *--------------------------------------------------------------------------
 *
 * extractFile --
 *
 *       This function iterates through the vmx log file and for every
 *       line which has a "Guest: >" writes the unencoded base64 output to
 *       a file, depending on the state machine.
 *
 * Results:
 *       None
 *
 * Side effects:
 *       The program would exit if unable to read the input file.
 *       A series of decoded output files are created.
 *--------------------------------------------------------------------------
 */

static void
extractFile(char *filename) //IN: vmx log filename e.g. vmware.log
{
   FILE *fp;
   FILE *outfp = NULL;
   char buf[BUF_OUT_SIZE];
   uint8 base64Out[BUF_OUT_SIZE];
   size_t lenOut;
   char fname[256];
   char *ptrStr, *logInpFilename, *ver;
   int version;
   int filenu = 0; // output file enumerator
   DEBUG_ONLY(extractMode state = NOT_IN_GUEST_LOGGING);


   if (!(fp = fopen(filename, "rt"))) {
      Warning("Error opening file %s, errno %d - %s \n",
              filename, errno, strerror(errno));
      exit(-1);
   }

   while (fgets(buf, sizeof buf, fp)) {

      /*
       * The state machine determines when to open, write and close a file.
       */
      if (strstr(buf, LOG_GUEST_MARK)) {
         if (strstr(buf, LOG_START_MARK)) { //open a new output file.
            const char *ext;
            char tstamp[32];
            time_t now;

            /*
             * There could be multiple LOG_START_MARK in the log,
             * close existing one before opening a new file.
             */
            if (outfp) {
               ASSERT(state == IN_GUEST_LOGGING);
               Warning("Found a new start mark before end mark for "
                       "previous one\n");
               fclose(outfp);
               outfp = NULL;
            } else {
               ASSERT(state == NOT_IN_GUEST_LOGGING);
            }
            DEBUG_ONLY(state = IN_GUEST_LOGGING);

            /*
             * read the input filename, which was the filename written by the
             * guest.
             */
            logInpFilename = strstr(buf, LOG_START_MARK);
            logInpFilename += sizeof LOG_START_MARK;
            ptrStr = strstr(logInpFilename, ": ver ");
            if (ptrStr == NULL) {
               fprintf(stderr, "Invalid start log mark.");
               break;
            }
            *ptrStr = '\0';

            /*
             * Ignore the filename in the log, for obvious security reasons
             * and create a new filename consiting of time and enumerator.
             * Try to maintain the same extension reported by the guest,
             * though, if it's in the "allowed" list.
             */
            if (StrUtil_EndsWith(logInpFilename, ".zip")) {
               ext = "zip";
            } else if (StrUtil_EndsWith(logInpFilename, ".tar.gz")) {
               ext = "tar.gz";
            } else {
               /* Something else we don't expect from out vm-support scripts. */
               ext = "log";
            }

            time(&now);
            strftime(tstamp, sizeof tstamp, "%Y-%m-%d-%H-%M", localtime(&now));
            Str_Sprintf(fname, sizeof fname, "vm-support-%d-%s.%s",
                        filenu++, tstamp, ext);

            /*
             * Read the version information, if they dont match just warn
             * and leave the outfp null, so we do process the input file, but
             * dont write anything.
             */
            ptrStr++;
            ver = strstr(ptrStr, "ver - ");
            if (!ver) {
               Warning("No version information detected\n");
            } else {
               ver = ver + sizeof "ver - " - 1;
               version = strtol(ver, NULL, 0);
               if (version != LOG_VERSION) {
                  Warning("Input version %d doesn't match the\
                          version of this binary %d", version, LOG_VERSION);
               } else {
                  printf("Reading file %s to %s \n", logInpFilename, fname);
                  if (!(outfp = fopen(fname, "wb"))) {
                     Warning("Error opening file %s\n", fname);
                  }
               }
            }
         } else if (strstr(buf, LOG_END_MARK)) { // close the output file.
            /*
             * Need to check outfp, because we might get LOG_END_MARK
             * before LOG_START_MARK due to log rotation.
             */
            if (outfp) {
               ASSERT(state == IN_GUEST_LOGGING);
               fclose(outfp);
               outfp = NULL;
            } else {
               ASSERT(state == NOT_IN_GUEST_LOGGING);
               Warning("Reached file end mark without start mark\n");
            }
            DEBUG_ONLY(state = NOT_IN_GUEST_LOGGING);
         } else { // write to the output file
            if (outfp) {
               ASSERT(state == IN_GUEST_LOGGING);
               ptrStr = strstr(buf, LOG_GUEST_MARK);
               ptrStr += sizeof LOG_GUEST_MARK - 1;
               if (Base64_Decode(ptrStr, base64Out, BUF_OUT_SIZE, &lenOut)) {
                  if (fwrite(base64Out, 1, lenOut, outfp) != lenOut) {
                     Warning("Error writing output\n");
                  }
               } else {
                  Warning("Error decoding output %s\n", ptrStr);
               }
            } else {
               ASSERT(state == NOT_IN_GUEST_LOGGING);
               Warning("Missing file start mark\n");
            }
         }
      }
   }

   /*
    * We may need to close file in case LOG_END_MARK is missing.
    */
   if (outfp) {
      ASSERT(state == IN_GUEST_LOGGING);
      fclose(outfp);
   }
   fclose(fp);
}


static void
usage(GOptionContext *optCtx)
{
   gchar *usage =  g_option_context_get_help(optCtx, TRUE, NULL);
   g_print("%s", usage);
   g_free(usage);
}


int
main(int argc,
     char *argv[])
{
   gchar *gAppName;
   gchar *encBuffer = NULL; // storage for filename for 'enc' option
   gchar *decBuffer = NULL; // storage for filename for 'dec' option
   gchar *vmsupportStatus = NULL; // storage for vmsupport status for 'upd' opt
   int success = -1;
   int status;
   /*
    * This flag will be true if option passed starts with '-'.
    * This means we can use glib parser to see if it is a
    * valid option. Otherwise, try original parsing.
    */
   gboolean useGlibParser = (argc > 1) && (argv[1][0] == '-');

   GOptionEntry options[] = {
      {"put", 'p', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &encBuffer,
       "encodes and transfers <filename> to the VMX log.", "<filename>"},
      {"get", 'g', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &decBuffer,
       "extracts encoded data to <filename> from the VMX log.", "<filename>"},
      {"update", 'u', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,
       &vmsupportStatus, "updates status of vmsupport to <status>.",
       "<status>"},
      {NULL},
   };
   GOptionContext *optCtx;

#ifdef _WIN32
   WinUtil_EnableSafePathSearching(TRUE);
#endif

   gAppName = g_path_get_basename(argv[0]);
   g_set_prgname(gAppName);
   optCtx = g_option_context_new(NULL);
   g_option_context_add_main_entries(optCtx, options, NULL);

   /*
    * Check if environment is a VM
    */
   if (!VmCheck_IsVirtualWorld()) {
      g_printerr("Error: %s must be run inside a virtual machine"
                 " on a VMware hypervisor product.\n", gAppName);
      goto out;
   }

   if (useGlibParser) {
     /*
      * numOptions will count the number of options passed
      * and fail if more than one option is passed.
      */
      int numOptions;
      GError *gErr = NULL;

      if (!g_option_context_parse(optCtx, &argc, &argv, &gErr)) {
         g_printerr("%s: %s\n", gAppName, gErr->message);
         g_error_free(gErr);
         goto out;
      }

      numOptions = (encBuffer != NULL ? 1 : 0) + (decBuffer != NULL ? 1 : 0) +
                   (vmsupportStatus != NULL ? 1 : 0);

      if (numOptions > 1) {
         g_printerr("%s: Use one option per command.\n", gAppName);
         usage(optCtx);
         goto out;
      }
   } else {
      /*
       * If not using glib parser then check for old style options
       */
      if (argc != 3) {
         g_printerr("%s: Incorrect number of arguments.\n", gAppName);
         usage(optCtx);
         goto out;
      }

      if ((strncmp(argv[1], "enc", 3) == 0)) {
         encBuffer = argv[2];
      } else if ((strncmp(argv[1], "dec", 3) == 0)) {
         decBuffer = argv[2];
      } else if ((strncmp(argv[1], "upd", 3) == 0)) {
         vmsupportStatus = argv[2];
      }
   }

   if (encBuffer != NULL) {
      xmitFile(encBuffer);
   } else if (decBuffer != NULL) {
      extractFile(decBuffer);
   } else if (vmsupportStatus != NULL) {
      if (!(StrUtil_StrToInt(&status, vmsupportStatus))) {
         g_printerr("%s: Bad value specified.\n", gAppName);
         goto out;
      }
      RpcOut_sendOne(NULL, NULL, RPC_VMSUPPORT_STATUS " %d", status);
   } else {
      g_printerr("%s: Incorrect usage.\n", gAppName);
      usage(optCtx);
      goto out;
   }

   success = 0;

out:
   if (useGlibParser) {
      g_free(encBuffer);
      g_free(decBuffer);
      g_free(vmsupportStatus);
   }

   g_option_context_free(optCtx);
   g_free(gAppName);
   return success;
}
