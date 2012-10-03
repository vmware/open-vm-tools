/*********************************************************
 * Copyright (C) 1999 VMware, Inc. All rights reserved.
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
 * vmxnet.c: A virtual network driver for VMware.
 */
#include "driver-config.h"
#include "compat_module.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#include <linux/moduleparam.h>
#endif

#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_pci.h"
#include "compat_pci_mapping.h"
#include "compat_timer.h"
#include "compat_ethtool.h"
#include "compat_netdevice.h"
#include "compat_skbuff.h"
#include <linux/etherdevice.h>
#include "compat_ioport.h"
#ifndef KERNEL_2_1
#include <linux/delay.h>
#endif
#include "compat_interrupt.h"

#include <linux/init.h>

#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#include "vm_basic_types.h"
#include "vmnet_def.h"
#include "vmxnet_def.h"
#include "vmxnet2_def.h"
#include "vm_device_version.h"
#include "vmxnetInt.h"
#include "net.h"
#include "eth_public.h"
#include "vmxnet_version.h"

static int vmxnet_debug = 1;

#define VMXNET_WATCHDOG_TIMEOUT (5 * HZ)

#if defined(CONFIG_NET_POLL_CONTROLLER) || defined(HAVE_POLL_CONTROLLER)
#define VMW_HAVE_POLL_CONTROLLER
#endif

static int vmxnet_open(struct net_device *dev);
static int vmxnet_start_tx(struct sk_buff *skb, struct net_device *dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static compat_irqreturn_t vmxnet_interrupt(int irq, void *dev_id,
					   struct pt_regs * regs);
#else
static compat_irqreturn_t vmxnet_interrupt(int irq, void *dev_id);
#endif
#ifdef VMW_HAVE_POLL_CONTROLLER
static void vmxnet_netpoll(struct net_device *dev);
#endif
static int vmxnet_close(struct net_device *dev);
static void vmxnet_set_multicast_list(struct net_device *dev);
static int vmxnet_set_mac_address(struct net_device *dev, void *addr);
static struct net_device_stats *vmxnet_get_stats(struct net_device *dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
static int vmxnet_set_features(struct net_device *netdev, compat_netdev_features_t
			       features);
#endif
#if defined(HAVE_CHANGE_MTU) || defined(HAVE_NET_DEVICE_OPS)
static int vmxnet_change_mtu(struct net_device *dev, int new_mtu);
#endif

static Bool vmxnet_check_version(unsigned int ioaddr);
static Bool vmxnet_probe_features(struct net_device *dev, Bool morphed,
                                  Bool probeFromResume);
static void vmxnet_release_private_data(Vmxnet_Private *lp,
                                        struct pci_dev *pdev);
static int vmxnet_probe_device(struct pci_dev *pdev, const struct pci_device_id *id);
static void vmxnet_remove_device(struct pci_dev *pdev);

static Bool vmxnet_alloc_shared_mem(struct pci_dev *pdev, size_t size,
				    void **vaOut, dma_addr_t *paOut);

#ifdef CONFIG_PM

/*
 * Prototype of suspend and resume handlers are different depending on the
 * version of Linux kernel.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)

#  define VMXNET_SUSPEND_DEVICE(pdev, state)                                  \
   static int vmxnet_suspend_device(struct pci_dev *pdev, pm_message_t state)
#  define VMXNET_RESUME_DEVICE(pdev)                                          \
   static int vmxnet_resume_device(struct pci_dev *pdev)

#  define VMXNET_SET_POWER_STATE_D0(pdev) pci_set_power_state(pdev, PCI_D0)

#  define VMXNET_SET_POWER_STATE(pdev, state)                                 \
      pci_set_power_state(pdev, pci_choose_state(pdev, state))

#  define VMXNET_PM_RETURN(ret) return ret

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 6)

#  define VMXNET_SUSPEND_DEVICE(pdev, state)                                  \
   static int vmxnet_suspend_device(struct pci_dev *pdev, u32 state)
#  define VMXNET_RESUME_DEVICE(pdev)                                          \
   static int vmxnet_resume_device(struct pci_dev *pdev)
#  define VMXNET_SET_POWER_STATE_D0(pdev) (pdev)->current_state = 0
#  define VMXNET_SET_POWER_STATE(pdev, state)                                 \
   do {                                                                       \
      (pdev)->current_state = state;                                          \
   } while (0)
#  define VMXNET_PM_RETURN(ret) return ret

#else

#  define VMXNET_SUSPEND_DEVICE(pdev, state)                                  \
   static void vmxnet_suspend_device(struct pci_dev *pdev)
#  define VMXNET_RESUME_DEVICE(pdev)                                          \
   static void vmxnet_resume_device(struct pci_dev *pdev)
#  define VMXNET_SET_POWER_STATE_D0(pdev)
#  define VMXNET_SET_POWER_STATE(pdev, state)
#  define VMXNET_PM_RETURN(ret)

#endif

VMXNET_SUSPEND_DEVICE(pdev, state);
VMXNET_RESUME_DEVICE(pdev);

#endif /* CONFIG_PM */

#ifdef MODULE
static int debug = -1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
   module_param(debug, int, 0444);
#else
   MODULE_PARM(debug, "i");
#endif
#endif

#ifdef VMXNET_DO_ZERO_COPY
#undef VMXNET_DO_ZERO_COPY
#endif

#if defined(MAX_SKB_FRAGS) && \
    ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,18) ) && \
    ( LINUX_VERSION_CODE != KERNEL_VERSION(2, 6, 0) )
#define VMXNET_DO_ZERO_COPY
#endif

#define VMXNET_GET_LO_ADDR(dma)   ((uint32)(dma))
#define VMXNET_GET_HI_ADDR(dma)   ((uint16)(((uint64)(dma)) >> 32))
#define VMXNET_GET_DMA_ADDR(sge)  ((dma_addr_t)((((uint64)(sge).addrHi) << 32) | \
                                                (sge).addrLow))

#define VMXNET_FILL_SG(sg, dma, size)\
do{\
   (sg).addrLow = VMXNET_GET_LO_ADDR(dma);\
   (sg).addrHi  = VMXNET_GET_HI_ADDR(dma);\
   (sg).length  = size;\
} while (0)

#ifdef VMXNET_DO_ZERO_COPY
#include <net/checksum.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>

/*
 * Tx buffer size that we need for copying header
 * max header is: 14(ip) + 4(vlan) + ip (60) + tcp(60) = 138
 * round it up to the power of 2
 */
#define TX_PKT_HEADER_SIZE      256

/* Constants used for Zero Copy Tx */
#define ETHERNET_HEADER_SIZE           14
#define VLAN_TAG_LENGTH                4
#define ETH_FRAME_TYPE_LOCATION        12
#define ETH_TYPE_VLAN_TAG              0x0081 /* in NBO */
#define ETH_TYPE_IP                    0x0008 /* in NBO */

#define PKT_OF_PROTO(skb, type) \
   (*(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION) == (type) || \
    (*(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION) == ETH_TYPE_VLAN_TAG && \
     *(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION + VLAN_TAG_LENGTH) == (type)))

#define PKT_OF_IPV4(skb) PKT_OF_PROTO(skb, ETH_TYPE_IP)

#if defined(NETIF_F_TSO)
#define VMXNET_DO_TSO

#if defined(NETIF_F_GSO) /* 2.6.18 and upwards */
#define VMXNET_SKB_MSS(skb) skb_shinfo(skb)->gso_size
#else
#define VMXNET_SKB_MSS(skb) skb_shinfo(skb)->tso_size
#endif
#endif

#endif // VMXNET_DO_ZERO_COPY

#ifdef VMXNET_DO_TSO
static int disable_lro = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
   module_param(disable_lro, int, 0);
#else
   MODULE_PARM(disable_lro, "i");
#endif
#endif

#ifdef VMXNET_DEBUG
#define VMXNET_LOG(msg...) printk(KERN_ERR msg)
#else
#define VMXNET_LOG(msg...)
#endif // VMXNET_DEBUG

/* Data structure used when determining what hardware the driver supports. */

static const struct pci_device_id vmxnet_chips[] =
   {
      {
         PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_NET),
         .driver_data = VMXNET_CHIP,
      },
      {
         PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE),
         .driver_data = LANCE_CHIP,
      },
      {
         0,
      },
   };

static struct pci_driver vmxnet_driver = {
					    .name = "vmxnet",
                                            .id_table = vmxnet_chips,
                                            .probe = vmxnet_probe_device,
                                            .remove = vmxnet_remove_device,
#ifdef CONFIG_PM
                                            .suspend = vmxnet_suspend_device,
                                            .resume = vmxnet_resume_device,
#endif
                                         };

#if defined(HAVE_CHANGE_MTU) || defined(HAVE_NET_DEVICE_OPS)
static int
vmxnet_change_mtu(struct net_device *dev, int new_mtu)
{
   struct Vmxnet_Private *lp = netdev_priv(dev);

   if (new_mtu < VMXNET_MIN_MTU || new_mtu > VMXNET_MAX_MTU) {
      return -EINVAL;
   }

   if (new_mtu > 1500 && !lp->jumboFrame) {
      return -EINVAL;
   }

   dev->mtu = new_mtu;
   return 0;
}

#endif


#ifdef SET_ETHTOOL_OPS
/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_get_settings --
 *
 *      Get device-specific settings.
 *
 * Results:
 *      0 on success, errno on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_get_settings(struct net_device *dev,
                    struct ethtool_cmd *ecmd)
{
   ecmd->supported = SUPPORTED_1000baseT_Full | SUPPORTED_TP;
   ecmd->advertising = ADVERTISED_TP;
   ecmd->port = PORT_TP;
   ecmd->transceiver = XCVR_INTERNAL;

   if (netif_carrier_ok(dev)) {
      ecmd->speed = 1000;
      ecmd->duplex = DUPLEX_FULL;
   } else {
      ecmd->speed = -1;
      ecmd->duplex = -1;
   }
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_get_drvinfo --
 *
 *      Ethtool callback to return driver information
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Updates *drvinfo
 *
 *----------------------------------------------------------------------------
 */

static void
vmxnet_get_drvinfo(struct net_device *dev,
                   struct ethtool_drvinfo *drvinfo)
{
   struct Vmxnet_Private *lp = netdev_priv(dev);

   strncpy(drvinfo->driver, vmxnet_driver.name, sizeof(drvinfo->driver));
   drvinfo->driver[sizeof(drvinfo->driver) - 1] = '\0';

   strncpy(drvinfo->version, VMXNET_DRIVER_VERSION_STRING,
           sizeof(drvinfo->version));
   drvinfo->driver[sizeof(drvinfo->version) - 1] = '\0';

   strncpy(drvinfo->fw_version, "N/A", sizeof(drvinfo->fw_version));
   drvinfo->fw_version[sizeof(drvinfo->fw_version) - 1] = '\0';

