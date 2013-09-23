/*********************************************************
 * Copyright (C) 2006-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * x86_basic_defs.h --
 *
 *	Basic macros describing the x86 architecture.
 */

#ifndef _X86_BASIC_DEFS_H_
#define _X86_BASIC_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"

#define SIZE_8BIT   1
#define SIZE_16BIT  2
#define SIZE_24BIT  3
#define SIZE_32BIT  4
#define SIZE_48BIT  6
#define SIZE_64BIT  8
#define SIZE_80BIT  10
#define SIZE_128BIT 16
#define SIZE_256BIT 32


#define X86_MAX_INSTR_LEN  15   /* Max byte length of an x86 instruction. */

#define NUM_IDT_VECTORS 256

/*
 *   control registers
 */

#define CR0_PE         0x00000001
#define CR0_MP         0x00000002
#define CR0_EM         0x00000004
#define CR0_TS         0x00000008
#define CR0_ET         0x00000010
#define CR0_NE         0x00000020
#define CR0_WP         0x00010000
#define CR0_AM         0x00040000
#define CR0_NW         0x20000000
#define CR0_CD         0x40000000
#define CR0_PG         0x80000000
#define CR0_RESERVED   CONST64U(0xffffffff1ffaffc0)

#define CR3_PWT        0x00000008
#define CR3_PCD        0x00000010
#define CR3_PDB_SHIFT  12
#define CR3_PDB_MASK   0xfffff000
#define CR3_IGNORE     0xFFF
#define PAE_CR3_IGNORE 0x1F
#define CR3_PCID_MASK  0xFFF
#define CR3_NO_FLUSH   (1ULL << 63)

#define CR4_VME        0x00000001
#define CR4_PVI        0x00000002
#define CR4_TSD        0x00000004
#define CR4_DE         0x00000008
#define CR4_PSE        0x00000010
#define CR4_PAE        0x00000020
#define CR4_MCE        0x00000040
#define CR4_PGE        0x00000080
#define CR4_PCE        0x00000100
#define CR4_OSFXSR     0x00000200 // CPU/OS supports SIMD insts
#define CR4_OSXMMEXCPT 0x00000400 // #XF exception enable PIII only
#define CR4_VMXE       0x00002000
#define CR4_SMXE       0x00004000
#define CR4_FSGSBASE   0x00010000
#define CR4_PCIDE      0x00020000
#define CR4_OSXSAVE    0x00040000
#define CR4_SMEP       0x00100000
/* Removing a bit from CR4_RESERVED causes Task_Switch to leave the bit set. */
#define CR4_RESERVED   CONST64U(0xffffffffffe89800) 
#define CR8_RESERVED   CONST64U(0xfffffffffffffff0)

#define DR6_B0         0x00000001
#define DR6_B1         0x00000002
#define DR6_B2         0x00000004
#define DR6_B3         0x00000008
#define DR6_B0123      (DR6_B0 | DR6_B1 | DR6_B2 | DR6_B3)
#define DR6_B(_n)      (1 << (_n))
#define DR6_BD         0x00002000
#define DR6_BS         0x00004000
#define DR6_BT         0x00008000
#define DR6_ONES       0xffff0ff0
#define DR6_STICKY     (~DR6_B0123)
#define DR6_RESERVED_MASK 0xffff1ff0

/* 
 * Debug registers.
 */

#define DR7_L_MASK(_n)   (1 << ((_n) * 2))
#define DR7_G_MASK(_n)   (1 << ((_n) * 2 + 1))
#define DR7_LG_MASK(_n)  (3 << ((_n) * 2))
#define DR7_RW_MASK(_n)  (3 << (16 + (_n) * 4))
#define DR7_LEN_MASK(_n) (3 << (18 + (_n) * 4))

#define DR7_L0         DR7_L_MASK(0)
#define DR7_G0         DR7_G_MASK(0)
#define DR7_L1         DR7_L_MASK(1)
#define DR7_G1         DR7_G_MASK(1)
#define DR7_L2         DR7_L_MASK(2)
#define DR7_G2         DR7_G_MASK(2)
#define DR7_L3         DR7_L_MASK(3)
#define DR7_G3         DR7_G_MASK(3)
#define DR7_ENABLED    0x000000ff

