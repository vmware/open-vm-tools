/*********************************************************
 * Copyright (C) 2011-2020 VMware, Inc. All rights reserved.
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
 * @file main.c
 *
 *    The GuestAuth certificate manipulation command line tool.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifndef _WIN32
#include <unistd.h>
#include <errno.h>
#endif
#include <glib.h>

#include "VGAuthBasicDefs.h"
#include "VGAuthAlias.h"
#include "VGAuthCommon.h"
#include "VGAuthError.h"
#include "VGAuthLog.h"
#include "VGAuthUtil.h"
#define VMW_TEXT_DOMAIN "VGAuthCli"
#include "i18n.h"
#include "prefs.h"

static gchar *appName;

static gboolean verbose = FALSE;


#ifdef _WIN32


/*
 ******************************************************************************
 * GetHelp --                                                            */ /**
 *
 * Get help message for CLI on windows
 *
 * @param[in] context      The GOptionContext for generating the message
 * @param[in] paramStr     The command line operation arguments
 * @param[in] cmdOptions   The command line options for given operation
 *
 ******************************************************************************
 */

static gchar *
GetHelp(GOptionContext *context,
        const char *paramStr,
        const GOptionEntry *cmdOptions)
{
   gchar *helpMsg;
   const gchar *lUsage = SU_(cmdline.help.usage, "Usage");
   const gchar *lHelpOption = SU_(cmdline.help.helpoption, "Help Options");
   const gchar *lAppOption = SU_(cmdline.help.appoption, "Application Options");
   const gchar *lHint = SU_(cmdline.help.hint, "Show help options");

   if (cmdOptions == NULL){
      helpMsg = g_strdup_printf("%s:\n"
                                "  %s %s\n\n"
                                "%s\n\n"
                                "%s:\n"
                                "  -h, --help       %s\n",
                                lUsage,
                                appName, paramStr,
                                g_option_context_get_summary(context),
                                lHelpOption,
                                lHint);
   } else {
      gchar *optionMsg;
      GString *optStr;
      int i;
      optStr = g_string_new(NULL);
      for (i = 0; cmdOptions[i].long_name != NULL; i++) {
         g_string_append_printf(optStr,
                                "  -%c, --%-8s    %s\n",
                                cmdOptions[i].short_name,
                                cmdOptions[i].long_name,
                                cmdOptions[i].description);
      }
      optionMsg = g_string_free(optStr, FALSE);

      helpMsg = g_strdup_printf("%s:\n"
                                "  %s %s\n\n"
                                "%s\n\n"
                                "%s:\n"
                                "  -h, --help       %s\n"
                                "%s:\n"
                                "%s",
                                lUsage,
                                appName, paramStr,
                                g_option_context_get_summary(context),
                                lHelpOption,
                                lHint,
                                lAppOption,
                                optionMsg);
      g_free(optionMsg);
   }

   return helpMsg;
}

#endif



/*
 ******************************************************************************
 * Usage --                                                              */ /**
 *
 * Usage message for CLI
 *
 * @param[in] optContext   The GOptionContext for generating the message
 * @param[in] paramStr     The command line operation arguments
 * @param[in] cmdOptions   The command line options for given operation
 *
 ******************************************************************************
 */

static void
Usage(GOptionContext *optContext,
      const char *paramStr,
      const GOptionEntry *cmdOptions)
{
   gchar *usage;
#ifdef _WIN32
   usage = GetHelp(optContext, paramStr, cmdOptions);
#else
   usage = g_option_context_get_help(optContext, TRUE, NULL);
#endif

   g_printerr("%s", usage);
   g_free(usage);
   exit(-1);
}


/*
 ******************************************************************************
 * CliLog --                                                             */ /**
 *
 * Error message logging function for the CLI.
 *
 * @param[in]     logDomain   The glib logging domain, which is set by the
 *                            various glib components and vgauth itself.
 * @param[in]     logLevel    The severity of the message.
 * @param[in]     msg         The error message.
 * @param[in]     userData    Any userData specified in the call to
 *                            VGAuth_SetLogHandler()
 *
 ******************************************************************************
 */

