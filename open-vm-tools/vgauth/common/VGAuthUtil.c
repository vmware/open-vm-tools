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
 * @file VGAuthUtil.c --
 *
 *       utility functions.
 */
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "VGAuthBasicDefs.h"
#include "VGAuthLog.h"
#include "VGAuthUtil.h"

#ifdef _WIN32
#include <windows.h>
#endif


/*
 ******************************************************************************
 * HangThread --                                                         */ /**
 *
 * Hang the current thread.
 * Debug function to help diagnose IPC issues and unit tests such as pipe full.
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 *
 ******************************************************************************
 */

void HangThread(const char *func,
                const char *file,
                unsigned line)
{
   LogDebug(func, file, line, "Hang the calling thread");

   do {
      g_usleep(1000 * G_USEC_PER_SEC);
   } while (TRUE);
}


#ifdef _WIN32
/*
 ******************************************************************************
 * Convert_Utf8ToUtf16 --                                                */ /**
 *
 * Convert a NULL terminated UTF8 string to a NULL terminiated UTF16 string
 * Log an error if the conversion fails.
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   str        The input NULL terminated UTF8 string
 *
 * @return a utf16 string result
 *
 ******************************************************************************
 */

gunichar2 *
Convert_Utf8ToUtf16(const char *func,
                    const char *file,
                    unsigned line,
                    const gchar *str)
{
   glong nRead = 0;
   glong nWritten = 0;
   GError *pError = NULL;
   gunichar2 *result = g_utf8_to_utf16(str, -1, &nRead, &nWritten, &pError);

   if (!result) {
      LogWarning(func, file, line, "g_utf8_to_utf16() failed, %s, "
                 "read %d byte(s), written %d wchar(s)", pError->message,
                 (int)nRead, (int)nWritten);
      g_clear_error(&pError);
   }

   return result;
}


/*
 ******************************************************************************
 * Convert_Utf16ToUtf8 --                                                */ /**
 *
 * Convert a NULL terminated UTF16 string to a NULL terminated UTF8 string
 * Log an error if the conversion fails.
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   str        The input NULL terminated UTF16 string
 *
 * @return a utf8 string result
 *
 ******************************************************************************
 */

gchar *
Convert_Utf16ToUtf8(const char *func,
                    const char *file,
                    unsigned line,
                    const gunichar2 *str)
{
   glong nRead = 0;
   glong nWritten = 0;
   GError *pError = NULL;
   gchar *result = g_utf16_to_utf8(str, -1, &nRead, &nWritten, &pError);

   if (!result) {
      LogWarning(func, file, line, "g_utf16_to_utf8() failed, %s, "
                 "read %d wchar(s), written %d byte(s)", pError->message,
                 (int)nRead, (int)nWritten);
      g_clear_error(&pError);
   }

   return result;
}


/*
 ******************************************************************************
 * Convert_TextToUnsignedInt32 --                                        */ /**
 *
 * Convert a NULL terminated ascii string to a unsigned 32bit number
 * Log an error if the conversion fails
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   str        The input NULL terminated ascii string
 * @param[out]  result     The converted unsigned 32bit value
 *
 * @return TRUE on sucess, FALSE otherwise
 *
 ******************************************************************************
 */

gboolean
Convert_TextToUnsignedInt32(const char *func,
                            const char *file,
                            unsigned line,
                            const char *str,
                            unsigned int *result)
{
   /*
    * On 32/64bit Windows, sizeof(unsigned long) = sizeof(unsigned int) = 4
    * On 32bit Linux, sizeof(unsigned long) = sizeof(unsigned int) = 4
    * On 64bit Linux, sizeof(unsigned long) = 8, sizeof(unsigned int) = 4
    */
   unsigned long value;
   char *stopChar;

   value = strtoul(str, &stopChar, 10);
   if (value == 0 || value == ULONG_MAX) {
      LogErrorPosix(func, file, line, "strtoul(%s) failed", str);
      return FALSE;
   }

#ifndef _WIN32
   if (!Check_Is32bitNumber((size_t)value)) {
      LogWarning(func, file, line, "Convert to uint32 overflowed, input = %s",
                 str);
      return FALSE;
   }
#endif

   *result = (unsigned int)value;

   return TRUE;
}


/*
 ******************************************************************************
 * Check_Is32bitNumber --                                               */ /**
 *
 * Check if the number is a 32bit number
 *
 * @param[in]   number     The number to check
 *
 * @note   size_t is 64bit on a 64bit system, and 32bit on a 32bit system
 *
 * @return TRUE if the number is a 32bit number, FALSE otherwise.
 *
 ******************************************************************************
 */

gboolean
Check_Is32bitNumber(size_t number)
{
   unsigned long long number64 = (unsigned long long)number;

   /*
    * Cast to 64bit to shift 32bit
    * Otherwise, the C compiler warns and generates no-op code
    */
   number64 >>=32;

   return (number64 == 0);
}


/*
 ******************************************************************************
 * Convert_UnsignedInt32ToText --                                       */ /**
 *
 * Convert a unsigned 32bit number to its text representation
 * Log an error if the conversion fails
 *
 * @param[in]   number       The number to convert
 *
 * @return   the result ascii string
 *
 ******************************************************************************
 */

gchar *
Convert_UnsignedInt32ToText(unsigned int number)
{
   return g_strdup_printf("%d", number);
}
#endif  // #ifdef _WIN32


/*
 ******************************************************************************
 * Util_CheckExpiration --                                               */ /**
 *
 * Checks whether, given a start time and duration, the current time is
 * passed that duration.
 *
 * @param[in]  start     The start time.
 * @param[in]  duration  The timeout in seconds.
 *
 * @return  TRUE if the current time is passed the duration or FALSE otherwise.
 *
 ******************************************************************************
 */

gboolean
Util_CheckExpiration(const GTimeVal *start,
                     unsigned int duration)
{
   GTimeVal now;
   GTimeVal expire;

   /*
    * XXX blech.  Move to GDateTime as soon as the toolchain glib
    * supports it (version 2.26).
    */
   g_get_current_time(&now);
   expire = *start;

   expire.tv_sec += duration;

   /*
    * We don't need the precision, so just ignore micros.
    */
   return now.tv_sec > expire.tv_sec;
}


/*
 ******************************************************************************
 * Util_Assert --                                                        */ /**
 *
 * Asserts after spewing some location data.
 *
 * @param[in]  cond      The assertion cause.
 * @param[in]  file      The file.
 * @param[in]  lineNum   The line number.
 *
 ******************************************************************************
 */

void
Util_Assert(const char *cond,
            const char *file,
            int lineNum)
{
   g_warning("Assertion '%s' failed at %s:%d\n", cond, file, lineNum);
#ifdef _WIN32
#ifdef VMX86_DEBUG
   DebugBreak();
#endif
#endif
   g_assert(0);
}
