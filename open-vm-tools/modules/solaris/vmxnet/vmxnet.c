/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/kstat.h>
#include <sys/vtrace.h>
#include <sys/dlpi.h>
#include <sys/strsun.h>
#include <sys/ethernet.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/gld.h>
#include <sys/pci.h>
#include <sys/strsubr.h>

/*
 * This used to be defined in sys/gld.h, but was flagged as private,
 * and we used it anyway.  Now it no longer exists, and we're stuck
 * with it for the time being.
 */
#ifndef GLD_MAX_MULTICAST
#define GLD_MAX_MULTICAST 64
#endif

#define __intptr_t_defined
#define _STDINT_H
#include "vm_basic_types.h"
#include "vmxnet2_def.h"
#include "vm_device_version.h"
#include "net.h"
#include "buildNumber.h"

#define SOLVMXNET_SUCCESS 1
#define SOLVMXNET_FAILURE 0

#ifdef SOLVMXNET_DEBUG_LEVEL
static int vxn_debug = SOLVMXNET_DEBUG_LEVEL;
#define  DPRINTF(n, args)  if (vxn_debug>(n)) cmn_err args
#else
#define  DPRINTF(n, args)
#endif

static char ident[] = "VMware Ethernet Adapter b" BUILD_NUMBER_NUMERIC_STRING;
char _depends_on[] = {"misc/gld"};

#define MAX_NUM_RECV_BUFFERS            128
#define DEFAULT_NUM_RECV_BUFFERS        100
#define MAX_NUM_XMIT_BUFFERS            128
#define DEFAULT_NUM_XMIT_BUFFERS        100
#define CRC_POLYNOMIAL_LE               0xedb88320UL
#define SOLVMXNET_MAXNAME               20
#define MAX_TX_WAIT_ON_STOP             2000

#define ETHERALIGN  2
#define SLACKBYTES  4
#define MAXPKTBUF   (14 + ETHERALIGN + ETHERMTU + SLACKBYTES)


#define QHIWATER (MAX_NUM_RECV_BUFFERS*ETHERMTU)

#define OUTB(dp, p, v)  \
        ddi_put8((dp)->vxnIOHdl, \
                (uint8_t *)((caddr_t)((dp)->vxnIOp) + (p)), v)
#define OUTW(dp, p, v)  \
        ddi_put16((dp)->vxnIOHdl, \
                (uint16_t *)((caddr_t)((dp)->vxnIOp) + (p)), v)
#define OUTL(dp, p, v)  \
        ddi_put32((dp)->vxnIOHdl, \
                (uint32_t *)((caddr_t)((dp)->vxnIOp) + (p)), v)
#define INB(dp, p)      \
        ddi_get8((dp)->vxnIOHdl, \
                (uint8_t *)(((caddr_t)(dp)->vxnIOp) + (p)))
#define INW(dp, p)      \
        ddi_get16((dp)->vxnIOHdl, \
                (uint16_t *)(((caddr_t)(dp)->vxnIOp) + (p)))
#define INL(dp, p)      \
        ddi_get32((dp)->vxnIOHdl, \
                (uint32_t *)(((caddr_t)(dp)->vxnIOp) + (p)))

#define VMXNET_INC(val, max) \
   val++; \
   if (UNLIKELY(val == max)) { \
      val = 0; \
   }

#define TX_RINGBUF_MBLK(dp, idx) (dp->txRingBuf[idx].mblk)
#define TX_RINGBUF_DMAMEM(dp, idx) (dp->txRingBuf[idx].dmaMem)

typedef struct {
   caddr_t           buf;             /* Virtual address */
   uint32_t          phyBuf;          /* Physical address */
   size_t            bufLen;          /* Buffer length */
   ddi_dma_cookie_t  cookie;          /* Dma cookie */
   uint_t            cookieCount;     /* Cookie count */
   ddi_dma_handle_t  dmaHdl;          /* Dma handle */ 
   ddi_acc_handle_t  dataAccHdl;      /* Dada access handle */
} dma_buf_t;

typedef struct rx_dma_buf {
   dma_buf_t           dmaDesc;       /* Dma descriptor */
   mblk_t              *mblk;         /* Streams message block */
   frtn_t              freeCB;        /* Free callback */
   struct vxn_softc    *softc;        /* Back pointer to softc */
   struct rx_dma_buf   *next;         /* Next one in list */
} rx_dma_buf_t;

typedef struct vxn_stats {
   uint32_t    errxmt;               /* Transmit errors */
   uint32_t    errrcv;               /* Receive errors */
   uint32_t    runt;                 /* Runt packets */
   uint32_t    norcvbuf;             /* Buffer alloc errors */
   uint32_t    interrupts;	     /* Interrupts */
   uint32_t    defer;		     /* Deferred transmits */
} vxn_stats_t;

typedef struct tx_ring_buf {
   mblk_t      *mblk;
   dma_buf_t   dmaMem;
} tx_ring_buf_t;

typedef struct vxn_softc {
   char                    drvName[SOLVMXNET_MAXNAME]; /* Driver name string */
   int                     unit;               /* Driver instance */
   vxn_stats_t             stats;              /* Stats */

   dev_info_t              *dip;               /* Info pointer */
   ddi_iblock_cookie_t     iblockCookie;       /* Interrupt block cookie */
   gld_mac_info_t          *macInfo;           /* GLD mac info */
   ddi_acc_handle_t        confHdl;            /* Configuration space handle */
   ddi_acc_handle_t        vxnIOHdl;           /* I/O space handle */
   caddr_t                 vxnIOp;             /* I/O space pointer */
   boolean_t               morphed;            /* Adapter morphed ? */

   kmutex_t                intrlock;           /* Interrupt lock */
   kmutex_t                xmitlock;           /* Transmit lock */
   kmutex_t                rxlistlock;         /* Rx free pool lock */

   boolean_t               nicActive;          /* NIC active flag */
   boolean_t               inIntr;             /* Interrupt processing flag */

   struct ether_addr       devAddr;            /* MAC address */

   uint32_t                vxnNumRxBufs;       /* Number of reveice buffers */
   uint32_t                vxnNumTxBufs;       /* Number of transmit buffers */

   dma_buf_t               driverDataDmaMem;   /* Driver Data (dma handle) */
   Vmxnet2_DriverData      *driverData;        /* Driver Data */
   void                    *driverDataPhy;     /* Driver Data busaddr pointer */
   Vmxnet2_RxRingEntry     *rxRing;            /* Receive ring */
   Vmxnet2_TxRingEntry     *txRing;            /* Transmit ring */
   ddi_dma_handle_t        txDmaHdl;           /* Tx buffers dma handle */
   rx_dma_buf_t            *rxRingBuffPtr[MAX_NUM_RECV_BUFFERS];
                                               /* DMA buffers associated with rxRing */
   tx_ring_buf_t           txRingBuf[MAX_NUM_XMIT_BUFFERS]; /* tx Ring buffers */

   rx_dma_buf_t            *rxFreeBufList;
   uint32_t                rxNumFreeBufs;      /* current # of buffers in pool */
   uint32_t                rxMaxFreeBufs;      /* max # of buffers in pool */

   uint32_t                txPending;          /* Pending transmits */
   uint32_t                maxTxFrags;         /* Max Tx fragments */

   int                     multiCount;         /* Multicast address count */
   struct ether_addr       multicastList[GLD_MAX_MULTICAST]; /* Multicast list */

   struct vxn_softc        *next;              /* Circular list of instances */
   struct vxn_softc        *prev;
} vxn_softc_t;

/* used for rx buffers or buffers allocated by ddi_dma_mem_alloc() */
static ddi_dma_attr_t vxn_dma_attrs = {
        DMA_ATTR_V0,            /* dma_attr version */
        0,                      /* dma_attr_addr_lo */
        (uint64_t)0xFFFFFFFF,   /* dma_attr_addr_hi */
        0x7FFFFFFF,             /* dma_attr_count_max */
        4,                      /* dma_attr_align */
        0x3F,                   /* dma_attr_burstsizes */
        1,                      /* dma_attr_minxfer */
        (uint64_t)0xFFFFFFFF,   /* dma_attr_maxxfer */
        (uint64_t)0xFFFFFFFF,   /* dma_attr_seg */
        1,                      /* dma_attr_sgllen */
        1,                      /* dma_attr_granular */
        0,                      /* dma_attr_flags */
};

/* used for tx buffers */
static ddi_dma_attr_t vxn_dma_attrs_tx = {
        DMA_ATTR_V0,            /* dma_attr version */
        0,                      /* dma_attr_addr_lo */
        (uint64_t)0xFFFFFFFF,   /* dma_attr_addr_hi */
        0x7FFFFFFF,             /* dma_attr_count_max */
        1,                      /* dma_attr_align */
        0x3F,                   /* dma_attr_burstsizes */
        1,                      /* dma_attr_minxfer */
        (uint64_t)0xFFFFFFFF,   /* dma_attr_maxxfer */
        (uint64_t)0xFFFFFFFF,   /* dma_attr_seg */
        1,                      /* dma_attr_sgllen */
        1,                      /* dma_attr_granular */
        0,                      /* dma_attr_flags */
};


