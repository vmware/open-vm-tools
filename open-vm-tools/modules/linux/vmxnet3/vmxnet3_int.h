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

#include "vmxnet3_defs.h"

typedef struct vmxnet3_cmd_ring {
   Vmxnet3_GenericDesc *base;
   uint32               size;
   uint32               next2fill;
   uint32               next2comp;
   uint8                gen;
   dma_addr_t           basePA;
} Vmxnet3_CmdRing;

static INLINE void 
vmxnet3_cmd_ring_adv_next2fill(struct vmxnet3_cmd_ring *ring)
{
   ring->next2fill++;
   if (UNLIKELY(ring->next2fill == ring->size)) {
      ring->next2fill = 0;
      VMXNET3_FLIP_RING_GEN(ring->gen);
   }
}

static INLINE void
vmxnet3_cmd_ring_adv_next2comp(struct vmxnet3_cmd_ring *ring)
{
   VMXNET3_INC_RING_IDX_ONLY(ring->next2comp, ring->size);
}

static INLINE int
vmxnet3_cmd_ring_desc_avail(struct vmxnet3_cmd_ring *ring)
{
   return (ring->next2comp > ring->next2fill ? 0 : ring->size) + 
           ring->next2comp - ring->next2fill - 1;
}

typedef struct vmxnet3_comp_ring {
   Vmxnet3_GenericDesc *base;
   uint32               size;
   uint32               next2proc;
   uint8                gen;
   uint8                intr_idx;
   dma_addr_t           basePA;
} Vmxnet3_CompRing;

static INLINE void
vmxnet3_comp_ring_adv_next2proc(struct vmxnet3_comp_ring *ring)
{
   ring->next2proc++;
   if (UNLIKELY(ring->next2proc == ring->size)) {
      ring->next2proc = 0;
      VMXNET3_FLIP_RING_GEN(ring->gen);
   }
}

struct vmxnet3_tx_data_ring {
   Vmxnet3_TxDataDesc *base;
   uint32              size;
   dma_addr_t          basePA;
};

enum vmxnet3_buf_map_type {
   VMXNET3_MAP_INVALID = 0,
   VMXNET3_MAP_NONE,
   VMXNET3_MAP_SINGLE,
   VMXNET3_MAP_PAGE,
};

struct vmxnet3_tx_buf_info {
   uint32      map_type;
   uint16      len;
   uint16      sop_idx;
   dma_addr_t  dma_addr;
   struct sk_buff *skb;
};

struct vmxnet3_tq_driver_stats {
   uint64 drop_total;     /* # of pkts dropped by the driver, the 
                           * counters below track droppings due to 
                           * different reasons
                           */
   uint64 drop_too_many_frags;
   uint64 drop_oversized_hdr;
   uint64 drop_hdr_inspect_err; 
   uint64 drop_tso;

   uint64 tx_ring_full;
   uint64 linearized;         /* # of pkts linearized */
   uint64 copy_skb_header;    /* # of times we have to copy skb header */
   uint64 oversized_hdr;    
};

