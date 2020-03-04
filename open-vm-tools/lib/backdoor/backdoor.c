/*********************************************************
 * Copyright (C) 1999-2020 VMware, Inc. All rights reserved.
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
 * backdoor.c --
 *
 *    First layer of the internal communication channel between guest
 *    applications and vmware
 *
 *    This is the backdoor. By using special ports of the virtual I/O space,
 *    and the virtual CPU registers, a guest application can send a
 *    synchroneous basic request to vmware, and vmware can reply to it.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "backdoor_def.h"
#include "backdoor.h"
#include "backdoorInt.h"

#if defined(USE_HYPERCALL)
#include "vm_assert.h"
#include "x86cpuid.h"
#include "x86cpuid_asm.h"
#endif

#ifdef USE_VALGRIND
/*
 * When running under valgrind, we need to ensure we have the correct register
 * state when poking the backdoor. The VALGRIND_NON_SIMD_CALLx macros are used
 * to escape from the valgrind emulated CPU to the physical CPU.
 */
#include "vm_valgrind.h"
#endif

#if defined(BACKDOOR_DEBUG) && defined(USERLEVEL)
#if defined(__KERNEL__) || defined(_KERNEL)
#else
#   include "debug.h"
#endif
#   include <stdio.h>
#   define BACKDOOR_LOG(...) Debug(__VA_ARGS__)
#   define BACKDOOR_LOG_PROTO_STRUCT(x) BackdoorPrintProtoStruct((x))
#   define BACKDOOR_LOG_HB_PROTO_STRUCT(x) BackdoorPrintHbProtoStruct((x))


/*
 *----------------------------------------------------------------------------
 *
 * BackdoorPrintProtoStruct --
 * BackdoorPrintHbProtoStruct --
 *
 *      Print the contents of the specified backdoor protocol structure via
 *      printf.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Output to stdout.
 *
 *----------------------------------------------------------------------------
 */

void
BackdoorPrintProtoStruct(Backdoor_proto *myBp)
{
   Debug("magic 0x%08x, command %d, size %"FMTSZ"u, port %d\n",
         myBp->in.ax.word, myBp->in.cx.halfs.low,
         myBp->in.size, myBp->in.dx.halfs.low);

#ifndef VM_X86_64
   Debug("ax %#x, "
         "bx %#x, "
         "cx %#x, "
         "dx %#x, "
         "si %#x, "
         "di %#x\n",
         myBp->out.ax.word,
         myBp->out.bx.word,
         myBp->out.cx.word,
         myBp->out.dx.word,
         myBp->out.si.word,
         myBp->out.di.word);
#else
   Debug("ax %#"FMT64"x, "
         "bx %#"FMT64"x, "
         "cx %#"FMT64"x, "
         "dx %#"FMT64"x, "
         "si %#"FMT64"x, "
         "di %#"FMT64"x\n",
         myBp->out.ax.quad,
         myBp->out.bx.quad,
         myBp->out.cx.quad,
         myBp->out.dx.quad,
         myBp->out.si.quad,
         myBp->out.di.quad);
#endif
}


void
BackdoorPrintHbProtoStruct(Backdoor_proto_hb *myBp)
{
   Debug("magic 0x%08x, command %d, size %"FMTSZ"u, port %d, "
         "srcAddr %"FMTSZ"u, dstAddr %"FMTSZ"u\n",
         myBp->in.ax.word, myBp->in.bx.halfs.low, myBp->in.size,
         myBp->in.dx.halfs.low, myBp->in.srcAddr, myBp->in.dstAddr);

#ifndef VM_X86_64
   Debug("ax %#x, "
         "bx %#x, "
         "cx %#x, "
         "dx %#x, "
         "si %#x, "
         "di %#x, "
         "bp %#x\n",
         myBp->out.ax.word,
         myBp->out.bx.word,
         myBp->out.cx.word,
         myBp->out.dx.word,
         myBp->out.si.word,
         myBp->out.di.word,
         myBp->out.bp.word);
#else
   Debug("ax %#"FMT64"x, "
         "bx %#"FMT64"x, "
         "cx %#"FMT64"x, "
         "dx %#"FMT64"x, "
         "si %#"FMT64"x, "
         "di %#"FMT64"x, "
         "bp %#"FMT64"x\n",
         myBp->out.ax.quad,
         myBp->out.bx.quad,
         myBp->out.cx.quad,
         myBp->out.dx.quad,
         myBp->out.si.quad,
         myBp->out.di.quad,
         myBp->out.bp.quad);
#endif
}

#else
#   define BACKDOOR_LOG(...)
#   define BACKDOOR_LOG_PROTO_STRUCT(x)
#   define BACKDOOR_LOG_HB_PROTO_STRUCT(x)
#endif

#if defined(USE_HYPERCALL)
/* Setting 'backdoorInterface' is idempotent, no atomic access is required. */
static BackdoorInterface backdoorInterface = BACKDOOR_INTERFACE_NONE;

