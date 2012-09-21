/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>

/*
 * FreeBSD 5.3 introduced an MP-safe (multiprocessor-safe) network stack.
 * I.e., drivers must either request Giant's protection or handle locking
 * themselves.  For performance reasons, the vmxnet driver is now MPSAFE.
 */
#if __FreeBSD_version >= 503000
#   define VXN_MPSAFE
#   include <sys/lock.h>
#   include <sys/mutex.h>
#endif

/*
 * FreeBSD 7.0-RELEASE changed the bus_setup_intr API to include a device_filter_t
 * parameter.
 */
#if __FreeBSD_version >= 700031
#   define VXN_NEWNEWBUS
#endif

#if __FreeBSD_version < 600000
#include <machine/bus_pio.h>
#else
#include <net/if_types.h>
#endif
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#if __FreeBSD__ >= 5
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#else
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#endif

/* define INLINE the way gcc likes it */
#define INLINE __inline__

#ifndef VMX86_TOOLS
#define VMX86_TOOLS
#endif
#include "vm_basic_types.h"
#include "vmxnet_def.h"
#include "vmxnet2_def.h"
#include "vm_device_version.h"
#include "net_compat.h"

#define VMXNET_ID_STRING "VMware PCI Ethernet Adpater"
#define CRC_POLYNOMIAL_LE 0xedb88320UL  /* Ethernet CRC, little endian */
#define ETHER_ALIGN  2

/* number of milliseconds to wait for pending transmits to complete on stop */
#define MAX_TX_WAIT_ON_STOP 2000

static int vxn_probe (device_t);
static int vxn_attach (device_t);
static int vxn_detach (device_t);

typedef struct vxn_softc {
#ifdef VXN_NEEDARPCOM
   struct arpcom            arpcom;
#else
   struct ifnet            *vxn_ifp;
   struct ifmedia           media;
#endif
#ifdef VXN_MPSAFE
   struct mtx               vxn_mtx;
#endif
   struct resource         *vxn_io;
   bus_space_handle_t	    vxn_iobhandle;
   bus_space_tag_t	    vxn_iobtag;
   struct resource         *vxn_irq;
   void			   *vxn_intrhand;
   Vmxnet2_DriverData      *vxn_dd;
   uint32                   vxn_dd_phys;
   int                      vxn_num_rx_bufs;
   int                      vxn_num_tx_bufs;
   Vmxnet2_RxRingEntry     *vxn_rx_ring;
   Vmxnet2_TxRingEntry     *vxn_tx_ring;
   int                      vxn_tx_pending;
   int                      vxn_rings_allocated;
   uint32                   vxn_max_tx_frags;

   struct mbuf             *vxn_tx_buffptr[VMXNET2_MAX_NUM_TX_BUFFERS];
   struct mbuf             *vxn_rx_buffptr[VMXNET2_MAX_NUM_RX_BUFFERS];

} vxn_softc_t;

/*
 * Driver entry points
 */
static void vxn_init(void *);
static void vxn_start(struct ifnet *);
static int vxn_ioctl(struct ifnet *, u_long, caddr_t);
#if __FreeBSD_version < 900000
static void vxn_watchdog(struct ifnet *);
#endif
static void vxn_intr (void *);

static void vxn_rx(vxn_softc_t *sc);
static void vxn_tx_complete(vxn_softc_t *sc);
static int vxn_init_rings(vxn_softc_t *sc);
static void vxn_release_rings(vxn_softc_t *);
static void vxn_stop(vxn_softc_t *);

/*
 * Locked counterparts above functions
 */
static void vxn_initl(vxn_softc_t *);
static void vxn_startl(struct ifnet *);
static void vxn_stopl(vxn_softc_t *);

static device_method_t vxn_methods[] = {
   DEVMETHOD(device_probe,	vxn_probe),
   DEVMETHOD(device_attach,	vxn_attach),
   DEVMETHOD(device_detach,	vxn_detach),

   { 0, 0 }
};

static driver_t vxn_driver = {
   "vxn",
   vxn_methods,
   sizeof(struct vxn_softc)
};

static devclass_t vxn_devclass;

MODULE_DEPEND(if_vxn, pci, 1, 1, 1);
DRIVER_MODULE(if_vxn, pci, vxn_driver, vxn_devclass, 0, 0);

/*
 *-----------------------------------------------------------------------------
 * vxn_probe --
 *      Probe device. Called when module is loaded
 *
 * Results:
 *      Returns 0 for success, negative errno value otherwise.
 *
 * Side effects:
 *      Register device name with OS
 *-----------------------------------------------------------------------------
 */
static int
vxn_probe(device_t dev)
{
   if ((pci_get_vendor(dev) == PCI_VENDOR_ID_VMWARE) &&
       (pci_get_device(dev) == PCI_DEVICE_ID_VMWARE_NET)) {
      device_set_desc(dev, VMXNET_ID_STRING);
      return 0;
   }

   return ENXIO;
}

/*
 *-----------------------------------------------------------------------------
 * vxn_execute_4 --
 *      Execute command returing 4 bytes on vmxnet.  Used to retrieve
 *      number of TX/RX buffers and to get hardware capabilities and
 *      features.
 *
 * Results:
 *      Returns value reported by hardware.
 *
 * Side effects:
 *      All commands supported are read-only, so no side effects.
 *-----------------------------------------------------------------------------
 */
static u_int32_t
vxn_execute_4(const vxn_softc_t *sc,	/* IN: adapter */
              u_int32_t cmd)		/* IN: command */
{
   bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                     VMXNET_COMMAND_ADDR, cmd);
   return bus_space_read_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                           VMXNET_COMMAND_ADDR);
}

