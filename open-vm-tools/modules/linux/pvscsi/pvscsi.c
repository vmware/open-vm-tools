/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * pvscsi.c --
 *
 *    This is a driver for the VMware PVSCSI paravirt SCSI device.
 *    The PVSCSI device is a SCSI adapter for virtual disks which
 *    is implemented as a PCIe device.
 */

#include "driver-config.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>

#include "compat_scsi.h"
#include "compat_pci.h"
#include "compat_interrupt.h"

#include "pvscsi_defs.h"
#include "pvscsi_version.h"
#include "scsi_defs.h"
#include "vm_device_version.h"
#include "vm_assert.h"

/**************************************************************
 *
 *   VMWARE PVSCSI Linux interaction
 *
 *   All Linux specific driver routines and operations should
 *   go here.
 *
 **************************************************************/

#define PVSCSI_LINUX_DRIVER_DESC "VMware PVSCSI driver"

/* Module definitions */
MODULE_DESCRIPTION(PVSCSI_LINUX_DRIVER_DESC);
MODULE_AUTHOR("VMware, Inc.");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(PVSCSI_DRIVER_VERSION_STRING);
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");

#define PVSCSI_DRIVER_VECTORS_USED	1
#define DEFAULT_PAGES_PER_RING		8
#define PVSCSI_LINUX_DEFAULT_QUEUE_DEPTH        64

/* MSI has horrible performance in < 2.6.13 due to needless mask frotzing */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
#define DISABLE_MSI	0
#else
#define DISABLE_MSI	1
#endif

/* MSI-X has horrible performance in < 2.6.19 due to needless mask frobbing */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#define DISABLE_MSIX	0
#else
#define DISABLE_MSIX	1
#endif

/* Command line parameters */
static int pvscsi_debug_level;
module_param_call(pvscsi_debug_level, param_set_int, param_get_int,
		  &pvscsi_debug_level, 0600);
MODULE_PARM_DESC(pvscsi_debug_level, "Debug logging level - (default=0)");

static int pvscsi_ring_pages = DEFAULT_PAGES_PER_RING;
module_param_call(pvscsi_ring_pages, param_set_int, param_get_int,
		  &pvscsi_ring_pages, 0600);
MODULE_PARM_DESC(pvscsi_ring_pages, "Pages per ring - (default="
				    XSTR(DEFAULT_PAGES_PER_RING) ")");

static int pvscsi_cmd_per_lun = PVSCSI_LINUX_DEFAULT_QUEUE_DEPTH;
module_param_call(pvscsi_cmd_per_lun, param_set_int, param_get_int,
		  &pvscsi_cmd_per_lun, 0600);
MODULE_PARM_DESC(pvscsi_cmd_per_lun, "Maximum commands per lun - (default="
				     XSTR(PVSCSI_MAX_REQ_QUEUE_DEPTH) ")");

static int pvscsi_disable_msi = DISABLE_MSI;
module_param_call(pvscsi_disable_msi, param_set_int, param_get_int,
		  &pvscsi_disable_msi, 0600);
MODULE_PARM_DESC(pvscsi_disable_msi, "Disable MSI use in driver - (default="
				     XSTR(DISABLE_MSI) ")");

static int pvscsi_disable_msix = DISABLE_MSIX;
module_param_call(pvscsi_disable_msix, param_set_int, param_get_int,
		  &pvscsi_disable_msix, 0600);
MODULE_PARM_DESC(pvscsi_disable_msix, "Disable MSI-X use in driver - (default="
				      XSTR(DISABLE_MSIX) ")");

static int __init pvscsi_init(void);
static int __devinit pvscsi_probe(struct pci_dev *pdev,
				  const struct pci_device_id *id);
static const char *pvscsi_info(struct Scsi_Host *host);
static int pvscsi_queue(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *));
static int pvscsi_abort(struct scsi_cmnd *cmd);
static int pvscsi_host_reset(struct scsi_cmnd *cmd);
static int pvscsi_bus_reset(struct scsi_cmnd *cmd);
static int pvscsi_device_reset(struct scsi_cmnd *cmd);
static irqreturn_t pvscsi_isr COMPAT_IRQ_HANDLER_ARGS(irq, devp);
static void pvscsi_remove(struct pci_dev *pdev);
static void COMPAT_PCI_DECLARE_SHUTDOWN(pvscsi_shutdown, dev);
static void __exit pvscsi_exit(void);

static struct pci_device_id pvscsi_pci_tbl[] = {
	{PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_PVSCSI,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0,}
};
MODULE_DEVICE_TABLE(pci, pvscsi_pci_tbl);

static struct pci_driver pvscsi_pci_driver = {
	.name		= "pvscsi",
	.id_table	= pvscsi_pci_tbl,
	.probe		= pvscsi_probe,
	.remove		= __devexit_p(pvscsi_remove),
	COMPAT_PCI_SHUTDOWN(pvscsi_shutdown)
};

#ifdef CONFIG_PCI_MSI
static const struct msix_entry base_entries[PVSCSI_DRIVER_VECTORS_USED] = {
	{ 0, PVSCSI_VECTOR_COMPLETION },
};
#endif

static struct scsi_host_template pvscsi_template = {
	.module				= THIS_MODULE,
	.name				= "VMware PVSCSI Host Adapter",
	.proc_name			= "pvscsi",
	.info				= pvscsi_info,
	.queuecommand			= pvscsi_queue,
	.this_id			= -1,
	.sg_tablesize			= PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT,
	.dma_boundary			= UINT_MAX,
	.max_sectors			= 0xffff,
	.use_clustering			= ENABLE_CLUSTERING,
	.eh_abort_handler		= pvscsi_abort,
	.eh_device_reset_handler	= pvscsi_device_reset,
	.eh_bus_reset_handler		= pvscsi_bus_reset,
	.eh_host_reset_handler		= pvscsi_host_reset,
};

