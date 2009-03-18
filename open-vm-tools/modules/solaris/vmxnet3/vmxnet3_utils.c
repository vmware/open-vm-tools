/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 
#include "vmxnet3_solaris.h"

/* This symbol is needed by the Solaris dynamic loader */
char _depends_on[] = {"misc/mac"};

/* Used by ddi_regs_map_setup() and ddi_dma_mem_alloc() */
ddi_device_acc_attr_t vmxnet3_dev_attr = {
   DDI_DEVICE_ATTR_V0,
   DDI_STRUCTURE_LE_ACC,
   DDI_STRICTORDER_ACC
};

/* Buffers with no alignment constraint DMA description */
static ddi_dma_attr_t vmxnet3_dma_attrs_1 = {
   DMA_ATTR_V0,           /* dma_attr_version */
   0x0000000000000000ull, /* dma_attr_addr_lo */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_addr_hi */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_count_max */
   0x0000000000000001ull, /* dma_attr_align */
   0x0000000000000001ull, /* dma_attr_burstsizes */
   0x00000001,            /* dma_attr_minxfer */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_maxxfer */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_seg */
   1,                     /* dma_attr_sgllen */
   0x00000001,            /* dma_attr_granular */
   0                      /* dma_attr_flags */
};

/* Buffers with a 128-bytes alignment constraint DMA description */
static ddi_dma_attr_t vmxnet3_dma_attrs_128 = {
   DMA_ATTR_V0,           /* dma_attr_version */
   0x0000000000000000ull, /* dma_attr_addr_lo */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_addr_hi */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_count_max */
   0x0000000000000080ull, /* dma_attr_align */
   0x0000000000000001ull, /* dma_attr_burstsizes */
   0x00000001,            /* dma_attr_minxfer */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_maxxfer */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_seg */
   1,                     /* dma_attr_sgllen */
   0x00000001,            /* dma_attr_granular */
   0                      /* dma_attr_flags */
};

/* Buffers with a 512-bytes alignment constraint DMA description */
static ddi_dma_attr_t vmxnet3_dma_attrs_512 = {
   DMA_ATTR_V0,           /* dma_attr_version */
   0x0000000000000000ull, /* dma_attr_addr_lo */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_addr_hi */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_count_max */
   0x0000000000000200ull, /* dma_attr_align */
   0x0000000000000001ull, /* dma_attr_burstsizes */
   0x00000001,            /* dma_attr_minxfer */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_maxxfer */
   0xFFFFFFFFFFFFFFFFull, /* dma_attr_seg */
   1,                     /* dma_attr_sgllen */
   0x00000001,            /* dma_attr_granular */
   0                      /* dma_attr_flags */
};

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_alloc_dma_mem --
 *
 *    Allocate /size/ bytes of contiguous DMA-ble memory.
 *
 * Results:
 *    DDI_SUCCESS or DDI_FAILURE.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
static int
vmxnet3_alloc_dma_mem(vmxnet3_softc_t *dp, vmxnet3_dmabuf_t *dma,
                      size_t size, boolean_t canSleep,
                      ddi_dma_attr_t *dma_attrs)
{
   ddi_dma_cookie_t cookie;
   uint_t cookieCount;
   int (*cb) (caddr_t) = canSleep ? DDI_DMA_SLEEP : DDI_DMA_DONTWAIT;

   ASSERT(size != 0);

   /*
    * Allocate a DMA handle
    */
   if (ddi_dma_alloc_handle(dp->dip, dma_attrs, cb, NULL,
                            &dma->dmaHandle) != DDI_SUCCESS) {
      VMXNET3_WARN(dp, "ddi_dma_alloc_handle() failed\n");
      goto error;
   }

   /*
    * Allocate memory
    */
   if (ddi_dma_mem_alloc(dma->dmaHandle, size, &vmxnet3_dev_attr,
                         DDI_DMA_CONSISTENT, cb, NULL, &dma->buf,
                         &dma->bufLen, &dma->dataHandle) != DDI_SUCCESS) {
      VMXNET3_WARN(dp, "ddi_dma_mem_alloc() failed\n");
      goto error_dma_handle;
   }

   /*
    * Map the memory
    */
   if (ddi_dma_addr_bind_handle(dma->dmaHandle, NULL,
                                dma->buf, dma->bufLen,
                                DDI_DMA_RDWR | DDI_DMA_STREAMING,
                                cb, NULL, &cookie,
                                &cookieCount) != DDI_DMA_MAPPED) {
      VMXNET3_WARN(dp, "ddi_dma_addr_bind_handle() failed\n");
      goto error_dma_mem;
   }

   ASSERT(cookieCount == 1);
   dma->bufPA = cookie.dmac_laddress;

   return DDI_SUCCESS;

error_dma_mem:
   ddi_dma_mem_free(&dma->dataHandle);
error_dma_handle:
   ddi_dma_free_handle(&dma->dmaHandle);
error:
   dma->buf = NULL;
   dma->bufPA = NULL;
   dma->bufLen = 0;
   return DDI_FAILURE;
}

int
vmxnet3_alloc_dma_mem_1(vmxnet3_softc_t *dp, vmxnet3_dmabuf_t *dma,
                        size_t size, boolean_t canSleep)
{
   return vmxnet3_alloc_dma_mem(dp, dma, size, canSleep,
                                &vmxnet3_dma_attrs_1);
}

int
vmxnet3_alloc_dma_mem_512(vmxnet3_softc_t *dp, vmxnet3_dmabuf_t *dma,
                          size_t size, boolean_t canSleep)
{
   return vmxnet3_alloc_dma_mem(dp, dma, size, canSleep,
                                &vmxnet3_dma_attrs_512);
}

int
vmxnet3_alloc_dma_mem_128(vmxnet3_softc_t *dp, vmxnet3_dmabuf_t *dma,
                          size_t size, boolean_t canSleep)
{
   return vmxnet3_alloc_dma_mem(dp, dma, size, canSleep,
                                &vmxnet3_dma_attrs_128);
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_free_dma_mem --
 *
 *    Free DMA-ble memory.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
void
vmxnet3_free_dma_mem(vmxnet3_dmabuf_t *dma)
{
   ddi_dma_unbind_handle(dma->dmaHandle);
   ddi_dma_mem_free(&dma->dataHandle);
   ddi_dma_free_handle(&dma->dmaHandle);

   dma->buf = NULL;
   dma->bufPA = NULL;
   dma->bufLen = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_getprop --
 *
 *    Get the numeric value of the property "name" in vmxnet3s.conf for
 *    the corresponding device instance.
 *    If the property isn't found or if it doesn't satisfy the conditions,
 *    "def" is returned.
 *
 * Results:
 *    The value of the property or "def".
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
int
vmxnet3_getprop(vmxnet3_softc_t *dp, char *name,
                int min, int max, int def)
{
   int ret = def;
   int *props;
   uint_t nprops;

   if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dp->dip, DDI_PROP_DONTPASS,
                                 name, &props, &nprops) == DDI_PROP_SUCCESS) {
      if (dp->instance < nprops) {
         ret = props[dp->instance];
      } else {
         VMXNET3_WARN(dp, "property %s not available for this device\n",
                      name);
      }
      ddi_prop_free(props);
   }

   if (ret < min || ret > max) {
      ASSERT(def >= min && def <= max);
      VMXNET3_WARN(dp, "property %s invalid (%d <= %d <= %d)\n",
                   name, min, ret, max);
      ret = def;
   }

   VMXNET3_DEBUG(dp, 2, "getprop(%s) -> %d\n", name, ret);

   return ret;
}
