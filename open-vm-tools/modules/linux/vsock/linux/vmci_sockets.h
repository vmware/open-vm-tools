/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmci_sockets.h --
 *
 *    VMCI sockets public constants and types.
 */

#ifndef _VMCI_SOCKETS_H_
#define _VMCI_SOCKETS_H_


#if defined(_WIN32)
#  if !defined(_DDK_DRIVER_)
#     include <winsock2.h>
#  endif // _DDK_DRIVER_
#else // _WIN32
#if defined(linux) && !defined(VMKERNEL)
#  if !defined(__KERNEL__)
#    include <sys/socket.h>
#  endif // __KERNEL__
#else // linux && !VMKERNEL
#  if defined(__APPLE__)
#    include <sys/socket.h>
#    include <string.h>
#  endif // __APPLE__
#endif // linux && !VMKERNEL
#endif

/*
 * We use the same value for the AF family and the socket option
 * level. To set options, use the value of VMCISock_GetAFValue for
 * 'level' and these constants for the optname.
 */
#define SO_VMCI_BUFFER_SIZE                 0
#define SO_VMCI_BUFFER_MIN_SIZE             1
#define SO_VMCI_BUFFER_MAX_SIZE             2
#define SO_VMCI_PEER_HOST_VM_ID             3

/*
 * The VMCI sockets address equivalents of INADDR_ANY.  The first works for
 * the svm_cid (context id) field of the address structure below and indicates
 * the current guest (or the host, if running outside a guest), while the
 * second indicates any available port.
 */
#define VMADDR_CID_ANY  ((unsigned int) -1)
#define VMADDR_PORT_ANY ((unsigned int) -1)


#if defined(_WIN32) || defined(VMKERNEL)
   typedef unsigned short sa_family_t;
#endif // _WIN32

#if defined(VMKERNEL)
   struct sockaddr {
      sa_family_t sa_family;
      char sa_data[14];
   };
#endif

/*
 * Address structure for VSockets VMCI sockets. The address family should be
 * set to AF_VMCI. The structure members should all align on their natural
 * boundaries without resorting to compiler packing directives.
 */

struct sockaddr_vm {
#if defined(__APPLE__)
   unsigned char svm_len;                           // Mac OS has the length first.
#endif // __APPLE__
   sa_family_t svm_family;                          // AF_VMCI.
   unsigned short svm_reserved1;                    // Reserved.
   unsigned int svm_port;                           // Port.
   unsigned int svm_cid;                            // Context id.
   unsigned char svm_zero[sizeof(struct sockaddr) - // Same size as sockaddr.
#if defined(__APPLE__)
                             sizeof(unsigned char) -
#endif // __APPLE__
                             sizeof(sa_family_t) -
                             sizeof(unsigned short) -
                             sizeof(unsigned int) -
                             sizeof(unsigned int)];
};