struct vmxnet3_tx_ctx {
   Bool   ipv4;
   uint16 mss;
   uint32 eth_ip_hdr_size; /* only valid for pkts requesting tso or csum offloading */
   uint32 l4_hdr_size;     /* only valid if mss != 0 */
   uint32 copy_size;       /* # of bytes copied into the data ring */
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
   int                             num_stop;  /* # of time queue is stopped */
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

enum vmxnet3_rx_buf_type {
   VMXNET3_RX_BUF_NONE = 0,
   VMXNET3_RX_BUF_SKB = 1,
   VMXNET3_RX_BUF_PAGE = 2
};

struct vmxnet3_rx_buf_info {
   enum vmxnet3_rx_buf_type buf_type;
   uint16     len;
   union {
      struct sk_buff *skb;
      struct page    *page;
   };
   dma_addr_t dma_addr;
};

struct vmxnet3_rx_ctx {
   struct sk_buff *skb;
   uint32 sop_idx;
};

struct vmxnet3_rq_driver_stats {
   uint64 drop_total;
   uint64 drop_err;
   uint64 drop_fcs;
   uint64 rx_buf_alloc_failure;
};

struct vmxnet3_rx_queue {
   struct vmxnet3_cmd_ring   rx_ring[2];
   struct vmxnet3_comp_ring  comp_ring;
   struct vmxnet3_rx_ctx     rx_ctx;
   uint32 qid;            /* rqID in RCD for buffer from 1st ring */
   uint32 qid2;           /* rqID in RCD for buffer from 2nd ring */
   uint32 uncommitted[2]; /* # of buffers allocated since last RXPROD update */
   struct vmxnet3_rx_buf_info     *buf_info[2];
   Vmxnet3_RxQueueCtrl            *shared;
   struct vmxnet3_rq_driver_stats  stats;
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

#define VMXNET3_LINUX_MAX_MSIX_VECT     1

struct vmxnet3_intr {
   enum vmxnet3_intr_mask_mode  mask_mode;
   enum vmxnet3_intr_type       type;          /* MSI-X, MSI, or INTx? */
   uint8  num_intrs;                           /* # of intr vectors */
   uint8  event_intr_idx;                      /* idx of the intr vector for event */
   uint8  mod_levels[VMXNET3_LINUX_MAX_MSIX_VECT]; /* moderation level */
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

   struct Vmxnet3_DriverShared   *shared;
   struct Vmxnet3_PMConf         *pm_conf;
   struct Vmxnet3_TxQueueDesc    *tqd_start;     /* first tx queue desc */
   struct Vmxnet3_RxQueueDesc    *rqd_start;     /* first rx queue desc */;
   struct net_device             *netdev;
   struct net_device_stats        net_stats;
   struct pci_dev                *pdev;

   uint8  *hw_addr0; /* for BAR 0 */
   uint8  *hw_addr1; /* for BAR 1 */

   /* feature control */
   Bool   rxcsum;
   Bool   lro;
   Bool   jumbo_frame;

   /* rx buffer related */
   unsigned   skb_buf_size;
   int        rx_buf_per_pkt;  /* only apply to the 1st ring */
   dma_addr_t shared_pa;
   dma_addr_t queue_desc_pa;

   /* Wake-on-LAN */
   uint32     wol;

   /* Link speed */
   uint32     link_speed; /* in mbps */

   uint64     tx_timeout_count;
   compat_work work;

   unsigned long  state;    /* VMXNET3_STATE_BIT_xxx */
};

struct vmxnet3_stat_desc {
   char desc[ETH_GSTRING_LEN];
   int  offset;
};

#define VMXNET3_WRITE_BAR0_REG(adapter, reg, val)  \
   writel((val), (adapter)->hw_addr0 + (reg))
#define VMXNET3_READ_BAR0_REG(adapter, reg)        \
   readl((adapter)->hw_addr0 + (reg))

#define VMXNET3_WRITE_BAR1_REG(adapter, reg, val)  \
   writel((val), (adapter)->hw_addr1 + (reg))
#define VMXNET3_READ_BAR1_REG(adapter, reg)        \
   readl((adapter)->hw_addr1 + (reg))

#define VMXNET3_WAKE_QUEUE_THRESHOLD(tq)  (5)
#define VMXNET3_RX_ALLOC_THRESHOLD(rq, ring_idx, adapter) \
   ((rq)->rx_ring[ring_idx].size >> 3)

#define VMXNET3_GET_ADDR_LO(dma)   ((uint32)(dma))
#define VMXNET3_GET_ADDR_HI(dma)   ((uint32)(((uint64)(dma)) >> 32))

/* must be a multiple of VMXNET3_RING_SIZE_ALIGN */
#define VMXNET3_DEF_TX_RING_SIZE    512
#define VMXNET3_DEF_RX_RING_SIZE    256

/* FIXME: what's the right value for this? */
#define VMXNET3_MAX_ETH_HDR_SIZE    22 

#define VMXNET3_MAX_SKB_BUF_SIZE    (3*1024)
#endif