static   struct ether_addr etherbroadcastaddr = {
   {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
};

static struct ddi_device_acc_attr vxn_buf_attrs = {
        DDI_DEVICE_ATTR_V0,
        DDI_STRUCTURE_LE_ACC,
        DDI_STRICTORDER_ACC
};

static struct ddi_device_acc_attr dev_attr = {
        DDI_DEVICE_ATTR_V0,
        DDI_STRUCTURE_LE_ACC,
        DDI_STRICTORDER_ACC
};

static vxn_softc_t vxnList;		/* for debugging */
static kmutex_t vxnListLock;

static void *Vxn_Memset(void *s, int c, size_t n);
static int Vxn_Reset(gld_mac_info_t *macInfo);
static int Vxn_SetPromiscuous(gld_mac_info_t *macInfo, int flag);
static int Vxn_GetStats(gld_mac_info_t *macInfo, struct gld_stats *gs);
static void Vxn_ApplyAddressFilter(vxn_softc_t *dp);
static int Vxn_SetMulticast(gld_mac_info_t *macinfo, uint8_t *ep, int flag);
static int Vxn_SetMacAddress(gld_mac_info_t *macInfo, uint8_t *mac);
static int Vxn_Start(gld_mac_info_t *macInfo);
static int Vxn_Stop(gld_mac_info_t *macInfo);
static void Vxn_FreeTxBuf(vxn_softc_t *dp, int idx);
static int Vxn_EncapTxBuf(vxn_softc_t *dp, mblk_t *mp, Vmxnet2_TxRingEntry *xre,
                          tx_ring_buf_t *txBuf);
static int Vxn_Send(gld_mac_info_t *macinfo, mblk_t *mp);
static boolean_t Vxn_TxComplete(vxn_softc_t *dp, boolean_t *reschedp);
static boolean_t Vxn_Receive(vxn_softc_t *dp);
static u_int Vxn_Interrupt(gld_mac_info_t *macInfo);
static void Vxn_ReclaimRxBuf(rx_dma_buf_t *rxDesc);
static void Vxn_FreeRxBuf(rx_dma_buf_t *rxDesc);
static rx_dma_buf_t *Vxn_AllocRxBuf(vxn_softc_t *dp, int cansleep);
static void Vxn_FreeInitBuffers(vxn_softc_t *dp);
static int Vxn_AllocInitBuffers(vxn_softc_t *dp);
static void Vxn_FreeDmaMem(dma_buf_t *dma);
static int Vxn_AllocDmaMem(vxn_softc_t *dp, int size, int cansleep, dma_buf_t *dma);
static void Vxn_FreeDriverData(vxn_softc_t *dp);
static int Vxn_AllocDriverData(vxn_softc_t *dp);
static int Vxn_Attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int Vxn_Detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int Vxn_AllocRxBufPool(vxn_softc_t *dp);
static void Vxn_FreeRxBufPool(vxn_softc_t *dp);
static rx_dma_buf_t * Vxn_AllocRxBufFromPool(vxn_softc_t *dp);
static void Vxn_FreeRxBufToPool(rx_dma_buf_t *rxDesc);

/*
 *-----------------------------------------------------------------------------
 * Vxn_Memset --
 *    memset() (Because bzero does not get resolved by module loader) 
 *
 * Results:
 *    pointer to the memory area s
 *
 * Side effects:
 *    None  
 *-----------------------------------------------------------------------------
 */
static void *
Vxn_Memset(void *s, int c, size_t n)
{
   while (n--) {
      ((uint8_t *)s)[n] = c;
   }

   return s;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_Reset -- 
 *    Stub routine to reset hardware. Presently does nothing. Start/Stop should
 *    take care of resets.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_Reset(gld_mac_info_t *macInfo)
{
   return GLD_SUCCESS;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_SetPromiscuous --
 *    Set/Reset NIC to/from promiscuous mode
 *
 * Results:
 *    GLD_SUCCESS
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_SetPromiscuous(gld_mac_info_t *macInfo, int flag)
{
   vxn_softc_t *dp = (vxn_softc_t *)macInfo->gldm_private;
   Vmxnet2_DriverData *dd = dp->driverData;

   mutex_enter(&dp->intrlock);
   if (flag == GLD_MAC_PROMISC_PHYS) {
      dd->ifflags |= VMXNET_IFF_PROMISC;
   } else if (flag == GLD_MAC_PROMISC_MULTI) {
     /*
      * This should really set VMXNET_IFF_ALLMULTI,
      * but unfortunately it doesn't exist.  The next
      * best thing would be to set the LADRFs to all
      * 0xFFs and set VMXNET_IFF_MULTICAST, but that
      * opens up a whole new set of potential pitfalls,
      * so this is a reasonable temporary solution.
      */
      dd->ifflags |= VMXNET_IFF_PROMISC;
   } else if (flag == GLD_MAC_PROMISC_NONE) {
      dd->ifflags &= ~VMXNET_IFF_PROMISC;
   } else {
     /* This could be GLD_MAC_PROMISC_NOOP? */
     mutex_exit(&dp->intrlock);
     cmn_err(CE_WARN, "%s%d: Vxn_SetPromiscuous: Unexpected mode flag: 0x%x", 
             dp->drvName, dp->unit, flag);

     return GLD_FAILURE;
   }

   OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_UPDATE_IFF);
   mutex_exit(&dp->intrlock);

   return GLD_SUCCESS;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_GetStats --
 *    Get driver specific stats
 *
 * Results:
 *    GLD_SUCCESS
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_GetStats(gld_mac_info_t *macInfo, struct gld_stats *gs)
{
   vxn_softc_t *dp = (vxn_softc_t *)macInfo->gldm_private;

   gs->glds_errxmt    = dp->stats.errxmt;
   gs->glds_errrcv    = dp->stats.errrcv;
   gs->glds_short     = dp->stats.runt;
   gs->glds_norcvbuf  = dp->stats.norcvbuf;
   gs->glds_intr      = dp->stats.interrupts;
   gs->glds_defer     = dp->stats.defer;

   return GLD_SUCCESS;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_ApplyAddressFilter --
 *    Go over multicast list and compute/apply address filter
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_ApplyAddressFilter(vxn_softc_t *dp)
{
   uint8_t *ep;
   int i, j, bit, byte;
   uint32_t crc, poly = CRC_POLYNOMIAL_LE;
   Vmxnet2_DriverData *dd = dp->driverData;
   volatile uint16_t *mcastTable = (uint16_t *)dd->LADRF;

   ASSERT(MUTEX_HELD(&dp->intrlock));

   /* clear the multicast filter */
   dd->LADRF[0] = 0;
   dd->LADRF[1] = 0;

   for (i = 0; i < dp->multiCount; i++) {
      crc = 0xffffffff;
      ep = (uint8_t *)&dp->multicastList[i].ether_addr_octet;

      for (byte = 0; byte < 6; byte++) {
         for (bit = *ep++, j = 0; j < 8; j++, bit >>= 1) {
            int test;

            test = ((bit ^ crc) & 0x01);
            crc >>= 1;

            if (test) {
               crc = crc ^ poly;
            }
         }
      }

      crc = crc >> 26;
      mcastTable[crc >> 4] |= 1 << (crc & 0xf);
   }
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_SetMulticast --
 *    Add delete entry from multicast list
 *
 * Results:
 *    GLD_FAILURE on failure
 *    GLD_SUCCESS on success
 *
 * Side effects:
 *    None  
 *-----------------------------------------------------------------------------
 */
static int
Vxn_SetMulticast(gld_mac_info_t *macinfo, uint8_t *ep, int flag)
{
   int i;
   int copyLen;
   vxn_softc_t *dp = (vxn_softc_t *)macinfo->gldm_private;
   Vmxnet2_DriverData *dd = dp->driverData;

   if (flag == GLD_MULTI_ENABLE) {
      /*
       * Exceeded multicast address limit
       */
      if (dp->multiCount >= GLD_MAX_MULTICAST) {
         return GLD_FAILURE;
      }

      /* 
       * Add mac address to multicast list
       */
      bcopy(ep, dp->multicastList[dp->multiCount].ether_addr_octet, 
                ETHERADDRL);
      dp->multiCount++;
   }
   else {
      for (i=0; i<dp->multiCount; i++) {
         if (bcmp(ep, dp->multicastList[i].ether_addr_octet, ETHERADDRL) == 0) {
            goto found;
         }
      }
      return GLD_FAILURE;

   found:
      /*
       * Delete mac address from multicast list
       */
      copyLen = (dp->multiCount - (i+1)) * sizeof(struct ether_addr);
      if (copyLen > 0) {
        bcopy(&dp->multicastList[i+1], &dp->multicastList[i], copyLen);
      }
      dp->multiCount--;
   }

   /*
    * Compute address filter from list of addressed and apply it
    */
   mutex_enter(&dp->intrlock);
   Vxn_ApplyAddressFilter(dp);

   if (dp->multiCount) {
     ASSERT(dd->LADRF[0] || dd->LADRF[1]);
     dd->ifflags |= VMXNET_IFF_MULTICAST;
   } else {
     ASSERT(!(dd->LADRF[0] || dd->LADRF[1]));
     dd->ifflags &= ~VMXNET_IFF_MULTICAST;
   }

   OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_UPDATE_IFF);
   OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_UPDATE_LADRF);
   mutex_exit(&dp->intrlock);   

   return GLD_SUCCESS;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_SetMacAddress --
 *    Change device MAC address  
 *
 * Results:
 *    GLD_SUCCESS
 *    GLD_FAILURE
 *
 * Side effects:
 *    None 
 *-----------------------------------------------------------------------------
 */
static int
Vxn_SetMacAddress(gld_mac_info_t *macInfo, uint8_t *mac)
{
   int i;
   int err = GLD_SUCCESS;
   vxn_softc_t * dp = (vxn_softc_t *)macInfo->gldm_private;

   mutex_enter(&dp->intrlock);
   mutex_enter(&dp->xmitlock);

   /*
    * Don't change MAC address on a running NIC
    */
   if (dp->nicActive) {
      err = GLD_FAILURE;
      goto out;
   }

   /*
    * Save new MAC address 
    */
   for (i = 0; i < 6; i++) {
      dp->devAddr.ether_addr_octet[i] = mac[i];
   }
   
   /*
    * Push new MAC address down into hardware
    */
   for (i = 0; i < 6; i++) {
      OUTB(dp, VMXNET_MAC_ADDR + i, mac[i]);
   }

out:
   mutex_exit(&dp->xmitlock);
   mutex_exit(&dp->intrlock);
   return err;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_Start --
 *    Device start routine. Called on "ifconfig plumb"
 *
 * Results:
 *    GLD_SUCCESS
 *    GLD_FAILURE
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_Start(gld_mac_info_t *macInfo)
{
   int err = GLD_SUCCESS;
   uint32_t r, capabilities, features;
   vxn_softc_t * dp = (vxn_softc_t *)macInfo->gldm_private;

   mutex_enter(&dp->intrlock);
   mutex_enter(&dp->xmitlock);

   if (!dp->nicActive) {
      /*
       * Register ring structure with hardware
       *
       * This downcast is OK because we requested a 32-bit physical address
       */
      OUTL(dp, VMXNET_INIT_ADDR, (uint32_t)(uintptr_t)dp->driverDataPhy);
      OUTL(dp, VMXNET_INIT_LENGTH, dp->driverData->length);

      /* 
       * Make sure registeration succeded 
       */
      r = INL(dp, VMXNET_INIT_LENGTH);
      if (!r) {
         cmn_err(CE_WARN, "%s%d: Vxn_Start: failed to register ring", 
                          dp->drvName, dp->unit);
         err = GLD_FAILURE;
         goto out;
      }

      /*
       * Get maximum tx fragments supported
       */
      OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_GET_CAPABILITIES);
      capabilities = INL(dp, VMXNET_COMMAND_ADDR);

      OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_GET_FEATURES);
      features = INL(dp, VMXNET_COMMAND_ADDR);

      DPRINTF(3, (CE_CONT, "%s%d: chip capabilities=0x%x features=0x%x\n", 
              dp->drvName, dp->unit, capabilities, features));

      if ((capabilities & VMNET_CAP_SG) &&
          (features & VMXNET_FEATURE_ZERO_COPY_TX)) {
         dp->maxTxFrags = VMXNET2_SG_DEFAULT_LENGTH;
      } else {
         dp->maxTxFrags = 1;
      }
      ASSERT(dp->maxTxFrags >= 1);

      /*
       * Alloc Tx DMA handle
       */
      vxn_dma_attrs_tx.dma_attr_sgllen = dp->maxTxFrags;
      if (ddi_dma_alloc_handle(dp->dip, &vxn_dma_attrs_tx, DDI_DMA_SLEEP,
                               NULL, &dp->txDmaHdl) != DDI_SUCCESS)  {
         cmn_err(CE_WARN, "%s%d: Vxn_Start: failed to alloc tx dma handle", 
                 dp->drvName, dp->unit);
         err = GLD_FAILURE;
         goto out;
      }

      /*
       * Enable interrupts on the card
       */
      dp->driverData->ifflags |= VMXNET_IFF_BROADCAST | VMXNET_IFF_DIRECTED;

      OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_INTR_ENABLE);
      OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_UPDATE_IFF);
      OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_UPDATE_LADRF);

      dp->nicActive = TRUE;
  }

out:
   mutex_exit(&dp->xmitlock);
   mutex_exit(&dp->intrlock);
   return err;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_Stop --
 *    Device stop routine. Called on "ifconfig unplumb"
 *
 * Results:
 *    GLD_SUCCESS
 *    GLD_FAILURE
 *
 * Side effects:
 *    None   
 *-----------------------------------------------------------------------------
 */
static int
Vxn_Stop(gld_mac_info_t *macInfo)
{
   int i;
   int err = GLD_SUCCESS;
   vxn_softc_t * dp = (vxn_softc_t *)macInfo->gldm_private;
   boolean_t resched;

   mutex_enter(&dp->intrlock);
   mutex_enter(&dp->xmitlock);

   if (!dp->nicActive) {
      goto out;
   }

   /*
    * Disable interrupts
    */
   OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_INTR_DISABLE);

