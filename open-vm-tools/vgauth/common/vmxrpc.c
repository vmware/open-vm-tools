/*********************************************************
 * Copyright (C) 2022 VMware, Inc. All rights reserved.
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
 * @file vmxrpc.c
 *
 * Simple guest->VMX RPC support.
 *
 * Doesn't share any Tools code or headers; key bits that can't change
 * are copied from vmci_sockets.h
 *
 */

#include <glib.h>
#include "vmxrpc.h"

#include <sys/types.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

/*
 * VMX listening address
 */
const int VMX_CID = 0;
const int RPCI_PORT = 976;
#define VMADDR_PORT_ANY ((unsigned int) -1)

#define PRIVILEGED_PORT_MAX 1023
#define PRIVILEGED_PORT_MIN 1

static int gAddressFamily = -1;

/*
 * Some typedefs for portability.
 */
#ifdef _WIN32
/* still have to care about pre-C++11 so use the old instead of stdint.h */
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;

#endif
#ifdef __linux__
typedef __uint32_t uint32;
typedef __uint64_t uint64;

#define SOCKET int
#endif
#ifdef sun
typedef uint32_t uint32;
typedef uint64_t uint64;

#define SOCKET int
#endif

#if defined(_WIN32)
typedef unsigned short sa_family_t;
#endif // _WIN32

#ifdef _WIN32
#define SYSERR_ECONNRESET        WSAECONNRESET
#define SYSERR_EADDRINUSE        WSAEADDRINUSE
#else
#define SYSERR_ECONNRESET        ECONNRESET
#define SYSERR_EADDRINUSE        EADDRINUSE
#endif


/*
 * Wrapper for socket errnos
 */
static int
GetSocketErrCode(void)
{
#ifdef _WIN32
   return WSAGetLastError();
#else
   return errno;
#endif
}


/*
 * Wrapper for socket close
 */
static void
Socket_Close(SOCKET fd)
{
#ifdef _WIN32
   closesocket(fd);
#else
   close(fd);
#endif
}


/*
 * start code cut&paste from vmci_sockets.h
 *
 * This is the subset from vmci_sockets.h required for our purposes.
 * this results in a few refs to other parts of the file that were
 * left out.
 */

#ifdef _WIN32
#     include <winioctl.h>
#     define VMCI_SOCKETS_DEVICE          L"\\\\.\\VMCI"
#     define VMCI_SOCKETS_VERSION         0x81032058
#     define VMCI_SOCKETS_GET_AF_VALUE    0x81032068
#     define VMCI_SOCKETS_GET_LOCAL_CID   0x8103206c
#     define VMCI_SOCKETS_UUID_2_CID      0x810320a4

