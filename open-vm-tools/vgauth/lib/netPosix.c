/*********************************************************
 * Copyright (C) 2011-2016,2019,2022 VMware, Inc. All rights reserved.
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
 * @file netPosix.c
 *
 * Client posix networking
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/unistd.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include "VGAuthInt.h"
#include "VGAuthLog.h"


/*
 ******************************************************************************
 * VGAuth_NetworkConnect --                                              */ /**
 *
 * Creates connection to pipe specified in ctx.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_NetworkConnect(VGAuthContext *ctx)
{
   VGAuthError err = VGAUTH_E_OK;
   int fd;
   int ret;
   struct sockaddr_un sockaddr;
   /*
    * For some reason, this is simply hardcoded in sys/un.h
    */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX   108
#endif

   sockaddr.sun_family = AF_UNIX;

   fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0) {
      VGAUTH_LOG_ERR_POSIX("socket() failed for %s", ctx->comm.pipeName);
      return VGAUTH_E_COMM;
   }

   /* Ignore return, returns the length of the source string */
   /* coverity[check_return] */
   g_strlcpy(sockaddr.sun_path, ctx->comm.pipeName, UNIX_PATH_MAX);

   do {
      ret = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
   } while (ret == -1 && errno == EINTR);

   if (ret < 0) {
      int saveErrno = errno;

      VGAUTH_LOG_ERR_POSIX("connect() failed for %s", ctx->comm.pipeName);
      close(fd);
      /*
       * Assume that ENOENT means the service isn't running (or
       * its pipe has been deleted), and ECONNREFUSED means the service
       * is down, so we can give a more end-user helpful error code.
       */
      if ((ECONNREFUSED == saveErrno) || (ENOENT == saveErrno)) {
         return VGAUTH_E_SERVICE_NOT_RUNNING;
         /*
          * Pass up a permission failure.
          */
      } else if (EACCES == saveErrno) {
         return VGAUTH_E_PERMISSION_DENIED;
         /*
          * Treat anything else as a generic comm error.
          */
      } else {
         return VGAUTH_E_COMM;
      }
   }

   ctx->comm.sock = fd;
   ctx->comm.connected = TRUE;

   return err;
}


/*
 ******************************************************************************
 * VGAuth_NetworkValidatePublicPipeOwner --                              */ /**
 *
 * Security check -- validates that the pipe is owned by the super user,
 * to try to catch spoofing.
 *
 * @param[in]  ctx        The VGAuthContext.
 *
 * @return TRUE if the pipe is owned by ther proper user.
 *         FALSE otherwise or if failed.
 *
 ******************************************************************************
 */

gboolean
VGAuth_NetworkValidatePublicPipeOwner(VGAuthContext *ctx)
{
   gboolean retval = FALSE;
   int ret;

#ifdef __linux__
   struct ucred peerCred;
   socklen_t peerCredLen = sizeof peerCred;

   /*
    * On Linux, the SO_PEERCRED socket option will give us the PID,
    * effective UID, and GID of the peer (the server in this case).
    */

   ret = getsockopt(ctx->comm.sock, SOL_SOCKET, SO_PEERCRED, &peerCred,
                    &peerCredLen);
   if (ret < 0) {
      VGAUTH_LOG_ERR_POSIX("getsockopt() failed on %s", ctx->comm.pipeName);
      goto done;
   }

   retval = (peerCred.uid == 0);

done:

   return retval;

#else

   struct stat stbuf;

   /*
    * XXX: fstat() on a UNIX domain socket does not return the UID of the
    * file's owner, but the UID of the client process (i.e., us). Also,
    * SO_PEERCRED is only available on Linux. So, we are left with using
    * stat() the pipe's filename. This introduces TOCTOU issues, but at least
    * it gives us a cursory check against someone else spoofing the service.
    */
   ret = g_stat(ctx->comm.pipeName, &stbuf);
   if (ret < 0) {
      VGAUTH_LOG_ERR_POSIX("g_stat() failed on %s", ctx->comm.pipeName);
      goto done;
   }

   retval = (stbuf.st_uid == 0);

done:

   return retval;
#endif
}


/*
 ******************************************************************************
 * VGAuth_NetworkReadBytes --                                            */ /**
 *
 * Reads the available data on the connection.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[out] len        The length of the data.  0 if the connection is lost.
 * @param[out] buffer     The data.  Should be g_free()d.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_NetworkReadBytes(VGAuthContext *ctx,
                        gsize *len,
                        gchar **buffer)
{
   VGAuthError err = VGAUTH_E_OK;
   int ret;
#if NETWORK_FORCE_TINY_PACKETS
#define  READ_BUFSIZE   1
#else
#define  READ_BUFSIZE   10240
#endif
   char buf[READ_BUFSIZE];

   *len = 0;
   *buffer = NULL;

   do {
      ret = recv(ctx->comm.sock, buf, sizeof(buf), 0);
      if (ret == 0) {
         Warning("%s: EOF on socket\n", __FUNCTION__);
         return err;
      }
   } while (ret == -1 && errno == EINTR);

   if (ret < 0) {
      VGAUTH_LOG_ERR_POSIX("error reading from %s", ctx->comm.pipeName);
      return VGAUTH_E_COMM;
   }

   *buffer = g_strndup(buf, ret);
   *len = ret;

   return err;
}


/*
 ******************************************************************************
 * VGAuthIgnoreSigPipe --                                                */ /**
 *
 * Ignore the SIGPIPE
 *
 ******************************************************************************
 */

static void
VGAuthIgnoreSigPipe(void)
{
   static gboolean alreadySetup = FALSE;
   if (alreadySetup) {
      return;
   }

   signal(SIGPIPE, SIG_IGN);

   alreadySetup = TRUE;
}


/*
 ******************************************************************************
 * VGAuth_NetworkWriteBytes --                                           */ /**
 *
 * Writes bytes to the connection in the ctx.
 *
 * @param[in]  ctx        The VGAuthContext.
 * @param[in]  len        The length of the data.
 * @param[in]  buffer     The data.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
VGAuth_NetworkWriteBytes(VGAuthContext *ctx,
                         gsize len,
                         gchar *buffer)
{
   VGAuthError err = VGAUTH_E_OK;
   gsize sent = 0;
   int ret;

   if (len == 0) {
      Warning("%s: asked to send %d bytes; bad caller?\n",
              __FUNCTION__, (int) len);
      return err;
   }

   VGAuthIgnoreSigPipe();

   do {
retry:
#if NETWORK_FORCE_TINY_PACKETS
      ret = send(ctx->comm.sock, buffer + sent, 1, 0);
#else
      ret = send(ctx->comm.sock, buffer + sent, len - sent, 0);
#endif
      if (ret < 0) {
         if (EINTR == errno) {
            goto retry;
         }
         VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, errno);
         VGAUTH_LOG_ERR_POSIX("send() failed on %s", ctx->comm.pipeName);
         return err;
      }
      sent += ret;
   } while (sent < len);

   return err;
}
