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
 * vmxnet3_shm_shared.h --
 *
 *   Header shared between vmxnet3 shared memory kernel driver and userspace.
 *
 */

#ifndef __VMXNET_SHARED_SHM
#define __VMXNET_SHARED_SHM

#include <linux/ioctl.h>

// ioctl constants
#define SHM_IOCTL_MAGIC 'v'
#define SHM_IOCTL_TX                  _IO(SHM_IOCTL_MAGIC, 0)
#define SHM_IOCTL_ALLOC_ONE           _IO(SHM_IOCTL_MAGIC, 1)
#define SHM_IOCTL_ALLOC_MANY          _IO(SHM_IOCTL_MAGIC, 2)
#define SHM_IOCTL_ALLOC_ONE_AND_MANY  _IO(SHM_IOCTL_MAGIC, 3)
#define SHM_IOCTL_FREE_ONE            _IO(SHM_IOCTL_MAGIC, 4)

/*
 * invalid index
 *
 * Must be 0 so that a invalid shared memory page has the same
 * value as a NULL struct page. We need that because we overload
 * the same field for regular and shared memory version of vmxnet3.
 */
#define SHM_INVALID_IDX 0

// sizes of shared memory regions in pages
#define SHM_DATA_START 0
#define SHM_DATA_SIZE 4096
#define SHM_CTL_START 16384
#define SHM_CTL_SIZE 1

// ring size (in entries) is limited by the single control page - 4 bytes per re
#define SHM_RX_RING_SIZE 500
#define SHM_TX_RING_SIZE 500

// maximum fragments per packet is 16 (64k) + 2 for metadata
#define VMXNET3_SHM_MAX_FRAGS 18

// shared memory ring entry
struct vmxnet3_shm_ringentry
{
   uint16_t idx;      // index of this page in the pool
   uint16_t len: 13;  // length of data in this page
   uint16_t own: 1;   // whether the receiver owns the re
   uint16_t eop: 1;   // end of packet
   uint16_t trash: 1; // ignore all the data in this packet, but still take ownership
};

static const struct vmxnet3_shm_ringentry RE_ZERO = {0,0,0,0,0};

// shared memory control page
struct vmxnet3_shm_ctl
{
   struct vmxnet3_shm_ringentry rx_ring[SHM_RX_RING_SIZE];
   struct vmxnet3_shm_ringentry tx_ring[SHM_TX_RING_SIZE];

   // XXX move kernel_* into the kernel, currently here for debugging
   // user_rxi is used by poll() to avoid going to sleep when there are packets waiting
   uint16_t user_rxi, user_txi;
   uint16_t kernel_rxi, kernel_txi;

   struct
   {
      uint64_t user_rx, user_tx;
      uint64_t kernel_rx, kernel_tx;
   } stats;

   uint64_t channelBad;
};

#endif
