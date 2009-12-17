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

#include "driver-config.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#error "vmxnet3 driver is not supported on kernels earlier than 2.6"
#endif

#include "vm_device_version.h"

#include "vmxnet3_int.h"
#include "vmxnet3_shm.h"

char vmxnet3_driver_name[] = "vmxnet3";
#define VMXNET3_DRIVER_DESC "VMware vmxnet3 virtual NIC driver"

static const struct pci_device_id vmxnet3_pciid_table[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_VMXNET3)},
	{0}
};

MODULE_DEVICE_TABLE(pci, vmxnet3_pciid_table);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static int disable_lro;
#endif

static atomic_t devices_found;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 2)
static unsigned int num_enable_shm;
#endif
#define VMXNET3_SHM_MAX_DEVICES 10
static int enable_shm[VMXNET3_SHM_MAX_DEVICES + 1] =
{ [0 ... VMXNET3_SHM_MAX_DEVICES] = -1 };
static char *shm_disclaimer = NULL;
static int correct_shm_disclaimer;
static int shm_pool_size = SHM_DEFAULT_DATA_SIZE;
#define VMXNET3_SHM_DISCLAIMER "IReallyWantThisModeIAmAVMwarePartner"

/*
 *    Enable/Disable the given intr
 */

static inline void
vmxnet3_enable_intr(struct vmxnet3_adapter *adapter, unsigned intr_idx)
{
	VMXNET3_WRITE_BAR0_REG(adapter, VMXNET3_REG_IMR + intr_idx * 8, 0);
}


static inline void
vmxnet3_disable_intr(struct vmxnet3_adapter *adapter, unsigned intr_idx)
{
	VMXNET3_WRITE_BAR0_REG(adapter, VMXNET3_REG_IMR + intr_idx * 8, 1);
}


/*
 *    Enable/Disable all intrs used by the device
 */

static void
vmxnet3_enable_all_intrs(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->intr.num_intrs; i++)
		vmxnet3_enable_intr(adapter, i);
}


static void
vmxnet3_disable_all_intrs(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->intr.num_intrs; i++)
		vmxnet3_disable_intr(adapter, i);
}


static INLINE void
vmxnet3_ack_events(struct vmxnet3_adapter *adapter, u32 events)
{
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_ECR, events);
}


static inline Bool
vmxnet3_tq_stopped(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter *adapter)
{
	return compat_netif_queue_stopped(adapter->netdev);
}


/*
 *
 * Request the stack to start/stop/wake the tq. This only deals with the OS
 * side, it does NOT handle the device side
 */
static inline void
vmxnet3_tq_start(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter  *adapter)
{
	tq->stopped = FALSE;
	compat_netif_start_queue(adapter->netdev);
}


static inline void
vmxnet3_tq_wake(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter  *adapter)
{
	tq->stopped = FALSE;
	compat_netif_wake_queue(adapter->netdev);
}


static inline void
vmxnet3_tq_stop(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter  *adapter)
{
	tq->stopped = TRUE;
	tq->num_stop++;
	compat_netif_stop_queue(adapter->netdev);
}


/*
 * This may start or stop the tx queue.
 */

static void
vmxnet3_check_link(struct vmxnet3_adapter *adapter)
{
	u32 ret;

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD, VMXNET3_CMD_GET_LINK);
	ret = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_CMD);
	adapter->link_speed = ret >> 16;
	if (ret & 1) { /* Link is up. */
		printk(KERN_INFO "%s: NIC Link is Up %d Mbps\n",
		       adapter->netdev->name, adapter->link_speed);
		if (!netif_carrier_ok(adapter->netdev))
			netif_carrier_on(adapter->netdev);

		vmxnet3_tq_start(&adapter->tx_queue, adapter);
	} else {
		printk(KERN_INFO "%s: NIC Link is Down\n",
		       adapter->netdev->name);
		if (netif_carrier_ok(adapter->netdev))
			netif_carrier_off(adapter->netdev);

		vmxnet3_tq_stop(&adapter->tx_queue, adapter);
	}
}


/*
 * process events indicated in ECR
 */

static void
vmxnet3_process_events(struct vmxnet3_adapter *adapter)
{
	u32 events = le32_to_cpu(adapter->shared->ecr);
	if (!events)
		return;

	vmxnet3_ack_events(adapter, events);

	/* Check if link state has changed */
	if (events & VMXNET3_ECR_LINK)
		vmxnet3_check_link(adapter);

	/* Check if there is an error on xmit/recv queues */
	if (events & (VMXNET3_ECR_TQERR | VMXNET3_ECR_RQERR)) {
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
				       VMXNET3_CMD_GET_QUEUE_STATUS);

		if (adapter->tqd_start->status.stopped) {
			printk(KERN_ERR "%s: tq error 0x%x\n",
			       adapter->netdev->name,
			       le32_to_cpu(adapter->tqd_start->status.error));
		}
		if (adapter->rqd_start->status.stopped) {
			printk(KERN_ERR "%s: rq error 0x%x\n",
			       adapter->netdev->name,
			       adapter->rqd_start->status.error);
		}

		compat_schedule_work(&adapter->work);
	}
}

#ifdef __BIG_ENDIAN_BITFIELD
/*
 * The device expects the bitfields in shared structures to be written in
 * little endian. When CPU is big endian, the following routines are used to
 * correctly read and write into ABI.
 * The general technique used here is : double word bitfields are defined in
 * opposite order for big endian architecture. Then before reading them in
 * driver the complete double word is translated using le32_to_cpu. Similarly
 * After the driver writes into bitfields, cpu_to_le32 is used to translate the
 * double words into required format.
 * In order to avoid touching bits in shared structure more than once, temporary
 * descriptors are used. These are passed as srcDesc to following functions.
 */
static void vmxnet3_RxDescToCPU(const struct Vmxnet3_RxDesc *srcDesc,
				struct Vmxnet3_RxDesc *dstDesc)
{
	u32 *src = (u32 *)srcDesc + 2;
	u32 *dst = (u32 *)dstDesc + 2;
	dstDesc->addr = le64_to_cpu(srcDesc->addr);
	*dst = le32_to_cpu(*src);
	dstDesc->ext1 = le32_to_cpu(srcDesc->ext1);
}

static void vmxnet3_TxDescToLe(const struct Vmxnet3_TxDesc *srcDesc,
			       struct Vmxnet3_TxDesc *dstDesc)
{
	int i;
	u32 *src = (u32 *)(srcDesc + 1);
	u32 *dst = (u32 *)(dstDesc + 1);

	/* Working backwards so that the gen bit is set at the end. */
	for (i = 2; i > 0; i--) {
		src--;
		dst--;
		*dst = cpu_to_le32(*src);
	}
}


static void vmxnet3_RxCompToCPU(const struct Vmxnet3_RxCompDesc *srcDesc,
				struct Vmxnet3_RxCompDesc *dstDesc)
{
	int i = 0;
	u32 *src = (u32 *)srcDesc;
	u32 *dst = (u32 *)dstDesc;
	for (i = 0; i < sizeof(struct Vmxnet3_RxCompDesc) / sizeof(u32); i++) {
		*dst = le32_to_cpu(*src);
		src++;
		dst++;
	}
}


/* Used to read bitfield values from double words. */
static u32 get_bitfield32(const __le32 *bitfield, u32 pos, u32 size)
{
	u32 temp = le32_to_cpu(*bitfield);
	u32 mask = ((1 << size) - 1) << pos;
	temp &= mask;
	temp >>= pos;
	return temp;
}



#endif  /* __BIG_ENDIAN_BITFIELD */

#ifdef __BIG_ENDIAN_BITFIELD

#   define VMXNET3_TXDESC_GET_GEN(txdesc) get_bitfield32(((const __le32 *) \
			txdesc) + VMXNET3_TXD_GEN_DWORD_SHIFT, \
			VMXNET3_TXD_GEN_SHIFT, VMXNET3_TXD_GEN_SIZE)
#   define VMXNET3_TXDESC_GET_EOP(txdesc) get_bitfield32(((const __le32 *) \
			txdesc) + VMXNET3_TXD_EOP_DWORD_SHIFT, \
			VMXNET3_TXD_EOP_SHIFT, VMXNET3_TXD_EOP_SIZE)
#   define VMXNET3_TCD_GET_GEN(tcd) get_bitfield32(((const __le32 *)tcd) + \
			VMXNET3_TCD_GEN_DWORD_SHIFT, VMXNET3_TCD_GEN_SHIFT, \
			VMXNET3_TCD_GEN_SIZE)
#   define VMXNET3_TCD_GET_TXIDX(tcd) get_bitfield32((const __le32 *)tcd, \
			VMXNET3_TCD_TXIDX_SHIFT, VMXNET3_TCD_TXIDX_SIZE)
#   define vmxnet3_getRxComp(dstrcd, rcd, tmp) do { \
			(dstrcd) = (tmp); \
			vmxnet3_RxCompToCPU((rcd), (tmp)); \
		} while (0)
#   define vmxnet3_getRxDesc(dstrxd, rxd, tmp) do { \
			(dstrxd) = (tmp); \
			vmxnet3_RxDescToCPU((rxd), (tmp)); \
		} while (0)

#else

#   define VMXNET3_TXDESC_GET_GEN(txdesc) ((txdesc)->gen)
#   define VMXNET3_TXDESC_GET_EOP(txdesc) ((txdesc)->eop)
#   define VMXNET3_TCD_GET_GEN(tcd) ((tcd)->gen)
#   define VMXNET3_TCD_GET_TXIDX(tcd) ((tcd)->txdIdx)
#   define vmxnet3_getRxComp(dstrcd, rcd, tmp) (dstrcd) = (rcd)
#   define vmxnet3_getRxDesc(dstrxd, rxd, tmp) (dstrxd) = (rxd)

#endif /* __BIG_ENDIAN_BITFIELD  */


static void
vmxnet3_unmap_tx_buf(struct vmxnet3_tx_buf_info *tbi,
		     struct pci_dev *pdev)
{
	if (tbi->map_type == VMXNET3_MAP_SINGLE)
		pci_unmap_single(pdev, tbi->dma_addr, tbi->len,
				 PCI_DMA_TODEVICE);
	else if (tbi->map_type == VMXNET3_MAP_PAGE)
		pci_unmap_page(pdev, tbi->dma_addr, tbi->len,
			       PCI_DMA_TODEVICE);
	else
		BUG_ON(tbi->map_type != VMXNET3_MAP_NONE);

	tbi->map_type = VMXNET3_MAP_NONE; /* to help debugging */
}


/*
 *    Returns # of tx descs that this pkt used
 *
 * Side-effects:
 *    1. mappings are freed
 *    2. buf_info[] are updated
 *    3. tx_ring.{avail, next2comp} are updated.
 */

static int
vmxnet3_unmap_pkt(u32 eop_idx, struct vmxnet3_tx_queue *tq,
		  struct pci_dev *pdev,	struct vmxnet3_adapter *adapter)
{
	struct sk_buff *skb;
	int entries = 0;

	/* no out of order completion */
	BUG_ON(tq->buf_info[eop_idx].sop_idx != tq->tx_ring.next2comp);
	BUG_ON(VMXNET3_TXDESC_GET_EOP(&(tq->tx_ring.base[eop_idx].txd)) != 1);

	dev_dbg(&adapter->pdev->dev, "tx complete [%u %u]\n",
		tq->tx_ring.next2comp, eop_idx);
	skb = tq->buf_info[eop_idx].skb;
	BUG_ON(skb == NULL);
	tq->buf_info[eop_idx].skb = NULL;

	VMXNET3_INC_RING_IDX_ONLY(eop_idx, tq->tx_ring.size);

	while (tq->tx_ring.next2comp != eop_idx) {
		vmxnet3_unmap_tx_buf(tq->buf_info + tq->tx_ring.next2comp,
				     pdev);

		/* update next2comp w/o tx_lock. Since we are marking more,
		 * instead of less, tx ring entries avail, the worst case is
		 * that the tx routine incorrectly re-queues a pkt due to
		 * insufficient tx ring entries.
		 */
		vmxnet3_cmd_ring_adv_next2comp(&tq->tx_ring);
		entries++;
	}

	vmxnet3_dev_kfree_skb_any(adapter, skb);
	return entries;
}


static int
vmxnet3_tq_tx_complete(struct vmxnet3_tx_queue *tq,
		       struct vmxnet3_adapter *adapter)
{
	int completed = 0;
	union Vmxnet3_GenericDesc *gdesc;

	gdesc = tq->comp_ring.base + tq->comp_ring.next2proc;
	while (VMXNET3_TCD_GET_GEN(&gdesc->tcd) == tq->comp_ring.gen) {
		completed += vmxnet3_unmap_pkt(VMXNET3_TCD_GET_TXIDX(
					       &gdesc->tcd), tq, adapter->pdev,
					       adapter);

		vmxnet3_comp_ring_adv_next2proc(&tq->comp_ring);
		gdesc = tq->comp_ring.base + tq->comp_ring.next2proc;
	}

	if (completed) {
		spin_lock(&tq->tx_lock);
		if (unlikely(vmxnet3_tq_stopped(tq, adapter) &&
			     vmxnet3_cmd_ring_desc_avail(&tq->tx_ring) >
			     VMXNET3_WAKE_QUEUE_THRESHOLD(tq) &&
			     compat_netif_carrier_ok(adapter->netdev))) {
			vmxnet3_tq_wake(tq, adapter);
		}
		spin_unlock(&tq->tx_lock);
	}
	return completed;
}


static void
vmxnet3_tq_cleanup(struct vmxnet3_tx_queue *tq,
		   struct vmxnet3_adapter *adapter)
{
	while (tq->tx_ring.next2comp != tq->tx_ring.next2fill) {
		struct vmxnet3_tx_buf_info *tbi;
		union Vmxnet3_GenericDesc *gdesc;

		tbi = tq->buf_info + tq->tx_ring.next2comp;
		gdesc = tq->tx_ring.base + tq->tx_ring.next2comp;

		vmxnet3_unmap_tx_buf(tbi, adapter->pdev);
		if (tbi->skb) {
			vmxnet3_dev_kfree_skb_any(adapter, tbi->skb);
			tbi->skb = NULL;
		}
		vmxnet3_cmd_ring_adv_next2comp(&tq->tx_ring);
	}

