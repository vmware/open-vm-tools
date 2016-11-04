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
 * cert_util.c --
 *
 *    Utilities to manage the certificates.
 */

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include "cert_util.h"

/*
 *----------------------------------------------------------------------
 *
 * CompareFile --
 *
 *    Check if two files are the same.
 *
 * Results:
 *    TRUE if file comparison is performed successfully, otherwise
 *    FALSE. When the returned value is TRUE, 'same' is TRUE if two
 *    input files are the same, otherwise FALSE. When the returned
 *    value is FALSE, 'same' is not defined.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
CompareFile(const gchar *fname1,                 // IN
            const gchar *fname2,                 // IN
            gboolean *same)                      // OUT
{
   gsize num;
   gboolean ret = FALSE;
   GMappedFile *m1;
   GMappedFile *m2 = NULL;
   GError *error = NULL;

   m1 = g_mapped_file_new(fname1, FALSE, &error);
   if (m1 == NULL) {
      Error("Unable to map %s: %s.\n", fname1, error->message);
      goto exit;
   }

   m2 = g_mapped_file_new(fname2, FALSE, &error);
   if (m2 == NULL) {
      Error("Unable to map %s: %s.\n", fname2, error->message);
      goto exit;
   }

   ret = TRUE;
   *same = FALSE;

   num = g_mapped_file_get_length(m1);
   if (g_mapped_file_get_length(m2) == num) {
      if (num) {
         if (memcmp(g_mapped_file_get_contents(m1),
                    g_mapped_file_get_contents(m2), num) == 0) {
            *same = TRUE;
         }
      } else {
         /* Two empty files */
         *same = TRUE;
      }
   }

