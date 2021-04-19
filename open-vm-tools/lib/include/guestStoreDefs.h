/*********************************************************
 * Copyright (C) 2019-2020 VMware, Inc. All rights reserved.
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
 *  guestStoreDefs.h  --
 *    Common definitions for VMware Tools guestStore plugin and client library.
 */

#ifndef __GUESTSTOREDEFS_H__
#define __GUESTSTOREDEFS_H__

#include "vm_basic_defs.h"

/*
 * GuestStore client connection definitions.
 */
#ifdef _WIN32
#define GUESTSTORE_LOOPBACK_PORT_MIN  7332
#define GUESTSTORE_LOOPBACK_PORT_MAX  7342
#else
#define GUESTSTORE_PIPE_DIR   "/var/run/vmware"
#define GUESTSTORE_PIPE_NAME  GUESTSTORE_PIPE_DIR "/guestStorePipe"
#endif


/*
 * HTTP definitions.
 */
#define HTTP_VER  "HTTP/1.1"

#define HTTP_LINE_END  "\r\n"

#define HTTP_HEADER_END      HTTP_LINE_END HTTP_LINE_END
#define HTTP_HEADER_END_LEN  (sizeof HTTP_HEADER_END - 1)

#define HTTP_REQ_METHOD_GET  "GET"

#define HTTP_STATUS_CODE_OK         200
#define HTTP_STATUS_CODE_FORBIDDEN  403
#define HTTP_STATUS_CODE_NOT_FOUND  404

#define HTTP_RES_OK_LINE         HTTP_VER " "  \
   XSTR(HTTP_STATUS_CODE_OK) " OK" HTTP_LINE_END
#define HTTP_RES_FORBIDDEN_LINE  HTTP_VER " "  \
   XSTR(HTTP_STATUS_CODE_FORBIDDEN) " Forbidden" HTTP_LINE_END
#define HTTP_RES_NOT_FOUND_LINE  HTTP_VER " "  \
   XSTR(HTTP_STATUS_CODE_NOT_FOUND) " Not Found" HTTP_LINE_END

#define CONTENT_LENGTH_HEADER      "Content-Length: "
#define CONTENT_LENGTH_HEADER_LEN  ((int)(sizeof(CONTENT_LENGTH_HEADER) - 1))

#define HTTP_RES_COMMON_HEADERS  "Date: %s" HTTP_LINE_END  \
   "Server: VMGuestStore" HTTP_LINE_END                    \
   "Accept-Ranges: bytes" HTTP_LINE_END                    \
   CONTENT_LENGTH_HEADER "%" FMT64 "d" HTTP_LINE_END       \
   "Content-Type: application/octet-stream" HTTP_LINE_END  \
   "Connection: close" HTTP_LINE_END                       \
   HTTP_LINE_END

#define HTTP_RES_OK         HTTP_RES_OK_LINE        HTTP_RES_COMMON_HEADERS
#define HTTP_RES_FORBIDDEN  HTTP_RES_FORBIDDEN_LINE HTTP_RES_COMMON_HEADERS
#define HTTP_RES_NOT_FOUND  HTTP_RES_NOT_FOUND_LINE HTTP_RES_COMMON_HEADERS

#endif /* __GUESTSTOREDEFS_H__ */