static int
vxn_check_link(vxn_softc_t *sc)
{
   uint32 status;
   int ok;

   status = bus_space_read_4(sc->vxn_iobtag, sc->vxn_iobhandle, VMXNET_STATUS_ADDR);
   ok = (status & VMXNET_STATUS_CONNECTED) != 0;
   return ok;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vxn_media_status --
 *
 *      This routine is called when the user quries the status of interface
 *      using ifconfig. Checks link state and updates media state accorgingly.
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
vxn_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
   vxn_softc_t *sc = ifp->if_softc;
   int connected = 0;

   VXN_LOCK((vxn_softc_t *)ifp->if_softc);
   connected = vxn_check_link(sc);

   ifmr->ifm_status = IFM_AVALID;
   ifmr->ifm_active = IFM_ETHER;

   if (!connected) {
      ifmr->ifm_status &= ~IFM_ACTIVE;
      VXN_UNLOCK((vxn_softc_t *)ifp->if_softc);
      return;
   }

   ifmr->ifm_status |= IFM_ACTIVE;

   VXN_UNLOCK((vxn_softc_t *)ifp->if_softc);
   return;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vxn_media_change --
 *
 *      This routine is called when the user changes speed/duplex using
 *      media/mediopt option with ifconfig.
 *
 * Results:
 *      Returns 0 for success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
vxn_media_change(struct ifnet * ifp)
{
   vxn_softc_t *sc = ifp->if_softc;
   struct ifmedia *ifm = &sc->media;

   if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
      return (EINVAL);

   if (IFM_SUBTYPE(ifm->ifm_media) != IFM_AUTO)
      printf("Media subtype is not AUTO, it is : %d.\n",
             IFM_SUBTYPE(ifm->ifm_media));

   return (0);
}


/*
 *-----------------------------------------------------------------------------
 * vxn_attach --
 *      Initialize data structures and attach driver to stack
 *
 * Results:
 *      Returns 0 for success, negative errno value otherwise.
 *
 * Side effects:
 *      Check device version number. Map interrupts.
 *-----------------------------------------------------------------------------
 */
static int
vxn_attach(device_t dev)
{
   struct ifnet *ifp = NULL;
   int error = 0;
   int s, i;
   vxn_softc_t *sc;
   int unit;
   int rid;
   u_int32_t r;
   u_int32_t vLow, vHigh;
   int driverDataSize;
   u_char mac[6];

   s = splimp();

   unit = device_get_unit(dev);

   sc = device_get_softc(dev);
   VXN_MTX_INIT(&sc->vxn_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
                MTX_DEF);
   sc->vxn_io = NULL;
   sc->vxn_irq = NULL;
   sc->vxn_intrhand = NULL;
   sc->vxn_dd = NULL;
   sc->vxn_tx_pending = 0;
   sc->vxn_rings_allocated = 0;
   sc->vxn_max_tx_frags = 1;

   pci_enable_busmaster(dev);

   /*
    * enable the I/O ports on the device
    */
   pci_enable_io(dev, SYS_RES_IOPORT);
   r = pci_read_config(dev, PCIR_COMMAND, 4);
   if (!(r & PCIM_CMD_PORTEN)) {
      printf("vxn%d: failed to enable I/O ports\n", unit);
      error = ENXIO;
      goto fail;
   }
   rid = VXN_PCIR_MAPS;
   sc->vxn_io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
                                   1, RF_ACTIVE);
   if (sc->vxn_io == NULL) {
      printf ("vxn%d: couldn't map I/O ports\n", unit);
      error = ENXIO;
      goto fail;
   }
   sc->vxn_iobtag = rman_get_bustag(sc->vxn_io);
   sc->vxn_iobhandle = rman_get_bushandle(sc->vxn_io);

   /*
    * check the version number of the device implementation
    */
   vLow = bus_space_read_4(sc->vxn_iobtag, sc->vxn_iobhandle, VMXNET_LOW_VERSION);
   vHigh = bus_space_read_4(sc->vxn_iobtag, sc->vxn_iobhandle, VMXNET_HIGH_VERSION);
   if ((vLow & 0xffff0000) != (VMXNET2_MAGIC & 0xffff0000)) {
      printf("vxn%d: driver version 0x%08X doesn't match %s version 0x%08X\n",
             unit, VMXNET2_MAGIC, "VMware", rid);
      error = ENXIO;
      goto fail;
   } else {
      if ((VMXNET2_MAGIC < vLow) ||
          (VMXNET2_MAGIC > vHigh)) {
         printf("vxn%d: driver version 0x%08X doesn't match %s version 0x%08X,0x%08X\n",
                unit, VMXNET2_MAGIC, "VMware", vLow, vHigh);
         error = ENXIO;
         goto fail;
      }
   }

   /*
    * map interrupt for the the device
    */
   rid = 0;
   sc->vxn_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0,
                                    1, RF_SHAREABLE | RF_ACTIVE);
   if (sc->vxn_irq == NULL) {
      printf("vxn%d: couldn't map interrupt\n", unit);
      error = ENXIO;
      goto fail;
   }
#if defined(VXN_NEWNEWBUS)
   error = bus_setup_intr(dev, sc->vxn_irq, INTR_TYPE_NET | INTR_MPSAFE,
                          NULL, vxn_intr, sc, &sc->vxn_intrhand);
#elif defined(VXN_MPSAFE)
   error = bus_setup_intr(dev, sc->vxn_irq, INTR_TYPE_NET | INTR_MPSAFE,
			  vxn_intr, sc, &sc->vxn_intrhand);
#else 
   error = bus_setup_intr(dev, sc->vxn_irq, INTR_TYPE_NET,
			  vxn_intr, sc, &sc->vxn_intrhand);