	/* sanity check */
#ifdef VMX86_DEBUG
	{
		/* verify all buffers are indeed unmapped and freed */
		int i;
		for (i = 0; i < tq->tx_ring.size; i++) {
			BUG_ON(tq->buf_info[i].skb != NULL ||
			       tq->buf_info[i].map_type != VMXNET3_MAP_NONE);
		}
	}
#endif

	tq->tx_ring.gen = VMXNET3_INIT_GEN;
	tq->tx_ring.next2fill = tq->tx_ring.next2comp = 0;

	tq->comp_ring.gen = VMXNET3_INIT_GEN;
	tq->comp_ring.next2proc = 0;
}


/*
 *	free rings and buf_info for the tx queue. There must be no pending pkt
 *	in the tx ring. the .base fields of all rings and buf_info will be
 *	set to NULL
 */

void
vmxnet3_tq_destroy(struct vmxnet3_tx_queue *tq,
		   struct vmxnet3_adapter *adapter)
{
	if (tq->tx_ring.base) {
		pci_free_consistent(adapter->pdev, tq->tx_ring.size *
				    sizeof(struct Vmxnet3_TxDesc),
				    tq->tx_ring.base, tq->tx_ring.basePA);
		tq->tx_ring.base = NULL;
	}
	if (tq->data_ring.base) {
		pci_free_consistent(adapter->pdev, tq->data_ring.size *
				    sizeof(struct Vmxnet3_TxDataDesc),
				    tq->data_ring.base, tq->data_ring.basePA);
		tq->data_ring.base = NULL;
	}
	if (tq->comp_ring.base) {
		pci_free_consistent(adapter->pdev, tq->comp_ring.size *
				    sizeof(struct Vmxnet3_TxCompDesc),
				    tq->comp_ring.base, tq->comp_ring.basePA);
		tq->comp_ring.base = NULL;
	}
	kfree(tq->buf_info);
	tq->buf_info = NULL;
}


/*
 *    reset all internal states and rings for a tx queue
 * Side-effects:
 *    1. contents of the rings are reset to 0
 *    2. indices and gen of rings are reset
 *    3. bookkeeping data is reset
 */
static void
vmxnet3_tq_init(struct vmxnet3_tx_queue *tq,
		struct vmxnet3_adapter *adapter)
{
	int i;

	/* reset the tx ring contents to 0 and reset the tx ring states */
	memset(tq->tx_ring.base, 0,
	       tq->tx_ring.size * sizeof(struct Vmxnet3_TxDesc));
	tq->tx_ring.next2fill = tq->tx_ring.next2comp = 0;
	tq->tx_ring.gen = VMXNET3_INIT_GEN;

	memset(tq->data_ring.base, 0,
	       tq->data_ring.size * sizeof(struct Vmxnet3_TxDataDesc));

	/* reset the tx comp ring contents to 0 and reset comp ring states */
	memset(tq->comp_ring.base, 0,
	       tq->comp_ring.size * sizeof(struct Vmxnet3_TxCompDesc));
	tq->comp_ring.next2proc = 0;
	tq->comp_ring.gen = VMXNET3_INIT_GEN;

	/* reset the bookkeeping data */
	memset(tq->buf_info, 0, sizeof(tq->buf_info[0]) * tq->tx_ring.size);
	for (i = 0; i < tq->tx_ring.size; i++)
		tq->buf_info[i].map_type = VMXNET3_MAP_NONE;

	/* stats are not reset */
}


/*
 * allocate and initialize rings for the tx queue, also allocate and
 * initialize buf_info. Returns 0 on success, negative errno on failure.
 */
static int
vmxnet3_tq_create(struct vmxnet3_tx_queue *tq,
		  struct vmxnet3_adapter *adapter)
{
	BUG_ON(tq->tx_ring.size <= 0 ||
	       tq->data_ring.size != tq->tx_ring.size);
	BUG_ON((tq->tx_ring.size & VMXNET3_RING_SIZE_MASK) != 0);
	BUG_ON(tq->tx_ring.base || tq->data_ring.base || tq->comp_ring.base ||
	       tq->buf_info);

	tq->tx_ring.base = pci_alloc_consistent(adapter->pdev,
			   tq->tx_ring.size * sizeof(struct Vmxnet3_TxDesc),
			   &tq->tx_ring.basePA);
	if (!tq->tx_ring.base) {
		printk(KERN_ERR "%s: failed to allocate tx ring\n",
		       adapter->netdev->name);
		goto err;
	}

	tq->data_ring.base = pci_alloc_consistent(adapter->pdev,
			     tq->data_ring.size *
			     sizeof(struct Vmxnet3_TxDataDesc),
			     &tq->data_ring.basePA);
	if (!tq->data_ring.base) {
		printk(KERN_ERR "%s: failed to allocate data ring\n",
		       adapter->netdev->name);
		goto err;
	}

	tq->comp_ring.base = pci_alloc_consistent(adapter->pdev,
			     tq->comp_ring.size *
			     sizeof(struct Vmxnet3_TxCompDesc),
			     &tq->comp_ring.basePA);
	if (!tq->comp_ring.base) {
		printk(KERN_ERR "%s: failed to allocate tx comp ring\n",
		       adapter->netdev->name);
		goto err;
	}

	tq->buf_info = kcalloc(tq->tx_ring.size, sizeof(tq->buf_info[0]),
			       GFP_KERNEL);
	if (!tq->buf_info) {
		printk(KERN_ERR "%s: failed to allocate tx bufinfo\n",
		       adapter->netdev->name);
		goto err;
	}

	return 0;

err:
	vmxnet3_tq_destroy(tq, adapter);
	return -ENOMEM;
}


/*
 *    starting from ring->next2fill, allocate rx buffers for the given ring
 *    of the rx queue and update the rx desc. stop after @num_to_alloc buffers
 *    are allocated or allocation fails. Returns # of buffers allocated
 *
 * Side-effects:
 *    1. rx descs are updated
 *    2. ring->{gen, next2fill} are updated
 *    3. uncommitted[ring_idx] is incremented
 */

static int
vmxnet3_rq_alloc_rx_buf(struct vmxnet3_rx_queue *rq, u32 ring_idx,
			int num_to_alloc, struct vmxnet3_adapter *adapter)
{
	int num_allocated = 0;
	struct vmxnet3_rx_buf_info *rbi_base = rq->buf_info[ring_idx];
	struct vmxnet3_cmd_ring *ring = &rq->rx_ring[ring_idx];
	u32 val;

	while (num_allocated < num_to_alloc) {
		struct vmxnet3_rx_buf_info *rbi;
		union Vmxnet3_GenericDesc *gd;

		rbi = rbi_base + ring->next2fill;
		gd = ring->base + ring->next2fill;

		if (rbi->buf_type == VMXNET3_RX_BUF_SKB) {
			if (rbi->skb == NULL) {
				rbi->skb = vmxnet3_dev_alloc_skb(adapter,
					 	rbi->len + COMPAT_NET_IP_ALIGN);
				if (unlikely(rbi->skb == NULL)) {
					rq->stats.rx_buf_alloc_failure++;
					break;
				}
				rbi->skb->dev = adapter->netdev;

				if (!adapter->is_shm)
					skb_reserve(rbi->skb, NET_IP_ALIGN);
				rbi->dma_addr = vmxnet3_map_single(
						adapter, rbi->skb, 0,
						rbi->len, PCI_DMA_FROMDEVICE);
			} else {
				/* rx buffer skipped by the device */
			}
			val = VMXNET3_RXD_BTYPE_HEAD << VMXNET3_RXD_BTYPE_SHIFT;
		} else {
			BUG_ON(rbi->buf_type != VMXNET3_RX_BUF_PAGE ||
			       rbi->len  != PAGE_SIZE);

			if (rbi->page == NULL) {
				rbi->page = vmxnet3_alloc_page(adapter);
				if (unlikely(rbi->page == NULL)) {
					rq->stats.rx_buf_alloc_failure++;
					break;
				}
				rbi->dma_addr = vmxnet3_map_page(adapter,
						rbi->page, 0, PAGE_SIZE,
						PCI_DMA_FROMDEVICE);
			} else {
				/* rx buffers skipped by the device */
			}
			val = VMXNET3_RXD_BTYPE_BODY << VMXNET3_RXD_BTYPE_SHIFT;
		}

		BUG_ON(rbi->dma_addr == 0);
		gd->rxd.addr = cpu_to_le64(rbi->dma_addr);
		gd->dword[2] = cpu_to_le32((ring->gen << VMXNET3_RXD_GEN_SHIFT)
					   | val | rbi->len);

		num_allocated++;
		vmxnet3_cmd_ring_adv_next2fill(ring);
	}
	rq->uncommitted[ring_idx] += num_allocated;

	dev_dbg(&adapter->pdev->dev, "alloc_rx_buf: %d allocated, next2fill "
		"%u, next2comp %u, uncommited %u\n", num_allocated,
		ring->next2fill, ring->next2comp, rq->uncommitted[ring_idx]);

	/* so that the device can distinguish a full ring and an empty ring */
	BUG_ON(num_allocated != 0 && ring->next2fill == ring->next2comp);

	return num_allocated;
}


/*
 * It assumes the skb still has space to accommodate the frag. It only
 * increments skb->data_len
 */

static void
vmxnet3_append_frag(struct sk_buff *skb, struct Vmxnet3_RxCompDesc *rcd,
		    struct vmxnet3_rx_buf_info *rbi)
{
	struct skb_frag_struct *frag = skb_shinfo(skb)->frags +
		skb_shinfo(skb)->nr_frags;

	BUG_ON(skb_shinfo(skb)->nr_frags >= MAX_SKB_FRAGS);

	frag->page = rbi->page;
	frag->page_offset = 0;
	frag->size = rcd->len;
	skb->data_len += frag->size;
	skb_shinfo(skb)->nr_frags++;
}


/*
 * Map the tx buffer and set up ONLY TXD.{addr, len, gen} based on the mapping.
 * It sets the other fields of the descriptors to 0.
 * Side Effects :
 *    1. the corresponding buf_info entries are upated,
 *    2. ring indices are advanced
 */

static void
vmxnet3_map_pkt(struct sk_buff *skb, struct vmxnet3_tx_ctx *ctx,
		struct vmxnet3_tx_queue *tq, struct pci_dev *pdev,
		struct vmxnet3_adapter *adapter)
{
	u32 dw2, len;
	unsigned long buf_offset;
	int i;
	union Vmxnet3_GenericDesc *gdesc;
	struct vmxnet3_tx_buf_info *tbi = NULL;

	BUG_ON(ctx->copy_size > skb_headlen(skb));

	/* use the previous gen bit for the SOP desc */
	dw2 = (tq->tx_ring.gen ^ 0x1) << VMXNET3_TXD_GEN_SHIFT;

	ctx->sop_txd = tq->tx_ring.base + tq->tx_ring.next2fill;
	gdesc = ctx->sop_txd; /* both loops below can be skipped */

	/* no need to map the buffer if headers are copied */
	if (ctx->copy_size) {
		BUG_ON(VMXNET3_TXDESC_GET_GEN(&(ctx->sop_txd->txd)) ==
		       tq->tx_ring.gen);

		ctx->sop_txd->txd.addr = cpu_to_le64(tq->data_ring.basePA +
					tq->tx_ring.next2fill *
					sizeof(struct Vmxnet3_TxDataDesc));
		ctx->sop_txd->dword[2] = cpu_to_le32(dw2 | ctx->copy_size);
		ctx->sop_txd->dword[3] = 0;

		tbi = tq->buf_info + tq->tx_ring.next2fill;
		tbi->map_type = VMXNET3_MAP_NONE;

		dev_dbg(&adapter->pdev->dev, "txd[%u]: 0x%llu 0x%x 0x%x\n",
			tq->tx_ring.next2fill,
			le64_to_cpu(ctx->sop_txd->txd.addr),
			ctx->sop_txd->dword[2], ctx->sop_txd->dword[3]);
		vmxnet3_cmd_ring_adv_next2fill(&tq->tx_ring);

		/* use the right gen for non-SOP desc */
		dw2 = tq->tx_ring.gen << VMXNET3_TXD_GEN_SHIFT;
	}

	/* linear part can use multiple tx desc if it's big */
	len = vmxnet3_skb_headlen(adapter, skb) - ctx->copy_size;
	buf_offset = ctx->copy_size;
	while (len) {
		u32 buf_size;

		buf_size = len > VMXNET3_MAX_TX_BUF_SIZE ?
			   VMXNET3_MAX_TX_BUF_SIZE : len;

		tbi = tq->buf_info + tq->tx_ring.next2fill;
		tbi->map_type = VMXNET3_MAP_SINGLE;
		tbi->dma_addr = vmxnet3_map_single(adapter, skb, buf_offset,
				buf_size, PCI_DMA_TODEVICE);

		tbi->len = buf_size; /* this automatically convert 2^14 to 0 */

		gdesc = tq->tx_ring.base + tq->tx_ring.next2fill;
		BUG_ON(gdesc->txd.gen == tq->tx_ring.gen);

		gdesc->txd.addr = cpu_to_le64(tbi->dma_addr);
		gdesc->dword[2] = cpu_to_le32(dw2 | buf_size);
		gdesc->dword[3] = 0;

		dev_dbg(&adapter->pdev->dev,
			"txd[%u]: 0x%llu 0x%x 0x%x\n",
			tq->tx_ring.next2fill, le64_to_cpu(gdesc->txd.addr),
			le32_to_cpu(gdesc->dword[2]), gdesc->dword[3]);
		vmxnet3_cmd_ring_adv_next2fill(&tq->tx_ring);
		dw2 = tq->tx_ring.gen << VMXNET3_TXD_GEN_SHIFT;

		len -= buf_size;
		buf_offset += buf_size;

		if (adapter->is_shm) {
		/*
		 * The linear region of the skb is never larger than a page, so
		 * it always fits into one descriptor.
		 */
			BUG_ON(len != 0);
		}
	}

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];

		tbi = tq->buf_info + tq->tx_ring.next2fill;
		tbi->map_type = VMXNET3_MAP_PAGE;
		tbi->dma_addr = vmxnet3_map_page(adapter, frag->page,
						frag->page_offset, frag->size,
						PCI_DMA_TODEVICE);

		tbi->len = frag->size;

		gdesc = tq->tx_ring.base + tq->tx_ring.next2fill;
		BUG_ON(gdesc->txd.gen == tq->tx_ring.gen);

		gdesc->txd.addr = cpu_to_le64(tbi->dma_addr);
		gdesc->dword[2] = cpu_to_le32(dw2 | frag->size);
		gdesc->dword[3] = 0;

		dev_dbg(&adapter->pdev->dev,
			"txd[%u]: 0x%llu %u %u\n",
			tq->tx_ring.next2fill, le64_to_cpu(gdesc->txd.addr),
			le32_to_cpu(gdesc->dword[2]), gdesc->dword[3]);
		vmxnet3_cmd_ring_adv_next2fill(&tq->tx_ring);
		dw2 = tq->tx_ring.gen << VMXNET3_TXD_GEN_SHIFT;
	}

	ctx->eop_txd = gdesc;

	/* set the last buf_info for the pkt */
	tbi->skb = skb;
	tbi->sop_idx = ctx->sop_txd - tq->tx_ring.base;
}


