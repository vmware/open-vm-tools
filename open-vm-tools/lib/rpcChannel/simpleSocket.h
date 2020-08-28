/*********************************************************
 * Copyright (C) 2013-2016,2020 VMware, Inc. All rights reserved.
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

#ifndef _SIMPLESOCKET_H_
#define _SIMPLESOCKET_H_

/**
 * @file simpleSocket.h
 *
 *    header of simple socket wrappers.
 */

#include <glib.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <winerror.h>
#else
#include <errno.h>
#endif

#include "vmci_defs.h"
#include "vmware/guestrpc/tclodefs.h"

/* Describe which socket API call failed */
typedef enum {
   SOCKERR_SUCCESS,
   SOCKERR_VMCI_FAMILY,
   SOCKERR_STARTUP,
   SOCKERR_SOCKET,
   SOCKERR_CONNECT,
   SOCKERR_BIND
} ApiError;

#if defined(_WIN32)

#define SYSERR_EADDRINUSE        WSAEADDRINUSE
#define SYSERR_EACCESS           WSAEACCES
#define SYSERR_EINTR             WSAEINTR
#define SYSERR_ECONNRESET        WSAECONNRESET
#define SYSERR_ENOBUFS           WSAENOBUFS

typedef int socklen_t;

#else  /* !_WIN32 */

#define SYSERR_EADDRINUSE        EADDRINUSE
#define SYSERR_EACCESS           EACCES
#define SYSERR_EINTR             EINTR
#define SYSERR_ECONNRESET        ECONNRESET
#define SYSERR_ENOBUFS           ENOBUFS

typedef int SOCKET;
#define SOCKET_ERROR              (-1)
#define INVALID_SOCKET            ((SOCKET) -1)

#endif

#define PRIVILEGED_PORT_MAX    1023
#define PRIVILEGED_PORT_MIN    1

void Socket_Close(SOCKET sock);
SOCKET Socket_ConnectVMCI(unsigned int cid,
                          unsigned int port,
                          gboolean isPriv,
                          ApiError *outApiErr,
                          int *outSysErr);
gboolean Socket_Recv(SOCKET fd,
                     char *buf,
                     int len);
gboolean Socket_Send(SOCKET fd,
                     char *buf,
                     int len);
gboolean Socket_RecvPacket(SOCKET sock,
                           char **payload,
                           int *payloadLen);
gboolean Socket_SendPacket(SOCKET sock,
                           const char *payload,
                           int payloadLen,
                           Bool fastClose);

#endif /* _SIMPLESOCKET_H_ */
