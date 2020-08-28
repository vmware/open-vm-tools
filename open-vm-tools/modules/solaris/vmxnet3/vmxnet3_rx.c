/*********************************************************
 * Copyright (C) 2007,2019 VMware, Inc. All rights reserved.
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

static void vmxnet3_put_rxbuf(vmxnet3_rxbuf_t *rxBuf);

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_alloc_rxbuf --
 *
 *    Allocate a new rxBuf from memory. All its fields are set except
 *    for its associated mblk which has to be allocated later.
 *
 * Results:
 *    A new rxBuf or NULL.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
static vmxnet3_rxbuf_t *
vmxnet3_alloc_rxbuf(vmxnet3_softc_t *dp, boolean_t canSleep)
{
   vmxnet3_rxbuf_t *rxBuf;
   int flag = canSleep ? KM_SLEEP : KM_NOSLEEP;
   int err;

   rxBuf = kmem_zalloc(sizeof(vmxnet3_rxbuf_t), flag);
   if (!rxBuf) {
      return NULL;
   }

   if ((err = vmxnet3_alloc_dma_mem_1(dp, &rxBuf->dma, (dp->cur_mtu + 18),
                                      canSleep)) != DDI_SUCCESS) {

      VMXNET3_DEBUG(dp, 0, "Failed to allocate %d bytes for rx buf, err:%d.\n",
                    (dp->cur_mtu + 18), err);
      kmem_free(rxBuf, sizeof(vmxnet3_rxbuf_t));
      return NULL;
   }

   rxBuf->freeCB.free_func = vmxnet3_put_rxbuf;
   rxBuf->freeCB.free_arg = (caddr_t) rxBuf;
   rxBuf->dp = dp;

   atomic_inc_32(&dp->rxNumBufs);

   return rxBuf;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_free_rxbuf --
 *
 *    Free a rxBuf.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
static void
vmxnet3_free_rxbuf(vmxnet3_softc_t *dp, vmxnet3_rxbuf_t *rxBuf)
{
   vmxnet3_free_dma_mem(&rxBuf->dma);
   kmem_free(rxBuf, sizeof(vmxnet3_rxbuf_t));

#ifndef DEBUG
   atomic_dec_32(&dp->rxNumBufs);
#else
   {
      uint32_t nv = atomic_dec_32_nv(&dp->rxNumBufs);
      ASSERT(nv != -1);
   }
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_put_rxbuf --
 *
 *    Return a rxBuf to the pool or free it.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
static void
vmxnet3_put_rxbuf(vmxnet3_rxbuf_t *rxBuf)
{
   vmxnet3_softc_t *dp = rxBuf->dp;
   vmxnet3_rxpool_t *rxPool = &dp->rxPool;

   VMXNET3_DEBUG(dp, 5, "free 0x%p\n", rxBuf);

   mutex_enter(&dp->rxPoolLock);
   if (dp->devEnabled && rxPool->nBufs < rxPool->nBufsLimit) {
      rxBuf->next = rxPool->listHead;
      rxPool->listHead = rxBuf;
      mutex_exit(&dp->rxPoolLock);
   } else {
      mutex_exit(&dp->rxPoolLock);
      vmxnet3_free_rxbuf(dp, rxBuf);
   }
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_get_rxbuf --
 *
 *    Get an unused rxBuf from either the pool or from memory.
 *    The returned rxBuf has a mblk associated with it.
 *
 * Results:
 *    A rxBuf or NULL.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
static vmxnet3_rxbuf_t *
vmxnet3_get_rxbuf(vmxnet3_softc_t *dp, boolean_t canSleep)
{
   vmxnet3_rxbuf_t *rxBuf;
   vmxnet3_rxpool_t *rxPool = &dp->rxPool;

   mutex_enter(&dp->rxPoolLock);
   if (rxPool->listHead) {
      rxBuf = rxPool->listHead;
      rxPool->listHead = rxBuf->next;
      mutex_exit(&dp->rxPoolLock);
      VMXNET3_DEBUG(dp, 5, "alloc 0x%p from pool\n", rxBuf);
   } else {
      mutex_exit(&dp->rxPoolLock);
      rxBuf = vmxnet3_alloc_rxbuf(dp, canSleep);
      if (!rxBuf) {
         goto done;
      }
      VMXNET3_DEBUG(dp, 5, "alloc 0x%p from mem\n", rxBuf);
   }

   ASSERT(rxBuf);

   rxBuf->mblk = desballoc((uchar_t *) rxBuf->dma.buf,
                           rxBuf->dma.bufLen, BPRI_MED,
                           &rxBuf->freeCB);
   if (!rxBuf->mblk) {
      vmxnet3_put_rxbuf(rxBuf);
      rxBuf = NULL;
   }

done:
   return rxBuf;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_rx_populate --
 *
 *    Populate a Rx descriptor with a new rxBuf.
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
vmxnet3_rx_populate(vmxnet3_softc_t *dp, vmxnet3_rxqueue_t *rxq,
                    uint16_t idx, boolean_t canSleep)
{
   int ret = DDI_SUCCESS;
   vmxnet3_rxbuf_t *rxBuf = vmxnet3_get_rxbuf(dp, canSleep);

   if (rxBuf) {
      vmxnet3_cmdring_t *cmdRing = &rxq->cmdRing;
      Vmxnet3_GenericDesc *rxDesc = VMXNET3_GET_DESC(cmdRing, idx);;

      rxq->bufRing[idx].rxBuf = rxBuf;
      rxDesc->rxd.addr = rxBuf->dma.bufPA;
      rxDesc->rxd.len = rxBuf->dma.bufLen;
      // rxDesc->rxd.btype = 0;
      membar_producer();
      rxDesc->rxd.gen = cmdRing->gen;
   } else {
      ret = DDI_FAILURE;
   }

   return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_rxqueue_init --
 *
 *    Initialize a RxQueue by populating the whole Rx ring with rxBufs.
 *
 * Results:
 *    DDI_SUCCESS or DDI_FAILURE.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
int
vmxnet3_rxqueue_init(vmxnet3_softc_t *dp, vmxnet3_rxqueue_t *rxq)
{
   vmxnet3_cmdring_t *cmdRing = &rxq->cmdRing;

   do {
      if (vmxnet3_rx_populate(dp, rxq, cmdRing->next2fill,
                              B_TRUE) != DDI_SUCCESS) {
         goto error;
      }
      VMXNET3_INC_RING_IDX(cmdRing, cmdRing->next2fill);
   } while (cmdRing->next2fill);

   dp->rxPool.nBufsLimit = vmxnet3_getprop(dp, "RxBufPoolLimit",
                                           0, cmdRing->size * 10,
                                           cmdRing->size * 2);

   return DDI_SUCCESS;

error:
   while (cmdRing->next2fill) {
      VMXNET3_DEC_RING_IDX(cmdRing, cmdRing->next2fill);
      vmxnet3_free_rxbuf(dp, rxq->bufRing[cmdRing->next2fill].rxBuf);
   }

   return DDI_FAILURE;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_rxqueue_fini --
 *
 *    Finish a RxQueue by freeing all the related rxBufs.
 *
 * Results:
 *    DDI_SUCCESS.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
void
vmxnet3_rxqueue_fini(vmxnet3_softc_t *dp, vmxnet3_rxqueue_t *rxq)
{
   vmxnet3_rxpool_t *rxPool = &dp->rxPool;
   vmxnet3_rxbuf_t *rxBuf;
   unsigned int i;

   ASSERT(!dp->devEnabled);

   /* First the rxPool */
   while (rxPool->listHead) {
      rxBuf = rxPool->listHead;
      rxPool->listHead = rxBuf->next;
      vmxnet3_free_rxbuf(dp, rxBuf);
   }

   /* Then the ring */
   for (i = 0; i < rxq->cmdRing.size; i++) {
      rxBuf = rxq->bufRing[i].rxBuf;
      ASSERT(rxBuf);
      ASSERT(rxBuf->mblk);
      /*
       * Here, freemsg() will trigger a call to vmxnet3_put_rxbuf() which
       * will then call vmxnet3_free_rxbuf() because the underlying
       * device is disabled.
       */
      freemsg(rxBuf->mblk);
   }
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_rx_hwcksum --
 *
 *    Determine if a received packet was checksummed by the Vmxnet3
 *    device and tag the mp appropriately.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The mp may get tagged.
 *
 *---------------------------------------------------------------------------
 */
