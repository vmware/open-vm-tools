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

#ifndef _VMXNET3_INT_H
#define _VMXNET3_INT_H

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_ioport.h"
#include "compat_pci.h"
#include "compat_highmem.h"
#include "compat_init.h"
#include "compat_timer.h"
#include "compat_netdevice.h"
#include "compat_skbuff.h"
#include "compat_interrupt.h"
#include "compat_workqueue.h"
#include "compat_module.h"

#include <asm/dma.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <asm/checksum.h>
#include <linux/moduleparam.h>


#include <linux/if_vlan.h>
#include <linux/if_arp.h>
#include <linux/inetdevice.h>

#ifdef CONFIG_COMPAT
#ifndef HAVE_UNLOCKED_IOCTL
#include <linux/ioctl32.h>
#endif
#endif

#include "vm_basic_types.h"
#include "vmxnet3_defs.h"
#include "vmxnet3_version.h"
#include <linux/stddef.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) && !defined(VMXNET3_NO_NAPI)
#   define VMXNET3_NAPI
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) || defined NETIF_F_GRO
/*
 * The new NAPI initially appeared only in 2.6.24 but Redhat backported it
 * to 2.6.18 in RHEL5.4. We assume that iwe need to use the new style
 * whenever NETIF_F_GRO is present.
 */
#   define VMXNET3_NEW_NAPI
#endif
#endif


#ifdef VLAN_GROUP_ARRAY_SPLIT_PARTS
#define compat_vlan_group_get_device(vlan_grp, vid)	vlan_group_get_device(vlan_grp, vid)
#define compat_vlan_group_set_device(vlan_grp, vid, dev)	vlan_group_set_device(vlan_grp, vid, dev)
#else
#define compat_vlan_group_get_device(vlan_grp, vid)	((vlan_grp)->vlan_devices[(vid)])
#define compat_vlan_group_set_device(vlan_grp, vid, dev)	((vlan_grp)->vlan_devices[(vid)] = (dev))
#endif


#ifdef VMXNET3_NAPI
#   ifdef VMX86_DEBUG
#      define VMXNET3_DRIVER_VERSION_REPORT VMXNET3_DRIVER_VERSION_STRING"-NAPI(debug)"
#   else
#      define VMXNET3_DRIVER_VERSION_REPORT VMXNET3_DRIVER_VERSION_STRING"-NAPI"
#   endif
#else
#   ifdef VMX86_DEBUG
#      define VMXNET3_DRIVER_VERSION_REPORT VMXNET3_DRIVER_VERSION_STRING"(debug)"
#   else
#      define VMXNET3_DRIVER_VERSION_REPORT VMXNET3_DRIVER_VERSION_STRING
#   endif
#endif


struct vmxnet3_cmd_ring {
	Vmxnet3_GenericDesc *base;
	u32		size;
	u32		next2fill;
	u32		next2comp;
	u8		gen;
	dma_addr_t           basePA;
};

static inline void
vmxnet3_cmd_ring_adv_next2fill(struct vmxnet3_cmd_ring *ring)
{
	ring->next2fill++;
	if (unlikely(ring->next2fill == ring->size)) {
		ring->next2fill = 0;
		VMXNET3_FLIP_RING_GEN(ring->gen);
	}
}

static inline void
vmxnet3_cmd_ring_adv_next2comp(struct vmxnet3_cmd_ring *ring)
{
	VMXNET3_INC_RING_IDX_ONLY(ring->next2comp, ring->size);
}

static inline int
vmxnet3_cmd_ring_desc_avail(struct vmxnet3_cmd_ring *ring)
{
	return (ring->next2comp > ring->next2fill ? 0 : ring->size) +
		ring->next2comp - ring->next2fill - 1;
}

struct vmxnet3_comp_ring {
	Vmxnet3_GenericDesc *base;
	u32               size;
	u32               next2proc;
	u8                gen;
	u8                intr_idx;
	dma_addr_t        basePA;
};

static inline void
vmxnet3_comp_ring_adv_next2proc(struct vmxnet3_comp_ring *ring)
{
	ring->next2proc++;
	if (unlikely(ring->next2proc == ring->size)) {
		ring->next2proc = 0;
		VMXNET3_FLIP_RING_GEN(ring->gen);
	}
}

struct vmxnet3_tx_data_ring {
	Vmxnet3_TxDataDesc *base;
	u32                 size;
	dma_addr_t          basePA;
};