/*
 *    parse and copy relevant protocol headers:
 *     For a tso pkt, relevant headers are L2/3/4 including options
 *     For a pkt requesting csum offloading, they are L2/3 and may include L4
 *     if it's a TCP/UDP pkt
 *
 *    The implementation works only when h/w vlan insertion is used, see PR
 *    171928
 *
 * Result:
 *    -1:  error happens during parsing
 *     0:  protocol headers parsed, but too big to be copied
 *     1:  protocol headers parsed and copied
 *
 * Side-effects:
 *    1. related *ctx fields are updated.
 *    2. ctx->copy_size is # of bytes copied
 *    3. the portion copied is guaranteed to be in the linear part
 *
 */
static int
vmxnet3_parse_and_copy_hdr(struct sk_buff *skb, struct vmxnet3_tx_queue *tq,
			   struct vmxnet3_tx_ctx *ctx,
			   struct vmxnet3_adapter *adapter)
{
	struct Vmxnet3_TxDataDesc *tdd;

	if (ctx->mss) {
		ctx->eth_ip_hdr_size = compat_skb_transport_offset(skb);
		ctx->l4_hdr_size = compat_skb_tcp_header(skb)->doff * 4;
		ctx->copy_size = ctx->eth_ip_hdr_size + ctx->l4_hdr_size;
	} else {
		unsigned int pull_size;

		if (skb->ip_summed == VM_TX_CHECKSUM_PARTIAL) {
			ctx->eth_ip_hdr_size = compat_skb_transport_offset(skb);

			if (ctx->ipv4) {
				struct iphdr *iph = (struct iphdr *)
						    compat_skb_ip_header(skb);
				if (iph->protocol == IPPROTO_TCP) {
					pull_size = ctx->eth_ip_hdr_size +
						    sizeof(struct tcphdr);

					if (unlikely(!compat_pskb_may_pull(skb,
								pull_size))) {
						goto err;
					}
					ctx->l4_hdr_size =
					   compat_skb_tcp_header(skb)->doff * 4;
				} else if (iph->protocol == IPPROTO_UDP) {
					ctx->l4_hdr_size =
							sizeof(struct udphdr);
				} else {
					ctx->l4_hdr_size = 0;
				}
			} else {
				/* for simplicity, don't copy L4 headers */
				ctx->l4_hdr_size = 0;
			}
			ctx->copy_size = ctx->eth_ip_hdr_size +
					 ctx->l4_hdr_size;
		} else {
			ctx->eth_ip_hdr_size = 0;
			ctx->l4_hdr_size = 0;
			/* copy as much as allowed */
			ctx->copy_size = min((unsigned int)VMXNET3_HDR_COPY_SIZE
					   , vmxnet3_skb_headlen(adapter, skb));
		}

		if(!adapter->is_shm) {
		         /* make sure headers are accessible directly */
			if (unlikely(!compat_pskb_may_pull(skb,
							ctx->copy_size)))
				goto err;

		}
	}

	if (unlikely(ctx->copy_size > VMXNET3_HDR_COPY_SIZE)) {
		tq->stats.oversized_hdr++;
		ctx->copy_size = 0;
		return 0;
	}

	tdd = tq->data_ring.base + tq->tx_ring.next2fill;
	BUG_ON(ctx->copy_size > vmxnet3_skb_headlen(adapter, skb));

	if (!adapter->is_shm) {
		memcpy(tdd->data, skb->data, ctx->copy_size);
	} else {
		void *virt = kmap(VMXNET3_SHM_IDX2PAGE(adapter->shm,
				  VMXNET3_SHM_SKB_GETIDX(skb)));
		memcpy(tdd->data, virt, ctx->copy_size);
		kunmap(VMXNET3_SHM_IDX2PAGE(adapter->shm,
		       VMXNET3_SHM_SKB_GETIDX(skb)));
	}

	dev_dbg(&adapter->pdev->dev, "copy %u bytes to dataRing[%u]\n",
		ctx->copy_size, tq->tx_ring.next2fill);
	return 1;

err:
	return -1;
}


/*
 *    Fix pkt headers for tso. ip hdr and tcp hdr are changed
 */

static void
vmxnet3_prepare_tso(struct sk_buff *skb, struct vmxnet3_tx_ctx *ctx)
{
	struct tcphdr *tcph = compat_skb_tcp_header(skb);
	if (ctx->ipv4) {
		struct iphdr *iph = compat_skb_ip_header(skb);
		iph->check = 0;
		tcph->check = ~csum_tcpudp_magic(iph->saddr, iph->daddr, 0,
						 IPPROTO_TCP, 0);
#ifdef NETIF_F_TSO6
	} else {
		struct ipv6hdr *iph = (struct ipv6hdr *)
				      compat_skb_network_header(skb);
		tcph->check = ~csum_ipv6_magic(&iph->saddr, &iph->daddr, 0,
					       IPPROTO_TCP, 0);
#endif
	}
}

inline void vmxnet3_le32_add_cpu(uint32 *addTo, uint32 addThis)
{
	*addTo = cpu_to_le32(le32_to_cpu(*addTo) + addThis);
}


/*
 *    transmit a pkt thru a given tq
 *
 * Result:
 *    COMPAT_NETDEV_TX_OK:      descriptors are setup successfully
 *    COMPAT_NETDEV_TX_OK:      error occured, the pkt is dropped
 *    COMPAT_NETDEV_TX_BUSY:    tx ring is full, queue is stopped
 *
 * Side-effects:
 *    1. tx ring may be changed
 *    2. tq stats may be updated accordingly
 *    3. shared->txNumDeferred may be updated
 */

int
vmxnet3_tq_xmit(struct sk_buff *skb,
		struct vmxnet3_tx_queue *tq,
		struct vmxnet3_adapter *adapter,
		struct net_device *netdev)
{
	int ret;
	u32 count;
	unsigned long flags;
	struct vmxnet3_tx_ctx ctx;
	union Vmxnet3_GenericDesc *gdesc;
#ifdef __BIG_ENDIAN_BITFIELD
	/* Use temporary descriptor to avoid touching bits multiple times */
	union Vmxnet3_GenericDesc tempTxDesc;
#endif

	/* conservatively estimate # of descriptors to use */
	count = VMXNET3_TXD_NEEDED(vmxnet3_skb_headlen(adapter, skb)) +
		skb_shinfo(skb)->nr_frags + 1;

	ctx.ipv4 = (skb->protocol == __constant_ntohs(ETH_P_IP));

	ctx.mss = compat_skb_mss(skb);
	if (ctx.mss) {
		if (compat_skb_header_cloned(skb)) {
			if (unlikely(pskb_expand_head(skb, 0, 0,
						      GFP_ATOMIC) != 0)) {
				tq->stats.drop_tso++;
				goto drop_pkt;
			}
			tq->stats.copy_skb_header++;
		}
		vmxnet3_prepare_tso(skb, &ctx);
	} else {
		if (unlikely(count > VMXNET3_MAX_TXD_PER_PKT)) {
			if (unlikely(adapter->is_shm)) {
				BUG_ON(count > VMXNET3_MAX_TXD_PER_PKT_SHM);
				if (count > VMXNET3_MAX_TXD_PER_PKT_SHM) {
					tq->stats.drop_too_many_frags++;
					goto drop_pkt;
				}
			} else {
				/* non-tso pkts must not use more than
				 * VMXNET3_MAX_TXD_PER_PKT entries
				 */
				if (compat_skb_linearize(skb) != 0) {
					tq->stats.drop_too_many_frags++;
					goto drop_pkt;
				}
				tq->stats.linearized++;

				/* recalculate the # of descriptors to use */
				count = VMXNET3_TXD_NEEDED(vmxnet3_skb_headlen(
							adapter, skb)) + 1;
			}
		}
	}

	ret = vmxnet3_parse_and_copy_hdr(skb, tq, &ctx, adapter);
	if (ret >= 0) {
		BUG_ON(ret <= 0 && ctx.copy_size != 0);
		/* hdrs parsed, check against other limits */
		if (ctx.mss) {
			if (unlikely(ctx.eth_ip_hdr_size + ctx.l4_hdr_size >
				     VMXNET3_MAX_TX_BUF_SIZE)) {
				goto hdr_too_big;
			}
		} else {
			if (skb->ip_summed == VM_TX_CHECKSUM_PARTIAL) {
				if (unlikely(ctx.eth_ip_hdr_size +
					     compat_skb_csum_offset(skb) >
					     VMXNET3_MAX_CSUM_OFFSET)) {
					goto hdr_too_big;
				}
			}
		}
	} else {
		tq->stats.drop_hdr_inspect_err++;
		goto drop_pkt;
	}

	spin_lock_irqsave(&tq->tx_lock, flags);

	if (count > vmxnet3_cmd_ring_desc_avail(&tq->tx_ring)) {
		tq->stats.tx_ring_full++;
		dev_dbg(&adapter->pdev->dev, "tx queue stopped on %s, next2comp"
			" %u next2fill %u\n", adapter->netdev->name,
			tq->tx_ring.next2comp, tq->tx_ring.next2fill);

		vmxnet3_tq_stop(tq, adapter);
		spin_unlock_irqrestore(&tq->tx_lock, flags);
		return COMPAT_NETDEV_TX_BUSY;
	}

	/* fill tx descs related to addr & len */
	vmxnet3_map_pkt(skb, &ctx, tq, adapter->pdev, adapter);

	/* setup the EOP desc */
	ctx.eop_txd->dword[3] = cpu_to_le32(VMXNET3_TXD_CQ | VMXNET3_TXD_EOP);

	/* setup the SOP desc */
#ifdef __BIG_ENDIAN_BITFIELD
	gdesc = &tempTxDesc;
	gdesc->dword[2] = ctx.sop_txd->dword[2];
	gdesc->dword[3] = ctx.sop_txd->dword[3];
#else
	gdesc = ctx.sop_txd;
#endif
	if (ctx.mss) {
		gdesc->txd.hlen = ctx.eth_ip_hdr_size + ctx.l4_hdr_size;
		gdesc->txd.om = VMXNET3_OM_TSO;
		gdesc->txd.msscof = ctx.mss;
		vmxnet3_le32_add_cpu(&tq->shared->txNumDeferred, (skb->len -
			     gdesc->txd.hlen + ctx.mss - 1) / ctx.mss);
	} else {
		if (skb->ip_summed == VM_TX_CHECKSUM_PARTIAL) {
			gdesc->txd.hlen = ctx.eth_ip_hdr_size;
			gdesc->txd.om = VMXNET3_OM_CSUM;
			gdesc->txd.msscof = ctx.eth_ip_hdr_size +
					    compat_skb_csum_offset(skb);
		} else {
			gdesc->txd.om = 0;
			gdesc->txd.msscof = 0;
		}
		vmxnet3_le32_add_cpu(&tq->shared->txNumDeferred, 1);
	}

	if (vlan_tx_tag_present(skb)) {
		gdesc->txd.ti = 1;
		gdesc->txd.tci = vlan_tx_tag_get(skb);
	}

	/* finally flips the GEN bit of the SOP desc. */
	gdesc->dword[2] = cpu_to_le32(le32_to_cpu(gdesc->dword[2]) ^
						  VMXNET3_TXD_GEN);
#ifdef __BIG_ENDIAN_BITFIELD
	/* Finished updating in bitfields of Tx Desc, so write them in original
	 * place.
	 */
	vmxnet3_TxDescToLe((struct Vmxnet3_TxDesc *)gdesc,
			   (struct Vmxnet3_TxDesc *)ctx.sop_txd);
	gdesc = ctx.sop_txd;
#endif
	dev_dbg(&adapter->pdev->dev,
		"txd[%u]: SOP 0x%llu 0x%x 0x%x\n",
		(u32)((union Vmxnet3_GenericDesc *)ctx.sop_txd -
		tq->tx_ring.base), le64_to_cpu(gdesc->txd.addr),
		le32_to_cpu(gdesc->dword[2]), le32_to_cpu(gdesc->dword[3]));

	spin_unlock_irqrestore(&tq->tx_lock, flags);

	if (le32_to_cpu(tq->shared->txNumDeferred) >=
					le32_to_cpu(tq->shared->txThreshold)) {
		tq->shared->txNumDeferred = 0;
		VMXNET3_WRITE_BAR0_REG(adapter, VMXNET3_REG_TXPROD,
				       tq->tx_ring.next2fill);
	}
	netdev->trans_start = jiffies;

	return COMPAT_NETDEV_TX_OK;

hdr_too_big:
	tq->stats.drop_oversized_hdr++;
drop_pkt:
	tq->stats.drop_total++;
	vmxnet3_dev_kfree_skb(adapter, skb);
	return COMPAT_NETDEV_TX_OK;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
static int
#else
static netdev_tx_t
#endif
vmxnet3_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);

	if (adapter->is_shm)
		return vmxnet3_shm_start_tx(skb, netdev);
	else
		return vmxnet3_tq_xmit(skb, &adapter->tx_queue, adapter, netdev);
}


