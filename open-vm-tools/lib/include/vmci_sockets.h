/*********************************************************
 * Copyright (C) 2007-2012 VMware, Inc. All rights reserved.
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
 * vmci_sockets.h --
 *
 *    VMCI sockets public constants and types.
 */

#ifndef _VMCI_SOCKETS_H_
#define _VMCI_SOCKETS_H_


#if defined(_WIN32)
#  if !defined(NT_INCLUDED)
#     include <winsock2.h>
#  endif // !NT_INCLUDED
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

/**
 * \brief Option name for STREAM socket buffer size.
 *
 * Use as the option name in \c setsockopt(3) or \c getsockopt(3) to set
 * or get an \c unsigned \c long \c long that specifies the size of the
 * buffer underlying a vSockets STREAM socket.
 *
 * \note Value is clamped to the MIN and MAX.
 *
 * \see VMCISock_GetAFValueFd()
 * \see SO_VMCI_BUFFER_MIN_SIZE
 * \see SO_VMCI_BUFFER_MAX_SIZE
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * unsigned long long val = 0x1000;
 * int fd = socket(af, SOCK_STREAM, 0);
 * setsockopt(fd, af, SO_VMCI_BUFFER_SIZE, &val, sizeof val);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define SO_VMCI_BUFFER_SIZE                 0

/**
 * \brief Option name for STREAM socket minimum buffer size.
 *
 * Use as the option name in \c setsockopt(3) or \c getsockopt(3) to set
 * or get an \c unsigned \c long \c long that specifies the minimum size
 * allowed for the buffer underlying a vSockets STREAM socket.
 *
 * \see VMCISock_GetAFValueFd()
 * \see SO_VMCI_BUFFER_SIZE
 * \see SO_VMCI_BUFFER_MAX_SIZE
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * unsigned long long val = 0x500;
 * int fd = socket(af, SOCK_STREAM, 0);
 * setsockopt(fd, af, SO_VMCI_BUFFER_MIN_SIZE, &val, sizeof val);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define SO_VMCI_BUFFER_MIN_SIZE             1

/**
 * \brief Option name for STREAM socket maximum buffer size.
 *
 * Use as the option name in \c setsockopt(3) or \c getsockopt(3) to set or
 * get an unsigned long long that specifies the maximum size allowed for the
 * buffer underlying a vSockets STREAM socket.
 *
 * \see VMCISock_GetAFValueFd()
 * \see SO_VMCI_BUFFER_SIZE
 * \see SO_VMCI_BUFFER_MIN_SIZE
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * unsigned long long val = 0x4000;
 * int fd = socket(af, SOCK_STREAM, 0);
 * setsockopt(fd, af, SO_VMCI_BUFFER_MAX_SIZE, &val, sizeof val);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define SO_VMCI_BUFFER_MAX_SIZE             2

/**
 * \brief Option name for socket peer's host-specific VM ID.
 *
 * Use as the option name in \c getsockopt(3) to get a host-specific identifier
 * for the peer endpoint's VM.  The identifier is a signed integer.
 *
 * \note Only available for ESX (VMKernel/userworld) endpoints.
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * int id;
 * socklen_t len = sizeof id;
 * int fd = socket(af, SOCK_DGRAM, 0);
 * getsockopt(fd, af, SO_VMCI_PEER_HOST_VM_ID, &id, &len);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define SO_VMCI_PEER_HOST_VM_ID             3

/**
 * \brief Option name for socket's service label.
 *
 * Use as the option name in \c setsockopt(3) or \c getsockopt(3) to set or
 * get the service label for a socket.  The service label is a C-style
 * NUL-terminated string.
 *
 * \note Only available for ESX (VMkernel/userworld) endpoints.
 */

#define SO_VMCI_SERVICE_LABEL               4