enum vmxnet3_buf_map_type {
	VMXNET3_MAP_INVALID = 0,
	VMXNET3_MAP_NONE,
	VMXNET3_MAP_SINGLE,
	VMXNET3_MAP_PAGE,
};

struct vmxnet3_tx_buf_info {
	u32      map_type;
	u16      len;
	u16      sop_idx;
	dma_addr_t  dma_addr;
	struct sk_buff *skb;
};

struct vmxnet3_tq_driver_stats {
	u64 drop_total;     /* # of pkts dropped by the driver, the
                           * counters below track droppings due to 
                           * different reasons
                           */
	u64 drop_too_many_frags;
	u64 drop_oversized_hdr;
	u64 drop_hdr_inspect_err;
	u64 drop_tso;

	u64 tx_ring_full;
	u64 linearized;         /* # of pkts linearized */
	u64 copy_skb_header;    /* # of times we have to copy skb header */
	u64 oversized_hdr;
};

struct vmxnet3_tx_ctx {
	Bool   ipv4;
	u16 mss;
	u32 eth_ip_hdr_size; /* only valid for pkts requesting tso or csum
				 * offloading
				 */
	u32 l4_hdr_size;     /* only valid if mss != 0 */
	u32 copy_size;       /* # of bytes copied into the data ring */
	Vmxnet3_GenericDesc *sop_txd;
	Vmxnet3_GenericDesc *eop_txd;
};

struct vmxnet3_tx_queue {
	spinlock_t                      tx_lock;
	struct vmxnet3_cmd_ring         tx_ring;
	struct vmxnet3_tx_buf_info     *buf_info;
	struct vmxnet3_tx_data_ring     data_ring;
	struct vmxnet3_comp_ring        comp_ring;
	Vmxnet3_TxQueueCtrl            *shared;
	struct vmxnet3_tq_driver_stats  stats;
	Bool                            stopped;
	int                             num_stop;  /* # of times the queue is
						    * stopped */
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

enum vmxnet3_rx_buf_type {
	VMXNET3_RX_BUF_NONE = 0,
	VMXNET3_RX_BUF_SKB = 1,
	VMXNET3_RX_BUF_PAGE = 2
};

struct vmxnet3_rx_buf_info {
	enum vmxnet3_rx_buf_type buf_type;
	u16     len;
	union {
		struct sk_buff *skb;
		struct page    *page;
		unsigned long   shm_idx;
	};
	dma_addr_t dma_addr;
};

struct vmxnet3_rx_ctx {
	struct sk_buff *skb;
	u32 sop_idx;
};

struct vmxnet3_rq_driver_stats {
	u64 drop_total;
	u64 drop_err;
	u64 drop_fcs;
	u64 rx_buf_alloc_failure;
};

struct vmxnet3_rx_queue {
	struct vmxnet3_cmd_ring   rx_ring[2];
	struct vmxnet3_comp_ring  comp_ring;
	struct vmxnet3_rx_ctx     rx_ctx;
	u32 qid;            /* rqID in RCD for buffer from 1st ring */
	u32 qid2;           /* rqID in RCD for buffer from 2nd ring */
	u32 uncommitted[2]; /* # of buffers allocated since last RXPROD
			     * update */
	struct vmxnet3_rx_buf_info     *buf_info[2];
	Vmxnet3_RxQueueCtrl            *shared;
	struct vmxnet3_rq_driver_stats  stats;
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

#define VMXNET3_LINUX_MAX_MSIX_VECT     1

struct vmxnet3_intr {
	enum vmxnet3_intr_mask_mode  mask_mode;
	enum vmxnet3_intr_type       type;          /* MSI-X, MSI, or INTx? */
	u8  num_intrs;			/* # of intr vectors */
	u8  event_intr_idx;		/* idx of the intr vector for event */
	u8  mod_levels[VMXNET3_LINUX_MAX_MSIX_VECT]; /* moderation level */
#ifdef CONFIG_PCI_MSI
	struct msix_entry msix_entries[VMXNET3_LINUX_MAX_MSIX_VECT];
#endif
};

#define VMXNET3_STATE_BIT_RESETTING   0
#define VMXNET3_STATE_BIT_QUIESCED    1
struct vmxnet3_adapter {
	struct vmxnet3_tx_queue        tx_queue;
	struct vmxnet3_rx_queue        rx_queue;
#ifdef VMXNET3_NAPI
	struct napi_struct             napi;
#endif
	struct vlan_group             *vlan_grp;

	struct vmxnet3_intr            intr;