   /*
    * Wait for pending transmits
    */
   if (dp->txPending) {
      for (i=0; i < MAX_TX_WAIT_ON_STOP && dp->txPending; i++) {
         delay(drv_usectohz(1000));
         OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_CHECK_TX_DONE);
         (void) Vxn_TxComplete(dp, &resched);
         /*
          * Don't worry about rescheduling transmits - GLD handles
          * this automatically.
          */
      }
   }
   if (dp->txPending) {
      cmn_err(CE_WARN, "%s%d: Vxn_Stop: giving up on %d pending transmits", 
                       dp->drvName, dp->unit, dp->txPending);      
   }

   OUTL(dp, VMXNET_INIT_ADDR, 0);
   dp->nicActive = FALSE;

   /*
    * Free Tx DMA handle
    *
    * The ddi_dma_free_handle() man page says that ddi_dma_unbind_handle() must be called
    * prior to calling ddi_dma_free_handle().
    * However, call to ddi_dma_unbind_handle() is not required here, because
    * ddi_dma_addr_bind_handle() and matching ddi_dma_unbind_handle() are called from
    * Vxn_EncapTxBuf().
    * xmitlock is held in Vxn_EncapTxBuf() as well as acquired above in Vxn_Stop().
    */
   ddi_dma_free_handle(&dp->txDmaHdl);
   dp->txDmaHdl = NULL;

