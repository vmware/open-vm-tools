/*********************************************************
 * Copyright (C) 2007,2017-2019 VMware, Inc. All rights reserved.
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

typedef enum vmxnet3_txstatus {
   VMXNET3_TX_OK,
   VMXNET3_TX_FAILURE,
   VMXNET3_TX_PULLUP,
   VMXNET3_TX_RINGFULL
} vmxnet3_txstatus;

typedef struct vmxnet3_offload_t {
   uint16_t om;
   uint16_t hlen;
   uint16_t msscof;
} vmxnet3_offload_t;

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_txqueue_init --
 *
 *    Initialize a TxQueue. Currently nothing needs to be done.
 *
 * Results:
 *    DDI_SUCCESS.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
int
vmxnet3_txqueue_init(vmxnet3_softc_t *dp, vmxnet3_txqueue_t *txq)
{
   return DDI_SUCCESS;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_txqueue_fini --
 *
 *    Finish a TxQueue by freeing all pending Tx.
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
vmxnet3_txqueue_fini(vmxnet3_softc_t *dp, vmxnet3_txqueue_t *txq)
{
   unsigned int i;

   ASSERT(!dp->devEnabled);

   for (i = 0; i < txq->cmdRing.size; i++) {
      mblk_t *mp = txq->metaRing[i].mp;
      if (mp) {
         freemsg(mp);
      }
   }
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_tx_prepare_offload --
 *
 *    Build the offload context of a msg.
 *
 * Results:
 *    0 if everything went well.
 *    +n if n bytes need to be pulled up.
 *    -1 in case of error (not used).
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
static int
vmxnet3_tx_prepare_offload(vmxnet3_softc_t *dp,
                           vmxnet3_offload_t *ol,
                           mblk_t *mp)
{
   int ret = 0;
   uint32_t start, stuff, value, flags, lso_flag, mss;

   ol->om = VMXNET3_OM_NONE;
   ol->hlen = 0;
   ol->msscof = 0;

   hcksum_retrieve(mp, NULL, NULL, &start, &stuff, NULL, &value, &flags);
   mac_lso_get(mp, &mss, &lso_flag);

   if (flags || lso_flag) {
      struct ether_vlan_header *eth = (void *) mp->b_rptr;
      uint8_t ethLen;

      if (eth->ether_tpid == htons(ETHERTYPE_VLAN)) {
         ethLen = sizeof(struct ether_vlan_header);
      } else {
         ethLen = sizeof(struct ether_header);
      }

      VMXNET3_DEBUG(dp, 4, "flags=0x%x, ethLen=%u, start=%u, stuff=%u, value=%u\n",
                            flags,      ethLen,    start,    stuff,    value);

      if (lso_flag & HW_LSO) {
         mblk_t *mblk = mp;
         uint8_t *ip, *tcp;
         uint8_t ipLen, tcpLen;

         /*
          * Copy e1000g's behavior:
          * - Do not assume all the headers are in the same mblk.
          * - Assume each header is always within one mblk.
          * - Assume the ethernet header is in the first mblk.
          */
         ip = mblk->b_rptr + ethLen;
         if (ip >= mblk->b_wptr) {
            mblk = mblk->b_cont;
            ip = mblk->b_rptr;
         }
         ipLen = IPH_HDR_LENGTH((ipha_t *) ip);
         tcp = ip + ipLen;
         if (tcp >= mblk->b_wptr) {
            mblk = mblk->b_cont;
            tcp = mblk->b_rptr;
         }
         tcpLen = TCP_HDR_LENGTH((tcph_t *) tcp);
         if (tcp + tcpLen > mblk->b_wptr) { // careful, '>' instead of '>=' here
            mblk = mblk->b_cont;
         }

         ol->om = VMXNET3_OM_TSO;
         ol->hlen = ethLen + ipLen + tcpLen;
         ol->msscof = mss;

         if (mblk != mp) {
            ret = ol->hlen;
         }
      } else if (flags & HCK_PARTIALCKSUM) {
         ol->om = VMXNET3_OM_CSUM;
         ol->hlen = start + ethLen;
         ol->msscof = stuff + ethLen;
      }

   }

   return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_tx_one --
 *
 *    Map a msg into the Tx command ring of a vmxnet3 device.
 *
 * Results:
 *    VMXNET3_TX_OK if everything went well.
 *    VMXNET3_TX_RINGFULL if the ring is nearly full.
 *    VMXNET3_TX_PULLUP if the msg is overfragmented.
 *    VMXNET3_TX_FAILURE if there was a DMA or offload error.
 *
 * Side effects:
 *    The ring is filled if VMXNET3_TX_OK is returned.
 *
 *---------------------------------------------------------------------------
 */