static void
CliLog(const char *logDomain,
       int logLevel,
       const char *msg,
       void *userData)
{
   // ignore all but errors
   if (logLevel & G_LOG_LEVEL_WARNING) {
      g_printerr("%s[%d]: %s", logDomain, logLevel, msg);
#ifdef VMX86_DEBUG
   } else {
      fprintf(stderr, "Dropping message %s[%d]: %s", logDomain, logLevel, msg);
#endif
   }
}


/*
 ******************************************************************************
 * SubjectName --                                                        */ /**
 *
 * Returns the name value for a subject, or <ANY>.
 *
 * @param[in] s      The VGAuthSubject.
 *
 ******************************************************************************
 */

static const gchar *
SubjectName(VGAuthSubject *s)
{
   if (s->type == VGAUTH_SUBJECT_NAMED) {
      return s->val.name;
   } else {
      return SU_(name.any, "<ANY>");
   }
}


/*
 ******************************************************************************
 * CliLoadPemFile --                                                     */ /**
 *
 * Loads a PEM certificate from a file.  The caller should g_free() the
 * return value when finished.
 *
 * @param[in]  fileName          The filename of the cert in PEM format.
 *
 * @return The contents of the certificate file.
 *
 ******************************************************************************
 */

static gchar *
CliLoadPemFILE(const gchar *fileName)
{
   gchar *contents = NULL;
   gsize fileSize;
   GError *gErr = NULL;

   /*
    * XXX
    *
    * Might be nice for this to handle stdin.  Either a NULL
    * filename or "-" ?
    */
   if (!g_file_get_contents(fileName, &contents, &fileSize, &gErr)) {
      g_printerr(SU_(loadfile.fail,
                     "%s: Unable to read PEM file '%s'\n"),
                 appName, gErr->message);
      g_error_free(gErr);
   }

   return contents;
}


/*
 ******************************************************************************
 * CliAddAlias --                                                        */ /**
 *
 * Adds a certficate and subject for the user.
 *
 * @param[in]  ctx                  The VGAuthContext.
 * @param[in]  userName             The user whose store is being changed.
 * @param[in]  subject              The associated subject name.
 * @param[in]  pemFileName          The filename of the cert in PEM format.
 * @param[in]  addMapped            Set if a link is also to be added to
 *                                  the mapping file.
 * @param[in]  comment              The comment.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
CliAddAlias(VGAuthContext *ctx,
            const char *userName,
            const char *subject,
            const char *pemFilename,
            gboolean addMapped,
            const char *comment)
{
   gchar *pemCert;
   VGAuthError err;
   VGAuthAliasInfo ai;

   pemCert = CliLoadPemFILE(pemFilename);
   if (NULL == pemCert) {
      return VGAUTH_E_INVALID_CERTIFICATE;
   }

   /*
    * The 'comment' cmdline arg is optional, but the underlying API needs
    * a real value.
    */
   ai.comment = (comment) ? (char *) comment : "";

   ai.subject.type = VGAUTH_SUBJECT_NAMED;
   ai.subject.val.name = (char *) subject;

   err = VGAuth_AddAlias(ctx, userName, addMapped, pemCert, &ai, 0, NULL);
   if (VGAUTH_E_OK != err) {
      g_printerr(SU_(addsubj.fail,
                    "%s: Failed to add alias for user '%s': %s.\n"),
                 appName, userName, VGAuth_GetErrorText(err, NULL));
   } else if (verbose) {
      g_print(SU_(addsubj.success, "%s: alias added\n"), appName);
   }

   g_free(pemCert);

   return err;
}