struct PVSCSISGList {
   PVSCSISGElement sge[PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT];
};

/* Private per-request struct */
struct pvscsi_ctx {
	/*
	 * We use cmd->scsi_done to store the completion callback.
	 * The index of the context in cmd_map serves as the context ID for a
	 * 1-to-1 mapping completions back to requests.
	 */
	struct scsi_cmnd	*cmd;
	struct PVSCSISGList	*sgl;
	struct list_head	list;
};

/* Private per-adapter struct */
struct pvscsi_adapter {
	unsigned long		base;
	unsigned long		iomap;
	unsigned int		irq;
	char			rev;
	char			use_msi;
	char			use_msix;
	char			log;

	spinlock_t		hw_lock;
	RingReqDesc		*req_ring;
	unsigned		req_pages;
	unsigned		req_depth;

	RingCmpDesc		*cmp_ring;
	unsigned		cmp_pages;
	unsigned		cmp_depth;

	RingsState		*ring_state;

	struct pci_dev		*dev;
	struct Scsi_Host	*host;

	struct list_head	cmd_pool;
	struct pvscsi_ctx	*cmd_map;
	unsigned		last_map;

	int			irq_vectors[PVSCSI_DRIVER_VECTORS_USED];
};

#define HOST_ADAPTER(host) ((struct pvscsi_adapter *)(host)->hostdata)

/* Low-level adapter function prototypes */
static inline u32 pvscsi_read_intr_status(const struct pvscsi_adapter *adapter);
static inline void pvscsi_write_intr_status(const struct pvscsi_adapter *adapter,
					    u32 val);
static inline void pvscsi_write_intr_mask(const struct pvscsi_adapter *adapter,
					  u32 val);
static void pvscsi_abort_cmd(const struct pvscsi_adapter *adapter,
			     const struct pvscsi_ctx *ctx);
static void pvscsi_kick_io(const struct pvscsi_adapter *adapter, unsigned char op);
static void pvscsi_process_request_ring(const struct pvscsi_adapter *adapter);
static void ll_adapter_reset(const struct pvscsi_adapter *adapter);
static void ll_bus_reset(const struct pvscsi_adapter *adapter);
static void ll_device_reset(const struct pvscsi_adapter *adapter, u32 target);
static void pvscsi_setup_rings(struct pvscsi_adapter *adapter);
static void pvscsi_process_completion_ring(struct pvscsi_adapter *adapter);

static inline int pvscsi_queue_ring(struct pvscsi_adapter *adapter,
				    struct pvscsi_ctx *ctx,
				    struct scsi_cmnd *cmd);
static inline void pvscsi_complete_request(struct pvscsi_adapter *adapter,
					   const RingCmpDesc *e);

#define LOG(level, fmt, args...)				\
do {								\
	if (pvscsi_debug_level > level)				\
		printk(KERN_DEBUG "pvscsi: " fmt, args);	\
} while (0)

module_init(pvscsi_init);
static int __init pvscsi_init(void)
{
	printk(KERN_DEBUG "%s - version %s\n",
	       PVSCSI_LINUX_DRIVER_DESC, PVSCSI_DRIVER_VERSION_STRING);
	return pci_register_driver(&pvscsi_pci_driver);
}

module_exit(pvscsi_exit);
static void __exit pvscsi_exit(void)
{
	pci_unregister_driver(&pvscsi_pci_driver);
}

static inline void pvscsi_free_sgls(struct pvscsi_adapter *adapter)
{
	unsigned i, max;
	struct pvscsi_ctx *ctx = adapter->cmd_map;

	max = adapter->req_depth;
	for (i = 0; i < max; ++i, ++ctx)
		kfree(ctx->sgl);
}

static inline int pvscsi_setup_msix(struct pvscsi_adapter *adapter)
{
#ifdef CONFIG_PCI_MSI
	int ret;
	unsigned i;
	struct msix_entry entries[PVSCSI_DRIVER_VECTORS_USED];

	memcpy(entries, base_entries, sizeof entries);
	ret = pci_enable_msix(adapter->dev, entries, ARRAY_SIZE(entries));
	if (ret != 0)
		return ret;

	for (i = 0; i < PVSCSI_DRIVER_VECTORS_USED; i++)
		adapter->irq_vectors[i] = entries[i].vector;

	return 0;
#else
	return -1;
#endif
}

static inline void pvscsi_shutdown_msi(struct pvscsi_adapter *adapter)
{
#ifdef CONFIG_PCI_MSI

	if (adapter->use_msi)
		pci_disable_msi(adapter->dev);

	if (adapter->use_msix) {
#if 0
		/* For when multiple vectors are supported */
		unsigned i;
		for (i = 0; i < PVSCSI_DRIVER_VECTORS_USED; i++)
			if (adapter->irq_vectors[i] != -1)
				free_irq(adapter->irq_vectors[i], adapter);
#endif
		pci_disable_msix(adapter->dev);
	}
#endif
}