exit:
   g_clear_error(&error);
   if (m1) {
      g_mapped_file_unref(m1);
   }
   if (m2) {
      g_mapped_file_unref(m2);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * CertUtil_CreateCertFileName --
 *
 *    A convenient function to make up the certificate file name based
 *    on the supplied guest proxy certificate store (certDir), subject
 *    name hash (hash), and certificate version (version).
 *
 * Results:
 *    Return the full path name of the certificate. Callers should free
 *    the returned string.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gchar *
CertUtil_CreateCertFileName(const gchar *certDir, // IN
                            const gchar *hash,    // IN
                            int version)          // IN
{
   gchar *ret;
   gchar *tmp;

   tmp = g_strdup_printf("%s.%d", hash, version);
   ret = g_build_filename(certDir, tmp, NULL);

   g_free(tmp);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * IntCmp --
 *
 *    This is an integer comparator, which is used to sort a list
 *    with ascending order.
 *
 * Results:
 *    The difference of ga and gb.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gint
IntCmp(gconstpointer ga,                         // IN
       gconstpointer gb)                         // IN
{
   gint a = GPOINTER_TO_INT(ga);
   gint b = GPOINTER_TO_INT(gb);

   return (a - b);
}


/*
 *----------------------------------------------------------------------
 *
 * MatchFile --
 *
 *    Scan each file at the directory and collect file extensions of
 *    matched files. Sort the file extension list in ascending order.
 *
 * Results:
 *    Return a list of file extensions, their file names matching the
 *    regular expression. These extensions are file version numbers.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static GList *
MatchFile(GDir *dir,                             // IN
          GRegex *regExpr)                       // IN
{
   const gchar *fn, *cp;
   GList *list = NULL;

   while ((fn = g_dir_read_name(dir)) != NULL) {
      if (g_regex_match(regExpr, fn, 0, NULL)) {

         cp = strrchr(fn, '.');
         list = g_list_prepend(list, GINT_TO_POINTER(atoi(cp + 1)));
      }
   }

   list = g_list_sort(list, IntCmp);

   return list;
}


/*
 *----------------------------------------------------------------------
 *
 * SearchFile --
 *
 *    Search files with pattern (<fname>.[0-9]+) at a directory <path>.
 *
 * Results:
 *    Return TRUE if file search is performed successfully, otherwise
 *    FALSE. When the returned value is TRUE, 'list' is set to include
 *    a list of file extensions matching the pattern.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
SearchFile(const gchar *path,                    // IN
           const gchar *fname,                   // IN
           GList **list)                         // OUT
{
   gboolean ret = FALSE;
   gchar *pattern;
   GRegex *regExpr;
   GDir *dir = NULL;
   GError *error = NULL;

   pattern = g_strdup_printf("%s.[0-9]+", fname);
   regExpr = g_regex_new(pattern, 0, 0, &error);
   if (!regExpr) {
      Error("Failed to compile %s: %s.\n", pattern, error->message);
      goto exit;
   }

   dir = g_dir_open(path, 0, &error);
   if (!dir) {
      Error("Failed to open %s: %s.\n", path, error->message);
      goto exit;
   }

   *list = MatchFile(dir, regExpr);
   ret = TRUE;

exit:
   g_free(pattern);
   g_clear_error(&error);
   if (dir) {
      g_dir_close(dir);
   }
   if (regExpr) {
      g_regex_unref(regExpr);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * CertUtil_FindCert --
 *
 *    From the trusted certificate directory (certDir), check if
 *    there is any certificate file matching the contents of the
 *    supplied one. In general, certificate files are saved in the
 *    directory by the format of <hash>.[0-9]+.
 *
 * Results:
 *    Return TRUE if the function is successfully executed. Otherwise
 *    FALSE. When return TRUE, 'num' is set to the version of matching
 *    certificate file or -1 if no matching. For 'last', it is set to
 *    the highest version, or -1 if the store has no certificate file.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gboolean
CertUtil_FindCert(const gchar *certFile,          // IN
                  const gchar *certDir,           // IN
                  const gchar *hash,              // IN
                  int *num,                       // OUT
                  int *last)                      // OUT
{
   gboolean ret = FALSE;
   const GList *node;
   GList *list = NULL;
   gchar *path = NULL;

   *last = *num = -1;
   if (!SearchFile(certDir, hash, &list)) {
      goto exit;
   }

   ret = TRUE;
   if (!list) {
      goto exit;
   }

   /* *last = the highest file version */
   node = g_list_last(list);
   *last = GPOINTER_TO_INT(node->data);

   for (node = g_list_first(list); node; node = g_list_next(node)) {
      gboolean same = FALSE;
      int ext = GPOINTER_TO_INT(node->data);

      g_free(path);
      path = CertUtil_CreateCertFileName(certDir, hash, ext);

      if (!CompareFile(certFile, path, &same)) {
         ret = FALSE;
         goto exit;
      }

      if (same) {
         *num = ext;
         break;
      }
   }

exit:
   g_free(path);
   if (list) {
      g_list_free(list);
   }

   return ret;
}


#ifndef _WIN32
/*
 *----------------------------------------------------------------------
 *
 * CertUtil_GetToolDir --
 *
 *    Get the VMware tool installation directory.
 *
 * Results:
 *    The VMware tool installation directory. Callers should not free
 *    the returned string.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

const gchar *
CertUtil_GetToolDir(void)
{
   static gchar *path = NULL;

   if (!path) {
      path = g_build_filename(G_DIR_SEPARATOR_S, "etc", "vmware-tools", NULL);
   }

   return path;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * CertUtil_CopyFile --
 *
 *    Copy a file from source to destination.
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
CertUtil_CopyFile(const gchar *src,              // IN
                  const gchar *dst)              // IN
{
   gsize length;
   gboolean ret = FALSE;
   GMappedFile *smap;
   GError *error = NULL;
   FILE *file = NULL;
   const gchar *content;

   smap = g_mapped_file_new(src, FALSE, &error);
   if (!smap) {
      Error("Unable to map %s: %s.\n", src, error->message);
      goto exit;
   }

   file = g_fopen(dst, "w");
   if (!file) {
      Error("Failed to open %s: %s.\n", dst, strerror(errno));
      goto exit;
   }

   length = g_mapped_file_get_length(smap);
   content = g_mapped_file_get_contents(smap);
   if (fwrite(content, 1, length, file) < length) {
      Error("Failed to copy %s to %s: %s.\n", src, dst, strerror(errno));
      goto exit;
   }

   ret = TRUE;

exit:
   g_clear_error(&error);
   if (smap) {
      g_mapped_file_unref(smap);
   }
   if (file) {
      fclose(file);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * CertUtil_RemoveDir --
 *
 *    Remove a directory. This directory can be non-empty. When it is
 *    non-empty, all of its files and subdirectories are removed too.
 *
 * Results:
 *    TRUE if the directory is successfully removed, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

gboolean
CertUtil_RemoveDir(const gchar *dirToRemove)
{
   gboolean ret = FALSE;
   GDir *dir = NULL;
   const gchar *file;
   GError *error = NULL;
   gchar *fname = NULL;

   if ((dir = g_dir_open(dirToRemove, 0, &error)) == NULL) {
      Error("Failed to open %s: %s.\n", dirToRemove, error->message);
      goto exit;
   }

   while ((file = g_dir_read_name(dir)) != NULL) {
      g_free(fname);
      fname = g_build_filename(dirToRemove, file, NULL);

      if (g_file_test(fname, G_FILE_TEST_IS_DIR)) {
         if (!CertUtil_RemoveDir(fname)) {
            Error("Couldn't remove the directory '%s'.\n", fname);
            goto exit;
         }
      } else if (g_remove(fname) < 0) {
         Error("Couldn't remove the file '%s'.\n", fname);
         goto exit;
      }
   }

   g_dir_close(dir);
   dir = NULL;

   if (g_rmdir(dirToRemove) < 0) {
      Error("Couldn't remove the directory '%s'.\n", dirToRemove);
      goto exit;
   }

   ret = TRUE;

exit:
   g_free(fname);
   g_clear_error(&error);
   if (dir) {
      g_dir_close(dir);
   }

   return ret;
}
