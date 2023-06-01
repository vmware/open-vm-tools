/*********************************************************
 * Copyright (c) 2011-2017, 2019-2022 VMware, Inc. All rights reserved.
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

/**
 * @file netPosix.c --
 *
 *    Networking interfaces for posix systems.
 */


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
#include "serviceInt.h"
#include "VGAuthProto.h"


/*
 ******************************************************************************
 * ServiceNetworkCreateSocketDir --                                      */ /**
 *
 * Creates the directory for the UNIX domain sockets and pid files.
 *
 * @return TRUE on success, FALSE on failure.
 *
 ******************************************************************************
 */

gboolean
ServiceNetworkCreateSocketDir(void)
{
   gboolean bRet = TRUE;
   char *socketDir = g_path_get_dirname(SERVICE_PUBLIC_PIPE_NAME);

   ASSERT(socketDir != NULL);

   /*
    * Punt if its there but not a directory.
    *
    * g_file_test() on a symlink will return true for IS_DIR
    * if it's a link pointing to a directory, since glib
    * uses stat() instead of lstat().
    */
   if (g_file_test(socketDir, G_FILE_TEST_EXISTS) &&
       (!g_file_test(socketDir, G_FILE_TEST_IS_DIR) ||
       g_file_test(socketDir, G_FILE_TEST_IS_SYMLINK))) {
      bRet = FALSE;
      Warning("%s: socket dir path '%s' already exists as a non-directory; "
              "canceling\n", __FUNCTION__, socketDir);
      goto quit;
   }

   /*
    * XXX May want to add some security checks here.
    */
   if (!g_file_test(socketDir, G_FILE_TEST_EXISTS)) {
      int ret;

      ret = ServiceFileMakeDirTree(socketDir, 0755);
      if (ret < 0) {
         bRet = FALSE;
         Warning("%s: failed to create socket dir '%s' error: %d\n",
                 __FUNCTION__, socketDir, ret);
         goto quit;
      }
      Log("%s: Created socket directory '%s'\n", __FUNCTION__, socketDir);
   }
quit:
   g_free(socketDir);

   return bRet;
}


/*
 ******************************************************************************
 * ServiceNetworkListen --                                               */ /**
 *
 * Creates the UNIX domain socket and starts listening on it.
 *
 * @param[in]   conn          The ServiceConnection describing the connection.
 * @param[in]   makeSecure    If set, the new pipe is restricted in access to
 *                            the userName in the ServiceConnection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceNetworkListen(ServiceConnection *conn,            // IN/OUT
                     gboolean makeSecure)
{
   int sock = -1;
   struct sockaddr_un sockaddr;
   int ret;
   VGAuthError err = VGAUTH_E_OK;
   mode_t mode;
   struct stat stbuf;
   uid_t uid;
   gid_t gid;

   /*
    * For some reason, this is simply hardcoded in sys/un.h
    */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX   108