/*
 ******************************************************************************
 * CliRemoveAlias --                                                     */ /**
 *
 * Removes a certficate for the user.
 *
 * @param[in]  ctx                  The VGAuthContext.
 * @param[in]  userName             The user whose store is being changed.
 * @param[in]  subject              The associated subject.
 * @param[in]  pemFileName          The filename of the cert in PEM format.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
CliRemoveAlias(VGAuthContext *ctx,
                 const char *userName,
                 const char *subject,
                 const char *pemFilename)
{
   VGAuthError err;
   gchar *pemCert;
   VGAuthSubject subj;

   pemCert = CliLoadPemFILE(pemFilename);
   if (NULL == pemCert) {
      return VGAUTH_E_INVALID_CERTIFICATE;
   }

   if (subject) {
      subj.val.name = (char *) subject;
      subj.type = VGAUTH_SUBJECT_NAMED;
      err = VGAuth_RemoveAlias(ctx, userName, pemCert, &subj, 0, NULL);
   } else {
      err = VGAuth_RemoveAliasByCert(ctx, userName, pemCert, 0, NULL);
   }

   if (VGAUTH_E_OK != err) {
      g_printerr(SU_(removesubj.fail,
                     "%s: Failed to remove alias for user '%s': %s.\n"),
                 appName, userName, VGAuth_GetErrorText(err, NULL));
   } else if (verbose) {
      g_print(SU_(removesubj.success, "%s: alias removed\n"), appName);
   }

   g_free(pemCert);

   return err;
}


/*
 ******************************************************************************
 * CliRemoveAllAlias --                                                  */ /**
 *
 * Removes aliases for given subject and [userName].
 * If userName is not provided, only remove mapped aliases.
 *
 * @param[in]  ctx                  The VGAuthContext.
 * @param[in]  subject              The associated subject.
 * @param[in]  userName             The user whose store is being changed.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
CliRemoveAllAlias(VGAuthContext *ctx,
                  const char *subject,
                  const char *userName)
{
   VGAuthError err;
   gboolean fail = FALSE;
   int num = 0;
   int i;
   int j;

   if (NULL != userName) {
      VGAuthUserAlias *uaList = NULL;

      /* get aliases by userName */
      err = VGAuth_QueryUserAliases(ctx, userName, 0, NULL, &num, &uaList);
      if (VGAUTH_E_OK != err) {
         g_printerr(SU_(list.error,
                        "%s: Failed to list aliases for user '%s': %s.\n"),
                    appName, userName, VGAuth_GetErrorText(err, NULL));
         return err;
      }

      /* find matched aliases, remove */
      for(i = 0; i < num; i++) {
         for(j = 0; j < uaList[i].numInfos; j++) {
            if (!g_strcmp0(subject, uaList[i].infos[j].subject.val.name)) {
               err = VGAuth_RemoveAlias(ctx,
                                        userName,
                                        uaList[i].pemCert,
                                        &(uaList[i].infos[j].subject),
                                        0,
                                        NULL);
               if (VGAUTH_E_OK != err) {
                  g_printerr(SU_(removeall.removefail,
                                 "%s: Failed to remove alias for user '%s'"
                                 " subject '%s' pemCert '%s': %s.\n"),
                             appName,
                             userName,
                             subject,
                             uaList[i].pemCert,
                             VGAuth_GetErrorText(err, NULL));
                  fail = TRUE;
               }

               break;
            }
         }

         if (fail){
            break;
         }
      }

      VGAuth_FreeUserAliasList(num, uaList);
   } else {
      VGAuthMappedAlias *maList = NULL;

      /* no userName provided, so only can get mapped aliases */
      err = VGAuth_QueryMappedAliases(ctx, 0, NULL, &num, &maList);
      if (VGAUTH_E_OK != err) {
         g_printerr(SU_(listmapped.error,
                        "%s: Failed to list mapped aliases: %s.\n"),
                     appName, VGAuth_GetErrorText(err, NULL));
         return err;
      }

      /* find matched aliases, remove */
      for(i = 0; i < num; i++) {
         for(j = 0; j < maList[i].numSubjects; j++) {
            if (!g_strcmp0(subject, maList[i].subjects[j].val.name)){
               err = VGAuth_RemoveAlias(ctx,
                                        maList[i].userName,
                                        maList[i].pemCert,
                                        &(maList[i].subjects[j]),
                                        0,
                                        NULL);
               if (VGAUTH_E_OK != err) {
                  g_printerr(SU_(removeall.removefail,
                                 "%s: Failed to remove alias for user '%s'"
                                 " subject '%s' pemCert '%s': %s.\n"),
                             appName,
                             maList[i].userName,
                             subject,
                             maList[i].pemCert,
                             VGAuth_GetErrorText(err, NULL));
                  fail = TRUE;
               }

               break;
            }
         }

         if (fail){
            break;
         }
      }

      VGAuth_FreeMappedAliasList(num, maList);
   }

   if (VGAUTH_E_OK == err && verbose) {
      g_print(SU_(removeall.success, "%s: all aliases removed\n"), appName);
   }

   return err;
}