#define DR7_LE         0x00000100    // Deprecated in modern hardware
#define DR7_GE         0x00000200    // Deprecated in modern hardware
#define DR7_GD         0x00002000
#define DR7_ONES       0x00000400
#define DR7_RESERVED   CONST64U(0xffffffff0000dc00)
#define DR7_DEFUNCT    (DR7_LE | DR7_GE)
#define DR7_DEFAULT    DR7_ONES
#define DR7_LX_MASK    (DR7_L0 | DR7_L1 | DR7_L2 | DR7_L3 | DR7_LE)
#define DR7_GX_MASK    (DR7_G0 | DR7_G1 | DR7_G2 | DR7_G3 | DR7_GE)
#define DR7_LGX_MASK   (DR7_LX_MASK | DR7_GX_MASK)

#define DR7_RW(_r,_n)  (((_r) >> (16+(_n)*4)) & 0x3)
#define DR7_L(_r,_n)   (((_r) >> ((_n)*2)) & 1)
#define DR7_G(_r,_n)   (((_r) >> (1 + (_n)*2)) & 1)
#define DR7_LEN(_r,_n) (((_r) >> (18+(_n)*4)) & 0x3)

#define DR7_RW_BITS(_n,_rw)     ((_rw) << (16 + (_n) * 4))
#define DR7_LEN_BITS(_n,_len)   ((_len) << (18 + (_n) * 4))

#define DR7_RW_INST    0x0
#define DR7_RW_WRITES  0x1
#define DR7_RW_IO      0x2
#define DR7_RW_ACCESS  0x3

#define DR7_LENGTH_1   0x0
#define DR7_LENGTH_2   0x1
#define DR7_LENGTH_8   0x2
#define DR7_LENGTH_4   0x3

#define DEBUG_STATUS_B0   (1<<0)
#define DEBUG_STATUS_B1   (1<<1)
#define DEBUG_STATUS_B2   (1<<2)
#define DEBUG_STATUS_B3   (1<<3)
#define DEBUG_STATUS_DB   (1<<13)
#define DEBUG_STATUS_BS   (1<<14)
#define DEBUG_STATUS_BT   (1<<15)

/*
 *   exception error codes
 */

#define EXC_DE            0
#define EXC_DB            1
#define EXC_NMI           2
#define EXC_BP            3
#define EXC_OF            4
#define EXC_BR            5
#define EXC_UD            6
#define EXC_NM            7
#define EXC_DF            8
#define EXC_TS           10
#define EXC_NP           11
#define EXC_SS           12
#define EXC_GP           13
#define EXC_PF           14
#define EXC_MF           16
#define EXC_AC           17
#define EXC_MC           18
#define EXC_XF           19  /* SIMD exception.                */
#define EXC_SX           30  /* Security exception (SVM only). */

/*
 * eflag/rflag definitions.
 */

#define EFLAGS_IOPL_SHIFT 12

typedef enum x86_FLAGS {
   EFLAGS_NONE  = 0,
   EFLAGS_CF    = (1 << 0),     /* User */ 
   EFLAGS_SET   = (1 << 1),                                  
   EFLAGS_PF    = (1 << 2),     /* User */ 
   EFLAGS_AF    = (1 << 4),     /* User */ 
   EFLAGS_ZF    = (1 << 6),     /* User */ 
   EFLAGS_SF    = (1 << 7),     /* User */ 
   EFLAGS_TF    = (1 << 8),     /* Priv */ 
   EFLAGS_IF    = (1 << 9),     /* Priv */ 
   EFLAGS_DF    = (1 << 10),    /* User */ 
   EFLAGS_OF    = (1 << 11),    /* User */ 
   EFLAGS_NT    = (1 << 14),    /* Priv */ 
   EFLAGS_RF    = (1 << 16),    /* Priv */ 
   EFLAGS_VM    = (1 << 17),    /* Priv */ 
   EFLAGS_AC    = (1 << 18),    /* Priv */ 
   EFLAGS_VIF   = (1 << 19),    /* Priv */ 
   EFLAGS_VIP   = (1 << 20),    /* Priv */ 
   EFLAGS_ID    = (1 << 21),    /* Priv */
   
   EFLAGS_IOPL  = 3 << EFLAGS_IOPL_SHIFT,
   EFLAGS_ARITH = (EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF |
                   EFLAGS_SF | EFLAGS_OF),
   EFLAGS_ALL   = (EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF |
                   EFLAGS_SF | EFLAGS_DF | EFLAGS_OF | EFLAGS_TF |
                   EFLAGS_IF | EFLAGS_IOPL | EFLAGS_NT | EFLAGS_RF |
                   EFLAGS_VM | EFLAGS_AC | EFLAGS_VIF | EFLAGS_VIP | 
                   EFLAGS_ID),
   EFLAGS__4    = 0x7fffffff    /* ensure 4 byte encoding */
} x86_FLAGS;


#endif // ifndef _VM_BASIC_DEFS_H_
