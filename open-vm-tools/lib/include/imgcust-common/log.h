/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * log.h --
 *
 *      Logging method that need to be provided by the client of the
 *      library to enable logging.
 */

#ifndef IMGCUST_COMMON_LOG_H
#define IMGCUST_COMMON_LOG_H

#define GUESTCUST_LOG_DIRNAME "vmware-imc"

#ifdef __cplusplus
namespace ImgCustCommon
{
#endif

enum LogLevel {log_debug, log_info, log_warning, log_error};

typedef void (*LogFunction) (int level, const char *fmtstr, ...);

#ifdef __cplusplus
} // namespace ImgCustCommon
#endif

#endif // IMGCUST_COMMON_LOG_H
