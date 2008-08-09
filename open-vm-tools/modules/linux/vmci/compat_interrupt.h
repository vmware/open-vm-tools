/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_INTERRUPT_H__
#   define __COMPAT_INTERRUPT_H__


#include <linux/interrupt.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 69)
/*
 * We cannot just define irqreturn_t, as some 2.4.x kernels have
 * typedef void irqreturn_t; for "increasing" backward compatibility.
 */
typedef void compat_irqreturn_t;
#define COMPAT_IRQ_NONE
#define COMPAT_IRQ_HANDLED
#define COMPAT_IRQ_RETVAL(x)
#else
typedef irqreturn_t compat_irqreturn_t;
#define COMPAT_IRQ_NONE		IRQ_NONE
#define COMPAT_IRQ_HANDLED	IRQ_HANDLED
#define COMPAT_IRQ_RETVAL(x)	IRQ_RETVAL(x)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#define COMPAT_IRQF_DISABLED    SA_INTERRUPT
#define COMPAT_IRQF_SHARED      SA_SHIRQ
#else
#define COMPAT_IRQF_DISABLED    IRQF_DISABLED
#define COMPAT_IRQF_SHARED      IRQF_SHARED
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
#define COMPAT_IRQ_HANDLER_ARGS(irq, devp) (int irq, void *devp, struct pt_regs *regs)
#else
#define COMPAT_IRQ_HANDLER_ARGS(irq, devp) (int irq, void *devp)
#endif

#endif /* __COMPAT_INTERRUPT_H__ */