#endif
   if (error) {
      printf("vxn%d: couldn't set up irq\n", unit);
      error = ENXIO;
      goto fail;
   }

   /*
    * allocate and initialize our private and shared data structures
    */
   r = vxn_execute_4(sc, VMXNET_CMD_GET_NUM_RX_BUFFERS);
   if (r == 0 || r > VMXNET2_MAX_NUM_RX_BUFFERS) {
      r = VMXNET2_DEFAULT_NUM_RX_BUFFERS;
   }
   sc->vxn_num_rx_bufs = r;

   r = vxn_execute_4(sc, VMXNET_CMD_GET_NUM_TX_BUFFERS);
   if (r == 0 || r > VMXNET2_MAX_NUM_TX_BUFFERS) {
      r = VMXNET2_DEFAULT_NUM_TX_BUFFERS;
   }
   sc->vxn_num_tx_bufs = r;

   driverDataSize =
      sizeof(Vmxnet2_DriverData) +
      /* numRxBuffers + 1 for the dummy rxRing2 (used only by Windows) */
      (sc->vxn_num_rx_bufs + 1) * sizeof(Vmxnet2_RxRingEntry) +
      sc->vxn_num_tx_bufs * sizeof(Vmxnet2_TxRingEntry);

   sc->vxn_dd = contigmalloc(driverDataSize, M_DEVBUF, M_NOWAIT,
                             0, 0xffffffff, PAGE_SIZE, 0);
   if (sc->vxn_dd == NULL) {
      printf("vxn%d: can't contigmalloc %d bytes for vxn_dd\n",
             unit, driverDataSize);
      error = ENOMEM;
      goto fail;
   }

   memset(sc->vxn_dd, 0, driverDataSize);

   /* So that the vmkernel can check it is compatible */
   sc->vxn_dd->magic = VMXNET2_MAGIC;
   sc->vxn_dd->length = driverDataSize;

   /* This downcast is OK because we've asked for vxn_dd to fit in 32 bits */
   sc->vxn_dd_phys = (uint32)vtophys(sc->vxn_dd);

   /*
    * set up entry points, data and defaults for the kernel
    */
   ifp = VXN_IF_ALLOC(sc);
   if (ifp == NULL) {
      printf("vxn%d: if_alloc() failed\n", unit);
      error = ENOMEM;
      goto fail;
   }
   ifp->if_softc = sc;
   VXN_IF_INITNAME(ifp, device_get_name(dev), unit);
   ifp->if_mtu = ETHERMTU;
   ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
   ifp->if_ioctl = vxn_ioctl;
   ifp->if_output = ether_output;
   ifp->if_start = vxn_start;
#if __FreeBSD_version < 900000
   ifp->if_watchdog = vxn_watchdog;
#endif
   ifp->if_init = vxn_init;
   ifp->if_baudrate = 1000000000;
   ifp->if_snd.ifq_maxlen = sc->vxn_num_tx_bufs;
   ifp->if_capenable = ifp->if_capabilities;

   /*
    * read the MAC address from the device
    */
   for (i = 0; i < 6; i++) {
      mac[i] = bus_space_read_1(sc->vxn_iobtag, sc->vxn_iobhandle, VMXNET_MAC_ADDR
                                + i);
   }

#ifdef VXN_NEEDARPCOM
   /*
    * FreeBSD 4.x requires that we manually record the device's MAC address to
    * the attached arpcom structure prior to calling ether_ifattach().
    */
   bcopy(mac, sc->arpcom.ac_enaddr, 6);
#endif

   /*
    * success
    */
   VXN_ETHER_IFATTACH(ifp, mac);
   printf("vxn%d: attached [num_rx_bufs=(%d*%d) num_tx_bufs=(%d*%d) driverDataSize=%d]\n",
          unit,
          sc->vxn_num_rx_bufs, (int)sizeof(Vmxnet2_RxRingEntry),
          sc->vxn_num_tx_bufs, (int)sizeof(Vmxnet2_TxRingEntry),
          driverDataSize);

   /*
    * Specify the media types supported by this adapter and register
    * callbacks to update media and link information
    */
   ifmedia_init(&sc->media, IFM_IMASK, vxn_media_change,
                vxn_media_status);
   ifmedia_add(&sc->media, IFM_ETHER | IFM_FDX, 0, NULL);
   ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
   ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
   ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
   ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);


   goto done;

fail:

   if (sc->vxn_intrhand != NULL) {
      bus_teardown_intr(dev, sc->vxn_irq, sc->vxn_intrhand);
   }
   if (sc->vxn_irq != NULL) {
      bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vxn_irq);
   }
   if (sc->vxn_io != NULL) {
      bus_release_resource(dev, SYS_RES_IOPORT, VXN_PCIR_MAPS, sc->vxn_io);
   }
   if (sc->vxn_dd != NULL) {
      contigfree(sc->vxn_dd, sc->vxn_dd->length, M_DEVBUF);
   }
   if (ifp != NULL) {
      VXN_IF_FREE(sc);
   }

   pci_disable_io(dev, SYS_RES_IOPORT);
   pci_disable_busmaster(dev);
   VXN_MTX_DESTROY(&sc->vxn_mtx);

  done:

   splx(s);
   return error;
}

/*
 *-----------------------------------------------------------------------------
 * vxn_detach --
 *      Free data structures and detach driver from stack
 *
 * Results:
 *      Returns 0 for success (always)
 *
 * Side effects:
 *	None
 *-----------------------------------------------------------------------------
 */
