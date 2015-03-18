/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * stats.h --
 *
 *      Stats functions for Linux vsock module.
 */

#ifndef __STATS_H__
#define __STATS_H__

#include "driver-config.h"

#include "vm_basic_math.h"

#include "vsockCommon.h"
#include "vsockPacket.h"


/*
 * Define VSOCK_GATHER_STATISTICS to turn on statistics gathering.
 * Currently this consists of 3 types of stats:
 * 1. The number of control datagram messages sent.
 * 2. The level of queuepair fullness (in 10% buckets) whenever data is
 *    about to be enqueued or dequeued from the queuepair.
 * 3. The total number of bytes enqueued/dequeued.
 */

//#define VSOCK_GATHER_STATISTICS 1

#ifdef VSOCK_GATHER_STATISTICS

#define VSOCK_NUM_QUEUE_LEVEL_BUCKETS 10
extern uint64 vSockStatsCtlPktCount[VSOCK_PACKET_TYPE_MAX];
extern uint64 vSockStatsConsumeQueueHist[VSOCK_NUM_QUEUE_LEVEL_BUCKETS];
extern uint64 vSockStatsProduceQueueHist[VSOCK_NUM_QUEUE_LEVEL_BUCKETS];
extern Atomic_uint64 vSockStatsConsumeTotal;
extern Atomic_uint64 vSockStatsProduceTotal;

#define VSOCK_STATS_STREAM_CONSUME_HIST(vsk)                            \
   VSockVmciStatsUpdateQueueBucketCount((vsk)->qpair,                   \
                                        (vsk)->consumeSize,             \
                               VMCIQPair_ConsumeBufReady((vsk)->qpair), \
                                        vSockStatsConsumeQueueHist)
#define VSOCK_STATS_STREAM_PRODUCE_HIST(vsk)                            \
   VSockVmciStatsUpdateQueueBucketCount((vsk)->qpair,                   \
                                        (vsk)->produceSize,             \
                               VMCIQPair_ProduceBufReady((vsk)->qpair), \
                                        vSockStatsProduceQueueHist)
#define VSOCK_STATS_CTLPKT_LOG(pktType)                                 \
   do {                                                                 \
      ++vSockStatsCtlPktCount[pktType];                                 \
   } while (0)
#define VSOCK_STATS_STREAM_CONSUME(bytes)                               \
   Atomic_ReadAdd64(&vSockStatsConsumeTotal, bytes)
#define VSOCK_STATS_STREAM_PRODUCE(bytes)                               \
   Atomic_ReadAdd64(&vSockStatsProduceTotal, bytes)
#define VSOCK_STATS_CTLPKT_DUMP_ALL() VSockVmciStatsCtlPktDumpAll()
#define VSOCK_STATS_HIST_DUMP_ALL()   VSockVmciStatsHistDumpAll()
#define VSOCK_STATS_TOTALS_DUMP_ALL() VSockVmciStatsTotalsDumpAll()
#define VSOCK_STATS_RESET()           VSockVmciStatsReset()

/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStatsUpdateQueueBucketCount --
 *
 *      Given a queue, determine how much data is enqueued and add that to
 *      the specified queue level statistic bucket.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciStatsUpdateQueueBucketCount(VMCIQPair *qpair,   // IN
                                     uint64 queueSize,   // IN
                                     uint64 dataReady,   // IN
                                     uint64 queueHist[]) // IN/OUT
{
   uint64 bucket = 0;
   uint32 remainder = 0;

   ASSERT(qpair);
   ASSERT(queueHist);

   /*
    * We can't do 64 / 64 = 64 bit divides on linux because it requires a
    * libgcc which is not linked into the kernel module. Since this code is
    * only used by developers we just limit the queueSize to be less than
    * MAX_UINT for now.
    */
   ASSERT(queueSize <= MAX_UINT32);
   Div643264(dataReady * 10, queueSize, &bucket, &remainder);
   ASSERT(bucket < VSOCK_NUM_QUEUE_LEVEL_BUCKETS);
   ++queueHist[bucket];
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStatsCtlPktDumpAll --
 *
 *      Prints all stream control packet counts out to the console using
 *      the appropriate platform logging.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciStatsCtlPktDumpAll(void)
{
   uint32 index;

   ASSERT_ON_COMPILE(VSOCK_PACKET_TYPE_MAX ==
		     ARRAYSIZE(vSockStatsCtlPktCount));

   for (index = 0; index < ARRAYSIZE(vSockStatsCtlPktCount); index++) {
      Warning("Control packet count: Type = %u, Count = %"FMT64"u\n",
              index, vSockStatsCtlPktCount[index]);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStatsHistDumpAll --
 *
 *      Prints the produce and consume queue histograms to the console.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciStatsHistDumpAll(void)
{
   uint32 index;

   #define VSOCK_DUMP_HIST(strname, name) do {             \
      for (index = 0; index < ARRAYSIZE(name); index++) {  \
         Warning(strname " Bucket count %u = %"FMT64"u\n", \
              index, name[index]);                         \
      }                                                    \
   } while (0)

   VSOCK_DUMP_HIST("Produce Queue", vSockStatsProduceQueueHist);
   VSOCK_DUMP_HIST("Consume Queue", vSockStatsConsumeQueueHist);

   #undef VSOCK_DUMP_HIST
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStatsTotalsDumpAll --
 *
 *      Prints the produce and consume totals.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciStatsTotalsDumpAll(void)
{
   Warning("Produced %"FMT64"u total bytes\n",
           Atomic_Read64(&vSockStatsProduceTotal));
   Warning("Consumed %"FMT64"u total bytes\n",
           Atomic_Read64(&vSockStatsConsumeTotal));
}


/*
 *----------------------------------------------------------------------------
 *
 * VSockVmciStatsReset --
 *
 *      Reset all VSock statistics.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static INLINE void
VSockVmciStatsReset(void)
{
   uint32 index;

   #define VSOCK_RESET_ARRAY(name) do {                   \
      for (index = 0; index < ARRAYSIZE(name); index++) { \
         name[index] = 0;                                 \
      }                                                   \
   } while (0)

   VSOCK_RESET_ARRAY(vSockStatsCtlPktCount);
   VSOCK_RESET_ARRAY(vSockStatsProduceQueueHist);
   VSOCK_RESET_ARRAY(vSockStatsConsumeQueueHist);

   #undef VSOCK_RESET_ARRAY

   Atomic_Write64(&vSockStatsConsumeTotal, 0);
   Atomic_Write64(&vSockStatsProduceTotal, 0);
}

#else
#define VSOCK_STATS_STREAM_CONSUME_HIST(vsk)
#define VSOCK_STATS_STREAM_PRODUCE_HIST(vsk)
#define VSOCK_STATS_STREAM_PRODUCE(bytes)
#define VSOCK_STATS_STREAM_CONSUME(bytes)
#define VSOCK_STATS_CTLPKT_LOG(pktType)
#define VSOCK_STATS_CTLPKT_DUMP_ALL()
#define VSOCK_STATS_HIST_DUMP_ALL()
#define VSOCK_STATS_TOTALS_DUMP_ALL()
#define VSOCK_STATS_RESET()
#endif // VSOCK_GATHER_STATISTICS

#endif // __STATS_H__