static void pvscsi_release_resources(struct pvscsi_adapter *adapter)
{
	if (adapter->irq)
		free_irq(adapter->irq, adapter);

	pvscsi_shutdown_msi(adapter);

	if (adapter->iomap)
		iounmap((void *)adapter->iomap);

	pci_release_regions(adapter->dev);

	if (adapter->cmd_map) {
		pvscsi_free_sgls(adapter);
		kfree(adapter->cmd_map);
	}

	kfree(adapter->ring_state);

	if (adapter->req_ring)
		vfree(adapter->req_ring);

	if (adapter->cmp_ring)
		vfree(adapter->cmp_ring);
}

static int __devinit pvscsi_allocate_rings(struct pvscsi_adapter *adapter)
{
	adapter->ring_state = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!adapter->ring_state)
		return -ENOMEM;

	adapter->req_pages = MIN(PVSCSI_MAX_NUM_PAGES_REQ_RING,
				 pvscsi_ring_pages);
	adapter->req_depth = adapter->req_pages *
			     PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE;
	adapter->req_ring = vmalloc(adapter->req_pages * PAGE_SIZE);
	if (!adapter->req_ring)
		return -ENOMEM;

	adapter->cmp_pages = MIN(PVSCSI_MAX_NUM_PAGES_CMP_RING,
				 pvscsi_ring_pages);
	adapter->cmp_depth = adapter->cmp_pages *
			     PVSCSI_MAX_NUM_CMP_ENTRIES_PER_PAGE;
	adapter->cmp_ring = vmalloc(adapter->cmp_pages * PAGE_SIZE);
	if (!adapter->cmp_ring)
		return -ENOMEM;

	return 0;
}

/*
 * Allocate scatter gather lists.
 *
 * These are statically allocated.  Trying to be clever was not worth it.
 *
 * Dynamic allocation can fail, and we can't go deeep into the memory
 * allocator, since we're a SCSI driver, and trying too hard to allocate
 * memory might generate disk I/O.  We also don't want to fail disk I/O
 * in that case because we can't get an allocation - the I/O could be
 * trying to swap out data to free memory.  Since that is pathological,
 * just use a statically allocated scatter list.
 *
 */
static int __devinit pvscsi_allocate_sg(struct pvscsi_adapter *adapter)
{
	struct pvscsi_ctx *ctx;
	unsigned max;
	int i;

        ctx = adapter->cmd_map;
	max = adapter->req_depth;
	ASSERT_ON_COMPILE(sizeof(struct PVSCSISGList) <= PAGE_SIZE);

	for (i = 0; i < max; ++i, ++ctx) {
		ctx->sgl = kmalloc(PAGE_SIZE, GFP_KERNEL);
		BUG_ON((long)ctx->sgl & ~PAGE_MASK);
		if (!ctx->sgl) {
			for (; i >= 0; --i, --ctx) {
				kfree(ctx->sgl);
				ctx->sgl = NULL;
			}
			return -ENOMEM;
		}
	}

	return 0;
}

static int __devinit
pvscsi_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int error = -ENODEV;
	u8 pci_bus, pci_dev_func, rev;
	unsigned long base, remap, i;
	struct Scsi_Host *host;
	int irq;
	struct pvscsi_adapter *adapter;

	if (pci_enable_device(pdev))
		return error;

	if (pdev->vendor != PCI_VENDOR_ID_VMWARE)
		goto out_disable_device;

	pci_bus = pdev->bus->number;
	pci_dev_func = pdev->devfn;
	pci_read_config_byte(pdev, PCI_CLASS_REVISION, &rev);

	if (pci_set_dma_mask(pdev, DMA_64BIT_MASK) ||
	    pci_set_consistent_dma_mask(pdev, DMA_64BIT_MASK)) {
		if (pci_set_dma_mask(pdev, DMA_32BIT_MASK) &&
		    pci_set_consistent_dma_mask(pdev, DMA_32BIT_MASK)) {
			printk(KERN_ERR "pvscsi: unable to set usable DMA mask\n");
			goto out_disable_device;
		}
	}

	printk(KERN_NOTICE "pvscsi: found VMware PVSCSI rev %d on "
			   "bus %d:slot %d:func %d\n", rev, pci_bus,
		PCI_SLOT(pci_dev_func), PCI_FUNC(pci_dev_func));

	pvscsi_template.can_queue =
		MIN(PVSCSI_MAX_NUM_PAGES_REQ_RING, pvscsi_ring_pages) *
		PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE;
	pvscsi_template.cmd_per_lun =
		MIN(pvscsi_template.can_queue, pvscsi_cmd_per_lun);
	host = scsi_host_alloc(&pvscsi_template, sizeof(struct pvscsi_adapter));
	if (!host) {
		printk(KERN_ERR "pvscsi: failed to allocate host\n");
		goto out_disable_device;
	}

	adapter = HOST_ADAPTER(host);
	memset(adapter, 0, sizeof(*adapter));
	for (i = 0; i < PVSCSI_DRIVER_VECTORS_USED; i++)
		adapter->irq_vectors[i] = -1;
	adapter->dev = pdev;
	adapter->host = host;
	adapter->rev = rev;

	spin_lock_init(&adapter->hw_lock);

	host->max_channel = 0;
	host->max_id = 16;
	host->max_lun = 1;

	if (pci_request_regions(pdev, "pvscsi")) {
		printk(KERN_ERR "pvscsi: pci memory selection failed\n");
		goto out_free_host;
	}

	/* Find the BARs for memory mapped I/O */
	base = 0;
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
		if ((pci_resource_flags(pdev, i) & PCI_BASE_ADDRESS_SPACE_IO))
			continue;

		/* Skip non-kick memory mapped space */
		if (pci_resource_len(pdev, i) < PVSCSI_MEM_SPACE_NUM_PAGES * PAGE_SIZE)
			continue;

		base = pci_resource_start(pdev, i);
		break;
	}

	if (!base) {
		printk(KERN_ERR "pvscsi: adapter has no suitable MMIO region\n");
		goto out_release_resources;
	}
	remap = (unsigned long)ioremap(base, PVSCSI_MEM_SPACE_SIZE);
	if (!remap) {
		printk(KERN_ERR "pvscsi: can't ioremap 0x%lx\n", base);
		goto out_release_resources;
	}
	adapter->iomap = remap;

	pci_set_master(pdev);
	pci_set_drvdata(pdev, host);

	ll_adapter_reset(adapter);
	error = pvscsi_allocate_rings(adapter);
	if (error) {
		printk(KERN_ERR "pvscsi: unable to allocate ring memory\n");
		goto out_release_resources;
	}

	/*
	 * Setup the rings; from this point on we should reset
	 * the adapter if anything goes wrong.
	 */
	pvscsi_setup_rings(adapter);

	adapter->cmd_map = kmalloc(adapter->req_depth *
				   sizeof(struct pvscsi_ctx), GFP_KERNEL);
	if (!adapter->cmd_map) {
		printk(KERN_ERR "pvscsi: failed to allocate memory.\n");
		goto out_reset_adapter;
	}
	memset(adapter->cmd_map, 0, adapter->req_depth *
				    sizeof(struct pvscsi_ctx));

	INIT_LIST_HEAD(&adapter->cmd_pool);
	for (i = 0; i < adapter->req_depth; i++) {
		struct pvscsi_ctx *ctx = adapter->cmd_map + i;
		list_add(&ctx->list, &adapter->cmd_pool);
	}

	/* Now allocate a DMA-able cache for SG lists */
	if (pvscsi_allocate_sg(adapter) != 0) {
		printk(KERN_WARNING "pvscsi: unable to allocate SG cache\n");
		printk(KERN_WARNING "pvscsi: disabling scatter/gather.\n");
		host->sg_tablesize = 1;
	}

	/* Setup MSI if possible */