/**
 * \brief Option name for determining if a socket is trusted.
 *
 * Use as the option name in \c getsockopt(3) to determine if a socket is
 * trusted.  The value is a signed integer.
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * int trusted;
 * socklen_t len = sizeof trusted;
 * int fd = socket(af, SOCK_DGRAM, 0);
 * getsockopt(fd, af, SO_VMCI_TRUSTED, &trusted, &len);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define SO_VMCI_TRUSTED                     5

/**
 * \brief Option name for STREAM socket connection timeout.
 *
 * Use as the option name in \c setsockopt(3) or \c getsockopt(3) to set or
 * get the connection timeout for a STREAM socket.  The value is platform
 * dependent.  On ESX, Linux and Mac OS, it is a \c struct \c timeval.
 * On Windows, it is a \c DWORD.
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * struct timeval t = { 5, 100000 }; // 5.1 seconds
 * int fd = socket(af, SOCK_STREAM, 0);
 * setsockopt(fd, af, SO_VMCI_CONNECT_TIMEOUT, &t, sizeof t);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define SO_VMCI_CONNECT_TIMEOUT             6

/**
 * \brief Option name for using non-blocking send/receive.
 *
 * Use as the option name for \c setsockopt(3) or \c getsockopt(3) to set or
 * get the non-blocking transmit/receive flag for a STREAM socket.  This flag
 * determines whether \c send() and \c recv() can be called in non-blocking
 * contexts for the given socket.  The value is a signed integer.
 *
 * This option is only relevant to kernel endpoints, where descheduling
 * the thread of execution is not allowed, for example, while holding a
 * spinlock.  It is not to be confused with conventional non-blocking socket
 * operations.
 *
 * \note Only available for VMKernel endpoints.
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * int nonblock;
 * socklen_t len = sizeof nonblock;
 * int fd = socket(af, SOCK_STREAM, 0);
 * getsockopt(fd, af, SO_VMCI_NONBLOCK_TXRX, &nonblock, &len);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define SO_VMCI_NONBLOCK_TXRX               7

/**
 * \brief The vSocket equivalent of INADDR_ANY.
 *
 * This works for the \c svm_cid field of sockaddr_vm and indicates the
 * context ID of the current endpoint.
 *
 * \see sockaddr_vm
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * struct sockaddr_vm addr;
 * int fd = socket(af, SOCK_DGRAM, 0);
 * addr.svm_family = af;
 * addr.svm_cid = VMADDR_CID_ANY;
 * addr.svm_port = 2000;
 * bind(fd, &addr, sizeof addr);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define VMADDR_CID_ANY  ((unsigned int)-1)

/**
 * \brief Bind to any available port.
 *
 * Works for the \c svm_port field of sockaddr_vm.
 *
 * \see sockaddr_vm
 *
 * An example is given below.
 *
 * \code
 * int vmciFd;
 * int af = VMCISock_GetAFValueFd(&vmciFd);
 * struct sockaddr_vm addr;
 * int fd = socket(af, SOCK_DGRAM, 0);
 * addr.svm_family = af;
 * addr.svm_cid = VMADDR_CID_ANY;
 * addr.svm_port = VMADDR_PORT_ANY;
 * bind(fd, &addr, sizeof addr);
 * ...
 * close(fd);
 * VMCISock_ReleaseAFValueFd(vmciFd);
 * \endcode
 */

#define VMADDR_PORT_ANY ((unsigned int)-1)

/**
 * \brief Invalid vSockets version.
 *
 * \see VMCISock_Version()
 */

#define VMCI_SOCKETS_INVALID_VERSION ((unsigned int)-1)

/**
 * \brief The epoch (first) component of the vSockets version.
 *
 * A single byte representing the epoch component of the vSockets version.
 *
 * \see VMCISock_Version()
 *
 * An example is given below.
 *
 * \code
 * unsigned int ver = VMCISock_Version();
 * unsigned char epoch = VMCI_SOCKETS_VERSION_EPOCH(ver);
 * \endcode
 */

#define VMCI_SOCKETS_VERSION_EPOCH(_v) (((_v) & 0xFF000000) >> 24)

/**
 * \brief The major (second) component of the vSockets version.
 *
 * A single byte representing the major component of the vSockets version.
 * Typically changes for every major release of a product.
 *
 * \see VMCISock_Version()
 *
 * An example is given below.
 *
 * \code
 * unsigned int ver = VMCISock_Version();
 * unsigned char major = VMCI_SOCKETS_VERSION_MAJOR(ver);
 * \endcode
 */

#define VMCI_SOCKETS_VERSION_MAJOR(_v) (((_v) & 0x00FF0000) >> 16)

/**
 * \brief The minor (third) component of the vSockets version.
 *
 * Two bytes representing the minor component of the vSockets version.
 *
 * \see VMCISock_Version()
 *
 * An example is given below.
 *
 * \code
 * unsigned int ver = VMCISock_Version();
 * unsigned short minor = VMCI_SOCKETS_VERSION_MINOR(ver);
 * \endcode
 */

#define VMCI_SOCKETS_VERSION_MINOR(_v) (((_v) & 0x0000FFFF))

/** \cond PRIVATE */
#if defined(_WIN32) || defined(VMKERNEL)
   typedef unsigned short sa_family_t;
#endif // _WIN32

#if defined(VMKERNEL)
   struct sockaddr {
      sa_family_t sa_family;
      char sa_data[14];
   };
#endif
/** \endcond */

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
#if defined(__APPLE__)
   unsigned char svm_len;
#endif // __APPLE__

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