#endif

   ASSERT(strlen(conn->pipeName) < UNIX_PATH_MAX);

   conn->sock = -1;

   /*
    * Make sure the socket dir exists.  In theory this is only ever done once,
    * but something could clobber it.
    */
   if (!ServiceNetworkCreateSocketDir()) {
      err = VGAUTH_E_COMM;
      goto quit;
   }

   sock = socket(PF_UNIX, SOCK_STREAM, 0);
   if (sock < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: socket() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   sockaddr.sun_family = PF_UNIX;

   ret = g_unlink(conn->pipeName);
   if (ret < 0 && errno != ENOENT) {
      Warning("%s: unlink(%s) failed, %d - continuing\n", __FUNCTION__,
              conn->pipeName, errno);
   }

   /* Ignore return, returns the length of the source string */
   /* coverity[check_return] */
   g_strlcpy(sockaddr.sun_path, conn->pipeName, UNIX_PATH_MAX);

   ret = bind(sock, (struct sockaddr *) &sockaddr, sizeof sockaddr);
   if (ret < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: bind(%s) failed, %d\n", __FUNCTION__, conn->pipeName, errno);
      goto quit;
   }

   /*
    * Adjust security as needed.
    */
   if (stat(conn->pipeName, &stbuf) < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: stat(%s) failed, %d\n", __FUNCTION__, conn->pipeName, errno);
      goto quit;
   }

   mode = stbuf.st_mode;
   mode &= ~(S_IRWXU | S_IRWXG | S_IRWXO);
   if (makeSecure) {
      mode = (S_IRUSR | S_IWUSR);
   } else {
      mode = (S_IRUSR | S_IWUSR |
               S_IRGRP | S_IWGRP |
               S_IROTH | S_IWOTH);
   }

   if (chmod(conn->pipeName, mode) < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: chmod(%s) failed, %d\n", __FUNCTION__, conn->pipeName, errno);
      goto quit;
   }

   if (makeSecure) {
      err = UsercheckLookupUser(conn->userName, &uid, &gid);
      if (err != VGAUTH_E_OK) {
         err = VGAUTH_E_NO_SUCH_USER;
         Warning("%s: failed to get uid/gid for user '%s'\n",
                 __FUNCTION__, conn->userName);
         goto quit;
      }
      if (chown(conn->pipeName, uid, gid) < 0) {
         err = VGAUTH_E_COMM;
         Warning("%s: chown(%s) failed, %d\n", __FUNCTION__, conn->pipeName, errno);
         goto quit;
      }
   }

   if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: fcntl() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   if (listen(sock, 32) < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: listen() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   conn->sock = sock;

   return VGAUTH_E_OK;

quit:
   if (sock >= 0) {
      close(sock);
   }
   return err;
}


/*
 ******************************************************************************
 * ServiceNetworkAcceptConnection --                                     */ /**
 *
 * Accepts a connection on a socket.
 *
 * @param[in]   connIn            The connection that owns the socket being
 *                                accept()ed.
 * @param[out]  connOut           The new connection.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceNetworkAcceptConnection(ServiceConnection *connIn,
                               ServiceConnection *connOut)
{
   int newfd;
   struct sockaddr_un sockaddr;
   int addrlen = sizeof(struct sockaddr_un);
   VGAuthError err = VGAUTH_E_OK;

   memset(&sockaddr, 0, addrlen);
   newfd = accept(connIn->sock, (struct sockaddr *) &sockaddr, &addrlen);
   if (newfd < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: accept() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   Debug("%s: got new connection on '%s', sock %d\n",
         __FUNCTION__, connIn->pipeName, newfd);

   connOut->sock = newfd;

   return VGAUTH_E_OK;
quit:
   return err;
}


/*
 ******************************************************************************
 * ServiceNetworkCloseConnection --                                      */ /**
 *
 * Closes the network connection.
 *
 * @param[in]   conn    The connection to be closed.
 *
 ******************************************************************************
 */

void
ServiceNetworkCloseConnection(ServiceConnection *conn)
{
   if (conn->sock != -1) {
      close(conn->sock);
   }
   conn->sock = -1;
}


/*
 ******************************************************************************
 * ServiceNetworkRemoveListenPipe --                                     */ /**
 *
 * Closes the listening connection's pipe.
 *
 * @param[in]   conn    The listen connection owning the pipe to be removed.
 *
 ******************************************************************************
 */

void
ServiceNetworkRemoveListenPipe(ServiceConnection *conn)
{
   /* coverity[check_return] */
   ServiceFileUnlinkFile(conn->pipeName);
}


/*
 ******************************************************************************
 * ServiceNetworkReadData --                                             */ /**
 *
 * Reads some data off the wire.
 *
 * @param[in]   conn    The connection.
 * @param[out]  len     How much data was read.
 * @param[out]  data    The data.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceNetworkReadData(ServiceConnection *conn,
                       gsize *len,                                     // OUT
                       gchar **data)                                   // OUT
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
   *data = NULL;

   do {
      ret = recv(conn->sock, buf, sizeof(buf), 0);
      if (ret == 0) {
         Debug("%s: EOF on socket\n", __FUNCTION__);
         conn->eof = TRUE;
         return err;
      }
   } while (ret == -1 && errno == EINTR);

   if (ret < 0) {
      Warning("%s: error %d reading from socket\n", __FUNCTION__, errno);
      return VGAUTH_E_COMM;
   }

   *data = g_strndup(buf, ret);
   *len = ret;

   return err;
}


/*
 ******************************************************************************
 * ServiceNetworkWriteData --                                            */ /**
 *
 * Writes some data on the wire.
 *
 * @param[in]   conn    The connection.
 * @param[in]   len     How much data to write.
 * @param[in]   data    The data.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceNetworkWriteData(ServiceConnection *conn,
                        gsize len,
                        gchar *data)
{
   VGAuthError err = VGAUTH_E_OK;
   gsize sent = 0;
   int ret;

   if (len == 0) {
      Debug("%s: asked to send %d bytes; bad caller?\n", __FUNCTION__, (int) len);
      return err;
   }

   /*
    * XXX Potential DOS issue here.  This could wedge if the socket
    * gets full and is never drained (client asks for QueryMappedCerts,
    * reply is huge, client suspends before we start sending it,
    * buffer fills, service loops.)
    *
    * A couple potential fixes:
    * - instead of writing, toss it on a queue and watch the writable
    *   flag on the sock and dump when we can.
    * - have the current code just give up and declare the client dead
    *   if we get EAGAIN too many times in a row.
    *
    * Second fix is simpler, but might bite us in a dev situation if
    * we sit in a breakpoint too long.
    */

   do {
retry:
#if NETWORK_FORCE_TINY_PACKETS
      ret = send(conn->sock, data + sent, 1, 0);
#else
      ret = send(conn->sock, data + sent, len - sent, 0);
#endif
      if (ret < 0) {
         if (EINTR == errno) {
            goto retry;
         }
         Warning("%s: send() failed, errno %d\n", __FUNCTION__, errno);
         return VGAUTH_E_COMM;
      }
      sent += ret;
   } while (sent < len);

   return err;
}