static void
vmxnet3_rx_hwcksum(vmxnet3_softc_t *dp, mblk_t *mp,
                   Vmxnet3_GenericDesc *compDesc)
{
   if (!compDesc->rcd.cnc) {
      uint32_t flags = 0;

      if (compDesc->rcd.v4 && compDesc->rcd.ipc) {
         flags |= HCK_IPV4_HDRCKSUM;
         if ((compDesc->rcd.tcp || compDesc->rcd.udp) &&
              compDesc->rcd.tuc) {
            flags |= HCK_FULLCKSUM | HCK_FULLCKSUM_OK;
         }
      }

      VMXNET3_DEBUG(dp, 3, "rx cksum flags = 0x%x\n", flags);

      hcksum_assoc(mp, NULL, NULL, 0, 0, 0, 0, flags, 0);
   }
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_rx_intr --
 *
 *    Interrupt handler for Rx. Look if there are any pending Rx and
 *    put them in mplist.
 *
 * Results:
 *    A list of messages to pass to the MAC subystem.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
mblk_t *
vmxnet3_rx_intr(vmxnet3_softc_t *dp, vmxnet3_rxqueue_t *rxq)
{
   vmxnet3_compring_t *compRing = &rxq->compRing;
   vmxnet3_cmdring_t *cmdRing = &rxq->cmdRing;
   Vmxnet3_RxQueueCtrl *rxqCtrl = rxq->sharedCtrl;
   Vmxnet3_GenericDesc *compDesc;
   mblk_t *mplist = NULL, **mplistTail = &mplist;

   ASSERT(mutex_owned(&dp->intrLock));

   compDesc = VMXNET3_GET_DESC(compRing, compRing->next2comp);
   while (compDesc->rcd.gen == compRing->gen) {
      mblk_t *mp = NULL, **mpTail = &mp;
      boolean_t mpValid = B_TRUE;
      boolean_t eop;

      ASSERT(compDesc->rcd.sop);

      do {
         uint16_t rxdIdx = compDesc->rcd.rxdIdx;
         vmxnet3_rxbuf_t *rxBuf = rxq->bufRing[rxdIdx].rxBuf;
         mblk_t *mblk = rxBuf->mblk;
         Vmxnet3_GenericDesc *rxDesc;

         while (compDesc->rcd.gen != compRing->gen) {
            /*
             * H/W may be still be in the middle of generating this entry,
             * so hold on until the gen bit is flipped.
             */
            membar_consumer();
         }
         ASSERT(compDesc->rcd.gen == compRing->gen);
         ASSERT(rxBuf);
         ASSERT(mblk);

         /* Some Rx descriptors may have been skipped */
         while (cmdRing->next2fill != rxdIdx) {
            rxDesc = VMXNET3_GET_DESC(cmdRing, cmdRing->next2fill);
            rxDesc->rxd.gen = cmdRing->gen;
            VMXNET3_INC_RING_IDX(cmdRing, cmdRing->next2fill);
         }

         eop = compDesc->rcd.eop;

         /*
          * Now we have a piece of the packet in the rxdIdx descriptor.
          * Grab it only if we achieve to replace it with a fresh buffer.
          */
         if (vmxnet3_rx_populate(dp, rxq, rxdIdx, B_FALSE) == DDI_SUCCESS) {
            /* Success, we can chain the mblk with the mp */
            mblk->b_wptr = mblk->b_rptr + compDesc->rcd.len;
            *mpTail = mblk;
            mpTail = &mblk->b_cont;
            ASSERT(*mpTail == NULL);

            VMXNET3_DEBUG(dp, 3, "rx 0x%p on [%u]\n", mblk, rxdIdx);

            if (eop) {
               if (!compDesc->rcd.err) {
                  /* Tag the mp if it was checksummed by the H/W */
                  vmxnet3_rx_hwcksum(dp, mp, compDesc);
               } else {
                  mpValid = B_FALSE;
               }
            }
         } else {
            /* Keep the same buffer, we still need to flip the gen bit */
            rxDesc = VMXNET3_GET_DESC(cmdRing, rxdIdx);
            rxDesc->rxd.gen = cmdRing->gen;
            mpValid = B_FALSE;
         }

         VMXNET3_INC_RING_IDX(compRing, compRing->next2comp);
         VMXNET3_INC_RING_IDX(cmdRing, cmdRing->next2fill);
         compDesc = VMXNET3_GET_DESC(compRing, compRing->next2comp);
      } while (!eop);

      if (mp) {
         if (mpValid) {
            *mplistTail = mp;
            mplistTail = &mp->b_next;
            ASSERT(*mplistTail == NULL);
         } else {
            /* This message got holes, drop it */
            freemsg(mp);
         }
      }
   }

   if (rxqCtrl->updateRxProd) {
      uint32_t rxprod;

      /*
       * All buffers are actually available, but we can't tell that to
       * the device because it may interpret that as an empty ring.
       * So skip one buffer.
       */
      if (cmdRing->next2fill) {
         rxprod = cmdRing->next2fill - 1;
      } else {
         rxprod = cmdRing->size - 1;
      }
      VMXNET3_BAR0_PUT32(dp, VMXNET3_REG_RXPROD, rxprod);
   }

   return mplist;
}