static vmxnet3_txstatus
vmxnet3_tx_one(vmxnet3_softc_t *dp,
               vmxnet3_txqueue_t *txq,
               vmxnet3_offload_t *ol,
               mblk_t *mp,
               boolean_t retry)
{
   int ret = VMXNET3_TX_OK;
   unsigned int frags = 0, totLen = 0;
   vmxnet3_cmdring_t *cmdRing = &txq->cmdRing;
   Vmxnet3_TxQueueCtrl *txqCtrl = txq->sharedCtrl;
   Vmxnet3_GenericDesc *txDesc;
   uint16_t sopIdx, eopIdx;
   uint8_t sopGen, curGen;
   mblk_t *mblk;

   ASSERT(mutex_owned(&dp->txLock));

   sopIdx = eopIdx = cmdRing->next2fill;
   sopGen = cmdRing->gen;
   curGen = !cmdRing->gen;

   for (mblk = mp; mblk != NULL; mblk = mblk->b_cont) {
      unsigned int len = MBLKL(mblk);
      ddi_dma_cookie_t cookie;
      uint_t cookieCount;

      if (len) {
         totLen += len;
      } else {
         continue;
      }

      if (ddi_dma_addr_bind_handle(dp->txDmaHandle, NULL,
                                   (caddr_t) mblk->b_rptr, len,
                                   DDI_DMA_RDWR | DDI_DMA_STREAMING,
                                   DDI_DMA_DONTWAIT, NULL,
                                   &cookie, &cookieCount) != DDI_DMA_MAPPED) {
         VMXNET3_WARN(dp, "ddi_dma_addr_bind_handle() failed\n");
         ret = VMXNET3_TX_FAILURE;
         goto error;
      }

      ASSERT(cookieCount);

      do {
         uint64_t addr = cookie.dmac_laddress;
         size_t len = cookie.dmac_size;

         do {
            uint32_t dw2, dw3;
            size_t chunkLen;

            ASSERT(!txq->metaRing[eopIdx].mp);
            ASSERT(cmdRing->avail - frags);

            if (frags >= cmdRing->size - 1 ||
                (ol->om != VMXNET3_OM_TSO && frags >= VMXNET3_MAX_TXD_PER_PKT)) {

               if (retry) {
                  VMXNET3_DEBUG(dp, 2, "overfragmented, frags=%u ring=%hu om=%hu\n",
                                frags, cmdRing->size, ol->om);
               }
               ddi_dma_unbind_handle(dp->txDmaHandle);
               ret = VMXNET3_TX_PULLUP;
               goto error;
            }
            if (cmdRing->avail - frags <= 1) {
               dp->txMustResched = B_TRUE;
               ddi_dma_unbind_handle(dp->txDmaHandle);
               ret = VMXNET3_TX_RINGFULL;
               goto error;
            }

            if (len > VMXNET3_MAX_TX_BUF_SIZE) {
               chunkLen = VMXNET3_MAX_TX_BUF_SIZE;
            } else {
               chunkLen = len;
            }

            frags++;
            eopIdx = cmdRing->next2fill;

            txDesc = VMXNET3_GET_DESC(cmdRing, eopIdx);
            ASSERT(txDesc->txd.gen != cmdRing->gen);

            // txd.addr
            txDesc->txd.addr = addr;
            // txd.dw2
            dw2 = chunkLen == VMXNET3_MAX_TX_BUF_SIZE ? 0 : chunkLen;
            dw2 |= curGen << VMXNET3_TXD_GEN_SHIFT;
            txDesc->dword[2] = dw2;
            ASSERT(txDesc->txd.len == len || txDesc->txd.len == 0);
            // txd.dw3
            dw3 = 0;
            txDesc->dword[3] = dw3;

            VMXNET3_INC_RING_IDX(cmdRing, cmdRing->next2fill);
            curGen = cmdRing->gen;

            addr += chunkLen;
            len -= chunkLen;
         } while (len);

         if (--cookieCount) {
            ddi_dma_nextcookie(dp->txDmaHandle, &cookie);
         }
      } while (cookieCount);

      ddi_dma_unbind_handle(dp->txDmaHandle);
   }

   /* Update the EOP descriptor */
   txDesc = VMXNET3_GET_DESC(cmdRing, eopIdx);
   txDesc->dword[3] |= VMXNET3_TXD_CQ | VMXNET3_TXD_EOP;

   /* Update the SOP descriptor. Must be done last */
   txDesc = VMXNET3_GET_DESC(cmdRing, sopIdx);
   if (ol->om == VMXNET3_OM_TSO &&
       txDesc->txd.len != 0 &&
       txDesc->txd.len < ol->hlen) {
      ret = VMXNET3_TX_FAILURE;
      goto error;
   }
   txDesc->txd.om = ol->om;
   txDesc->txd.hlen = ol->hlen;
   txDesc->txd.msscof = ol->msscof;
   membar_producer();
   txDesc->txd.gen = sopGen;

   /* Update the meta ring & metadata */
   txq->metaRing[sopIdx].mp = mp;
   txq->metaRing[eopIdx].sopIdx = sopIdx;
   txq->metaRing[eopIdx].frags = frags;
   cmdRing->avail -= frags;
   if (ol->om == VMXNET3_OM_TSO) {
      txqCtrl->txNumDeferred +=
         (totLen - ol->hlen + ol->msscof - 1) / ol->msscof;
   } else {
      txqCtrl->txNumDeferred++;
   }

   VMXNET3_DEBUG(dp, 3, "tx 0x%p on [%u;%u]\n", mp, sopIdx, eopIdx);

   goto done;

error:
   /* Reverse the generation bits */
   while (sopIdx != cmdRing->next2fill) {
      VMXNET3_DEC_RING_IDX(cmdRing, cmdRing->next2fill);
      txDesc = VMXNET3_GET_DESC(cmdRing, cmdRing->next2fill);
      txDesc->txd.gen = !cmdRing->gen;
   }

done:
   return ret;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_tx --
 *
 *    Send packets on a vmxnet3 device.
 *
 * Results:
 *    NULL in case of success or failure.
 *    The mps to be retransmitted later if the ring is full.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
mblk_t *
vmxnet3_tx(void *data, mblk_t *mps)
{
   vmxnet3_softc_t *dp = data;
   vmxnet3_txqueue_t *txq = &dp->txQueue;
   vmxnet3_cmdring_t *cmdRing = &txq->cmdRing;
   Vmxnet3_TxQueueCtrl *txqCtrl = txq->sharedCtrl;
   vmxnet3_txstatus status = VMXNET3_TX_OK;
   mblk_t *mp;

   ASSERT(mps != NULL);
   mutex_enter(&dp->txLock);
   if (!dp->devEnabled) {
      goto done;
   }

   do {
      vmxnet3_offload_t ol;
      int pullup;

      mp = mps;
      mps = mp->b_next;
      mp->b_next = NULL;

      if (DB_TYPE(mp) != M_DATA) {
         /*
          * PR #315560: Solaris might pass M_PROTO mblks for some reason.
          * Drop them because we don't understand them and because their
          * contents are not Ethernet frames anyway.
          */
         ASSERT(B_FALSE);
         freemsg(mp);
         continue;
      }

      /*
       * Prepare the offload while we're still handling the original
       * message -- msgpullup() discards the metadata afterwards.
       */
      pullup = vmxnet3_tx_prepare_offload(dp, &ol, mp);
      if (pullup) {
         mblk_t *new_mp = msgpullup(mp, pullup);
         freemsg(mp);
         if (new_mp) {
            mp = new_mp;
         } else {
            continue;
         }
      }

      /*
       * Try to map the message in the Tx ring.
       * This call might fail for non-fatal reasons.
       */
      status = vmxnet3_tx_one(dp, txq, &ol, mp, B_FALSE);
      if (status == VMXNET3_TX_PULLUP) {
         /*
          * Try one more time after flattening
          * the message with msgpullup().
          */
         if (mp->b_cont != NULL) {
            mblk_t *new_mp = msgpullup(mp, -1);
            freemsg(mp);
            if (new_mp) {
               mp = new_mp;
               status = vmxnet3_tx_one(dp, txq, &ol, mp, B_TRUE);
            } else {
               continue;
            }
         }
      }
      if (status != VMXNET3_TX_OK && status != VMXNET3_TX_RINGFULL) {
         /* Fatal failure, drop it */
         freemsg(mp);
      }
   } while (mps && status != VMXNET3_TX_RINGFULL);

   if (status == VMXNET3_TX_RINGFULL) {
      mp->b_next = mps;
      mps = mp;
   } else {
      ASSERT(!mps);
   }

   /* Notify the device */
   if (txqCtrl->txNumDeferred >= txqCtrl->txThreshold) {
      txqCtrl->txNumDeferred = 0;
      VMXNET3_BAR0_PUT32(dp, VMXNET3_REG_TXPROD, cmdRing->next2fill);
   }

done:
   mutex_exit(&dp->txLock);

   return mps;
}

/*
 *---------------------------------------------------------------------------
 *
 * vmxnet3_tx_complete --
 *
 *    Parse a transmit queue and complete packets.
 *
 * Results:
 *    B_TRUE if Tx must be updated or B_FALSE if no action is required.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------
 */
boolean_t
vmxnet3_tx_complete(vmxnet3_softc_t *dp, vmxnet3_txqueue_t *txq)
{
   vmxnet3_cmdring_t *cmdRing = &txq->cmdRing;
   vmxnet3_compring_t *compRing = &txq->compRing;
   Vmxnet3_GenericDesc *compDesc;
   boolean_t completedTx = B_FALSE;
   boolean_t ret = B_FALSE;

   mutex_enter(&dp->txLock);

   compDesc = VMXNET3_GET_DESC(compRing, compRing->next2comp);
   while (compDesc->tcd.gen == compRing->gen) {
      vmxnet3_metatx_t *sopMetaDesc, *eopMetaDesc;
      uint16_t sopIdx, eopIdx;
      mblk_t *mp;

      eopIdx = compDesc->tcd.txdIdx;
      eopMetaDesc = &txq->metaRing[eopIdx];
      sopIdx = eopMetaDesc->sopIdx;
      sopMetaDesc = &txq->metaRing[sopIdx];

      ASSERT(eopMetaDesc->frags);
      cmdRing->avail += eopMetaDesc->frags;

      ASSERT(sopMetaDesc->mp);
      mp = sopMetaDesc->mp;
      freemsg(mp);

      eopMetaDesc->sopIdx = 0;
      eopMetaDesc->frags = 0;
      sopMetaDesc->mp = NULL;

      completedTx = B_TRUE;

      VMXNET3_DEBUG(dp, 3, "cp 0x%p on [%u;%u]\n", mp, sopIdx, eopIdx);

      VMXNET3_INC_RING_IDX(compRing, compRing->next2comp);
      compDesc = VMXNET3_GET_DESC(compRing, compRing->next2comp);
   }

   if (dp->txMustResched && completedTx) {
      dp->txMustResched = B_FALSE;
      ret = B_TRUE;
   }

   mutex_exit(&dp->txLock);

   return ret;
}
