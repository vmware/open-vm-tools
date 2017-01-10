/*********************************************************
 * Copyright (C) 2014-2016 VMware, Inc. All rights reserved.
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
 * sslDirect.h --
 *
 *	declarations in ssl.h that are required by AsyncSocket.
 *
 */

#ifndef _SSLDIRECT_H_
#define _SSLDIRECT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

typedef struct _SSLVerifyParam SSLVerifyParam;
typedef struct SSLSockStruct *SSLSock;
typedef char* (SSLLibFn)(const char*, const char*);

void SSL_Init(SSLLibFn *getLibFn, const char *defaultLib, const char *name);
SSLSock SSL_New(int fd,Bool closeFdOnShutdown);
void SSL_SetCloseOnShutdownFlag(SSLSock ssl);
Bool SSL_SetupAcceptWithContext(SSLSock sSock, void *ctx);
int SSL_TryCompleteAccept(SSLSock sSock);
ssize_t SSL_Read(SSLSock ssl, char *buf, size_t num);
ssize_t SSL_RecvDataAndFd(SSLSock ssl, char *buf, size_t num, int *fd);
ssize_t SSL_Write(SSLSock ssl, const char  *buf, size_t num);
int SSL_Shutdown(SSLSock ssl);
int SSL_GetFd(SSLSock sSock);
int SSL_Pending(SSLSock ssl);
int SSL_WantRead(const SSLSock ssl);


#ifdef _WIN32
#define SSLGeneric_read(sock,buf,num) recv(sock,buf,num,0)
#define SSLGeneric_write(sock,buf,num) send(sock,buf,num,0)
#define SSLGeneric_recvmsg(sock,msg,flags) recvmsg(sock,msg,flags)
#define SSLGeneric_close(sock) closesocket(sock)
#else
#define SSLGeneric_read(sock,buf,num) read(sock, buf, num)
#define SSLGeneric_write(sock,buf,num) write(sock, buf,num)
#define SSLGeneric_recvmsg(sock,msg,flags) recvmsg(sock,msg,flags)
#define SSLGeneric_close(sock) close(sock)
#endif

void *SSL_NewContext(void);

#endif // ifndef _SSLDIRECT_H_

