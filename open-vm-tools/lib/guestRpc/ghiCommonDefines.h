/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * ghiCommonDefines.h --
 *
 *    Common definitions of maximum sizes/lengths for data used in
 *    guest/host integration XDR serialization.
 */

#ifndef _GHI_COMMON_DEFINES_H_
#define _GHI_COMMON_DEFINES_H_

#define GHI_MAX_NUM_ACTION_URI_PAIRS 16
#define GHI_HANDLERS_SUFFIX_MAX_LEN 32
#define GHI_HANDLERS_MIMETYPE_MAX_LEN 256
#define GHI_HANDLERS_UTI_MAX_LEN 256

/*
 * The Windows MAX_PATH define specifies that paths may be up to 260 character
 * units in length. To allow for expansion when going to UTF8 we multiply that
 * value by 4 here.
 */
#define GHI_HANDLERS_ACTIONURI_MAX_PATH 1040

/*
 * Maximum length for a guest app hash value (usually much shorter).
 */
#define GHI_EXEC_INFO_HASH_MAX_LEN 1024

#endif // ifndef _GHI_COMMON_DEFINES_H_