/* called to process csum related bits in the EOP RCD descriptor */
static void
vmxnet3_rx_csum(struct vmxnet3_adapter *adapter,
		struct sk_buff *skb,
		union Vmxnet3_GenericDesc *gdesc)
{
	if (!gdesc->rcd.cnc && adapter->rxcsum) {
		/* typical case: TCP/UDP over IP and both csums are correct */
		if ((le32_to_cpu(gdesc->dword[3]) & VMXNET3_RCD_CSUM_OK) ==
							VMXNET3_RCD_CSUM_OK) {
			skb->ip_summed = VM_CHECKSUM_UNNECESSARY;
			BUG_ON(!(gdesc->rcd.tcp || gdesc->rcd.udp));
			BUG_ON(!(gdesc->rcd.v4  || gdesc->rcd.v6));
			BUG_ON(gdesc->rcd.frg);
		} else {
			if (gdesc->rcd.csum) {
				skb->csum = htons(gdesc->rcd.csum);
				skb->ip_summed = VM_RX_CHECKSUM_PARTIAL;
			} else {
				skb->ip_summed = CHECKSUM_NONE;
			}
		}
	} else {
		skb->ip_summed = CHECKSUM_NONE;
	}
}


/*
 * called when ERR bit is set for a received pkt. The desc and the associated
 * rx buffer have not been processed yet.
 *
 * Side-effects:
 *    1. up the stat counters
 *    2. free the skb if needed
 *    3. reset ctx->skb
 */

static void
vmxnet3_rx_error(struct vmxnet3_rx_queue *rq, struct Vmxnet3_RxCompDesc *rcd,
		 struct vmxnet3_rx_ctx *ctx,  struct vmxnet3_adapter *adapter)
{
	rq->stats.drop_err++;
	if (!rcd->fcs)
		rq->stats.drop_fcs++;

	rq->stats.drop_total++;

	/*
	 * We do not unmap and chain the rx buffer to the skb.
	 * We basically pretend this buffer is not used and will be recycled
	 * by vmxnet3_rq_alloc_rx_buf()
	 */

	/*
	 * ctx->skb may be NULL if this is the first and the only one
	 * desc for the pkt
	 */
	if (ctx->skb)
		vmxnet3_dev_kfree_skb_irq(adapter, ctx->skb);

	ctx->skb = NULL;
}

/* process the rx completion ring of the given rx queue. Quota specifies the
 * max # of rx completion entries to be processed. Returns # of descs completed.
 */
static int
#ifdef VMXNET3_NAPI
vmxnet3_rq_rx_complete(struct vmxnet3_rx_queue *rq,
		       struct vmxnet3_adapter *adapter, int quota)
#else
vmxnet3_rq_rx_complete(struct vmxnet3_rx_queue *rq,
		       struct vmxnet3_adapter *adapter)
#endif
{
	static u32 rxprod_reg[2] = {VMXNET3_REG_RXPROD, VMXNET3_REG_RXPROD2};
	u32 num_rxd = 0;
	struct Vmxnet3_RxCompDesc *rcd;
	struct vmxnet3_rx_ctx *ctx = &rq->rx_ctx;
#ifdef __BIG_ENDIAN_BITFIELD
	struct Vmxnet3_RxDesc rxCmdDesc;
	struct Vmxnet3_RxCompDesc rxComp;
#endif
	vmxnet3_getRxComp(rcd, &rq->comp_ring.base[rq->comp_ring.next2proc].rcd,
			  &rxComp);
	while (rcd->gen == rq->comp_ring.gen) {
		struct vmxnet3_rx_buf_info *rbi;
		struct sk_buff *skb;
		int num_to_alloc;
		struct Vmxnet3_RxDesc *rxd;
		u32 idx, ring_idx;
#ifdef VMXNET3_NAPI
		if (num_rxd >= quota) {
			/* we may stop even before we see the EOP desc of
			 * the current pkt
			 */
			break;
		}
		num_rxd++;
#endif

		idx = rcd->rxdIdx;
		ring_idx = rcd->rqID == rq->qid ? 0 : 1;
		vmxnet3_getRxDesc(rxd, &rq->rx_ring[ring_idx].base[idx].rxd,
				  &rxCmdDesc);
		rbi = rq->buf_info[ring_idx] + idx;

		BUG_ON(rcd->len > rxd->len);
		BUG_ON(rxd->addr != rbi->dma_addr ||
		       rxd->len != rbi->len);

		if (unlikely(rcd->eop && rcd->err)) {
			vmxnet3_rx_error(rq, rcd, ctx, adapter);
			goto rcd_done;
		}

		if (rcd->sop) { /* first buf of the pkt */
			BUG_ON(rxd->btype != VMXNET3_RXD_BTYPE_HEAD ||
			       rcd->rqID != rq->qid);

			BUG_ON(rbi->buf_type != VMXNET3_RX_BUF_SKB);
			BUG_ON(ctx->skb != NULL || rbi->skb == NULL);

			if (unlikely(rcd->len == 0)) {
				/* Pretend the rx buffer is skipped. */
				BUG_ON(!(rcd->sop && rcd->eop));
				dev_dbg(&adapter->pdev->dev, "rxRing[%u][%u] 0"
					" length\n", ring_idx, idx);
				goto rcd_done;
			}

			ctx->skb = rbi->skb;
			rbi->skb = NULL;

			pci_unmap_single(adapter->pdev, rbi->dma_addr, rbi->len,
					 PCI_DMA_FROMDEVICE);

			vmxnet3_skb_put(adapter, ctx->skb, rcd->len);
		} else {
			BUG_ON(ctx->skb == NULL);
			/* non SOP buffer must be type 1 in most cases */
			if (rbi->buf_type == VMXNET3_RX_BUF_PAGE) {
				BUG_ON(rxd->btype != VMXNET3_RXD_BTYPE_BODY);

				if (rcd->len) {
					pci_unmap_page(adapter->pdev,
						       rbi->dma_addr, rbi->len,
						       PCI_DMA_FROMDEVICE);

					vmxnet3_append_frag(ctx->skb, rcd, rbi);
					rbi->page = NULL;
				}
			} else {
				/*
				 * The only time a non-SOP buffer is type 0 is
				 * when it's EOP and error flag is raised, which
				 * has already been handled.
				 */
				BUG_ON(TRUE);
			}
		}

		skb = ctx->skb;
		if (rcd->eop) {
			if (adapter->is_shm) {
				vmxnet3_shm_rx_skb(adapter, skb);

				wake_up(&adapter->shm->rxq);
			} else {
				skb->len += skb->data_len;
				skb->truesize += skb->data_len;

				vmxnet3_rx_csum(adapter, skb,
					(union Vmxnet3_GenericDesc *)rcd);
				skb->protocol = eth_type_trans(skb,
							       adapter->netdev);

#ifdef VMXNET3_NAPI
				if (unlikely(adapter->vlan_grp && rcd->ts)) {
					vlan_hwaccel_receive_skb(skb,
						adapter->vlan_grp, rcd->tci);
				} else {
					netif_receive_skb(skb);
				}
#else
				if (unlikely(adapter->vlan_grp && rcd->ts)) {
					vlan_hwaccel_rx(skb, adapter->vlan_grp,
							rcd->tci);
				} else {
					netif_rx(skb);
				}
#endif
			}

			adapter->netdev->last_rx = jiffies;
			ctx->skb = NULL;
		}

rcd_done:
		/* device may skip some rx descs */
		rq->rx_ring[ring_idx].next2comp = idx;
		VMXNET3_INC_RING_IDX_ONLY(rq->rx_ring[ring_idx].next2comp,
					  rq->rx_ring[ring_idx].size);

		/* refill rx buffers frequently to avoid starving the h/w */
		num_to_alloc = vmxnet3_cmd_ring_desc_avail(rq->rx_ring +
							   ring_idx);
		if (unlikely(num_to_alloc > VMXNET3_RX_ALLOC_THRESHOLD(rq,
							ring_idx, adapter))) {
			vmxnet3_rq_alloc_rx_buf(rq, ring_idx, num_to_alloc,
						adapter);

			/* if needed, update the register */
			if (unlikely(rq->shared->updateRxProd)) {
				VMXNET3_WRITE_BAR0_REG(adapter,
					rxprod_reg[ring_idx] + rq->qid * 8,
					rq->rx_ring[ring_idx].next2fill);
				rq->uncommitted[ring_idx] = 0;
			}
		}

		vmxnet3_comp_ring_adv_next2proc(&rq->comp_ring);
		vmxnet3_getRxComp(rcd,
		     &rq->comp_ring.base[rq->comp_ring.next2proc].rcd, &rxComp);
	}

	return num_rxd;
}


/*
 * Unmap and free the rx buffers allocated to the rx queue. Other resources
 * are NOT freed. This is the counterpart of vmxnet3_rq_init()
 * Side-effects:
 *    1. indices and gen of each ring are reset to the initial value
 *    2. buf_info[] and buf_info2[] are cleared.
 */

static void
vmxnet3_rq_cleanup(struct vmxnet3_rx_queue *rq,
		   struct vmxnet3_adapter *adapter)
{
	u32 i, ring_idx;
	struct Vmxnet3_RxDesc *rxd;

	for (ring_idx = 0; ring_idx < 2; ring_idx++) {
		for (i = 0; i < rq->rx_ring[ring_idx].size; i++) {
#ifdef __BIG_ENDIAN_BITFIELD
			struct Vmxnet3_RxDesc rxDesc;
#endif
			vmxnet3_getRxDesc(rxd,
				&rq->rx_ring[ring_idx].base[i].rxd, &rxDesc);

			if (rxd->btype == VMXNET3_RXD_BTYPE_HEAD &&
					rq->buf_info[ring_idx][i].skb) {
				pci_unmap_single(adapter->pdev, rxd->addr,
						 rxd->len, PCI_DMA_FROMDEVICE);
				vmxnet3_dev_kfree_skb(adapter,
						rq->buf_info[ring_idx][i].skb);
				rq->buf_info[ring_idx][i].skb = NULL;
			} else if (rxd->btype == VMXNET3_RXD_BTYPE_BODY &&
					rq->buf_info[ring_idx][i].page) {
				pci_unmap_page(adapter->pdev, rxd->addr,
					       rxd->len, PCI_DMA_FROMDEVICE);
				vmxnet3_put_page(adapter,
						rq->buf_info[ring_idx][i].page);
				rq->buf_info[ring_idx][i].page = NULL;
			}
		}

		rq->rx_ring[ring_idx].gen = VMXNET3_INIT_GEN;
		rq->rx_ring[ring_idx].next2fill =
					rq->rx_ring[ring_idx].next2comp = 0;
		rq->uncommitted[ring_idx] = 0;
	}

	rq->comp_ring.gen = VMXNET3_INIT_GEN;
	rq->comp_ring.next2proc = 0;
}


/*
 *    Free rings and buf_info for the rx queue. The rx buffers must have
 *    ALREADY been freed. the .base fields of all rings will be set to NULL
 */

void
vmxnet3_rq_destroy(struct vmxnet3_rx_queue *rq,
		struct vmxnet3_adapter *adapter)
{
	int i;

#ifdef VMX86_DEBUG
	/* all rx buffers must have already been freed */
	{
		int j;

		for (i = 0; i < 2; i++) {
			if (rq->buf_info[i]) {
				for (j = 0; j < rq->rx_ring[i].size; j++) {
					BUG_ON(rq->buf_info[i][j].page != NULL);
				}
			}
		}
	}
#endif


	kfree(rq->buf_info[0]);

	for (i = 0; i < 2; i++) {
		if (rq->rx_ring[i].base) {
			pci_free_consistent(adapter->pdev, rq->rx_ring[i].size
					    * sizeof(struct Vmxnet3_RxDesc),
					    rq->rx_ring[i].base,
					    rq->rx_ring[i].basePA);
			rq->rx_ring[i].base = NULL;
		}
		rq->buf_info[i] = NULL;
	}

	if (rq->comp_ring.base) {
		pci_free_consistent(adapter->pdev, rq->comp_ring.size *
				    sizeof(struct Vmxnet3_RxCompDesc),
				    rq->comp_ring.base, rq->comp_ring.basePA);
		rq->comp_ring.base = NULL;
	}
}


/*
 *    initialize buf_info, allocate rx buffers and fill the rx rings. On
 *    failure, the rx buffers already allocated are NOT freed
 */

static int
vmxnet3_rq_init(struct vmxnet3_rx_queue *rq,
		struct vmxnet3_adapter  *adapter)
{
	int i;

	BUG_ON(adapter->rx_buf_per_pkt <= 0 ||
	       rq->rx_ring[0].size % adapter->rx_buf_per_pkt != 0);
	/* initialize buf_info */
	for (i = 0; i < rq->rx_ring[0].size; i++) {
		BUG_ON(rq->buf_info[0][i].skb != NULL);
		/* 1st buf for a pkt is skbuff */
		if (i % adapter->rx_buf_per_pkt == 0) {
			rq->buf_info[0][i].buf_type = VMXNET3_RX_BUF_SKB;
			rq->buf_info[0][i].len = adapter->skb_buf_size;
		} else { /* subsequent bufs for a pkt is frag */
			rq->buf_info[0][i].buf_type = VMXNET3_RX_BUF_PAGE;
			rq->buf_info[0][i].len = PAGE_SIZE;
		}
	}
	for (i = 0; i < rq->rx_ring[1].size; i++) {
		BUG_ON(rq->buf_info[1][i].page != NULL);
		rq->buf_info[1][i].buf_type = VMXNET3_RX_BUF_PAGE;
		rq->buf_info[1][i].len = PAGE_SIZE;
	}

	/* reset internal state and allocate buffers for both rings */
	for (i = 0; i < 2; i++) {
		rq->rx_ring[i].next2fill = rq->rx_ring[i].next2comp = 0;
		rq->uncommitted[i] = 0;

		memset(rq->rx_ring[i].base, 0, rq->rx_ring[i].size *
		       sizeof(struct Vmxnet3_RxDesc));
		rq->rx_ring[i].gen = VMXNET3_INIT_GEN;
	}
	if (vmxnet3_rq_alloc_rx_buf(rq, 0, rq->rx_ring[0].size - 1,
				    adapter) == 0) {
		/* at least has 1 rx buffer for the 1st ring */
		return -ENOMEM;
	}
	vmxnet3_rq_alloc_rx_buf(rq, 1, rq->rx_ring[1].size - 1, adapter);

