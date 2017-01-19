/*********************************************************
 * Copyright (C) 2013-2016 VMware, Inc. All rights reserved.
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
 * sslStubs.c --
 *
 *      Stubs for AsyncSokcet SSL functions without actually using SSL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#include "str.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "log.h"
#include "err.h"
#include "msg.h"
#include "sslDirect.h"
#include "vm_assert.h"

#define LOGLEVEL_MODULE SSLStubs
#include "loglevel_user.h"


struct SSLSockStruct {
   int fd;
   Bool closeFdOnShutdown;
#ifdef __APPLE__
   Bool loggedKernelReadBug;
#endif
};


/*
 *----------------------------------------------------------------------
 *
 * SSL_Write()
 *
 *    Functional equivalent of the write() syscall.
 *
 * Results:
 *    Returns the number of bytes written, or -1 on error.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

ssize_t
SSL_Write(SSLSock sslSock,   // IN
          const char *buf,   // IN
          size_t num)        // IN
{
   return send(sslSock->fd, buf, num, 0);
}


/*
 *----------------------------------------------------------------------
 *
 * SSL_Read --
 *
 *    Functional equivalent of the read() syscall.
 *
 * Results:
 *    Returns the number of bytes read, or -1 on error.  The
 *    data read will be placed in buf.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------
 */

ssize_t
SSL_Read(SSLSock sslSock,   // IN
         char *buf,         // OUT
         size_t num)        // IN
{
   int ret;
   ASSERT(sslSock);

#ifdef _WIN32
   ret = recv(sslSock->fd, buf, num, 0);
#else
   ret = read(sslSock->fd, buf, num);
#endif

#ifdef __APPLE__
   /*
    * Detect bug 161237 (Apple bug 5202831), which should no longer be
    * happening due to a workaround in our code.
    *
    * There is a bug on Mac OS 10.4 and 10.5 where passing an fd
    * over a socket can result in that fd being in an inconsistent state.
    * We can detect when this happens when read(2) returns zero
    * even if the other end of the socket is not disconnected.
    * We verify this by calling write(sslSock->fd, "", 0) and
    * see if it is okay. (If the socket was really closed, it would
    * return -1 with errno==EPIPE.)
    */
   if (ret == 0) {
      ssize_t writeRet;
#ifdef VMX86_DEBUG
      struct stat statBuffer;

      /*
       * Make sure we're using a socket.
       */
      ASSERT((fstat(sslSock->fd, &statBuffer) == 0) &&
             ((statBuffer.st_mode & S_IFSOCK) == S_IFSOCK));

#endif
      writeRet = write(sslSock->fd, "", 0);
      if (writeRet == 0) {
         /*
          * The socket is still good. read(2) should not have returned zero.
          */
         if (! sslSock->loggedKernelReadBug) {
            Log("Error: Encountered Apple bug #5202831.  Disconnecting.\n");
            sslSock->loggedKernelReadBug = TRUE;
         }
      }
   }
#endif

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * SSL_GetFd()
 *
 *    Returns an socket's file descriptor or handle.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int
SSL_GetFd(SSLSock sslSock) // IN
{
   ASSERT(sslSock);

   return sslSock->fd;
}


/*
 *----------------------------------------------------------------------
 *
 * SSL_Pending()
 *
 *	  Always returns 0 for non-SSL socket.
 *
 * Results:
 *	  0
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int
SSL_Pending(SSLSock sslSock) // IN
{
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * SSL_New()
 *
 * Results:
 *    Returns a freshly allocated SSLSock structure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

SSLSock
SSL_New(int fd,                       // IN
        Bool closeFdOnShutdown)       // IN
{
   SSLSock sslSock;

   sslSock = calloc(1, sizeof *sslSock);
   ASSERT_MEM_ALLOC(sslSock);
   sslSock->fd = fd;
   sslSock->closeFdOnShutdown = closeFdOnShutdown;

   return sslSock;
}


/*
 *----------------------------------------------------------------------
 *
 * SSL_Shutdown()
 *
 *    Functional equivalent of the close() syscall.  Does
 *    not close the actual fd used for the connection.
 *
 *
 * Results:
 *    0 on success, -1 on failure.
 *
 * Side effects:
 *    closes the connection, freeing up the memory associated
 *    with the passed in socket object
 *
 *----------------------------------------------------------------------
 */

int
SSL_Shutdown(SSLSock sslSock)     // IN
{
   int ret = 0;
   ASSERT(sslSock);

   LOG(10, ("Starting shutdown for %d\n", sslSock->fd));
   if (sslSock->closeFdOnShutdown) {
      LOG(10, ("Trying to close %d\n", sslSock->fd));

#ifdef _WIN32
      ret = closesocket(sslSock->fd);
#else
      ret = close(sslSock->fd);
#endif

   }

   free(sslSock);
   LOG(10, ("shutdown done\n"));

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * SSL_SetCloseOnShutdownFlag()
 *
 *    Sets closeFdOnShutdown flag.
 *
 * Results:
 *    None.  Always succeeds.  Do not call close/closesocket on
 *    the fd after this, call SSL_Shutdown() instead.
 *
 * Side effects:
 *
 *----------------------------------------------------------------------
 */

void
SSL_SetCloseOnShutdownFlag(SSLSock sslSock)    // IN
{
   ASSERT(sslSock);
   sslSock->closeFdOnShutdown = TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * SSL_RecvDataAndFd --
 *
 *    recvmsg wrapper which can receive only file descriptors, not other
 *    control data.
 *
 * Results:
 *    Returns the number of bytes received, or -1 on error.  The
 *    data read will be placed in buf.  *fd is either -1 if no fd was
 *    received, or descriptor...
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------
 */

ssize_t
SSL_RecvDataAndFd(SSLSock sslSock,   // IN/OUT
                  char *buf,         // OUT
                  size_t num,        // IN
                  int *fd)           // OUT
{
   int ret;
   ASSERT(sslSock);
   ASSERT(fd);

   *fd = -1;

   /*
    * No fd passing over socket or Windows. Windows needs different code.
    */
#ifdef _WIN32
   return SSL_Read(sslSock, buf, num);
#else
   {
      struct iovec iov;
      struct msghdr msg = { 0 };
      uint8 cmsgBuf[CMSG_SPACE(sizeof(int))];

      iov.iov_base = buf;
      iov.iov_len = num;
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;
      msg.msg_control = cmsgBuf;
      msg.msg_controllen = sizeof cmsgBuf;
      ret = recvmsg(sslSock->fd, &msg, 0);
      if (ret >= 0 && msg.msg_controllen != 0) {
         struct cmsghdr *cmsg;

         for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
               int receivedFd = *(int *)CMSG_DATA(cmsg);

               ASSERT(*fd == -1);
               *fd = receivedFd;
            }
         }
      }
   }
#endif

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SSL_TryCompleteAccept --
 *
 *    Stub only, should not be called when SSL is not used.
 * Results:
 *    0
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
SSL_TryCompleteAccept(SSLSock ssl) // IN
{
   ASSERT(0);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SSL_WantRead --
 *
 *    Stub only, should not be called when SSL is not used.
 * Results:
 *    0
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
SSL_WantRead(const SSLSock ssl)
{
   ASSERT(0);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SSL_SetupAcceptWithContext --
 *
 *    Stub only, should not be called when SSL is not used.
 * Results:
 *    FALSE
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
SSL_SetupAcceptWithContext(SSLSock sSock, // IN: SSL socket
                           void *ctx)     // IN: OpenSSL context (SSL_CTX *)
{
   ASSERT(0);
   return FALSE;
}
