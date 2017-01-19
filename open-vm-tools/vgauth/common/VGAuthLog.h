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
 * @file VGAuthLog.h --
 *
 *    Convenient log macros and declarations
 */

#ifndef _VGAUTH_LOG_H_
#define _VGAUTH_LOG_H_

#include "VGAuthBasicDefs.h"

/* Windows functions and macros*/
#ifdef _WIN32

void LogErrorWin(const char *func, const char *file,
                 unsigned line, const char *fmt, ...) PRINTF_DECL(4, 5);
#define VGAUTH_LOG_ERR_WIN(fmt, ...) do {                                      \
      LogErrorWin(__FUNCTION__, __FILE__, __LINE__, fmt, __VA_ARGS__);  \
   } while (0)

void LogErrorWinCode(int code, const char *func, const char *file,
                     unsigned line, const char *fmt, ...) PRINTF_DECL(5, 6);

#define VGAUTH_LOG_ERR_WIN_CODE(code, fmt, ...) do {                  \
      LogErrorWinCode(code, __FUNCTION__, __FILE__, __LINE__,  \
                      fmt, __VA_ARGS__);                       \
   } while (0)

#endif /* _WIN32 */

/* Common functions and macros */

/*
 * These differ from Warning(), Log() and Debug() by adding the
 * __FUNCTION__, __FILE__ and __LINE__ for you.
 */

void LogErrorPosix(const char *func, const char *file,
                   unsigned line, const char *fmt, ...) PRINTF_DECL(4, 5);
#define VGAUTH_LOG_ERR_POSIX(fmt, ...) do {                                    \
      LogErrorPosix(__FUNCTION__, __FILE__, __LINE__, fmt, __VA_ARGS__); \
   } while (0)

void LogErrorPosixCode(int code, const char *func, const char *file,
                       unsigned line, const char *fmt, ...) PRINTF_DECL(5, 6);

#define VGAUTH_LOG_ERR_POSIX_CODE(code, fmt, ...) do {                 \
      LogErrorPosixCode(code, __FUNCTION__, __FILE__, __LINE__, \
                        fmt, __VA_ARGS__);                      \
   } while (0)


/*
 * Logs a message at Log()/g_message() level.
 */
void LogInfo(const char *func, const char *file,
             unsigned line, const char *fmt, ...) PRINTF_DECL(4, 5);
#define VGAUTH_LOG_INFO(fmt, ...) do {                                         \
      LogInfo(__FUNCTION__, __FILE__, __LINE__, fmt, __VA_ARGS__);      \
   } while (0)


/*
 * Logs an error at Warning/g_warning() level.
 */
void LogWarning(const char *func, const char *file,
                unsigned line, const char *fmt, ...) PRINTF_DECL(4, 5);
#define VGAUTH_LOG_WARNING(fmt, ...) do {                                      \
      LogWarning(__FUNCTION__, __FILE__, __LINE__, fmt, __VA_ARGS__);   \
   } while (0)


/*
 * Logs a message at Debug/g_debug() level.
 */
void LogDebug(const char *func, const char *file,
              unsigned line, const char *fmt, ...) PRINTF_DECL(4, 5);
#define VGAUTH_LOG_DEBUG(fmt, ...) do {                                      \
      LogDebug(__FUNCTION__, __FILE__, __LINE__, fmt, __VA_ARGS__);   \
   } while (0)

#endif
