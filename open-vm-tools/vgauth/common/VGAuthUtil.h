/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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
 * @file VGAuthUtil.h --
 *
 *    Convenient utility macros and declarations
 */

#ifndef _VGAUTH_UTIL_H_
#define _VGAUTH_UTIL_H_

#include <glib.h>

/* For unit testing the code */
void HangThread(const char *func, const char *file, unsigned line);
#define HANG_THREAD() do {                                              \
      HangThread(__FUNCTION__, __FILE__, __LINE__);                     \
   } while (0)

#ifdef _WIN32
gunichar2 *Convert_Utf8ToUtf16(const char *func, const char *file,
                               unsigned line, const gchar *str);
#define CHK_UTF8_TO_UTF16(utf16Out, utf8In, onErrStmt)  do {            \
      utf16Out = Convert_Utf8ToUtf16(__FUNCTION__, __FILE__, __LINE__,  \
                                     utf8In);                           \
      if (!utf16Out) {                                                  \
         onErrStmt;                                                     \
      }                                                                 \
   } while(0)

gchar *Convert_Utf16ToUtf8(const char *func, const char *file,
                           unsigned line, const gunichar2 *str);
#define CHK_UTF16_TO_UTF8(utf8Out, utf16In, onErrStmt)  do {            \
      utf8Out = Convert_Utf16ToUtf8(__FUNCTION__, __FILE__, __LINE__,   \
                                    utf16In);                           \
      if (!utf8Out) {                                                   \
         onErrStmt;                                                     \
      }                                                                 \
   } while(0)

gboolean Convert_TextToUnsignedInt32(const char *func, const char *file,
                                     unsigned line, const char *repr,
                                     unsigned int *result);
#define CHK_TEXT_TO_UINT32(uint32Out, textIn, onErrStmt)  do {          \
      gboolean ok = Convert_TextToUnsignedInt32(__FUNCTION__, __FILE__, \
                                                __LINE__, textIn,       \
                                                &uint32Out);            \
      if (!ok) {                                                        \
         onErrStmt;                                                     \
      }                                                                 \
   } while(0)

gboolean Check_Is32bitNumber(size_t number);

gchar *Convert_UnsignedInt32ToText(unsigned int number);
#endif // #ifdef _WIN32

gboolean Util_CheckExpiration(const GTimeVal *start, unsigned int duration);

/*
 * Converts a utf8 path into the local encoding.  No-op on Windows.
 */
#ifdef _WIN32
#define GET_FILENAME_LOCAL(path, err) (gchar *) path
#define RELEASE_FILENAME_LOCAL(path) (void) (path)
#define DIRSEPS "\\"
#else
#define GET_FILENAME_LOCAL(path, err) g_filename_from_utf8((path), -1, NULL, NULL, (err))
#define RELEASE_FILENAME_LOCAL(path) g_free(path)
#define DIRSEPS "/"
#endif

/*
 * Macro literal fun.
 *
 * Given:
 *
 * #define FOO foo
 *
 * Then:
 *
 * MAKESTR(FOO) becomes "FOO"
 * XTR(FOO) becomes "foo"
 */
#ifndef MAKESTR
#define MAKESTR(x) #x
#define XSTR(x) MAKESTR(x)
#endif

void Util_Assert(const char *cond, const char *file, int lineNum);

#endif