/*
 ******************************************************************************
 * CliList --                                                            */ /**
 *
 * List all UserAliases for a user.
 *
 * @param[in]  ctx                  The VGAuthContext.
 * @param[in]  userName             The user whose store is being queried.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
CliList(VGAuthContext *ctx,
        const char *userName)
{
   VGAuthError err;
   int num;
   int i;
   int j;
   VGAuthUserAlias *uaList;

   err = VGAuth_QueryUserAliases(ctx, userName, 0, NULL, &num, &uaList);
   if (VGAUTH_E_OK != err) {
      g_printerr(SU_(list.error,
                     "%s: Failed to list aliases for user '%s': %s.\n"),
                 appName, userName, VGAuth_GetErrorText(err, NULL));
      return err;
   }

   if (verbose) {
      g_print(SU_(list.count, "%s Found %d aliases for user '%s'\n"),
              appName, num, userName);
   }

   for (i = 0; i < num; i++) {
      g_print("%s\n", uaList[i].pemCert);
      for (j = 0; j < uaList[i].numInfos; j++) {
         g_print("\t%s: %s %s: %s\n",
                 SU_(list.subject, "Subject"),
                 SubjectName(&(uaList[i].infos[j].subject)),
                 SU_(list.comment, "Comment"),
                 uaList[i].infos[j].comment);
      }
   }
   VGAuth_FreeUserAliasList(num, uaList);

   return err;
}


/*
 ******************************************************************************
 * CliListMapped --                                                      */ /**
 *
 * List all IdProviders in the mapping file.
 *
 * @param[in]  ctx                  The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

static VGAuthError
CliListMapped(VGAuthContext *ctx)
{
   VGAuthError err;
   int num;
   VGAuthMappedAlias *maList;
   int i;
   int j;

   err = VGAuth_QueryMappedAliases(ctx, 0, NULL, &num, &maList);
   if (VGAUTH_E_OK != err) {
      g_printerr(SU_(listmapped.error,
                     "%s: Failed to list mapped aliases: %s.\n"),
                 appName, VGAuth_GetErrorText(err, NULL));
      return err;
   }

   if (verbose) {
      g_print(SU_(listmapped.count, "%s Found %d mapped aliases\n"),
              appName, num);
   }
   for (i = 0; i < num; i++) {
      g_print("%s\n%s:%s\n", maList[i].pemCert,
              SU_(listmapped.username, "Username"),
              maList[i].userName);
      for (j = 0; j < maList[i].numSubjects; j++) {
         g_print("\t%s: %s\n",
                 SU_(listmapped.subject, "Subject"),
                 SubjectName(&(maList[i].subjects[j])));

      }
   }
   VGAuth_FreeMappedAliasList(num, maList);

   return err;
}


/*
 ******************************************************************************
 * InitMsgCatalog --                                                     */ /**
 *
 * Initialize language setting according to machine locale
 *
 ******************************************************************************
 */

static void
InitMsgCatalog(void)
{
   PrefHandle prefs;
   gchar *msgCatalog;

   /*
    * Do this first, so any noise from the locale setup is properly filtered.
    */
   VGAuth_SetLogHandler(CliLog, NULL, 0, NULL);

   /*
    * Find the location of the i18n catalogs.
    */
   setlocale(LC_ALL, "");
   prefs = Pref_Init(VGAUTH_PREF_CONFIG_FILENAME);
   msgCatalog = Pref_GetString(prefs,
                               VGAUTH_PREF_LOCALIZATION_DIR,
                               VGAUTH_PREF_GROUP_NAME_LOCALIZATION,
                               VGAUTH_PREF_DEFAULT_LOCALIZATION_CATALOG);

   I18n_BindTextDomain(VMW_TEXT_DOMAIN,    // domain -- base name of vmsg files
                       NULL,               // locale -- let it figure it out
                       msgCatalog);        // path to message catalogs
   g_free(msgCatalog);
   Pref_Shutdown(prefs);
}