#if defined(_WIN32)
#  if !defined(_DDK_DRIVER_)
#     include <winioctl.h>
#     define VMCI_SOCKETS_DEVICE          L"\\\\.\\VMCI"
#     define VMCI_SOCKETS_GET_AF_VALUE    0x81032068
#     define VMCI_SOCKETS_GET_LOCAL_CID   0x8103206c
      static __inline int VMCISock_GetAFValue(void)
      {
         int afvalue = -1;
         HANDLE device = CreateFileW(VMCI_SOCKETS_DEVICE, GENERIC_READ, 0, NULL,
                                     OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
         if (INVALID_HANDLE_VALUE != device) {
            DWORD ioReturn;
            DeviceIoControl(device, VMCI_SOCKETS_GET_AF_VALUE, &afvalue,
                            sizeof afvalue, &afvalue, sizeof afvalue,
                            &ioReturn, NULL);
            CloseHandle(device);
            device = INVALID_HANDLE_VALUE;
         }
         return afvalue;
      }
   
      static __inline unsigned int VMCISock_GetLocalCID(void)
      {
         unsigned int cid = VMADDR_CID_ANY;
         HANDLE device = CreateFileW(VMCI_SOCKETS_DEVICE, GENERIC_READ, 0, NULL,
                                     OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
         if (INVALID_HANDLE_VALUE != device) {
            DWORD ioReturn;
            DeviceIoControl(device, VMCI_SOCKETS_GET_LOCAL_CID, &cid,
                            sizeof cid, &cid, sizeof cid, &ioReturn,
                            NULL);
            CloseHandle(device);
            device = INVALID_HANDLE_VALUE;
         }
         return cid;
      }
#  endif // _DDK_DRIVER_
#else // _WIN32
#if defined(linux) && !defined(VMKERNEL)
#  ifdef __KERNEL__
   void VMCISock_KernelRegister(void);
   void VMCISock_KernelDeregister(void);
   int VMCISock_GetAFValue(void);
   int VMCISock_GetLocalCID(void);
#  else // __KERNEL__
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <unistd.h>

#  include <stdio.h>

#  define VMCI_SOCKETS_DEFAULT_DEVICE      "/dev/vsock"
#  define VMCI_SOCKETS_CLASSIC_ESX_DEVICE  "/vmfs/devices/char/vsock/vsock"
#  define IOCTL_VMCI_SOCKETS_GET_AF_VALUE  1976
#  define IOCTL_VMCI_SOCKETS_GET_LOCAL_CID 1977

   /*
    *----------------------------------------------------------------------------
    *
    * VMCISock_GetAFValue and VMCISock_GetAFValueFd --
    *
    *      Returns the value to be used for the VMCI Sockets address family.
    *      This value should be used as the domain argument to socket(2) (when
    *      you might otherwise use AF_INET).  For VMCI Socket-specific options,
    *      this value should also be used for the level argument to
    *      setsockopt(2) (when you might otherwise use SOL_TCP).
    *
    *      This function leaves its descriptor to the vsock device open so that
    *      the socket implementation knows that the socket family is still in
    *      use.  We do this because we register our address family with the
    *      kernel on-demand and need a notification to unregister the address
    *      family.
    *
    *      For many programs this behavior is sufficient as is, but some may
    *      wish to close this descriptor once they are done with VMCI Sockets.
    *      For these programs, we provide a VMCISock_GetAFValueFd() that takes
    *      an optional outFd argument.  This value can be provided to
    *      VMCISock_ReleaseAFValueFd() only after the program no longer will
    *      use VMCI Sockets.  Note that outFd is only valid in cases where
    *      VMCISock_GetAFValueFd() returns a non-negative value.
    *
    * Results:
    *      The address family value to use on success, negative error code on
    *      failure.
    *
    *----------------------------------------------------------------------------
    */

   static inline int VMCISock_GetAFValueFd(int *outFd)
   {
      int fd;
      int family;

      fd = open(VMCI_SOCKETS_DEFAULT_DEVICE, O_RDWR);
      if (fd < 0) {
         fd = open(VMCI_SOCKETS_CLASSIC_ESX_DEVICE, O_RDWR);
         if (fd < 0) {
            return -1;
         }
      }

      if (ioctl(fd, IOCTL_VMCI_SOCKETS_GET_AF_VALUE, &family) < 0) {
         family = -1;
      }

      if (family < 0) {
         close(fd);
      } else if (outFd) {
         *outFd = fd;
      }

      return family;
   }

   static inline int VMCISock_GetAFValue(void)
   {
      return VMCISock_GetAFValueFd(NULL);
   }


   static inline void VMCISock_ReleaseAFValueFd(int fd)
   {
      if (fd >= 0) {
         close(fd);
      }
   }

   static inline unsigned int VMCISock_GetLocalCID(void)
   {
      int fd;
      unsigned int contextId;

      fd = open(VMCI_SOCKETS_DEFAULT_DEVICE, O_RDWR);
      if (fd < 0) {
         fd = open(VMCI_SOCKETS_CLASSIC_ESX_DEVICE, O_RDWR);
         if (fd < 0) {
            return VMADDR_CID_ANY;
         }
      }

      if (ioctl(fd, IOCTL_VMCI_SOCKETS_GET_LOCAL_CID, &contextId) < 0) {
         contextId = VMADDR_CID_ANY;
      }

      close(fd);
      return contextId;
   }
#  endif // __KERNEL__
#else
#if defined(__APPLE__) && !defined(KERNEL)
extern int VMCISock_GetAFValue(void);
extern unsigned int VMCISock_GetLocalCID(void);
#endif // __APPLE__ && !KERNEL
#endif // linux && !VMKERNEL
#endif // _WIN32


#endif // _VMCI_SOCKETS_H_

