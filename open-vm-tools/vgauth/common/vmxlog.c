/*********************************************************
 * Copyright (C) 2018 VMware, Inc. All rights reserved.
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
 * @file logvmx.c
 *
 * Simple guest->VMX RPC log support that assumes VMCI is available.
 *
 * Doesn't share any Tools code or headers; key bits that can't change
 * are copied from vmci_sockets.h
 *
 */

#include "vmxlog.h"
#include <glib.h>

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

static int gAddressFamily = -1;
#define LOG_RPC_CMD  "log"
#define LOG_RPC_CMD_NEW  "guest.log.text"

static gboolean gDisableVMXLogging = TRUE;

/*
 * Error codes for SendString() and SendRpciPacket()
 */
#define VMX_RPC_OK       1                    // success
#define VMX_RPC_UNKNOWN  0                    // RPC disabled or not supported
#define VMX_RPC_ERROR   -1                    // failed to send RPC

#define VMXLOG_SERVICE_NAME      "[vgauthservice]"
/*
 * Some typedefs for portability.
 */
#ifdef _WIN32
/* still have to care about pre-C++11 so use the old instead of stdint.h */
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;

#endif
#ifdef linux
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
 * MakePacket --                                                         */ /**
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
MakePacket(const char *cmd,
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
    * this part of the data doesn't seem to care about network byte
    * order, but do it anyways.
    */
   payload.type = htonl(2);     // DMFIELDTYPE_STRING
   payload.fieldId = htonl(2);  // GUESTRPCPKT_FIELD_PAYLOAD
   payload.len = htonl(len);    // length of 'cmd'

   plen = sizeof(hdr) + sizeof(payload) + len;

   tlen = plen + sizeof(int);
   packet = (char *) g_malloc(tlen);
   p = packet;

   /* use network byte order overall length */
   slen = htonl(plen);
   memcpy(p, (char *)&slen, sizeof slen);
   p += sizeof(uint32);

   memcpy(p, &hdr, sizeof hdr);
   p += sizeof hdr;
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

int
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
 * Returns a new socket that should be close()d or -1 on failure.
 *
 ******************************************************************************
 */

static SOCKET
CreateVMCISocket(void)
{
   struct sockaddr_vm localAddr;
   struct sockaddr_vm addr;
   int ret;
   SOCKET fd = socket(gAddressFamily, SOCK_STREAM, 0);
   if (fd < 0) {
      g_warning("%s: socket() failed %d\n", __FUNCTION__, GetSocketErrCode());
      return -1;
   }

   /* bind to a priviledged port */
   memset(&localAddr, 0, sizeof localAddr);
   localAddr.svm_family = gAddressFamily;
#ifdef _WIN32
   localAddr.svm_cid = __VMCISock_DeviceIoControl(VMCI_SOCKETS_GET_LOCAL_CID);
#else
   localAddr.svm_cid = -1;
#endif
   localAddr.svm_port = VMADDR_PORT_ANY;
   ret = bind(fd, (struct sockaddr *)&localAddr, sizeof localAddr);
   if (ret != 0) {
      g_warning("%s: bind() failed %d\n", __FUNCTION__, GetSocketErrCode());
      goto err;
   }

   /* connect to destination */
   memset(&addr, 0, sizeof addr);
   addr.svm_family = gAddressFamily;
   addr.svm_cid = VMX_CID;
   addr.svm_port = RPCI_PORT;

   ret = connect(fd, (struct sockaddr *)&addr, sizeof addr);
   if (ret < 0) {
      g_warning("%s: connect() failed %d\n", __FUNCTION__, GetSocketErrCode());
      goto err;
   }

   return fd;
err:
#ifdef _WIN32
   closesocket(fd);
#else
   close(fd);
#endif

   return -1;
}


/*
 ******************************************************************************
 * SendRpciPacket --                                                     */ /**
 *
 * Sends RPC packet to the VMX.  Reads but ignores any response.
 *
 * @param[in] packet      RPC packet.
 * @param[in] packetLen   Length of packet.
 *
 * Returns VMX_RPC_ERROR on failure, VMX_RPC_OK on success, VMX_RPC_UNKNOWN
 * if RPC failed (doesn't exist or disabled).
 *
 ******************************************************************************
 */