/*****************************************************************************/

#ifdef SUPPORT_TCP

/*
 * XXX this is untested, and may not even compile, but is
 * left as a starting point for an external client.
 */

VGAuthError
ServiceNetworkAcceptTCPConnection(ServiceConnection *connIn,
                                  ServiceConnection *connOut)
{
   int newfd;
   struct sockaddr_in sockaddr;
   int addrlen = sizeof(sockaddr);
   char *ipaddr = NULL;
   VGAuthError err = VGAUTH_E_OK;

   memset(&sockaddr, 0, sizeof(sockaddr));
   newfd = accept(connIn->sock, (struct sockaddr *) &sockaddr, &addrlen);
   if (newfd < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: accept() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   ipaddr = inet_ntoa(sockaddr.sin_addr);
   Debug("%s: got new connection from %s\n", __FUNCTION__, ipaddr);

   connOut->sock = newfd;

   return VGAUTH_E_OK;
quit:
   return err;
}


VGAuthError
ServiceNetworkTCPListen(ServiceConnection *conn)         // IN/OUT
{
   int sock;
   struct sockaddr_in sockaddr;
   int ret;
   VGAuthError err = VGAUTH_E_OK;
   int fd;
   struct protoent protobuf;
   char *buf;
   struct protoent *proto_ent;

   buf = g_malloc(2048);
   if (getprotobyname_r("TCP", &protobuf, buf, 2048, &proto_ent)) {
      err = VGAUTH_E_COMM;
      Warning("%s: getprotobyname_r() failed, %d\n", __FUNCTION__, errno);
      g_free(buf);
      goto quit;
   }


   sock = socket(AF_INET, SOCK_STREAM, proto_ent->p_proto);
   if (sock < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: socket() failed, %d\n", __FUNCTION__, errno);
      g_free(buf);
      goto quit;
   }

   g_free(buf);

   memset(&sockaddr, 0, sizeof(sockaddr));
   sockaddr.sin_family = AF_INET;
   sockaddr.sin_port = htons(conn->port);
   sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

#ifdef SO_REUSEADDR
   {
      int one = 1;
      setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int));
   }
#endif

   ret = bind(sock, (struct sockaddr *) &sockaddr, sizeof sockaddr);

   if (ret < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: bind() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: fcntl() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   if (listen(sock, 32) < 0) {
      err = VGAUTH_E_COMM;
      Warning("%s: listen() failed, %d\n", __FUNCTION__, errno);
      goto quit;
   }

   conn->sock = sock;

   return VGAUTH_E_OK;

quit:
   close(sock);
   return err;
}

#endif   // SUPPORT_TCP