/*
 ******************************************************************************
 * mainRun --                                                            */ /**
 *
 * Initializes and parses commandline args.
 *
 * @param[in]  argc        Number of command line arguments.
 * @param[in]  argv        The command line arguments.
 *
 * @return 0 if the operation ran successfully, -1 if there was an error during
 *         execution.
 *
 ******************************************************************************
 */

#ifdef _WIN32
#define use_glib_parser 0
#else
#define use_glib_parser 1
#endif

static int
mainRun(int argc,
        char *argv[])
{
   VGAuthError err;
   VGAuthContext *ctx = NULL;
   gboolean doAdd = FALSE;
   gboolean doRemove = FALSE;
   gboolean doList = FALSE;
   gboolean doRemoveAll = FALSE;
   gboolean addMapped = FALSE;
   gchar **argvCopy = NULL;
   int argcCopy;
   char *userName = NULL;
   char *pemFilename = NULL;
   gchar *comment = NULL;
   gchar *summaryMsg;
   gchar *noteMsg = NULL;
   gchar *subject = NULL;
   GOptionEntry *cmdOptions = NULL;
   const gchar *paramStr = "[add | list | remove | removeAll]\n";
   const gchar *lUsername = SU_(cmdline.summary.username, "username");
   const gchar *lSubject = SU_(cmdline.summary.subject, "subject");
   const gchar *lPEMfile = SU_(cmdline.summary.pemfile, "PEM-file");
   const gchar *lComm = SU_(cmdline.summary.comm, "comment");
   const gchar *lNote = SU_(cmdline.summary.note,
                            "Note: If no username is provided, "
                            "%s removes only the mapped aliases");

#if (use_glib_parser == 0)
   int i;
#else
   GError *gErr = NULL;
#endif

   GOptionEntry listOptions[] = {
      { "username", 'u', 0, G_OPTION_ARG_STRING, &userName,
         SU_(listoptions.username,
             "User whose certificate store is being queried"), NULL },
      { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
         SU_(listoptions.verbose,
             "Verbose operation"), NULL },
      { NULL }
   };
   GOptionEntry removeOptions[] = {
      { "username", 'u', 0, G_OPTION_ARG_STRING, &userName,
         SU_(removeoptions.username,
             "User whose certificate store is being removed from"), NULL },
      { "file", 'f', 0, G_OPTION_ARG_STRING, &pemFilename,
         SU_(removeoptions.file, "PEM file name"), NULL },
      { "subject", 's', 0, G_OPTION_ARG_STRING, &subject,
         SU_(removeoptions.subject, "The SAML subject"), NULL },
      { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
         SU_(removeoptions.verbose, "Verbose operation"), NULL },
      { NULL }
   };
   GOptionEntry addOptions[] = {
      { "username", 'u', 0, G_OPTION_ARG_STRING, &userName,
         SU_(addoptions.username,
             "User whose certificate store is being added to"), NULL },
      { "file", 'f', 0, G_OPTION_ARG_STRING, &pemFilename,
         SU_(addoptions.file, "PEM file name"), NULL },
      { "subject", 's', 0, G_OPTION_ARG_STRING, &subject,
         SU_(addoptions.subject, "The SAML subject"), NULL },
      { "global", 'g', 0, G_OPTION_ARG_NONE, &addMapped,
         SU_(addoptions.global,
             "Add the certificate to the global mapping file"), NULL },
      { "comment", 'c', 0, G_OPTION_ARG_STRING, &comment,
         SU_(addoptions.comment, "subject comment"), NULL},
      { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
         SU_(addoptions.verbose, "Verbose operation"), NULL },
      { NULL }
   };
   GOptionEntry removeAllOptions[] = {
      { "username", 'u', 0, G_OPTION_ARG_STRING, &userName,
         SU_(removealloptions.username,
             "User whose certificate store is being removed from"), NULL },
      { "subject", 's', 0, G_OPTION_ARG_STRING, &subject,
         SU_(removealloptions.subject, "The SAML subject"), NULL },
      { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
         SU_(removealloptions.verbose, "Verbose operation"), NULL },
      { NULL }
   };
   GOptionContext *context;

   appName = g_path_get_basename(argv[0]);

   /*
    * The option parser needs to modify these, and using the variables
    * coming into main doesn't work.
    */
   argcCopy = argc;
   argvCopy = argv;

   /*
    * Set up the option parser
    */
   g_set_prgname(appName);
   noteMsg = g_strdup_printf(lNote, "removeAll");
   context = g_option_context_new(paramStr);
   summaryMsg = g_strdup_printf(
      "add --global --username=%s --file=%s --subject=%s "
             "[ --comment=%s ]\n"
      "remove --username=%s --file=%s [ --subject=%s ]\n"
      "removeAll --subject=%s [ --username=%s ]\n"
      "list [ --username=%s ]\n\n"
      "%s",
      lUsername, lPEMfile, lSubject, lComm,
      lUsername, lPEMfile, lSubject,
      lSubject, lUsername,
      lUsername,
      noteMsg);

   g_option_context_set_summary(context, summaryMsg);
   g_free(summaryMsg);
   g_free(noteMsg);
   if (argc < 2) {
      Usage(context, paramStr, cmdOptions);
   }

   /*
    * Determine the command and set up the appropriate option table.
    */
   if (strcmp(argvCopy[1], "add") == 0) {
      doAdd = TRUE;
      g_option_context_add_main_entries(context, addOptions, NULL);
      cmdOptions = addOptions;
   } else if (strcmp(argvCopy[1], "remove") == 0) {
      doRemove = TRUE;
      g_option_context_add_main_entries(context, removeOptions, NULL);
      cmdOptions = removeOptions;
   } else if (strcmp(argvCopy[1], "list") == 0) {
      doList = TRUE;
      g_option_context_add_main_entries(context, listOptions, NULL);
      cmdOptions = listOptions;
   } else if (strcmp(argvCopy[1], "removeAll") == 0) {
      doRemoveAll = TRUE;
      g_option_context_add_main_entries(context, removeAllOptions, NULL);
      cmdOptions = removeAllOptions;
   } else {
      Usage(context, paramStr, cmdOptions);
   }

#if (use_glib_parser == 0)
   /*
    * In Windows, g_option_context_parse() does the wrong thing for the
    * incoming Unicode cmdline.  Modern glib (2.40 or later) solves this
    * with g_option_context_parse_strv(), but we're stuck with an older
    * glib for now.
    *
    * So instead lets do it all by hand.
    *
    * This does almost everything for the two types of options this code
    * cares about.  It doesn't handle multiple
    * short options behind a single dash, or '--' alone meaning
    * to stop parsing.
    *
    * XXX fix this when we upgrade glib.
    */
   for (i = 2; i < argc; i++) {
      GOptionEntry *e;
      gboolean match = FALSE;

      e = &cmdOptions[0];
      while (e && e->long_name) {
         gsize len;
         char longName[32];
         char shortName[3];

         g_snprintf(longName, sizeof(longName), "--%s", e->long_name);
         g_snprintf(shortName, sizeof(shortName), "-%c", e->short_name);
         len = strlen(longName);

         // short options don't support '='
         if (strcmp(shortName, argv[i]) == 0) {
            if (e->arg == G_OPTION_ARG_STRING) {
               if (argv[i+1]) {
                  *(gchar **) e->arg_data = argv[++i];
                  match = TRUE;
               } else {
                  Usage(context, paramStr, cmdOptions);
               }
            } else if (e->arg == G_OPTION_ARG_NONE) {
               *(gboolean *) e->arg_data = TRUE;
               match = TRUE;
            }
         } else if (strncmp(longName, argv[i], len) == 0 &&
                   (argv[i][len] == '=' || argv[i][len] == '\0')) {
            gchar *val = NULL;

            if (e->arg == G_OPTION_ARG_STRING) {
               if (argv[i][len] == '=') {
                  val = (gchar *) &(argv[i][len+1]);
               } else if (argv[i+1]) {
                  val = argv[++i];
               }
               *(gchar **) e->arg_data = val;
               match = TRUE;
            } else if ((e->arg == G_OPTION_ARG_NONE) && argv[i][len] == '\0') {
               *(gboolean *) e->arg_data = TRUE;
               match = TRUE;
            } else {
               Usage(context, paramStr, cmdOptions);
            }
         }
         if (match) {
            goto next;
         }
         e++;
      }
next:
      if (!match) {
         Usage(context, paramStr, cmdOptions);
      }
   }
#else
   if (!g_option_context_parse(context, &argcCopy, &argvCopy, &gErr)) {
      g_printerr("%s: %s: %s\n", appName,
                 SU_(cmdline.parse, "Command line parsing failed"),
                 gErr->message);
      g_error_free(gErr);
      exit(-1);
   }
#endif

   /*
    * XXX pull this if we use stdin for the cert contents.
    */
   if (((doAdd || doRemove) && !pemFilename) ||(doRemoveAll && !subject)) {
      Usage(context, paramStr, cmdOptions);
   }

   err = VGAuth_Init(appName, 0, NULL, &ctx);
   if (VGAUTH_E_OK != err) {
      g_printerr("%s\n", SU_(vgauth.init.failed, "Failed to init VGAuth"));
      exit(-1);
   }

   /*
    * XXX
    * If username is unset, should it use the current user?
    * This breaks the model where no username means listMapped.
    * Can we do it just for add/remove, or is that too confusing?
    * Add an explicit listmapped instead?
    */

   if (doAdd) {
      err = CliAddAlias(ctx, userName, subject, pemFilename, addMapped, comment);
   } else if (doRemove) {
      err= CliRemoveAlias(ctx, userName, subject, pemFilename);
   } else if (doList) {
      if (userName) {
         err = CliList(ctx, userName);
      } else {
         err = CliListMapped(ctx);
      }
   } else if (doRemoveAll) {
      err = CliRemoveAllAlias(ctx, subject, userName);
   }

   VGAuth_Shutdown(ctx);
   g_free(appName);
   return (err == VGAUTH_E_OK) ? 0 : -1;
}