#ifdef CONFIG_PCI_MSI
	if (!pvscsi_disable_msix && pvscsi_setup_msix(adapter) == 0) {
		printk(KERN_INFO "pvscsi: enabled MSI-X\n");
		adapter->use_msix = 1;
	} else if (!pvscsi_disable_msi && pci_enable_msi(pdev) == 0) {
		printk(KERN_INFO "pvscsi: enabled MSI\n");
		adapter->use_msi = 1;
	} else
		printk(KERN_INFO "pvscsi: using normal PCI interrupts\n");
#else
	printk(KERN_INFO "pvscsi: this kernel does not support MSI, consider enabling it\n");
#endif

	/* Now get an IRQ. For MSI-X, we only use one vector currently, vector zero */
	ASSERT_ON_COMPILE(PVSCSI_DRIVER_VECTORS_USED == 1);
	irq = adapter->use_msix ? adapter->irq_vectors[0] : pdev->irq;
	if (request_irq(irq, pvscsi_isr, COMPAT_IRQF_SHARED, "pvscsi", adapter)) {
		printk(KERN_ERR "pvscsi: unable to request IRQ %d\n", irq);
		goto out_reset_adapter;
	}
	adapter->irq = irq;

	error = scsi_add_host(host, &pdev->dev);
	if (error) {
		printk(KERN_ERR "pvscsi: scsi_add_host failed: %d\n", error);
		goto out_reset_adapter;
	}

	/* Enable device interrupts */
	pvscsi_write_intr_mask(adapter, PVSCSI_INTR_ALL);

	scsi_scan_host(host);

	return 0;

out_reset_adapter:
	ll_adapter_reset(adapter);
out_release_resources:
	pvscsi_release_resources(adapter);
out_free_host:
	scsi_host_put(host);
out_disable_device:
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);

	return error;
}

static const char *pvscsi_info(struct Scsi_Host *host)
{
	static char buf[512];
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);

	sprintf(buf, "VMware PVSCSI storage adapter rev %c, %u reqs (%u pages), %u cmps (%u pages), cmd_per_lun=%u",
		adapter->rev + 'A' - 1,
		adapter->req_depth, adapter->req_pages,
		adapter->cmp_depth, adapter->cmp_pages,
		pvscsi_template.cmd_per_lun);
	return buf;
}

static struct pvscsi_ctx *
pvscsi_find_context(const struct pvscsi_adapter *adapter, struct scsi_cmnd *cmd)
{
	struct pvscsi_ctx *ctx, *end;

	end = &adapter->cmd_map[adapter->req_depth];
	for (ctx = adapter->cmd_map; ctx < end; ctx++)
		if (ctx->cmd == cmd)
			return ctx;

	return NULL;
}

static struct pvscsi_ctx *
pvscsi_allocate_context(struct pvscsi_adapter *adapter, struct scsi_cmnd *cmd)
{
	struct pvscsi_ctx *ctx;

	if (list_empty(&adapter->cmd_pool))
		return NULL;

	ctx = list_entry(adapter->cmd_pool.next, struct pvscsi_ctx, list);
	ctx->cmd = cmd;
	list_del(&ctx->list);

	return ctx;
}

static inline struct scsi_cmnd *
pvscsi_free_context(struct pvscsi_adapter *adapter, struct pvscsi_ctx *ctx)
{
	struct scsi_cmnd *cmd;

	cmd = ctx->cmd;
	ctx->cmd = NULL;
	list_add(&ctx->list, &adapter->cmd_pool);

	return cmd;
}