out:
   mutex_exit(&dp->xmitlock);
   mutex_exit(&dp->intrlock);
   return err;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_FreeTxBuf --
 *    Free transmit buffer
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_FreeTxBuf(vxn_softc_t *dp, int idx)
{
   mblk_t **txMblkp = &TX_RINGBUF_MBLK(dp, idx);
   dma_buf_t *dmaMem = &TX_RINGBUF_DMAMEM(dp, idx);

   if (*txMblkp) {
      freemsg(*txMblkp);
      *txMblkp = NULL;
   }

   if (dmaMem->buf) {
      Vxn_FreeDmaMem(dmaMem);
      ASSERT(dmaMem->buf == NULL);
   }
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_EncapTxBuf -- 
 *    Go over dma mappings of Tx buffers and drop buffer physical address 
 *    into ring entry
 *
 * Results:
 *    SOLVMXNET_SUCCESS on success
 *    SOLVMXNET_FAILURE on failure
 *
 * Side effects:
 *    None  
 *---------------- -------------------------------------------------------------
 */
static int
Vxn_EncapTxBuf(vxn_softc_t *dp, 
               mblk_t *mp, 
               Vmxnet2_TxRingEntry *xre, 
               tx_ring_buf_t *txBuf)
{
   int frag;
   int fragcount;
   int rval;
   mblk_t *tp;
   mblk_t *mblk;
   boolean_t needPullup = FALSE;
   boolean_t dmaMemAlloced = FALSE;

   ASSERT(txBuf);
   ASSERT(txBuf->mblk == NULL);
   ASSERT(MUTEX_HELD(&dp->xmitlock));

   xre->sg.length = 0;
   xre->flags = 0;

   fragcount = 0;
   for (tp = mp; tp != NULL; tp = tp->b_cont) {
      fragcount++;
   }
   if (fragcount > dp->maxTxFrags) {
      needPullup = TRUE;
   }

pullup:
   frag = 0;
   if (needPullup) {
      if (!(mblk = msgpullup(mp, -1))) {
         cmn_err(CE_WARN, "%s%d: Vxn_EncapTxBuf: msgpullup failed",
                          dp->drvName, dp->unit);
         goto err;
      }
   } else {
      mblk = mp;
   }

   /*
    * Go through message chain and drop packet pointers into ring
    * scatter/gather array
    */
   for (tp = mblk; tp != NULL; tp = tp->b_cont) {

      uint_t nCookies;
      ddi_dma_cookie_t dmaCookie;
      int len = tp->b_wptr - tp->b_rptr;

      if (len) {
         /*
          * Associate tx buffer with dma handle
          */
         ASSERT(dp->txDmaHdl);
         if ((rval = ddi_dma_addr_bind_handle(dp->txDmaHdl, NULL, (caddr_t)tp->b_rptr,
                                      len, DDI_DMA_RDWR | DDI_DMA_STREAMING,
                                      DDI_DMA_DONTWAIT, NULL,
                                      &dmaCookie, &nCookies))
             != DDI_DMA_MAPPED) {

            /*
             *  Try to handle bind failure caused by a page boundary spill
             *  by allocating a private dma buffer and copying data into it
             */
            if ((rval == DDI_DMA_TOOBIG) && !dmaMemAlloced ) {
               /*
                * Force pullup 
                */
               if (!needPullup && (dp->maxTxFrags > 1)) {
                  needPullup = TRUE;
                  goto pullup;
               }

               if (Vxn_AllocDmaMem(dp, len, FALSE, &txBuf->dmaMem) 
                                             != SOLVMXNET_SUCCESS) {
                  goto err;
               }

               dmaMemAlloced = TRUE;

               /*
                * Copy data into DMA capable buffer
                */
               bcopy(tp->b_rptr, txBuf->dmaMem.buf, len);

               /*
                * Stick buffer physical addr in the ring
                */
               xre->sg.sg[frag].addrLow = txBuf->dmaMem.phyBuf;
               xre->sg.sg[frag].length = len;
               frag++;

               continue;

            } else {
               cmn_err(CE_WARN, "%s%d: Vxn_EncapTxBuf: failed (%d) to bind dma "
                                "handle for len %d. [dmaMemAlloced=%d]", 
                                dp->drvName, dp->unit, rval, len, dmaMemAlloced);
               goto err;
            }
         }

         /*
          * Extract tx buffer physical addresses from cookie
          */
         while (nCookies) {
            if (UNLIKELY(frag == dp->maxTxFrags)) {
               (void)ddi_dma_unbind_handle(dp->txDmaHdl);

               if (!needPullup) {
                  ASSERT(!dmaMemAlloced);
                  needPullup = TRUE;
                  goto pullup;
               } else {
                  cmn_err(CE_WARN, "%s%d: Vxn_EncapTxBuf: "
                          "exceeded max (%d) fragments in message",
                          dp->drvName, dp->unit, dp->maxTxFrags);
                  goto err;
               }
            }

            /*
             * Stick it in the ring
             */
            xre->sg.sg[frag].addrLow = dmaCookie.dmac_address;
            xre->sg.sg[frag].length = dmaCookie.dmac_size;
            frag++;

            if (--nCookies) {
               ddi_dma_nextcookie(dp->txDmaHdl, &dmaCookie);
            }
         }

         (void)ddi_dma_unbind_handle(dp->txDmaHdl);
      }
   }

   if (frag > 0) {
      xre->sg.length = frag;

      /* Give ownership to NIC */
      xre->sg.addrType = NET_SG_PHYS_ADDR;
      xre->ownership = VMXNET2_OWNERSHIP_NIC;
      xre->flags |= VMXNET2_TX_CAN_KEEP;
      txBuf->mblk = mblk;

      /*
       * If we called msgpullup to concatenate fragments, free
       * original mblk now since we're going to return success.
       */
      if (mblk != mp) {
         freemsg(mp);
      }

      return SOLVMXNET_SUCCESS;
   }

err:
   if (mblk != NULL && mblk != mp) {
      /*
       * Free mblk allocated by msgpullup.
       */
      freemsg(mblk);
   }

   if (dmaMemAlloced) {
      ASSERT(txBuf->dmaMem.buf);
      Vxn_FreeDmaMem(&txBuf->dmaMem);
   }

   return SOLVMXNET_FAILURE;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_Send --
 *    GLD Transmit routine. Starts packet hard tx.
 *
 * Results:
 *    GLD_SUCCESS on success
 *    GLD_FAILURE on failure
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_Send(gld_mac_info_t *macinfo, mblk_t *mp)
{
   Vmxnet2_TxRingEntry *xre;
   int err = GLD_SUCCESS;
   vxn_softc_t *dp = (vxn_softc_t *)macinfo->gldm_private;
   Vmxnet2_DriverData *dd = dp->driverData;
   boolean_t resched = FALSE;

   mutex_enter(&dp->xmitlock);

   /*
    * Check if ring entry at drop pointer is available
    */
   if (TX_RINGBUF_MBLK(dp, dd->txDriverNext) != NULL) {
      DPRINTF(3, (CE_NOTE, "%s%d: Vxn_Send: tx ring full",
                  dp->drvName, dp->unit));
      err = GLD_NORESOURCES;
      dd->txStopped = TRUE;
      dp->stats.defer++;
      goto out;
   }

   xre = &dp->txRing[dd->txDriverNext];

   /*
    * Drop packet into ring entry
    */
   if (Vxn_EncapTxBuf(dp, mp, xre, &dp->txRingBuf[dd->txDriverNext])
       != SOLVMXNET_SUCCESS) {
      err = GLD_FAILURE;
      dp->stats.errxmt++;
      goto out;
   }

   /*
    * Increment drop pointer
    */
   VMXNET_INC(dd->txDriverNext, dd->txRingLength);
   dd->txNumDeferred++;
   dp->txPending++;

   /*
    * Transmit, if number of pending packets > tx cluster length
    */
   if (dd->txNumDeferred >= dd->txClusterLength) {
      dd->txNumDeferred = 0;

      /* 
       * Call hardware transmit 
       */
      INL(dp, VMXNET_TX_ADDR);
   }

   /*
    * Clean up transmit ring. TX completion interrupts are not guaranteed
    */
   (void) Vxn_TxComplete(dp, &resched);

out:
   mutex_exit(&dp->xmitlock);
   if (resched) {
      /* Tell GLD to retry any deferred packets */
      gld_sched(dp->macInfo);
   }
   return err;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_TxComplete --
 *    Scan Tx ring for completed transmits. Reclaim Tx buffers.
 *
 * Results:
 *    Returns TRUE if it found a completed transmit, FALSE otherwise.
 *    Also sets *reschedp to TRUE if the caller should call gld_sched
 *    to reschedule transmits (once all locks are dropped).
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static boolean_t
Vxn_TxComplete(vxn_softc_t *dp, boolean_t *reschedp)
{
   Vmxnet2_DriverData *dd = dp->driverData;
   boolean_t found = FALSE;
   boolean_t needresched = FALSE;

   ASSERT(MUTEX_HELD(&dp->xmitlock));

   while (1) {
      Vmxnet2_TxRingEntry *xre = &dp->txRing[dd->txDriverCur];

      if (xre->ownership != VMXNET2_OWNERSHIP_DRIVER || 
          (TX_RINGBUF_MBLK(dp, dd->txDriverCur) == NULL)) {
         break;
      }

      found = TRUE;
      Vxn_FreeTxBuf(dp, dd->txDriverCur);

      dp->txPending--;
      VMXNET_INC(dd->txDriverCur, dd->txRingLength);
      if (dd->txStopped) {
         needresched = TRUE;
         dd->txStopped = FALSE;
      }
   }

   *reschedp = needresched;
   return found;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_Receive --
 *    Rx handler. First assembles the packets into a chain of mblks,
 *    then drops locks and passes them up the stack to GLD.
 *
 * Results:
 *    Returns TRUE if it find a packet ready for processing, FALSE
 *    otherwise.
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static boolean_t
Vxn_Receive(vxn_softc_t *dp)
{
   int ringnext;
   short pktlen;
   Vmxnet2_DriverData *dd = dp->driverData;   
   rx_dma_buf_t *rxDesc;
   rx_dma_buf_t *newRxDesc;
   mblk_t *mblk;
   mblk_t *head = NULL;
   mblk_t **tail = &head;
   mblk_t *next;
   boolean_t found = FALSE;	/* Did we find at least one packet? */

   ASSERT(MUTEX_HELD(&dp->intrlock));

   /*
    * Walk receive ring looking for entries with ownership 
    * reverted back to driver
    */
   while (1) {
      Vmxnet2_RxRingEntry *rre;
      rx_dma_buf_t **rbuf;

      ringnext = dd->rxDriverNext;
      rre = &dp->rxRing[ringnext];
      rbuf = &dp->rxRingBuffPtr[ringnext];
      
      if (rre->ownership != VMXNET2_OWNERSHIP_DRIVER) {
         break;
      }

      found = TRUE;

      pktlen = rre->actualLength;

      if (pktlen < (60 - 4)) {
         /*
          * Ethernet header vlan tags are 4 bytes.  Some vendors generate
          *  60byte frames including vlan tags.  When vlan tag
          *  is stripped, such frames become 60 - 4. (PR106153)
          */
         dp->stats.errrcv++;
         if (pktlen != 0) {
            DPRINTF(3, (CE_CONT, "%s%d: runt packet\n", dp->drvName, dp->unit));
            dp->stats.runt++;
         }
      } else {
         /*
          * Alloc new Rx buffer to replace current one
          */
         newRxDesc = Vxn_AllocRxBufFromPool(dp);

         if (newRxDesc) {
            rxDesc = *rbuf;
            mblk = rxDesc->mblk;
            
            *rbuf = newRxDesc;
            rre->paddr = newRxDesc->dmaDesc.phyBuf + ETHERALIGN;
            rre->bufferLength = MAXPKTBUF - ETHERALIGN;
            rre->actualLength = 0;

            /*
             * Advance write pointer past packet length
             */
            mblk->b_wptr = mblk->b_rptr + pktlen;
            
            /*
             * Add to end of chain.
             */
            mblk->b_next = NULL;
            *tail = mblk;
            tail = &mblk->b_next;
         } else {
            dp->stats.errrcv++;
            dp->stats.norcvbuf++;
         }
      }

      /* Give the descriptor back to NIC */
      rre->ownership = VMXNET2_OWNERSHIP_NIC;
      VMXNET_INC(dd->rxDriverNext, dd->rxRingLength);
   }

   /*
    * Walk chain and pass mblks up to gld_recv one by one.
    */
   mutex_exit(&dp->intrlock);
   for (mblk = head; mblk != NULL; mblk = next) {
      next = mblk->b_next;
      mblk->b_next = NULL;
      gld_recv(dp->macInfo, mblk);
   }
   mutex_enter(&dp->intrlock);

   return (found);
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_Interrupt --
 *    GLD interrupt handler. Scan: Rx ring for received packets, Tx ring for
 *    completed transmits
 *
 * Results:
 *    - DDI_INTR_CLAIMED (if we found something to do)
 *    - DDI_INTR_UNCLAIMED (if not)
 *
 * Side effects:
 *    None     
 *-----------------------------------------------------------------------------
 */
static u_int
Vxn_Interrupt(gld_mac_info_t *macInfo)
{
   u_int ret = DDI_INTR_UNCLAIMED;
   vxn_softc_t *dp = (vxn_softc_t *)macInfo->gldm_private;
   boolean_t foundRx, foundTx;
   boolean_t resched = FALSE;

   mutex_enter(&dp->intrlock);
   dp->inIntr = TRUE;

   if (!dp->nicActive) {
      goto out;
   }

   /*
    * Ack interrupt
    */
   OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_INTR_ACK);

   foundRx = Vxn_Receive(dp);

   mutex_enter(&dp->xmitlock);
   foundTx = Vxn_TxComplete(dp, &resched);
   mutex_exit(&dp->xmitlock);

   if (foundRx || foundTx) {
      ret = DDI_INTR_CLAIMED;
      dp->stats.interrupts++;
   }

out:
   dp->inIntr = FALSE;
   mutex_exit(&dp->intrlock);

   if (resched) {
      gld_sched(dp->macInfo);
   }

   return ret; 
}


/*
 *-----------------------------------------------------------------------------
 * Vxn_ReclaimRxBuf --
 *    Callback handler invoked by freemsg(). Frees Rx buffer memory and mappings
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_ReclaimRxBuf(rx_dma_buf_t *rxDesc)
{
   Vxn_FreeRxBufToPool(rxDesc);
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_FreeRxBuf --
 *    Free allocated Rx buffer
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_FreeRxBuf(rx_dma_buf_t *rxDesc)
{
   ASSERT(rxDesc);

   if (rxDesc->mblk) {
      freemsg(rxDesc->mblk);
   } else {
      Vxn_FreeDmaMem(&rxDesc->dmaDesc);
      kmem_free(rxDesc, sizeof(rx_dma_buf_t));
   }
}


/*
 *-----------------------------------------------------------------------------
 * Vxn_AllocRxBuf --
 *    Allocate Rx buffer
 *
 * Results:
 *    Pointer to Rx buffer descriptor - on success
 *    NULL - on failure
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static rx_dma_buf_t *
Vxn_AllocRxBuf(vxn_softc_t *dp, int cansleep)
{
   rx_dma_buf_t *rxDesc;

   rxDesc = (rx_dma_buf_t *)kmem_zalloc(sizeof(rx_dma_buf_t), 
                                  cansleep ? KM_SLEEP : KM_NOSLEEP);
   if (!rxDesc) {
      cmn_err(CE_WARN, "%s%d: Vxn_AllocRxBuf: kmem_zalloc failed", 
                       dp->drvName, dp->unit);
      return NULL;
   }

   rxDesc->softc = dp;

   /*
    * Alloc dma-able packet memory
    */
   if (Vxn_AllocDmaMem(dp, MAXPKTBUF, cansleep, &rxDesc->dmaDesc) 
         != SOLVMXNET_SUCCESS) {
      kmem_free(rxDesc, sizeof(rx_dma_buf_t));
      return NULL;
   }

   /*
    * Fill in free callback; fired by freemsg()
    */
   rxDesc->freeCB.free_func = &Vxn_ReclaimRxBuf;
   rxDesc->freeCB.free_arg = (caddr_t) rxDesc;

   rxDesc->mblk = NULL;
   return rxDesc;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_FreeInitBuffers --
 *    Free allocated Tx and Rx buffers 
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_FreeInitBuffers(vxn_softc_t *dp)
{
   int i;

   for (i=0; i<dp->vxnNumRxBufs; i++) {
      if (dp->rxRingBuffPtr[i]) {
         Vxn_FreeRxBuf(dp->rxRingBuffPtr[i]);
         dp->rxRingBuffPtr[i] = NULL;
      }
   }

   for (i=0; i<dp->vxnNumTxBufs; i++) {
      if (TX_RINGBUF_MBLK(dp, i)) {
         Vxn_FreeTxBuf(dp, i);
      }
   }

   /*
    * Rx pool must get freed last. Rx buffers above will
    * show up on the pool when freemsg callback fires.
    */
   Vxn_FreeRxBufPool(dp);
}


/*
 *-----------------------------------------------------------------------------
 * Vxn_AllocRxBufPool --
 *    Allocate pool of rx buffers - 3 * configured Rx buffers
 *
 * Results:
 *    SOLVMXNET_SUCCESS/SOLVMXNET_FAILURE
 *    
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_AllocRxBufPool(vxn_softc_t *dp)
{
   int i;

   dp->rxFreeBufList = NULL;

   // Allow list to double in size if needed.  Any additional buffers
   // that are allocated on the fly will be freed back to main memory.
   dp->rxMaxFreeBufs = dp->vxnNumRxBufs * 6;

   for (i = 0; i < dp->vxnNumRxBufs * 3; i++) {
      rx_dma_buf_t *rxDesc;

      /*
       * Alloc rx buffer
       */
      if (!(rxDesc = Vxn_AllocRxBuf(dp, TRUE))) {
         cmn_err(CE_WARN, "%s%d: Vxn_AllocRxBufPool: failed to allocate memory",
                 dp->drvName, dp->unit);
         dp->rxNumFreeBufs = i;
         return SOLVMXNET_FAILURE;
      }
      /*
       * Add to free list
       */
      rxDesc->next = dp->rxFreeBufList;
      dp->rxFreeBufList = rxDesc;
   }

   dp->rxNumFreeBufs = i;
   return SOLVMXNET_SUCCESS;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_FreeRxBufPool --
 *    Free rx buffers pool
 *
 * Results:
 *    None    
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_FreeRxBufPool(vxn_softc_t *dp)
{
   while (dp->rxFreeBufList) {
      rx_dma_buf_t *rxDesc = dp->rxFreeBufList;

      /* unlink */
      dp->rxFreeBufList = rxDesc->next;

      ASSERT(rxDesc->mblk == NULL);
      Vxn_FreeDmaMem(&rxDesc->dmaDesc);
      kmem_free(rxDesc, sizeof(rx_dma_buf_t));
   }
   dp->rxNumFreeBufs = 0;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_AllocRxBufFromPool --
 *    Allocate Rx buffer from free pool
 *
 * Results:
 *    Pointer to Rx buffer descriptor - on success
 *    NULL - on failure
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static rx_dma_buf_t *
Vxn_AllocRxBufFromPool(vxn_softc_t *dp)
{
   rx_dma_buf_t *rxDesc = NULL;

   mutex_enter(&dp->rxlistlock);
   if (dp->rxFreeBufList) {
      rxDesc = dp->rxFreeBufList;
      dp->rxFreeBufList = rxDesc->next;
      ASSERT(dp->rxNumFreeBufs >= 1);
      dp->rxNumFreeBufs--;
   }
   mutex_exit(&dp->rxlistlock);

   if (!rxDesc) {
      /*
       * Try to allocate new descriptor from memory.  Can't block here
       * since we could be being called from interrupt context.
       */
      DPRINTF(5, (CE_NOTE, "%s%d: allocating rx buf from memory",
                  dp->drvName, dp->unit)); 
      if (!(rxDesc = Vxn_AllocRxBuf(dp, FALSE))) {
         cmn_err(CE_WARN,
                 "%s%d: Vxn_AllocRxBufFromPool : pool rx alloc failed",
                 dp->drvName, dp->unit);
         return NULL;
      }
   }

   /*
    * Allocate new message block for this buffer
    */
   rxDesc->mblk = desballoc((uchar_t *)rxDesc->dmaDesc.buf + ETHERALIGN,
                           rxDesc->dmaDesc.bufLen - ETHERALIGN, 
                           BPRI_MED, &rxDesc->freeCB);
   if (!rxDesc->mblk) {
      cmn_err(CE_WARN, "%s%d: Vxn_AllocRxBufFromPool : desballoc failed", 
                        dp->drvName, dp->unit);

      /* put back on free list */
      Vxn_FreeRxBufToPool(rxDesc);
      return NULL;
   }

   return rxDesc;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_FreeRxBufToPool --
 *    Return rx buffer to free pool
 *
 * Results:
 *    None   
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_FreeRxBufToPool(rx_dma_buf_t *rxDesc)
{
   vxn_softc_t *dp = rxDesc->softc;

   rxDesc->mblk = NULL;

   /*
    * Insert on free list, or free if the list is full
    */
   mutex_enter(&dp->rxlistlock);
   if (dp->rxNumFreeBufs >= dp->rxMaxFreeBufs) {
      DPRINTF(5, (CE_NOTE, "%s%d: freeing rx buf to memory", 
                  dp->drvName, dp->unit));
      Vxn_FreeRxBuf(rxDesc);
   } else {
      rxDesc->next = dp->rxFreeBufList;
      dp->rxFreeBufList = rxDesc;
      dp->rxNumFreeBufs++;
   }
   mutex_exit(&dp->rxlistlock);
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_AllocInitBuffers --
 *    Allocated Rx buffers and init ring entries
 *
 * Results:
 *    SOLVMXNET_SUCCESS - on success
 *    SOLVMXNET_FAILURE - on failure
 *
 * Side effects:
 *    None      
 *-----------------------------------------------------------------------------
 */
static int
Vxn_AllocInitBuffers(vxn_softc_t *dp)
{
   Vmxnet2_DriverData  *dd;
   uint32_t            i, offset;

   dd = dp->driverData;
   offset = sizeof(*dd);

   /*
    * Init shared structures
    */
   dd->rxRingLength = dp->vxnNumRxBufs;
   dd->rxRingOffset = offset;
   dp->rxRing = (Vmxnet2_RxRingEntry *)((uintptr_t)dd + offset);
   offset += dp->vxnNumRxBufs * sizeof(Vmxnet2_RxRingEntry);

   dd->rxRingLength2 = 1;
   dd->rxRingOffset2 = offset;
   offset += sizeof(Vmxnet2_RxRingEntry);

   dd->txRingLength = dp->vxnNumTxBufs;
   dd->txRingOffset = offset;
   dp->txRing = (Vmxnet2_TxRingEntry *)((uintptr_t)dd + offset);
   offset += dp->vxnNumTxBufs * sizeof(Vmxnet2_TxRingEntry);

   /*
    * Alloc Rx buffers pool
    */
   if ( Vxn_AllocRxBufPool(dp) != SOLVMXNET_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: Vxn_AllocInitBuffers: failed to alloc buf pool", 
                       dp->drvName, dp->unit);
      return SOLVMXNET_FAILURE;
   }

   /*
    * Allocate receive buffers
    */
   for (i = 0; i < dp->vxnNumRxBufs; i++) {
      rx_dma_buf_t *rxDesc;
      Vmxnet2_RxRingEntry *rre = &dp->rxRing[i];

      if (!(rxDesc = Vxn_AllocRxBufFromPool(dp))) {
         cmn_err(CE_WARN, "%s%d: Vxn_AllocInitBuffers: "
                          "failed to alloc buf from pool", dp->drvName, dp->unit);
         goto err;
      }

      /*
       * Init ring entries
       */
      rre->paddr = rxDesc->dmaDesc.phyBuf + ETHERALIGN;
      rre->bufferLength = MAXPKTBUF - ETHERALIGN;
      rre->actualLength = 0;
      dp->rxRingBuffPtr[i] = rxDesc;
      rre->ownership = VMXNET2_OWNERSHIP_NIC;
   }

   dp->txDmaHdl = NULL;

   /* 
    * Dummy recvRing2 tacked on to the end, with a single unusable entry 
    */
   dp->rxRing[i].paddr = 0;
   dp->rxRing[i].bufferLength = 0;
   dp->rxRing[i].actualLength = 0;
   dp->rxRingBuffPtr[i] = NULL;
   dp->rxRing[i].ownership = VMXNET2_OWNERSHIP_DRIVER;
   
   dd->rxDriverNext = 0;
                                                                               
   /*
    * Give xmit ring ownership to DRIVER
    */
   for (i = 0; i < dp->vxnNumTxBufs; i++) {
      dp->txRing[i].ownership = VMXNET2_OWNERSHIP_DRIVER;
      dp->txRingBuf[i].mblk = NULL;
      dp->txRingBuf[i].dmaMem.buf = NULL;
      dp->txRing[i].sg.sg[0].addrHi = 0;
   }
   
   dd->txDriverCur = dd->txDriverNext = 0;
   dd->txStopped = FALSE;

   return SOLVMXNET_SUCCESS;

err:
   for (i=0; i<dp->vxnNumRxBufs; i++) {
      if (dp->rxRingBuffPtr[i]) {
         Vxn_FreeRxBuf(dp->rxRingBuffPtr[i]);
         dp->rxRingBuffPtr[i] = NULL;
      }
   }

   Vxn_FreeRxBufPool(dp);
   return SOLVMXNET_FAILURE;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_FreeDmaMem --
 *    Free allocated dma memory
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_FreeDmaMem(dma_buf_t *dma)
{
   ddi_dma_unbind_handle(dma->dmaHdl);
   ddi_dma_mem_free(&dma->dataAccHdl);
   ddi_dma_free_handle(&dma->dmaHdl);

   dma->buf = NULL;
   dma->phyBuf = NULL;
   dma->bufLen = 0;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_AllocDmaMem --
 *    Allocate dma-able memory and fill passed in dma descriptor pointer 
 *    if successful
 *
 * Results:
 *    SOLVMXNET_SUCCESS on success
 *    SOLVMXNET_FAILURE on failure
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_AllocDmaMem(vxn_softc_t *dp, int size, int cansleep, dma_buf_t *dma)
{
   /* 
    * Allocate handle
    */
   if (ddi_dma_alloc_handle(dp->dip, &vxn_dma_attrs,
                            cansleep ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT, 
                            NULL, &dma->dmaHdl) != DDI_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: Vxn_AllocDmaMem: failed to allocate handle", 
              dp->drvName, dp->unit);
      return SOLVMXNET_FAILURE;
   }

   /* 
    * Allocate memory 
    */
   if (ddi_dma_mem_alloc(dma->dmaHdl, size, &vxn_buf_attrs, DDI_DMA_CONSISTENT,
                         cansleep ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT, NULL, 
                         &dma->buf, &dma->bufLen, &dma->dataAccHdl) 
       != DDI_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: Vxn_AllocDmaMem: "
                       "ddi_dma_mem_alloc %d bytes failed", 
                       dp->drvName, dp->unit, size);
      ddi_dma_free_handle(&dma->dmaHdl);
      return SOLVMXNET_FAILURE;
   }

   /*
    * Mapin memory
    */
   if (ddi_dma_addr_bind_handle(dma->dmaHdl, NULL, dma->buf, dma->bufLen,
                                DDI_DMA_RDWR | DDI_DMA_STREAMING,
                                cansleep ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT, 
                                NULL, &dma->cookie, &dma->cookieCount) 
       != DDI_DMA_MAPPED) {
      cmn_err(CE_WARN, "%s%d: Vxn_AllocDmaMem: failed to bind handle",
              dp->drvName, dp->unit);
      ddi_dma_mem_free(&dma->dataAccHdl);
      ddi_dma_free_handle(&dma->dmaHdl);
      return SOLVMXNET_FAILURE;
   }

   if (dma->cookieCount != 1) {
      cmn_err(CE_WARN, "%s%d: Vxn_AllocDmaMem: too many DMA cookies", 
                       dp->drvName, dp->unit);
      Vxn_FreeDmaMem(dma);
      return SOLVMXNET_FAILURE;
   }

   /*
    * Save physical address (for easy use)
    */
   dma->phyBuf = dma->cookie.dmac_address;

   return SOLVMXNET_SUCCESS;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_FreeDriverData --
 *   Free driver data structures and Tx Rx buffers
 *
 * Results:
 *   None
 *
 * Side effects:
 *   None
 *-----------------------------------------------------------------------------
 */
static void
Vxn_FreeDriverData(vxn_softc_t *dp)
{
   Vxn_FreeInitBuffers(dp);
   Vxn_FreeDmaMem(&dp->driverDataDmaMem);
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_AllocDriverData --
 *    Allocate driver data structures and Tx Rx buffers on init
 *
 * Results:
 *    SOLVMXNET_SUCCESS on success
 *    SOLVMXNET_FAILURE on failure
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */
static int
Vxn_AllocDriverData(vxn_softc_t *dp)
{
   uint32_t r, driverDataSize;

   /*
    * Get configured receive buffers
    */
   OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_GET_NUM_RX_BUFFERS);
   r = INL(dp, VMXNET_COMMAND_ADDR);
   if (r == 0 || r > MAX_NUM_RECV_BUFFERS) {
      r = DEFAULT_NUM_RECV_BUFFERS;
   }
   dp->vxnNumRxBufs = r;

   /*
    * Get configured transmit buffers
    */
   OUTL(dp, VMXNET_COMMAND_ADDR, VMXNET_CMD_GET_NUM_TX_BUFFERS);
   r = INL(dp, VMXNET_COMMAND_ADDR);
   if (r == 0 || r > MAX_NUM_XMIT_BUFFERS) {
      r = DEFAULT_NUM_XMIT_BUFFERS;
   }
   dp->vxnNumTxBufs = r;

   /*
    * Calculate shared data size and allocate memory for it
    */
   driverDataSize =
      sizeof(Vmxnet2_DriverData) +
      /* numRecvBuffers + 1 for the dummy recvRing2 (used only by Windows) */
      (dp->vxnNumRxBufs + 1) * sizeof(Vmxnet2_RxRingEntry) +
      dp->vxnNumTxBufs * sizeof(Vmxnet2_TxRingEntry);

   if (Vxn_AllocDmaMem(dp, driverDataSize, TRUE, &dp->driverDataDmaMem) 
        != SOLVMXNET_SUCCESS) {
      return SOLVMXNET_FAILURE;
   }

   /*
    * Clear memory (bzero isn't resolved by module loader for some reason) 
    */
   ASSERT(dp->driverDataDmaMem.buf && dp->driverDataDmaMem.bufLen);
   Vxn_Memset(dp->driverDataDmaMem.buf, 0, dp->driverDataDmaMem.bufLen);

   dp->driverData = (Vmxnet2_DriverData *)dp->driverDataDmaMem.buf;
   dp->driverDataPhy = (void *)(uintptr_t)dp->driverDataDmaMem.phyBuf;

   /* So that the vmkernel can check it is compatible */
   dp->driverData->magic = VMXNET2_MAGIC;
   dp->driverData->length = driverDataSize;

   /*
    * Alloc rx/tx buffers, init ring, register with hardware etc.
    */
   if (Vxn_AllocInitBuffers(dp) != SOLVMXNET_SUCCESS) {
      Vxn_FreeDmaMem(&dp->driverDataDmaMem);
      return SOLVMXNET_FAILURE;
   }

   DPRINTF(3, (CE_CONT, "%s%d: numRxBufs=(%d*%"FMT64"d) numTxBufs=(%d*%"FMT64"d)" 
               " driverDataSize=%d driverDataPhy=0x%p\n", 
               dp->drvName, dp->unit, 
               dp->vxnNumRxBufs, (uint64_t)sizeof(Vmxnet2_RxRingEntry),
               dp->vxnNumTxBufs, (uint64_t)sizeof(Vmxnet2_TxRingEntry),
               driverDataSize, dp->driverDataPhy));

   return SOLVMXNET_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 * Vxn_Attach --
 *    Probe and attach driver to stack
 *
 * Results:
 *    DDI_SUCCESS
 *    DDI_FAILURE
 *
 * Side effects:
 *    None 
 *-----------------------------------------------------------------------------
 */
static int
Vxn_Attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
   int                  i, ret, len, unit;
   const char           *drvName;
   ddi_acc_handle_t     confHdl;
   uint16_t             vid, did;
   uint8_t              revid;
   struct pci_phys_spec *regs;
   caddr_t              vxnIOp;
   ddi_acc_handle_t     vxnIOHdl;
   uint32_t             vLow, vHigh;
   gld_mac_info_t       *macInfo;
   vxn_softc_t          *dp;
   boolean_t            morphed = FALSE;
   uint_t               regSpaceSize;
   uint_t               chip;
   uint_t               vxnIOSize;

   if (cmd != DDI_ATTACH) {
      return DDI_FAILURE;
   }

   unit = ddi_get_instance(dip);
   drvName = ddi_driver_name(dip);

   /*
    * Check if chip is supported.
    */
   if (pci_config_setup(dip, &confHdl) != DDI_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: pci_config_setup() failed", drvName, unit);
      return DDI_FAILURE;
   }

   vid   = pci_config_get16(confHdl, PCI_CONF_VENID);
   did   = pci_config_get16(confHdl, PCI_CONF_DEVID);
   revid = pci_config_get8(confHdl, PCI_CONF_REVID);

   if (vid == PCI_VENDOR_ID_VMWARE && did == PCI_DEVICE_ID_VMWARE_NET) {
      /* Found vmxnet */
      chip = VMXNET_CHIP;
   }
   else if (vid == PCI_VENDOR_ID_AMD && did == PCI_DEVICE_ID_AMD_VLANCE) {
      /* Found vlance (maybe a vmxnet disguise) */
      chip = LANCE_CHIP;
   }
   else {
      /* Not Found */
      DPRINTF(3, (CE_WARN, "%s: Vxn_Attach: wrong PCI venid/devid (0x%x, 0x%x)",
              drvName, vid, did));
      goto err;
   }

   DPRINTF(3, (CE_CONT, "%s%d: (vid: 0x%04x, did: 0x%04x, revid: 0x%02x)\n",
           drvName, unit, vid, did, revid));

   /* 
    * Get device properties
    */
   regs = NULL;
   len  = 0;
   if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
                       "reg", (caddr_t)&regs, &len) != DDI_PROP_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: Vxn_Attach: failed to get reg property", 
              drvName, unit);
      goto err;
   }

   ASSERT(regs != NULL && len > 0);

   /* 
    * Search device properties for IO-space 
    */
   for (i = 0; i <len / sizeof(struct pci_phys_spec); i++) {
      if ((regs[i].pci_phys_hi & PCI_REG_ADDR_M) == PCI_ADDR_IO) {
         regSpaceSize = regs[i].pci_size_low;
         DPRINTF(5, (CE_CONT, "%s%d: Vxn_Attach: regSpaceSize=%d\n", 
                     drvName, unit, regSpaceSize));
         kmem_free(regs, len);
         goto map_space_found;
      }
   }

   cmn_err(CE_WARN, "%s%d: Vxn_Attach: failed to find IO space", drvName, unit);
   kmem_free(regs, len);
   goto err;

map_space_found:

   /* 
    * Ensure we can access registers through IO space. 
    */
   ret = pci_config_get16(confHdl, PCI_CONF_COMM);
   ret |= PCI_COMM_IO | PCI_COMM_ME;
   pci_config_put16(confHdl, PCI_CONF_COMM, ret);

   if (ddi_regs_map_setup(dip, i, (caddr_t *)&vxnIOp, 0, 0, &dev_attr, 
                          &vxnIOHdl) != DDI_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: Vxn_Attach: ddi_regs_map_setup failed", 
              drvName, unit);
      goto err;
   }

   if (chip == VMXNET_CHIP) {
      vxnIOSize = VMXNET_CHIP_IO_RESV_SIZE;
   }
   else {
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
      vxnIOp += LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE;
      vxnIOSize = LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE +
                  VMXNET_CHIP_IO_RESV_SIZE;
   }

   /*
    * Do not attempt to morph non-morphable AMD PCnet
    */
   if (vxnIOSize > regSpaceSize) {
      cmn_err(CE_WARN, "%s%d: Vxn_Attach: "
              "vlance device is not supported by this driver", drvName, unit);
      goto err_free_regs_map;
   }

   /*
    * Morph, if we found a vlance adapter
    */
   if (chip == LANCE_CHIP) {
      uint16_t magic;

      /* Read morph port to verify that we can morph the adapter */
      magic = ddi_get16(vxnIOHdl, (uint16_t *)(vxnIOp - MORPH_PORT_SIZE));
      if (magic != LANCE_CHIP && magic != VMXNET_CHIP) {
         cmn_err(CE_WARN, "%s%d: Vxn_Attach: Invalid magic, read: 0x%08X",
                 drvName, unit, magic);
         goto err_free_regs_map;
      }

      /* Morph */
      ddi_put16(vxnIOHdl, (uint16_t *)(vxnIOp - MORPH_PORT_SIZE), VMXNET_CHIP);
      morphed = TRUE;

      /* Verify that we morphed correctly */
      magic = ddi_get16(vxnIOHdl, (uint16_t *)(vxnIOp - MORPH_PORT_SIZE));
      if (magic != VMXNET_CHIP) {
         cmn_err(CE_WARN, "%s%d: Vxn_Attach: Couldn't morph adapter."
                 " Invalid magic, read:: 0x%08X", drvName, unit, magic);
         goto err_morph_back;
      }
   }

   /*
    * Check the version number of the device implementation
    */ 
   vLow  = (uint32_t)ddi_get32(vxnIOHdl, 
                   (uint32_t *)(vxnIOp+VMXNET_LOW_VERSION));
   vHigh = (uint32_t)ddi_get32(vxnIOHdl, 
                   (uint32_t *)(vxnIOp+VMXNET_HIGH_VERSION));

   if ((vLow & 0xffff0000) != (VMXNET2_MAGIC & 0xffff0000) || 
       ((VMXNET2_MAGIC < vLow) || (VMXNET2_MAGIC > vHigh))) {
      cmn_err(CE_WARN, "%s%d: Vxn_Attach: driver version 0x%08X doesn't "
                       "match device 0x%08X:0x%08X", 
              drvName, unit, VMXNET2_MAGIC, vLow, vHigh);
      goto err_version_mismatch;
   }

   /*
    * Alloc soft state
    */
   macInfo = gld_mac_alloc(dip);
   if (!macInfo) {
      cmn_err(CE_WARN, "%s%d: Vxn_Attach: gld_mac_alloc failed", 
              drvName, unit);
      goto err_gld_mac_alloc;
   }

   dp = (vxn_softc_t *) kmem_zalloc(sizeof(vxn_softc_t), KM_SLEEP);
   ASSERT(dp);

   /*
    * Get interrupt cookie
    */
   if (ddi_get_iblock_cookie(dip, 0, &dp->iblockCookie) != DDI_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: Vxn_Attach: ddi_get_iblock_cookie failed", 
              drvName, unit);
      goto err_get_iblock_cookie;
   }

   strncpy(dp->drvName, drvName, SOLVMXNET_MAXNAME);
   dp->unit = unit;
   dp->dip = dip;
   dp->macInfo = macInfo;
   dp->confHdl = confHdl;
   dp->vxnIOHdl = vxnIOHdl;
   dp->vxnIOp = vxnIOp;
   dp->morphed = morphed;
   dp->nicActive = FALSE;
   dp->txPending = 0;
   dp->maxTxFrags = 1;

   /*
    * Initialize mutexes
    */
   mutex_init(&dp->intrlock, NULL, MUTEX_DRIVER, (void *)dp->iblockCookie);
   mutex_init(&dp->xmitlock, NULL, MUTEX_DRIVER, (void *)dp->iblockCookie);
   mutex_init(&dp->rxlistlock, NULL, MUTEX_DRIVER, (void *)dp->iblockCookie);

   /* 
    * Allocate and initialize our private and shared data structures
    */
   if (Vxn_AllocDriverData(dp) != SOLVMXNET_SUCCESS) {
      goto err_alloc_driverdata;
   }

   /*
    * Read the MAC address from the device
    */
   for (i = 0; i < 6; i++) {
      dp->devAddr.ether_addr_octet[i] = 
         (uint8_t)ddi_get8(vxnIOHdl, (uint8_t *)(vxnIOp + VMXNET_MAC_ADDR + i));
   }
   macInfo->gldm_vendor_addr = dp->devAddr.ether_addr_octet;
   macInfo->gldm_broadcast_addr = etherbroadcastaddr.ether_addr_octet;

   DPRINTF(3, (CE_CONT,
           "MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dp->devAddr.ether_addr_octet[0],
           dp->devAddr.ether_addr_octet[1],
           dp->devAddr.ether_addr_octet[2],
           dp->devAddr.ether_addr_octet[3],
           dp->devAddr.ether_addr_octet[4],
           dp->devAddr.ether_addr_octet[5]));

   /*
    * Configure GLD entry points
    */
   macInfo->gldm_devinfo      = dip;
   macInfo->gldm_private      = (caddr_t)dp;
   macInfo->gldm_cookie       = dp->iblockCookie;
   macInfo->gldm_reset        = Vxn_Reset;
   macInfo->gldm_start        = Vxn_Start;
   macInfo->gldm_stop         = Vxn_Stop;
   macInfo->gldm_set_mac_addr = Vxn_SetMacAddress;
   macInfo->gldm_send         = Vxn_Send;
   macInfo->gldm_set_promiscuous = Vxn_SetPromiscuous;
   macInfo->gldm_get_stats    = Vxn_GetStats;
   macInfo->gldm_ioctl        = NULL;
   macInfo->gldm_set_multicast= Vxn_SetMulticast;
   macInfo->gldm_intr         = Vxn_Interrupt;
   macInfo->gldm_mctl         = NULL;
   
   macInfo->gldm_ident        = (char *)ddi_driver_name(dip);
   macInfo->gldm_type         = DL_ETHER;
   macInfo->gldm_minpkt       = 0;
   macInfo->gldm_maxpkt       = ETHERMTU;
   macInfo->gldm_addrlen      = ETHERADDRL;
   macInfo->gldm_saplen       = -2;
   macInfo->gldm_ppa          = unit;

   /*
    * Register with GLD (Generic Lan Driver) framework
    */
   if (gld_register(dip,
            (char *)ddi_driver_name(dip), macInfo) != DDI_SUCCESS) {
      goto err_gld_register;
   }

   /*
    * Add interrupt to system.
    */
   if (ddi_add_intr(dip, 0, NULL, NULL, gld_intr,
                    (caddr_t)macInfo) != DDI_SUCCESS) {
      cmn_err(CE_WARN, "%s%d: ddi_add_intr failed", drvName, unit);
      goto err_ddi_add_intr;
   }

   /*
    * Add to list of interfaces.
    */
   mutex_enter(&vxnListLock);
   dp->next = &vxnList;
   dp->prev = vxnList.prev;
   vxnList.prev->next = dp;
   vxnList.prev = dp;
   mutex_exit(&vxnListLock);

   /*
    * Success
    */
   return DDI_SUCCESS;

err_ddi_add_intr:
   gld_unregister(macInfo);

err_gld_register:
   Vxn_FreeDriverData(dp);

err_alloc_driverdata:
   mutex_destroy(&dp->intrlock);
   mutex_destroy(&dp->xmitlock);

err_get_iblock_cookie:
   kmem_free(dp, sizeof(*dp));
   gld_mac_free(macInfo);

err_gld_mac_alloc:
err_version_mismatch:
err_morph_back:
   if (morphed) {
      ddi_put16(vxnIOHdl, (uint16_t *)(vxnIOp - MORPH_PORT_SIZE), LANCE_CHIP);
   }

err_free_regs_map:
   ddi_regs_map_free(&vxnIOHdl);

err:
   pci_config_teardown(&confHdl);
   return DDI_FAILURE;
}

/*
 *-----------------------------------------------------------------------------
 * Vxn_Detach --
 *    Called on module unload
 *
 * Results:
 *    DDI_SUCCESS
 *    DDI_FAILURE
 *
 * Side effects:
 *    None   
 *-----------------------------------------------------------------------------
 */
static int
Vxn_Detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
   gld_mac_info_t *macInfo;
   vxn_softc_t    *dp;

   macInfo = (gld_mac_info_t *)ddi_get_driver_private(dip);
   dp = (vxn_softc_t *)macInfo->gldm_private;

   if (cmd == DDI_DETACH) {
      /*
       * Tear down interrupt
       */
      ddi_remove_intr(dip, 0,  macInfo->gldm_cookie);
      gld_unregister(macInfo);

      /*
       * Quiesce hardware
       */
      Vxn_Stop(macInfo);

      /*
       * Free driver-data, tx/rx buffers etc
       */
      Vxn_FreeDriverData(dp);

      /*
       * Destroy locks
       */
      mutex_destroy(&dp->intrlock);
      mutex_destroy(&dp->xmitlock);

      /*
       * Unmorph
       */
      if (dp->morphed) {
         uint16_t magic;

         /* Verify that we had morphed earlier */
         magic = ddi_get16(dp->vxnIOHdl, 
                           (uint16_t *)(dp->vxnIOp - MORPH_PORT_SIZE));
         if (magic != VMXNET_CHIP) {
            cmn_err(CE_WARN, "%s%d: Vxn_Detach: Adapter not morphed"
                             " magic=0x%08X", dp->drvName, dp->unit, magic);
         }
         else {
            /* Unmorph */
            ddi_put16(dp->vxnIOHdl, 
                      (uint16_t *)(dp->vxnIOp - MORPH_PORT_SIZE), LANCE_CHIP);

            /* Verify */
            magic = ddi_get16(dp->vxnIOHdl, 
                              (uint16_t *)(dp->vxnIOp - MORPH_PORT_SIZE));
            if (magic != LANCE_CHIP) {
               cmn_err(CE_WARN, "%s%d: Vxn_Detach: Unable to unmorph adapter"
                             " magic=0x%08X", dp->drvName, dp->unit, magic);
            }
         }
      }

      /*
       * Release resister mappings
       */ 
      ddi_regs_map_free(&dp->vxnIOHdl);
      pci_config_teardown(&dp->confHdl);

      /*
       * Remove from list of interfaces.
       */
      mutex_enter(&vxnListLock);
      ASSERT(dp != &vxnList);
      dp->prev->next = dp->next;
      dp->next->prev = dp->prev;
      mutex_exit(&vxnListLock);

      /*
       * Release memory
       */
      kmem_free(dp, sizeof(*dp));
      gld_mac_free(macInfo);

      return DDI_SUCCESS;
   }
   else {
      return DDI_FAILURE;
   }
}