static int
SendRpciPacket(const char *packet,
               int packetLen)
{
   SOCKET sock;
   char buf[1024];
   char *reply;
   int ret;
   int retVal = VMX_RPC_OK;

   /*
    * Its inefficient to create/destroy the socket each time, but
    * there's potential to run the VMX out of connections if we hold it open.
    * Since performance isn't a major concern, play it safe.
    */
   sock = CreateVMCISocket();
   if (sock < 0) {
      g_warning("%s: failed to create VMCI socket\n", __FUNCTION__);
      return VMX_RPC_ERROR;
   }
   ret = send(sock, packet, packetLen, 0);
   if (ret != packetLen) {
      g_warning("%s: failed to send packet (%d)\n",
                __FUNCTION__, GetSocketErrCode());
      retVal = VMX_RPC_ERROR;
      goto done;
   }

   /*
    * Read the answer to see if the RPC went through.
    */
   ret = recv(sock, buf, sizeof buf, 0);
   /*
    * Cheat a bit here -- just get to the text and ignore the header.
    * The string data starts 19 chars into the buffer.
    *
    * XXX should this only happen on 'Unknown'?  If the VMX
    * level changes on the fly, this can start working.
    *
    * Possible optimization -- every N minutes, retry the new RPC.
    */
   if (ret >= 18 && ret < sizeof buf) {
      buf[ret] = '\0';
      reply = &buf[18];
      g_debug("%s: RPC returned '%s'\n", __FUNCTION__, reply);
      if (g_strcmp0(reply, "disabled") == 0 ||
          g_strcmp0(reply, "Unknown") == 0) {
         g_warning("%s: RPC unknown or disabled\n", __FUNCTION__);
         retVal = VMX_RPC_UNKNOWN;
      }
   } else {
      g_warning("%s: recv() returned %d\n", __FUNCTION__, ret);
      retVal = VMX_RPC_ERROR;
   }

done:
#ifdef _WIN32
   closesocket(sock);
#else
   close(sock);
#endif

   return retVal;
}


/*
 ******************************************************************************
 * VMXLog_Init --                                                        */ /**
 *
 * Initializes the VMX log facility.
 *
 * Returns -1 on error, 1 on success.
 *
 ******************************************************************************
 */

int
VMXLog_Init(void)
{
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

   gDisableVMXLogging = FALSE;
   return 1;
}


/*
 ******************************************************************************
 * VMXLog_Shutdown --                                                    */ /**
 *
 * Shuts down the VMX log facility.
 *
 ******************************************************************************
 */

void
VMXLog_Shutdown(void)
{
   gDisableVMXLogging = TRUE;
   gAddressFamily = -1;
}


/*
 ******************************************************************************
 * SendString --                                                         */ /**
 *
 * Formats a message as an RPC and sends it to the VMX.
 *
 * @param[in] cmd       The message to send.
 *
 * Returns VMX_RPC_ERROR on failure, VMX_RPC_OK on success, VMX_RPC_UNKNOWN
 * if RPC failed (doesn't exist or disabled).
 ******************************************************************************
 */

static int
SendString(const gchar *cmd)
{
   gchar *packet = NULL;
   int packetLen;
   int ret;

   packetLen = MakePacket(cmd, &packet);
   if (packetLen <= 0) {
      g_warning("%s: failed to build RPC packet\n", __FUNCTION__);
      return VMX_RPC_ERROR;
   }

   ret = SendRpciPacket(packet, packetLen);
   if (ret == VMX_RPC_ERROR) {
      g_warning("%s: failed to send RPC packet\n", __FUNCTION__);
   }

   g_free(packet);
   return ret;
}


/*
 ******************************************************************************
 * VMXLog_LogV --                                                        */ /**
 *
 * Logs to the VMX using va_list arguments.
 *
 * @param[in] level       Logging level (currently unused).
 * @param[in] fmt         The format message for the event.
 * @param[in] args        The arguments for @a fmt.
 *
 ******************************************************************************
 */

void
VMXLog_LogV(int level,
            const char *fmt,
            va_list args)
{
   gchar *msg = NULL;
   gchar *cmd = NULL;
   int ret;
   static gboolean useNewRpc = TRUE;
   static gboolean rpcBroken = FALSE;

   /*
    * RPCs don't work -- not in a VM or no vmci -- so drop any messages.
    */
   if (gDisableVMXLogging || rpcBroken) {
      return;
   }

   msg = g_strdup_vprintf(fmt, args);
again:
   /*
    * Try the new logging RPC, fail over to the old
    *
    * Possible optimization -- every N minutes, retry the new RPC in
    * case its been enabled dynamically.
    */
   if (useNewRpc) {
      /* XXX TODO use the level */
      cmd = g_strdup_printf("%s " VMXLOG_SERVICE_NAME " %s", LOG_RPC_CMD_NEW, msg);
   } else {
      cmd = g_strdup_printf("%s " VMXLOG_SERVICE_NAME " %s", LOG_RPC_CMD, msg);
   }

   ret = SendString(cmd);
   g_free(cmd);
   cmd = NULL;
   if ((ret == VMX_RPC_UNKNOWN) && useNewRpc) {
      g_debug("%s: new RPC Failed, using old\n", __FUNCTION__);
      useNewRpc = FALSE;
      goto again;
   } else if (ret == VMX_RPC_ERROR) {
      rpcBroken = TRUE;
      g_debug("%s: Error sending RPC, assume they aren't supported\n",
              __FUNCTION__);
   }

   g_free(msg);
   msg = NULL;
}


/*
 ******************************************************************************
 * VMXLog_Log --                                                        */ /**
 *
 * Logs to the VMX.
 *
 * @param[in] level       Logging level (currently unused).
 * @param[in] fmt         The format message for the event.
 * @param[in] args        The arguments for @a fmt.
 *
 ******************************************************************************
 */

void
VMXLog_Log(int level,
           const char *fmt,
           ...)
{
   va_list args;

   va_start(args, fmt);
   VMXLog_LogV(level, fmt, args);
   va_end(args);
}