/*
 * Map a pvscsi_ctx struct to a context ID field value; we map to a simple
 * non-zero integer.
 */
static inline u64
pvscsi_map_context(const struct pvscsi_adapter *adapter, const struct pvscsi_ctx *ctx)
{
	return (ctx - adapter->cmd_map) + 1;
}

static inline struct pvscsi_ctx *
pvscsi_get_context(const struct pvscsi_adapter *adapter, u64 context)
{
	return &adapter->cmd_map[context - 1];
}

static int pvscsi_queue(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	struct pvscsi_ctx *ctx;
	unsigned long flags;

	spin_lock_irqsave(&adapter->hw_lock, flags);

	ctx = pvscsi_allocate_context(adapter, cmd);
	if (!ctx || pvscsi_queue_ring(adapter, ctx, cmd) != 0) {
		if (ctx)
			pvscsi_free_context(adapter, ctx);
		spin_unlock_irqrestore(&adapter->hw_lock, flags);
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	cmd->scsi_done = done;

	LOG(3, "queued cmd %p, ctx %p, op=%x\n", cmd, ctx, cmd->cmnd[0]);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);

	pvscsi_kick_io(adapter, cmd->cmnd[0]);

	return 0;
}

static int pvscsi_abort(struct scsi_cmnd *cmd)
{
	struct pvscsi_adapter *adapter = HOST_ADAPTER(cmd->device->host);
	struct pvscsi_ctx *ctx;
	unsigned long flags;

	printk(KERN_DEBUG "pvscsi: attempting task abort on %p, %p\n",
		adapter, cmd);

	spin_lock_irqsave(&adapter->hw_lock, flags);

	/*
	 * Poll the completion ring first - we might be trying to abort
	 * a command that is waiting to be dispatched in the completion ring.
	 */
	pvscsi_process_completion_ring(adapter);

	/*
	 * If there is no context for the command, it either already succeeded
	 * or else was never properly issued.  Not our problem.
	 */
	ctx = pvscsi_find_context(adapter, cmd);
	if (!ctx) {
		LOG(1, "Failed to abort cmd %p\n", cmd);
		goto out;
	}

	pvscsi_abort_cmd(adapter, ctx);

	pvscsi_process_completion_ring(adapter);

out:
	spin_unlock_irqrestore(&adapter->hw_lock, flags);
	return SUCCESS;
}

static inline void
pvscsi_free_sg(const struct pvscsi_adapter *adapter, struct scsi_cmnd *cmd)
{
	unsigned count = scsi_sg_count(cmd);

	if (count) {
		struct scatterlist *sg = scsi_sglist(cmd);
		pci_unmap_sg(adapter->dev, sg, count, cmd->sc_data_direction);
	}
}

/*
 * Abort all outstanding requests.  This is only safe to use if the completion
 * ring will never be walked again or the device has been reset, because it
 * destroys the 1-1 mapping between context field passed to emulation and our
 * request structure.
 */
static inline void pvscsi_reset_all(struct pvscsi_adapter *adapter)
{
	unsigned i;

	for (i = 0; i < adapter->req_depth; i++) {
		struct pvscsi_ctx *ctx = &adapter->cmd_map[i];
		struct scsi_cmnd *cmd = ctx->cmd;
		if (cmd) {
			printk(KERN_ERR "pvscsi: forced reset on cmd %p\n", cmd);
			pvscsi_free_sg(adapter, cmd);
			pvscsi_free_context(adapter, ctx);
			cmd->result = (DID_RESET << 16);
			cmd->scsi_done(cmd);
		}
	}
}

static int pvscsi_host_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	unsigned long flags;

	printk(KERN_NOTICE "pvscsi: attempting host reset on %p\n", adapter);

	/*
	 * We're going to tear down the entire ring structure and set it back
	 * up, so stalling new requests until all completions are flushed and
	 * the rings are back in place.
	 */
	spin_lock_irqsave(&adapter->hw_lock, flags);

	pvscsi_process_request_ring(adapter);
	ll_adapter_reset(adapter);

	/*
	 * Now process any completions.  Note we do this AFTER adapter reset,
	 * which is strange, but stops races where completions get posted
	 * between processing the ring and issuing the reset.  The backend will
	 * not touch the ring memory after reset, so the immediately pre-reset
	 * completion ring state is still valid.
	 */
	pvscsi_process_completion_ring(adapter);

	pvscsi_reset_all(adapter);
	pvscsi_setup_rings(adapter);
	pvscsi_write_intr_mask(adapter, PVSCSI_INTR_ALL);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);

	return SUCCESS;
}

static int pvscsi_bus_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	unsigned long flags;

	printk(KERN_NOTICE "pvscsi: attempting bus reset on %p\n", adapter);

	/*
	 * We don't want to queue new requests for this bus after
	 * flushing all pending requests to emulation, since new
	 * requests could then sneak in during this bus reset phase,
	 * so take the lock now.
	 */
	spin_lock_irqsave(&adapter->hw_lock, flags);

	pvscsi_process_request_ring(adapter);
	ll_bus_reset(adapter);
	pvscsi_process_completion_ring(adapter);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);

	return SUCCESS;
}

