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

/*
 * namespacetool.c --
 *
 *      Command line tool to communicate namespace DB.
 */

#include <stdio.h>
#include <glib.h>
#include "vmware.h"
#include "str.h"
#include "util.h"
#include "dynbuf.h"
#include "vmware/tools/guestrpc.h"
#include "vmware/tools/log.h"
#if defined(_WIN32)
#include "vmware/tools/win32util.h"
#endif
#include "debug.h"

// Core Namespace commands
#define NSDB_PRIV_GET_VALUES_CMD  "namespace-priv-get-values"
#define NSDB_PRIV_SET_KEYS_CMD    "namespace-priv-set-keys"

// namespace commands for end user
#define NSDB_GET_VALUE_USER_CMD  "get-value"
#define NSDB_SET_KEY_USER_CMD    "set-key"
#define NSDB_DEL_KEY_USER_CMD    "delete-key"

#define SUPPORTED_FILE_SIZE_IN_BYTES (16 * 1024) // Refer to namespaceDb.h

/*
 * Aggregation of command options.
 */

typedef struct NamespaceOptionsState {
   gchar *cmdName;
   gchar *nsName;
   gchar *keyName;    //for command set-key or delete-key or get-value
   gchar *valueToSet;
   gchar *oldValueToSet;
   gchar *getValueFromFile;
   gboolean verboseLogFlag;
   gboolean standardInput;
} NamespaceOptionsState;

static gchar *gAppName = NULL;


/*
 ******************************************************************************
 * PrintUsage --
 *
 * Prints the usage mesage.
 *
 ******************************************************************************
 */

static void
PrintUsage(GOptionContext *optCtx)
{
   gchar *usage;

   usage = g_option_context_get_help(optCtx, TRUE, NULL);
   g_printerr("%s", usage);
   g_free(usage);
}


/*
 ******************************************************************************
 * GetInternalNamespaceCommand --
 *
 * Namespacetool should allow only privilege namespaceDB.
 *
 * @param[in] cmd     namespace command name.
 *
 * @return internal namespace command
 *
 ******************************************************************************
 */

static char*
GetInternalNamespaceCommand(const gchar *cmd)
{
   if (g_strcmp0(cmd, NSDB_GET_VALUE_USER_CMD) == 0) {
      return NSDB_PRIV_GET_VALUES_CMD;
   } else if (g_strcmp0(cmd, NSDB_SET_KEY_USER_CMD) == 0) {
      return NSDB_PRIV_SET_KEYS_CMD;
   } else if (g_strcmp0(cmd, NSDB_DEL_KEY_USER_CMD) == 0) {
      return NSDB_PRIV_SET_KEYS_CMD;
   } else {
      return NULL;
   }
}


/*
 ******************************************************************************
 * ValidateNSCommands --
 *
 * Namespacetool should allow only privilege and non privilege commands of
 * namespaceDB.
 * @param[in] cmdName     namespace command name.
 *
 * @return TRUE if successful, FALSE otherwise.
 *
 ******************************************************************************
 */

static gboolean
ValidateNSCommands(const gchar *cmdName)
{
   if ((g_strcmp0(cmdName, NSDB_GET_VALUE_USER_CMD) == 0) ||
       (g_strcmp0(cmdName, NSDB_SET_KEY_USER_CMD) == 0) ||
       (g_strcmp0(cmdName, NSDB_DEL_KEY_USER_CMD) == 0)) {
      return TRUE;
   } else {
      fprintf(stderr, "Invalid command \"%s\"\n", cmdName);
      return FALSE;
   }
}


/*
 *******************************************************************************
 * PrintInternalCommand --
 *
 * Prints internal command in verbose mode
 *
 * @param[in] data      set of strings which has '\0' as a delimiter.
 * @param[in] dataSize  size of data.
 *
 *******************************************************************************
 */