	/* reset the comp ring */
	rq->comp_ring.next2proc = 0;
	memset(rq->comp_ring.base, 0, rq->comp_ring.size *
	       sizeof(struct Vmxnet3_RxCompDesc));
	rq->comp_ring.gen = VMXNET3_INIT_GEN;

	/* reset rxctx */
	rq->rx_ctx.skb = NULL;

	/* stats are not reset */
	return 0;
}


/*
 *    allocate and initialize two cmd rings and the completion ring for the
 *    given rx queue. Also allocate and initialize buf_info.
 *    rx buffers are NOT allocated
 */
static int
vmxnet3_rq_create(struct vmxnet3_rx_queue *rq, struct vmxnet3_adapter *adapter)
{
	int i;
	size_t sz;
	struct vmxnet3_rx_buf_info *bi;

	BUG_ON(rq->rx_ring[0].size % adapter->rx_buf_per_pkt != 0);
	for (i = 0; i < 2; i++) {
		BUG_ON((rq->rx_ring[i].size & VMXNET3_RING_SIZE_MASK) != 0);
		BUG_ON(rq->rx_ring[i].base != NULL);
		sz = rq->rx_ring[i].size * sizeof(struct Vmxnet3_RxDesc);
		rq->rx_ring[i].base = pci_alloc_consistent(adapter->pdev, sz,
							&rq->rx_ring[i].basePA);
		if (!rq->rx_ring[i].base) {
			printk(KERN_ERR "%s: failed to allocate rx ring %d\n",
			       adapter->netdev->name, i);
			goto err;
		}
	}

	sz = rq->comp_ring.size * sizeof(Vmxnet3_RxCompDesc);
	BUG_ON(rq->comp_ring.base != NULL);
	rq->comp_ring.base = pci_alloc_consistent(adapter->pdev,
			sz,
			&rq->comp_ring.basePA);
	if (!rq->comp_ring.base) {
		printk(KERN_ERR "%s: failed to allocate rx comp ring\n",
		       adapter->netdev->name);
		goto err;
	}

	BUG_ON(rq->buf_info[0] || rq->buf_info[1]);
	sz = sizeof(struct vmxnet3_rx_buf_info) * (rq->rx_ring[0].size +
						   rq->rx_ring[1].size);
	bi = kmalloc(sz, GFP_KERNEL);
	if (!bi) {
		printk(KERN_ERR "%s: failed to allocate rx bufinfo\n",
		       adapter->netdev->name);
		goto err;
	}
	memset(bi, 0, sz);
	rq->buf_info[0] = bi;
	rq->buf_info[1] = bi + rq->rx_ring[0].size;

	return 0;

err:
	vmxnet3_rq_destroy(rq, adapter);
	return -ENOMEM;
}

#ifdef VMXNET3_NAPI

static int
vmxnet3_do_poll(struct vmxnet3_adapter *adapter, int budget)
{
	if (unlikely(adapter->shared->ecr))
		vmxnet3_process_events(adapter);

	vmxnet3_tq_tx_complete(&adapter->tx_queue, adapter);
	return vmxnet3_rq_rx_complete(&adapter->rx_queue, adapter, budget);
}


#ifdef VMXNET3_NEW_NAPI

/*
 * New NAPI polling function. Returns # of the NAPI credit consumed (# of rx
 * descriptors processed)
 */
static int
vmxnet3_poll(struct napi_struct *napi, int budget)
{
	struct vmxnet3_adapter *adapter = container_of(napi,
					  struct vmxnet3_adapter, napi);
	int rxd_done;

	rxd_done = vmxnet3_do_poll(adapter, budget);

	if (rxd_done < budget) {
		compat_napi_complete(adapter->netdev, napi);
		vmxnet3_enable_intr(adapter, 0);
	}
	return rxd_done;
}
#else
/*
 * Result:
 *    0: napi is done
 *    1: continue polling
 */

static int
vmxnet3_poll(struct net_device *poll_dev, int *budget)
{
	int rxd_done, quota;
	struct vmxnet3_adapter *adapter = netdev_priv(poll_dev);

	quota = min(*budget, poll_dev->quota);

	rxd_done = vmxnet3_do_poll(adapter, quota);

	*budget -= rxd_done;
	poll_dev->quota -= rxd_done;

	if (rxd_done < quota) {
                compat_napi_complete(poll_dev, NULL);
		vmxnet3_enable_intr(adapter, 0);
		return 0;
	}

	return 1; /* not done */
}

#endif /* VMXNET3_NEW_NAPI  */
#endif /* VMXNET3_NAPI  */


/* Interrupt handler for vmxnet3  */
static compat_irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
vmxnet3_intr(int irq, void *dev_id, struct pt_regs * regs)
#else
vmxnet3_intr(int irq, void *dev_id)
#endif
{
	struct net_device *dev = dev_id;
	struct vmxnet3_adapter *adapter = netdev_priv(dev);

	if (unlikely(adapter->intr.type == VMXNET3_IT_INTX)) {
		u32 icr = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_ICR);
		if (unlikely(icr == 0))
			/* not ours */
			return IRQ_NONE;
	}

#ifdef VMXNET3_NAPI
	/* disable intr if needed */
	if (adapter->intr.mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_intr(adapter, 0);

	compat_napi_schedule(dev, &adapter->napi);

#else
	vmxnet3_tq_tx_complete(&adapter->tx_queue, adapter);
	vmxnet3_rq_rx_complete(&adapter->rx_queue, adapter);
	if (unlikely(adapter->shared->ecr)) {
		vmxnet3_process_events(adapter);
	}
	vmxnet3_enable_intr(adapter, 0);
#endif

	return COMPAT_IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER


static void
vmxnet3_netpoll(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	int irq;

#ifdef CONFIG_PCI_MSI
	if (adapter->intr.type == VMXNET3_IT_MSIX)
		irq = adapter->intr.msix_entries[0].vector;
	else
#endif
		irq = adapter->pdev->irq;

	disable_irq(irq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
	vmxnet3_intr(irq, netdev, NULL);
#else
	vmxnet3_intr(irq, netdev);
#endif
	enable_irq(irq);
}
#endif


/*
 * event_intr_idx and intr_idx for different comp rings get updated here.
 */

static int
vmxnet3_request_irqs(struct vmxnet3_adapter *adapter)
{
	int err;

#ifdef CONFIG_PCI_MSI
	if (adapter->intr.type == VMXNET3_IT_MSIX)
		/* we only use 1 MSI-X vector */
		err = request_irq(adapter->intr.msix_entries[0].vector,
				  vmxnet3_intr, 0, adapter->netdev->name,
				  adapter->netdev);
	else if (adapter->intr.type == VMXNET3_IT_MSI)
		err = request_irq(adapter->pdev->irq, vmxnet3_intr, 0,
				  adapter->netdev->name, adapter->netdev);
	else
#endif
		err = request_irq(adapter->pdev->irq, vmxnet3_intr,
				  COMPAT_IRQF_SHARED, adapter->netdev->name,
				  adapter->netdev);

	if (err)
		printk(KERN_ERR "Failed to request irq %s (intr type:%d), error"
		       ":%d\n", adapter->netdev->name, adapter->intr.type, err);


	if (!err) {
		int i;
		/* init our intr settings */
		for (i = 0; i < adapter->intr.num_intrs; i++)
			adapter->intr.mod_levels[i] = UPT1_IML_ADAPTIVE;

		/* next setup intr index for all intr sources */
		adapter->tx_queue.comp_ring.intr_idx = 0;
		adapter->rx_queue.comp_ring.intr_idx = 0;
		adapter->intr.event_intr_idx = 0;

		printk(KERN_INFO "%s: intr type %u, mode %u, %u vectors "
		       "allocated\n", adapter->netdev->name, adapter->intr.type,
		       adapter->intr.mask_mode, adapter->intr.num_intrs);
	}

	return err;
}


static void
vmxnet3_free_irqs(struct vmxnet3_adapter *adapter)
{
	BUG_ON(adapter->intr.type == VMXNET3_IT_AUTO ||
	       adapter->intr.num_intrs <= 0);

	switch (adapter->intr.type) {
#ifdef CONFIG_PCI_MSI
	case VMXNET3_IT_MSIX:
	{
		int i;

		for (i = 0; i < adapter->intr.num_intrs; i++)
			free_irq(adapter->intr.msix_entries[i].vector,
				 adapter->netdev);
		break;
	}
	case VMXNET3_IT_MSI:
		free_irq(adapter->pdev->irq, adapter->netdev);
		break;
#endif
	case VMXNET3_IT_INTX:
		free_irq(adapter->pdev->irq, adapter->netdev);
		break;
	default:
		BUG_ON(TRUE);
	}
}


static void
vmxnet3_vlan_rx_register(struct net_device *netdev, struct vlan_group *grp)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	struct Vmxnet3_DriverShared *shared = adapter->shared;
	u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;

	if (grp) {
		/* add vlan rx stripping. */
		if (adapter->netdev->features & NETIF_F_HW_VLAN_RX) {
			int i;
			struct Vmxnet3_DSDevRead *devRead = &shared->devRead;
			adapter->vlan_grp = grp;

			/* update FEATURES to device */
			set_flag_le64(&devRead->misc.uptFeatures,
				      UPT1_F_RXVLAN);
			VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
					       VMXNET3_CMD_UPDATE_FEATURE);
			/*
			 *  Clear entire vfTable; then enable untagged pkts.
			 *  Note: setting one entry in vfTable to non-zero turns
			 *  on VLAN rx filtering.
			 */
			for (i = 0; i < VMXNET3_VFT_SIZE; i++)
				vfTable[i] = 0;

			VMXNET3_SET_VFTABLE_ENTRY(vfTable, 0);
			VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
					       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
		} else {
			printk(KERN_ERR "%s: vlan_rx_register when device has "
			       "no NETIF_F_HW_VLAN_RX\n", netdev->name);
		}
	} else {
		/* remove vlan rx stripping. */
		struct Vmxnet3_DSDevRead *devRead = &shared->devRead;
		adapter->vlan_grp = NULL;

		if (le64_to_cpu(devRead->misc.uptFeatures) & UPT1_F_RXVLAN) {
			int i;

			for (i = 0; i < VMXNET3_VFT_SIZE; i++) {
				/* clear entire vfTable; this also disables
				 * VLAN rx filtering
				 */
				vfTable[i] = 0;
			}
			VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
					       VMXNET3_CMD_UPDATE_VLAN_FILTERS);

			/* update FEATURES to device */
			reset_flag_le64(&devRead->misc.uptFeatures,
					UPT1_F_RXVLAN);
			VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
					       VMXNET3_CMD_UPDATE_FEATURE);
		}
	}
}


static void
vmxnet3_restore_vlan(struct vmxnet3_adapter *adapter)
{
	if (adapter->vlan_grp) {
		u16 vid;
		u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;
		Bool activeVlan = FALSE;

		for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
			if (compat_vlan_group_get_device(adapter->vlan_grp,
							 vid)) {
				VMXNET3_SET_VFTABLE_ENTRY(vfTable, vid);
				activeVlan = TRUE;
			}
		}
		if (activeVlan) {
			/* continue to allow untagged pkts */
			VMXNET3_SET_VFTABLE_ENTRY(vfTable, 0);
		}
	}
}

/* Inherit net_device features from real device to VLAN device. */
void
vmxnet3_vlan_features(struct vmxnet3_adapter *adapter, u16 vid, Bool allvids)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	struct net_device *v_netdev;

	if (adapter->vlan_grp) {
		if (allvids) {
			for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
				v_netdev = compat_vlan_group_get_device(
							adapter->vlan_grp, vid);
				if (v_netdev) {
					v_netdev->features |=
						      adapter->netdev->features;
					compat_vlan_group_set_device(
					      adapter->vlan_grp, vid, v_netdev);
				}
			}
		} else {
			v_netdev = compat_vlan_group_get_device(
							adapter->vlan_grp, vid);
			if (v_netdev) {
				v_netdev->features |= adapter->netdev->features;
				compat_vlan_group_set_device(adapter->vlan_grp,
							     vid, v_netdev);
			}
		}
	}
#endif
}


static void
vmxnet3_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;

	vmxnet3_vlan_features(adapter, vid, FALSE);
	VMXNET3_SET_VFTABLE_ENTRY(vfTable, vid);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
}


static void
vmxnet3_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;

	VMXNET3_CLEAR_VFTABLE_ENTRY(vfTable, vid);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
}


/*
 * Allocate a buffer and copy into the mcast list. Returns NULL if the mcast
 * list exceeds the limit. Returns the addr of the allocated buffer or NULL.
 */

static u8 *
vmxnet3_copy_mc(struct net_device *netdev)
{
	u8 *buf = NULL;
	u32 sz = netdev->mc_count * ETH_ALEN;

	/* Vmxnet3_RxFilterConf.mfTableLen is u16. */
	if (sz <= 0xffff) {
		/* We may be called with BH disabled */
		buf = kmalloc(sz, GFP_ATOMIC);
		if (buf) {
			int i;
			struct dev_mc_list *mc = netdev->mc_list;

			for (i = 0; i < netdev->mc_count; i++) {
				BUG_ON(!mc);
				memcpy(buf + i * ETH_ALEN, mc->dmi_addr,
				       ETH_ALEN);
				mc = mc->next;
			}
		}
	}
	return buf;
}


