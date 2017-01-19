/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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

#ifndef _I18N_H_
#define _I18N_H_

/**
 * @file i18n.h
 *
 * @defgroup vmtools_i18n Internationalization
 * @{
 *
 * @brief Functions and macros to help with internationalization of applications.
 *
 * To create a localizable string, use the MSGID macro in the following manner:
 *
 * @code
 *    VMTools_GetString("domain", MSGID(message.id) "Default English text.")
 * @endcode
 *
 * Or, in shorthand form:
 *
 * @code
 *    SU_(message.id, "Default English text.")
 * @endcode
 *
 * This will instruct the code to retrive the message under key "message.id"
 * in the translation catalog for the configured locale.
 *
 * The shorthand macros use the VMW_TEXT_DOMAIN macro to identify the domain
 * from which translated messages will be loaded. Each domain should first be
 * initialized by calling VMTools_BindTextDomain().
 */

#include <glib.h>

/*
 * Copied from msgid.h to avoid exposing VMware internal headers. Don't
 * change these values. Ever.
 */
#define MSG_MAGIC       "@&!*@*@"
#define MSG_MAGIC_LEN   7
#define MSGID(id)       MSG_MAGIC "(" #id ")"

/**
 * Shorthand macro to retrieve a localized message in UTF-8.
 *
 * @param[in]  msgid    The message ID.
 * @param[in]  en       English version of the message.
 *
 * @return A localized message.
 */
#define SU_(msgid, en) VMTools_GetString(VMW_TEXT_DOMAIN, MSGID(msgid) en)

#if defined(_WIN32)
/**
 * Shorthand macro to retrieve a localized message in UTF-16LE. Win32 only.
 *
 * @param[in]  msgid    The message ID.
 * @param[in]  en       English version of the message.
 *
 * @return A localized message.
 */
#  define SW_(msgid, en) VMTools_GetUtf16String(VMW_TEXT_DOMAIN, MSGID(msgid) en)
#endif

G_BEGIN_DECLS

void
VMTools_BindTextDomain(const char *domain,
                       const char *locale,
                       const char *catdir);

const char *
VMTools_GetString(const char *domain,
                  const char *msgid);

#if defined(_WIN32)
const wchar_t *
VMTools_GetUtf16String(const char *domain,
                       const char *msgid);
#endif

G_END_DECLS

/** @} */

#endif /* _I18N_H_ */