static void
PrintInternalCommand(const char *data, size_t dataSize)
{
   int readCounter = 0;
   char *tmp = NULL;
   char *printBuf = NULL;
   if (dataSize > 0) {
      printBuf = (char *) calloc((int)dataSize, sizeof(char));
      if (printBuf == NULL) {
         fprintf(stderr, "Out of memory error");
         return;
      }
      tmp = printBuf;
      while (dataSize > readCounter) {
         if (*data != '\0') {
            *printBuf++ = *data;
         } else if (readCounter < dataSize - 1) {
            *printBuf++ = ',';
         }
         ++data;
         ++readCounter;
      }
      fprintf(stdout, "Internal command is %s\n", tmp);
   }
   free(tmp);
}


/*
 ******************************************************************************
 * GetValueFromStdin --
 *
 * Get standard input as a value
 *
 * @param[Out]  data     return standard input strings. This data is terminated
 *                       by an extra nul character, but there may be other
 *                       nuls in the intervening data.
 * @param[Out]  length   return length of standard input strings.
 *
 * @return TRUE on success or FALSE if stdin data is empty or more than Max
 *         limit
 *
 ******************************************************************************
 */

static Bool
GetValueFromStdin(gchar **data, gsize *length)
{
   GError *gErr = NULL;
   GIOStatus status;
   GIOChannel* iochannel;
   Bool retVal = TRUE;
#if defined(_WIN32)
   iochannel = g_io_channel_win32_new_fd(0);
#else
   iochannel = g_io_channel_unix_new(0);
#endif
   /**
    * This function never returns EOF
    */
   status =  g_io_channel_read_to_end(iochannel, data, length, &gErr);

   if (status != G_IO_STATUS_NORMAL) {
      fprintf(stderr, "%s: Read failed from stdin with status code %d. %s\n",
              gAppName, status, (gErr != NULL ? gErr->message : ""));
      retVal = FALSE;
   } else {
      if (*length > SUPPORTED_FILE_SIZE_IN_BYTES) {
         retVal = FALSE;
         fprintf(stderr, "%s: stdin data must not exceed %d bytes\n",
               gAppName, SUPPORTED_FILE_SIZE_IN_BYTES);
      } else  if (*length == 0) {
         retVal = FALSE;
         fprintf(stderr, "%s: stdin data must not be empty\n", gAppName);
      }
   }
   if (retVal == FALSE) {
      g_free(*data);
      *data = NULL;
      *length = 0;
   }
   g_free(gErr);
   return retVal;
}


/*
 ******************************************************************************
 * GetValueFromFile --
 *
 * Read file contents as a string.
 *
 * @param[in]  filePath       file path
 * @param[out] fileContents   file contents
 * @param[out] length         length of file
 *
 * @return TRUE on success or FALSE if file is empty or more than Max limit
 *
 ******************************************************************************
 */

static Bool
GetValueFromFile(const char *filePath, char **fileContents, gsize *length)
{
   GError *gErr = NULL;
   Bool retVal = FALSE;
   retVal = g_file_get_contents(filePath, fileContents, length, &gErr);
   if (retVal == FALSE) {
      fprintf(stderr, "%s: %s: %s\n", gAppName,
              (gErr != NULL ? gErr->message : "Failed while reading file"),
              filePath);
   } else {
      if (*length > SUPPORTED_FILE_SIZE_IN_BYTES) {
         retVal = FALSE;
         fprintf(stderr, "%s: File size must not exceed %d bytes\n",
                 gAppName, SUPPORTED_FILE_SIZE_IN_BYTES);
      } else  if (*length == 0) {
         retVal = FALSE;
         fprintf(stderr, "%s: File must not be empty\n", gAppName);
      }
   }
   if (retVal == FALSE) {
      g_free(*fileContents);
      *fileContents = NULL;
      *length = 0;
   }
   g_free(gErr);
   return retVal;
}


