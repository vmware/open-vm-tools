/*********************************************************
 * Copyright (C) 2013 VMware, Inc. All rights reserved.
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
 * sslStubs.h --
 *
 *	declarations in ssl.h that required by AsyncSocket when SSL is not used.
 *
 */

#ifndef _SSLSTUBS_H_
#define _SSLSTUBS_H_

typedef struct SSLSockStruct *SSLSock;
typedef struct _SSLVerifyParam { } SSLVerifyParam;

SSLSock SSL_New(int fd,Bool closeFdOnShutdown);
void SSL_SetCloseOnShutdownFlag(SSLSock sslSock);
ssize_t SSL_Read(SSLSock sslSock, char *buf, size_t num);
ssize_t SSL_RecvDataAndFd(SSLSock sslSock, char *buf, size_t num, int *fd);
ssize_t SSL_Write(SSLSock sslSock,const char  *buf, size_t num);
int SSL_Shutdown(SSLSock sslSock);
int SSL_GetFd(SSLSock sslSock);
int SSL_Pending(SSLSock sslSock);
int SSL_GetFd(SSLSock sslSock);
Bool SSL_ConnectAndVerify(SSLSock sSock, SSLVerifyParam *verifyParam);
Bool SSL_Accept(SSLSock sSock);
int SSL_TryCompleteAccept(SSLSock ssl);
int SSL_WantRead(const SSLSock ssl);
Bool SSL_SetupAcceptWithContext(SSLSock sSock, void *ctx);


#ifdef _WIN32
#define SSLGeneric_close(sock) closesocket(sock)
#else
#define SSLGeneric_close(sock) close(sock)
#endif


#endif // ifndef _SSLSTUBS_H_

