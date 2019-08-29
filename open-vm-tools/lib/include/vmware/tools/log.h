/*********************************************************
 * Copyright (C) 2011-2019 VMware, Inc. All rights reserved.
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

#ifndef _VMTOOLS_LOG_H_
#define _VMTOOLS_LOG_H_

/**
 * @file log.h
 *
 * Some wrappers around glib log functions, expanding their functionality to
 * support common usage patterns at VMware.
 *
 * @defgroup vmtools_logging Logging
 * @{
 *
 * The Tools logging facility is built on top of glib's logging functions
 * (http://developer.gnome.org/glib/stable/glib-Message-Logging.html). Some
 * convenience macros built on top of glib's existing macros are also provided.
 *
 * Logging is configurable on a per-domain basis. The configuration options
 * for each domain are:
 *
 *    - level: minimum log level to log. Also used to declare specific log
 *      domain configurations.
 *      - Valid values: error, critical, warning, message, info, debug, none
 *      - This value is required when configuring a domain.
 *    - handler: the handler to use when logging.
 *      - Valid values: std, outputdebugstring (Win32-only), file, file+ (same as
 *        "file", but appends to existing log file), vmx, syslog.
 *      - Default: "syslog".
 *
 * For file handlers, the following extra configuration information can be
 * provided:
 *
 *    - data: path to the log file, required.
 *    - maxOldLogFiles: maximum number of rotated log files to keep around. By
 *      default, at most 10 backed up log files will be kept. Value should be >= 1.
 *    - maxLogSize: maximum size of each log file, defaults to 10 (MB). A value of
 *      0 disables log rotation.
 *
 * When using syslog on Unix, the following options are available:
 *
 *    - facility: either of "daemon", "user" or "local[0-7]". Controls whether to
 *      connect to syslog as LOG_DAEMON, LOG_USER or LOG_LOCAL[0-7], respectively
 *      (see syslog(3)). Defaults to "user". Any unknown value is mapped to
 *      LOG_USER. This option should be defined for the application's default
 *      log domain (it's ignored for all other domains).
 *
 * The "vmx" logger will log all messages to the host; it's not recommended
 * for normal use, since writing to the host log is an expensive operation and
 * can also affect other running applications that need to send messages to the
 * host. Do not use this logger unless explicitly instructed to do so.
 *
 * Log levels:
 *
 * glib log levels are supported.  The error levels from
 * most to least severe:
 *
 * 'error' - fatal errors
 * 'critical' - critical errors
 * 'warning' - something unexpected happened (useful when an error will
 *            be reported back.)
 * 'message' - messages about services starting, version data
 * 'info'    - informational and diagnostic messages.
 * 'debug'   - debug messages, typically only of interest ot a developer
 *
 *
 * Until vSphere 6.0, the default logging level for beta/rel is 'warning'.
 * Since vsphere 6.0 it is 'message'.
 *
 * When adding new logging messages, be sure to use the appropriate
 * level to balance the amount of logging and usability.  The goal
 * is to be able to debug a customer problem with the default log level
 * whenever possible, while not filling up logfiles with noise
 * or customer-sensitive data.
 *
 *
 * Logging configuration should be under the "[logging]" group in the
 * application's configuration file.
 *
 * Each application can specify a default log domain (which defaults to
 * "vmtools"). If no handler is specified for a particular domain when
 * logging, the default handler will be used. The default logging level
 * for the default domain is "warning" in non-debug builds, and "message"
 * in debug builds.
 *
 * Example of logging configuration in the config file:
 *
 * @verbatim
 * [logging]
 * # Turns on logging globally. It can still be disabled for each domain.
 * log = true
 *
 * # Disables core dumps on fatal errors; they're enabled by default.
 * enableCoreDump = false
 *
 * # Defines the "vmsvc" domain, logging to stdout/stderr.
 * vmsvc.level = info
 * vmsvc.handler = std
 *
 * # Defines the "unity" domain, logging to a file.
 * unity.level = warning
 * unity.handler = file
 * unity.data = /tmp/unity.log
 *
 * # Defines the "vmtoolsd" domain, and disable logging for it.
 * vmtoolsd.level = none
 * @endverbatim
 *
 * Log file names can contain references to pre-defined variables. The following
 * variables are expanded when determining the path of the log file:
 *
 *    - @a ${USER}: expands to the current user's login name
 *    - @a ${PID}: expands to the current process's ID.
 *    - @a ${IDX}: expands to the log file index (for rolling logs).
 *
 * So, for example, @a log.${USER}.${PID}.txt would expand to "log.jdoe.1234.txt"
 * for user "jdoe" if the process ID were 1234.
 * */