/*
 ******************************************************************************
 * RunNamespaceCommand --
 *
 * Processes the namespace command for get/set/delete key.
 * @param[in]  nsOptions     Parsed namespace command line options will
 *                           be placed in this struct.
 *
 * @return TRUE if successful, FALSE otherwise.
 *
 ******************************************************************************
 */

static Bool
RunNamespaceCommand(NamespaceOptionsState *nsOptions)
{
   char *result = NULL;
   size_t resultLen = 0;
   Bool status = FALSE;
   gchar *opCode = NULL;
   gchar *keyValueData = NULL;
   gsize keyValueLength = 0;

   const char *nscmd = GetInternalNamespaceCommand(nsOptions->cmdName);

   DynBuf buf;
   ASSERT(nscmd);
   DynBuf_Init(&buf);
   if (!DynBuf_Append(&buf, nscmd, strlen(nscmd)) ||
       !DynBuf_Append(&buf, " ", 1) ||
       !DynBuf_AppendString(&buf, nsOptions->nsName)) {
      fprintf(stderr, "Could not construct request buffer\n");
      goto exit;
   }
   if (strlen(nsOptions->oldValueToSet) == 0) {
      opCode = g_strdup("0");
   } else {
      opCode = g_strdup("1");
   }
   if (g_strcmp0(nsOptions->cmdName, NSDB_GET_VALUE_USER_CMD) == 0) {
      ASSERT(nsOptions->keyName);
      if (!DynBuf_AppendString(&buf, nsOptions->keyName)) {
         fprintf(stderr, "Could not construct request buffer\n");
         goto exit;
      }
   } else if (g_strcmp0(nsOptions->cmdName, NSDB_SET_KEY_USER_CMD) == 0) {
      ASSERT(nsOptions->keyName);
      if (!DynBuf_AppendString(&buf, "1") || // numOps
          !DynBuf_AppendString(&buf, opCode)||
          !DynBuf_AppendString(&buf, nsOptions->keyName)) {
         fprintf(stderr, "Could not construct request buffer\n");
         goto exit;
      }
      if (nsOptions->getValueFromFile == NULL) {
         if (nsOptions->valueToSet != NULL) {
            if (strlen(nsOptions->valueToSet) == 0) {
               fprintf(stderr, "%s: Key value must not be empty\n", gAppName);
               goto exit;
            }
            if (!DynBuf_AppendString(&buf, nsOptions->valueToSet) ||
                !DynBuf_AppendString(&buf, nsOptions->oldValueToSet)) {
               fprintf(stderr, "Could not construct request buffer\n");
               goto exit;
            }
         } else {
            if (GetValueFromStdin(&keyValueData, &keyValueLength) == FALSE) {
               goto exit;
            }
            if (!DynBuf_Append(&buf, keyValueData, keyValueLength + 1) ||
                !DynBuf_AppendString(&buf, nsOptions->oldValueToSet)) {
               fprintf(stderr, "Could not construct request buffer\n");
               goto exit;
            }
         }
      } else {
         if (GetValueFromFile(nsOptions->getValueFromFile,
                              &keyValueData, &keyValueLength) == FALSE) {
            goto exit;
         }
         if (!DynBuf_Append(&buf, keyValueData, keyValueLength + 1) ||
             !DynBuf_AppendString(&buf, nsOptions->oldValueToSet)) {
            fprintf(stderr, "Could not construct request buffer\n");
            goto exit;
         }
      }
   } else if (g_strcmp0(nsOptions->cmdName, NSDB_DEL_KEY_USER_CMD) == 0) {
      ASSERT(nsOptions->keyName);
      if (!DynBuf_AppendString(&buf, "1") ||
          !DynBuf_AppendString(&buf, opCode) ||
          !DynBuf_AppendString(&buf, nsOptions->keyName) ||
          !DynBuf_AppendString(&buf, "") || // zero length for value to delete
          !DynBuf_AppendString(&buf, nsOptions->oldValueToSet)) {
         fprintf(stderr, "Could not construct request buffer\n");
         goto exit;
      }
   }

   if (nsOptions->verboseLogFlag) {
      PrintInternalCommand(DynBuf_Get(&buf), DynBuf_GetSize(&buf));
   }

   status = RpcChannel_SendOneRaw(DynBuf_Get(&buf),
                                  DynBuf_GetSize(&buf), &result, &resultLen);
   if (!status) {
      fprintf(stderr, "failure: %s\n",
            result && *result ? result : "unknown");
   } else {
      char *p = result;
      if (resultLen == 0) {
         if (nsOptions->verboseLogFlag) {
            printf("success\n");
         }
      } else {
         if (nsOptions->verboseLogFlag) {
            printf("success - result:");
         }
         while (p < result + resultLen) {
            printf("%s", p);
            p += strlen(p) + 1;
         }
      }
      fflush(stdout);
   }
   free( result);

 exit:
   DynBuf_Destroy(&buf);
   fflush(stderr);
   g_free(keyValueData);
   g_free(opCode);
   return status;
}


