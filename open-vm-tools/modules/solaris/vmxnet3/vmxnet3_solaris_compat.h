/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#ifndef _VMXNET3_SOLARIS_COMPAT_H_
#define _VMXNET3_SOLARIS_COMPAT_H_

/*
 * In this file are stored all the definitions/helpers that the
 * DDI/DDK provides but that our toolchain currently lacks.
 * Nuke them the day the toolchain is refreshed.
 */

#define DDI_INTR_PRI(pri) (void *)((uintptr_t)(pri))

/* OPEN_SOLARIS and SOLARIS 11 */
#if defined(OPEN_SOLARIS) || defined(SOL11)
#define COMPAT_DDI_DEFINE_STREAM_OPS(XXname, XXidentify, XXprobe, XXattach, XXdetach, XXreset, XXgetinfo, XXflag, XXstream_tab) \
   DDI_DEFINE_STREAM_OPS(XXname, XXidentify, XXprobe, XXattach, XXdetach, \
   XXreset, XXgetinfo, XXflag, XXstream_tab, ddi_quiesce_not_supported)
#else
/* All other Solari */
#define LSO_TX_BASIC_TCP_IPV4 0x02
#define COMPAT_DDI_DEFINE_STREAM_OPS DDI_DEFINE_STREAM_OPS
#endif

#define HW_LSO 0x10

#define DB_LSOMSS(mp) ((mp)->b_datap->db_struioun.cksum.pad)

#define ETHERTYPE_VLAN (0x8100)

#endif /* _VMXNET3_SOLARIS_COMPAT_H_ */