#if defined(_WIN32)
#  if !defined(NT_INCLUDED)
#     include <winioctl.h>
#     define VMCI_SOCKETS_DEVICE          L"\\\\.\\VMCI"
#     define VMCI_SOCKETS_VERSION         0x81032058
#     define VMCI_SOCKETS_GET_AF_VALUE    0x81032068
#     define VMCI_SOCKETS_GET_LOCAL_CID   0x8103206c

      static __inline unsigned int __VMCISock_DeviceIoControl(DWORD cmd)
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

      static __inline unsigned int VMCISock_Version(void)
      {
         return __VMCISock_DeviceIoControl(VMCI_SOCKETS_VERSION);
      }

      static __inline int VMCISock_GetAFValue(void)
      {
         return (int)__VMCISock_DeviceIoControl(VMCI_SOCKETS_GET_AF_VALUE);
      }

      static __inline int VMCISock_GetAFValueFd(int *outFd)
      {
         (void)outFd; /* Unused parameter. */
         return VMCISock_GetAFValue();
      }

      static __inline void VMCISock_ReleaseAFValueFd(int fd)
      {
         (void)fd; /* Unused parameter. */
      }

      static __inline unsigned int VMCISock_GetLocalCID(void)
      {
         return __VMCISock_DeviceIoControl(VMCI_SOCKETS_GET_LOCAL_CID);
      }
#  endif // !NT_INCLUDED
#else // _WIN32
#if (defined(linux) && !defined(VMKERNEL)) || (defined(__APPLE__))
#  if defined(linux) && defined(__KERNEL__)
   void VMCISock_KernelRegister(void);
   void VMCISock_KernelDeregister(void);
   int VMCISock_GetAFValue(void);
   int VMCISock_GetLocalCID(void);
#  elif defined(__APPLE__) && (KERNEL)
   /* Nothing to define here. */
#  else // __KERNEL__
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <unistd.h>

#  include <stdio.h>