/*
 ******************************************************************************
 * PostVerifyGetValueOptions --
 *
 * Post parse hook to verify namespace command get-value
 *
 * @param[in]  context    Unused.
 * @param[in]  group      Unused.
 * @param[in]  data       Unused.
 * @param[out] error      Setting error message to display in case of invalid
 *                        command line options.
 *
 * @return TRUE if successful, FALSE if option is invalid.
 *
 ******************************************************************************
 */

static gboolean
PostVerifyGetValueOptions(GOptionContext *context, GOptionGroup *group,
                          gpointer data, GError **error)
{
   NamespaceOptionsState *nsOptions;
   ASSERT(data);
   nsOptions = (NamespaceOptionsState *) data;
   if (nsOptions->cmdName == NULL) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "Namespace command must be specified");
      return FALSE;
   }
   if (nsOptions->nsName == NULL) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "Namespace name must be specified");
      return FALSE;
   }
   if (g_strcmp0(nsOptions->cmdName, NSDB_GET_VALUE_USER_CMD) == 0) {
      if (nsOptions->keyName == NULL) {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                      "Key name must be specified");
         return FALSE;
      }
   }
   return TRUE;
}


/*
 ******************************************************************************
 * PostVerifyDeleteKeyOptions --
 *
 * Post parse hook to verify namespace command delete-key
 *
 * @param[in]  context    Unused.
 * @param[in]  group      Unused.
 * @param[in]  data       Unused.
 * @param[out] error      Setting error message to display in case of invalid
 *                        command line options.
 *
 * @return TRUE if successful, FALSE if option is invalid.
 *
 ******************************************************************************
 */

static gboolean
PostVerifyDeleteKeyOptions(GOptionContext *context, GOptionGroup *group,
                           gpointer data, GError **error)
{
   NamespaceOptionsState *nsOptions;
   ASSERT(data);
   nsOptions = (NamespaceOptionsState *) data;

   if (nsOptions->cmdName == NULL) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "Namespace command must be specified");
      return FALSE;
   }
   if (nsOptions->nsName == NULL) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "Namespace name must be specified");
      return FALSE;
   }
   if (g_strcmp0(nsOptions->cmdName, NSDB_DEL_KEY_USER_CMD) == 0) {
      if (nsOptions->keyName == NULL) {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
               "Key name must be specified");
         return FALSE;
      }
   }
   return TRUE;
}


/*
 ******************************************************************************
 * PostVerifySetKeyOptions --
 *
 * Post parse hook to verify namespace command set-key
 *
 * @param[in]  context    Unused.
 * @param[in]  group      Unused.
 * @param[in]  data       Unused.
 * @param[out] error      Setting error message to display in case of invalid
 *                        command line options.
 *
 * @return TRUE if successful, FALSE if option is invalid.
 *
 ******************************************************************************
 */

