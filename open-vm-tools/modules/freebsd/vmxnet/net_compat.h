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
 * net_compat.h:
 *   This file contains wrapper macros that abstract out
 *   the differences between FreeBSD 4.x, 5.x and 6.x in
 *   network related kernel calls used in if_vxn.c.
 */

#ifndef _VXN_NET_COMPAT_H_
#define _VXN_NET_COMPAT_H_

#if __FreeBSD_version < 500009
   #define VXN_ETHER_IFATTACH(ifp, llc) (ether_ifattach(ifp, ETHER_BPF_SUPPORTED))
   #define VXN_ETHER_IFDETACH(ifp) (ether_ifdetach(ifp, ETHER_BPF_SUPPORTED))
#else /* >= 500009 */
   #define VXN_ETHER_IFATTACH(ifp, llc) (ether_ifattach(ifp, llc))
   #define VXN_ETHER_IFDETACH(ifp) (ether_ifdetach(ifp))
#endif /* 500009 */

#if __FreeBSD_version < 500016
   #define VXN_IFMULTI_FIRST LIST_FIRST
   #define VXN_IFMULTI_NEXT  LIST_NEXT
#else /* >= 500016 */
   #define VXN_IFMULTI_FIRST TAILQ_FIRST
   #define VXN_IFMULTI_NEXT  TAILQ_NEXT
#endif /* 500016 */

#if __FreeBSD_version < 500043
   /*
    * This code used to be in our if_vxn.c; it is now included in
    * FreeBSD's ether_input().
    */
   #define VXN_ETHER_INPUT(ifp, m) do {                                \
           struct ether_header *eh = mtod((m), struct ether_header *); \
           m_adj(m, sizeof(struct ether_header));                      \
           ether_input((ifp), eh, (m));                                \
   } while (0)

   #define VXN_BPF_MTAP(ifp, m) do {                    \
	   if ((ifp)->if_bpf)                           \
              bpf_mtap((ifp), (m));                     \
   } while (0)
#else /* >= 500043 */
   #define VXN_ETHER_INPUT(ifp, m) ((*(ifp)->if_input)(ifp, m))
   #define VXN_BPF_MTAP(ifp, m) BPF_MTAP(ifp, m)
#endif /* 500043 */

#if __FreeBSD_version < 501113
   #define VXN_IF_UNIT(ifp) ((ifp)->if_unit)

   #define VXN_IF_INITNAME(ifp, name, unit) do {           \
           (ifp)->if_name = (char *)(name);                \
           (ifp)->if_unit = (unit);                        \
   } while (0)
#else /* >= 501113 */
   #define VXN_IF_UNIT(ifp) ((ifp)->if_dunit)
   #define VXN_IF_INITNAME(ifp, name, unit) (if_initname(ifp, name, unit))
#endif /* 501113 */

/*
 * In FreeBSD 6.x, ifnet struct handling has changed.
 *
 * On older releases, each NIC driver's softc structure
 * started with an ifnet struct, nested within an arpcom.
 *
 *   struct vxn_softc {
 *      struct arpcom {
 *         struct ifnet {
 *            void *if_softc;
 *            ...
 *         } ac_if;
 *         u_char       ac_enaddr[6];
 *         ...
 *      } <arpcom>;
 *      ...
 *   };
 *
 * In FreeBSD 6.x, the ifnet struct is now a pointer
 * returned from if_alloc() and if_alloc() allocates
 * space for if_l2com.
 *
 * Accessing the link layer address via arpcom's ac_enaddr was deprecated as
 * of sys/net/if_var.h:1.98, and drivers are to instead get/set said address
 * through the embedded ifnet structure.  So with FreeBSD 6 and above, we
 * instead define vxn_softc as follows and rely on FreeBSD's macro glue for
 * easy access to the LL-address.
 *
 *   struct vxn_softc {
 *      struct ifnet    *vxn_ifp;
 *      ...
 *   };
 */