static int
vxn_detach(device_t dev)
{
   int s;
   vxn_softc_t *sc;
   struct ifnet *ifp;

   s = splimp();

   sc = device_get_softc(dev);

   ifp = VXN_SC2IFP(sc);
   if (device_is_attached(dev)) {
      vxn_stop(sc);
      /*
       * detach from stack
       */
      VXN_ETHER_IFDETACH(ifp);
   }

   /*
    * Cleanup - release resources and memory
    */
   VXN_IF_FREE(sc);
   contigfree(sc->vxn_dd, sc->vxn_dd->length, M_DEVBUF);
   bus_teardown_intr(dev, sc->vxn_irq, sc->vxn_intrhand);
   bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vxn_irq);
   bus_release_resource(dev, SYS_RES_IOPORT, VXN_PCIR_MAPS, sc->vxn_io);
   pci_disable_io(dev, SYS_RES_IOPORT);
   pci_disable_busmaster(dev);
   VXN_MTX_DESTROY(&sc->vxn_mtx);

   splx(s);
   return 0;
}

/*
 *-----------------------------------------------------------------------------
 * vxn_stop --
 *      Called when the interface is brought down
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
static void
vxn_stop(vxn_softc_t *sc)
{
   VXN_LOCK(sc);
   vxn_stopl(sc);
   VXN_UNLOCK(sc);
}

/*
 *-----------------------------------------------------------------------------
 * vxn_stopl --
 *      Called when the interface is brought down & is locked
 *
 * Results:
 *      None
 *
 * Side effects:
 *	Don't do anything if not running. Flush pending transmits. Release
 *      private data structures.
 *-----------------------------------------------------------------------------
 */
static void
vxn_stopl(vxn_softc_t *sc)
{
   int i;
   struct ifnet *ifp = VXN_SC2IFP(sc);

   VXN_LOCK_ASSERT(sc);

   if (!(VXN_GET_IF_DRV_FLAGS(ifp) & VXN_IFF_RUNNING)) {
      return;
   }

   /*
    * Disable device interrupts
    */
   bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
		     VMXNET_COMMAND_ADDR, VMXNET_CMD_INTR_DISABLE);

   /*
    * Try to flush pending transmits
    */
   if (sc->vxn_tx_pending) {
      printf("vxn%d: waiting for %d pending transmits\n",
             VXN_IF_UNIT(ifp), sc->vxn_tx_pending);
      for (i = 0; i < MAX_TX_WAIT_ON_STOP && sc->vxn_tx_pending; i++) {
         DELAY(1000);
         bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                           VMXNET_COMMAND_ADDR, VMXNET_CMD_CHECK_TX_DONE);
         vxn_tx_complete(sc);
      }
      if (sc->vxn_tx_pending) {
         printf("vxn%d: giving up on %d pending transmits\n",
                VXN_IF_UNIT(ifp), sc->vxn_tx_pending);
      }
   }

   /*
    * Stop hardware
    */
   bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                     VMXNET_INIT_ADDR, 0);

   VXN_CLR_IF_DRV_FLAGS(ifp, VXN_IFF_RUNNING);

   /*
    * Free ring
    */
   vxn_release_rings(sc);
}

/*
 *-----------------------------------------------------------------------------
 * vxn_load_multicast --
 *      Called to change set of addresses to listen to.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	Sets device multicast table
 *-----------------------------------------------------------------------------
 */
static int
vxn_load_multicast(vxn_softc_t *sc)
{
   struct ifmultiaddr *ifma;
   struct ifnet *ifp = VXN_SC2IFP(sc);
   Vmxnet2_DriverData *dd = sc->vxn_dd;
   volatile uint16 *mcast_table = (uint16 *)dd->LADRF;
   int i, bit, byte;
   uint32 crc, poly = CRC_POLYNOMIAL_LE;
   int any = 0;

   if (ifp->if_flags & IFF_ALLMULTI) {
        dd->LADRF[0] = 0xffffffff;
        dd->LADRF[1] = 0xffffffff;

        any++;
	goto done;
   }

   dd->LADRF[0] = 0;
   dd->LADRF[1] = 0;

   VXN_IF_ADDR_LOCK(ifp);
   for (ifma = VXN_IFMULTI_FIRST(&ifp->if_multiaddrs);
        ifma != NULL;
        ifma = VXN_IFMULTI_NEXT(ifma, ifma_link)) {
      char *addrs = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);

      if (ifma->ifma_addr->sa_family != AF_LINK)
         continue;

      any++;
      crc = 0xffffffff;
      for (byte = 0; byte < 6; byte++) {
         for (bit = *addrs++, i = 0; i < 8; i++, bit >>= 1) {
            int test;

            test = ((bit ^ crc) & 0x01);
            crc >>= 1;

            if (test) {
               crc = crc ^ poly;
            }
         }
      }

      crc = crc >> 26;
      mcast_table[crc >> 4] |= 1 << (crc & 0xf);
   }
   VXN_IF_ADDR_UNLOCK(ifp);

 done:
   if (VXN_GET_IF_DRV_FLAGS(ifp) & VXN_IFF_RUNNING) {
      bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                        VMXNET_COMMAND_ADDR, VMXNET_CMD_UPDATE_LADRF);
   }
   return any;
}

/*
 *-----------------------------------------------------------------------------
 * vxn_init --
 *      Called when the interface is brought up.
 *
 * Results:
 *      None
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static void
vxn_init(void *v)
{
   vxn_softc_t *sc = (vxn_softc_t *)v;
   VXN_LOCK(sc);
   vxn_initl(sc);
   VXN_UNLOCK(sc);
}

/*
 *-----------------------------------------------------------------------------
 * vxn_initl --
 *      Called by vxn_init() after lock acquired.
 *
 * Results:
 *      None
 *
 * Side effects:
 *	Initialize rings, Register driver data structures with device,
 *      Enable interrupts on device.
 *
 *-----------------------------------------------------------------------------
 */