static void
vmxnet3_set_mc(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	struct Vmxnet3_RxFilterConf *rxConf =
					&adapter->shared->devRead.rxFilterConf;
	u8 *new_table = NULL;
	u32 new_mode = VMXNET3_RXM_UCAST;

	if (netdev->flags & IFF_PROMISC)
		new_mode |= VMXNET3_RXM_PROMISC;

	if (netdev->flags & IFF_BROADCAST)
		new_mode |= VMXNET3_RXM_BCAST;

	if (netdev->flags & IFF_ALLMULTI)
		new_mode |= VMXNET3_RXM_ALL_MULTI;
	else if (netdev->mc_count > 0) {
		new_table = vmxnet3_copy_mc(netdev);
		if (new_table) {
			new_mode |= VMXNET3_RXM_MCAST;
			rxConf->mfTableLen = cpu_to_le16(
					netdev->mc_count * ETH_ALEN);
			rxConf->mfTablePA = cpu_to_le64(virt_to_phys(
					    new_table));
		} else {
			printk(KERN_INFO "%s: failed to copy mcast list, "
			       "setting ALL_MULTI\n", netdev->name);
			new_mode |= VMXNET3_RXM_ALL_MULTI;
		}
	}


	if (!(new_mode & VMXNET3_RXM_MCAST)) {
		rxConf->mfTableLen = 0;
		rxConf->mfTablePA = 0;
	}

	if (new_mode != rxConf->rxMode) {
		rxConf->rxMode = cpu_to_le32(new_mode);
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
				       VMXNET3_CMD_UPDATE_RX_MODE);
	}

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_MAC_FILTERS);

	kfree(new_table);
}


/*
 * Wipes out the whole driver_shared area and re-initializes it
 */

static void
vmxnet3_setup_driver_shared(struct vmxnet3_adapter *adapter)
{
	struct Vmxnet3_DriverShared *shared = adapter->shared;
	struct Vmxnet3_DSDevRead *devRead = &shared->devRead;
	struct Vmxnet3_TxQueueConf *tqc;
	struct Vmxnet3_RxQueueConf *rqc;
	int i;

	memset(shared, 0, sizeof(*shared));

	/* driver settings */
	shared->magic = cpu_to_le32(VMXNET3_REV1_MAGIC);
	devRead->misc.driverInfo.version = cpu_to_le32(
						VMXNET3_DRIVER_VERSION_NUM);
	devRead->misc.driverInfo.gos.gosBits = (sizeof(void *) == 4 ?
				VMXNET3_GOS_BITS_32 : VMXNET3_GOS_BITS_64);
	devRead->misc.driverInfo.gos.gosType = VMXNET3_GOS_TYPE_LINUX;
	*((u32 *)&devRead->misc.driverInfo.gos) = cpu_to_le32(
				*((u32 *)&devRead->misc.driverInfo.gos));
	devRead->misc.driverInfo.vmxnet3RevSpt = cpu_to_le32(1);
	devRead->misc.driverInfo.uptVerSpt = cpu_to_le32(1);

	devRead->misc.ddPA = cpu_to_le64(virt_to_phys(adapter));
	devRead->misc.ddLen = cpu_to_le32(sizeof(struct vmxnet3_adapter));

	/* set up feature flags */
	if (adapter->rxcsum)
		set_flag_le64(&devRead->misc.uptFeatures, UPT1_F_RXCSUM);

	if (adapter->lro) {
		set_flag_le64(&devRead->misc.uptFeatures, UPT1_F_LRO);
		devRead->misc.maxNumRxSG = cpu_to_le16(1 + MAX_SKB_FRAGS);
	}
	if ((adapter->netdev->features & NETIF_F_HW_VLAN_RX)
			&& adapter->vlan_grp) {
		set_flag_le64(&devRead->misc.uptFeatures, UPT1_F_RXVLAN);
	}

	devRead->misc.mtu = cpu_to_le32(adapter->netdev->mtu);
	devRead->misc.queueDescPA = cpu_to_le64(adapter->queue_desc_pa);
	devRead->misc.queueDescLen = cpu_to_le32(
				     sizeof(struct Vmxnet3_TxQueueDesc) +
				     sizeof(struct Vmxnet3_RxQueueDesc));

	/* tx queue settings */
	BUG_ON(adapter->tx_queue.tx_ring.base == NULL);

	devRead->misc.numTxQueues = 1;
	tqc = &adapter->tqd_start->conf;
	tqc->txRingBasePA   = cpu_to_le64(adapter->tx_queue.tx_ring.basePA);
	tqc->dataRingBasePA = cpu_to_le64(adapter->tx_queue.data_ring.basePA);
	tqc->compRingBasePA = cpu_to_le64(adapter->tx_queue.comp_ring.basePA);
	tqc->ddPA           = cpu_to_le64(virt_to_phys(
						adapter->tx_queue.buf_info));
	tqc->txRingSize     = cpu_to_le32(adapter->tx_queue.tx_ring.size);
	tqc->dataRingSize   = cpu_to_le32(adapter->tx_queue.data_ring.size);
	tqc->compRingSize   = cpu_to_le32(adapter->tx_queue.comp_ring.size);
	tqc->ddLen          = cpu_to_le32(sizeof(struct vmxnet3_tx_buf_info) *
			      tqc->txRingSize);
	tqc->intrIdx        = adapter->tx_queue.comp_ring.intr_idx;

	/* rx queue settings */
	devRead->misc.numRxQueues = 1;
	rqc = &adapter->rqd_start->conf;
	rqc->rxRingBasePA[0] = cpu_to_le64(adapter->rx_queue.rx_ring[0].basePA);
	rqc->rxRingBasePA[1] = cpu_to_le64(adapter->rx_queue.rx_ring[1].basePA);
	rqc->compRingBasePA  = cpu_to_le64(adapter->rx_queue.comp_ring.basePA);
	rqc->ddPA            = cpu_to_le64(virt_to_phys(
						adapter->rx_queue.buf_info));
	rqc->rxRingSize[0]   = cpu_to_le32(adapter->rx_queue.rx_ring[0].size);
	rqc->rxRingSize[1]   = cpu_to_le32(adapter->rx_queue.rx_ring[1].size);
	rqc->compRingSize    = cpu_to_le32(adapter->rx_queue.comp_ring.size);
	rqc->ddLen           = cpu_to_le32(sizeof(struct vmxnet3_rx_buf_info) *
			       (rqc->rxRingSize[0] + rqc->rxRingSize[1]));
	rqc->intrIdx         = adapter->rx_queue.comp_ring.intr_idx;

	/* intr settings */
	devRead->intrConf.autoMask = adapter->intr.mask_mode ==
				     VMXNET3_IMM_AUTO;
	devRead->intrConf.numIntrs = adapter->intr.num_intrs;
	for (i = 0; i < adapter->intr.num_intrs; i++)
		devRead->intrConf.modLevels[i] = adapter->intr.mod_levels[i];

	devRead->intrConf.eventIntrIdx = adapter->intr.event_intr_idx;

	/* rx filter settings */
	devRead->rxFilterConf.rxMode = 0;
	vmxnet3_restore_vlan(adapter);
	/* the rest are already zeroed */
}

/*
 * put the vNIC into an operational state. After this function finishes, the
 * adapter is fully functional. It does the following:
 * 1. initialize tq and rq
 * 2. fill rx rings with rx buffers
 * 3. setup intr
 * 4. setup driver_shared
 * 5. activate the dev
 * 6. signal the stack that the vNIC is ready to tx/rx
 * 7. enable intrs for the vNIC
 *
 * Returns:
 *    0 if the vNIC is in operation state
 *    error code if any intermediate step fails.
 */

int
vmxnet3_activate_dev(struct vmxnet3_adapter *adapter)
{
	int err;
	u32 ret;

	dev_dbg(&adapter->pdev->dev, "%s: skb_buf_size %d, rx_buf_per_pkt %d, "
		"ring sizes %u %u %u\n", adapter->netdev->name,
		adapter->skb_buf_size, adapter->rx_buf_per_pkt,
		adapter->tx_queue.tx_ring.size,
		adapter->rx_queue.rx_ring[0].size,
		adapter->rx_queue.rx_ring[1].size);

	vmxnet3_tq_init(&adapter->tx_queue, adapter);
	err = vmxnet3_rq_init(&adapter->rx_queue, adapter);
	if (err) {
		printk(KERN_ERR "Failed to init rx queue for %s: error %d\n",
		       adapter->netdev->name, err);
		goto rq_err;
	}

	err = vmxnet3_request_irqs(adapter);
	if (err) {
		printk(KERN_ERR "Failed to setup irq for %s: error %d\n",
		       adapter->netdev->name, err);
		goto irq_err;
	}

	vmxnet3_setup_driver_shared(adapter);

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAL,VMXNET3_GET_ADDR_LO(
			       adapter->shared_pa));
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAH, VMXNET3_GET_ADDR_HI(
			       adapter->shared_pa));

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_ACTIVATE_DEV);
	ret = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_CMD);
	if (ret != 0) {
		printk(KERN_ERR "Failed to activate dev %s: error %u\n",
		       adapter->netdev->name, ret);
		err = -EINVAL;
		goto activate_err;
	}
	VMXNET3_WRITE_BAR0_REG(adapter, VMXNET3_REG_RXPROD,
			       adapter->rx_queue.rx_ring[0].next2fill);
	VMXNET3_WRITE_BAR0_REG(adapter, VMXNET3_REG_RXPROD2,
			       adapter->rx_queue.rx_ring[1].next2fill);

	/* Apply the rx filter settins last. */
	vmxnet3_set_mc(adapter->netdev);

	/*
	 * Check link state when first activating device. It will start the
	 * tx queue if the link is up.
	 */
	vmxnet3_check_link(adapter);
#ifdef VMXNET3_NAPI
	compat_napi_enable(adapter->netdev, &adapter->napi);
#endif
	vmxnet3_enable_all_intrs(adapter);
	clear_bit(VMXNET3_STATE_BIT_QUIESCED, &adapter->state);
	return 0;

activate_err:
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAL, 0);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAH, 0);
	vmxnet3_free_irqs(adapter);
irq_err:
rq_err:
	/* free up buffers we allocated */
	vmxnet3_rq_cleanup(&adapter->rx_queue, adapter);
	return err;
}


void
vmxnet3_reset_dev(struct vmxnet3_adapter *adapter)
{
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD, VMXNET3_CMD_RESET_DEV);
}


/*
 * Stop the device. After this function returns, the adapter stop pkt tx/rx
 * and won't generate intrs. The stack won't try to xmit pkts through us,
 * nor will it poll us for pkts. It does the following:
 *
 * 1. ask the vNIC to quiesce
 * 2. disable the vNIC from generating intrs
 * 3. free intr
 * 4. stop the stack from xmiting pkts thru us and polling
 * 5. free rx buffers
 * 6. tx complete pkts pending
 *
 */

int
vmxnet3_quiesce_dev(struct vmxnet3_adapter *adapter)
{
	if (test_and_set_bit(VMXNET3_STATE_BIT_QUIESCED, &adapter->state))
		return 0;


	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_QUIESCE_DEV);
	vmxnet3_disable_all_intrs(adapter);
#ifdef VMXNET3_NAPI
	compat_napi_disable(adapter->netdev, &adapter->napi);
#endif
	netif_tx_disable(adapter->netdev);
	adapter->link_speed = 0;
	netif_carrier_off(adapter->netdev);

	vmxnet3_tq_cleanup(&adapter->tx_queue, adapter);
	vmxnet3_rq_cleanup(&adapter->rx_queue, adapter);
	vmxnet3_free_irqs(adapter);
	return 0;
}


static void
vmxnet3_write_mac_addr(struct vmxnet3_adapter *adapter, u8 *mac)
{
	u32 tmp;

	tmp = *(u32 *)mac;
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_MACL, tmp);

	tmp = (mac[5] << 8) | mac[4];
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_MACH, tmp);
}


static int
vmxnet3_set_mac_addr(struct net_device *netdev, void *p)
{
	struct sockaddr *addr = p;
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	vmxnet3_write_mac_addr(adapter, netdev->dev_addr);

	return 0;
}


/* ==================== initialization and cleanup routines ============ */

static int
vmxnet3_alloc_pci_resources(struct vmxnet3_adapter *adapter, Bool *dma64)
{
	int err;
	unsigned long mmio_start, mmio_len;
	struct pci_dev *pdev = adapter->pdev;

	err = compat_pci_enable_device(pdev);
	if (err) {
		printk(KERN_ERR "Failed to enable adapter %s: error %d\n",
		       compat_pci_name(pdev), err);
		return err;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 6)
	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) == 0) {
		if (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64)) != 0) {
			printk(KERN_ERR "pci_set_consistent_dma_mask failed "
			       "for adapter %s\n", compat_pci_name(pdev));
			err = -EIO;
			goto err_set_mask;
		}
		*dma64 = TRUE;
	} else {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0) {
			printk(KERN_ERR "pci_set_dma_mask failed for adapter "
			       "%s\n",	compat_pci_name(pdev));
			err = -EIO;
			goto err_set_mask;
		}
		*dma64 = FALSE;
	}
#else
	*dma64 = TRUE;
#endif

	err = compat_pci_request_regions(pdev, vmxnet3_driver_name);
	if (err) {
		printk(KERN_ERR "Failed to request region for adapter %s: "
		       "error %d\n", compat_pci_name(pdev), err);
		goto err_set_mask;
	}

	compat_pci_set_master(pdev);

	mmio_start = compat_pci_resource_start(pdev, 0);
	mmio_len = compat_pci_resource_len(pdev, 0);
	adapter->hw_addr0 = ioremap(mmio_start, mmio_len);
	if (!adapter->hw_addr0) {
		printk(KERN_ERR "Failed to map bar0 for adapter %s\n",
		       compat_pci_name(pdev));
		err = -EIO;
		goto err_ioremap;
	}

	mmio_start = compat_pci_resource_start(pdev, 1);
	mmio_len = compat_pci_resource_len(pdev, 1);
	adapter->hw_addr1 = ioremap(mmio_start, mmio_len);
	if (!adapter->hw_addr1) {
		printk(KERN_ERR "Failed to map bar1 for adapter %s\n",
		       compat_pci_name(pdev));
		err = -EIO;
		goto err_bar1;
	}
	return 0;

err_bar1:
	iounmap(adapter->hw_addr0);
err_ioremap:
	compat_pci_release_regions(pdev);
err_set_mask:
	compat_pci_disable_device(pdev);
	return err;
}


static void
vmxnet3_free_pci_resources(struct vmxnet3_adapter *adapter)
{
	BUG_ON(!adapter->pdev);

	iounmap(adapter->hw_addr0);
	iounmap(adapter->hw_addr1);
	compat_pci_release_regions(adapter->pdev);
	compat_pci_disable_device(adapter->pdev);
}