static   struct module_info vxnminfo = {
  0,                    /* mi_idnum */
  "vmxnet",             /* mi_idname */
  0,                    /* mi_minpsz */
  ETHERMTU,             /* mi_maxpsz */
  QHIWATER,             /* mi_hiwat */
  1,                    /* mi_lowat */
};

static   struct qinit vxnrinit = {
   NULL,                /* qi_putp */
   gld_rsrv,            /* qi_srvp */
   gld_open,            /* qi_qopen */
   gld_close,           /* qi_qclose */
   NULL,                /* qi_qadmin */
   &vxnminfo,           /* qi_minfo */
   NULL                 /* qi_mstat */
};

static   struct qinit vxnwinit = {
   gld_wput,            /* qi_putp */
   gld_wsrv,            /* qi_srvp */
   NULL,                /* qi_qopen */
   NULL,                /* qi_qclose */
   NULL,                /* qi_qadmin */
   &vxnminfo,           /* qi_minfo */
   NULL                 /* qi_mstat */
};

static struct streamtab vxn_info = {
   &vxnrinit,           /* st_rdinit */
   &vxnwinit,           /* st_wrinit */
   NULL,                /* st_muxrinit */
   NULL                 /* st_muxwrinit */
};

static   struct cb_ops cb_vxn_ops = {
   nulldev,             /* cb_open */
   nulldev,             /* cb_close */
   nodev,               /* cb_strategy */
   nodev,               /* cb_print */
   nodev,               /* cb_dump */
   nodev,               /* cb_read */
   nodev,               /* cb_write */
   nodev,               /* cb_ioctl */
   nodev,               /* cb_devmap */
   nodev,               /* cb_mmap */
   nodev,               /* cb_segmap */
   nochpoll,            /* cb_chpoll */
   ddi_prop_op,         /* cb_prop_op */
   &vxn_info,           /* cb_stream */
   D_NEW|D_MP           /* cb_flag */
};