static void
vxn_initl(vxn_softc_t *sc)
{
   Vmxnet2_DriverData *dd = sc->vxn_dd;
   struct ifnet *ifp = VXN_SC2IFP(sc);
   uint32 r, i;
   u_char mac_addr[6];

   VXN_LOCK_ASSERT(sc);

   if (!(VXN_GET_IF_DRV_FLAGS(ifp) & VXN_IFF_RUNNING)) {
      u_int32_t capabilities;
      u_int32_t features;

      if (vxn_init_rings(sc) != 0) {
         printf("vxn%d: ring intitialization failed\n", VXN_IF_UNIT(ifp));
         return;
      }

      /* Get MAC address from interface and set it in hardware */
#if __FreeBSD_version >= 700000
      printf("addrlen : %d. \n", ifp->if_addrlen);
      bcopy(LLADDR((struct sockaddr_dl *)ifp->if_addr->ifa_addr), mac_addr,
            ifp->if_addrlen > 6 ? 6 : ifp->if_addrlen);
#else
      if (!ifaddr_byindex(ifp->if_index)) {
         printf("vxn:%d Invalid link address, interface index :%d.\n",
                VXN_IF_UNIT(ifp), ifp->if_index);
      } else {
         bcopy(LLADDR((struct sockaddr_dl *)ifaddr_byindex(ifp->if_index)->ifa_addr),
               mac_addr, ifp->if_addrlen);
      }
#endif
      printf("vxn%d: MAC Address : %02x:%02x:%02x:%02x:%02x:%02x \n",
             VXN_IF_UNIT(ifp), mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
      for (i = 0; i < 6; i++) {
         bus_space_write_1(sc->vxn_iobtag, sc->vxn_iobhandle, VMXNET_MAC_ADDR +
                           i, mac_addr[i]);
      }

      /*
       * Start hardware
       */
      bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                        VMXNET_INIT_ADDR, sc->vxn_dd_phys);
      bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                        VMXNET_INIT_LENGTH, sc->vxn_dd->length);

      /* Make sure the initialization succeeded for the hardware. */
      r = bus_space_read_4(sc->vxn_iobtag, sc->vxn_iobhandle, VMXNET_INIT_LENGTH);
      if (!r) {
         vxn_release_rings(sc);
         printf("vxn%d: device intitialization failed: %x\n", VXN_IF_UNIT(ifp), r);
         return;
      }
      capabilities = vxn_execute_4(sc, VMXNET_CMD_GET_CAPABILITIES);
      features = vxn_execute_4(sc, VMXNET_CMD_GET_FEATURES);
      if ((capabilities & VMNET_CAP_SG) &&
          (features & VMXNET_FEATURE_ZERO_COPY_TX)) {
         sc->vxn_max_tx_frags = VMXNET2_SG_DEFAULT_LENGTH;
      } else {
         sc->vxn_max_tx_frags = 1;
      }

      VXN_SET_IF_DRV_FLAGS(ifp, VXN_IFF_RUNNING);
      VXN_CLR_IF_DRV_FLAGS(ifp, VXN_IFF_OACTIVE);
   }

   dd->ifflags &= ~(VMXNET_IFF_PROMISC
                    |VMXNET_IFF_BROADCAST
                    |VMXNET_IFF_MULTICAST);

   if (ifp->if_flags & IFF_PROMISC) {
      printf("vxn%d: promiscuous mode enabled\n", VXN_IF_UNIT(ifp));
      dd->ifflags |= VMXNET_IFF_PROMISC;
   }
   if (ifp->if_flags & IFF_BROADCAST) {
      dd->ifflags |= VMXNET_IFF_BROADCAST;
   }
   /*
    * vnx_load_multicast does the right thing for IFF_ALLMULTI
    */
   if (vxn_load_multicast(sc)) {
      dd->ifflags |= VMXNET_IFF_MULTICAST;
   }

   /*
    * enable interrupts on the card
    */
   bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                     VMXNET_COMMAND_ADDR, VMXNET_CMD_INTR_ENABLE);

   bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
		     VMXNET_COMMAND_ADDR, VMXNET_CMD_UPDATE_IFF);
}

/*
 *-----------------------------------------------------------------------------
 * vxn_encap --
 *     Stick packet address and length in given ring entry
 *
 * Results:
 *      0 on success, 1 on error
 *
 * Side effects:
 *	Allocate a new mbuf cluster and copy data, if mbuf chain is too
 *	fragmented for us to include in our scatter/gather array
 *
 *-----------------------------------------------------------------------------
 */
static int
vxn_encap(struct ifnet *ifp,
	  Vmxnet2_TxRingEntry *xre,
	  struct mbuf *m_head,
	  struct mbuf **pbuffptr)
{
   vxn_softc_t *sc = ifp->if_softc;
   int frag = 0;
   struct mbuf *m;

   xre->sg.length = 0;
   xre->flags = 0;

   /*
    * Go through mbuf chain and drop packet pointers into ring
    * scatter/gather array
    */
   for (m = m_head; m != NULL; m = m->m_next) {
      if (m->m_len) {
         if (frag == sc->vxn_max_tx_frags) {
            break;
         }

         xre->sg.sg[frag].addrLow = (uint32)vtophys(mtod(m, vm_offset_t));
         xre->sg.sg[frag].length = m->m_len;
         frag++;
      }
   }