/*
 * Calculate the # of buffers for a pkt based on mtu, then adjust the size of
 * the 1st rx ring accordingly
 */

static void
vmxnet3_adjust_rx_ring_size(struct vmxnet3_adapter *adapter)
{
	size_t sz;

	if (adapter->netdev->mtu <= VMXNET3_MAX_SKB_BUF_SIZE -
				    VMXNET3_MAX_ETH_HDR_SIZE) {
		adapter->skb_buf_size = adapter->netdev->mtu +
					VMXNET3_MAX_ETH_HDR_SIZE;
		if (adapter->skb_buf_size < VMXNET3_MIN_T0_BUF_SIZE)
			adapter->skb_buf_size = VMXNET3_MIN_T0_BUF_SIZE;

		adapter->rx_buf_per_pkt = 1;
	} else {
		adapter->skb_buf_size = VMXNET3_MAX_SKB_BUF_SIZE;
		sz = adapter->netdev->mtu - VMXNET3_MAX_SKB_BUF_SIZE +
					    VMXNET3_MAX_ETH_HDR_SIZE;
		adapter->rx_buf_per_pkt = 1 + (sz + PAGE_SIZE - 1) / PAGE_SIZE;
	}

	if (adapter->is_shm) {
		adapter->skb_buf_size = PAGE_SIZE;
	}

	/*
	 * for simplicity, force the ring0 size to be a multiple of
	 * rx_buf_per_pkt * VMXNET3_RING_SIZE_ALIGN
	 */
	sz = adapter->rx_buf_per_pkt * VMXNET3_RING_SIZE_ALIGN;
	adapter->rx_queue.rx_ring[0].size = (adapter->rx_queue.rx_ring[0].size +
					     sz - 1) / sz * sz;
	adapter->rx_queue.rx_ring[0].size = min_t(u32,
					    adapter->rx_queue.rx_ring[0].size,
					    VMXNET3_RX_RING_MAX_SIZE / sz * sz);
}

/* Create the specified number of tx queues and rx queues. On failure, it
 * destroys the queues created. */
int
vmxnet3_create_queues(struct vmxnet3_adapter *adapter, u32 tx_ring_size,
		      u32 rx_ring_size, u32 rx_ring2_size)
{
	int err;

	adapter->tx_queue.tx_ring.size   = tx_ring_size;
	adapter->tx_queue.data_ring.size = tx_ring_size;
	adapter->tx_queue.comp_ring.size = tx_ring_size;
	adapter->tx_queue.shared = &adapter->tqd_start->ctrl;
	adapter->tx_queue.stopped = TRUE;
	err = vmxnet3_tq_create(&adapter->tx_queue, adapter);
	if (err)
		return err;

	adapter->rx_queue.rx_ring[0].size = rx_ring_size;
	adapter->rx_queue.rx_ring[1].size = rx_ring2_size;
	vmxnet3_adjust_rx_ring_size(adapter);
	adapter->rx_queue.comp_ring.size  = adapter->rx_queue.rx_ring[0].size +
					    adapter->rx_queue.rx_ring[1].size;
	adapter->rx_queue.qid  = 0;
	adapter->rx_queue.qid2 = 1;
	adapter->rx_queue.shared = &adapter->rqd_start->ctrl;
	err = vmxnet3_rq_create(&adapter->rx_queue, adapter);
	if (err)
		vmxnet3_tq_destroy(&adapter->tx_queue, adapter);

	return err;
}

/*
 * setup rings, allocate necessary resources, request for IRQs, configure
 * the device. The device is functional after this function finishes
 * successfully.
 * Returns 0 on success, negative errno value on failure
 */

static int
vmxnet3_open(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter;
	int err;

	adapter = compat_netdev_priv(netdev);

	spin_lock_init(&adapter->tx_queue.tx_lock);

	if (adapter->is_shm) {
		printk(KERN_INFO "bringing up shared memory vmxnet3 %s\n",
		       netdev->name);
		err = vmxnet3_shm_open(adapter, netdev->name, shm_pool_size);
		if (err) {
			goto shm_err;
		}
	}
	err = vmxnet3_create_queues(adapter, VMXNET3_DEF_TX_RING_SIZE,
				    VMXNET3_DEF_RX_RING_SIZE,
				    VMXNET3_DEF_RX_RING_SIZE);
	if (err)
		goto queue_err;

	err = vmxnet3_activate_dev(adapter);
	if (err)
		goto activate_err;

	COMPAT_NETDEV_MOD_INC_USE_COUNT;

	return 0;

activate_err:
	vmxnet3_rq_destroy(&adapter->rx_queue, adapter);
	vmxnet3_tq_destroy(&adapter->tx_queue, adapter);
queue_err:
	if (adapter->is_shm) {
		vmxnet3_shm_close(adapter);
	}
shm_err:
	return err;
}


static int
vmxnet3_close(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);

	/*
	 * Reset_work may be in the middle of resetting the device, wait for its
	 * completion.
	 */
	while (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		compat_msleep(1);

	vmxnet3_quiesce_dev(adapter);

	if (adapter->is_shm) {
		vmxnet3_shm_close(adapter);
	}

	vmxnet3_rq_destroy(&adapter->rx_queue, adapter);
	vmxnet3_tq_destroy(&adapter->tx_queue, adapter);

	COMPAT_NETDEV_MOD_DEC_USE_COUNT;

	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);


	return 0;
}


/*
 * Called to forcibly close the device when the driver failed to re-activate it.
 */
void
vmxnet3_force_close(struct vmxnet3_adapter *adapter)
{
	/*
	 * we must clear VMXNET3_STATE_BIT_RESETTING, otherwise
	 * vmxnet3_close() will deadlock.
	 */
	BUG_ON(test_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state));

#ifdef VMXNET3_NAPI
	/* we need to enable NAPI, otherwise dev_close will deadlock */
	compat_napi_enable(adapter->netdev, &adapter->napi);
#endif
	dev_close(adapter->netdev);
}


static int
vmxnet3_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	int err = 0;

	if (new_mtu < VMXNET3_MIN_MTU || new_mtu > VMXNET3_MAX_MTU)
		return -EINVAL;

	if (new_mtu > 1500 && !adapter->jumbo_frame)
		return -EINVAL;

	netdev->mtu = new_mtu;

	/*
	 * Reset_work may be in the middle of resetting the device, wait for its
	 * completion.
	 */
	while (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		compat_msleep(1);

	if (compat_netif_running(netdev)) {
		vmxnet3_quiesce_dev(adapter);
		vmxnet3_reset_dev(adapter);

		/* we need to re-create the rx queue based on the new mtu */
		vmxnet3_rq_destroy(&adapter->rx_queue, adapter);
		vmxnet3_adjust_rx_ring_size(adapter);
		adapter->rx_queue.comp_ring.size  =
					adapter->rx_queue.rx_ring[0].size +
					adapter->rx_queue.rx_ring[1].size;
		err = vmxnet3_rq_create(&adapter->rx_queue, adapter);
		if (err) {
			printk(KERN_ERR "%s: failed to re-create rx queue,"
				" error %d. Closing it.\n", netdev->name, err);
			goto out;
		}

		err = vmxnet3_activate_dev(adapter);
		if (err) {
			printk(KERN_ERR "%s: failed to re-activate, error %d. "
				"Closing it\n", netdev->name, err);
			goto out;
		}
	}

out:
	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);
	if (err)
		vmxnet3_force_close(adapter);

	return err;
}


static void
vmxnet3_declare_features(struct vmxnet3_adapter *adapter, Bool dma64)
{
	struct net_device *netdev = adapter->netdev;

	netdev->features = NETIF_F_SG |
		NETIF_F_HW_CSUM |
		NETIF_F_HW_VLAN_TX |
		NETIF_F_HW_VLAN_RX |
		NETIF_F_HW_VLAN_FILTER |
		NETIF_F_TSO;
	printk(KERN_INFO "features: sg csum vlan jf tso");

	adapter->rxcsum = TRUE;
	adapter->jumbo_frame = TRUE;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	 /* LRO feature control by module param */
	if (!disable_lro) {
#else
	 /* LRO feature control by ethtool */
		netdev->features |= NETIF_F_LRO;
#endif

		adapter->lro = TRUE;
		printk(" lro");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	}
#endif

#ifdef NETIF_F_TSO6
	netdev->features |= NETIF_F_TSO6;
	printk(" tsoIPv6");
#endif

	if (dma64) {
		netdev->features |= NETIF_F_HIGHDMA;
		printk(" highDMA");
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
	netdev->vlan_features = netdev->features;
#endif
	printk("\n");
}


static void
vmxnet3_read_mac_addr(struct vmxnet3_adapter *adapter, u8 *mac)
{
	u32 tmp;

	tmp = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_MACL);
	*(u32 *)mac = tmp;

	tmp = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_MACH);
	mac[4] = tmp & 0xff;
	mac[5] = (tmp >> 8) & 0xff;
}



/*
 * read the intr configuration, pick the intr type, and enable MSI/MSI-X if
 * needed. adapter->intr.{type, mask_mode, num_intr} are modified
 */

static void
vmxnet3_alloc_intr_resources(struct vmxnet3_adapter *adapter)
{
	u32 cfg;

	/* intr settings */
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_GET_CONF_INTR);
	cfg = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_CMD);
	adapter->intr.type = cfg & 0x3;
	adapter->intr.mask_mode = (cfg >> 2) & 0x3;

#ifdef CONFIG_PCI_MSI
	if (adapter->intr.type == VMXNET3_IT_AUTO) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
		/* start with MSI-X */
		adapter->intr.type = VMXNET3_IT_MSIX;
		adapter->intr.msix_entries[0].entry = 0;
		if (!pci_enable_msix(adapter->pdev, adapter->intr.msix_entries,
				     VMXNET3_LINUX_MAX_MSIX_VECT)) {
			adapter->intr.num_intrs = 1;
			return;
		}
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
		adapter->intr.type = VMXNET3_IT_MSI;
		if (!pci_enable_msi(adapter->pdev)) {
			adapter->intr.num_intrs = 1;
			return;
		}
#endif
	}
#endif
	adapter->intr.type = VMXNET3_IT_INTX;

	/* INT-X related setting */
	adapter->intr.num_intrs = 1;
}


static void
vmxnet3_free_intr_resources(struct vmxnet3_adapter *adapter)
{
#ifdef CONFIG_PCI_MSI
	if (adapter->intr.type == VMXNET3_IT_MSIX)
		pci_disable_msix(adapter->pdev);
	else if (adapter->intr.type == VMXNET3_IT_MSI)
		pci_disable_msi(adapter->pdev);
	else
#endif
	{
		BUG_ON(adapter->intr.type != VMXNET3_IT_INTX);
	}

}


/*
 * Called when the stack detects a Tx hang. Schedule a job to reset the device
 */

static void
vmxnet3_tx_timeout(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	adapter->tx_timeout_count++;

	printk(KERN_ERR "%s: tx hang\n", adapter->netdev->name);
	compat_schedule_work(&adapter->work);
}


static void
vmxnet3_reset_work(compat_work_arg data)
{
	struct vmxnet3_adapter *adapter;

	adapter = COMPAT_WORK_GET_DATA(data, struct vmxnet3_adapter, work);

	/* if another thread is resetting the device, no need to proceed */
	if (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		return;

	/* if the device is closed, we must leave it alone */
	if (netif_running(adapter->netdev)) {
		printk(KERN_INFO "%s: resetting\n", adapter->netdev->name);

		vmxnet3_quiesce_dev(adapter);
		vmxnet3_reset_dev(adapter);
		vmxnet3_activate_dev(adapter);
	} else {
		printk(KERN_INFO "%s: already closed\n", adapter->netdev->name);
	}

	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);
}


/*
 * Initialize a vmxnet3 device. Returns 0 on success, negative errno code
 * otherwise. Initialize the h/w and allocate necessary resources
 */

