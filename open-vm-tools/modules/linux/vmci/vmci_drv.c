/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * vmci.c --
 *
 *      Linux guest driver for the VMCI device.
 */
   
#include "driver-config.h"

#define EXPORT_SYMTAB
   
   
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#include <linux/moduleparam.h>
#endif
   
#include "compat_kernel.h"
#include "compat_module.h"
#include "compat_pci.h"
#include "compat_wait.h"
#include "compat_init.h"
#include "compat_ioport.h"
#include "compat_interrupt.h"
#include "compat_page.h"
#include "vm_basic_types.h"
#include "vm_device_version.h"
#include "kernelStubs.h"
#include "vmci_iocontrols.h"
#include "vmci_defs.h"
#include "vmciInt.h"
#include "vmci_infrastructure.h"
#include "vmciDatagram.h"
#include "vmciProcess.h"
#include "vmciUtil.h"
#include "vmciEvent.h"
#include "vmciQueuePairInt.h"
#include "vmci_version.h"
#include "vmciCommonInt.h"

#define LGPFX "VMCI: "
#define VMCI_DEVICE_MINOR_NUM 0

typedef struct vmci_device {
   struct semaphore lock;

   unsigned int ioaddr;
   unsigned int ioaddr_size;
   unsigned int irq;

   Bool         enabled;
   spinlock_t   dev_spinlock;
} vmci_device;

static int vmci_probe_device(struct pci_dev *pdev,
                             const struct pci_device_id *id);
static void vmci_remove_device(struct pci_dev* pdev);
static int vmci_open(struct inode *inode, struct file *file);
static int vmci_close(struct inode *inode, struct file *file);
static int vmci_ioctl(struct inode *inode, struct file *file, 
                      unsigned int cmd, unsigned long arg);
static unsigned int vmci_poll(struct file *file, poll_table *wait);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static compat_irqreturn_t vmci_interrupt(int irq, void *dev_id, 
                                         struct pt_regs * regs);
#else
static compat_irqreturn_t vmci_interrupt(int irq, void *dev_id);
#endif
static void dispatch_datagrams(unsigned long data);

static const struct pci_device_id vmci_ids[] = {
   { PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_VMCI), },
   { 0 },
};

static struct file_operations vmci_ops = {
   .owner   = THIS_MODULE,
   .open    = vmci_open,
   .release = vmci_close,
   .ioctl   = vmci_ioctl,
   .poll    = vmci_poll,
};

static struct pci_driver vmci_driver = {
   .name     = "vmci",
   .id_table = vmci_ids,
   .probe = vmci_probe_device,
   .remove = vmci_remove_device,
};

static vmci_device vmci_dev;

/* We dynamically request the device major number at init time. */
static int device_major_nr = 0;

DECLARE_TASKLET(vmci_tasklet, dispatch_datagrams, 
                (unsigned long)&vmci_dev);

/* 
 * Allocate a buffer for incoming datagrams globally to avoid repeated 
 * allocation in the interrupt handler's atomic context. 
 */ 