   /*
    * Allocate a new mbuf cluster and copy data if we can't use the mbuf chain
    * as such
    */
   if (m != NULL) {
      struct mbuf    *m_new = NULL;

      MGETHDR(m_new, M_DONTWAIT, MT_DATA);
      if (m_new == NULL) {
         printf("vxn%d: no memory for tx list\n", VXN_IF_UNIT(ifp));
         return 1;
      }

      if (m_head->m_pkthdr.len > MHLEN) {
         MCLGET(m_new, M_DONTWAIT);
         if (!(m_new->m_flags & M_EXT)) {
            m_freem(m_new);
            printf("vxn%d: no memory for tx list\n", VXN_IF_UNIT(ifp));
            return 1;
         }
      }

      m_copydata(m_head, 0, m_head->m_pkthdr.len,
          mtod(m_new, caddr_t));
      m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
      m_freem(m_head);
      m_head = m_new;

      xre->sg.sg[0].addrLow = (uint32)vtophys(mtod(m_head, vm_offset_t));
      xre->sg.sg[0].length = m_head->m_pkthdr.len;
      frag = 1;
   }

   xre->sg.length = frag;

   /*
    * Mark ring entry as "NIC owned"
    */
   if (frag > 0) {
      if (m_head->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP)) {
         xre->flags |= VMXNET2_TX_HW_XSUM;
      }
      xre->sg.addrType = NET_SG_PHYS_ADDR;
      *pbuffptr = m_head;
      xre->ownership = VMXNET2_OWNERSHIP_NIC;
      xre->flags |= VMXNET2_TX_CAN_KEEP;
   }

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 * vxn_start --
 *     Called to transmit a packet.  Acquires device mutex & hands off to
 *     vxn_startl.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
vxn_start(struct ifnet *ifp)
{
   VXN_LOCK((vxn_softc_t *)ifp->if_softc);
   vxn_startl(ifp);
   VXN_UNLOCK((vxn_softc_t *)ifp->if_softc);
}

/*
 *-----------------------------------------------------------------------------
 * vxn_startl --
 *     Called to transmit a packet (lock acquired)
 *
 * Results:
 *      None
 *
 * Side effects:
 *	Bounces a copy to possible BPF listener. Sets RING_LOW flag
 *	if ring is getting crowded. Starts device TX. Aggressively cleans
 *	up tx ring after starting TX.
 *
 *-----------------------------------------------------------------------------
 */
static void
vxn_startl(struct ifnet *ifp)
{
   vxn_softc_t *sc = ifp->if_softc;
   Vmxnet2_DriverData *dd = sc->vxn_dd;

   VXN_LOCK_ASSERT(sc);

   if (VXN_GET_IF_DRV_FLAGS(ifp) & VXN_IFF_OACTIVE) {
      return;
   }

   /*
    * No room on ring
    */
   if (sc->vxn_tx_buffptr[dd->txDriverNext]) {
      dd->txStopped = TRUE;
   }

   /*
    * Dequeue packets from send queue and drop them into tx ring
    */
   while (sc->vxn_tx_buffptr[dd->txDriverNext] == NULL) {
      struct mbuf *m_head = NULL;
      Vmxnet2_TxRingEntry *xre;

      IF_DEQUEUE(&ifp->if_snd, m_head);
      if (m_head == NULL) {
         break;
      }

      xre = &sc->vxn_tx_ring[dd->txDriverNext];
      if (vxn_encap(ifp, xre, m_head, &(sc->vxn_tx_buffptr[dd->txDriverNext]))) {
         IF_PREPEND(&ifp->if_snd, m_head);
         break;
      }

      /*
       * Bounce copy to (possible) BPF listener
       */
      VXN_BPF_MTAP(ifp, sc->vxn_tx_buffptr[dd->txDriverNext]);

      if (sc->vxn_tx_pending > (dd->txRingLength - 5)) {
         xre->flags |= VMXNET2_TX_RING_LOW;
      }

      VMXNET_INC(dd->txDriverNext, dd->txRingLength);
      dd->txNumDeferred++;
      sc->vxn_tx_pending++;
      ifp->if_opackets++;
   }

   /*
    * Transmit, if number of pending packets > tx cluster length
    */
   if (dd->txNumDeferred >= dd->txClusterLength) {
      dd->txNumDeferred = 0;

      /*
       * reading this port causes the implementation to transmit everything
       * in the ring
       */
      bus_space_read_4(sc->vxn_iobtag, sc->vxn_iobhandle, VMXNET_TX_ADDR);
   }

   /*
    * Clean up tx ring after calling into vmkernel, as TX completion intrs
    * are not guaranteed.
    */
   vxn_tx_complete(sc);
}

/*
 *-----------------------------------------------------------------------------
 * vxn_ioctl --
 *     IOCTL
 *
 * Results:
 *      Returns 0 for success, negative errno value otherwise.
 *
 * Side effects:
 *	None
 *-----------------------------------------------------------------------------
 */
static int
vxn_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
   int error = 0;
   int s;
   vxn_softc_t *sc = ifp->if_softc;

   s = splimp();

   switch(command) {
   case SIOCSIFADDR:
   case SIOCGIFADDR:
   case SIOCSIFMTU:
      error = ether_ioctl(ifp, command, data);
      break;

   case SIOCSIFFLAGS:
      VXN_LOCK(sc);
      if (ifp->if_flags & IFF_UP) {
         vxn_initl(sc);
      } else {
         vxn_stopl(sc);
      }
      VXN_UNLOCK(sc);
      break;

   case SIOCADDMULTI:
   case SIOCDELMULTI:
      VXN_LOCK(sc);
      vxn_load_multicast(sc);
      VXN_UNLOCK(sc);
      error = 0;
      break;

   case SIOCSIFMEDIA:
   case SIOCGIFMEDIA:
      ifmedia_ioctl(ifp, (struct ifreq *)data, &sc->media, command);

   default:
      error = EINVAL;
      break;
   }

   splx(s);

   return error;
}