static int __devinit
vmxnet3_probe_device(struct pci_dev *pdev,
		     const struct pci_device_id *id)
{
#ifdef HAVE_NET_DEVICE_OPS
	static const struct net_device_ops vmxnet3_netdev_ops = {
		.ndo_open = vmxnet3_open,
		.ndo_stop = vmxnet3_close,
		.ndo_start_xmit = vmxnet3_xmit_frame,
		.ndo_set_mac_address = vmxnet3_set_mac_addr,
		.ndo_change_mtu = vmxnet3_change_mtu,
		.ndo_get_stats = vmxnet3_get_stats,
		.ndo_tx_timeout = vmxnet3_tx_timeout,
		.ndo_set_multicast_list = vmxnet3_set_mc,
		.ndo_vlan_rx_register = vmxnet3_vlan_rx_register,
		.ndo_vlan_rx_add_vid = vmxnet3_vlan_rx_add_vid,
		.ndo_vlan_rx_kill_vid = vmxnet3_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
		.ndo_poll_controller = vmxnet3_netpoll,
#endif
	};
#endif	/* HAVE_NET_DEVICE_OPS  */
	int err;
	Bool dma64 = FALSE; /* stupid gcc */
	u32 ver;
	struct net_device *netdev;
	struct vmxnet3_adapter *adapter;
	u8 mac[ETH_ALEN];

	netdev = compat_alloc_etherdev(sizeof(struct vmxnet3_adapter));
	if (!netdev) {
		printk(KERN_ERR "Failed to alloc ethernet device %s\n",
				compat_pci_name(pdev));
		return -ENOMEM;
	}

	pci_set_drvdata(pdev, netdev);
	adapter = compat_netdev_priv(netdev);
	adapter->netdev = netdev;
	adapter->pdev = pdev;

	adapter->shared = pci_alloc_consistent(adapter->pdev,
			  sizeof(struct Vmxnet3_DriverShared),
			  &adapter->shared_pa);
	if (!adapter->shared) {
		printk(KERN_ERR "Failed to allocate memory for %s\n",
			compat_pci_name(pdev));
		err = -ENOMEM;
		goto err_alloc_shared;
	}

	adapter->tqd_start = pci_alloc_consistent(adapter->pdev,
			     sizeof(struct Vmxnet3_TxQueueDesc) +
			     sizeof(struct Vmxnet3_RxQueueDesc),
			     &adapter->queue_desc_pa);

	if (!adapter->tqd_start) {
		printk(KERN_ERR "Failed to allocate memory for %s\n",
			compat_pci_name(pdev));
		err = -ENOMEM;
		goto err_alloc_queue_desc;
	}
	adapter->rqd_start = (struct Vmxnet3_RxQueueDesc *)(adapter->tqd_start
							    + 1);

	adapter->pm_conf = kmalloc(sizeof(struct Vmxnet3_PMConf), GFP_KERNEL);
	if (adapter->pm_conf == NULL) {
		printk(KERN_ERR "Failed to allocate memory for %s\n",
			compat_pci_name(pdev));
		err = -ENOMEM;
		goto err_alloc_pm;
	}

	err = vmxnet3_alloc_pci_resources(adapter, &dma64);
	if (err < 0)
		goto err_alloc_pci;

	ver = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_VRRS);
	if (ver & 1) {
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_VRRS, 1);
	} else {
		printk(KERN_ERR "Incompatible h/w version (0x%x) for adapter"
		       " %s\n",	ver, compat_pci_name(pdev));
		err = -EBUSY;
		goto err_ver;
	}

	ver = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_UVRS);
	if (ver & 1) {
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_UVRS, 1);
	} else {
		printk(KERN_ERR "Incompatible upt version (0x%x) for "
		       "adapter %s\n", ver, compat_pci_name(pdev));
		err = -EBUSY;
		goto err_ver;
	}

	vmxnet3_declare_features(adapter, dma64);

	adapter->dev_number = atomic_read(&devices_found);
	adapter->is_shm = FALSE;
	if (adapter->dev_number < VMXNET3_SHM_MAX_DEVICES) {
		if (enable_shm[adapter->dev_number] == 1) {
			if (!correct_shm_disclaimer) {
				printk(KERN_ERR "Did not activate shm, "
				       "disclaimer missing\n");
			} else {
				adapter->is_shm = TRUE;
			}
		}
	}

	vmxnet3_alloc_intr_resources(adapter);

	vmxnet3_read_mac_addr(adapter, mac);
	memcpy(netdev->dev_addr,  mac, netdev->addr_len);

#ifdef HAVE_NET_DEVICE_OPS
	netdev->netdev_ops = &vmxnet3_netdev_ops;
#else
	netdev->open  = vmxnet3_open;
	netdev->stop  = vmxnet3_close;
	netdev->hard_start_xmit = vmxnet3_xmit_frame;
	netdev->set_mac_address = vmxnet3_set_mac_addr;
	netdev->change_mtu = vmxnet3_change_mtu;
	netdev->get_stats = vmxnet3_get_stats;
	netdev->tx_timeout = vmxnet3_tx_timeout;
	netdev->set_multicast_list = vmxnet3_set_mc;
	netdev->vlan_rx_register = vmxnet3_vlan_rx_register;
	netdev->vlan_rx_add_vid  = vmxnet3_vlan_rx_add_vid;
	netdev->vlan_rx_kill_vid = vmxnet3_vlan_rx_kill_vid;
#ifdef CONFIG_NET_POLL_CONTROLLER
	netdev->poll_controller  = vmxnet3_netpoll;
#endif
#endif /* HAVE_NET_DEVICE_OPS  */
	netdev->watchdog_timeo = 5 * HZ;
	vmxnet3_set_ethtool_ops(netdev);

	COMPAT_INIT_WORK(&adapter->work, vmxnet3_reset_work, adapter);

#ifdef VMXNET3_NAPI
	compat_netif_napi_add(netdev, &adapter->napi, vmxnet3_poll, 64);
#endif

	COMPAT_SET_MODULE_OWNER(netdev);
	COMPAT_SET_NETDEV_DEV(netdev, &pdev->dev);

	err = register_netdev(netdev);
	if (err) {
		printk(KERN_ERR "Failed to register adapter %s\n",
		       compat_pci_name(pdev));
		goto err_register;
	}

	set_bit(VMXNET3_STATE_BIT_QUIESCED, &adapter->state);
	atomic_inc(&devices_found);
	return 0;

err_register:
	vmxnet3_free_intr_resources(adapter);
err_ver:
	vmxnet3_free_pci_resources(adapter);
err_alloc_pci:
	kfree(adapter->pm_conf);
err_alloc_pm:
	pci_free_consistent(adapter->pdev, sizeof(struct Vmxnet3_TxQueueDesc) +
			    sizeof(struct Vmxnet3_RxQueueDesc),
			    adapter->tqd_start, adapter->queue_desc_pa);
err_alloc_queue_desc:
	pci_free_consistent(adapter->pdev, sizeof(struct Vmxnet3_DriverShared),
			    adapter->shared, adapter->shared_pa);
err_alloc_shared:
	pci_set_drvdata(pdev, NULL);
	compat_free_netdev(netdev);
	return err;
}


static void __devexit
vmxnet3_remove_device(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);

	flush_scheduled_work();

	unregister_netdev(netdev);

	vmxnet3_free_intr_resources(adapter);
	vmxnet3_free_pci_resources(adapter);
	kfree(adapter->pm_conf);
	pci_free_consistent(adapter->pdev, sizeof(struct Vmxnet3_TxQueueDesc) +
			    sizeof(struct Vmxnet3_RxQueueDesc),
			    adapter->tqd_start, adapter->queue_desc_pa);
	pci_free_consistent(adapter->pdev, sizeof(struct Vmxnet3_DriverShared),
			    adapter->shared, adapter->shared_pa);
	compat_free_netdev(netdev);
}


#ifdef CONFIG_PM

/*
 *      May programs the wake-up filters if configured to do so.
 */

static int
vmxnet3_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	struct Vmxnet3_PMConf *pmConf;
	struct ethhdr *ehdr;
	struct arphdr *ahdr;
	u8 *arpreq;
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	int i = 0;

	if (!compat_netif_running(netdev))
		return 0;

	vmxnet3_disable_all_intrs(adapter);
	vmxnet3_free_irqs(adapter);
	vmxnet3_free_intr_resources(adapter);
	netif_device_detach(netdev);
	netif_stop_queue(netdev);

	/* Create wake-up filters. */
	pmConf = adapter->pm_conf;
	memset(pmConf, 0, sizeof(*pmConf));

	if (adapter->wol & WAKE_UCAST) {
		pmConf->filters[i].patternSize = ETH_ALEN;
		pmConf->filters[i].maskSize = 1;
		memcpy(pmConf->filters[i].pattern, netdev->dev_addr, ETH_ALEN);
		pmConf->filters[i].mask[0] = 0x3F; /* LSB ETH_ALEN bits */

		set_flag_le16(&pmConf->wakeUpEvents, VMXNET3_PM_WAKEUP_FILTER);
		i++;
	}

	if (adapter->wol & WAKE_ARP) {
		in_dev = in_dev_get(netdev);
		if (!in_dev)
			goto skip_arp;

		ifa = (struct in_ifaddr *)in_dev->ifa_list;
		if (!ifa) {
			dev_dbg(&adapter->pdev->dev, "Cannot program WoL ARP"
				" filter for %s: no IPv4 address.\n",
				netdev->name);
			in_dev_put(in_dev);
			goto skip_arp;
		}
		pmConf->filters[i].patternSize = ETH_HLEN + /* Ethernet header */
			sizeof(struct arphdr) +                  /* ARP header */
			2 * ETH_ALEN +                           /* 2 Ethernet addresses */
			2 * sizeof (u32);                     /* 2 IPv4 addresses */
		pmConf->filters[i].maskSize =
			(pmConf->filters[i].patternSize - 1) / 8 + 1;
		/* ETH_P_ARP in Ethernet header. */
		ehdr = (struct ethhdr *)pmConf->filters[i].pattern;
		ehdr->h_proto = htons(ETH_P_ARP);
		/* ARPOP_REQUEST in ARP header. */
		ahdr = (struct arphdr *)&pmConf->filters[i].pattern[ETH_HLEN];
		ahdr->ar_op = htons(ARPOP_REQUEST);
		arpreq = (u8 *)(ahdr + 1);

		/* The Unicast IPv4 address in 'tip' field. */
		arpreq += 2 * ETH_ALEN + sizeof(u32);
		*(u32 *)arpreq = ifa->ifa_address;

		/* The mask for the relevant bits. */
		pmConf->filters[i].mask[0] = 0x00;
		pmConf->filters[i].mask[1] = 0x30; /* ETH_P_ARP */
		pmConf->filters[i].mask[2] = 0x30; /* ARPOP_REQUEST */
		pmConf->filters[i].mask[3] = 0x00;
		pmConf->filters[i].mask[4] = 0xC0; /* IPv4 TIP */
		pmConf->filters[i].mask[5] = 0x03; /* IPv4 TIP */
		in_dev_put(in_dev);

		set_flag_le16(&pmConf->wakeUpEvents, VMXNET3_PM_WAKEUP_FILTER);
		i++;
	}

skip_arp:
	if (adapter->wol & WAKE_MAGIC)
		set_flag_le16(&pmConf->wakeUpEvents, VMXNET3_PM_WAKEUP_MAGIC);

	pmConf->numFilters = i;

	adapter->shared->devRead.pmConfDesc.confVer = cpu_to_le32(1);
	adapter->shared->devRead.pmConfDesc.confLen = cpu_to_le32(sizeof(
								  *pmConf));
	adapter->shared->devRead.pmConfDesc.confPA = cpu_to_le64(virt_to_phys(
								 pmConf));

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_PMCFG);

	compat_pci_save_state(pdev);
	pci_enable_wake(pdev, compat_pci_choose_state(pdev, state),
			adapter->wol);
	compat_pci_disable_device(pdev);
	pci_set_power_state(pdev, compat_pci_choose_state(pdev, state));

	return 0;
}


static int
vmxnet3_resume(struct pci_dev *pdev)
{
	int err;
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct vmxnet3_adapter *adapter = compat_netdev_priv(netdev);
	struct Vmxnet3_PMConf *pmConf;

	if (!compat_netif_running(netdev))
		return 0;

	/* Destroy wake-up filters. */
	pmConf = adapter->pm_conf;
	memset(pmConf, 0, sizeof(*pmConf));

	adapter->shared->devRead.pmConfDesc.confVer = cpu_to_le32(1);
	adapter->shared->devRead.pmConfDesc.confLen = cpu_to_le32(sizeof(
								  *pmConf));
	adapter->shared->devRead.pmConfDesc.confPA = cpu_to_le32(virt_to_phys(
								 pmConf));

	netif_device_attach(netdev);
	pci_set_power_state(pdev, PCI_D0);
	compat_pci_restore_state(pdev);
	err = compat_pci_enable_device(pdev);
	if (err != 0)
		return err;

	pci_enable_wake(pdev, PCI_D0, 0);

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_PMCFG);
	vmxnet3_alloc_intr_resources(adapter);
	vmxnet3_request_irqs(adapter);
	vmxnet3_enable_all_intrs(adapter);

	return 0;
}

#endif

static struct pci_driver vmxnet3_driver = {
	.name		= vmxnet3_driver_name,
	.id_table	= vmxnet3_pciid_table,
	.probe		= vmxnet3_probe_device,
	.remove		= __devexit_p(vmxnet3_remove_device),
#ifdef CONFIG_PM
	.suspend	= vmxnet3_suspend,
	.resume		= vmxnet3_resume,
#endif
};


static int __init
vmxnet3_init_module(void)
{
	printk(KERN_INFO "%s - version %s\n", VMXNET3_DRIVER_DESC,
	       VMXNET3_DRIVER_VERSION_REPORT);

	correct_shm_disclaimer = shm_disclaimer &&
		(strncmp(shm_disclaimer, VMXNET3_SHM_DISCLAIMER,
		strlen(VMXNET3_SHM_DISCLAIMER)) == 0);

#ifdef CONFIG_COMPAT
#ifndef HAVE_UNLOCKED_IOCTL
	if (correct_shm_disclaimer) {
		register_ioctl32_conversion(SHM_IOCTL_TX, NULL);
		register_ioctl32_conversion(SHM_IOCTL_ALLOC_ONE, NULL);
		register_ioctl32_conversion(SHM_IOCTL_ALLOC_MANY, NULL);
		register_ioctl32_conversion(SHM_IOCTL_ALLOC_ONE_AND_MANY, NULL);
		register_ioctl32_conversion(SHM_IOCTL_FREE_ONE, NULL);
	}
#endif
#endif
	return pci_register_driver(&vmxnet3_driver);
}

module_init(vmxnet3_init_module);

static void
vmxnet3_exit_module(void)
{
#ifdef CONFIG_COMPAT
#ifndef HAVE_UNLOCKED_IOCTL
	if (correct_shm_disclaimer) {
		unregister_ioctl32_conversion(SHM_IOCTL_TX);
		unregister_ioctl32_conversion(SHM_IOCTL_ALLOC_ONE);
		unregister_ioctl32_conversion(SHM_IOCTL_ALLOC_MANY);
		unregister_ioctl32_conversion(SHM_IOCTL_ALLOC_ONE_AND_MANY);
		unregister_ioctl32_conversion(SHM_IOCTL_FREE_ONE);
	}
#endif
#endif
	pci_unregister_driver(&vmxnet3_driver);
}


module_exit(vmxnet3_exit_module);

MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION(VMXNET3_DRIVER_DESC);
MODULE_LICENSE("GPL v2");
MODULE_VERSION(VMXNET3_DRIVER_VERSION_STRING);
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
module_param(disable_lro, int, 0);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 2)
MODULE_PARM(enable_shm, "0-10i");
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 10)
module_param_array(enable_shm, int, num_enable_shm, 0);
#else
module_param_array(enable_shm, int, &num_enable_shm, 0);
#endif
MODULE_PARM_DESC(enable_shm, "Shared memory enable");
module_param(shm_disclaimer, charp, 0);
MODULE_PARM_DESC(shm_disclaimer, "Shared memory disclaimer");
module_param(shm_pool_size, int, 0);
MODULE_PARM_DESC(shm_pool_size, "Shared memory pool size");
