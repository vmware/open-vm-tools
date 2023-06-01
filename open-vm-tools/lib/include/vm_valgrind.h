/*********************************************************
 * Copyright (c) 2014, 2021-2022 VMware, Inc. All rights reserved.
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
 * vm_valgrind.h --
 *
 *      This header incorporates the client requests used by Valgrind
 *      (http://www.valgrind.org/) to instrument the binary for dynamic
 *      analysis.
 *
 *      Various hints are available to mark regions of memory as accessible,
 *      inaccessible, defined, undefined, to check the definedness or
 *      accessibliity of regions of memory, or to run code outside of
 *      Valgrind's instrumentation.
 *
 *      See the Valgrind headers for further information.
 *
 *      If USE_VALGRIND is defined, the Valgrind instrumentation macros will
 *      take effect.
 *
 *      If USE_VALGRIND is not defined, the Valgrind instrumentation macros
 *      will still be available but will have no effect (no-op).
 */

#ifndef _VM_VALGRIND_H_
#define _VM_VALGRIND_H_ 1

#ifdef USE_VALGRIND
# include "valgrind/memcheck.h"
#else

/*
 * No-ops for Valgrind macros we might use.  The Valgrind headers include their
 * own "NVALGRIND" mechanism to disable the emission of the Valgrind Client
 * Request magic sequences, but that assumes that we have suitable headers
 * available in all possible build environments.  NVALGRIND just turns each of
 * the VALGRIND_* macros into a default expression, usually 0, or a default
 * empty statement, "do { (void)0; } while (0);".  Let's do that ourselves.
 */

#define RUNNING_ON_VALGRIND 0
#define VALGRIND_CHECK_VALUE_IS_DEFINED(__lvalue) (void)0
#define VALGRIND_CHECK_MEM_IS_ADDRESSABLE(_qzz_addr, _qzz_len) (void)0
#define VALGRIND_CHECK_MEM_IS_DEFINED(_qzz_addr, _qzz_len) (void)0
#define VALGRIND_MAKE_MEM_NOACCESS(_qzz_addr, _qzz_len) (void)0
#define VALGRIND_MAKE_MEM_DEFINED(_qzz_addr, _qzz_len) (void)0
#define VALGRIND_MAKE_MEM_UNDEFINED(_qzz_addr, _qzz_len) (void)0

#endif

/*
 * VALGRIND_SPEED_FACTOR is an approximation of how much Valgrind's
 * instrumentation is likely to slow down execution.  It is a _very_ rough
 * guess because the actual slowdown will depend on the nature of the code
 * being executed, but regardless this is a handy macro for adjusting (by
 * multiplying or dividing) times or loop counts or anything else impacted by
 * Valgrind's instrumentation overhead.
 */

#define VALGRIND_SPEED_FACTOR (RUNNING_ON_VALGRIND ? 100 : 1)

#endif /* _VM_VALGRIND_H_ */