   strncpy(drvinfo->bus_info, pci_name(lp->pdev), ETHTOOL_BUSINFO_LEN);
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_get_tx_csum --
 *
 *    Ethtool op to check whether or not hw csum offload is enabled.
 *
 * Result:
 *    1 if csum offload is currently used and 0 otherwise.
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static uint32
vmxnet_get_tx_csum(struct net_device *netdev)
{
   return (netdev->features & NETIF_F_HW_CSUM) != 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_get_rx_csum --
 *
 *    Ethtool op to check whether or not rx csum offload is enabled.
 *
 * Result:
 *    Always return 1 to indicate that rx csum is enabled.
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static uint32
vmxnet_get_rx_csum(struct net_device *netdev)
{
   return 1;
}


/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_set_tx_csum --
 *
 *    Ethtool op to change if hw csum offloading should be used or not.
 *    If the device supports hardware checksum capability netdev features bit
 *    is set/reset. This bit is referred to while setting hw checksum required
 *    flag (VMXNET2_TX_HW_XSUM) in xmit ring entry.
 *
 * Result:
 *    0 on success. -EOPNOTSUPP if ethtool asks to set hw checksum and device
 *    does not support it.
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_set_tx_csum(struct net_device *netdev, uint32 val)
{
   if (val) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
      struct Vmxnet_Private *lp = netdev_priv(netdev);
      if (lp->capabilities & (VMNET_CAP_IP4_CSUM | VMNET_CAP_HW_CSUM)) {
         netdev->features |= NETIF_F_HW_CSUM;
         return 0;
      }
#endif
      return -EOPNOTSUPP;
   } else {
      netdev->features &= ~NETIF_F_HW_CSUM;
   }
   return 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_set_rx_csum --
 *
 *    Ethtool op to change if hw csum offloading should be used or not for
 *    received packets. Hardware checksum on received packets cannot be turned
 *    off. Hence we fail the ethtool op which turns h/w csum off.
 *
 * Result:
 *    0 when rx csum is set. -EOPNOTSUPP when ethtool tries to reset rx csum.
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_set_rx_csum(struct net_device *netdev, uint32 val)
{
   if (val) {
      return 0;
   } else {
      return -EOPNOTSUPP;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_set_tso --
 *
 *    Ethtool handler to set TSO. If the data is non-zero, TSO is
 *    enabled. Othewrise, it is disabled.
 *
 *  Results:
 *    0 if successful, error code otherwise.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#   ifdef VMXNET_DO_TSO
static int
vmxnet_set_tso(struct net_device *dev, u32 data)
{
   if (data) {
      struct Vmxnet_Private *lp = netdev_priv(dev);

      if (!lp->tso) {
         return -EINVAL;
      }
      dev->features |= NETIF_F_TSO;
   } else {
      dev->features &= ~NETIF_F_TSO;
   }
   return 0;
}
#   endif
#endif


static struct ethtool_ops
vmxnet_ethtool_ops = {
   .get_settings        = vmxnet_get_settings,
   .get_drvinfo         = vmxnet_get_drvinfo,
   .get_link            = ethtool_op_get_link,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
   .get_rx_csum         = vmxnet_get_rx_csum,
   .set_rx_csum         = vmxnet_set_rx_csum,
   .get_tx_csum         = vmxnet_get_tx_csum,
   .set_tx_csum         = vmxnet_set_tx_csum,
   .get_sg              = ethtool_op_get_sg,
   .set_sg              = ethtool_op_set_sg,
#   ifdef VMXNET_DO_TSO
   .get_tso             = ethtool_op_get_tso,
   .set_tso             = vmxnet_set_tso,
#      if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
   .get_ufo             = ethtool_op_get_ufo,
#      endif
#   endif
#endif
};


#else   /* !defined(SET_ETHTOOL_OPS) */


/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_get_settings --
 *
 *    Ethtool handler to get device settings.
 *
 *  Results:
 *    0 if successful, error code otherwise. Settings are copied to addr.
 *
 *  Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

#ifdef ETHTOOL_GSET
static int
vmxnet_get_settings(struct net_device *dev, void *addr)
{
   struct ethtool_cmd cmd;
   memset(&cmd, 0, sizeof(cmd));
   cmd.speed = 1000;     // 1 Gb
   cmd.duplex = 1;       // full-duplex
   cmd.maxtxpkt = 1;     // no tx coalescing
   cmd.maxrxpkt = 1;     // no rx coalescing
   cmd.autoneg = 0;      // no autoneg
   cmd.advertising = 0;  // advertise nothing

   return copy_to_user(addr, &cmd, sizeof(cmd));
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_get_link --
 *
 *    Ethtool handler to get the link state.
 *
 *  Results:
 *    0 if successful, error code otherwise. The link status is copied to
 *    addr.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef ETHTOOL_GLINK
static int
vmxnet_get_link(struct net_device *dev, void *addr)
{
   compat_ethtool_value value = {ETHTOOL_GLINK};
   value.data = netif_carrier_ok(dev) ? 1 : 0;
   return copy_to_user(addr, &value, sizeof(value));
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_get_tso --
 *
 *    Ethtool handler to get the TSO setting.
 *
 *  Results:
 *    0 if successful, error code otherwise. The TSO setting is copied to
 *    addr.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef VMXNET_DO_TSO
static int
vmxnet_get_tso(struct net_device *dev, void *addr)
{
   compat_ethtool_value value = { ETHTOOL_GTSO };
   value.data = (dev->features & NETIF_F_TSO) ? 1 : 0;
   if (copy_to_user(addr, &value, sizeof(value))) {
       return -EFAULT;
   }
   return 0;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_set_tso --
 *
 *    Ethtool handler to set TSO. If the data in addr is non-zero, TSO is
 *    enabled. Othewrise, it is disabled.
 *
 *  Results:
 *    0 if successful, error code otherwise.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef VMXNET_DO_TSO
static int
vmxnet_set_tso(struct net_device *dev, void *addr)
{
   compat_ethtool_value value;
   if (copy_from_user(&value, addr, sizeof(value))) {
      return -EFAULT;
   }

   if (value.data) {
      struct Vmxnet_Private *lp = netdev_priv(dev);

      if (!lp->tso) {
         return -EINVAL;
      }
      dev->features |= NETIF_F_TSO;
   } else {
      dev->features &= ~NETIF_F_TSO;
   }
   return 0;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_ethtool_ioctl --
 *
 *    Handler for ethtool ioctl calls.
 *
 *  Results:
 *    If ethtool op is supported, the outcome of the op. Otherwise,
 *    -EOPNOTSUPP.
 *
 *  Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#ifdef SIOCETHTOOL
static int
vmxnet_ethtool_ioctl(struct net_device *dev, struct ifreq *ifr)
{
   uint32_t cmd;
   if (copy_from_user(&cmd, ifr->ifr_data, sizeof(cmd))) {
      return -EFAULT;
   }
   switch (cmd) {
#ifdef ETHTOOL_GSET
      case ETHTOOL_GSET:
         return vmxnet_get_settings(dev, ifr->ifr_data);
#endif
#ifdef ETHTOOL_GLINK
      case ETHTOOL_GLINK:
         return vmxnet_get_link(dev, ifr->ifr_data);
#endif
#ifdef VMXNET_DO_TSO
      case ETHTOOL_GTSO:
         return vmxnet_get_tso(dev, ifr->ifr_data);
      case ETHTOOL_STSO:
         return vmxnet_set_tso(dev, ifr->ifr_data);
#endif
      default:
         return -EOPNOTSUPP;
   }
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_ioctl --
 *
 *    Handler for ioctl calls.
 *
 * Results:
 *    If ioctl is supported, the result of that operation. Otherwise,
 *    -EOPNOTSUPP.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
   switch (cmd) {
#ifdef SIOCETHTOOL
      case SIOCETHTOOL:
         return vmxnet_ethtool_ioctl(dev, ifr);
#endif
   }
   return -EOPNOTSUPP;
}
#endif /* SET_ETHTOOL_OPS */


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_init --
 *
 *      Initialization, called by Linux when the module is loaded.
 *
 * Results:
 *      Returns 0 for success, negative errno value otherwise.
 *
 * Side effects:
 *      See vmxnet_probe_device, which does all the work.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet_init(void)
{
   int err;

   if (vmxnet_debug > 0) {
      vmxnet_debug = debug;
   }

   printk(KERN_INFO "VMware vmxnet virtual NIC driver\n");

   err = pci_register_driver(&vmxnet_driver);
   if (err < 0) {
      return err;
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_exit --
 *
 *      Cleanup, called by Linux when the module is unloaded.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Unregisters all vmxnet devices with Linux and frees memory.
 *
 *-----------------------------------------------------------------------------
 */

static void
vmxnet_exit(void)
{
   pci_unregister_driver(&vmxnet_driver);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43)
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_tx_timeout --
 *
 *      Network device tx_timeout routine.  Called by Linux when the tx
 *      queue has been stopped for more than dev->watchdog_timeo jiffies.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Tries to restart the transmit queue.
 *
 *-----------------------------------------------------------------------------
 */
static void
vmxnet_tx_timeout(struct net_device *dev)
{
   netif_wake_queue(dev);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43) */


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_link_check --
 *
 *      Propagate device link status to netdev.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Rearms timer for next check.
 *
 *-----------------------------------------------------------------------------
 */

static void
vmxnet_link_check(unsigned long data)   // IN: netdevice pointer
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43)
   struct net_device *dev = (struct net_device *)data;
   struct Vmxnet_Private *lp = netdev_priv(dev);
   uint32 status;
   int ok;

   status = inl(dev->base_addr + VMXNET_STATUS_ADDR);
   ok = (status & VMXNET_STATUS_CONNECTED) != 0;
   if (ok != netif_carrier_ok(dev)) {
      if (ok) {
         netif_carrier_on(dev);
      } else {
         netif_carrier_off(dev);
      }
   }

   /*
    * It would be great if vmxnet2 could generate interrupt when link
    * state changes.  Maybe next time.  Let's just poll media every
    * two seconds (2 seconds is same interval pcnet32 uses).
    */
   mod_timer(&lp->linkCheckTimer, jiffies + 2 * HZ);
#else
   /*
    * Nothing to do on kernels before 2.3.43.  They do not have
    * netif_carrier_*, and as we've lived without link state for
    * years, let's live without it forever on these kernels.
    */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43) */
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_morph_device --
 *
 *      Morph a lance device into vmxnet device.
 *
 * Results:
 *      Returns 0 on success, -1 on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet_morph_device(unsigned int morphAddr) // IN
{
   uint16 magic;

   /* Read morph port to verify that we can morph the adapter. */
   magic = inw(morphAddr);
   if (magic != LANCE_CHIP && magic != VMXNET_CHIP) {
      printk(KERN_ERR "Invalid magic, read: 0x%08X\n", magic);
      return -1;
   }

   /* Morph adapter. */
   outw(VMXNET_CHIP, morphAddr);

   /* Verify that we morphed correctly. */

   magic = inw(morphAddr);
   if (magic != VMXNET_CHIP) {
      printk(KERN_ERR "Couldn't morph adapter. Invalid magic, read: 0x%08X\n",
             magic);
      goto morph_back;
   }

   return 0;

morph_back:
   /* Morph back to LANCE hw. */
   outw(LANCE_CHIP, morphAddr);
   return -1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_unmorph_device --
 *
 *      Morph a vmxnet adapter back to vlance.
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
vmxnet_unmorph_device(unsigned int morphAddr) // IN
{
   uint16 magic;

   /* Read morph port to verify that we can morph the adapter. */
   magic = inw(morphAddr);
   if (magic != VMXNET_CHIP) {
      printk(KERN_ERR "Adapter not morphed, magic: 0x%08X\n", magic);
      return;
   }

   /* Unmorph adapter. */
   outw(LANCE_CHIP, morphAddr);

   /* Verify that we morphed correctly. */

   magic = inw(morphAddr);
   if (magic != LANCE_CHIP) {
      printk(KERN_ERR "Couldn't unmorph adapter. Invalid magic, read: 0x%08X\n",
             magic);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_probe_device --
 *
 *      Most of the initialization at module load time is done here.
 *
 * Results:
 *      Returns 0 for success, an error otherwise.
 *
 * Side effects:
 *      Switches device from vlance to vmxnet mode, creates ethernet
 *      structure for device, and registers device with network stack.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet_probe_device(struct pci_dev             *pdev, // IN: vmxnet PCI device
                    const struct pci_device_id *id)   // IN: matching device ID
{
#ifdef HAVE_NET_DEVICE_OPS
   static const struct net_device_ops vmxnet_netdev_ops = {
      .ndo_open = &vmxnet_open,
      .ndo_start_xmit = &vmxnet_start_tx,
      .ndo_stop = &vmxnet_close,
      .ndo_get_stats = &vmxnet_get_stats,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
      .ndo_set_features = vmxnet_set_features,
#endif
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
      .ndo_set_multicast_list = &vmxnet_set_multicast_list,
#else
      .ndo_set_rx_mode = &vmxnet_set_multicast_list,
#endif
      .ndo_change_mtu = &vmxnet_change_mtu,
#   ifdef VMW_HAVE_POLL_CONTROLLER
      .ndo_poll_controller = vmxnet_netpoll,
#   endif
      .ndo_set_mac_address = &vmxnet_set_mac_address,
      .ndo_tx_timeout = &vmxnet_tx_timeout,
   };
#endif /* HAVE_NET_DEVICE_OPS */
   struct Vmxnet_Private *lp;
   struct net_device *dev;
   unsigned int ioaddr, reqIOAddr, reqIOSize;
   unsigned int irq_line;
   Bool morphed = FALSE;
   int i;

   i = pci_enable_device(pdev);
   if (i) {
      printk(KERN_ERR "Cannot enable vmxnet adapter %s: error %d\n",
             pci_name(pdev), i);
      return i;
   }
   pci_set_master(pdev);
   irq_line = pdev->irq;
   ioaddr = pci_resource_start(pdev, 0);

   reqIOAddr = ioaddr;
   /* Found adapter, adjust ioaddr to match the adapter we found. */
   if (id->driver_data == VMXNET_CHIP) {
      reqIOSize = VMXNET_CHIP_IO_RESV_SIZE;
   } else {
      /*
       * Since this is a vlance adapter we can only use it if
       * its I/0 space is big enough for the adapter to be
       * capable of morphing. This is the first requirement
       * for this adapter to potentially be morphable. The
       * layout of a morphable LANCE adapter is
       *
       * I/O space:
       *
       * |------------------|
       * | LANCE IO PORTS   |
       * |------------------|
       * | MORPH PORT       |
       * |------------------|
       * | VMXNET IO PORTS  |
       * |------------------|
       *
       * VLance has 8 ports of size 4 bytes, the morph port is 4 bytes, and
       * Vmxnet has 10 ports of size 4 bytes.
       *
       * We shift up the ioaddr with the size of the LANCE I/O space since
       * we want to access the vmxnet ports. We also shift the ioaddr up by
       * the MORPH_PORT_SIZE so other port access can be independent of
       * whether we are Vmxnet or a morphed VLance. This means that when
       * we want to access the MORPH port we need to subtract the size
       * from ioaddr to get to it.
       */

      ioaddr += LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE;
      reqIOSize = LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE +
                  VMXNET_CHIP_IO_RESV_SIZE;
   }
   /* Do not attempt to morph non-morphable AMD PCnet */
   if (reqIOSize > pci_resource_len(pdev, 0)) {
      printk(KERN_INFO "vmxnet: Device in slot %s is not supported by this driver.\n",
             pci_name(pdev));
      goto pci_disable;
   }

   /*
    * Request I/O region with adjusted base address and size. The adjusted
    * values are needed and used if we release the region in case of failure.
    */

   if (!compat_request_region(reqIOAddr, reqIOSize, VMXNET_CHIP_NAME)) {
      printk(KERN_INFO "vmxnet: Another driver already loaded for device in slot %s.\n",
             pci_name(pdev));
      goto pci_disable;
   }

   /* Morph the underlying hardware if we found a VLance adapter. */
   if (id->driver_data == LANCE_CHIP) {
      if (vmxnet_morph_device(ioaddr - MORPH_PORT_SIZE) == 0) {
         morphed = TRUE;
      } else {
         goto release_reg;
      }
   }

   printk(KERN_INFO "Found vmxnet/PCI at %#x, irq %u.\n", ioaddr, irq_line);

   if (!vmxnet_check_version(ioaddr)) {
      goto morph_back;
   }

   dev = alloc_etherdev(sizeof *lp);
   if (!dev) {
      printk(KERN_ERR "Unable to allocate ethernet device\n");
      goto morph_back;
   }

   lp = netdev_priv(dev);
   lp->pdev = pdev;

   dev->base_addr = ioaddr;

   if (!vmxnet_probe_features(dev, morphed, FALSE)) {
      goto free_dev;
   }

   dev->irq = irq_line;

#ifdef HAVE_NET_DEVICE_OPS
   dev->netdev_ops = &vmxnet_netdev_ops;
#else
   dev->open = &vmxnet_open;
   dev->hard_start_xmit = &vmxnet_start_tx;
   dev->stop = &vmxnet_close;
   dev->get_stats = &vmxnet_get_stats;
   dev->set_multicast_list = &vmxnet_set_multicast_list;
#ifdef HAVE_CHANGE_MTU
   dev->change_mtu = &vmxnet_change_mtu;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43)
   dev->tx_timeout = &vmxnet_tx_timeout;
#endif
#ifdef VMW_HAVE_POLL_CONTROLLER
   dev->poll_controller = vmxnet_netpoll;
#endif
   /* Do this after ether_setup(), which sets the default value. */
   dev->set_mac_address = &vmxnet_set_mac_address;
#endif /* HAVE_NET_DEVICE_OPS */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43)
   dev->watchdog_timeo = VMXNET_WATCHDOG_TIMEOUT;
#endif

#ifdef SET_ETHTOOL_OPS
   SET_ETHTOOL_OPS(dev, &vmxnet_ethtool_ops);
#else
   dev->do_ioctl = vmxnet_ioctl;
#endif

   COMPAT_SET_MODULE_OWNER(dev);
   COMPAT_SET_NETDEV_DEV(dev, &pdev->dev);

   if (register_netdev(dev)) {
      printk(KERN_ERR "Unable to register device\n");
      goto free_dev_dd;
   }
   /*
    * Use deferrable timer - we want 2s interval, but if it will
    * be 2 seconds or 10 seconds, we do not care.
    */
   compat_init_timer_deferrable(&lp->linkCheckTimer);
   lp->linkCheckTimer.data = (unsigned long)dev;
   lp->linkCheckTimer.function = vmxnet_link_check;
   vmxnet_link_check(lp->linkCheckTimer.data);

   /* Do this after register_netdev(), which sets device name */
   VMXNET_LOG("%s: %s at %#3lx assigned IRQ %d.\n",
              dev->name, lp->name, dev->base_addr, dev->irq);

   pci_set_drvdata(pdev, dev);
#ifdef CONFIG_PM
   /*
    * Initialize pci_dev's current_state for .suspend to work properly.
    */

   VMXNET_SET_POWER_STATE_D0(pdev);
#endif
   return 0;

free_dev_dd:
   vmxnet_release_private_data(lp, pdev);
free_dev:
   free_netdev(dev);
morph_back:
   if (morphed) {
      vmxnet_unmorph_device(ioaddr - MORPH_PORT_SIZE);
   }
release_reg:
   release_region(reqIOAddr, reqIOSize);
pci_disable:
   pci_disable_device(pdev);
   return -EBUSY;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_check_version --
 *
 *      Helper to check version of the device backend to see if it is
 *      compatible with the driver.  Called from .probe or .resume.
 *
 * Results:
 *      TRUE/FALSE on success/failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
vmxnet_check_version(unsigned int ioaddr) // IN:
{
   /* VMware's version of the magic number */
   unsigned int low_vmware_version;

   low_vmware_version = inl(ioaddr + VMXNET_LOW_VERSION);
   if ((low_vmware_version & 0xffff0000) != (VMXNET2_MAGIC & 0xffff0000)) {
      printk(KERN_ERR "Driver version 0x%08X doesn't match version 0x%08X\n",
             VMXNET2_MAGIC, low_vmware_version);
      return FALSE;
   } else {
      /*
       * The low version looked OK so get the high version and make sure that
       * our version is supported.
       */
      unsigned int high_vmware_version = inl(ioaddr + VMXNET_HIGH_VERSION);
      if ((VMXNET2_MAGIC < low_vmware_version) ||
          (VMXNET2_MAGIC > high_vmware_version)) {
         printk(KERN_ERR
                "Driver version 0x%08X doesn't match version 0x%08X, 0x%08X\n",
                VMXNET2_MAGIC, low_vmware_version, high_vmware_version);
         return FALSE;
      }
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_probe_features --
 *
 *      Helper to probe device features and capabilities and initialize various
 *      driver state.  Called from the .probe and .resume handlers.  During
 *      .resume phase this function validates that compatible
 *      feature/capabilities (or other state) is obtained from the device as
 *      during the .probe phase prior to guest suspend or hibernate.
 *
 * Results:
 *      Boolean indicating success/failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
vmxnet_probe_features(struct net_device *dev, // IN:
                      Bool morphed,           // IN:
                      Bool probeFromResume)   // IN:
{
   struct Vmxnet_Private *lp = netdev_priv(dev);
   unsigned int numRxBuffers, numRxBuffers2, maxNumRxBuffers, defNumRxBuffers;
   unsigned int numTxBuffers, maxNumTxBuffers, defNumTxBuffers;
   Bool enhanced = FALSE;
   int i;
   unsigned int ioaddr = dev->base_addr;

   VMXNET_ASSERT(lp);

   outl(VMXNET_CMD_GET_FEATURES, dev->base_addr + VMXNET_COMMAND_ADDR);
   if (probeFromResume) {
      if ((lp->features & inl(dev->base_addr + VMXNET_COMMAND_ADDR)) !=
          lp->features) {
         return FALSE;
      }
   } else {
      lp->features = inl(dev->base_addr + VMXNET_COMMAND_ADDR);
   }

   outl(VMXNET_CMD_GET_CAPABILITIES, dev->base_addr + VMXNET_COMMAND_ADDR);
   if (probeFromResume) {
      if ((lp->capabilities & inl(dev->base_addr + VMXNET_COMMAND_ADDR)) !=
          lp->capabilities) {
         return FALSE;
      }
   } else {
      lp->capabilities = inl(dev->base_addr + VMXNET_COMMAND_ADDR);
   }

   /* determine the features supported */
   lp->zeroCopyTx = FALSE;
   lp->partialHeaderCopyEnabled = FALSE;
   lp->tso = FALSE;
   lp->chainTx = FALSE;
   lp->chainRx = FALSE;
   lp->jumboFrame = FALSE;
   lp->lpd = FALSE;

   printk(KERN_INFO "features:");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
   if (lp->capabilities & VMNET_CAP_IP4_CSUM) {
      dev->features |= NETIF_F_HW_CSUM;
      printk(" ipCsum");
   }
   if (lp->capabilities & VMNET_CAP_HW_CSUM) {
      dev->features |= NETIF_F_HW_CSUM;
      printk(" hwCsum");
   }
#endif

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->capabilities & VMNET_CAP_SG &&
       lp->features & VMXNET_FEATURE_ZERO_COPY_TX){
      dev->features |= NETIF_F_SG;
      lp->zeroCopyTx = TRUE;
      printk(" zeroCopy");

      if (lp->capabilities & VMNET_CAP_ENABLE_HEADER_COPY) {
         lp->partialHeaderCopyEnabled = TRUE;
         printk(" partialHeaderCopy");
      }

      if (lp->capabilities & VMNET_CAP_TX_CHAIN) {
         lp->chainTx = TRUE;
      }

      if (lp->capabilities & VMNET_CAP_RX_CHAIN) {
         lp->chainRx = TRUE;
      }

      if (lp->chainRx && lp->chainTx &&
          (lp->features & VMXNET_FEATURE_JUMBO_FRAME)) {
         lp->jumboFrame = TRUE;
         printk(" jumboFrame");
      }
   }

#ifdef VMXNET_DO_TSO
   if ((lp->capabilities & VMNET_CAP_TSO) &&
       (lp->capabilities & (VMNET_CAP_IP4_CSUM | VMNET_CAP_HW_CSUM)) &&
       // tso only makes sense if we have hw csum offload
       lp->chainTx && lp->zeroCopyTx &&
       lp->features & VMXNET_FEATURE_TSO) {
      dev->features |= NETIF_F_TSO;
      lp->tso = TRUE;
      printk(" tso");
   }

   if ((lp->capabilities & VMNET_CAP_LPD) &&
       (lp->features & VMXNET_FEATURE_LPD) &&
       !disable_lro) {
      lp->lpd = TRUE;
      printk(" lpd");
   }
#endif
#endif

   printk("\n");

   /* check if this is enhanced vmxnet device */
   if ((lp->features & VMXNET_FEATURE_TSO) && 
       (lp->features & VMXNET_FEATURE_JUMBO_FRAME)) {
      enhanced = TRUE;
   }

   /* determine rx/tx ring sizes */ 
   if (enhanced) {
      maxNumRxBuffers = ENHANCED_VMXNET2_MAX_NUM_RX_BUFFERS;
      defNumRxBuffers = ENHANCED_VMXNET2_DEFAULT_NUM_RX_BUFFERS;
   } else {
      maxNumRxBuffers = VMXNET2_MAX_NUM_RX_BUFFERS;
      defNumRxBuffers = VMXNET2_DEFAULT_NUM_RX_BUFFERS;
   }

   outl(VMXNET_CMD_GET_NUM_RX_BUFFERS, dev->base_addr + VMXNET_COMMAND_ADDR);
   numRxBuffers = inl(dev->base_addr + VMXNET_COMMAND_ADDR);
   if (numRxBuffers == 0 || numRxBuffers > maxNumRxBuffers) {
      numRxBuffers = defNumRxBuffers;
   }

   if (lp->jumboFrame || lp->lpd) {
      numRxBuffers2 = numRxBuffers * 4;
      if (numRxBuffers2 > VMXNET2_MAX_NUM_RX_BUFFERS2) {
         numRxBuffers2 = VMXNET2_MAX_NUM_RX_BUFFERS2;
      }
   } else {
      numRxBuffers2 = 1;
   }

   printk(KERN_INFO "numRxBuffers = %d, numRxBuffers2 = %d\n", 
	  numRxBuffers, numRxBuffers2);
   if (lp->tso || lp->jumboFrame) {
      maxNumTxBuffers = VMXNET2_MAX_NUM_TX_BUFFERS_TSO;
      defNumTxBuffers = VMXNET2_DEFAULT_NUM_TX_BUFFERS_TSO;
   } else {
      maxNumTxBuffers = VMXNET2_MAX_NUM_TX_BUFFERS;
      defNumTxBuffers = VMXNET2_DEFAULT_NUM_TX_BUFFERS;
   }

   outl(VMXNET_CMD_GET_NUM_TX_BUFFERS, dev->base_addr + VMXNET_COMMAND_ADDR);
   numTxBuffers = inl(dev->base_addr + VMXNET_COMMAND_ADDR);
   if (numTxBuffers == 0 || numTxBuffers > maxNumTxBuffers) {
      numTxBuffers = defNumTxBuffers;
   }

   lp->ddSize = sizeof(Vmxnet2_DriverData) +
                   (numRxBuffers + numRxBuffers2) * sizeof(Vmxnet2_RxRingEntry) +
                   numTxBuffers * sizeof(Vmxnet2_TxRingEntry);
   VMXNET_LOG("vmxnet: numRxBuffers=((%d+%d)*%d) numTxBuffers=(%d*%d) ddSize=%d\n",
              numRxBuffers, numRxBuffers2, (uint32)sizeof(Vmxnet2_RxRingEntry),
              numTxBuffers, (uint32)sizeof(Vmxnet2_TxRingEntry),
              (int)lp->ddSize);
   if (!vmxnet_alloc_shared_mem(lp->pdev, lp->ddSize, (void **)&lp->dd, &lp->ddPA)) {
      printk(KERN_ERR "Unable to allocate memory for driver data\n");
      return FALSE;
   }
   memset(lp->dd, 0, lp->ddSize);
   spin_lock_init(&lp->txLock);
   lp->numRxBuffers = numRxBuffers;
   lp->numRxBuffers2 = numRxBuffers2;
   lp->numTxBuffers = numTxBuffers;
   /* So that the vmkernel can check it is compatible */
   lp->dd->magic = VMXNET2_MAGIC;
   lp->dd->length = lp->ddSize;
   lp->name = VMXNET_CHIP_NAME;

   /*
    * Store whether we are morphed so we can figure out how to
    * clean up when we unload.
    */
   lp->morphed = morphed;

   if (lp->capabilities & VMNET_CAP_VMXNET_APROM) {
      for (i = 0; i < ETH_ALEN; i++) {
         dev->dev_addr[i] = inb(ioaddr + VMXNET_APROM_ADDR + i);
      }
      for (i = 0; i < ETH_ALEN; i++) {
         outb(dev->dev_addr[i], ioaddr + VMXNET_MAC_ADDR + i);
      }
   } else {
      /*
       * Be backwards compatible and use the MAC address register to
       * get MAC address.
       */
      for (i = 0; i < ETH_ALEN; i++) {
         dev->dev_addr[i] = inb(ioaddr + VMXNET_MAC_ADDR + i);
      }
   }

#ifdef VMXNET_DO_ZERO_COPY
   lp->txBufferStart = NULL;
   lp->dd->txBufferPhysStart = 0;
   lp->dd->txBufferPhysLength = 0;

   if (lp->partialHeaderCopyEnabled) {
      lp->txBufferSize = numTxBuffers * TX_PKT_HEADER_SIZE;

      if (vmxnet_alloc_shared_mem(lp->pdev, lp->txBufferSize,
				  (void **)&lp->txBufferStart, &lp->txBufferPA)) {
	 lp->dd->txBufferPhysStart = (uint32)lp->txBufferPA;
         lp->dd->txBufferPhysLength = (uint32)lp->txBufferSize;
         lp->dd->txPktMaxSize = TX_PKT_HEADER_SIZE;
      } else {
         lp->partialHeaderCopyEnabled = FALSE;
         printk(KERN_INFO "failed to allocate tx buffer, disable partialHeaderCopy\n");
      }
   }
#endif

   return TRUE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_alloc_shared_mem --
 *
 *      Attempts to allocate dma-able memory that uses a 32-bit PA.
 *
 * Results:
 *      TRUE on success, otherwise FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#define FITS_IN_32_BITS(_x) ((_x) == ((_x) & 0xFFFFFFFF))

static Bool 
vmxnet_alloc_shared_mem(struct pci_dev *pdev, // IN:
			size_t size,          // IN:
			void **vaOut,         // OUT:
			dma_addr_t *paOut)    // OUT:
{
   void *va = NULL;
   dma_addr_t pa = 0;

   /* DMA-mapping.txt says 32-bit DMA by default */
   va = compat_pci_alloc_consistent(pdev, size, &pa);
   if (!va) {
      *vaOut = NULL;
      *paOut = 0;
      return FALSE;
   }

   VMXNET_ASSERT(FITS_IN_32_BITS(pa) &&
		 FITS_IN_32_BITS((uint64)pa + (size - 1)));
   *vaOut = va;
   *paOut = pa;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_remove_device --
 *
 *      Cleanup, called for each device on unload.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Unregisters vmxnet device with Linux and frees memory.
 *
 *-----------------------------------------------------------------------------
 */
static void
vmxnet_remove_device(struct pci_dev* pdev)
{
   struct net_device *dev = pci_get_drvdata(pdev);
   struct Vmxnet_Private *lp = netdev_priv(dev);

   /*
    * Do this before device is gone so we never call netif_carrier_* after
    * unregistering netdevice.
    */
   compat_del_timer_sync(&lp->linkCheckTimer);
   unregister_netdev(dev);

   /* Unmorph adapter if it was morphed. */

   if (lp->morphed) {
      vmxnet_unmorph_device(dev->base_addr - MORPH_PORT_SIZE);
      release_region(dev->base_addr -
                     (LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE),
                     VMXNET_CHIP_IO_RESV_SIZE +
                     (LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE));
   } else {
      release_region(dev->base_addr, VMXNET_CHIP_IO_RESV_SIZE);
   }

   vmxnet_release_private_data(lp, pdev);
   free_netdev(dev);
   pci_disable_device(pdev);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_release_private_data --
 *
 *      Helper to release/free some private driver data.  Called from the
 *      .remove handler and the .suspend handler.
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
vmxnet_release_private_data(Vmxnet_Private *lp,   // IN:
                            struct pci_dev *pdev) // IN:
{
#ifdef VMXNET_DO_ZERO_COPY
   if (lp->partialHeaderCopyEnabled && lp->txBufferStart) {
      compat_pci_free_consistent(pdev, lp->txBufferSize, 
				 lp->txBufferStart, lp->txBufferPA);
      lp->txBufferStart = NULL;
   }
#endif

   if (lp->dd) {
      compat_pci_free_consistent(pdev, lp->ddSize, lp->dd, lp->ddPA);
      lp->dd = NULL;
   }
}


#ifdef CONFIG_PM
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_suspend_device --
 *
 *      Suspend PM handler.
 *
 * Results:
 *      Returns 0 for success.
 *
 * Side effects:
 *      Morphs device back to lance mode.
 *
 *-----------------------------------------------------------------------------
 */

VMXNET_SUSPEND_DEVICE(/* struct pci_dev * */ pdev,  // IN: pci device
                      /* pm_message_t     */ state) // IN: state
{
   /*
    * Suspend needs to:
    * 1. Disable IRQ.
    * 2. Morph back to vlance.
    * 3. Disable bus-mastering.
    * 4. Put device to low power state.
    * 5. Enable wake up.
    *
    * TODO: implement 5.
    */

   struct net_device *dev = pci_get_drvdata(pdev);
   struct Vmxnet_Private *lp = netdev_priv(dev);

   if (lp->devOpen) {
      /*
       * Close the device first (and unmap rings, frees skbs, etc)
       * since morphing will reset the device. However, still
       * keep the device marked as "opened" so we can reopen it at
       * resume time.
       */
      vmxnet_close(dev);
      lp->devOpen = TRUE;
   }

   if (lp->morphed) {               /* Morph back to vlance. */
      vmxnet_unmorph_device(dev->base_addr - MORPH_PORT_SIZE);
   }

   pci_disable_device(pdev); /* Disables bus-mastering. */
   vmxnet_release_private_data(lp, pdev);

   VMXNET_SET_POWER_STATE(pdev, state);
   VMXNET_PM_RETURN(0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_resume_device --
 *
 *      Resume PM handler.
 *      TODO: check capability of the device on resume.
 *
 * Results:
 *      Returns 0 for success, an error otherwise.
 *
 * Side effects:
 *      Morphs device to vxmnet mode.
 *
 *-----------------------------------------------------------------------------
 */

VMXNET_RESUME_DEVICE(/* struct pci_dev* */ pdev) // IN: pci device
{
   struct net_device *dev = pci_get_drvdata(pdev);
   struct Vmxnet_Private *lp = netdev_priv(dev);
   int ret;

   ret = pci_enable_device(pdev); /* Does not enable bus-mastering. */
   if (ret) {
      printk(KERN_ERR "Cannot resume vmxnet adapter %s: error %d\n",
             pci_name(pdev), ret);
      VMXNET_PM_RETURN(ret);
   }

   pci_set_master(pdev);

   if (lp->morphed) {
      if (vmxnet_morph_device(dev->base_addr - MORPH_PORT_SIZE) != 0) {
         ret = -ENODEV;
         goto disable_pci;
      }
   }

   if (!vmxnet_check_version(dev->base_addr) ||
       !vmxnet_probe_features(dev, lp->morphed, TRUE)) {
      ret = -ENODEV;
      goto disable_pci;
   }

   if (lp->devOpen) {
      /*
       * The adapter was closed at suspend time. So mark it as closed, then
       * try to reopen it.
       */

      lp->devOpen = FALSE;
      ret = vmxnet_open(dev);
      if (ret) {
         /*
          * We do not unmorph the device here since that would be handled in
          * the .remove handler.
          */

         printk(KERN_ERR "Could not open vmxnet adapter %s: error %d\n",
                pci_name(pdev), ret);
         goto disable_pci;
      }
   }

   VMXNET_SET_POWER_STATE_D0(pdev);
   VMXNET_PM_RETURN(0);

disable_pci:
   pci_disable_device(pdev); /* Disables bus-mastering. */
   VMXNET_PM_RETURN(ret);
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_init_ring --
 *
 *      Initializes buffer rings in Vmxnet_Private structure.  Allocates skbs
 *      to receive into.  Called by vmxnet_open.
 *
 * Results:
 *      0 on success; -1 on failure to allocate skbs.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_init_ring(struct net_device *dev)
{
   struct Vmxnet_Private *lp = netdev_priv(dev);
   Vmxnet2_DriverData *dd = lp->dd;
   unsigned int i;
   size_t offset;

   offset = sizeof(*dd);

   dd->rxRingLength = lp->numRxBuffers;
   dd->rxRingOffset = offset;
   lp->rxRing = (Vmxnet2_RxRingEntry *)((uintptr_t)dd + offset);
   offset += lp->numRxBuffers * sizeof(Vmxnet2_RxRingEntry);

   dd->rxRingLength2 = lp->numRxBuffers2;
   dd->rxRingOffset2 = offset;
   lp->rxRing2 = (Vmxnet2_RxRingEntry *)((uintptr_t)dd + offset);
   offset += lp->numRxBuffers2 * sizeof(Vmxnet2_RxRingEntry);

   dd->txRingLength = lp->numTxBuffers;
   dd->txRingOffset = offset;
   lp->txRing = (Vmxnet2_TxRingEntry *)((uintptr_t)dd + offset);
   offset += lp->numTxBuffers * sizeof(Vmxnet2_TxRingEntry);

   VMXNET_LOG("vmxnet_init_ring: offset=%"FMT64"d length=%d\n",
              (uint64)offset, dd->length);

   for (i = 0; i < lp->numRxBuffers; i++) {
      lp->rxSkbuff[i] = dev_alloc_skb(PKT_BUF_SZ + COMPAT_NET_IP_ALIGN);
      if (lp->rxSkbuff[i] == NULL) {
         unsigned int j;

	 printk (KERN_ERR "%s: vmxnet_init_ring dev_alloc_skb failed.\n", dev->name);
         for (j = 0; j < i; j++) {
            compat_dev_kfree_skb(lp->rxSkbuff[j], FREE_WRITE);
            lp->rxSkbuff[j] = NULL;
         }
	 return -ENOMEM;
      }

      skb_reserve(lp->rxSkbuff[i], COMPAT_NET_IP_ALIGN);

      lp->rxRing[i].paddr = compat_pci_map_single(lp->pdev, lp->rxSkbuff[i]->data,
                                                  PKT_BUF_SZ, PCI_DMA_FROMDEVICE);
      lp->rxRing[i].bufferLength = PKT_BUF_SZ;
      lp->rxRing[i].actualLength = 0;
      lp->rxRing[i].ownership = VMXNET2_OWNERSHIP_NIC;
   }

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->jumboFrame || lp->lpd) {
      struct pci_dev *pdev = lp->pdev;

      dd->maxFrags = MAX_SKB_FRAGS;

      for (i = 0; i < lp->numRxBuffers2; i++) {
         lp->rxPages[i] = alloc_page(GFP_KERNEL);
         if (lp->rxPages[i] == NULL) {
            unsigned int j;

            printk (KERN_ERR "%s: vmxnet_init_ring alloc_page failed.\n", dev->name);
            for (j = 0; j < i; j++) {
               put_page(lp->rxPages[j]);
               lp->rxPages[j] = NULL;
            }
            for (j = 0; j < lp->numRxBuffers; j++) {
               compat_dev_kfree_skb(lp->rxSkbuff[j], FREE_WRITE);
               lp->rxSkbuff[j] = NULL;
            }
            return -ENOMEM;
         }

         lp->rxRing2[i].paddr = pci_map_page(pdev, lp->rxPages[i], 0,
                                             PAGE_SIZE, PCI_DMA_FROMDEVICE);
         lp->rxRing2[i].bufferLength = PAGE_SIZE;
         lp->rxRing2[i].actualLength = 0;
         lp->rxRing2[i].ownership = VMXNET2_OWNERSHIP_NIC_FRAG;
      }
   } else
#endif
   {
      // dummy rxRing2 tacked on to the end, with a single unusable entry
      lp->rxRing2[0].paddr = 0;
      lp->rxRing2[0].bufferLength = 0;
      lp->rxRing2[0].actualLength = 0;
      lp->rxRing2[0].ownership = VMXNET2_OWNERSHIP_DRIVER;
   }

   dd->rxDriverNext = 0;
   dd->rxDriverNext2 = 0;

   for (i = 0; i < lp->numTxBuffers; i++) {
      lp->txRing[i].ownership = VMXNET2_OWNERSHIP_DRIVER;
      lp->txBufInfo[i].skb = NULL;
      lp->txBufInfo[i].eop = 0;
      lp->txRing[i].sg.sg[0].addrHi = 0;
      lp->txRing[i].sg.addrType = NET_SG_PHYS_ADDR;
   }

   dd->txDriverCur = dd->txDriverNext = 0;
   dd->savedRxNICNext = dd->savedRxNICNext2 = dd->savedTxNICNext = 0;
   dd->txStopped = FALSE;

   if (lp->lpd) {
      dd->featureCtl |= VMXNET_FEATURE_LPD;
   }

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_open --
 *
 *      Network device open routine.  Called by Linux when the interface is
 *      brought up.
 *
 * Results:
 *      0 on success; else negative errno value.
 *
 * Side effects:
 *      Allocates an IRQ if not already allocated.  Sets our Vmxnet_Private
 *      structure to be the shared area with the lower layer.
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_open(struct net_device *dev)
{
   struct Vmxnet_Private *lp = netdev_priv(dev);
   unsigned int ioaddr = dev->base_addr;
   uint32 ddPA;

   /*
    * The .suspend handler frees driver data, so we need this check.
    */
   if (UNLIKELY(!lp->dd)) {
      return -ENOMEM;
   }

   if (dev->irq == 0 ||	request_irq(dev->irq, &vmxnet_interrupt,
			            COMPAT_IRQF_SHARED, dev->name, (void *)dev)) {
      return -EAGAIN;
   }

   if (vmxnet_debug > 1) {
      printk(KERN_DEBUG "%s: vmxnet_open() irq %d lp %p.\n",
	     dev->name, dev->irq, lp);
   }

   if (vmxnet_init_ring(dev)) {
      return -ENOMEM;
   }

   ddPA = VMXNET_GET_LO_ADDR(lp->ddPA);
   outl(ddPA, ioaddr + VMXNET_INIT_ADDR);
   outl(lp->dd->length, ioaddr + VMXNET_INIT_LENGTH);

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->partialHeaderCopyEnabled) {
      outl(VMXNET_CMD_PIN_TX_BUFFERS, ioaddr + VMXNET_COMMAND_ADDR);
   }
   // Pin the Tx buffers if partial header copy is enabled
#endif

   lp->dd->txStopped = FALSE;
   netif_start_queue(dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   dev->interrupt = 0;
   dev->start = 1;
#endif

   lp->devOpen = TRUE;

   COMPAT_NETDEV_MOD_INC_USE_COUNT;

   return 0;
}

#ifdef VMXNET_DO_ZERO_COPY
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_unmap_buf  --
 *
 *      Unmap the PAs of the tx entry that we pinned for DMA.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *-----------------------------------------------------------------------------
 */

void
vmxnet_unmap_buf(struct sk_buff *skb,
                 struct Vmxnet2_TxBuf *tb,
                 Vmxnet2_TxRingEntry *xre,
                 struct pci_dev *pdev)
{
   int sgIdx;

   // unmap the mapping for skb->data if needed
   if (tb->sgForLinear >= 0) {
      pci_unmap_single(pdev,
                       VMXNET_GET_DMA_ADDR(xre->sg.sg[(int)tb->sgForLinear]),
                       xre->sg.sg[(int)tb->sgForLinear].length,
                       PCI_DMA_TODEVICE);
      VMXNET_LOG("vmxnet_unmap_buf: sg[%d] (%uB)\n", (int)tb->sgForLinear,
                 xre->sg.sg[(int)tb->sgForLinear].length);
   }

   // unmap the mapping for skb->frags[]
   for (sgIdx = tb->firstSgForFrag; sgIdx < xre->sg.length; sgIdx++) {
      pci_unmap_page(pdev,
                     VMXNET_GET_DMA_ADDR(xre->sg.sg[sgIdx]),
                     xre->sg.sg[sgIdx].length,
                     PCI_DMA_TODEVICE);
      VMXNET_LOG("vmxnet_unmap_buf: sg[%d] (%uB)\n", sgIdx,
                      xre->sg.sg[sgIdx].length);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_map_pkt  --
 *
 *      Map the buffers/pages that we need for DMA and populate the SG.
 *
 *      "offset" indicates the position inside the pkt where mapping should start.
 *      "startSgIdx" indicates the first free sg slot of the first tx entry
 *      (pointed to by txDriverNext).
 *
 *      The caller should guarantee the first tx has at least one sg slot
 *      available. The caller should also ensure that enough tx entries are
 *      available for this pkt.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      1. Ownership of all tx entries used (EXCEPT the 1st one) are updated.
 *         The only flag set is VMXNET2_TX_MORE if needed. caller is
 *         responsible to set up other flags after this call returns.
 *      2. lp->dd->numTxPending is updated
 *      3. txBufInfo corresponding to used tx entries (including the 1st one)
 *         are updated
 *      4. txDriverNext is advanced accordingly
 *
 *-----------------------------------------------------------------------------
 */

void
vmxnet_map_pkt(struct sk_buff *skb,
               int offset,
               struct Vmxnet_Private *lp,
               int startSgIdx)
{
   int nextFrag = 0, nextSg = startSgIdx;
   struct skb_frag_struct *frag;
   Vmxnet2_DriverData *dd = lp->dd;
   Vmxnet2_TxRingEntry *xre;
   struct Vmxnet2_TxBuf *tb;
   dma_addr_t dma;

   VMXNET_ASSERT(startSgIdx < VMXNET2_SG_DEFAULT_LENGTH);

   lp->numTxPending++;
   tb = &lp->txBufInfo[dd->txDriverNext];
   xre = &lp->txRing[dd->txDriverNext];

   if (offset == skb_headlen(skb)) {
      tb->sgForLinear = -1;
      tb->firstSgForFrag = nextSg;
   } else if (offset < skb_headlen(skb)) {
      /* we need to map some of the non-frag data. */
      dma = pci_map_single(lp->pdev,
                           skb->data + offset,
                           skb_headlen(skb) - offset,
                           PCI_DMA_TODEVICE);
      VMXNET_FILL_SG(xre->sg.sg[nextSg], dma, skb_headlen(skb) - offset);
      VMXNET_LOG("vmxnet_map_pkt: txRing[%u].sg[%d] -> data %p offset %u size %u\n",
                 dd->txDriverNext, nextSg, skb->data, offset, skb_headlen(skb) - offset);
      tb->sgForLinear = nextSg++;
      tb->firstSgForFrag = nextSg;
   } else {
      // all non-frag data is copied, skip it
      tb->sgForLinear = -1;
      tb->firstSgForFrag = nextSg;

      offset -= skb_headlen(skb);

      for ( ; nextFrag < skb_shinfo(skb)->nr_frags; nextFrag++){
         int fragSize;
         frag = &skb_shinfo(skb)->frags[nextFrag];
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
         fragSize = frag->size;
#else
         fragSize = skb_frag_size(frag);
#endif

         // skip those frags that are completely copied
         if (offset >= fragSize){
            offset -= fragSize;
         } else {
            // map the part of the frag that is not copied
            dma = pci_map_page(lp->pdev,
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
                               frag->page,
#else
                               frag->page.p,
#endif
                               frag->page_offset + offset,
                               fragSize - offset,
                               PCI_DMA_TODEVICE);
            VMXNET_FILL_SG(xre->sg.sg[nextSg], dma, fragSize - offset);
            VMXNET_LOG("vmxnet_map_tx: txRing[%u].sg[%d] -> frag[%d]+%u (%uB)\n",
                       dd->txDriverNext, nextSg, nextFrag, offset, fragSize - offset);
            nextSg++;
            nextFrag++;

            break;
         }
      }
   }

   // map the remaining frags, we might need to use additional tx entries
   for ( ; nextFrag < skb_shinfo(skb)->nr_frags; nextFrag++) {
      int fragSize;
      frag = &skb_shinfo(skb)->frags[nextFrag];
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
      fragSize = frag->size;
#else
      fragSize = skb_frag_size(frag);
#endif

      dma = pci_map_page(lp->pdev,
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
                         frag->page,
#else
                         frag->page.p,
#endif
                         frag->page_offset,
                         fragSize,
                         PCI_DMA_TODEVICE);

      if (nextSg == VMXNET2_SG_DEFAULT_LENGTH) {
         xre->flags = VMXNET2_TX_MORE;
         xre->sg.length = VMXNET2_SG_DEFAULT_LENGTH;
         tb->skb = skb;
         tb->eop = 0;

         // move to the next tx entry
         VMXNET_INC(dd->txDriverNext, dd->txRingLength);
         xre = &lp->txRing[dd->txDriverNext];
         tb = &lp->txBufInfo[dd->txDriverNext];

         // the new tx entry must be available
         VMXNET_ASSERT(xre->ownership == VMXNET2_OWNERSHIP_DRIVER && tb->skb == NULL);

         /*
          * we change it even before the sg are populated but this is
          * fine, because the first tx entry's ownership is not
          * changed yet
          */
         xre->ownership = VMXNET2_OWNERSHIP_NIC;
         tb->sgForLinear = -1;
         tb->firstSgForFrag = 0;
         lp->numTxPending ++;

         nextSg = 0;
      }
      VMXNET_FILL_SG(xre->sg.sg[nextSg], dma, fragSize);
      VMXNET_LOG("vmxnet_map_tx: txRing[%u].sg[%d] -> frag[%d] (%uB)\n",
                 dd->txDriverNext, nextSg, nextFrag, fragSize);
      nextSg++;
   }

   // setup the last tx entry
   xre->flags = 0;
   xre->sg.length = nextSg;
   tb->skb = skb;
   tb->eop = 1;

   VMXNET_ASSERT(nextSg <= VMXNET2_SG_DEFAULT_LENGTH);
   VMXNET_INC(dd->txDriverNext, dd->txRingLength);
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * check_tx_queue --
 *
 *      Loop through the tx ring looking for completed transmits.
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
check_tx_queue(struct net_device *dev)
{
   struct Vmxnet_Private *lp = netdev_priv(dev);
   Vmxnet2_DriverData *dd = lp->dd;
   int completed = 0;

   /*
    * The .suspend handler frees driver data, so we need this check.
    */
   while (dd) {
      Vmxnet2_TxRingEntry *xre = &lp->txRing[dd->txDriverCur];
      struct sk_buff *skb = lp->txBufInfo[dd->txDriverCur].skb;

      if (xre->ownership != VMXNET2_OWNERSHIP_DRIVER || skb == NULL) {
	 break;
      }
#ifdef VMXNET_DO_ZERO_COPY
      if (lp->zeroCopyTx) {
         VMXNET_LOG("unmap txRing[%u]\n", dd->txDriverCur);
         vmxnet_unmap_buf(skb, &lp->txBufInfo[dd->txDriverCur], xre, lp->pdev);
      } else
#endif
      {
         compat_pci_unmap_single(lp->pdev, xre->sg.sg[0].addrLow,
                                 xre->sg.sg[0].length,
                                 PCI_DMA_TODEVICE);
      }
      if (lp->txBufInfo[dd->txDriverCur].eop) {
         compat_dev_kfree_skb_irq(skb, FREE_WRITE);
      }
      lp->txBufInfo[dd->txDriverCur].skb = NULL;

      completed ++;

      VMXNET_INC(dd->txDriverCur, dd->txRingLength);
   }

   if (completed){
      lp->numTxPending -= completed;

      // XXX conditionally wake up the queue based on the # of freed entries
      if (netif_queue_stopped(dev)) {
         netif_wake_queue(dev);
         dd->txStopped = FALSE;
      }
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_tx --
 *
 *      Network device hard_start_xmit helper routine.  This is called by
 *	the drivers hard_start_xmit routine when it wants to send a packet.
 *
 * Results:
 *      VMXNET_CALL_TRANSMIT:	The driver should ask the virtual NIC to
 *				transmit a packet.
 *      VMXNET_DEFER_TRANSMIT:	This transmit is deferred because of
 *				transmit clustering.
 *      VMXNET_STOP_TRANSMIT:	We ran out of queue space so the caller
 *				should stop transmitting.
 *
 * Side effects:
 *	The drivers tx ring may get modified.
 *
 *-----------------------------------------------------------------------------
 */
Vmxnet_TxStatus
vmxnet_tx(struct sk_buff *skb, struct net_device *dev)
{
   Vmxnet_TxStatus status = VMXNET_DEFER_TRANSMIT;
   struct Vmxnet_Private *lp = netdev_priv(dev);
   Vmxnet2_DriverData *dd = lp->dd;
   unsigned long flags;
   Vmxnet2_TxRingEntry *xre;
#ifdef VMXNET_DO_TSO
   int mss;
#endif

   /*
    * The .suspend handler frees driver data, so we need this check.
    */
   if (UNLIKELY(!dd)) {
      return VMXNET_STOP_TRANSMIT;
   }

   xre = &lp->txRing[dd->txDriverNext];

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->zeroCopyTx) {
      int txEntries, sgCount;
      unsigned int headerSize;

      /* conservatively estimate the # of tx entries needed in the worse case */
      sgCount = (lp->partialHeaderCopyEnabled ? 2 : 1) + skb_shinfo(skb)->nr_frags;
      txEntries = (sgCount + VMXNET2_SG_DEFAULT_LENGTH - 1) / VMXNET2_SG_DEFAULT_LENGTH;

      if (UNLIKELY(!lp->chainTx && txEntries > 1)) {
         /*
          * rare case, no tx desc chaining support but the pkt need more than 1
          * tx entry, linearize it
          */
         if (compat_skb_linearize(skb) != 0) {
            VMXNET_LOG("vmxnet_tx: skb_linearize failed\n");
            compat_dev_kfree_skb(skb, FREE_WRITE);
            return VMXNET_DEFER_TRANSMIT;
         }

         txEntries = 1;
      }

      VMXNET_LOG("\n%d(%d) bytes, %d frags, %d tx entries\n", skb->len,
                 skb_headlen(skb), skb_shinfo(skb)->nr_frags, txEntries);

      spin_lock_irqsave(&lp->txLock, flags);

      /* check for the availability of tx ring entries */
      if (dd->txRingLength - lp->numTxPending < txEntries) {
         dd->txStopped = TRUE;
         netif_stop_queue(dev);
         check_tx_queue(dev);

         spin_unlock_irqrestore(&lp->txLock, flags);
         VMXNET_LOG("queue stopped\n");
         return VMXNET_STOP_TRANSMIT;
      }

      /* copy protocol headers if needed */
      if (LIKELY(lp->partialHeaderCopyEnabled)) {
         unsigned int pos = dd->txDriverNext * dd->txPktMaxSize;
         char *header = lp->txBufferStart + pos;

         /* figure out the protocol and header sizes */

         /* PR 171928
          * compat_skb_ip_header isn't updated in rhel5 for
          * vlan tagging.  using these macros causes incorrect
          * computation of the headerSize
          */
         headerSize = ETHERNET_HEADER_SIZE;
         if (UNLIKELY((skb_headlen(skb) < headerSize))) {

            if (skb_is_nonlinear(skb)) {
               compat_skb_linearize(skb);
            }
            /*
             * drop here if we don't have a complete ETH header for delivery
             */
            if (skb_headlen(skb) < headerSize) {
               compat_dev_kfree_skb(skb, FREE_WRITE);
               spin_unlock_irqrestore(&lp->txLock, flags);
               return VMXNET_DEFER_TRANSMIT;
            }
         }
         if (UNLIKELY(*(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION) ==
                      ETH_TYPE_VLAN_TAG)) {
            headerSize += VLAN_TAG_LENGTH;
            if (UNLIKELY(skb_headlen(skb) < headerSize)) {

               if (skb_is_nonlinear(skb)) {
                  compat_skb_linearize(skb);
               }
               /*
                * drop here if we don't have a ETH header and a complete VLAN tag
                */
               if (skb_headlen(skb) < headerSize) {
                  compat_dev_kfree_skb(skb, FREE_WRITE);
                  spin_unlock_irqrestore(&lp->txLock, flags);
                  return VMXNET_DEFER_TRANSMIT;
               }
            }
         }
         if (LIKELY(PKT_OF_IPV4(skb))){
            // PR 171928 -- compat_skb_ip_header broken with vconfig
            // please do not rewrite using compat_skb_ip_header
            struct iphdr *ipHdr = (struct iphdr *)(skb->data + headerSize);

            if (UNLIKELY(skb_headlen(skb) < headerSize + sizeof(*ipHdr))) {

               if (skb_is_nonlinear(skb)) {
                    compat_skb_linearize(skb);
               }
            }
            if (LIKELY(skb_headlen(skb) > headerSize + sizeof(*ipHdr)) &&
               (LIKELY(ipHdr->version == 4))) {
               headerSize += ipHdr->ihl << 2;
               if (LIKELY(ipHdr->protocol == IPPROTO_TCP)) {
                  /*
                   * tcp traffic, copy all protocol headers
                   * refrain from using compat_skb macros PR 171928
                   */
                  struct tcphdr *tcpHdr = (struct tcphdr *)
                     (skb->data + headerSize);
                  /*
                   * tcp->doff is near the end of the tcpHdr, use the
                   * entire struct as the required size
                   */
                  if (skb->len < headerSize + sizeof(*tcpHdr)) {
                     compat_dev_kfree_skb(skb, FREE_WRITE);
                     spin_unlock_irqrestore(&lp->txLock, flags);
                     return VMXNET_DEFER_TRANSMIT;
                  }
                  if (skb_headlen(skb) < (headerSize + sizeof(*tcpHdr))) {
                     /*
                      * linearized portion of the skb doesn't have a tcp header
                      */
                     compat_skb_linearize(skb);
                  }
                  headerSize += tcpHdr->doff << 2;
               }
            }
         }

         if (skb_copy_bits(skb, 0, header, headerSize) != 0) {
            compat_dev_kfree_skb(skb, FREE_WRITE);
            spin_unlock_irqrestore(&lp->txLock, flags);
            return VMXNET_DEFER_TRANSMIT;
         }

         xre->sg.sg[0].addrLow = dd->txBufferPhysStart + pos;
         xre->sg.sg[0].addrHi = 0;
         xre->sg.sg[0].length = headerSize;
         vmxnet_map_pkt(skb, headerSize, lp, 1);
      } else {
         headerSize = 0;
         vmxnet_map_pkt(skb, 0, lp, 0);
      }

#ifdef VMXNET_DO_TSO
      mss = VMXNET_SKB_MSS(skb);
      if (mss) {
         xre->flags |= VMXNET2_TX_TSO;
         xre->tsoMss = mss;
         dd->txNumDeferred += ((skb->len - headerSize) + mss - 1) / mss;
      } else
#endif
      {
         dd->txNumDeferred++;
      }
   } else /* zero copy not enabled */
#endif
   {
      struct Vmxnet2_TxBuf *tb;
      dma_addr_t dmaAddr;

      spin_lock_irqsave(&lp->txLock, flags);

      if (lp->txBufInfo[dd->txDriverNext].skb != NULL) {
         dd->txStopped = TRUE;
         netif_stop_queue(dev);
         check_tx_queue(dev);

         spin_unlock_irqrestore(&lp->txLock, flags);
         return VMXNET_STOP_TRANSMIT;
      }

      lp->numTxPending ++;

      dmaAddr = compat_pci_map_single(lp->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);
      VMXNET_FILL_SG(xre->sg.sg[0], dmaAddr, skb->len);
      xre->sg.length = 1;
      xre->flags = 0;

      tb = &lp->txBufInfo[dd->txDriverNext];
      tb->skb = skb;
      tb->sgForLinear = -1;
      tb->firstSgForFrag = -1;
      tb->eop = 1;

      VMXNET_INC(dd->txDriverNext, dd->txRingLength);
      dd->txNumDeferred++;
      dd->stats.copyTransmits++;
   }

   /* at this point, xre must point to the 1st tx entry for the pkt */
   if (skb->ip_summed == VM_TX_CHECKSUM_PARTIAL &&
       ((dev->features & NETIF_F_HW_CSUM) != 0)) {
      xre->flags |= VMXNET2_TX_HW_XSUM | VMXNET2_TX_CAN_KEEP;
   } else {
      xre->flags |= VMXNET2_TX_CAN_KEEP;
   }
   if (lp->numTxPending > dd->txRingLength - 5) {
      xre->flags |= VMXNET2_TX_RING_LOW;
      status = VMXNET_CALL_TRANSMIT;
   }

   wmb();
   xre->ownership = VMXNET2_OWNERSHIP_NIC;

   if (dd->txNumDeferred >= dd->txClusterLength) {
      dd->txNumDeferred = 0;
      status = VMXNET_CALL_TRANSMIT;
   }

   dev->trans_start = jiffies;

   lp->stats.tx_packets++;
   dd->stats.pktsTransmitted++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
   lp->stats.tx_bytes += skb->len;
#endif

   if (lp->numTxPending > dd->stats.maxTxsPending) {
      dd->stats.maxTxsPending = lp->numTxPending;
   }

   check_tx_queue(dev);

   spin_unlock_irqrestore(&lp->txLock, flags);

   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_start_tx --
 *
 *      Network device hard_start_xmit routine.  Called by Linux when it has
 *      a packet for us to transmit.
 *
 * Results:
 *      0 on success; 1 if no resources.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_start_tx(struct sk_buff *skb, struct net_device *dev)
{
   int retVal = 0;
   Vmxnet_TxStatus xs = vmxnet_tx(skb, dev);
   switch (xs) {
   case VMXNET_CALL_TRANSMIT:
      inl(dev->base_addr + VMXNET_TX_ADDR);
      break;
   case VMXNET_DEFER_TRANSMIT:
      break;
   case VMXNET_STOP_TRANSMIT:
      retVal = 1;
      break;
   }

   return retVal;
}

#ifdef VMXNET_DO_ZERO_COPY
/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_drop_frags --
 *
 *    return the entries in the 2nd ring to the hw. The entries returned are
 *    from rxDriverNext2 to the entry with VMXNET2_RX_FRAG_EOP set.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static void
vmxnet_drop_frags(Vmxnet_Private *lp)
{
   Vmxnet2_DriverData *dd = lp->dd;
   Vmxnet2_RxRingEntry *rre2;
   uint16 flags;

   do {
      rre2 = &lp->rxRing2[dd->rxDriverNext2];
      flags = rre2->flags;
      VMXNET_ASSERT(rre2->ownership == VMXNET2_OWNERSHIP_DRIVER_FRAG);

      rre2->ownership = VMXNET2_OWNERSHIP_NIC_FRAG;
      VMXNET_INC(dd->rxDriverNext2, dd->rxRingLength2);
   }  while(!(flags & VMXNET2_RX_FRAG_EOP));
}

/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_rx_frags --
 *
 *    get data from the 2nd rx ring and append the frags to the skb. Multiple
 *    rx entries in the 2nd rx ring are processed until the one with
 *    VMXNET2_RX_FRAG_EOP set.
 *
 * Result:
 *    0 on success
 *    -1 on error
 *
 * Side-effects:
 *    frags are appended to skb. related fields in skb are updated
 *
 *----------------------------------------------------------------------------
 */
static int
vmxnet_rx_frags(Vmxnet_Private *lp, struct sk_buff *skb)
{
   Vmxnet2_DriverData *dd = lp->dd;
   struct pci_dev *pdev = lp->pdev;
   struct page *newPage;
   int numFrags = 0;
   Vmxnet2_RxRingEntry *rre2;
   uint16 flags;
#ifdef VMXNET_DEBUG
   uint32 firstFrag = dd->rxDriverNext2;
#endif

   do {
      rre2 = &lp->rxRing2[dd->rxDriverNext2];
      flags = rre2->flags;
      VMXNET_ASSERT(rre2->ownership == VMXNET2_OWNERSHIP_DRIVER_FRAG);

      if (rre2->actualLength > 0) {
         newPage = alloc_page(GFP_ATOMIC);
         if (UNLIKELY(newPage == NULL)) {
            skb_shinfo(skb)->nr_frags = numFrags;
            skb->len += skb->data_len;
            skb->truesize += PAGE_SIZE;

            compat_dev_kfree_skb(skb, FREE_WRITE);

            vmxnet_drop_frags(lp);

            return -1;
         }

         pci_unmap_page(pdev, rre2->paddr, PAGE_SIZE, PCI_DMA_FROMDEVICE);
#if COMPAT_LINUX_VERSION_CHECK_LT(3, 2, 0)
         skb_shinfo(skb)->frags[numFrags].page = lp->rxPages[dd->rxDriverNext2];
#else
         __skb_frag_set_page(&skb_shinfo(skb)->frags[numFrags],
                             lp->rxPages[dd->rxDriverNext2]);
#endif
         skb_shinfo(skb)->frags[numFrags].page_offset = 0;
         skb_shinfo(skb)->frags[numFrags].size = rre2->actualLength;
         skb->data_len += rre2->actualLength;
         skb->truesize += PAGE_SIZE;
         numFrags++;

         /* refill the buffer */
         lp->rxPages[dd->rxDriverNext2] = newPage;
         rre2->paddr = pci_map_page(pdev, newPage, 0, PAGE_SIZE, PCI_DMA_FROMDEVICE);
         rre2->bufferLength = PAGE_SIZE;
         rre2->actualLength = 0;
         wmb();
      }

      rre2->ownership = VMXNET2_OWNERSHIP_NIC_FRAG;
      VMXNET_INC(dd->rxDriverNext2, dd->rxRingLength2);
   } while (!(flags & VMXNET2_RX_FRAG_EOP));

   VMXNET_ASSERT(numFrags > 0);
   skb_shinfo(skb)->nr_frags = numFrags;
   skb->len += skb->data_len;
   skb->truesize += PAGE_SIZE;
   VMXNET_LOG("vmxnet_rx: %dB from rxRing[%d](%dB)+rxRing2[%d, %d)(%dB)\n",
              skb->len, dd->rxDriverNext, skb_headlen(skb),
              firstFrag, dd->rxDriverNext2, skb->data_len);
   return 0;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_rx --
 *
 *      Receive a packet.
 *
 * Results:
 *      0
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_rx(struct net_device *dev)
{
   struct Vmxnet_Private *lp = netdev_priv(dev);
   Vmxnet2_DriverData *dd = lp->dd;

   if (!lp->devOpen || !dd) {
      return 0;
   }

   while (1) {
      struct sk_buff *skb, *newSkb;
      Vmxnet2_RxRingEntry *rre;

      rre = &lp->rxRing[dd->rxDriverNext];
      if (rre->ownership != VMXNET2_OWNERSHIP_DRIVER) {
	 break;
      }

      if (UNLIKELY(rre->actualLength == 0)) {
#ifdef VMXNET_DO_ZERO_COPY
         if (rre->flags & VMXNET2_RX_WITH_FRAG) {
            vmxnet_drop_frags(lp);
         }
#endif
         lp->stats.rx_errors++;
         goto next_pkt;
      }

      skb = lp->rxSkbuff[dd->rxDriverNext];

      /* refill the rx ring */
      newSkb = dev_alloc_skb(PKT_BUF_SZ + COMPAT_NET_IP_ALIGN);
      if (UNLIKELY(newSkb == NULL)) {
         printk(KERN_DEBUG "%s: Memory squeeze, dropping packet.\n", dev->name);
#ifdef VMXNET_DO_ZERO_COPY
         if (rre->flags & VMXNET2_RX_WITH_FRAG) {
            vmxnet_drop_frags(lp);
         }
#endif
         lp->stats.rx_errors++;
         goto next_pkt;
      }
      skb_reserve(newSkb, COMPAT_NET_IP_ALIGN);

      compat_pci_unmap_single(lp->pdev, rre->paddr, PKT_BUF_SZ, PCI_DMA_FROMDEVICE);
      skb_put(skb, rre->actualLength);

      lp->rxSkbuff[dd->rxDriverNext] = newSkb;
      rre->paddr = compat_pci_map_single(lp->pdev, newSkb->data,
                                         PKT_BUF_SZ, PCI_DMA_FROMDEVICE);
      rre->bufferLength = PKT_BUF_SZ;


#ifdef VMXNET_DO_ZERO_COPY
      if (rre->flags & VMXNET2_RX_WITH_FRAG) {
         if (vmxnet_rx_frags(lp, skb) < 0) {
            lp->stats.rx_errors++;
            goto next_pkt;
         }
      } else
#endif
      {
         VMXNET_LOG("vmxnet_rx: %dB from rxRing[%d]\n", skb->len, dd->rxDriverNext);
      }

      if (skb->len < (ETH_MIN_FRAME_LEN - 4)) {
         /*
          * Ethernet header vlan tags are 4 bytes.  Some vendors generate
          *  ETH_MIN_FRAME_LEN frames including vlan tags.  When vlan tag
          *  is stripped, such frames become ETH_MIN_FRAME_LEN - 4. (PR106153)
          */
         if (skb->len != 0) {
	    printk(KERN_DEBUG "%s: Runt pkt (%d bytes) entry %d!\n", dev->name,
                   skb->len, dd->rxDriverNext);
         }
	 lp->stats.rx_errors++;
      } else {
         if (rre->flags & VMXNET2_RX_HW_XSUM_OK) {
            skb->ip_summed = CHECKSUM_UNNECESSARY;
         }

         skb->dev = dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
         lp->stats.rx_bytes += skb->len;
#endif
         skb->protocol = eth_type_trans(skb, dev);
         netif_rx(skb);
         lp->stats.rx_packets++;
         dd->stats.pktsReceived++;
      }

next_pkt:
      rre->ownership = VMXNET2_OWNERSHIP_NIC;
      VMXNET_INC(dd->rxDriverNext, dd->rxRingLength);
   }

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_interrupt --
 *
 *      Interrupt handler.  Calls vmxnet_rx to receive a packet.
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
vmxnet_interrupt(int irq, void *dev_id, struct pt_regs * regs)
#else
static compat_irqreturn_t
vmxnet_interrupt(int irq, void *dev_id)
#endif
{
   struct net_device *dev = (struct net_device *)dev_id;
   struct Vmxnet_Private *lp;
   Vmxnet2_DriverData *dd;

   if (dev == NULL) {
      printk (KERN_DEBUG "vmxnet_interrupt(): irq %d for unknown device.\n", irq);
      return COMPAT_IRQ_NONE;
   }


   lp = netdev_priv(dev);
   outl(VMXNET_CMD_INTR_ACK, dev->base_addr + VMXNET_COMMAND_ADDR);

   dd = lp->dd;
   if (LIKELY(dd)) {
      dd->stats.interrupts++;
   }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   if (dev->interrupt) {
      printk(KERN_DEBUG "%s: Re-entering the interrupt handler.\n", dev->name);
   }
   dev->interrupt = 1;
#endif

   vmxnet_rx(dev);

   if (lp->numTxPending > 0) {
      spin_lock(&lp->txLock);
      check_tx_queue(dev);
      spin_unlock(&lp->txLock);
   }

   if (netif_queue_stopped(dev) && dd && !dd->txStopped) {
      netif_wake_queue(dev);
   }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   dev->interrupt = 0;
#endif
   return COMPAT_IRQ_HANDLED;
}


#ifdef VMW_HAVE_POLL_CONTROLLER
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_netpoll --
 *
 *      Poll network controller.  We reuse hardware interrupt for this.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Packets received/transmitted/whatever.
 *
 *-----------------------------------------------------------------------------
 */
static void
vmxnet_netpoll(struct net_device *dev)
{
   disable_irq(dev->irq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
   vmxnet_interrupt(dev->irq, dev, NULL);
#else
   vmxnet_interrupt(dev->irq, dev);
#endif
   enable_irq(dev->irq);
}
#endif /* VMW_HAVE_POLL_CONTROLLER */


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_close --
 *
 *      Network device stop (close) routine.  Called by Linux when the
 *      interface is brought down.
 *
 * Results:
 *      0 for success (always).
 *
 * Side effects:
 *      Flushes pending transmits.  Frees IRQs and shared memory area.
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_close(struct net_device *dev)
{
   unsigned int ioaddr = dev->base_addr;
   struct Vmxnet_Private *lp = netdev_priv(dev);
   int i;
   unsigned long flags;

   if (vmxnet_debug > 1) {
      printk(KERN_DEBUG "%s: Shutting down ethercard\n", dev->name);
   }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   dev->start = 0;
#endif

   netif_stop_queue(dev);

   lp->devOpen = FALSE;

   spin_lock_irqsave(&lp->txLock, flags);
   if (lp->numTxPending > 0) {
      //Wait absurdly long (2sec) for all the pending packets to be returned.
      printk(KERN_DEBUG "vmxnet_close: Pending tx = %d\n", lp->numTxPending);
      for (i = 0; i < 200 && lp->numTxPending > 0; i++) {
	 outl(VMXNET_CMD_CHECK_TX_DONE, dev->base_addr + VMXNET_COMMAND_ADDR);
	 udelay(10000);
	 check_tx_queue(dev);
      }

      //This can happen when the related vmxnet device is disabled or when
      //something's wrong with the pNIC, or even both.
      //Will go ahead and free these skb's anyways (possibly dangerous,
      //but seems to work in practice)
      if (lp->numTxPending > 0) {
         printk(KERN_EMERG "vmxnet_close: %s failed to finish all pending tx (%d).\n"
	        "Is the related vmxnet device disabled?\n"
                "This virtual machine may be in an inconsistent state.\n", 
		dev->name, lp->numTxPending);
         lp->numTxPending = 0;
      }
   }
   spin_unlock_irqrestore(&lp->txLock, flags);

   outl(0, ioaddr + VMXNET_INIT_ADDR);

   free_irq(dev->irq, dev);

   for (i = 0; lp->dd && i < lp->dd->txRingLength; i++) {
      if (lp->txBufInfo[i].skb != NULL && lp->txBufInfo[i].eop) {
	 compat_dev_kfree_skb(lp->txBufInfo[i].skb, FREE_WRITE);
	 lp->txBufInfo[i].skb = NULL;
      }
   }

   for (i = 0; i < lp->numRxBuffers; i++) {
      if (lp->rxSkbuff[i] != NULL) {
         compat_pci_unmap_single(lp->pdev, lp->rxRing[i].paddr,
                                 PKT_BUF_SZ, PCI_DMA_FROMDEVICE);
	 compat_dev_kfree_skb(lp->rxSkbuff[i], FREE_WRITE);
	 lp->rxSkbuff[i] = NULL;
      }
   }
#ifdef VMXNET_DO_ZERO_COPY
   if (lp->jumboFrame || lp->lpd) {
      for (i = 0; i < lp->numRxBuffers2; i++) {
         if (lp->rxPages[i] != NULL) {
            pci_unmap_page(lp->pdev, lp->rxRing2[i].paddr, PAGE_SIZE,
                           PCI_DMA_FROMDEVICE);
            put_page(lp->rxPages[i]);
            lp->rxPages[i] = NULL;
         }
      }
   }
#endif

   COMPAT_NETDEV_MOD_DEC_USE_COUNT;

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_load_multicast --
 *
 *      Load the multicast filter.
 *
 * Results:
 *      return number of entries used to compute LADRF
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_load_multicast (struct net_device *dev)
{
   struct Vmxnet_Private *lp = netdev_priv(dev);
    volatile u16 *mcast_table = (u16 *)lp->dd->LADRF;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34)
    struct netdev_hw_addr *dmi;
#else
    int i=0;
    struct dev_mc_list *dmi = dev->mc_list;
#endif
    u8 *addrs;
    int j, bit, byte;
    u32 crc, poly = CRC_POLYNOMIAL_LE;

    /* clear the multicast filter */
    lp->dd->LADRF[0] = 0;
    lp->dd->LADRF[1] = 0;

    /* Add addresses */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34)
    netdev_for_each_mc_addr(dmi, dev) {
	addrs = dmi->addr;
#else
    for (i = 0; i < dev->mc_count; i++){
	addrs = dmi->dmi_addr;
	dmi   = dmi->next;
#endif

	/* multicast address? */
	if (!(*addrs & 1))
	    continue;

	crc = 0xffffffff;
	for (byte = 0; byte < 6; byte++) {
	    for (bit = *addrs++, j = 0; j < 8; j++, bit >>= 1) {
		int test;

		test = ((bit ^ crc) & 0x01);
		crc >>= 1;

		if (test) {
		    crc = crc ^ poly;
		}
	    }
	 }

	 crc = crc >> 26;
	 mcast_table [crc >> 4] |= 1 << (crc & 0xf);
    }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34)
	return netdev_mc_count(dev);
#else
	return i;
#endif
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_set_multicast_list --
 *
 *      Network device set_multicast_list routine.  Called by Linux when the
 *      set of addresses to listen to changes, including both the multicast
 *      list and the broadcast, promiscuous, multicast, and allmulti flags.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Informs lower layer of the changes.
 *
 *-----------------------------------------------------------------------------
 */
static void
vmxnet_set_multicast_list(struct net_device *dev)
{
   unsigned int ioaddr = dev->base_addr;
   struct Vmxnet_Private *lp = netdev_priv(dev);

   /*
    * The .suspend handler frees driver data, so we need this check.
    */
   if (UNLIKELY(!lp->dd)) {
      return;
   }

   lp->dd->ifflags = ~(VMXNET_IFF_PROMISC
                      |VMXNET_IFF_BROADCAST
                      |VMXNET_IFF_MULTICAST);

   if (dev->flags & IFF_PROMISC) {
      printk(KERN_DEBUG "%s: Promiscuous mode enabled.\n", dev->name);
      lp->dd->ifflags |= VMXNET_IFF_PROMISC;
   }
   if (dev->flags & IFF_BROADCAST) {
      lp->dd->ifflags |= VMXNET_IFF_BROADCAST;
   }

   if (dev->flags & IFF_ALLMULTI) {
      lp->dd->LADRF[0] = 0xffffffff;
      lp->dd->LADRF[1] = 0xffffffff;
      lp->dd->ifflags |= VMXNET_IFF_MULTICAST;
   } else {
      if (vmxnet_load_multicast(dev)) {
         lp->dd->ifflags |= VMXNET_IFF_MULTICAST;
      }
   }
   outl(VMXNET_CMD_UPDATE_LADRF, ioaddr + VMXNET_COMMAND_ADDR);

   outl(VMXNET_CMD_UPDATE_IFF, ioaddr + VMXNET_COMMAND_ADDR);
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_set_mac_address --
 *
 *      Network device set_mac_address routine.  Called by Linux when someone
 *      asks to change the interface's MAC address.
 *
 * Results:
 *      0 for success; -EBUSY if interface is up.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_set_mac_address(struct net_device *dev, void *p)
{
   struct sockaddr *addr=p;
   unsigned int ioaddr = dev->base_addr;
   int i;

   memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

   for (i = 0; i < ETH_ALEN; i++) {
      outb(addr->sa_data[i], ioaddr + VMXNET_MAC_ADDR + i);
   }
   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_get_stats --
 *
 *      Network device get_stats routine.  Called by Linux when interface
 *      statistics are requested.
 *
 * Results:
 *      Returns a pointer to our private stats structure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static struct net_device_stats *
vmxnet_get_stats(struct net_device *dev)
{
   Vmxnet_Private *lp = netdev_priv(dev);

   return &lp->stats;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
static int
vmxnet_set_features(struct net_device *netdev, compat_netdev_features_t features)
{
   compat_netdev_features_t changed = features ^ netdev->features;

   if (changed & (NETIF_F_RXCSUM)) {
      if (features & NETIF_F_RXCSUM)
         return 0;
   }
   return -1;
}
#endif

module_init(vmxnet_init);
module_exit(vmxnet_exit);
MODULE_DEVICE_TABLE(pci, vmxnet_chips);

/* Module information. */
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Virtual Ethernet driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VMXNET_DRIVER_VERSION_STRING);
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
