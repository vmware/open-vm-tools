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
 * @file VGAuthLog.c --
 *
 *       Log functions.
 */

#ifdef _WIN32
#include <windows.h>
#else
/*
 * Need GNU definition of strerror_r for better compatibility
 * across different glibc versions.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>
#include <string.h>
#endif
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "VGAuthBasicDefs.h"
#include "VGAuthLog.h"
#ifdef _WIN32
#include "winUtil.h"
#endif


#ifdef _WIN32

/*
 ******************************************************************************
 * LogErrorWinCodeV--                                                    */ /**
 *
 * Log an error message
 *
 * @param[in]   code       The Windows system error code
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 * @param[in]   args       The printf like var args
 *
 ******************************************************************************
 */

static void
LogErrorWinCodeV(int code,
                 const char *func,
                 const char *file,
                 unsigned line,
                 const char *fmt,
                 va_list args)
{
   char *msgBuf = WinUtil_GetErrorText(code);
   gchar buf[4096];

   g_vsnprintf(buf, sizeof buf, fmt, args);
   buf[sizeof buf - 1] = '\0';

   g_warning("[function %s, file %s, line %d], %s, [Win32 Error = %d] %s\n",
             func, file, line, buf, code, (const char *)msgBuf);

   g_free(msgBuf);
}


/*
 ******************************************************************************
 * LogErrorWinCode --                                                    */ /**
 *
 * Log an error message
 *
 * @param[in]   code       The Windows system error code
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 *
 ******************************************************************************
 */

void LogErrorWinCode(int code,
                     const char *func,
                     const char *file,
                     unsigned line,
                     const char *fmt,
                     ...)
{
   va_list args;

   va_start(args, fmt);
   LogErrorWinCodeV(code, func, file, line, fmt, args);
   va_end(args);
}


/*
 ******************************************************************************
 * LogErrorWin --                                                        */ /**
 *
 * Log an error message after a Windows system API call
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 *
 ******************************************************************************
 */

void LogErrorWin(const char *func,
                 const char *file,
                 unsigned line,
                 const char *fmt,
                 ...)
{
   int code = GetLastError();
   va_list args;

   va_start(args, fmt);
   LogErrorWinCodeV(code, func, file, line, fmt, args);
   va_end(args);
}


/*
 ******************************************************************************
 * LogErrorPosixCodeV--                                                  */ /**
 *
 * Log an error message
 *
 * @param[in]   code       The Posix error code (errno)
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 * @param[in]   args       The printf like var args
 *
 ******************************************************************************
 */

static void
LogErrorPosixCodeV(int code,
                   const char *func,
                   const char *file,
                   unsigned line,
                   const char *fmt,
                   va_list args)
{
   char errMsg[4096];
   gchar buf[4096];

   g_vsnprintf(buf, sizeof buf, fmt, args);
   buf[sizeof buf - 1] = '\0';
   strerror_s(errMsg, sizeof errMsg, code);
   g_warning("[function %s, file %s, line %d], %s, [errno = %d] %s\n",
             func, file, line, buf, code, errMsg);
}

#else

/*
 ******************************************************************************
 * LogErrorPosixCodeV--                                                  */ /**
 *
 * Log an error message
 *
 * @param[in]   code       The Posix error code (errno)
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 * @param[in]   args       The printf like var args
 *
 ******************************************************************************
 */

static void
LogErrorPosixCodeV(int code,
                   const char *func,
                   const char *file,
                   unsigned line,
                   const char *fmt,
                   va_list args)
{
   char errMsg[4096];
   gchar buf[4096];

   g_vsnprintf(buf, sizeof buf, fmt, args);
   buf[sizeof buf - 1] = '\0';

#ifdef sun
   strerror_r(code, errMsg, sizeof errMsg);
   g_warning("[function %s, file %s, line %d], %s, [errno = %d], %s\n",
             func, file, line, buf, code, errMsg);
#else
   g_warning("[function %s, file %s, line %d], %s, [errno = %d], %s\n",
             func, file, line, buf, code,
             strerror_r(code, errMsg, sizeof errMsg));
#endif
}

#endif


/*
 ******************************************************************************
 * LogErrorPosixCode --                                                  */ /**
 *
 * Log an error message
 *
 * @param[in]   code       The Posix error code (errno)
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 *
 ******************************************************************************
 */

void LogErrorPosixCode(int code,
                       const char *func,
                       const char *file,
                       unsigned line,
                       const char *fmt,
                       ...)
{
   va_list args;

   va_start(args, fmt);
   LogErrorPosixCodeV(code, func, file, line, fmt, args);
   va_end(args);
}


/*
 ******************************************************************************
 * LogErrorPosix --                                                      */ /**
 *
 * Log an error message after a Posix API call
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 *
 ******************************************************************************
 */

void LogErrorPosix(const char *func,
                   const char *file,
                   unsigned line,
                   const char *fmt,
                   ...)
{
   int code = errno;
   va_list args;

   va_start(args, fmt);
   LogErrorPosixCodeV(code, func, file, line, fmt, args);
   va_end(args);
}


/*
 ******************************************************************************
 * LogInfo --                                                           */ /**
 *
 * Log an information message
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 *
 ******************************************************************************
 */

void LogInfo(const char *func,
             const char *file,
             unsigned line,
             const char *fmt,
             ...)
{
   gchar buf[4096];

   va_list args;
   va_start(args, fmt);
   g_vsnprintf(buf, sizeof buf, fmt, args);
   buf[sizeof buf - 1] = '\0';
   va_end(args);

   g_message("[function %s, file %s, line %d], %s\n",
             func, file, line, buf);
}


/*
 ******************************************************************************
 * LogWarning --                                                         */ /**
 *
 * Log an error message
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 *
 ******************************************************************************
 */

void LogWarning(const char *func,
                const char *file,
                unsigned line,
                const char *fmt,
                ...)
{
   gchar buf[4096];

   va_list args;
   va_start(args, fmt);
   g_vsnprintf(buf, sizeof buf, fmt, args);
   buf[sizeof buf - 1] = '\0';
   va_end(args);

   g_warning("[function %s, file %s, line %d], %s\n",
             func, file, line, buf);
}


/*
 ******************************************************************************
 * LogDebug --                                                           */ /**
 *
 * Log a debug message
 *
 * @param[in]   func       The function name
 * @param[in]   file       The file name
 * @param[in]   line       The line number
 * @param[in]   fmt        The printf like format string
 *
 ******************************************************************************
 */

void LogDebug(const char *func,
              const char *file,
              unsigned line,
              const char *fmt,
              ...)
{
   gchar buf[4096];

   va_list args;
   va_start(args, fmt);
   g_vsnprintf(buf, sizeof buf, fmt, args);
   buf[sizeof buf - 1] = '\0';
   va_end(args);

   g_debug("[function %s, file %s, line %d], %s\n",
           func, file, line, buf);
}