static int pvscsi_device_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);
	unsigned long flags;

	printk(KERN_NOTICE "pvscsi: attempting device reset on %p,%d\n",
		adapter, cmd->device->id);

	/*
	 * We don't want to queue new requests for this device after flushing
	 * all pending requests to emulation, since new requests could then
	 * sneak in during this device reset phase, so take the lock now.
	 */
	spin_lock_irqsave(&adapter->hw_lock, flags);

	pvscsi_process_request_ring(adapter);
	ll_device_reset(adapter, cmd->device->id);
	pvscsi_process_completion_ring(adapter);

	spin_unlock_irqrestore(&adapter->hw_lock, flags);

	return SUCCESS;
}

static irqreturn_t pvscsi_isr COMPAT_IRQ_HANDLER_ARGS(irq, devp)
{
	struct pvscsi_adapter *adapter = devp;
	int handled = FALSE;

	if (adapter->use_msi || adapter->use_msix)
		handled = TRUE;
	else {
		u32 val = pvscsi_read_intr_status(adapter);
		handled = (val & PVSCSI_INTR_ALL) != 0;
		if (handled)
			pvscsi_write_intr_status(devp, val);
	}

	if (handled) {
		unsigned long flags;
		spin_lock_irqsave(&adapter->hw_lock, flags);
		pvscsi_process_completion_ring(adapter);
		spin_unlock_irqrestore(&adapter->hw_lock, flags);
	}

	LOG(2, "pvscsi_isr %d\n", handled);

	return IRQ_RETVAL(handled);
}

/* Shutdown an entire device, syncinc all outstanding I/O */
static void __pvscsi_shutdown(struct pvscsi_adapter *adapter)
{
	pvscsi_write_intr_mask(adapter, 0);
	if (adapter->irq) {
		free_irq(adapter->irq, adapter);
		adapter->irq = 0;
	}

#ifdef CONFIG_PCI_MSI
	if (adapter->use_msi) {
		pci_disable_msi(adapter->dev);
		adapter->use_msi = 0;
	}
#endif

	pvscsi_process_request_ring(adapter);
        pvscsi_process_completion_ring(adapter);
	ll_adapter_reset(adapter);
}

static void COMPAT_PCI_DECLARE_SHUTDOWN(pvscsi_shutdown, dev)
{
	struct Scsi_Host *host = pci_get_drvdata(COMPAT_PCI_TO_DEV(dev));
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);

	__pvscsi_shutdown(adapter);
}

static void pvscsi_remove(struct pci_dev *pdev)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	struct pvscsi_adapter *adapter = HOST_ADAPTER(host);

	scsi_remove_host(host);

	__pvscsi_shutdown(adapter);
	pvscsi_release_resources(adapter);

	scsi_host_put(host);

	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
}


/**************************************************************
 *
 *   VMWARE Hypervisor ring / SCSI mid-layer interactions
 *
 *   Functions which have to deal with both ring semantics
 *   and Linux SCSI internals are placed here.
 *
 **************************************************************/

static inline struct PVSCSISGList *
pvscsi_create_sg(struct pvscsi_ctx *ctx, struct scatterlist *sg, unsigned count)
{
	unsigned i;
	struct PVSCSISGList *sgl;
	struct PVSCSISGElement *sge;

	sgl = ctx->sgl;

	BUG_ON(count > PVSCSI_MAX_NUM_SG_ENTRIES_PER_SEGMENT);

	sge = &sgl->sge[0];
	for (i = 0; i < count; i++, sg++) {
		sge[i].addr = sg_dma_address(sg);
		sge[i].length = sg_dma_len(sg);
		sge[i].flags = 0;
	}

	return sgl;
}

/*
 * Map all data buffers for a command into PCI space and
 * setup the scatter/gather list if needed.
 */
static inline void
pvscsi_map_buffers(struct pvscsi_adapter *adapter, struct pvscsi_ctx *ctx,
		   struct scsi_cmnd *cmd, RingReqDesc *e)
{
	unsigned count;
	unsigned bufflen = scsi_bufflen(cmd);

	e->dataLen = bufflen;
	e->dataAddr = 0;
	if (bufflen == 0)
		return;

	count = scsi_sg_count(cmd);
	if (count != 0) {
		struct scatterlist *sg = scsi_sglist(cmd);
		int segs = pci_map_sg(adapter->dev, sg, count,
				      cmd->sc_data_direction);
		if (segs > 1) {
			struct PVSCSISGList *sgl;

			e->flags |= PVSCSI_FLAG_CMD_WITH_SG_LIST;
			sgl = pvscsi_create_sg(ctx, sg, segs);
			e->dataAddr = __pa(sgl);
		} else
			e->dataAddr = sg_dma_address(sg);
	} else {
		e->dataAddr = __pa(scsi_request_buffer(cmd));
	}
}

/*
 * Translate a Linux SCSI request into a request ring entry.
 */
static inline int
pvscsi_queue_ring(struct pvscsi_adapter *adapter, struct pvscsi_ctx *ctx,
		  struct scsi_cmnd *cmd)
{
	RingsState *s;
	RingReqDesc *e, *ring;

	s = adapter->ring_state;
	ring = adapter->req_ring;

	/*
	 * If this condition holds, we might have room on the request ring, but
	 * we might not have room on the completion ring for the response.
	 * However, we have already ruled out this possibility - we would not
	 * have successfully allocated a context if it were true, since we only
	 * have one context per request entry.  Check for it anyway, since it
	 * would be a serious bug.
	 */
	if (s->reqProdIdx - s->cmpConsIdx >= adapter->req_depth) {
		printk(KERN_ERR "pvscsi: ring full: reqProdIdx=%d cmpConsIdx=%d\n",
			s->reqProdIdx, s->cmpConsIdx);
		return -1;
	}

