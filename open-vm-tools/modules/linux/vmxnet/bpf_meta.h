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

#ifndef _BPF_META_H_ 
#define _BPF_META_H_

/* This file is to be shared between vmxnet and afpacket */

/* The Control byte flags of bpf_meta_data */

#define VMXNET_BPF_PROCESSED    0x01

/* The following is the definition for dev feature bpf in linux */

#ifndef NETIF_F_BPF             
#define NETIF_F_BPF             (1<<31) /* BPF Capable Virtual Nic */
#endif /* NETIF_F_BPF */


/* TODO: These three definitions are picked up from vmkernel/public/net_pkt.h.
 * Reorg the code and take this from a common place 
 */

#define MAX_BPF_FILTERS 8       /* Maximum number of Filters supported */
typedef unsigned int BpfSnapLen;
typedef unsigned int BpfSnapLens[MAX_BPF_FILTERS];

/*
 * skb->cb[40] maps to this structure. The bpf Trailer in vmxnet is stashed to
 * this structure in skb.
 *
 */

struct BPF_MetaData {

    BpfSnapLens bpfSnapLens; /* 4 * 8 = 32 bytes of SnapLens as recved from VMK*/

    unsigned char unused[7]; /* 7  bytes unused */

    unsigned char controlByte; /* 1  used as controlbyte. For now indicates
                                  whether VMK processed BPF or not*/ 
};

#endif
