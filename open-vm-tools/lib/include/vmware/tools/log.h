/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * @addtogroup vmtools_utils
 * @{
 */

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
 * Log a message with G_LOG_LEVEL_INFO; this function is missing in glib for
 * whatever reason.
 *
 * @param[in]  fmt   Log message format.
 * @param[in]  ...   Message arguments.
 *
 *******************************************************************************
 */

#define g_info(fmt, ...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, fmt, ## __VA_ARGS__)


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

/** @} */

#endif /* _VMTOOLS_LOG_H_ */