static uint8 *data_buffer = NULL;  
static uint32 data_buffer_size = VMCI_MAX_DG_SIZE;


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_init --
 *
 *      Initialization, called by Linux when the module is loaded.
 *
 * Results:
 *      Returns 0 for success, negative errno value otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_init(void)
{
   int err = -ENOMEM;

   /* Register device node ops. */
   err = register_chrdev(0, "vmci", &vmci_ops);
   if (err < 0) {
      printk(KERN_ERR "Unable to register vmci device\n"); 
      return err;
   }
   device_major_nr = err;

   printk("VMCI: Major device number is: %d\n", device_major_nr);

   /* Initialize device data. */
   init_MUTEX(&vmci_dev.lock);
   spin_lock_init(&vmci_dev.dev_spinlock);
   vmci_dev.enabled = FALSE;

   data_buffer = vmalloc(data_buffer_size);
   if (data_buffer == NULL) {
      goto error;
   }
  
   /* This should be last to make sure we are done initializing. */
   err = pci_register_driver(&vmci_driver);
   if (err < 0) {
      goto error;
   }

   return 0;

error:
   unregister_chrdev(device_major_nr, "vmci");
   vfree(data_buffer);
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_exit --
 *
 *      Cleanup, called by Linux when the module is unloaded.
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
vmci_exit(void)
{
   pci_unregister_driver(&vmci_driver);
   
   unregister_chrdev(device_major_nr, "vmci");

   vfree(data_buffer);
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

   result = compat_pci_enable_device(pdev);
   if (result) {
      printk(KERN_ERR "Cannot VMCI device %s: error %d\n",
             compat_pci_name(pdev), result);
      return result;
   }
   compat_pci_set_master(pdev); /* To enable QueuePair functionality. */
   ioaddr = compat_pci_resource_start(pdev, 0);
   ioaddr_size = compat_pci_resource_len(pdev, 0);

   /*
    * Request I/O region with adjusted base address and size. The adjusted
    * values are needed and used if we release the region in case of failure.
    */

   if (!compat_request_region(ioaddr, ioaddr_size, "vmci")) {
      printk(KERN_INFO "vmci: Another driver already loaded "
                       "for device in slot %s.\n", compat_pci_name(pdev));
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

   /* Let the host know which capabilities we intend to use. */
   outl(VMCI_CAPS_DATAGRAM, ioaddr + VMCI_CAPS_ADDR);

   /* Device struct initialization. */
   down(&vmci_dev.lock);
   if (vmci_dev.enabled) {
      printk(KERN_ERR "VMCI device already enabled.\n");
      goto unlock;
   }

   vmci_dev.ioaddr = ioaddr;
   vmci_dev.ioaddr_size = ioaddr_size;
   vmci_dev.irq = pdev->irq;

   /* Check host capabilities. */
   if (!VMCI_CheckHostCapabilities()) {
      goto unlock;
   }

   /* Enable device. */
   vmci_dev.enabled = TRUE;
   pci_set_drvdata(pdev, &vmci_dev);

   /* 
    * We do global initialization here because we need datagrams for
    * event init. If we ever support more than one VMCI device we will
    * have to create seperate LateInit/EarlyExit functions that can be
    * used to do initialization/cleanup that depends on the device
    * being accessible.  We need to initialize VMCI components before
    * requesting an irq - the VMCI interrupt handler uses these
    * components, and it may be invoked once request_irq() has
    * registered the handler (as the irq line may be shared).
    */
   VMCIProcess_Init();
   VMCIDatagram_Init();
   VMCIEvent_Init();
   VMCIUtil_Init();
   VMCIQueuePair_Init();

   if (request_irq(vmci_dev.irq, vmci_interrupt, COMPAT_IRQF_SHARED, 
                   "vmci", &vmci_dev)) {
      printk(KERN_ERR "vmci: irq %u in use\n", vmci_dev.irq);
      goto components_exit;
   }

   printk(KERN_INFO "Registered vmci device.\n");

   up(&vmci_dev.lock);

   /* Enable specific interrupt bits. */
   outl(VMCI_IMR_DATAGRAM, vmci_dev.ioaddr + VMCI_IMR_ADDR);

   /* Enable interrupts. */
   outl(VMCI_CONTROL_INT_ENABLE, vmci_dev.ioaddr + VMCI_CONTROL_ADDR);

   return 0;

 components_exit:
   VMCIQueuePair_Exit();
   VMCIUtil_Exit();
   VMCIEvent_Exit();
   VMCIProcess_Exit();
 unlock:
   up(&vmci_dev.lock);
 release:
   release_region(ioaddr, ioaddr_size);
 pci_disable:
   compat_pci_disable_device(pdev);
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

   VMCIQueuePair_Exit();

   // XXX Todo add exit/cleanup functions for util, sm, dg, and resource apis.
   VMCIUtil_Exit();
   VMCIEvent_Exit();
   //VMCIDatagram_Exit();
   VMCIProcess_Exit();
   
   down(&dev->lock);
   printk(KERN_INFO "Resetting vmci device\n");
   outl(VMCI_CONTROL_RESET, vmci_dev.ioaddr + VMCI_CONTROL_ADDR);
   free_irq(dev->irq, dev);
   release_region(dev->ioaddr, dev->ioaddr_size);
   dev->enabled = FALSE;

   printk(KERN_INFO "Unregistered vmci device.\n");
   up(&dev->lock);
   
   compat_pci_disable_device(pdev);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_open --
 *
 *      Open device.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_open(struct inode *inode,  // IN
          struct file *file)    // IN
{
   VMCIGuestDeviceHandle *devHndl;
   int errcode;

   printk(KERN_INFO "Opening vmci device\n");

   if (MINOR(inode->i_rdev) != VMCI_DEVICE_MINOR_NUM) {
      return -ENODEV;
   }

   down(&vmci_dev.lock);
   if (!vmci_dev.enabled) {
      printk(KERN_INFO "Received open on uninitialized vmci device.\n");
      errcode = -ENODEV;
      goto unlock;
   }

   /* Do open ... */
   devHndl = VMCI_AllocKernelMem(sizeof *devHndl, VMCI_MEMORY_NORMAL);
   if (!devHndl) {
      printk(KERN_INFO "Failed to create device obj when opening device.\n");
      errcode = -ENOMEM;
      goto unlock;
   }
   devHndl->obj = NULL;
   devHndl->objType = VMCIOBJ_NOT_SET;
   file->private_data = devHndl;

   up(&vmci_dev.lock);

   return 0;

 unlock:
   up(&vmci_dev.lock);
   return errcode;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_close --
 *
 *      Close device.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_close(struct inode *inode,  // IN
           struct file *file)    // IN
{
   VMCIGuestDeviceHandle *devHndl =
      (VMCIGuestDeviceHandle *) file->private_data;

   if (devHndl) {
      if (devHndl->objType == VMCIOBJ_PROCESS) {
         VMCIProcess_Destroy((VMCIProcess *) devHndl->obj);
      } else if (devHndl->objType == VMCIOBJ_DATAGRAM_PROCESS) {
         VMCIDatagramProcess_Destroy((VMCIDatagramProcess *) devHndl->obj);
      }
      VMCI_FreeKernelMem(devHndl, sizeof *devHndl);
      file->private_data = NULL;
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_ioctl --
 *
 *      IOCTL interface to device.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmci_ioctl(struct inode *inode,  // IN
           struct file *file,    // IN
           unsigned int cmd,     // IN
           unsigned long arg)    // IN
{
#ifndef VMX86_DEVEL
   return -ENOTTY;
#else
   int retval;
   VMCIGuestDeviceHandle *devHndl =
      (VMCIGuestDeviceHandle *) file->private_data;

   if (devHndl == NULL) {
      return -EINVAL;
   }

   switch (cmd) {
   case IOCTL_VMCI_CREATE_PROCESS: {
      if (devHndl->objType != VMCIOBJ_NOT_SET) {
         printk("VMCI: Received IOCTLCMD_VMCI_CREATE_PROCESS on "
                "initialized handle.\n");
         retval = -EINVAL;
         break;
      }
      ASSERT(!devHndl->obj);
      retval = VMCIProcess_Create((VMCIProcess **) &devHndl->obj);
      if (retval != 0) {
         printk("VMCI: Failed to create process.\n");
         break;
      }
      devHndl->objType = VMCIOBJ_PROCESS;
      break;
   }

   case IOCTL_VMCI_CREATE_DATAGRAM_PROCESS: {
      VMCIDatagramCreateInfo createInfo;
      VMCIDatagramProcess *dgmProc;

      if (devHndl->objType != VMCIOBJ_NOT_SET) {
         printk("VMCI: Received IOCTLCMD_VMCI_CREATE_DATAGRAM_PROCESS on "
                "initialized handle.\n");
         retval = -EINVAL;
         break;
      }
      ASSERT(!devHndl->obj);

      retval = copy_from_user(&createInfo, (void *)arg, sizeof createInfo);
      if (retval != 0) {
	 printk("VMCI: Error getting datagram create info, %d.\n", retval);
	 retval = -EFAULT;
	 break;
      }
      
      if (VMCIDatagramProcess_Create(&dgmProc, &createInfo,
                                     0 /* Unused */) < VMCI_SUCCESS) {
	 retval = -EINVAL;
	 break;
      }

      retval = copy_to_user((void *)arg, &createInfo, sizeof createInfo);
      if (retval != 0) {
         VMCIDatagramProcess_Destroy(dgmProc);
         printk("VMCI: Failed to create datagram process.\n");
	 retval = -EFAULT;
         break;
      }
      devHndl->obj = dgmProc;
      devHndl->objType = VMCIOBJ_DATAGRAM_PROCESS;
      break;
   }

   case IOCTL_VMCI_DATAGRAM_SEND: {
      VMCIDatagramSendRecvInfo sendInfo;
      VMCIDatagram *dg = NULL;

      if (devHndl->objType != VMCIOBJ_DATAGRAM_PROCESS) {
         printk("VMCI: Ioctl %d only valid for process datagram handle.\n",
		cmd);
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&sendInfo, (void *) arg, sizeof sendInfo);
      if (retval) {
         printk("VMCI: copy_from_user failed.\n");
         retval = -EFAULT;
         break;
      }

      if (sendInfo.len > VMCI_MAX_DG_SIZE) {
         printk("VMCI: datagram size too big.\n");
	 retval = -EINVAL;
	 break;
      }

      dg = VMCI_AllocKernelMem(sendInfo.len, VMCI_MEMORY_NORMAL);
      if (dg == NULL) {
         printk("VMCI: Cannot allocate memory to dispatch datagram.\n");
         retval = -ENOMEM;
         break;
      }

      retval = copy_from_user(dg, (char *)(VA)sendInfo.addr, sendInfo.len);
      if (retval != 0) {
         printk("VMCI: Error getting datagram: %d\n", retval);
         VMCI_FreeKernelMem(dg, sendInfo.len);
         retval = -EFAULT;
         break;
      }

      DEBUG_ONLY(printk("VMCI: Datagram dst handle 0x%x:0x%x, src handle "
			"0x%x:0x%x, payload size %"FMT64"u.\n", 
			dg->dst.context, dg->dst.resource, 
			dg->src.context, dg->src.resource, dg->payloadSize));

      sendInfo.result = VMCIDatagram_Send(dg);
      VMCI_FreeKernelMem(dg, sendInfo.len);

      retval = copy_to_user((void *)arg, &sendInfo, sizeof sendInfo);
      break;
   }

   case IOCTL_VMCI_DATAGRAM_RECEIVE: {
      VMCIDatagramSendRecvInfo recvInfo;
      VMCIDatagram *dg = NULL;

      if (devHndl->objType != VMCIOBJ_DATAGRAM_PROCESS) {
         printk("VMCI: Ioctl %d only valid for process datagram handle.\n",
		cmd);
         retval = -EINVAL;
         break;
      }

      retval = copy_from_user(&recvInfo, (void *) arg, sizeof recvInfo);
      if (retval) {
         printk("VMCI: copy_from_user failed.\n");
         retval = -EFAULT;
         break;
      }

      ASSERT(devHndl->obj);
      recvInfo.result = 
	 VMCIDatagramProcess_ReadCall((VMCIDatagramProcess *)devHndl->obj, 
				      recvInfo.len, &dg);
      if (recvInfo.result < VMCI_SUCCESS) {
	 retval = -EINVAL;
	 break;
      }
      ASSERT(dg);
      retval = copy_to_user((void *) ((uintptr_t) recvInfo.addr), dg,
			    VMCI_DG_SIZE(dg));
      VMCI_FreeKernelMem(dg, VMCI_DG_SIZE(dg));
      if (retval != 0) {
	 break;
      }
      retval = copy_to_user((void *)arg, &recvInfo, sizeof recvInfo);
      break;
   }

   case IOCTL_VMCI_GET_CONTEXT_ID: {
      VMCIId cid = VMCI_GetContextID();

      retval = copy_to_user((void *)arg, &cid, sizeof cid);
      break;
   }

   default:
      printk(KERN_DEBUG "vmci_ioctl(): unknown ioctl 0x%x.\n", cmd);
      retval = -EINVAL;
      break;
   }

   return retval;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_poll --
 *
 *      vmci poll function
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static unsigned int
vmci_poll(struct file *file, // IN
          poll_table *wait)  // IN
{
   VMCILockFlags flags;
   unsigned int mask = 0;
   VMCIGuestDeviceHandle *devHndl =
      (VMCIGuestDeviceHandle *) file->private_data;

   /* 
    * Check for call to this VMCI process. 
    */
   
   if (!devHndl) {
      return mask;
   }
   if (devHndl->objType == VMCIOBJ_DATAGRAM_PROCESS) {
      VMCIDatagramProcess *dgmProc = (VMCIDatagramProcess *) devHndl->obj;
      ASSERT(dgmProc);
      
      if (wait != NULL) {
         poll_wait(file, &dgmProc->host.waitQueue, wait);
      }

      VMCI_GrabLock_BH(&dgmProc->datagramQueueLock, &flags);
      if (dgmProc->pendingDatagrams > 0) {
         mask = POLLIN;
      }
      VMCI_ReleaseLock_BH(&dgmProc->datagramQueueLock, flags);
   }

   return mask;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_interrupt --
 *
 *      Interrupt handler.
 *
 * Results:
 *      None.
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
   unsigned int icr = 0;

   if (dev == NULL) {
      printk (KERN_DEBUG "vmci_interrupt(): irq %d for unknown device.\n",
              irq);
      return COMPAT_IRQ_NONE;
   }

   /* Acknowledge interrupt and determine what needs doing. */
   icr = inl(dev->ioaddr + VMCI_ICR_ADDR);
   if (icr == 0) {
      return COMPAT_IRQ_NONE;
   }

   if (icr & VMCI_ICR_DATAGRAM) {
      tasklet_schedule(&vmci_tasklet);
      icr &= ~VMCI_ICR_DATAGRAM;
   }
   if (icr != 0) {
      printk(KERN_INFO LGPFX"Ignoring unknown interrupt cause (%d).\n", icr);
   }

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
   Bool retval;

   down(&vmci_dev.lock);
   retval = vmci_dev.enabled;
   up(&vmci_dev.lock);
   
   return retval;
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


module_init(vmci_init);
module_exit(vmci_exit);
MODULE_DEVICE_TABLE(pci, vmci_ids);

/* Module information. */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Virtual Machine Communication Interface");
MODULE_VERSION(VMCI_DRIVER_VERSION_STRING);
MODULE_LICENSE("GPL v2");
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