static gboolean
PostVerifySetKeyOptions(GOptionContext *context, GOptionGroup *group,
                        gpointer data, GError **error)
{
   int usedOptions = 0;
   NamespaceOptionsState *nsOptions;
   ASSERT(data);
   nsOptions = (NamespaceOptionsState *) data;

   if (nsOptions->cmdName == NULL) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "Namespace command must be specified");
      return FALSE;
   }
   if (nsOptions->nsName == NULL) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "Namespace name must be specified");
      return FALSE;
   }
   if (g_strcmp0(nsOptions->cmdName, NSDB_SET_KEY_USER_CMD) == 0) {
      if (nsOptions->keyName == NULL) {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Key name must be specified");
         return FALSE;
      }
      if (nsOptions->valueToSet != NULL) {
         ++usedOptions;
      }
      if (nsOptions->getValueFromFile != NULL) {
         ++usedOptions;
      }
      if (nsOptions->standardInput == 1) {
         ++usedOptions;
      }
      if ((usedOptions > 1) || (usedOptions == 0)) {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Key value must be specified with either -f or -v or -s");
         return FALSE;
      }
   }
   return TRUE;
}


/*
 ******************************************************************************
 * main --
 *
 * Main entry point.
 *
 * @param[in]  argc  Number of args.
 * @param[in]  argv  The args.
 *
 ******************************************************************************
 */

