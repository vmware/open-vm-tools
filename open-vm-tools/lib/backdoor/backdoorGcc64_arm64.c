/*********************************************************
 * Copyright (C) 2015-2019 VMware, Inc. All rights reserved.
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

/*********************************************************
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

/*
 * backdoorGcc64_arm64.c --
 *
 *      Implements the real work for guest-side backdoor for GCC, 64-bit
 *      target (supports inline ASM, GAS syntax). The asm sections are marked
 *      volatile since vmware can change the registers content without the
 *      compiler knowing it.
 *
 *      See backdoor_def.h for implementation details.
 *
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "backdoor.h"
#include "backdoor_def.h"
#include "backdoorInt.h"


/*
 *----------------------------------------------------------------------------
 *
 * Backdoor_InOut --
 *
 *      Send a low-bandwidth basic request (16 bytes) to vmware, and return its
 *      reply (24 bytes).
 *
 * Results:
 *      Host-side response returned in bp IN/OUT parameter.
 *
 * Side effects:
 *      Pokes the backdoor.
 *
 *----------------------------------------------------------------------------
 */

void
Backdoor_InOut(Backdoor_proto *myBp) // IN/OUT
{
   /*
    * The low-bandwidth backdoor call has the following effects:
    * o The VM can modify the calling vCPU's registers x0, x1, x2, x3, x4
    *   and x5.
    * o The VM can modify arbitrary guest memory.
    * So far the VM does not modify the calling vCPU's conditional flags.
    */
   __asm__ __volatile__(
      "ldp x4, x5, [%0, 8 * 4] \n\t"
      "ldp x2, x3, [%0, 8 * 2] \n\t"
      "ldp x0, x1, [%0       ] \n\t"
      "mov x7, %1              \n\t"
      "movk x7, %2, lsl #32    \n\t"
      "mrs xzr, mdccsr_el0     \n\t"
      "stp x4, x5, [%0, 8 * 4] \n\t"
      "stp x2, x3, [%0, 8 * 2] \n\t"
      "stp x0, x1, [%0       ]     "
      :
      : "r" (myBp),
        "M" (X86_IO_W7_WITH | X86_IO_W7_DIR | 2 << X86_IO_W7_SIZE_SHIFT),
        "i" (X86_IO_MAGIC)
      : "x0", "x1", "x2", "x3", "x4", "x5", "x7", "memory"
   );
}


/*
 *-----------------------------------------------------------------------------
 *
 * BackdoorHbIn  --
 * BackdoorHbOut --
 *
 *      Send a high-bandwidth basic request to vmware, and return its
 *      reply.
 *
 * Results:
 *      Host-side response returned in bp IN/OUT parameter.
 *
 * Side-effects:
 *      Pokes the high-bandwidth backdoor port.
 *
 *-----------------------------------------------------------------------------
 */

#define _BACKDOOR_HB(myBp, w7dir)                                             \
   __asm__ __volatile__(                                                      \
      "ldp x5, x6, [%0, 8 * 5] \n\t"                                          \
      "ldp x3, x4, [%0, 8 * 3] \n\t"                                          \
      "ldp x1, x2, [%0, 8 * 1] \n\t"                                          \
      "ldr x0,     [%0       ] \n\t"                                          \
      "mov x7, %1              \n\t"                                          \
      "movk x7, %2, lsl #32    \n\t"                                          \
      "mrs xzr, mdccsr_el0     \n\t"                                          \
      "stp x5, x6, [%0, 8 * 5] \n\t"                                          \
      "stp x3, x4, [%0, 8 * 3] \n\t"                                          \
      "stp x1, x2, [%0, 8 * 1] \n\t"                                          \
      "str x0,     [%0       ]     "                                          \
      :                                                                       \
      : "r" (myBp),                                                           \
        "M" (X86_IO_W7_STR | X86_IO_W7_WITH | w7dir),                         \
        "i" (X86_IO_MAGIC)                                                    \
      : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "memory"              \
   )

void
BackdoorHbIn(Backdoor_proto_hb *myBp) // IN/OUT
{
   /*
    * The high-bandwidth backdoor call has the following effects:
    * o The VM can modify the calling vCPU's registers x0, x1, x2, x3, x4, x5
    *   and x6.
    * o The VM can modify arbitrary guest memory.
    * So far the VM does not modify the calling vCPU's conditional flags.
    */
   _BACKDOOR_HB(myBp, X86_IO_W7_DIR);
}

void
BackdoorHbOut(Backdoor_proto_hb *myBp) // IN/OUT
{
   /*
    * The high-bandwidth backdoor call has the following effects:
    * o The VM can modify the calling vCPU's registers x0, x1, x2, x3, x4, x5
    *   and x6.
    * o The VM can modify arbitrary guest memory.
    * So far the VM does not modify the calling vCPU's conditional flags.
    */
   _BACKDOOR_HB(myBp, 0);
}

#undef _BACKDOOR_HB


#ifdef __cplusplus
}
#endif
