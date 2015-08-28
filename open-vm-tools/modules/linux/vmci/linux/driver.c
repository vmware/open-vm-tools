/*********************************************************
 * Copyright (C) 2011-2014 VMware, Inc. All rights reserved.
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

/* Must come before any kernel header file */
#include "driver-config.h"

#define EXPORT_SYMTAB

#include <asm/atomic.h>
#include <asm/io.h>

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#if defined(__x86_64__) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
#   include <linux/ioctl32.h>
/* Use weak: not all kernels export sys_ioctl for use by modules */
asmlinkage __attribute__((weak)) long
sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
#endif
#include <linux/miscdevice.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/smp.h>

#include "compat_highmem.h"
#include "compat_interrupt.h"
#include "compat_ioport.h"
#include "compat_kernel.h"
#include "compat_mm.h"
#include "compat_module.h"
#include "compat_mutex.h"
#include "compat_page.h"
#include "compat_pci.h"
#include "compat_sched.h"
#include "compat_slab.h"
#include "compat_uaccess.h"
#include "compat_version.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
#  error "Linux kernels before 2.6.9 are not supported."
#endif

#include "vm_basic_types.h"
#include "vm_device_version.h"

#include "vmware.h"
#include "driverLog.h"
#include "pgtbl.h"
#include "vmci_defs.h"
#include "vmci_handle_array.h"
#include "vmci_infrastructure.h"
#include "vmci_iocontrols.h"
#include "vmci_version.h"
#include "vmci_kernel_if.h"
#include "vmciCommonInt.h"
#include "vmciContext.h"
#include "vmciDatagram.h"
#include "vmciDoorbell.h"
#include "vmciDriver.h"
#include "vmciEvent.h"
#include "vmciKernelAPI.h"
#include "vmciQueuePair.h"
#include "vmciResource.h"

#define LGPFX "VMCI: "

#define VMCI_DEVICE_NAME   "vmci"
#define VMCI_MODULE_NAME   "vmci"


/*
 *----------------------------------------------------------------------
 *
 * PCI Device interface --
 *
 *      Declarations of types and functions related to the VMCI PCI
 *      device personality.
 *
 *
 *----------------------------------------------------------------------
 */

/*
 * VMCI PCI driver state
 */

typedef struct vmci_device {
   compat_mutex_t    lock;

   unsigned int      ioaddr;
   unsigned int      ioaddr_size;
   unsigned int      irq;
   unsigned int      intr_type;
   Bool              exclusive_vectors;
   struct msix_entry msix_entries[VMCI_MAX_INTRS];

   Bool              enabled;
   spinlock_t        dev_spinlock;
   atomic_t          datagrams_allowed;
} vmci_device;

static const struct pci_device_id vmci_ids[] = {
   { PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_VMCI), },
   { 0 },
};

static int vmci_probe_device(struct pci_dev *pdev,
                             const struct pci_device_id *id);
static void vmci_remove_device(struct pci_dev* pdev);