#if __FreeBSD_version < 900000
/*
 *-----------------------------------------------------------------------------
 * vxn_watchdog --
 *	Watchdog function
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Not implemented.
 *-----------------------------------------------------------------------------
 */
static void
vxn_watchdog(struct ifnet *ifp)
{
   printf("vxn%d: watchdog\n", VXN_IF_UNIT(ifp));
}
#endif

/*
 *-----------------------------------------------------------------------------
 * vxn_intr --
 *	Interrupt handler
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *-----------------------------------------------------------------------------
 */
static void
vxn_intr (void *v)
{
   vxn_softc_t *sc = (vxn_softc_t *)v;
   struct ifnet *ifp = VXN_SC2IFP(sc);

   VXN_LOCK(sc);

   /*
    * Without rings being allocated we have nothing to do.  We should not
    * need even this INTR_ACK, as our hardware should be disabled when
    * rings are not allocated, but on other side INTR_ACK should be noop
    * then, and this makes sure that some bug will not force IRQ line
    * active forever.
    */
   bus_space_write_4(sc->vxn_iobtag, sc->vxn_iobhandle,
                     VMXNET_COMMAND_ADDR, VMXNET_CMD_INTR_ACK);

   if (sc->vxn_rings_allocated) {
      vxn_rx(sc);
      vxn_tx_complete(sc);
      /*
       * After having freed some of the transmit ring, go ahead and refill
       * it, if possible, while we're here.  (Idea stolen from if_sis.c.)
       */
      if (!VXN_IFQ_IS_EMPTY(&ifp->if_snd)) {
         vxn_startl(ifp);
      }
   }

   VXN_UNLOCK(sc);
}

/*
 *-----------------------------------------------------------------------------
 * vxn_rx --
 *	RX handler
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Scan RX ring and pass legit packets up to FreeBSD. Allocate a
 *      new mbuf for each packet pulled out, stick it into the ring and
 *      pass ownership back to NIC.
 *-----------------------------------------------------------------------------
 */
static void
vxn_rx(vxn_softc_t *sc)
{
   short pkt_len;
   struct ifnet *ifp = VXN_SC2IFP(sc);
   Vmxnet2_DriverData *dd = sc->vxn_dd;

   /*
    * receive packets from all the descriptors that the device implementation
    * has given back to us
    */
   while (1) {
      Vmxnet2_RxRingEntry *rre;
      VXN_LOCK_ASSERT(sc);

      rre = &sc->vxn_rx_ring[dd->rxDriverNext];
      if (rre->ownership != VMXNET2_OWNERSHIP_DRIVER) {
         break;
      }

      pkt_len = rre->actualLength;

      if (pkt_len < (60 - 4)) {
         /*
          * Ethernet header vlan tags are 4 bytes.  Some vendors generate
          *  60byte frames including vlan tags.  When vlan tag
          *  is stripped, such frames become 60 - 4. (PR106153)
          */
         if (pkt_len != 0) {
            printf("vxn%d: runt packet\n", VXN_IF_UNIT(ifp));
         }
      } else {
         struct mbuf *m_new = NULL;

         /*
	  * Allocate a new mbuf cluster to replace the current one
          */
         MGETHDR(m_new, M_DONTWAIT, MT_DATA);
         if (m_new != NULL) {
            MCLGET(m_new, M_DONTWAIT);
            if (m_new->m_flags & M_EXT) {
               m_adj(m_new, ETHER_ALIGN);
            } else {
               m_freem(m_new);
               m_new = NULL;
            }
         }

         /*
          * replace the current mbuf in the descriptor with the new one
          * and pass the packet up to the kernel
          */
         if (m_new != NULL) {
            struct mbuf *m = sc->vxn_rx_buffptr[dd->rxDriverNext];

            sc->vxn_rx_buffptr[dd->rxDriverNext] = m_new;
            rre->paddr = (uint32)vtophys(mtod(m_new, caddr_t));

            ifp->if_ipackets++;
            m->m_pkthdr.rcvif = ifp;
            m->m_pkthdr.len = m->m_len = pkt_len;

            if (rre->flags & VMXNET2_RX_HW_XSUM_OK) {
               m->m_pkthdr.csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
               m->m_pkthdr.csum_data = 0xffff;
            }

            /*
             * "Drop the driver lock around calls to if_input to avoid a LOR
             * when the packets are immediately returned for sending (e.g.  when
             * bridging or packet forwarding).  There are more efficient ways to
             * do this but for now use the least intrusive approach."
             *   - Sam Leffler (sam@FreeBSD.org), if_sis.c rev 1.90
             *
             * This function is only called by the interrupt handler, and said
             * handler isn't reentrant.  (Interrupts are masked.)  I.e., the
             * receive rings are still protected while we give up the mutex.
             */
            VXN_UNLOCK(sc);
            VXN_ETHER_INPUT(ifp, m);
            VXN_LOCK(sc);
         }
      }

      /*
       * Give the descriptor back to the device implementation
       */
      rre->ownership = VMXNET2_OWNERSHIP_NIC;
      VMXNET_INC(dd->rxDriverNext, dd->rxRingLength);
   }
}

/*
 *-----------------------------------------------------------------------------
 * vxn_tx_complete --
 *	Loop through the tx ring looking for completed transmits
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *-----------------------------------------------------------------------------
 */