static unsigned int
__VMCISock_DeviceIoControl(DWORD cmd)
{
   unsigned int val = (unsigned int)-1;
   HANDLE device = CreateFileW(VMCI_SOCKETS_DEVICE, GENERIC_READ, 0, NULL,
                               OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
   if (INVALID_HANDLE_VALUE != device) {
      DWORD ioReturn;
      DeviceIoControl(device, cmd, &val, sizeof val, &val, sizeof val,
                      &ioReturn, NULL);
      CloseHandle(device);
      device = INVALID_HANDLE_VALUE;
   }
   return val;
}
#endif   //_WIN32


/**
 * \brief Address structure for vSockets.
 *
 * The address family should be set to whatever VMCISock_GetAFValueFd()
 * returns.  The structure members should all align on their natural
 * boundaries without resorting to compiler packing directives.  The total
 * size of this structure should be exactly the same as that of \c struct
 * \c sockaddr.
 *
 * \see VMCISock_GetAFValueFd()
 */

struct sockaddr_vm {
#if defined(__APPLE__) || defined(__FreeBSD__)
   unsigned char svm_len;
#endif // __APPLE__ || __FreeBSD__

   /** \brief Address family. \see VMCISock_GetAFValueFd() */
   sa_family_t svm_family;

   /** \cond PRIVATE */
   unsigned short svm_reserved1;
   /** \endcond */

   /** \brief Port.  \see VMADDR_PORT_ANY */
   unsigned int svm_port;

   /** \brief Context ID.  \see VMADDR_CID_ANY */
   unsigned int svm_cid;

   /** \cond PRIVATE */
   unsigned char svm_zero[sizeof(struct sockaddr) -
#if defined(__APPLE__)
                             sizeof(unsigned char) -
#endif // __APPLE__
                             sizeof(sa_family_t) -
                             sizeof(unsigned short) -
                             sizeof(unsigned int) -
                             sizeof(unsigned int)];
   /** \endcond */
};
/* end code copied from vmci_sockets.h */


/*
 * Local version of htonll() which is missing in many environments.
 * Assumes the host is little-endian.
 */
static uint64
_vmxlog_htonll(uint64 s)
{
   uint64 out;
   unsigned char *buf = (unsigned char *) &out;

   buf[0] = s >> 56 & 0xff;
   buf[1] = s >> 48 & 0xff;
   buf[2] = s >> 40 & 0xff;
   buf[3] = s >> 32 & 0xff;
   buf[4] = s >> 24 & 0xff;
   buf[5] = s >> 16 & 0xff;
   buf[6] = s >>  8 & 0xff;
   buf[7] = s >>  0 & 0xff;

   return out;
}


/*
 ******************************************************************************
 * VMXRPC_MakePacket --                                                  */ /**
 *
 * Takes 'cmd' and builds an RPC packet out of it, putting in the
 * length and header info (properly byte swapped).
 *
 * See bora-vmsoft/lib/rpcChannel/simpleSocket.c:Socket_PackSendData()
 *
 * retPacket contains the new packet, which must be g_free()d.
 * Returns the size of the new packet.
 *
 * Returns -1 on failure, size of packet on success.
 *
 ******************************************************************************
 */

static int
VMXRPC_MakePacket(const char *cmd,
                  char **retPacket)
{
   int len;
   int tlen;
   uint32 plen;
   uint32 slen;
   char *packet = NULL;
   char *p;
   struct {
      uint32 type;
      uint32 fieldId;
      uint64 value;
   } hdr;
   struct {
      uint32 type;
      uint32 fieldId;
      uint64 value;
   } flags;
   struct {
      uint32 type;
      uint32 fieldId;
      uint32 len;
   } payload;

   *retPacket = NULL;
   if (cmd == NULL) {
      return -1;
   }

   len = (int) strlen(cmd);

   /* network byte order is important here */
   hdr.type = htonl(1);         // DMFIELDTYPE_INT64
   hdr.fieldId = htonl(1);      // GUESTRPCPKT_FIELD_TYPE
   hdr.value = _vmxlog_htonll(1);       // GUESTRPCPKT_TYPE_DATA

   /*
    * Adding the fast_close flag GUESTRPCPKT_FIELD_FAST_CLOSE in the packet
    * to indicate vmx to close the channel as soon as the response is sent.
    */
   flags.type = htonl(1);            // DMFIELDTYPE_INT64
   flags.fieldId = htonl(3);         // GUESTRPCPKT_FIELD_FAST_CLOSE
   flags.value = _vmxlog_htonll(1);  // GUESTRPCPKT_TYPE_DATA

   /*
    * this part of the data doesn't seem to care about network byte
    * order, but do it anyways.
    */
   payload.type = htonl(2);     // DMFIELDTYPE_STRING
   payload.fieldId = htonl(2);  // GUESTRPCPKT_FIELD_PAYLOAD
   payload.len = htonl(len);    // length of 'cmd'

   plen = sizeof(hdr) + sizeof(flags) + sizeof(payload) + len;

   tlen = plen + sizeof(int);
   packet = (char *) g_malloc(tlen);
   p = packet;

   /* use network byte order overall length */
   slen = htonl(plen);
   memcpy(p, &slen, sizeof slen);
   p += sizeof slen;

   memcpy(p, &hdr, sizeof hdr);
   p += sizeof hdr;
   memcpy(p, &flags, sizeof flags);
   p += sizeof flags;
   memcpy(p, &payload, sizeof payload);
   p += sizeof payload;
   memcpy(p, cmd, len);

   *retPacket = packet;

   return tlen;
}


/*
 ******************************************************************************
 * GetAddressFamily --                                                   */ /**
 *
 * Returns the vsock socket family on success, -1 on failure.
 *
 * This assumes modern vsock is in the kernel.
 ******************************************************************************
 */

static int
GetAddressFamily(void)
{
#ifdef _WIN32
   return __VMCISock_DeviceIoControl(VMCI_SOCKETS_GET_AF_VALUE);
#else
   const int AF_VSOCK_LOCAL = 40;
   int s = socket(AF_VSOCK_LOCAL, SOCK_DGRAM, 0);
   if (s != -1) {
      close(s);
      return AF_VSOCK_LOCAL;
   }

   return -1;
#endif
}


/*
 ******************************************************************************
 * CreateVMCISocket --                                                   */ /**
 *
 * Creates, binds and connects a socket to the VMX.
 *
 * @param[in] useSecure   If TRUE, use bind to a reserved port locally to allow
 *                        for a secure channel.
 *
 * Returns a new socket that should be close()d or -1 on failure.
 *
 ******************************************************************************
 */

static SOCKET
CreateVMCISocket(gboolean useSecure)
{
   struct sockaddr_vm localAddr;
   struct sockaddr_vm addr;
   int ret;
   int errCode;
   unsigned int localPort = PRIVILEGED_PORT_MAX;
   SOCKET fd;

again:
   fd = socket(gAddressFamily, SOCK_STREAM, 0);
   if (fd < 0) {
      g_warning("%s: socket() failed %d\n", __FUNCTION__, GetSocketErrCode());
      return -1;
   }

   memset(&localAddr, 0, sizeof localAddr);
   localAddr.svm_family = gAddressFamily;
#ifdef _WIN32
   localAddr.svm_cid = __VMCISock_DeviceIoControl(VMCI_SOCKETS_GET_LOCAL_CID);
#else
   localAddr.svm_cid = -1;
#endif

   if (useSecure) {
      while (localPort >= PRIVILEGED_PORT_MIN) {
         localAddr.svm_port = localPort;
         ret = bind(fd, (struct sockaddr *)&localAddr, sizeof localAddr);
         if (ret != 0) {
            errCode = GetSocketErrCode();
            if (errCode == SYSERR_EADDRINUSE) {
               g_debug("%s: bind() failed w/ ADDRINUSE, trying another port\n",
                       __FUNCTION__);
               --localPort;
               continue; /* Try next port */
            } else {
               // unexpected failure, bail
               g_warning("%s: bind() failed %d\n", __FUNCTION__, errCode);
               goto err;
            }
         }  else {
            g_debug("%s: bind() worked for port %d\n", __FUNCTION__, localPort);
            goto bound;
         }
      }
      g_warning("%s: failed to find a bindable port\n", __FUNCTION__);
      goto err;
   } else {
      localAddr.svm_port = VMADDR_PORT_ANY;
      ret = bind(fd, (struct sockaddr *)&localAddr, sizeof localAddr);
      if (ret != 0) {
         g_warning("%s: bind() failed %d\n", __FUNCTION__, GetSocketErrCode());
         goto err;
      }
   }

bound:
   /* connect to destination */
   memset(&addr, 0, sizeof addr);
   addr.svm_family = gAddressFamily;
   addr.svm_cid = VMX_CID;
   addr.svm_port = RPCI_PORT;

   ret = connect(fd, (struct sockaddr *)&addr, sizeof addr);
   if (ret < 0) {
      errCode = GetSocketErrCode();
      if (errCode == SYSERR_ECONNRESET) {
         /*
          * VMX might be slow releasing a port pair
          * when another client closed the client side end.
          * Simply try next port.
          */
         g_debug("%s: connect() failed with RESET, trying another port\n",
                 __FUNCTION__);
         localPort--;
         Socket_Close(fd);
         goto again;
      }
      g_warning("%s: connect() failed %d\n", __FUNCTION__, GetSocketErrCode());
      goto err;
   }

   return fd;
err:
   Socket_Close(fd);

   return -1;
}



/*
 ******************************************************************************
 * VMXRPC_Init --                                                        */ /**
 *
 * Initializes VMX secure RPCs.
 *
 * Returns -1 on error, 1 on success.
 *
 ******************************************************************************
 */

int
VMXRPC_Init(void)
{
   if (gAddressFamily != -1) {
      // already initted
      return 1;
   }
#ifdef _WIN32
   int ret;
   WSADATA wsaData;

   ret = WSAStartup(MAKEWORD(2,0), &wsaData);
   if (ret != 0) {
      g_warning("%s: Failed to init winsock (%d)\n", __FUNCTION__, ret);
      return -1;
   }
#endif
   gAddressFamily = GetAddressFamily();
   if (gAddressFamily < 0) {
      g_warning("%s: Failed to set up VMX logging\n", __FUNCTION__);
      return -1;
   }

   return 1;
}


/*
 ******************************************************************************
 * VMXRPC_SendRpc --                                                     */ /**
 *
 * Sends RPC packet to the VMX.  Returns any response if retBuf is
 * non-NULL.
 *
 * @param[in] cmd         RPC command
 * @param[in] useSecure   If TRUE, use bind to a reserved port locally to allow
 *                        for a secure channel.
 * @param[out] retBuf     RPC reply.
 *
 * Returns -1 on failure, or the length of the returned reply on success (0 if
 * retBuf is NULL).
 *
 ******************************************************************************
 */

int
VMXRPC_SendRpc(const gchar *cmd,
               gboolean useSecure,
               gchar **retBuf)
{
   SOCKET sock;
   char *fullReply = NULL;
   char *bp;
   uint32 repLen;
   uint32 curLen;
   int ret;
   int retVal = 0;
   gchar *packet = NULL;
   int packetLen;
   struct {
      uint32 len;
   } hdr;
   // bytes into the DataMap stream where the reply starts
#define REPLY_OFFSET 14

   if (VMXRPC_Init() != 1) {
      g_warning("%s: couldn't get VMCI address family\n", __FUNCTION__);
      return -1;
   }

   sock = CreateVMCISocket(useSecure);
   if (sock < 0) {
      g_warning("%s: failed to create VMCI socket\n", __FUNCTION__);
      return -1;
   }

   packetLen = VMXRPC_MakePacket(cmd, &packet);
   if (packetLen <= 0) {
      g_warning("%s: failed to build RPC packet\n", __FUNCTION__);
      retVal = -1;
      goto done;
   }

   ret = send(sock, packet, packetLen, 0);
   if (ret != packetLen) {
      g_warning("%s: failed to send packet (%d)\n",
                __FUNCTION__, GetSocketErrCode());
      retVal = -1;
      goto done;
   }

   // get the header, which is the length of the rest of the reply
   ret = recv(sock, (char *) &hdr, sizeof hdr, 0);
   if (ret != sizeof(hdr)) {
      g_warning("%s: failed to read reply length (wanted %d, got %d) (%d)\n",
                __FUNCTION__, (int) sizeof(hdr), ret, GetSocketErrCode());
      retVal = -1;
      goto done;
   }
   repLen = ntohl(hdr.len);
   g_debug("%s: reply len: %u\n", __FUNCTION__, repLen);
   if (repLen < REPLY_OFFSET) {
      g_warning("%s: reply len too small (%u)\n",
                __FUNCTION__, repLen);
      retVal = -1;
      goto done;
   }
   // +1 to ensure a NUL at the end
   fullReply = g_malloc0((sizeof(char) * repLen) + 1);
   curLen = 0;
   bp = fullReply;
   // handle the case it somehow gets chopped up
   while (curLen < repLen) {
      ret = recv(sock, bp, repLen - curLen, 0);
      if (ret < 0) {
         g_warning("%s: failed to read reply  packet (%d)\n",
                   __FUNCTION__, GetSocketErrCode());
         retVal = -1;
         goto done;
      } else if (ret == 0) {  // unexpected EOF, fail
         g_warning("%s: unexpected EOF, failing\n", __FUNCTION__);
         retVal = -1;
         goto done;
      }
      curLen += ret;
      bp += ret;
   }

done:
#ifdef _WIN32
   closesocket(sock);
#else
   close(sock);
#endif
   if (retVal >= 0 && retBuf != NULL) {
      // the useful part starts at REPLY_OFFSET after the DataMaps
      *retBuf = g_strdup(fullReply + REPLY_OFFSET);
      retVal = repLen - REPLY_OFFSET;
   }
   g_free(fullReply);
   g_free(packet);

   return retVal;
}


#ifdef TEST
/* build test app  with
 * $ gcc -g -DTEST -I/usr/include/glib-2.0 \
 *    -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -o /tmp/vmxrpc-test \
 *    vmxrpc.c -lglib-2.0
 */
int
main(int argc, char **argv)
{
   char *reply;
   int ret;

   if (argc < 2) {
      fprintf(stderr, "%s: needs an RPC arg\n", argv[0]);
      exit(-1);
   }
   ret = VMXRPC_SendRpc(argv[1], TRUE, &reply);
   if (ret < 0) {
      fprintf(stderr, "%s: failed to send RPC\n", argv[0]);
      exit(-1);
   } else {
      puts(reply);
      g_free(reply);
   }
   exit(0);
}
#endif // TEST