	Vmxnet3_DriverShared		*shared;
	Vmxnet3_PMConf			*pm_conf;
	Vmxnet3_TxQueueDesc		*tqd_start;    /* first tx queue desc */
	Vmxnet3_RxQueueDesc		*rqd_start;    /* first rx queue desc */
	struct net_device             *netdev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)
	struct net_device_stats        net_stats;
#endif
	struct pci_dev                *pdev;

	u8				*hw_addr0; /* for BAR 0 */
	u8				*hw_addr1; /* for BAR 1 */

	/* feature control */
	Bool				rxcsum;
	Bool				lro;
	Bool				jumbo_frame;

	/* rx buffer related */
	unsigned   skb_buf_size;
	int        rx_buf_per_pkt;  /* only apply to the 1st ring */
	dma_addr_t shared_pa;
	dma_addr_t queue_desc_pa;

	/* Wake-on-LAN */
	u32     wol;

	/* Link speed */
	u32     link_speed; /* in mbps */

	u64     tx_timeout_count;
	compat_work work;

	unsigned long  state;    /* VMXNET3_STATE_BIT_xxx */

	int dev_number;
	Bool is_shm;
	struct vmxnet3_shm_pool *shm;
};

struct vmxnet3_stat_desc {
	char desc[ETH_GSTRING_LEN];
	int  offset;
};

#define VMXNET3_WRITE_BAR0_REG(adapter, reg, val)  \
	writel(cpu_to_le32(val), (adapter)->hw_addr0 + (reg))
#define VMXNET3_READ_BAR0_REG(adapter, reg)        \
	le32_to_cpu(readl((adapter)->hw_addr0 + (reg)))

#define VMXNET3_WRITE_BAR1_REG(adapter, reg, val)  \
	writel(cpu_to_le32(val), (adapter)->hw_addr1 + (reg))
#define VMXNET3_READ_BAR1_REG(adapter, reg)        \
	le32_to_cpu(readl((adapter)->hw_addr1 + (reg)))

#define VMXNET3_WAKE_QUEUE_THRESHOLD(tq)  (5)
#define VMXNET3_RX_ALLOC_THRESHOLD(rq, ring_idx, adapter) \
	((rq)->rx_ring[ring_idx].size >> 3)

#define VMXNET3_GET_ADDR_LO(dma)   ((u32)(dma))
#define VMXNET3_GET_ADDR_HI(dma)   ((u32)(((u64)(dma)) >> 32))

/* must be a multiple of VMXNET3_RING_SIZE_ALIGN */
#define VMXNET3_DEF_TX_RING_SIZE    512
#define VMXNET3_DEF_RX_RING_SIZE    256

/* FIXME: what's the right value for this? */
#define VMXNET3_MAX_ETH_HDR_SIZE    22

#define VMXNET3_MAX_SKB_BUF_SIZE    (3*1024)

static inline void
set_flag_le16(__le16 *data, u16 flag)
{
	*data = cpu_to_le16(le16_to_cpu(*data) | flag);
}

static inline void
set_flag_le64(__le64 *data, u64 flag)
{
	*data = cpu_to_le64(le64_to_cpu(*data) | flag);
}

static inline void
reset_flag_le64(__le64 *data, u64 flag)
{
	*data = cpu_to_le64(le64_to_cpu(*data) & ~flag);
}

int
vmxnet3_tq_xmit(struct sk_buff *skb, struct vmxnet3_tx_queue *tq, struct
		vmxnet3_adapter *adapter, struct net_device *netdev);
int
vmxnet3_quiesce_dev(struct vmxnet3_adapter *adapter);

int
vmxnet3_activate_dev(struct vmxnet3_adapter *adapter);

void
vmxnet3_force_close(struct vmxnet3_adapter *adapter);

void
vmxnet3_reset_dev(struct vmxnet3_adapter *adapter);

void
vmxnet3_tq_destroy(struct vmxnet3_tx_queue *tq,
		   struct vmxnet3_adapter *adapter);

void
vmxnet3_rq_destroy(struct vmxnet3_rx_queue *rq,
		   struct vmxnet3_adapter *adapter);

int
vmxnet3_create_queues(struct vmxnet3_adapter *adapter,
		      u32 tx_ring_size, u32 rx_ring_size, u32 rx_ring2_size);

void
vmxnet3_vlan_features(struct vmxnet3_adapter *adapter, u16 vid, Bool allvids);

extern void vmxnet3_set_ethtool_ops(struct net_device *netdev);
extern struct net_device_stats *vmxnet3_get_stats(struct net_device *netdev);

extern char vmxnet3_driver_name[];


#endif