static BackdoorInterface
BackdoorGetInterface(void)
{
   if (UNLIKELY(backdoorInterface == BACKDOOR_INTERFACE_NONE)) {
      CPUIDRegs regs;

      /* Check whether we're on a VMware hypervisor that supports vmmcall. */
      __GET_CPUID(1, &regs);
      if (CPUID_ISSET(1, ECX, HYPERVISOR, regs.ecx)) {
         __GET_CPUID(CPUID_HYPERVISOR_LEVEL_0, &regs);
         if (CPUID_IsRawVendor(&regs, CPUID_VMWARE_HYPERVISOR_VENDOR_STRING)) {
            if (__GET_EAX_FROM_CPUID(CPUID_HYPERVISOR_LEVEL_0) >=
                                     CPUID_VMW_FEATURES) {
               uint32 features = __GET_ECX_FROM_CPUID(CPUID_VMW_FEATURES);
               if (CPUID_ISSET(CPUID_VMW_FEATURES, ECX,
                               VMCALL_BACKDOOR, features)) {
                  backdoorInterface = BACKDOOR_INTERFACE_VMCALL;
                  BACKDOOR_LOG("Backdoor interface: vmcall\n");
               } else if (CPUID_ISSET(CPUID_VMW_FEATURES, ECX,
                                      VMMCALL_BACKDOOR, features)) {
                  backdoorInterface = BACKDOOR_INTERFACE_VMMCALL;
                  BACKDOOR_LOG("Backdoor interface: vmmcall\n");
               }
            }
         }
      }
      if (backdoorInterface == BACKDOOR_INTERFACE_NONE) {
         backdoorInterface = BACKDOOR_INTERFACE_IO;
         BACKDOOR_LOG("Backdoor interface: I/O port\n");
      }
   }
   return backdoorInterface;
}
#else
static BackdoorInterface
BackdoorGetInterface(void) {
   return BACKDOOR_INTERFACE_IO;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Backdoor_ForceLegacy --
 *
 *     In some cases, it may be desirable to use the legacy IO interface to
 *     access the backdoor, even if CPUID reports support for the VMCALL/VMMCALL
 *     interface.
 *
 * Params:
 *     force  Set to TRUE to force the library to use the legacy IO interface
 *            for dispatching backdoor calls; set to FALSE to use the
 *            autodetected interface.
 *
 * Side-effects:
 *      Changes the interface used to access the backdoor.
 *
 *-----------------------------------------------------------------------------
 */
#if defined(USE_HYPERCALL)
void
Backdoor_ForceLegacy(Bool force)
{
   if (force) {
      backdoorInterface = BACKDOOR_INTERFACE_IO;
   } else {
      backdoorInterface = BACKDOOR_INTERFACE_NONE;
      BackdoorGetInterface();
   }
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Backdoor --
 *
 *      Send a low-bandwidth basic request (16 bytes) to vmware, and return its
 *      reply (24 bytes).
 *
 * Result:
 *      None
 *
 * Side-effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#ifdef USE_VALGRIND
static void
BackdoorInOutValgrind(uint16 tid, Backdoor_proto *myBp)
{
    Backdoor_InOut(myBp);
}
static void
BackdoorHbInValgrind(uint16 tid, Backdoor_proto_hb *myBp)
{
    BackdoorHbIn(myBp);
}
static void
BackdoorHbOutValgrind(uint16 tid, Backdoor_proto_hb *myBp)
{
    BackdoorHbOut(myBp);
}
#if defined(USE_HYPERCALL)
static void
BackdoorVmcallValgrind(uint16 tid, Backdoor_proto *myBp)
{
    Backdoor_Vmcall(myBp);
}
static void
BackdoorVmmcallValgrind(uint16 tid, Backdoor_proto *myBp)
{
    Backdoor_Vmmcall(myBp);
}
static void
BackdoorHbVmcallValgrind(uint16 tid, Backdoor_proto_hb *myBp)
{
    BackdoorHbVmcall(myBp);
}
static void
BackdoorHbVmmcallValgrind(uint16 tid, Backdoor_proto_hb *myBp)
{
    BackdoorHbVmmcall(myBp);
}
#endif
#endif

void
Backdoor(Backdoor_proto *myBp) // IN/OUT
{
   BackdoorInterface interface = BackdoorGetInterface();
   ASSERT(myBp);

   myBp->in.ax.word = BDOOR_MAGIC;

   switch (interface) {
   case BACKDOOR_INTERFACE_IO:
      myBp->in.dx.halfs.low = BDOOR_PORT;
      break;
#if defined(USE_HYPERCALL)
   case BACKDOOR_INTERFACE_VMCALL:  // Fall through.
   case BACKDOOR_INTERFACE_VMMCALL:
      myBp->in.dx.halfs.low = BDOOR_FLAGS_LB | BDOOR_FLAGS_READ;
      break;
#endif
   default:
      ASSERT(FALSE);
      break;
   }

   BACKDOOR_LOG("Backdoor: before ");
   BACKDOOR_LOG_PROTO_STRUCT(myBp);

   switch (interface) {
   case BACKDOOR_INTERFACE_IO:
#ifdef USE_VALGRIND
      VALGRIND_NON_SIMD_CALL1(BackdoorInOutValgrind, myBp);
#else
      Backdoor_InOut(myBp);
#endif
      break;
#if defined(USE_HYPERCALL)
   case BACKDOOR_INTERFACE_VMCALL:
#ifdef USE_VALGRIND
      VALGRIND_NON_SIMD_CALL1(BackdoorVmcallValgrind, myBp);
#else
      Backdoor_Vmcall(myBp);
#endif
      break;
   case BACKDOOR_INTERFACE_VMMCALL:
#ifdef USE_VALGRIND
      VALGRIND_NON_SIMD_CALL1(BackdoorVmmcallValgrind, myBp);
#else
      Backdoor_Vmmcall(myBp);
#endif
      break;
#endif // defined(USE_HYPERCALL)
   default:
      ASSERT(FALSE);
      break;
   }

   BACKDOOR_LOG("Backdoor: after ");
   BACKDOOR_LOG_PROTO_STRUCT(myBp);
}


void
BackdoorHb(Backdoor_proto_hb *myBp, // IN/OUT
           Bool outbound)           // IN
{
   BackdoorInterface interface = BackdoorGetInterface();
   ASSERT(myBp);

   myBp->in.ax.word = BDOOR_MAGIC;

   switch (interface) {
   case BACKDOOR_INTERFACE_IO:
      myBp->in.dx.halfs.low = BDOORHB_PORT;
      break;
#if defined(USE_HYPERCALL)
   case BACKDOOR_INTERFACE_VMCALL:  // Fall through.
   case BACKDOOR_INTERFACE_VMMCALL:
      myBp->in.dx.halfs.low = BDOOR_FLAGS_HB;
      if (outbound) {
         myBp->in.dx.halfs.low |= BDOOR_FLAGS_WRITE;
      } else {
         myBp->in.dx.halfs.low |= BDOOR_FLAGS_READ;
      }
      break;
#endif
   default:
      ASSERT(FALSE);
      break;
   }

   BACKDOOR_LOG("BackdoorHb: before ");
   BACKDOOR_LOG_HB_PROTO_STRUCT(myBp);

   switch (interface) {
   case BACKDOOR_INTERFACE_IO:
      if (outbound) {
#ifdef USE_VALGRIND
         VALGRIND_NON_SIMD_CALL1(BackdoorHbOutValgrind, myBp);
#else
         BackdoorHbOut(myBp);
#endif
      } else {
#ifdef USE_VALGRIND
         VALGRIND_NON_SIMD_CALL1(BackdoorHbInValgrind, myBp);
#else
         BackdoorHbIn(myBp);
#endif
      }
      break;
#if defined(USE_HYPERCALL)
   case BACKDOOR_INTERFACE_VMCALL:
#ifdef USE_VALGRIND
      VALGRIND_NON_SIMD_CALL1(BackdoorHbVmcallValgrind, myBp);
#else
      BackdoorHbVmcall(myBp);
#endif
      break;
   case BACKDOOR_INTERFACE_VMMCALL:
#ifdef USE_VALGRIND
      VALGRIND_NON_SIMD_CALL1(BackdoorHbVmmcallValgrind, myBp);
#else
      BackdoorHbVmmcall(myBp);
#endif
      break;
#endif
   default:
      ASSERT(FALSE);
      break;
   }

   BACKDOOR_LOG("BackdoorHb: after ");
   BACKDOOR_LOG_HB_PROTO_STRUCT(myBp);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Backdoor_HbOut --
 *
 *      Send a high-bandwidth basic request to vmware, and return its
 *      reply.
 *
 * Result:
 *      The host-side response is returned via the IN/OUT parameter.
 *
 * Side-effects:
 *      Pokes the high-bandwidth backdoor.
 *
 *-----------------------------------------------------------------------------
 */

void
Backdoor_HbOut(Backdoor_proto_hb *myBp) // IN/OUT
{
   ASSERT(myBp);

   BackdoorHb(myBp, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Backdoor_HbIn --
 *
 *      Send a basic request to vmware, and return its high-bandwidth
 *      reply
 *
 * Result:
 *      Host-side response returned via the IN/OUT parameter.
 *
 * Side-effects:
 *      Pokes the high-bandwidth backdoor.
 *
 *-----------------------------------------------------------------------------
 */

void
Backdoor_HbIn(Backdoor_proto_hb *myBp) // IN/OUT
{
   ASSERT(myBp);

   BackdoorHb(myBp, FALSE);
}

#ifdef __cplusplus
}
#endif