/** \cond PRIVATE */
#  define VMCI_SOCKETS_DEFAULT_DEVICE      "/dev/vsock"
#  define VMCI_SOCKETS_CLASSIC_ESX_DEVICE  "/vmfs/devices/char/vsock/vsock"
#  if defined(linux)
#     define VMCI_SOCKETS_VERSION       1972
#     define VMCI_SOCKETS_GET_AF_VALUE  1976
#     define VMCI_SOCKETS_GET_LOCAL_CID 1977
#  elif defined(__APPLE__)
#     include <sys/ioccom.h>
#     define VMCI_SOCKETS_VERSION       _IOR('V', 21,  unsigned)
#     define VMCI_SOCKETS_GET_AF_VALUE  _IOR('V', 25 , int)
#     define VMCI_SOCKETS_GET_LOCAL_CID _IOR('V', 26 , unsigned)
#endif
/** \endcond */

   /*
    ***********************************************************************
    * VMCISock_Version                                               */ /**
    *
    * \brief Retrieve the vSockets version.
    *
    * Returns the current version of vSockets.  The version is a 32-bit
    * unsigned integer that consist of three components: the epoch, the
    * major version, and the minor version.  Use the \c VMCI_SOCKETS_VERSION
    * macros to extract the components.
    *
    * \see VMCI_SOCKETS_VERSION_EPOCH()
    * \see VMCI_SOCKETS_VERSION_MAJOR()
    * \see VMCI_SOCKETS_VERSION_MINOR()
    *
    * \retval  VMCI_SOCKETS_INVALID_VERSION  Not available.
    * \retval  other                         The current version.
    *
    * An example is given below.
    *
    * \code
    * unsigned int ver = VMCISock_Version();
    * if (ver != VMCI_SOCKETS_INVALID_VERSION) {
    *    printf("vSockets version=%d.%d.%d\n",
    *           VMCI_SOCKETS_VERSION_EPOCH(ver),
    *           VMCI_SOCKETS_VERSION_MAJOR(ver),
    *           VMCI_SOCKETS_VERSION_MINOR(ver));
    * }
    * \endcode
    *
    ***********************************************************************
    */

   static inline unsigned int VMCISock_Version(void)
   {
      int fd;
      unsigned int version;

      fd = open(VMCI_SOCKETS_DEFAULT_DEVICE, O_RDWR);
      if (fd < 0) {
         fd = open(VMCI_SOCKETS_CLASSIC_ESX_DEVICE, O_RDWR);
         if (fd < 0) {
            return VMCI_SOCKETS_INVALID_VERSION;
         }
      }

      if (ioctl(fd, VMCI_SOCKETS_VERSION, &version) < 0) {
         version = VMCI_SOCKETS_INVALID_VERSION;
      }

      close(fd);
      return version;
   }

   /*
    ***********************************************************************
    * VMCISock_GetAFValueFd                                          */ /**
    *
    * \brief Retrieve the address family value for vSockets.
    *
    * Returns the value to be used for the VMCI Sockets address family.
    * This value should be used as the domain argument to \c socket(2) (when
    * you might otherwise use \c AF_INET).  For VMCI Socket-specific options,
    * this value should also be used for the level argument to
    * \c setsockopt(2) (when you might otherwise use \c SOL_TCP).
    *
    * \see VMCISock_ReleaseAFValueFd()
    * \see sockaddr_vm
    *
    * \param[out]    outFd    File descriptor to the VMCI device.  The
    *                         address family value is valid until this
    *                         descriptor is closed.  This parameter is
    *                         only valid if the return value is not -1.
    *                         Call VMCISock_ReleaseAFValueFd() to  close
    *                         this descriptor.
    *
    * \retval  -1       Not available.
    * \retval  other    The address family value.
    *
    * An example is given below.
    *
    * \code
    * int vmciFd;
    * int af = VMCISock_GetAFValueFd(&vmciFd);
    * if (af != -1) {
    *    int fd = socket(af, SOCK_STREAM, 0);
    *    ...
    *    close(fd);
    *    close(vmciFd);
    * }
    * \endcode
    *
    ***********************************************************************
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

      if (ioctl(fd, VMCI_SOCKETS_GET_AF_VALUE, &family) < 0) {
         family = -1;
      }

      if (family < 0) {
         close(fd);
      } else if (outFd) {
         *outFd = fd;
      }

      return family;
   }

   /** \cond PRIVATE */
   /*
    ***********************************************************************
    * VMCISock_GetAFValue                                            */ /**
    *
    * \brief Retrieve the address family value for vSockets.
    *
    * Returns the value to be used for the VMCI Sockets address family.
    * This value should be used as the domain argument to \c socket(2) (when
    * you might otherwise use \c AF_INET).  For VMCI Socket-specific options,
    * this value should also be used for the level argument to
    * \c setsockopt(2) (when you might otherwise use \c SOL_TCP).
    *
    * \note This function leaves its descriptor to the vsock device open so
    * that the socket implementation knows that the socket family is still in
    * use.  This is done because the address family is registered with the
    * kernel on-demand and a notification is needed to unregister the address
    * family.  Use of this function is thus discouraged; please use
    * VMCISock_GetAFValueFd() instead.
    *
    * \see VMCISock_GetAFValueFd()
    * \see sockaddr_vm
    *
    * \retval  -1       Not available.
    * \retval  other    The address family value.
    *
    * An example is given below.
    *
    * \code
    * int af = VMCISock_GetAFValue();
    * if (af != -1) {
    *    int fd = socket(af, SOCK_STREAM, 0);
    *    ...
    *    close(fd);
    * }
    * \endcode
    *
    ***********************************************************************
    */

   static inline int VMCISock_GetAFValue(void)
   {
      return VMCISock_GetAFValueFd(NULL);
   }
   /** \endcond PRIVATE */

   /*
    ***********************************************************************
    * VMCISock_ReleaseAFValueFd                                      */ /**
    *
    * \brief Release the file descriptor obtained when retrieving the
    *        address family value.
    *
    * Use this to release the file descriptor obtained by calling
    * VMCISock_GetAFValueFd().
    *
    * \see VMCISock_GetAFValueFd()
    *
    * \param[in]  fd    File descriptor to the VMCI device.
    *
    ***********************************************************************
    */

   static inline void VMCISock_ReleaseAFValueFd(int fd)
   {
      if (fd >= 0) {
         close(fd);
      }
   }

   /*
    ***********************************************************************
    * VMCISock_GetLocalCID                                           */ /**
    *
    * \brief Retrieve the current context ID.
    *
    * \see VMADDR_CID_ANY
    *
    * \retval  VMADDR_CID_ANY    Not available.
    * \retval  other             The current context ID.
    *
    * An example is given below.
    *
    * \code
    * int vmciFd;
    * int af = VMCISock_GetAFValueFd(&vmciFd);
    * struct sockaddr_vm addr;
    * addr.svm_family = af;
    * addr.svm_cid = VMCISock_GetLocalCID();
    * VMCISock_ReleaseAFValueFd(vmciFd);
    * \endcode
    *
    ***********************************************************************
    */

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

      if (ioctl(fd, VMCI_SOCKETS_GET_LOCAL_CID, &contextId) < 0) {
         contextId = VMADDR_CID_ANY;
      }

      close(fd);
      return contextId;
   }
#  endif // __KERNEL__
#endif // linux && !VMKERNEL
#endif // _WIN32


#endif // _VMCI_SOCKETS_H_