	e = ring + (s->reqProdIdx % adapter->req_depth);
	{
		struct scsi_device *sdev;
		sdev = cmd->device;
		e->bus = sdev->channel;
		e->target = sdev->id;
		memset(e->lun, 0, sizeof(e->lun));
		e->lun[1] = sdev->lun;
	}
	if (cmd->sense_buffer) {
		e->senseLen = SCSI_SENSE_BUFFERSIZE;
		e->senseAddr = __pa(cmd->sense_buffer);
	} else {
		e->senseLen = 0;
		e->senseAddr = 0;
	}
	e->cdbLen = cmd->cmd_len;
	e->vcpuHint = smp_processor_id();
	memcpy(e->cdb, cmd->cmnd, e->cdbLen);

	e->tag = SIMPLE_QUEUE_TAG;
	if (cmd->device->tagged_supported) {
		if (cmd->tag == HEAD_OF_QUEUE_TAG ||
		    cmd->tag == ORDERED_QUEUE_TAG)
			e->tag = cmd->tag;
	}

	if (cmd->sc_data_direction == DMA_FROM_DEVICE)
		e->flags = PVSCSI_FLAG_CMD_DIR_TOHOST;
	else if (cmd->sc_data_direction == DMA_TO_DEVICE)
		e->flags = PVSCSI_FLAG_CMD_DIR_TODEVICE;
	else if (cmd->sc_data_direction == DMA_NONE)
		e->flags = PVSCSI_FLAG_CMD_DIR_NONE;
	else
		e->flags = 0;

	pvscsi_map_buffers(adapter, ctx, cmd, e);

	/*
	 * Fill in the context entry so we can recognize this
	 * request off the completion queue
	 */
	e->context = pvscsi_map_context(adapter, ctx);

        barrier();

	s->reqProdIdx++;

	return 0;
}

/*
 * Pull a completion descriptor off and pass the completion back
 * to the SCSI mid layer.
 */
static inline void
pvscsi_complete_request(struct pvscsi_adapter *adapter, const RingCmpDesc *e)
{
	struct pvscsi_ctx *ctx;
	struct scsi_cmnd *cmd;
	u32 btstat = e->hostStatus;
	u32 sdstat = e->scsiStatus;

	ctx = pvscsi_get_context(adapter, e->context);
	cmd = pvscsi_free_context(adapter, ctx);
	cmd->result = 0;

	if (sdstat != SAM_STAT_GOOD &&
	    (btstat == BTSTAT_SUCCESS ||
	     btstat == BTSTAT_LINKED_COMMAND_COMPLETED ||
	     btstat == BTSTAT_LINKED_COMMAND_COMPLETED_WITH_FLAG))
		switch (sdstat) {
		case SAM_STAT_CHECK_CONDITION:
			/*
			 * Sense data is set by the emulation.
			 * Linux seems to want DID_OK despite the error.
			 */
			cmd->result = (DID_OK << 16) | (SAM_STAT_CHECK_CONDITION);
			if (cmd->sense_buffer)
				cmd->result |= (DRIVER_SENSE << 24);
			break;
		case SAM_STAT_BUSY:
			/* Back off. */
			cmd->result = (DID_OK << 16) | sdstat;
			break;
		default:
			cmd->result = (DID_ERROR << 16);
			LOG(0, "Unhandled SCSI status: 0x%x\n", sdstat);
		}

	else
		switch (btstat) {
		case BTSTAT_SUCCESS:
		case BTSTAT_LINKED_COMMAND_COMPLETED:
		case BTSTAT_LINKED_COMMAND_COMPLETED_WITH_FLAG:
			/* If everything went fine, let's move on..  */
			cmd->result = (DID_OK << 16);
			break;

		case BTSTAT_DATARUN:
		case BTSTAT_DATA_UNDERRUN:
			/* Report residual data in underruns */
			scsi_set_resid(cmd, scsi_bufflen(cmd) - e->dataLen);
			cmd->result = (DID_ERROR << 16);
			break;

		case BTSTAT_SELTIMEO:
			/* Our emulation returns this for non-connected devs */
			cmd->result = (DID_BAD_TARGET << 16);
			break;

		case BTSTAT_LUNMISMATCH:
		case BTSTAT_TAGREJECT:
		case BTSTAT_BADMSG:
			cmd->result = (DRIVER_INVALID << 24);
			/* fall through */

		case BTSTAT_HAHARDWARE:
		case BTSTAT_INVPHASE:
		case BTSTAT_HATIMEOUT:
		case BTSTAT_NORESPONSE:
		case BTSTAT_DISCONNECT:
		case BTSTAT_HASOFTWARE:
		case BTSTAT_BUSFREE:
		case BTSTAT_SENSFAILED:
			cmd->result |= (DID_ERROR << 16);
			break;

		case BTSTAT_SENTRST:
		case BTSTAT_RECVRST:
		case BTSTAT_BUSRESET:
			cmd->result = (DID_RESET << 16);
			break;

		case BTSTAT_ABORTQUEUE:
			cmd->result = (DID_ABORT << 16);
			break;

		case BTSTAT_SCSIPARITY:
			cmd->result = (DID_PARITY << 16);
			break;

		default:
			cmd->result = (DID_ERROR << 16);
			LOG(0, "Unknown completion status: 0x%x\n", btstat);
	}

	LOG(3, "cmd=%p %x ctx=%p result=0x%x status=0x%x,%x\n",
		cmd, cmd->cmnd[0], ctx, cmd->result, btstat, sdstat);

	pvscsi_free_sg(adapter, cmd);

	cmd->scsi_done(cmd);
}


