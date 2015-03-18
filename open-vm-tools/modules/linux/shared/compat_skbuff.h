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

#ifndef __COMPAT_SKBUFF_H__
#   define __COMPAT_SKBUFF_H__

#include <linux/skbuff.h>

/*
 * When transition from mac/nh/h to skb_* accessors was made, also SKB_WITH_OVERHEAD
 * was introduced.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22) || \
   (LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 21) && defined(SKB_WITH_OVERHEAD))
#define compat_skb_mac_header(skb)         skb_mac_header(skb)
#define compat_skb_network_header(skb)     skb_network_header(skb)
#define compat_skb_network_offset(skb)     skb_network_offset(skb)
#define compat_skb_transport_header(skb)   skb_transport_header(skb)
#define compat_skb_transport_offset(skb)   skb_transport_offset(skb)
#define compat_skb_network_header_len(skb) skb_network_header_len(skb)
#define compat_skb_tail_pointer(skb)       skb_tail_pointer(skb)
#define compat_skb_end_pointer(skb)        skb_end_pointer(skb)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#   define compat_skb_ip_header(skb)       ip_hdr(skb)
#   define compat_skb_ipv6_header(skb)     ipv6_hdr(skb)
#   define compat_skb_tcp_header(skb)      tcp_hdr(skb)
#else
#   define compat_skb_ip_header(skb)   ((struct iphdr *)skb_network_header(skb))
#   define compat_skb_ipv6_header(skb) ((struct ipv6hdr *)skb_network_header(skb))
#   define compat_skb_tcp_header(skb)  ((struct tcphdr *)skb_transport_header(skb))
#endif
#define compat_skb_reset_mac_header(skb)          skb_reset_mac_header(skb)
#define compat_skb_reset_network_header(skb)      skb_reset_network_header(skb)
#define compat_skb_reset_transport_header(skb)    skb_reset_transport_header(skb)
#define compat_skb_set_network_header(skb, off)   skb_set_network_header(skb, off)
#define compat_skb_set_transport_header(skb, off) skb_set_transport_header(skb, off)
#else
#define compat_skb_mac_header(skb)         (skb)->mac.raw
#define compat_skb_network_header(skb)     (skb)->nh.raw
#define compat_skb_network_offset(skb)     ((skb)->nh.raw - (skb)->data)
#define compat_skb_transport_header(skb)   (skb)->h.raw
#define compat_skb_transport_offset(skb)   ((skb)->h.raw - (skb)->data)
#define compat_skb_network_header_len(skb) ((skb)->h.raw - (skb)->nh.raw)
#define compat_skb_tail_pointer(skb)       (skb)->tail
#define compat_skb_end_pointer(skb)        (skb)->end
#define compat_skb_ip_header(skb)          (skb)->nh.iph
#define compat_skb_ipv6_header(skb)        (skb)->nh.ipv6h
#define compat_skb_tcp_header(skb)         (skb)->h.th
#define compat_skb_reset_mac_header(skb)   ((skb)->mac.raw = (skb)->data)
#define compat_skb_reset_network_header(skb)      ((skb)->nh.raw = (skb)->data)
#define compat_skb_reset_transport_header(skb)    ((skb)->h.raw = (skb)->data)
#define compat_skb_set_network_header(skb, off)   ((skb)->nh.raw = (skb)->data + (off))
#define compat_skb_set_transport_header(skb, off) ((skb)->h.raw = (skb)->data + (off))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18) || defined(VMW_SKB_LINEARIZE_2618)
#   define compat_skb_linearize(skb) skb_linearize((skb))
#else

#   if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 0)
#      define compat_skb_linearize(skb) __skb_linearize((skb), GFP_ATOMIC)
#   elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 4)
#      define compat_skb_linearize(skb) skb_linearize((skb), GFP_ATOMIC)
#   else
static inline int
compat_skb_linearize(struct sk_buff *skb)
{
   return 0;
}
#   endif

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#define compat_skb_csum_offset(skb)        (skb)->csum_offset
#else
#define compat_skb_csum_offset(skb)        (skb)->csum
#endif

/*
 * Note that compat_skb_csum_start() has semantic different from kernel's csum_start:
 * kernel's skb->csum_start is offset between start of checksummed area and start of
 * complete skb buffer, while our compat_skb_csum_start(skb) is offset from start
 * of packet itself.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define compat_skb_csum_start(skb)         ((skb)->csum_start - skb_headroom(skb))
#else
#define compat_skb_csum_start(skb)         compat_skb_transport_offset(skb)
#endif

#if defined(NETIF_F_GSO) /* 2.6.18 and upwards */
#define compat_skb_mss(skb) (skb_shinfo(skb)->gso_size)
#else
#define compat_skb_mss(skb) (skb_shinfo(skb)->tso_size)
#endif

/* used by both received pkts and outgoing ones */
#define VM_CHECKSUM_UNNECESSARY CHECKSUM_UNNECESSARY

/* csum status of received pkts */
#if defined(CHECKSUM_COMPLETE)
#   define VM_RX_CHECKSUM_PARTIAL     CHECKSUM_COMPLETE
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19) && defined(CHECKSUM_HW)
#   define VM_RX_CHECKSUM_PARTIAL     CHECKSUM_HW
#else
#   define VM_RX_CHECKSUM_PARTIAL     CHECKSUM_PARTIAL
#endif

/* csum status of outgoing pkts */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19) && defined(CHECKSUM_HW)
#   define VM_TX_CHECKSUM_PARTIAL      CHECKSUM_HW
#else
#   define VM_TX_CHECKSUM_PARTIAL      CHECKSUM_PARTIAL
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0))
#   define compat_kfree_skb(skb, type) kfree_skb(skb, type)
#   define compat_dev_kfree_skb(skb, type) dev_kfree_skb(skb, type)
#   define compat_dev_kfree_skb_any(skb, type) dev_kfree_skb(skb, type)
#   define compat_dev_kfree_skb_irq(skb, type) dev_kfree_skb(skb, type)
#else
#   define compat_kfree_skb(skb, type) kfree_skb(skb)
#   define compat_dev_kfree_skb(skb, type) dev_kfree_skb(skb)
#   if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43))
#      define compat_dev_kfree_skb_any(skb, type) dev_kfree_skb(skb)
#      define compat_dev_kfree_skb_irq(skb, type) dev_kfree_skb(skb)
#   else
#      define compat_dev_kfree_skb_any(skb, type) dev_kfree_skb_any(skb)
#      define compat_dev_kfree_skb_irq(skb, type) dev_kfree_skb_irq(skb)
#   endif
#endif

#ifndef NET_IP_ALIGN
#   define COMPAT_NET_IP_ALIGN  2
#else
#   define COMPAT_NET_IP_ALIGN  NET_IP_ALIGN 
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 4)
#   define compat_skb_headlen(skb)         skb_headlen(skb)
#   define compat_pskb_may_pull(skb, len)  pskb_may_pull(skb, len)
#   define compat_skb_is_nonlinear(skb)    skb_is_nonlinear(skb)
#else
#   define compat_skb_headlen(skb)         (skb)->len
#   define compat_pskb_may_pull(skb, len)  1
#   define compat_skb_is_nonlinear(skb)    0
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12)
#   define compat_skb_header_cloned(skb)   skb_header_cloned(skb)
#else
#   define compat_skb_header_cloned(skb)   0
#endif
#endif /* __COMPAT_SKBUFF_H__ */