static void
vxn_tx_complete(vxn_softc_t *sc)
{
   Vmxnet2_DriverData *dd = sc->vxn_dd;

   while (1) {
      Vmxnet2_TxRingEntry *xre = &sc->vxn_tx_ring[dd->txDriverCur];

      if (xre->ownership != VMXNET2_OWNERSHIP_DRIVER ||
	  sc->vxn_tx_buffptr[dd->txDriverCur] == NULL) {
         break;
      }

      m_freem(sc->vxn_tx_buffptr[dd->txDriverCur]);
      sc->vxn_tx_buffptr[dd->txDriverCur] = NULL;
      sc->vxn_tx_pending--;
      VMXNET_INC(dd->txDriverCur, dd->txRingLength);
      dd->txStopped = FALSE;
   }
}

/*
 *-----------------------------------------------------------------------------
 * vxn_init_rings --
 *	Loop through the tx ring looking for completed transmits
 *
 * Results:
 *	Returns 0 for success, negative errno value otherwise.
 *
 * Side effects:
 *	None
 *-----------------------------------------------------------------------------
 */
static int
vxn_init_rings(vxn_softc_t *sc)
{
   Vmxnet2_DriverData *dd = sc->vxn_dd;
   /*struct ifnet *ifp = &sc->arpcom.ac_if;*/
   int i;
   int32 offset;

   offset = sizeof(*dd);

   dd->rxRingLength = sc->vxn_num_rx_bufs;
   dd->rxRingOffset = offset;
   sc->vxn_rx_ring = (Vmxnet2_RxRingEntry *)((uintptr_t)dd + offset);
   offset += sc->vxn_num_rx_bufs * sizeof(Vmxnet2_RxRingEntry);

   /* dummy rxRing2, only used by windows */
   dd->rxRingLength2 = 1;
   dd->rxRingOffset2 = offset;
   offset += sizeof(Vmxnet2_RxRingEntry);

   dd->txRingLength = sc->vxn_num_tx_bufs;
   dd->txRingOffset = offset;
   sc->vxn_tx_ring = (Vmxnet2_TxRingEntry *)((uintptr_t)dd + offset);
   offset += sc->vxn_num_tx_bufs * sizeof(Vmxnet2_TxRingEntry);

   /*
    * Allocate receive buffers
    */
   for (i = 0; i < sc->vxn_num_rx_bufs; i++) {
      struct mbuf *m_new = NULL;

      /*
       * Allocate an mbuf and initialize it to contain a packet header and
       * internal data.
       */
      MGETHDR(m_new, M_DONTWAIT, MT_DATA);
      if (m_new != NULL) {
         /* Allocate and attach an mbuf cluster to mbuf. */
         MCLGET(m_new, M_DONTWAIT);
         if (m_new->m_flags & M_EXT) {
            m_adj(m_new, ETHER_ALIGN);
            sc->vxn_rx_ring[i].paddr = (uint32)vtophys(mtod(m_new, caddr_t));
            sc->vxn_rx_ring[i].bufferLength = MCLBYTES;
            sc->vxn_rx_ring[i].actualLength = 0;
            sc->vxn_rx_buffptr[i] = m_new;
            sc->vxn_rx_ring[i].ownership = VMXNET2_OWNERSHIP_NIC;
         } else {
            /*
             * Allocation and attachment of mbuf clusters failed.
             */
            m_freem(m_new);
            m_new = NULL;
            goto err_release_ring;
         }
      } else {
         /* Allocation of mbuf failed. */
         goto err_release_ring;
      }
   }

   /* dummy rxRing2 tacked on to the end, with a single unusable entry */
   sc->vxn_rx_ring[i].paddr = 0;
   sc->vxn_rx_ring[i].bufferLength = 0;
   sc->vxn_rx_ring[i].actualLength = 0;
   sc->vxn_rx_buffptr[i] = 0;
   sc->vxn_rx_ring[i].ownership = VMXNET2_OWNERSHIP_DRIVER;

   dd->rxDriverNext = 0;

   /*
    * Give tx ring ownership to DRIVER
    */
   for (i = 0; i < sc->vxn_num_tx_bufs; i++) {
      sc->vxn_tx_ring[i].ownership = VMXNET2_OWNERSHIP_DRIVER;
      sc->vxn_tx_buffptr[i] = NULL;
      sc->vxn_tx_ring[i].sg.sg[0].addrHi = 0;
   }

   dd->txDriverCur = dd->txDriverNext = 0;
   dd->txStopped = FALSE;

   sc->vxn_rings_allocated = 1;
   return 0;
err_release_ring:
   /*
    * Clearup already allocated mbufs and attached clusters.
    */
  for (--i; i >= 0; i--) {
     m_freem(sc->vxn_rx_buffptr[i]);
     sc->vxn_rx_buffptr[i] = NULL;
     sc->vxn_rx_ring[i].paddr = 0;
     sc->vxn_rx_ring[i].bufferLength = 0;
     sc->vxn_rx_ring[i].ownership = 0;
  }
  return ENOMEM;

}

/*
 *-----------------------------------------------------------------------------
 * vxn_release_rings --
 *	Free tx and rx ring driverdata
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *-----------------------------------------------------------------------------
 */
static void
vxn_release_rings(vxn_softc_t *sc)
{
   int i;

   sc->vxn_rings_allocated = 0;

   /*
    * Free rx ring packets
    */
   for (i = 0; i < sc->vxn_num_rx_bufs; i++) {
      if (sc->vxn_rx_buffptr[i] != NULL) {
         m_freem(sc->vxn_rx_buffptr[i]);
         sc->vxn_rx_buffptr[i] = NULL;
      }
   }

   /*
    * Free tx ring packets
    */
   for (i = 0; i < sc->vxn_num_tx_bufs; i++) {
      if (sc->vxn_tx_buffptr[i] != NULL) {
         m_freem(sc->vxn_tx_buffptr[i]);
         sc->vxn_tx_buffptr[i] = NULL;
      }
   }
}