static struct pci_driver vmci_driver = {
   .name     = VMCI_DEVICE_NAME,
   .id_table = vmci_ids,
   .probe = vmci_probe_device,
   .remove = vmci_remove_device,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static compat_irqreturn_t vmci_interrupt(int irq, void *dev_id,
                                           struct pt_regs * regs);
static compat_irqreturn_t vmci_interrupt_bm(int irq, void *dev_id,
                                            struct pt_regs * regs);
#else
static compat_irqreturn_t vmci_interrupt(int irq, void *dev_id);
static compat_irqreturn_t vmci_interrupt_bm(int irq, void *dev_id);
#endif
static void dispatch_datagrams(unsigned long data);
static void process_bitmap(unsigned long data);

/* MSI-X has performance problems in < 2.6.19 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#  define VMCI_DISABLE_MSIX   0
#else
#  define VMCI_DISABLE_MSIX   1
#endif

/*
 * Needed by other components of this module.  It's okay to have one global
 * instance of this because there can only ever be one VMCI device.  Our
 * virtual hardware enforces this.
 */

struct pci_dev *vmci_pdev;

static vmci_device vmci_dev;
static compat_mod_param_bool vmci_disable_host = 0;
static compat_mod_param_bool vmci_disable_guest = 0;
static compat_mod_param_bool vmci_disable_msi;
static compat_mod_param_bool vmci_disable_msix = VMCI_DISABLE_MSIX;

DECLARE_TASKLET(vmci_dg_tasklet, dispatch_datagrams,
                (unsigned long)&vmci_dev);

DECLARE_TASKLET(vmci_bm_tasklet, process_bitmap,
                (unsigned long)&vmci_dev);

/*
 * Allocate a buffer for incoming datagrams globally to avoid repeated
 * allocation in the interrupt handler's atomic context.
 */

static uint8 *data_buffer = NULL;
static uint32 data_buffer_size = VMCI_MAX_DG_SIZE;

/*
 * If the VMCI hardware supports the notification bitmap, we allocate
 * and register a page with the device.
 */

static uint8 *notification_bitmap;
static dma_addr_t notification_base;


/*
 *----------------------------------------------------------------------
 *
 * Host device node interface --
 *
 *      Implements VMCI by implementing open/close/ioctl functions
 *
 *
 *----------------------------------------------------------------------
 */

/*
 * Per-instance host state
 */

typedef struct VMCILinux {
   VMCIContext *context;
   int userVersion;
   VMCIObjType ctType;
#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
   compat_mutex_t lock;
#endif
} VMCILinux;

/*
 * Static driver state.
 */

#define VM_DEVICE_NAME_SIZE 32
#define LINUXLOG_BUFFER_SIZE  1024

typedef struct VMCILinuxState {
   struct miscdevice misc;
   char buf[LINUXLOG_BUFFER_SIZE];
   atomic_t activeContexts;
} VMCILinuxState;

static int VMCISetupNotify(VMCIContext *context, VA notifyUVA);
static void VMCIUnsetNotifyInt(VMCIContext *context, Bool useLock);

static int LinuxDriver_Open(struct inode *inode, struct file *filp);

static int LinuxDriver_Ioctl(struct inode *inode, struct file *filp,
                             u_int iocmd, unsigned long ioarg);

#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
static long LinuxDriver_UnlockedIoctl(struct file *filp,
                                      u_int iocmd, unsigned long ioarg);
#endif

static int LinuxDriver_Close(struct inode *inode, struct file *filp);
static unsigned int LinuxDriverPoll(struct file *file, poll_table *wait);


#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
#define LinuxDriverLockIoctlPerFD(mutex) compat_mutex_lock(mutex)
#define LinuxDriverUnlockIoctlPerFD(mutex) compat_mutex_unlock(mutex)
#else
#define LinuxDriverLockIoctlPerFD(mutex) do {} while (0)
#define LinuxDriverUnlockIoctlPerFD(mutex) do {} while (0)
#endif

/* should be const if not for older kernels support */
static struct file_operations vmuser_fops = {
   .owner            = THIS_MODULE,
   .open             = LinuxDriver_Open,
   .release          = LinuxDriver_Close,
   .poll             = LinuxDriverPoll,
#ifdef HAVE_UNLOCKED_IOCTL
   .unlocked_ioctl   = LinuxDriver_UnlockedIoctl,
#else
   .ioctl            = LinuxDriver_Ioctl,
#endif
#ifdef HAVE_COMPAT_IOCTL
   .compat_ioctl     = LinuxDriver_UnlockedIoctl,
#endif
};

static struct VMCILinuxState linuxState = {
   .misc             = {
      .name          = VMCI_DEVICE_NAME,
      .minor         = MISC_DYNAMIC_MINOR,
      .fops          = &vmuser_fops,
   },
   .activeContexts   = ATOMIC_INIT(0),
};


/*
 *----------------------------------------------------------------------
 *
 * Shared VMCI device definitions --
 *
 *      Types and variables shared by both host and guest personality
 *
 *
 *----------------------------------------------------------------------
 */

static Bool guestDeviceInit;
static atomic_t guestDeviceActive;
static Bool hostDeviceInit;

/*
 *-----------------------------------------------------------------------------
 *
 * Host device support --
 *
 *      The following functions implement the support for the VMCI
 *      host driver.
 *
 *
 *-----------------------------------------------------------------------------
 */


#ifdef VM_X86_64
#ifndef HAVE_COMPAT_IOCTL
static int
LinuxDriver_Ioctl32_Handler(unsigned int fd, unsigned int iocmd,
                            unsigned long ioarg, struct file * filp)
{
   int ret;
   ret = -ENOTTY;
   if (filp && filp->f_op && filp->f_op->ioctl == LinuxDriver_Ioctl) {
      ret = LinuxDriver_Ioctl(filp->f_dentry->d_inode, filp, iocmd, ioarg);
   }
   return ret;
}
#endif /* !HAVE_COMPAT_IOCTL */

static int
register_ioctl32_handlers(void)
{
#ifndef HAVE_COMPAT_IOCTL
   {
      int i;

      for (i = IOCTL_VMCI_FIRST; i < IOCTL_VMCI_LAST; i++) {
         int retval = register_ioctl32_conversion(i,
                                                  LinuxDriver_Ioctl32_Handler);

         if (retval) {
            Warning(LGPFX"Failed to register ioctl32 conversion "
                    "(cmd=%d,err=%d).\n", i, retval);
            return retval;
         }
      }

      for (i = IOCTL_VMCI_FIRST2; i < IOCTL_VMCI_LAST2; i++) {
         int retval = register_ioctl32_conversion(i,
                                                  LinuxDriver_Ioctl32_Handler);

         if (retval) {
            Warning(LGPFX"Failed to register ioctl32 conversion "
                    "(cmd=%d,err=%d).\n", i, retval);
            return retval;
         }
      }
   }
#endif /* !HAVE_COMPAT_IOCTL */
   return 0;
}

static void
unregister_ioctl32_handlers(void)
{
#ifndef HAVE_COMPAT_IOCTL
   {
      int i;

      for (i = IOCTL_VMCI_FIRST; i < IOCTL_VMCI_LAST; i++) {
         int retval = unregister_ioctl32_conversion(i);

         if (retval) {
            Warning(LGPFX"Failed to unregister ioctl32 conversion "
                    "(cmd=%d,err=%d).\n", i, retval);
         }
      }

      for (i = IOCTL_VMCI_FIRST2; i < IOCTL_VMCI_LAST2; i++) {
         int retval = unregister_ioctl32_conversion(i);

         if (retval) {
            Warning(LGPFX"Failed to unregister ioctl32 conversion "
                    "(cmd=%d,err=%d).\n", i, retval);
         }
      }
   }
#endif /* !HAVE_COMPAT_IOCTL */
}
#else /* VM_X86_64 */
#define register_ioctl32_handlers() (0)
#define unregister_ioctl32_handlers() do { } while (0)
#endif /* VM_X86_64 */


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_host_init --
 *
 *      Initializes the VMCI host device driver.
 *
 * Results:
 *      0 on success, other error codes on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_host_init(void)
{
   int error;

   if (VMCI_HostInit() < VMCI_SUCCESS) {
      return -ENOMEM;
   }

   error = misc_register(&linuxState.misc);
   if (error) {
      Warning(LGPFX "Module registration error "
              "(name=%s, major=%d, minor=%d, err=%d).\n",
              linuxState.misc.name, MISC_MAJOR, linuxState.misc.minor,
              error);
      goto err_host_cleanup;
   }

   error = register_ioctl32_handlers();
   if (error) {
      Warning(LGPFX "Failed to register ioctl32 handlers, err: %d\n", error);
      goto err_misc_unregister;
   }

   Log(LGPFX "Module registered (name=%s, major=%d, minor=%d).\n",
       linuxState.misc.name, MISC_MAJOR, linuxState.misc.minor);

   return 0;

err_misc_unregister:
   misc_deregister(&linuxState.misc);
err_host_cleanup:
   VMCI_HostCleanup();
   return error;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriver_Open  --
 *
 *     Called on open of /dev/vmci.
 *
 * Side effects:
 *     Increment use count used to determine eventual deallocation of
 *     the module
 *
 *----------------------------------------------------------------------
 */

static int
LinuxDriver_Open(struct inode *inode, // IN
                 struct file *filp)   // IN
{
   VMCILinux *vmciLinux;

   vmciLinux = kmalloc(sizeof *vmciLinux, GFP_KERNEL);
   if (vmciLinux == NULL) {
      return -ENOMEM;
   }
   memset(vmciLinux, 0, sizeof *vmciLinux);
   vmciLinux->ctType = VMCIOBJ_NOT_SET;
   vmciLinux->userVersion = 0;
#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
   compat_mutex_init(&vmciLinux->lock);
#endif

   filp->private_data = vmciLinux;

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriver_Close  --
 *
 *      Called on close of /dev/vmci, most often when the process
 *      exits.
 *
 *----------------------------------------------------------------------
 */

static int
LinuxDriver_Close(struct inode *inode, // IN
                  struct file *filp)   // IN
{
   VMCILinux *vmciLinux;

   vmciLinux = (VMCILinux *)filp->private_data;
   ASSERT(vmciLinux);

   if (vmciLinux->ctType == VMCIOBJ_CONTEXT) {
      ASSERT(vmciLinux->context);

      VMCIContext_ReleaseContext(vmciLinux->context);
      vmciLinux->context = NULL;

      /*
       * The number of active contexts is used to track whether any
       * VMX'en are using the host personality. It is incremented when
       * a context is created through the IOCTL_VMCI_INIT_CONTEXT
       * ioctl.
       */

      atomic_dec(&linuxState.activeContexts);
   }
   vmciLinux->ctType = VMCIOBJ_NOT_SET;

   kfree(vmciLinux);
   filp->private_data = NULL;
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * LinuxDriverPoll  --
 *
 *      This is used to wake up the VMX when a VMCI call arrives, or
 *      to wake up select() or poll() at the next clock tick.
 *
 *----------------------------------------------------------------------
 */

static unsigned int
LinuxDriverPoll(struct file *filp,
                poll_table *wait)
{
   VMCILockFlags flags;
   VMCILinux *vmciLinux = (VMCILinux *) filp->private_data;
   unsigned int mask = 0;

   if (vmciLinux->ctType == VMCIOBJ_CONTEXT) {
      ASSERT(vmciLinux->context != NULL);
      /*
       * Check for VMCI calls to this VM context.
       */

      if (wait != NULL) {
         poll_wait(filp, &vmciLinux->context->hostContext.waitQueue, wait);
      }

      VMCI_GrabLock(&vmciLinux->context->lock, &flags);
      if (vmciLinux->context->pendingDatagrams > 0 ||
          VMCIHandleArray_GetSize(vmciLinux->context->pendingDoorbellArray) > 0) {
         mask = POLLIN;
      }
      VMCI_ReleaseLock(&vmciLinux->context->lock, flags);
   }
   return mask;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCICopyHandleArrayToUser  --
 *
 *      Copies the handles of a handle array into a user buffer, and
 *      returns the new length in userBufferSize. If the copy to the
 *      user buffer fails, the functions still returns VMCI_SUCCESS,
 *      but retval != 0.
 *
 *----------------------------------------------------------------------
 */

static int
VMCICopyHandleArrayToUser(void *userBufUVA,             // IN
                          uint64 *userBufSize,          // IN/OUT
                          VMCIHandleArray *handleArray, // IN
                          int *retval)                  // IN
{
   uint32 arraySize;
   VMCIHandle *handles;

   if (handleArray) {
      arraySize = VMCIHandleArray_GetSize(handleArray);
   } else {
      arraySize = 0;
   }

   if (arraySize * sizeof *handles > *userBufSize) {
      return VMCI_ERROR_MORE_DATA;
   }

   *userBufSize = arraySize * sizeof *handles;
   if (*userBufSize) {
      *retval = copy_to_user(userBufUVA,
                             VMCIHandleArray_GetHandles(handleArray),
                             *userBufSize);
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDoQPBrokerAlloc --
 *
 *      Helper function for creating queue pair and copying the result
 *      to user memory.
 *
 * Results:
 *      0 if result value was copied to user memory, -EFAULT otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIDoQPBrokerAlloc(VMCIHandle handle,
                    VMCIId peer,
                    uint32 flags,
                    uint64 produceSize,
                    uint64 consumeSize,
                    QueuePairPageStore *pageStore,
                    VMCIContext *context,
                    Bool vmToVm,
                    void *resultUVA)
{
   VMCIId cid;
   int result;
   int retval;

   cid = VMCIContext_GetId(context);

   result = VMCIQPBroker_Alloc(handle, peer, flags, VMCI_NO_PRIVILEGE_FLAGS,
                               produceSize, consumeSize, pageStore, context);
   if (result == VMCI_SUCCESS && vmToVm) {
      result = VMCI_SUCCESS_QUEUEPAIR_CREATE;
   }
   retval = copy_to_user(resultUVA, &result, sizeof result);
   if (retval) {
      retval = -EFAULT;
      if (result >= VMCI_SUCCESS) {
         result = VMCIQPBroker_Detach(handle, context);
         ASSERT(result >= VMCI_SUCCESS);
      }
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * LinuxDriver_Ioctl --
 *
 *      Main path for UserRPC
 *
 * Results:
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
LinuxDriver_Ioctl(struct inode *inode,
                  struct file *filp,
                  u_int iocmd,
                  unsigned long ioarg)
{
   VMCILinux *vmciLinux = (VMCILinux *) filp->private_data;
   int retval = 0;

   switch (iocmd) {
   case IOCTL_VMCI_VERSION2: {
      int verFromUser;

      if (copy_from_user(&verFromUser, (void *)ioarg, sizeof verFromUser)) {
         retval = -EFAULT;
         break;
      }

      vmciLinux->userVersion = verFromUser;
   }
      /* Fall through. */
   case IOCTL_VMCI_VERSION:
      /*
       * The basic logic here is:
       *
       * If the user sends in a version of 0 tell it our version.
       * If the user didn't send in a version, tell it our version.
       * If the user sent in an old version, tell it -its- version.
       * If the user sent in an newer version, tell it our version.
       *
       * The rationale behind telling the caller its version is that
       * Workstation 6.5 required that VMX and VMCI kernel module were
       * version sync'd.  All new VMX users will be programmed to
       * handle the VMCI kernel module version.
       */

      if (vmciLinux->userVersion > 0 &&
          vmciLinux->userVersion < VMCI_VERSION_HOSTQP) {
         retval = vmciLinux->userVersion;
      } else {
         retval = VMCI_VERSION;
      }
      break;

   case IOCTL_VMCI_INIT_CONTEXT: {
      VMCIInitBlock initBlock;
      VMCIHostUser user;

      retval = copy_from_user(&initBlock, (void *)ioarg, sizeof initBlock);
      if (retval != 0) {
         Log(LGPFX"Error reading init block.\n");
         retval = -EFAULT;
         break;
      }

      LinuxDriverLockIoctlPerFD(&vmciLinux->lock);
      if (vmciLinux->ctType != VMCIOBJ_NOT_SET) {
         Log(LGPFX"Received VMCI init on initialized handle.\n");
         retval = -EINVAL;
         goto init_release;
      }

      if (initBlock.flags & ~VMCI_PRIVILEGE_FLAG_RESTRICTED) {
         Log(LGPFX"Unsupported VMCI restriction flag.\n");
         retval = -EINVAL;
         goto init_release;
      }

      user = current_uid();
      retval = VMCIContext_InitContext(initBlock.cid, initBlock.flags,
                                       0 /* Unused */, vmciLinux->userVersion,
                                       &user, &vmciLinux->context);
      if (retval < VMCI_SUCCESS) {
         Log(LGPFX"Error initializing context.\n");
         retval = retval == VMCI_ERROR_DUPLICATE_ENTRY ? -EEXIST : -EINVAL;
         goto init_release;
      }

      /*
       * Copy cid to userlevel, we do this to allow the VMX to enforce its
       * policy on cid generation.
       */
      initBlock.cid = VMCIContext_GetId(vmciLinux->context);
      retval = copy_to_user((void *)ioarg, &initBlock, sizeof initBlock);
      if (retval != 0) {
         VMCIContext_ReleaseContext(vmciLinux->context);
         vmciLinux->context = NULL;
         Log(LGPFX"Error writing init block.\n");
         retval = -EFAULT;
         goto init_release;
      }
      ASSERT(initBlock.cid != VMCI_INVALID_ID);

      vmciLinux->ctType = VMCIOBJ_CONTEXT;

      atomic_inc(&linuxState.activeContexts);

     init_release:
      LinuxDriverUnlockIoctlPerFD(&vmciLinux->lock);
      break;
   }

   case IOCTL_VMCI_DATAGRAM_SEND: {
      VMCIDatagramSendRecvInfo sendInfo;
      VMCIDatagram *dg = NULL;
      VMCIId cid;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Warning(LGPFX"Ioctl only valid for context handle (iocmd=%d).\n", iocmd);
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&sendInfo, (void *) ioarg, sizeof sendInfo);
      if (retval) {
         Warning(LGPFX"copy_from_user failed.\n");
         retval = -EFAULT;
         break;
      }

      if (sendInfo.len > VMCI_MAX_DG_SIZE) {
         Warning(LGPFX"Datagram too big (size=%d).\n", sendInfo.len);
         retval = -EINVAL;
         break;
      }

      if (sendInfo.len < sizeof *dg) {
         Warning(LGPFX"Datagram too small (size=%d).\n", sendInfo.len);
         retval = -EINVAL;
         break;
      }

      dg = VMCI_AllocKernelMem(sendInfo.len, VMCI_MEMORY_NORMAL);
      if (dg == NULL) {
         Log(LGPFX"Cannot allocate memory to dispatch datagram.\n");
         retval = -ENOMEM;
         break;
      }

      retval = copy_from_user(dg, (char *)(VA)sendInfo.addr, sendInfo.len);
      if (retval != 0) {
         Log(LGPFX"Error getting datagram (err=%d).\n", retval);
         VMCI_FreeKernelMem(dg, sendInfo.len);
         retval = -EFAULT;
         break;
      }

      VMCI_DEBUG_LOG(10, (LGPFX"Datagram dst (handle=0x%x:0x%x) src "
                          "(handle=0x%x:0x%x), payload (size=%"FMT64"u "
                          "bytes).\n", dg->dst.context, dg->dst.resource,
                          dg->src.context, dg->src.resource,
                          dg->payloadSize));

      /* Get source context id. */
      ASSERT(vmciLinux->context);
      cid = VMCIContext_GetId(vmciLinux->context);
      ASSERT(cid != VMCI_INVALID_ID);
      sendInfo.result = VMCIDatagram_Dispatch(cid, dg, TRUE);
      VMCI_FreeKernelMem(dg, sendInfo.len);
      retval = copy_to_user((void *)ioarg, &sendInfo, sizeof sendInfo);
      break;
   }

   case IOCTL_VMCI_DATAGRAM_RECEIVE: {
      VMCIDatagramSendRecvInfo recvInfo;
      VMCIDatagram *dg = NULL;
      size_t size;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Warning(LGPFX"Ioctl only valid for context handle (iocmd=%d).\n",
                 iocmd);
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&recvInfo, (void *) ioarg, sizeof recvInfo);
      if (retval) {
         Warning(LGPFX"copy_from_user failed.\n");
         retval = -EFAULT;
         break;
      }

      ASSERT(vmciLinux->ctType == VMCIOBJ_CONTEXT);

      size = recvInfo.len;
      ASSERT(vmciLinux->context);
      recvInfo.result = VMCIContext_DequeueDatagram(vmciLinux->context,
                                                    &size, &dg);

      if (recvInfo.result >= VMCI_SUCCESS) {
         ASSERT(dg);
         retval = copy_to_user((void *) ((uintptr_t) recvInfo.addr), dg,
                               VMCI_DG_SIZE(dg));
         VMCI_FreeKernelMem(dg, VMCI_DG_SIZE(dg));
         if (retval != 0) {
            break;
         }
      }
      retval = copy_to_user((void *)ioarg, &recvInfo, sizeof recvInfo);
      break;
   }

   case IOCTL_VMCI_QUEUEPAIR_ALLOC: {
      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_QUEUEPAIR_ALLOC only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      if (vmciLinux->userVersion < VMCI_VERSION_NOVMVM) {
         VMCIQueuePairAllocInfo_VMToVM queuePairAllocInfo;
         VMCIQueuePairAllocInfo_VMToVM *info = (VMCIQueuePairAllocInfo_VMToVM *)ioarg;

         retval = copy_from_user(&queuePairAllocInfo, (void *)ioarg,
                                 sizeof queuePairAllocInfo);
         if (retval) {
            retval = -EFAULT;
            break;
         }

         retval = VMCIDoQPBrokerAlloc(queuePairAllocInfo.handle,
                                      queuePairAllocInfo.peer,
                                      queuePairAllocInfo.flags,
                                      queuePairAllocInfo.produceSize,
                                      queuePairAllocInfo.consumeSize,
                                      NULL,
                                      vmciLinux->context,
                                      TRUE, // VM to VM style create
                                      &info->result);
      } else {
         VMCIQueuePairAllocInfo queuePairAllocInfo;
         VMCIQueuePairAllocInfo *info = (VMCIQueuePairAllocInfo *)ioarg;
         QueuePairPageStore pageStore;

         retval = copy_from_user(&queuePairAllocInfo, (void *)ioarg,
                                 sizeof queuePairAllocInfo);
         if (retval) {
            retval = -EFAULT;
            break;
         }

         pageStore.pages = queuePairAllocInfo.ppnVA;
         pageStore.len = queuePairAllocInfo.numPPNs;

         retval = VMCIDoQPBrokerAlloc(queuePairAllocInfo.handle,
                                      queuePairAllocInfo.peer,
                                      queuePairAllocInfo.flags,
                                      queuePairAllocInfo.produceSize,
                                      queuePairAllocInfo.consumeSize,
                                      &pageStore,
                                      vmciLinux->context,
                                      FALSE, // Not VM to VM style create
                                      &info->result);
      }
      break;
   }

   case IOCTL_VMCI_QUEUEPAIR_SETVA: {
      VMCIQueuePairSetVAInfo setVAInfo;
      VMCIQueuePairSetVAInfo *info = (VMCIQueuePairSetVAInfo *)ioarg;
      int32 result;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_QUEUEPAIR_SETVA only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      if (vmciLinux->userVersion < VMCI_VERSION_NOVMVM) {
         Log(LGPFX"IOCTL_VMCI_QUEUEPAIR_SETVA not supported for this VMX version.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&setVAInfo, (void *)ioarg, sizeof setVAInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      if (setVAInfo.va) {
         /*
          * VMX is passing down a new VA for the queue pair mapping.
          */

         result = VMCIQPBroker_Map(setVAInfo.handle, vmciLinux->context, setVAInfo.va);
      } else {
         /*
          * The queue pair is about to be unmapped by the VMX.
          */

         result = VMCIQPBroker_Unmap(setVAInfo.handle, vmciLinux->context, 0);
      }

      retval = copy_to_user(&info->result, &result, sizeof result);
      if (retval) {
         retval = -EFAULT;
      }

      break;
   }

   case IOCTL_VMCI_QUEUEPAIR_SETPAGEFILE: {
      VMCIQueuePairPageFileInfo pageFileInfo;
      VMCIQueuePairPageFileInfo *info = (VMCIQueuePairPageFileInfo *)ioarg;
      int32 result;

      if (vmciLinux->userVersion < VMCI_VERSION_HOSTQP ||
          vmciLinux->userVersion >= VMCI_VERSION_NOVMVM) {
         Log(LGPFX"IOCTL_VMCI_QUEUEPAIR_SETPAGEFILE not supported this VMX "
             "(version=%d).\n", vmciLinux->userVersion);
         retval = -EINVAL;
         break;
      }

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_QUEUEPAIR_SETPAGEFILE only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&pageFileInfo, (void *)ioarg, sizeof *info);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      /*
       * Communicate success pre-emptively to the caller.  Note that
       * the basic premise is that it is incumbent upon the caller not
       * to look at the info.result field until after the ioctl()
       * returns.  And then, only if the ioctl() result indicates no
       * error.  We send up the SUCCESS status before calling
       * SetPageStore() store because failing to copy up the result
       * code means unwinding the SetPageStore().
       *
       * It turns out the logic to unwind a SetPageStore() opens a can
       * of worms.  For example, if a host had created the QueuePair
       * and a guest attaches and SetPageStore() is successful but
       * writing success fails, then ... the host has to be stopped
       * from writing (anymore) data into the QueuePair.  That means
       * an additional test in the VMCI_Enqueue() code path.  Ugh.
       */

      result = VMCI_SUCCESS;
      retval = copy_to_user(&info->result, &result, sizeof result);
      if (retval == 0) {
         result = VMCIQPBroker_SetPageStore(pageFileInfo.handle,
                                            pageFileInfo.produceVA,
                                            pageFileInfo.consumeVA,
                                            vmciLinux->context);
         if (result < VMCI_SUCCESS) {

            retval = copy_to_user(&info->result, &result, sizeof result);
            if (retval != 0) {
               /*
                * Note that in this case the SetPageStore() call
                * failed but we were unable to communicate that to the
                * caller (because the copy_to_user() call failed).
                * So, if we simply return an error (in this case
                * -EFAULT) then the caller will know that the
                * SetPageStore failed even though we couldn't put the
                * result code in the result field and indicate exactly
                * why it failed.
                *
                * That says nothing about the issue where we were once
                * able to write to the caller's info memory and now
                * can't.  Something more serious is probably going on
                * than the fact that SetPageStore() didn't work.
                */
               retval = -EFAULT;
            }
         }

      } else {
         /*
          * In this case, we can't write a result field of the
          * caller's info block.  So, we don't even try to
          * SetPageStore().
          */
         retval = -EFAULT;
      }

      break;
   }

   case IOCTL_VMCI_QUEUEPAIR_DETACH: {
      VMCIQueuePairDetachInfo detachInfo;
      VMCIQueuePairDetachInfo *info = (VMCIQueuePairDetachInfo *)ioarg;
      int32 result;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_QUEUEPAIR_DETACH only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&detachInfo, (void *)ioarg, sizeof detachInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      result = VMCIQPBroker_Detach(detachInfo.handle, vmciLinux->context);
      if (result == VMCI_SUCCESS &&
          vmciLinux->userVersion < VMCI_VERSION_NOVMVM) {
         result = VMCI_SUCCESS_LAST_DETACH;
      }

      retval = copy_to_user(&info->result, &result, sizeof result);
      if (retval) {
         retval = -EFAULT;
      }

      break;
   }

   case IOCTL_VMCI_CTX_ADD_NOTIFICATION: {
      VMCINotifyAddRemoveInfo arInfo;
      VMCINotifyAddRemoveInfo *info = (VMCINotifyAddRemoveInfo *)ioarg;
      int32 result;
      VMCIId cid;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_CTX_ADD_NOTIFICATION only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&arInfo, (void *)ioarg, sizeof arInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      cid = VMCIContext_GetId(vmciLinux->context);
      result = VMCIContext_AddNotification(cid, arInfo.remoteCID);
      retval = copy_to_user(&info->result, &result, sizeof result);
      if (retval) {
         retval = -EFAULT;
         break;
      }
      break;
   }

   case IOCTL_VMCI_CTX_REMOVE_NOTIFICATION: {
      VMCINotifyAddRemoveInfo arInfo;
      VMCINotifyAddRemoveInfo *info = (VMCINotifyAddRemoveInfo *)ioarg;
      int32 result;
      VMCIId cid;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_CTX_REMOVE_NOTIFICATION only valid for "
             "contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&arInfo, (void *)ioarg, sizeof arInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      cid = VMCIContext_GetId(vmciLinux->context);
      result = VMCIContext_RemoveNotification(cid, arInfo.remoteCID);
      retval = copy_to_user(&info->result, &result, sizeof result);
      if (retval) {
         retval = -EFAULT;
         break;
      }
      break;
   }

   case IOCTL_VMCI_CTX_GET_CPT_STATE: {
      VMCICptBufInfo getInfo;
      VMCIId cid;
      char *cptBuf;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_CTX_GET_CPT_STATE only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&getInfo, (void *)ioarg, sizeof getInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      cid = VMCIContext_GetId(vmciLinux->context);
      getInfo.result = VMCIContext_GetCheckpointState(cid, getInfo.cptType,
                                                      &getInfo.bufSize,
                                                      &cptBuf);
      if (getInfo.result == VMCI_SUCCESS && getInfo.bufSize) {
         retval = copy_to_user((void *)(VA)getInfo.cptBuf, cptBuf,
                               getInfo.bufSize);
         VMCI_FreeKernelMem(cptBuf, getInfo.bufSize);
         if (retval) {
            retval = -EFAULT;
            break;
         }
      }
      retval = copy_to_user((void *)ioarg, &getInfo, sizeof getInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }
      break;
   }

   case IOCTL_VMCI_CTX_SET_CPT_STATE: {
      VMCICptBufInfo setInfo;
      VMCIId cid;
      char *cptBuf;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_CTX_SET_CPT_STATE only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&setInfo, (void *)ioarg, sizeof setInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      cptBuf = VMCI_AllocKernelMem(setInfo.bufSize, VMCI_MEMORY_NORMAL);
      if (cptBuf == NULL) {
         Log(LGPFX"Cannot allocate memory to set cpt state (type=%d).\n",
             setInfo.cptType);
         retval = -ENOMEM;
         break;
      }
      retval = copy_from_user(cptBuf, (void *)(VA)setInfo.cptBuf,
                              setInfo.bufSize);
      if (retval) {
         VMCI_FreeKernelMem(cptBuf, setInfo.bufSize);
         retval = -EFAULT;
         break;
      }

      cid = VMCIContext_GetId(vmciLinux->context);
      setInfo.result = VMCIContext_SetCheckpointState(cid, setInfo.cptType,
                                                      setInfo.bufSize, cptBuf);
      VMCI_FreeKernelMem(cptBuf, setInfo.bufSize);
      retval = copy_to_user((void *)ioarg, &setInfo, sizeof setInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }
      break;
   }

   case IOCTL_VMCI_GET_CONTEXT_ID: {
      VMCIId cid = VMCI_HOST_CONTEXT_ID;

      retval = copy_to_user((void *)ioarg, &cid, sizeof cid);
      break;
   }

   case IOCTL_VMCI_SET_NOTIFY: {
      VMCISetNotifyInfo notifyInfo;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_SET_NOTIFY only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&notifyInfo, (void *)ioarg, sizeof notifyInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      if ((VA)notifyInfo.notifyUVA != (VA)NULL) {
         notifyInfo.result = VMCISetupNotify(vmciLinux->context,
                                             (VA)notifyInfo.notifyUVA);
      } else {
         VMCIUnsetNotifyInt(vmciLinux->context, TRUE);
         notifyInfo.result = VMCI_SUCCESS;
      }

      retval = copy_to_user((void *)ioarg, &notifyInfo, sizeof notifyInfo);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      break;
   }

   case IOCTL_VMCI_NOTIFY_RESOURCE: {
      VMCINotifyResourceInfo info;
      VMCIId cid;

      if (vmciLinux->userVersion < VMCI_VERSION_NOTIFY) {
         Log(LGPFX"IOCTL_VMCI_NOTIFY_RESOURCE is invalid for current"
             " VMX versions.\n");
         retval = -EINVAL;
         break;
      }

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_NOTIFY_RESOURCE is only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&info, (void *)ioarg, sizeof info);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      cid = VMCIContext_GetId(vmciLinux->context);
      switch (info.action) {
      case VMCI_NOTIFY_RESOURCE_ACTION_NOTIFY:
         if (info.resource == VMCI_NOTIFY_RESOURCE_DOOR_BELL) {
            info.result = VMCIContext_NotifyDoorbell(cid, info.handle,
                                                     VMCI_NO_PRIVILEGE_FLAGS);
         } else {
            info.result = VMCI_ERROR_UNAVAILABLE;
         }
         break;
      case VMCI_NOTIFY_RESOURCE_ACTION_CREATE:
         info.result = VMCIContext_DoorbellCreate(cid, info.handle);
         break;
      case VMCI_NOTIFY_RESOURCE_ACTION_DESTROY:
         info.result = VMCIContext_DoorbellDestroy(cid, info.handle);
         break;
      default:
         Log(LGPFX"IOCTL_VMCI_NOTIFY_RESOURCE got unknown action (action=%d).\n",
             info.action);
         info.result = VMCI_ERROR_INVALID_ARGS;
      }
      retval = copy_to_user((void *)ioarg, &info,
                            sizeof info);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      break;
   }

   case IOCTL_VMCI_NOTIFICATIONS_RECEIVE: {
      VMCINotificationReceiveInfo info;
      VMCIHandleArray *dbHandleArray;
      VMCIHandleArray *qpHandleArray;
      VMCIId cid;

      if (vmciLinux->ctType != VMCIOBJ_CONTEXT) {
         Log(LGPFX"IOCTL_VMCI_NOTIFICATIONS_RECEIVE is only valid for contexts.\n");
         retval = -EINVAL;
         break;
      }

      if (vmciLinux->userVersion < VMCI_VERSION_NOTIFY) {
         Log(LGPFX"IOCTL_VMCI_NOTIFICATIONS_RECEIVE is not supported for the"
             " current vmx version.\n");
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&info, (void *)ioarg, sizeof info);
      if (retval) {
         retval = -EFAULT;
         break;
      }

      if ((info.dbHandleBufSize && !info.dbHandleBufUVA) ||
          (info.qpHandleBufSize && !info.qpHandleBufUVA)) {
         retval = -EINVAL;
         break;
      }

      cid = VMCIContext_GetId(vmciLinux->context);
      info.result = VMCIContext_ReceiveNotificationsGet(cid,
                                                        &dbHandleArray,
                                                        &qpHandleArray);
      if (info.result == VMCI_SUCCESS) {
         info.result =
            VMCICopyHandleArrayToUser((void *)(VA)info.dbHandleBufUVA,
                                      &info.dbHandleBufSize,
                                      dbHandleArray,
                                      &retval);
         if (info.result == VMCI_SUCCESS && !retval) {
            info.result =
               VMCICopyHandleArrayToUser((void *)(VA)info.qpHandleBufUVA,
                                         &info.qpHandleBufSize,
                                         qpHandleArray,
                                         &retval);
         }
         if (!retval) {
            retval = copy_to_user((void *)ioarg, &info, sizeof info);
         }
         VMCIContext_ReceiveNotificationsRelease(cid, dbHandleArray, qpHandleArray,
                                                 info.result == VMCI_SUCCESS && !retval);
      } else {
         retval = copy_to_user((void *)ioarg, &info, sizeof info);
      }
      break;
   }

   default:
      Warning(LGPFX"Unknown ioctl (iocmd=%d).\n", iocmd);
      retval = -EINVAL;
   }

   return retval;
}


#if defined(HAVE_COMPAT_IOCTL) || defined(HAVE_UNLOCKED_IOCTL)
/*
 *-----------------------------------------------------------------------------
 *
 * LinuxDriver_UnlockedIoctl --
 *
 *      Wrapper for LinuxDriver_Ioctl supporting the compat_ioctl and
 *      unlocked_ioctl methods that have signatures different from the
 *      old ioctl. Used as compat_ioctl method for 32bit apps running
 *      on 64bit kernel and for unlocked_ioctl on systems supporting
 *      those.  LinuxDriver_Ioctl may safely be called without holding
 *      the BKL.
 *
 * Results:
 *      Same as LinuxDriver_Ioctl.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static long
LinuxDriver_UnlockedIoctl(struct file *filp,
                          u_int iocmd,
                          unsigned long ioarg)
{
   return LinuxDriver_Ioctl(NULL, filp, iocmd, ioarg);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUserVAInvalidPointer --
 *
 *      Checks if a given user VA is valid or not.  Copied from
 *      bora/modules/vmnet/linux/hostif.c:VNetUserIfInvalidPointer().  TODO
 *      libify the common code.
 *
 * Results:
 *      TRUE iff invalid.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
VMCIUserVAInvalidPointer(VA uva,      // IN:
                         size_t size) // IN:
{
   return !access_ok(VERIFY_WRITE, (void *)uva, size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUserVALockPage --
 *
 *      Lock physical page backing a given user VA.  Copied from
 *      bora/modules/vmnet/linux/userif.c:UserIfLockPage().  TODO libify the
 *      common code.
 *
 * Results:
 *      Pointer to struct page on success, NULL otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE struct page *
VMCIUserVALockPage(VA addr) // IN:
{
   struct page *page = NULL;
   int retval;

   down_read(&current->mm->mmap_sem);
   retval = get_user_pages(current, current->mm, addr,
                           1, 1, 0, &page, NULL);
   up_read(&current->mm->mmap_sem);

   if (retval != 1) {
      return NULL;
   }

   return page;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMapBoolPtr --
 *
 *      Lock physical page backing a given user VA and maps it to kernel
 *      address space.  The range of the mapped memory should be within a
 *      single page otherwise an error is returned.  Copied from
 *      bora/modules/vmnet/linux/userif.c:VNetUserIfMapUint32Ptr().  TODO
 *      libify the common code.
 *
 * Results:
 *      0 on success, negative error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VMCIMapBoolPtr(VA notifyUVA,     // IN:
               struct page **p,  // OUT:
               Bool **notifyPtr) // OUT:
{
   if (VMCIUserVAInvalidPointer(notifyUVA, sizeof **notifyPtr) ||
       (((notifyUVA + sizeof **notifyPtr - 1) & ~(PAGE_SIZE - 1)) !=
        (notifyUVA & ~(PAGE_SIZE - 1)))) {
      return -EINVAL;
   }

   *p = VMCIUserVALockPage(notifyUVA);
   if (*p == NULL) {
      return -EAGAIN;
   }

   *notifyPtr = (Bool *)((uint8 *)kmap(*p) + (notifyUVA & (PAGE_SIZE - 1)));
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCISetupNotify --
 *
 *      Sets up a given context for notify to work.  Calls VMCIMapBoolPtr()
 *      which maps the notify boolean in user VA in kernel space.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCISetupNotify(VMCIContext *context, // IN:
                VA notifyUVA)         // IN:
{
   int retval;

   if (context->notify) {
      Warning(LGPFX"Notify mechanism is already set up.\n");
      return VMCI_ERROR_DUPLICATE_ENTRY;
   }

   retval =
      VMCIMapBoolPtr(notifyUVA, &context->notifyPage, &context->notify) == 0 ?
         VMCI_SUCCESS : VMCI_ERROR_GENERIC;
   if (retval == VMCI_SUCCESS) {
      VMCIContext_CheckAndSignalNotify(context);
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUnsetNotifyInt --
 *
 *      Internal version of VMCIUnsetNotify, that allows for locking
 *      the context before unsetting the notify pointer. If useLock is
 *      TRUE, the context lock is grabbed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIUnsetNotifyInt(VMCIContext *context, // IN
                   Bool useLock)         // IN
{
   VMCILockFlags flags;

   if (useLock) {
      VMCI_GrabLock(&context->lock, &flags);
   }

   if (context->notifyPage) {
      struct page *notifyPage = context->notifyPage;

      context->notify = NULL;
      context->notifyPage = NULL;

      if (useLock) {
         VMCI_ReleaseLock(&context->lock, flags);
      }

      kunmap(notifyPage);
      put_page(notifyPage);
   } else {
      if (useLock) {
         VMCI_ReleaseLock(&context->lock, flags);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIUnsetNotify --
 *
 *      Reverts actions set up by VMCISetupNotify().  Unmaps and unlocks the
 *      page mapped/locked by VMCISetupNotify().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIUnsetNotify(VMCIContext *context) // IN:
{
   VMCIUnsetNotifyInt(context, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * PCI device support --
 *
 *      The following functions implement the support for the VMCI
 *      guest device. This includes initializing the device and
 *      interrupt handling.
 *
 *-----------------------------------------------------------------------------
 */


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_guest_init --
 *
 *      Initializes the VMCI PCI device. The initialization might fail
 *      if there is no VMCI PCI device.
 *
 * Results:
 *      0 on success, other error codes on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_guest_init(void)
{
   int retval;

   /* Initialize guest device data. */
   compat_mutex_init(&vmci_dev.lock);
   vmci_dev.intr_type = VMCI_INTR_TYPE_INTX;
   vmci_dev.exclusive_vectors = FALSE;
   spin_lock_init(&vmci_dev.dev_spinlock);
   vmci_dev.enabled = FALSE;
   atomic_set(&vmci_dev.datagrams_allowed, 0);
   atomic_set(&guestDeviceActive, 0);

   data_buffer = vmalloc(data_buffer_size);
   if (!data_buffer) {
      return -ENOMEM;
   }

   /* This should be last to make sure we are done initializing. */
   retval = pci_register_driver(&vmci_driver);
   if (retval < 0) {
      vfree(data_buffer);
      data_buffer = NULL;
      return retval;
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_enable_msix --
 *
 *      Enable MSI-X.  Try exclusive vectors first, then shared vectors.
 *
 * Results:
 *      0 on success, other error codes on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_enable_msix(struct pci_dev *pdev) // IN
{
   int i;
   int result;

   for (i = 0; i < VMCI_MAX_INTRS; ++i) {
      vmci_dev.msix_entries[i].entry = i;
      vmci_dev.msix_entries[i].vector = i;
   }

   result = pci_enable_msix(pdev, vmci_dev.msix_entries, VMCI_MAX_INTRS);
   if (!result) {
      vmci_dev.exclusive_vectors = TRUE;
   } else if (result > 0) {
      result = pci_enable_msix(pdev, vmci_dev.msix_entries, 1);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_probe_device --
 *
 *      Most of the initialization at module load time is done here.
 *
 * Results:
 *      Returns 0 for success, an error otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_probe_device(struct pci_dev *pdev,           // IN: vmci PCI device
                  const struct pci_device_id *id) // IN: matching device ID
{
   unsigned int ioaddr;
   unsigned int ioaddr_size;
   unsigned int capabilities;
   int result;

   printk(KERN_INFO "Probing for vmci/PCI.\n");

   result = pci_enable_device(pdev);
   if (result) {
      printk(KERN_ERR "Cannot VMCI device %s: error %d\n",
             pci_name(pdev), result);
      return result;
   }
   pci_set_master(pdev); /* To enable QueuePair functionality. */
   ioaddr = pci_resource_start(pdev, 0);
   ioaddr_size = pci_resource_len(pdev, 0);

   /*
    * Request I/O region with adjusted base address and size. The adjusted
    * values are needed and used if we release the region in case of failure.
    */

   if (!compat_request_region(ioaddr, ioaddr_size, "vmci")) {
      printk(KERN_INFO "vmci: Another driver already loaded "
                       "for device in slot %s.\n", pci_name(pdev));
      goto pci_disable;
   }

   printk(KERN_INFO "Found vmci/PCI at %#x, irq %u.\n", ioaddr, pdev->irq);

   /*
    * Verify that the VMCI Device supports the capabilities that
    * we need. If the device is missing capabilities that we would
    * like to use, check for fallback capabilities and use those
    * instead (so we can run a new VM on old hosts). Fail the load if
    * a required capability is missing and there is no fallback.
    *
    * Right now, we need datagrams. There are no fallbacks.
    */
   capabilities = inl(ioaddr + VMCI_CAPS_ADDR);

   if ((capabilities & VMCI_CAPS_DATAGRAM) == 0) {
      printk(KERN_ERR "VMCI device does not support datagrams.\n");
      goto release;
   }

   /*
    * If the hardware supports notifications, we will use that as
    * well.
    */
   if (capabilities & VMCI_CAPS_NOTIFICATIONS) {
      capabilities = VMCI_CAPS_DATAGRAM;
      notification_bitmap = dma_alloc_coherent(&pdev->dev, PAGE_SIZE,
                                               &notification_base,
                                               GFP_KERNEL);
      if (notification_bitmap == NULL) {
         printk(KERN_ERR "VMCI device unable to allocate notification bitmap.\n");
      } else {
         memset(notification_bitmap, 0, PAGE_SIZE);
         capabilities |= VMCI_CAPS_NOTIFICATIONS;
      }
   } else {
      capabilities = VMCI_CAPS_DATAGRAM;
   }
   printk(KERN_INFO "VMCI: using capabilities 0x%x.\n", capabilities);

   /* Let the host know which capabilities we intend to use. */
   outl(capabilities, ioaddr + VMCI_CAPS_ADDR);

   /* Device struct initialization. */
   compat_mutex_lock(&vmci_dev.lock);
   if (vmci_dev.enabled) {
      printk(KERN_ERR "VMCI device already enabled.\n");
      goto unlock;
   }

   vmci_dev.ioaddr = ioaddr;
   vmci_dev.ioaddr_size = ioaddr_size;
   atomic_set(&vmci_dev.datagrams_allowed, 1);

   /*
    * Register notification bitmap with device if that capability is
    * used
    */
   if (capabilities & VMCI_CAPS_NOTIFICATIONS) {
      unsigned long bitmapPPN = notification_base >> PAGE_SHIFT;
      if (!VMCI_RegisterNotificationBitmap(bitmapPPN)) {
         printk(KERN_ERR "VMCI device unable to register notification bitmap "
                "with PPN 0x%x.\n", (uint32)bitmapPPN);
         goto datagram_disallow;
      }
   }

   /* Check host capabilities. */
   if (!VMCI_CheckHostCapabilities()) {
      goto remove_bitmap;
   }

   /* Enable device. */
   vmci_dev.enabled = TRUE;
   pci_set_drvdata(pdev, &vmci_dev);
   vmci_pdev = pdev;

   /*
    * We do global initialization here because we need datagrams
    * during VMCIUtil_Init, since it registers for VMCI events. If we
    * ever support more than one VMCI device we will have to create
    * seperate LateInit/EarlyExit functions that can be used to do
    * initialization/cleanup that depends on the device being
    * accessible.  We need to initialize VMCI components before
    * requesting an irq - the VMCI interrupt handler uses these
    * components, and it may be invoked once request_irq() has
    * registered the handler (as the irq line may be shared).
    */
   VMCIUtil_Init();

   if (VMCIQPGuestEndpoints_Init() < VMCI_SUCCESS) {
      goto util_exit;
   }

   /*
    * Enable interrupts.  Try MSI-X first, then MSI, and then fallback on
    * legacy interrupts.
    */
   if (!vmci_disable_msix && !vmci_enable_msix(pdev)) {
      vmci_dev.intr_type = VMCI_INTR_TYPE_MSIX;
      vmci_dev.irq = vmci_dev.msix_entries[0].vector;
   } else if (!vmci_disable_msi && !pci_enable_msi(pdev)) {
      vmci_dev.intr_type = VMCI_INTR_TYPE_MSI;
      vmci_dev.irq = pdev->irq;
   } else {
      vmci_dev.intr_type = VMCI_INTR_TYPE_INTX;
      vmci_dev.irq = pdev->irq;
   }

   /* Request IRQ for legacy or MSI interrupts, or for first MSI-X vector. */
   result = request_irq(vmci_dev.irq, vmci_interrupt, COMPAT_IRQF_SHARED,
                        "vmci", &vmci_dev);
   if (result) {
      printk(KERN_ERR "vmci: irq %u in use: %d\n", vmci_dev.irq, result);
      goto components_exit;
   }

   /*
    * For MSI-X with exclusive vectors we need to request an interrupt for each
    * vector so that we get a separate interrupt handler routine.  This allows
    * us to distinguish between the vectors.
    */

   if (vmci_dev.exclusive_vectors) {
      ASSERT(vmci_dev.intr_type == VMCI_INTR_TYPE_MSIX);
      result = request_irq(vmci_dev.msix_entries[1].vector, vmci_interrupt_bm,
                           0, "vmci", &vmci_dev);
      if (result) {
         printk(KERN_ERR "vmci: irq %u in use: %d\n",
                vmci_dev.msix_entries[1].vector, result);
         free_irq(vmci_dev.irq, &vmci_dev);
         goto components_exit;
      }
   }

   printk(KERN_INFO "Registered vmci device.\n");

   atomic_inc(&guestDeviceActive);

   compat_mutex_unlock(&vmci_dev.lock);

   /* Enable specific interrupt bits. */
   if (capabilities & VMCI_CAPS_NOTIFICATIONS) {
      outl(VMCI_IMR_DATAGRAM | VMCI_IMR_NOTIFICATION,
           vmci_dev.ioaddr + VMCI_IMR_ADDR);
   } else {
      outl(VMCI_IMR_DATAGRAM, vmci_dev.ioaddr + VMCI_IMR_ADDR);
   }

   /* Enable interrupts. */
   outl(VMCI_CONTROL_INT_ENABLE, vmci_dev.ioaddr + VMCI_CONTROL_ADDR);

   return 0;

 components_exit:
   VMCIQPGuestEndpoints_Exit();
 util_exit:
   VMCIUtil_Exit();
   vmci_dev.enabled = FALSE;
   if (vmci_dev.intr_type == VMCI_INTR_TYPE_MSIX) {
      pci_disable_msix(pdev);
   } else if (vmci_dev.intr_type == VMCI_INTR_TYPE_MSI) {
      pci_disable_msi(pdev);
   }
 remove_bitmap:
   if (notification_bitmap) {
      outl(VMCI_CONTROL_RESET, vmci_dev.ioaddr + VMCI_CONTROL_ADDR);
   }
 datagram_disallow:
   atomic_set(&vmci_dev.datagrams_allowed, 0);
 unlock:
   compat_mutex_unlock(&vmci_dev.lock);
 release:
   if (notification_bitmap) {
      dma_free_coherent(&pdev->dev, PAGE_SIZE, notification_bitmap,
                        notification_base);
      notification_bitmap = NULL;
   }
   release_region(ioaddr, ioaddr_size);
 pci_disable:
   pci_disable_device(pdev);
   return -EBUSY;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_remove_device --
 *
 *      Cleanup, called for each device on unload.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
vmci_remove_device(struct pci_dev* pdev)
{
   struct vmci_device *dev = pci_get_drvdata(pdev);

   printk(KERN_INFO "Removing vmci device\n");

   atomic_dec(&guestDeviceActive);

   VMCIQPGuestEndpoints_Exit();
   VMCIUtil_Exit();
   vmci_pdev = NULL;

   compat_mutex_lock(&dev->lock);

   atomic_set(&vmci_dev.datagrams_allowed, 0);

   printk(KERN_INFO "Resetting vmci device\n");
   outl(VMCI_CONTROL_RESET, vmci_dev.ioaddr + VMCI_CONTROL_ADDR);

   /*
    * Free IRQ and then disable MSI/MSI-X as appropriate.  For MSI-X, we might
    * have multiple vectors, each with their own IRQ, which we must free too.
    */

   free_irq(dev->irq, dev);
   if (dev->intr_type == VMCI_INTR_TYPE_MSIX) {
      if (dev->exclusive_vectors) {
         free_irq(dev->msix_entries[1].vector, dev);
      }
      pci_disable_msix(pdev);
   } else if (dev->intr_type == VMCI_INTR_TYPE_MSI) {
      pci_disable_msi(pdev);
   }
   dev->exclusive_vectors = FALSE;
   dev->intr_type = VMCI_INTR_TYPE_INTX;

   release_region(dev->ioaddr, dev->ioaddr_size);
   dev->enabled = FALSE;
   if (notification_bitmap) {
      /*
       * The device reset above cleared the bitmap state of the
       * device, so we can safely free it here.
       */

      pci_free_consistent(pdev, PAGE_SIZE, notification_bitmap,
                          notification_base);
      notification_bitmap = NULL;
   }

   printk(KERN_INFO "Unregistered vmci device.\n");
   compat_mutex_unlock(&dev->lock);

   pci_disable_device(pdev);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_interrupt --
 *
 *      Interrupt handler for legacy or MSI interrupt, or for first MSI-X
 *      interrupt (vector VMCI_INTR_DATAGRAM).
 *
 * Results:
 *      COMPAT_IRQ_HANDLED if the interrupt is handled, COMPAT_IRQ_NONE if
 *      not an interrupt.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static compat_irqreturn_t
vmci_interrupt(int irq,               // IN
               void *clientdata,      // IN
               struct pt_regs *regs)  // IN
#else
static compat_irqreturn_t
vmci_interrupt(int irq,               // IN
               void *clientdata)      // IN
#endif
{
   vmci_device *dev = clientdata;

   if (dev == NULL) {
      printk(KERN_DEBUG "vmci_interrupt(): irq %d for unknown device.\n", irq);
      return COMPAT_IRQ_NONE;
   }

   /*
    * If we are using MSI-X with exclusive vectors then we simply schedule
    * the datagram tasklet, since we know the interrupt was meant for us.
    * Otherwise we must read the ICR to determine what to do.
    */

   if (dev->intr_type == VMCI_INTR_TYPE_MSIX && dev->exclusive_vectors) {
      tasklet_schedule(&vmci_dg_tasklet);
   } else {
      unsigned int icr;

      ASSERT(dev->intr_type == VMCI_INTR_TYPE_INTX ||
             dev->intr_type == VMCI_INTR_TYPE_MSI);

      /* Acknowledge interrupt and determine what needs doing. */
      icr = inl(dev->ioaddr + VMCI_ICR_ADDR);
      if (icr == 0 || icr == 0xffffffff) {
         return COMPAT_IRQ_NONE;
      }

      if (icr & VMCI_ICR_DATAGRAM) {
         tasklet_schedule(&vmci_dg_tasklet);
         icr &= ~VMCI_ICR_DATAGRAM;
      }
      if (icr & VMCI_ICR_NOTIFICATION) {
         tasklet_schedule(&vmci_bm_tasklet);
         icr &= ~VMCI_ICR_NOTIFICATION;
      }
      if (icr != 0) {
         printk(KERN_INFO LGPFX"Ignoring unknown interrupt cause (%d).\n", icr);
      }
   }

   return COMPAT_IRQ_HANDLED;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_interrupt_bm --
 *
 *      Interrupt handler for MSI-X interrupt vector VMCI_INTR_NOTIFICATION,
 *      which is for the notification bitmap.  Will only get called if we are
 *      using MSI-X with exclusive vectors.
 *
 * Results:
 *      COMPAT_IRQ_HANDLED if the interrupt is handled, COMPAT_IRQ_NONE if
 *      not an interrupt.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static compat_irqreturn_t
vmci_interrupt_bm(int irq,               // IN
                  void *clientdata,      // IN
                  struct pt_regs *regs)  // IN
#else
static compat_irqreturn_t
vmci_interrupt_bm(int irq,               // IN
                  void *clientdata)      // IN
#endif
{
   vmci_device *dev = clientdata;

   if (dev == NULL) {
      printk(KERN_DEBUG "vmci_interrupt_bm(): irq %d for unknown device.\n", irq);
      return COMPAT_IRQ_NONE;
   }

   /* For MSI-X we can just assume it was meant for us. */
   ASSERT(dev->intr_type == VMCI_INTR_TYPE_MSIX && dev->exclusive_vectors);
   tasklet_schedule(&vmci_bm_tasklet);

   return COMPAT_IRQ_HANDLED;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_DeviceEnabled --
 *
 *      Checks whether the VMCI device is enabled.
 *
 * Results:
 *      TRUE if device is enabled, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCI_DeviceEnabled(void)
{
   return VMCI_GuestPersonalityActive() || VMCI_HostPersonalityActive();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_SendDatagram --
 *
 *      VM to hypervisor call mechanism. We use the standard VMware naming
 *      convention since shared code is calling this function as well.
 *
 * Results:
 *      The result of the hypercall.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCI_SendDatagram(VMCIDatagram *dg)
{
   unsigned long flags;
   int result;

   /* Check args. */
   if (dg == NULL) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (atomic_read(&vmci_dev.datagrams_allowed) == 0) {
      return VMCI_ERROR_UNAVAILABLE;
   }

   /*
    * Need to acquire spinlock on the device because
    * the datagram data may be spread over multiple pages and the monitor may
    * interleave device user rpc calls from multiple VCPUs. Acquiring the
    * spinlock precludes that possibility. Disabling interrupts to avoid
    * incoming datagrams during a "rep out" and possibly landing up in this
    * function.
    */
   spin_lock_irqsave(&vmci_dev.dev_spinlock, flags);

   /*
    * Send the datagram and retrieve the return value from the result register.
    */
   __asm__ __volatile__(
      "cld\n\t"
      "rep outsb\n\t"
      : /* No output. */
      : "d"(vmci_dev.ioaddr + VMCI_DATA_OUT_ADDR),
        "c"(VMCI_DG_SIZE(dg)), "S"(dg)
      );

   /*
    * XXX Should read result high port as well when updating handlers to
    * return 64bit.
    */
   result = inl(vmci_dev.ioaddr + VMCI_RESULT_LOW_ADDR);
   spin_unlock_irqrestore(&vmci_dev.dev_spinlock, flags);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * dispatch_datagrams --
 *
 *      Reads and dispatches incoming datagrams.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Reads data from the device.
 *
 *-----------------------------------------------------------------------------
 */

void
dispatch_datagrams(unsigned long data)
{
   vmci_device *dev = (vmci_device *)data;

   if (dev == NULL) {
      printk(KERN_DEBUG "vmci: dispatch_datagrams(): no vmci device"
             "present.\n");
      return;
   }

   if (data_buffer == NULL) {
      printk(KERN_DEBUG "vmci: dispatch_datagrams(): no buffer present.\n");
      return;
   }


   VMCI_ReadDatagramsFromPort((VMCIIoHandle) 0, dev->ioaddr + VMCI_DATA_IN_ADDR,
                              data_buffer, data_buffer_size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * process_bitmap --
 *
 *      Scans the notification bitmap for raised flags, clears them
 *      and handles the notifications.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
process_bitmap(unsigned long data)
{
   vmci_device *dev = (vmci_device *)data;

   if (dev == NULL) {
      printk(KERN_DEBUG "vmci: process_bitmaps(): no vmci device"
             "present.\n");
      return;
   }

   if (notification_bitmap == NULL) {
      printk(KERN_DEBUG "vmci: process_bitmaps(): no bitmap present.\n");
      return;
   }


   VMCI_ScanNotificationBitmap(notification_bitmap);
}


/*
 *----------------------------------------------------------------------
 *
 * Shared functions --
 *
 *      Functions shared between host and guest personality.
 *
 *----------------------------------------------------------------------
 */


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_GuestPersonalityActive --
 *
 *      Determines whether the VMCI PCI device has been successfully
 *      initialized.
 *
 * Results:
 *      TRUE, if VMCI guest device is operational, FALSE otherwise.
 *
 * Side effects:
 *      Reads data from the device.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCI_GuestPersonalityActive(void)
{
   return guestDeviceInit && atomic_read(&guestDeviceActive) > 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_HostPersonalityActive --
 *
 *      Determines whether the VMCI host personality is
 *      available. Since the core functionality of the host driver is
 *      always present, all guests could possibly use the host
 *      personality. However, to minimize the deviation from the
 *      pre-unified driver state of affairs, we only consider the host
 *      device active, if there is no active guest device, or if there
 *      are VMX'en with active VMCI contexts using the host device.
 *
 * Results:
 *      TRUE, if VMCI host driver is operational, FALSE otherwise.
 *
 * Side effects:
 *      Reads data from the device.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCI_HostPersonalityActive(void)
{
   return hostDeviceInit &&
      (!VMCI_GuestPersonalityActive() ||
       atomic_read(&linuxState.activeContexts) > 0);
}


/*
 *----------------------------------------------------------------------
 *
 * Module definitions --
 *
 *      Implements support for module load/unload.
 *
 *----------------------------------------------------------------------
 */


/*
 *----------------------------------------------------------------------
 *
 * vmci_init --
 *
 *      linux module entry point. Called by /sbin/insmod command
 *
 * Results:
 *      registers a device driver for a major # that depends
 *      on the uid. Add yourself to that list.  List is now in
 *      private/driver-private.c.
 *
 *----------------------------------------------------------------------
 */

static int __init
vmci_init(void)
{
   int retval;

   retval = VMCI_SharedInit();
   if (retval != VMCI_SUCCESS) {
      Warning(LGPFX"Failed to initialize VMCI common components (err=%d).\n",
              retval);
      return -ENOMEM;
   }

   if (!vmci_disable_guest) {
      retval = vmci_guest_init();
      if (retval != 0) {
         Warning(LGPFX"VMCI PCI device not initialized (err=%d).\n", retval);
      } else {
         guestDeviceInit = TRUE;
         if (VMCI_GuestPersonalityActive()) {
            Log(LGPFX"Using guest personality\n");
         }
      }
   }

   if (!vmci_disable_host) {
      retval = vmci_host_init();
      if (retval != 0) {
         Warning(LGPFX"Unable to initialize host personality (err=%d).\n",
                 retval);
      } else {
         hostDeviceInit = TRUE;
         Log(LGPFX"Using host personality\n");
      }
   }

   if (!guestDeviceInit && !hostDeviceInit) {
      VMCI_SharedCleanup();
      return -ENODEV;
   }

   Log(LGPFX"Module (name=%s) is initialized\n", VMCI_MODULE_NAME);

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * vmci_exit --
 *
 *      Called by /sbin/rmmod
 *
 *
 *----------------------------------------------------------------------
 */

static void __exit
vmci_exit(void)
{
   int retval;

   if (guestDeviceInit) {
      pci_unregister_driver(&vmci_driver);
      vfree(data_buffer);
      guestDeviceInit = FALSE;
   }

   if (hostDeviceInit) {
      unregister_ioctl32_handlers();

      VMCI_HostCleanup();

      retval = misc_deregister(&linuxState.misc);
      if (retval) {
         Warning(LGPFX "Module %s: error unregistering\n", VMCI_MODULE_NAME);
      } else {
         Log(LGPFX"Module %s: unloaded\n", VMCI_MODULE_NAME);
      }

      hostDeviceInit = FALSE;
   }

   VMCI_SharedCleanup();
}


module_init(vmci_init);
module_exit(vmci_exit);
MODULE_DEVICE_TABLE(pci, vmci_ids);

module_param_named(disable_host, vmci_disable_host, bool, 0);
MODULE_PARM_DESC(disable_host, "Disable driver host personality - (default=0)");

module_param_named(disable_guest, vmci_disable_guest, bool, 0);
MODULE_PARM_DESC(disable_guest, "Disable driver guest personality - (default=0)");

module_param_named(disable_msi, vmci_disable_msi, bool, 0);
MODULE_PARM_DESC(disable_msi, "Disable MSI use in driver - (default=0)");

module_param_named(disable_msix, vmci_disable_msix, bool, 0);
MODULE_PARM_DESC(disable_msix, "Disable MSI-X use in driver - (default="
                 __stringify(VMCI_DISABLE_MSIX) ")");

MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Virtual Machine Communication Interface (VMCI).");
MODULE_VERSION(VMCI_DRIVER_VERSION_STRING);
MODULE_LICENSE("GPL v2");
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