static   struct dev_ops vxn_ops = {
   DEVO_REV,            /* devo_rev */
   0,                   /* devo_refcnt */
   gld_getinfo,         /* devo_getinfo */
   nulldev,             /* devo_identify */
   nulldev,             /* devo_probe */
   Vxn_Attach,          /* devo_attach */
   Vxn_Detach,          /* devo_detach */
   nodev,               /* devo_reset */
   &cb_vxn_ops,         /* devo_cb_ops */
   NULL,                /* devo_bus_ops */
   ddi_power            /* devo_power */
};

static struct modldrv modldrv = {
   &mod_driverops,
   ident,
   &vxn_ops,
};

static struct modlinkage modlinkage = {
   MODREV_1, {&modldrv, NULL,}
};


/*
 * Module load entry point
 */
int
_init(void)
{
   int err;

   DPRINTF(5, (CE_CONT, "vxn: _init:\n"));
   /* Initialize interface list */
   vxnList.next = vxnList.prev = &vxnList;
   mutex_init(&vxnListLock, NULL, MUTEX_DRIVER, NULL);
   if ((err = mod_install(&modlinkage)) != 0) {
      mutex_destroy(&vxnListLock);
   }
   return err;
}

/*
 * Module unload entry point
 */
int
_fini(void)
{
   int err;

   DPRINTF(5, (CE_CONT, "vxn: _fini:\n"));
   if ((err = mod_remove(&modlinkage)) == 0) {
      mutex_destroy(&vxnListLock);
   }
   return err;
}

/*
 * Module info entry point
 */
int
_info(struct modinfo *modinfop)
{
   return (mod_info(&modlinkage, modinfop));
}