int
main(int argc, char *argv[])
{
   int success = -1;
   GError *gErr = NULL;
   gchar *descriptionBuf;
   gchar *helpBuf;
   GOptionContext *optCtx;
   GOptionGroup *gr;
   gchar *summary;
   NamespaceOptionsState nsOptions = { NULL, NULL, NULL, NULL, "",
                                       NULL, FALSE, FALSE};

   //Options for namespacetool commands

   GOptionEntry mainEntry[] = {
      { "verbose", 'V', 0, G_OPTION_ARG_NONE, &nsOptions.verboseLogFlag,
        "Verbose logging mode", NULL },
      { NULL }
   };
   GOptionEntry getValuesEntry[] = {
      { "key", 'k', G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_STRING, &nsOptions.keyName,
        "Key value to return", "<key-name>" },
      { NULL }
   };
   GOptionEntry setKeysEntry[] = {
      { "key", 'k', G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_STRING,
         &nsOptions.keyName, "Key name to use",  "<key-name>" },
      { "value", 'v', 0, G_OPTION_ARG_STRING, &nsOptions.valueToSet,
        "Value to set", "<value>"},
      { "oldValue", 'o', 0, G_OPTION_ARG_STRING, &nsOptions.oldValueToSet,
        "Value must match with current key value in the "
        "namespace for update operation to proceed", "<old-value>"},
      { "fromFile", 'f', 0, G_OPTION_ARG_STRING, &nsOptions.getValueFromFile,
        "Value to use from file path", "<file-path>"},
      { "stdin", 's', 0, G_OPTION_ARG_NONE, &nsOptions.standardInput,
        "Value to use from standard input", NULL},
      { NULL }
   };
   GOptionEntry deleteKeyEntry[] = {
      { "key", 'k', G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_STRING,
         &nsOptions.keyName, "Key name to use", "<key-name>"},
      { "oldValue", 'o', G_OPTION_FLAG_NOALIAS, G_OPTION_ARG_STRING,
         &nsOptions.oldValueToSet,
        "Value must match with current key value in "
        "the namespace for delete operation to proceed", "<old-value>"},
      { NULL }
   };

#if defined(_WIN32)
   WinUtil_EnableSafePathSearching(TRUE);
#endif

   gAppName = g_path_get_basename(argv[0]);
   g_set_prgname(gAppName);

   optCtx = g_option_context_new("[get-value | set-key | delete-key] "
                                  "[<namespace-name>]");

   gr = g_option_group_new("namespace commands", "", "", optCtx, NULL);
   g_option_group_add_entries(gr, mainEntry);
   g_option_context_set_main_group(optCtx, gr);

   summary = g_strdup_printf("Example:\n  %s set-key <namespace-name> "
                             "-k <key-name> -v <value>\n  %s set-key "
                             "<namespace-name> -k <key-name> -f <file-path>"
                             "\n  echo \"<value>\" | %s set-key "
                             "<namespace-name> -k <key-name> -s\n  %s "
                             "delete-key  <namespace-name> -k <key-name>"
                             "\n  %s get-value <namespace-name> "
                             "-k <key-name>\n", gAppName,
                              gAppName, gAppName, gAppName, gAppName);
   g_option_context_set_summary(optCtx, summary);

   if (argc > 1) {
      nsOptions.cmdName = g_strdup(argv[1]);
   }
   if (argc > 2) {
      nsOptions.nsName = g_strdup(argv[2]);
   }
   //Namespacetool command - namespace-get-values
   descriptionBuf = g_strdup_printf("%s command %s:", gAppName,
                                    NSDB_GET_VALUE_USER_CMD);
   helpBuf = g_strdup_printf("Show help for command \"%s\"",
                              NSDB_GET_VALUE_USER_CMD);
   gr = g_option_group_new(NSDB_GET_VALUE_USER_CMD, descriptionBuf,
                           helpBuf, &nsOptions, NULL);
   g_free(descriptionBuf);
   g_free(helpBuf);
   g_option_group_add_entries(gr, getValuesEntry);
   g_option_context_add_group(optCtx, gr);
   g_option_group_set_parse_hooks(gr, NULL, PostVerifyGetValueOptions);

   //Namespacetool command - namespace-set-keys
   descriptionBuf = g_strdup_printf("%s command %s: - "
                                    "Create or update key value pair\n",
                                     gAppName, NSDB_SET_KEY_USER_CMD);
   helpBuf = g_strdup_printf ("Show help for command \"%s\"",
                              NSDB_SET_KEY_USER_CMD);

   gr = g_option_group_new(NSDB_SET_KEY_USER_CMD, descriptionBuf, helpBuf,
                           &nsOptions, NULL);
   g_free(descriptionBuf);
   g_free(helpBuf);
   g_option_group_add_entries(gr, setKeysEntry);
   g_option_context_add_group(optCtx, gr);
   g_option_group_set_parse_hooks(gr, NULL, PostVerifySetKeyOptions);

   //Namespacetool command - namespace-set-key for deleting key
   descriptionBuf = g_strdup_printf("%s command %s:- Delete key value pair\n",
                                    gAppName, NSDB_DEL_KEY_USER_CMD);
   helpBuf = g_strdup_printf("Show help for command \"%s\"",
                              NSDB_DEL_KEY_USER_CMD);

   gr = g_option_group_new(NSDB_DEL_KEY_USER_CMD, descriptionBuf, helpBuf,
                           &nsOptions, NULL);
   g_free(descriptionBuf);
   g_free(helpBuf);
   g_option_group_add_entries(gr, deleteKeyEntry);
   g_option_context_add_group(optCtx, gr);
   g_option_group_set_parse_hooks(gr, NULL, PostVerifyDeleteKeyOptions);

   if (!g_option_context_parse(optCtx, &argc, &argv, &gErr)) {
      PrintUsage(optCtx);
      fprintf(stderr, "%s: %s\n", gAppName, (gErr != NULL ? gErr->message : ""));
      g_error_free(gErr);
      goto exit;
   }

   if (nsOptions.verboseLogFlag) {
      VMTools_ConfigLogToStdio(gAppName);
   }

   /*
    *  Validating namespace command name after g_context_parser
    *  because argv[1] can have string "--help"
    */
   if (argc > 1 && ValidateNSCommands(nsOptions.cmdName) == FALSE) {
      goto exit;
   }

   success = RunNamespaceCommand(&nsOptions) ? 0 : 1;

 exit:
   g_option_context_free(optCtx);
   g_free(summary);
   g_free(gAppName);
   return success;
}
