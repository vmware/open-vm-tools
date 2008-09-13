/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * vm_procps.h --
 *
 *      Provides an interface to Linux's libproc.so.
 *
 *      The upstream procps package is structured such that most Linux
 *      distributions do not provide a libproc-dev package containing the
 *      interface to libproc.so. Instead, we provide this header containing
 *      just enough bits and pieces of the procps headers to satisfy our needs.
 */

#ifndef _VM_PROCPS_H_
#define _VM_PROCPS_H_

/*
 * The getstat() function below makes use of restricted pointers (added in C99)
 * and the 'jiff' type.
 */
#if !defined(restrict) && __STDC_VERSION__ < 199901
#   if __GNUC__ > 2 || __GNUC_MINOR__ >= 92
#      define restrict __restrict__
#   else
#      warning No restrict keyword?
#      define restrict
#   endif
#endif

typedef unsigned long long jiff;

/*
 * Global variables
 */

extern unsigned long long Hertz;
extern unsigned long kb_main_buffers;
extern unsigned long kb_main_cached;
extern unsigned long kb_main_free;
extern unsigned long kb_active;
extern unsigned long kb_inactive;

/*
 * Global functions
 */

extern void getstat(jiff *restrict cuse, jiff *restrict cice,
                    jiff *restrict csys, jiff *restrict cide,
                    jiff *restrict ciow, jiff *restrict cxxx,
                    jiff *restrict cyyy, jiff *restrict czzz,
                    unsigned long *restrict pin, unsigned long *restrict pout,
                    unsigned long *restrict s_in, unsigned long *restrict sout,
                    unsigned *restrict intr, unsigned *restrict ctxt,
                    unsigned int *restrict running,
                    unsigned int *restrict blocked,
                    unsigned int *restrict btime,
                    unsigned int *restrict processes);
extern void meminfo(void);

#endif // ifndef _VM_PROCPS_H_