#if !defined(G_LOG_DOMAIN)
#  error "G_LOG_DOMAIN must be defined."
#endif

#include <glib.h>

#if defined(__GNUC__)
#  define FUNC __func__
#else
#  define FUNC __FUNCTION__
#endif

/*
 *******************************************************************************
 * g_info --                                                              */ /**
 *
 * Log a message with G_LOG_LEVEL_INFO; this function is missing in glib < 2.39
 * for whatever reason.
 *
 * @param[in]  fmt   Log message format.
 * @param[in]  ...   Message arguments.
 *
 *******************************************************************************
 */
#if !defined(g_info)
#  define g_info(fmt, ...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, fmt, ## __VA_ARGS__)
#endif

/** default logging level */
#ifdef VMX86_DEBUG
#define VMTOOLS_LOGGING_LEVEL_DEFAULT "info"
#else
#define VMTOOLS_LOGGING_LEVEL_DEFAULT "message"
#endif


/*
 * As of version 2.46, glib thinks the Windows compiler where
 * _MSC_VER >= 1400 can handle G_HAVE_ISO_VARARGS.  This makes our
 * magic macros wrapping their macros fail in the simplest case,
 * where only the fmt arg is present (eg vm_debug("test").
 * vm_debug("test %d", 123) works fine.
 *
 * Work around this by making g_debug() et all be inline functions,
 * which is how it works if G_HAVE_ISO_VARARGS isn't set.
 *
 * Though experimentation we found that this also works:
 *
 * #define LOGHELPER(...)  ,##_VA_ARGS__
 * #define LOG(fmt, ...) g_debug_macro(__FUNCTION__, ": " fmt, LOGHELPER(__VA_ARGS__))
 *
 * but since its disgusting and even more magical, the inline variant was chosen
 * instead.
 */

#if defined(_WIN32) && GLIB_CHECK_VERSION(2, 46, 0)
static inline void
g_critical_inline(const gchar *fmt,
               ...)
{
   va_list args;
   va_start(args, fmt);
   g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, fmt, args);
   va_end(args);
}

/*
 *******************************************************************************
 * vm_{critical,debug,error,info,message,warning} --                      */ /**
 *
 * Wrapper around the corresponding glib function that automatically includes
 * the calling function name in the log message. The "fmt" parameter must be
 * a string constant.
 *
 * @param[in]  fmt   Log message format.
 * @param[in]  ...   Message arguments.
 *
 *******************************************************************************
 */
#define  vm_critical(fmt, ...)      g_critical_inline("%s: " fmt, FUNC, ## __VA_ARGS__)

static inline void
g_debug_inline(const gchar *fmt,
               ...)
{
   va_list args;
   va_start(args, fmt);
   g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt, args);
   va_end(args);
}

/** @copydoc vm_critical */
#define  vm_debug(fmt, ...)      g_debug_inline("%s: " fmt, FUNC, ## __VA_ARGS__)

static inline void
g_error_inline(const gchar *fmt,
               ...)
{
   va_list args;
   va_start(args, fmt);
   g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, fmt, args);
   va_end(args);
}

/** @copydoc vm_critical */
#define  vm_error(fmt, ...)      g_error_inline("%s: " fmt, FUNC, ## __VA_ARGS__)


static inline void
g_info_inline(const gchar *fmt,
               ...)
{
   va_list args;
   va_start(args, fmt);
   g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, fmt, args);
   va_end(args);
}