#if __FreeBSD_version < 600000  /* Pre-FreeBSD 6.0-RELEASE */
   #define VXN_NEEDARPCOM
   #define VXN_IF_ALLOC(softc)  (&(softc)->arpcom.ac_if)
   #define VXN_IF_FREE(softc)

   #define VXN_SC2IFP(softc)    (&(softc)->arpcom.ac_if)

   #define VXN_PCIR_MAPS        PCIR_MAPS

   #define VXN_IFF_RUNNING      IFF_RUNNING
   #define VXN_IFF_OACTIVE      IFF_OACTIVE

   #define VXN_SET_IF_DRV_FLAGS(ifp, flags) \
                                ((ifp)->if_flags |= (flags))
   #define VXN_CLR_IF_DRV_FLAGS(ifp, flags) \
                                ((ifp)->if_flags &= ~(flags))
   #define VXN_GET_IF_DRV_FLAGS(ifp) \
                                ((ifp)->if_flags)
#else                           /* FreeBSD 6.x+ */
   #define VXN_IF_ALLOC(softc)  ((softc)->vxn_ifp = if_alloc(IFT_ETHER))
   #define VXN_IF_FREE(softc)   if_free((softc)->vxn_ifp)

   #define VXN_SC2IFP(softc)    ((softc)->vxn_ifp)

   #define VXN_PCIR_MAPS        PCIR_BARS

   #define VXN_IFF_RUNNING      IFF_DRV_RUNNING
   #define VXN_IFF_OACTIVE      IFF_DRV_OACTIVE

   #define VXN_SET_IF_DRV_FLAGS(ifp, flags) \
                                ((ifp)->if_drv_flags |= (flags))
   #define VXN_CLR_IF_DRV_FLAGS(ifp, flags) \
                                ((ifp)->if_drv_flags &= ~(flags))
   #define VXN_GET_IF_DRV_FLAGS(ifp) \
                                ((ifp)->if_drv_flags)
#endif

#ifdef VXN_MPSAFE
#   define VXN_MTX_INIT(mtx, name, type, opts) \
                                        mtx_init(mtx, name, type, opts)
#   define VXN_MTX_DESTROY(mtx)         mtx_destroy(mtx)
#   define VXN_LOCK(_sc)                mtx_lock(&(_sc)->vxn_mtx)
#   define VXN_UNLOCK(_sc)              mtx_unlock(&(_sc)->vxn_mtx)
#   define VXN_LOCK_ASSERT(_sc)         mtx_assert(&(_sc)->vxn_mtx, MA_OWNED)
#   define VXN_IFQ_IS_EMPTY(_ifq)       IFQ_DRV_IS_EMPTY((_ifq))
#else
#   define VXN_MTX_INIT(mtx, name, type, opts)
#   define VXN_MTX_DESTROY(mtx)
#   define VXN_LOCK(_sc)
#   define VXN_UNLOCK(_sc)
#   define VXN_LOCK_ASSERT(_sc)
#   define VXN_IFQ_IS_EMPTY(_ifq) ((_ifq)->ifq_head == NULL)
#endif

/*
 * sys/net/if_var.h ver 1.100 introduced struct ifnet::if_addr_mtx to protect
 * address lists.  We're particularly concerned with the list of multicast
 * addresses, if_multiaddrs, as our driver traverses this list during mcast
 * filter setup.  (MFC'd for RELENG_5_5.)
 */
#if __FreeBSD_version < 505000
#   define VXN_IF_ADDR_LOCK(_ifp)
#   define VXN_IF_ADDR_UNLOCK(_ifp)
#elif __FreeBSD_version < 1000000
#   define VXN_IF_ADDR_LOCK(_ifp)       IF_ADDR_LOCK((_ifp))
#   define VXN_IF_ADDR_UNLOCK(_ifp)     IF_ADDR_UNLOCK((_ifp))
#else
#   define VXN_IF_ADDR_LOCK(_ifp)       if_maddr_rlock((_ifp))
#   define VXN_IF_ADDR_UNLOCK(_ifp)     if_maddr_runlock((_ifp))
#endif

#endif /* _VXN_NET_COMPAT_H_ */