/**************************************************************
 *
 *   VMWARE PVSCSI Hypervisor Communication Implementation
 *
 *   This code should be maintained to match the Windows driver
 *   as closely as possible.  This code is largely independent
 *   of any Linux internals.
 *
 **************************************************************/

static inline void pvscsi_reg_write(const struct pvscsi_adapter *adapter,
				    u32 offset, u32 val)
{
	writel(val, (void *)(adapter->iomap + offset));
}

static inline u32 pvscsi_reg_read(const struct pvscsi_adapter *adapter,
				    u32 offset)
{
	return readl((void *)(adapter->iomap + offset));
}

static inline u32 pvscsi_read_intr_status(const struct pvscsi_adapter *adapter)
{
	return pvscsi_reg_read(adapter, PVSCSI_REG_OFFSET_INTR_STATUS);
}

static inline void pvscsi_write_intr_status(const struct pvscsi_adapter *adapter,
					    u32 val)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_INTR_STATUS, val);
}

static inline void pvscsi_write_intr_mask(const struct pvscsi_adapter *adapter,
					  u32 val)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_INTR_MASK, val);
}

static inline void pvscsi_write_cmd_desc(const struct pvscsi_adapter *adapter,
					 u32 cmd, void *desc, size_t len)
{
	u32 *ptr = (u32 *)desc;
	unsigned i;

	len /= sizeof(u32);
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_COMMAND, cmd);
	for (i = 0; i < len; i++)
		pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_COMMAND_DATA, ptr[i]);
}

static void pvscsi_abort_cmd(const struct pvscsi_adapter *adapter,
			     const struct pvscsi_ctx *ctx)
{
	struct CmdDescAbortCmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.target = ctx->cmd->device->id;
	cmd.context = pvscsi_map_context(adapter, ctx);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_ABORT_CMD, &cmd, sizeof(cmd));
}

static inline void pvscsi_kick_rw_io(const struct pvscsi_adapter *adapter)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_KICK_RW_IO, 0);
}

/* Get device to respond immediately */
static void pvscsi_process_request_ring(const struct pvscsi_adapter *adapter)
{
	pvscsi_reg_write(adapter, PVSCSI_REG_OFFSET_KICK_NON_RW_IO, 0);
}

static inline int scsi_is_rw(unsigned char op)
{
	return op == READ_6  || op == WRITE_6 ||
	       op == READ_10 || op == WRITE_10 ||
	       op == READ_12 || op == WRITE_12 ||
	       op == READ_16 || op == WRITE_16;
}

static void pvscsi_kick_io(const struct pvscsi_adapter *adapter, unsigned char op)
{
	if (scsi_is_rw(op))
		pvscsi_kick_rw_io(adapter);
	else
		pvscsi_process_request_ring(adapter);
}

static void ll_adapter_reset(const struct pvscsi_adapter *adapter)
{
	u32 val;

	LOG(0, "Adapter Reset on %p\n", adapter);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_ADAPTER_RESET, NULL, 0);
	val = pvscsi_read_intr_status(adapter);
	LOG(0, "Adapter Reset done: %u\n", val);
}

static void ll_bus_reset(const struct pvscsi_adapter *adapter)
{
	LOG(0, "Reseting bus on %p\n", adapter);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_RESET_BUS, NULL, 0);
}

static void ll_device_reset(const struct pvscsi_adapter *adapter, u32 target)
{
	struct CmdDescResetDevice cmd;

	LOG(0, "Reseting device: target=%u\n", target);

	memset(&cmd, 0, sizeof(cmd));
	cmd.target = target;

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_RESET_DEVICE, &cmd, sizeof(cmd));
}

static void pvscsi_setup_rings(struct pvscsi_adapter *adapter)
{
	struct CmdDescSetupRings cmd;
	unsigned i, pages;
	void *base;

	memset(&cmd, 0, sizeof(cmd));
	cmd.ringsStatePPN = __pa(adapter->ring_state) >> PAGE_SHIFT;

	cmd.reqRingNumPages = pages = adapter->req_pages;
	for (i = 0, base = adapter->req_ring; i < pages; i++, base += PAGE_SIZE)
		cmd.reqRingPPNs[i] = page_to_pfn(vmalloc_to_page(base));

	cmd.cmpRingNumPages = pages = adapter->cmp_pages;
	for (i = 0, base = adapter->cmp_ring; i < pages; i++, base += PAGE_SIZE)
		cmd.cmpRingPPNs[i] = page_to_pfn(vmalloc_to_page(base));

	memset(adapter->ring_state, 0, PAGE_SIZE);
	memset(adapter->req_ring, 0, adapter->req_pages * PAGE_SIZE);
	memset(adapter->cmp_ring, 0, adapter->cmp_pages * PAGE_SIZE);

	pvscsi_write_cmd_desc(adapter, PVSCSI_CMD_SETUP_RINGS, &cmd, sizeof(cmd));
}

static void pvscsi_process_completion_ring(struct pvscsi_adapter *adapter)
{
	RingsState *s = adapter->ring_state;
	RingCmpDesc *ring = adapter->cmp_ring;

	while (s->cmpConsIdx != s->cmpProdIdx) {
		RingCmpDesc *e = ring + (s->cmpConsIdx % adapter->cmp_depth);

		pvscsi_complete_request(adapter, e);
		smp_wmb();
		s->cmpConsIdx++;
	}
}