/** @copydoc vm_critical */
#define  vm_info(fmt, ...)      g_info_inline("%s: " fmt, FUNC, ## __VA_ARGS__)

static inline void
g_message_inline(const gchar *fmt,
               ...)
{
   va_list args;
   va_start(args, fmt);
   g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, fmt, args);
   va_end(args);
}

/** @copydoc vm_critical */
#define  vm_message(fmt, ...)      g_message_inline("%s: " fmt, FUNC, ## __VA_ARGS__)

static inline void
g_warning_inline(const gchar *fmt,
               ...)
{
   va_list args;
   va_start(args, fmt);
   g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, fmt, args);
   va_end(args);
}

/** @copydoc vm_critical */
#define  vm_warning(fmt, ...)      g_warning_inline("%s: " fmt, FUNC, ## __VA_ARGS__)

#else // ! (windows & glib >= 2.46)

/*
 *******************************************************************************
 * vm_{critical,debug,error,info,message,warning} --                      */ /**
 *
 * Wrapper around the corresponding glib function that automatically includes
 * the calling function name in the log message. The "fmt" parameter must be
 * a string constant.
 *
 * @param[in]  fmt   Log message format.
 * @param[in]  ...   Message arguments.
 *
 *******************************************************************************
 */
#define  vm_critical(fmt, ...)   g_critical("%s: " fmt, FUNC, ## __VA_ARGS__)

/** @copydoc vm_critical */
#define  vm_debug(fmt, ...)      g_debug("%s: " fmt, FUNC, ## __VA_ARGS__)

/** @copydoc vm_critical */
#define  vm_error(fmt, ...)      g_error("%s: " fmt, FUNC, ## __VA_ARGS__)

/** @copydoc vm_critical */
#define  vm_info(fmt, ...)       g_info("%s: " fmt, FUNC, ## __VA_ARGS__)

/** @copydoc vm_critical */
#define  vm_message(fmt, ...)    g_message("%s: " fmt, FUNC, ## __VA_ARGS__)

/** @copydoc vm_critical */
#define  vm_warning(fmt, ...)    g_warning("%s: " fmt, FUNC, ## __VA_ARGS__)
#endif // ! (windows & glib >= 2.46)


G_BEGIN_DECLS

void
VMTools_ConfigLogToStdio(const gchar *domain);

void
VMTools_ConfigLogging(const gchar *defaultDomain,
                      GKeyFile *cfg,
                      gboolean force,
                      gboolean reset);

void
VMTools_UseVmxGuestLog(const gchar *appName);

void
VMTools_SetupVmxGuestLog(gboolean refreshRpcChannel, GKeyFile *cfg,
                         const gchar *level);

typedef enum {
   TO_HOST,
   IN_GUEST
} LogWhere;

void
VMTools_Log(LogWhere where,
            GLogLevelFlags level,
            const gchar *domain,
            const gchar *fmt,
            ...);

G_END_DECLS

#define host_warning(fmt, ...)                                          \
   VMTools_Log(TO_HOST, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

#define guest_warning(fmt, ...)                                         \
   VMTools_Log(IN_GUEST, G_LOG_LEVEL_WARNING, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

#define host_message(fmt, ...)                                          \
   VMTools_Log(TO_HOST, G_LOG_LEVEL_MESSAGE, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

#define guest_message(fmt, ...)                                         \
   VMTools_Log(IN_GUEST, G_LOG_LEVEL_MESSAGE, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

#define host_info(fmt, ...)                                     \
   VMTools_Log(TO_HOST, G_LOG_LEVEL_INFO, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

#define guest_info(fmt, ...)                                    \
   VMTools_Log(IN_GUEST, G_LOG_LEVEL_INFO, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

#define host_debug(fmt, ...)                                            \
   VMTools_Log(TO_HOST, G_LOG_LEVEL_DEBUG, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

#define guest_debug(fmt, ...)                                           \
   VMTools_Log(IN_GUEST, G_LOG_LEVEL_DEBUG, G_LOG_DOMAIN, fmt, ## __VA_ARGS__)

/** @} */

#endif /* _VMTOOLS_LOG_H_ */