#ifdef _WIN32


/*
 ******************************************************************************
 * wmain --                                                              */ /**
 *
 * Initializes and parses commandline args.
 *
 * @param[in]  argc        Number of command line arguments.
 * @param[in]  argv        The command line arguments in unicode.
 *
 * @return 0 if the operation ran successfully, -1 if there was an error during
 *         execution.
 *
 ******************************************************************************
 */

int
wmain(int argc,
      wchar_t *argv[])
{
   int retval = -1;
   int i;
   char **argvUtf8 = g_malloc0((argc + 1) * sizeof (char*));

   for (i = 0; i < argc; ++i) {
      CHK_UTF16_TO_UTF8(argvUtf8[i], argv[i], goto end);
   }

   InitMsgCatalog();

   retval = mainRun(argc, argvUtf8);

end:

   for (i = 0; i < argc; ++i) {
      g_free(argvUtf8[i]);
   }

   g_free(argvUtf8);

   return retval;
}

#else


/*
 ******************************************************************************
 * main --                                                               */ /**
 *
 * Initializes and parses commandline args.
 *
 * @param[in]  argc        Number of command line arguments.
 * @param[in]  argv        The command line arguments.
 *
 * @return 0 if the operation ran successfully, -1 if there was an error during
 *         execution.
 *
 ******************************************************************************
 */

int
main(int argc,
     char *argv[])
{
   InitMsgCatalog();
   return mainRun(argc, argv);
}

#endif
