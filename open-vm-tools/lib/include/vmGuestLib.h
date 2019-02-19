/*********************************************************
 * Copyright (C) 2003-2016,2019 VMware, Inc. All rights reserved.
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

#ifndef _VM_GUEST_LIB_H_
#define _VM_GUEST_LIB_H_


#include "vm_basic_types.h"
#include "vmSessionId.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * This is the VMware GuestLib, an API used for accessing various
 * performance statistics pertaining to the VMware virtual environment
 * from within a VMware Virtual Machine.
 */


/*
 * Error codes returned by GuestLib functions.
 *
 * XXX These should be unified with Foundry's error codes.
 */
typedef enum {
   VMGUESTLIB_ERROR_SUCCESS                = 0,  // No error
   VMGUESTLIB_ERROR_OTHER,                       // Other error
   VMGUESTLIB_ERROR_NOT_RUNNING_IN_VM,           // Not running in a VM
   VMGUESTLIB_ERROR_NOT_ENABLED,                 // GuestLib not enabled on the host.
   VMGUESTLIB_ERROR_NOT_AVAILABLE,               // This stat not available on this host.
   VMGUESTLIB_ERROR_NO_INFO,                     // UpdateInfo() has never been called.
   VMGUESTLIB_ERROR_MEMORY,                      // Not enough memory
   VMGUESTLIB_ERROR_BUFFER_TOO_SMALL,            // Buffer too small
   VMGUESTLIB_ERROR_INVALID_HANDLE,              // Handle is invalid
   VMGUESTLIB_ERROR_INVALID_ARG,                 // One or more arguments were invalid
   VMGUESTLIB_ERROR_UNSUPPORTED_VERSION          // The host doesn't support this request
} VMGuestLibError;


char const * VMGuestLib_GetErrorText(VMGuestLibError error); // IN

/*
 * GuestLib handle.
 *
 * This handle provides a context for accessing all GuestLib
 * state. Use VMGuestLib_OpenHandle to get a handle for use with other
 * GuestLib functions, and use VMGuestLib_CloseHandle to release a
 * handle previously acquired with VMGuestLib_OpenHandle.
 *
 * All of the statistics and session state are maintained per GuestLib
 * handle, so operating on one GuestLib handle will not affect the
 * state of another handle.
 */

struct _VMGuestLibHandle;
typedef struct _VMGuestLibHandle* VMGuestLibHandle;


VMGuestLibError VMGuestLib_OpenHandle(VMGuestLibHandle *handle); // OUT
VMGuestLibError VMGuestLib_CloseHandle(VMGuestLibHandle handle); // IN


/*
 * Update the info and session state for the given handle.
 *
 * Concurrency/thread safety: No locking is done internally around the
 * access of a handle. If a calling program uses multiple threads then
 * the caller must either ensure that each thread of execution is
 * using a separate handle, or the caller must implement locking
 * around calls to VMGuestLib_UpdateInfo() on a given handle to ensure
 * that two threads do not update the handle concurrently.
 *
 * Because the state is maintained per handle and no two handles can
 * be updated exactly simultaneously, the state of two handles may
 * differ even if they are updated one immediately after the other.
 *
 * VMGuestLib_UpdateInfo() is a fairly heavyweight function; it should
 * be viewed similar to a system call in terms of the computational
 * cost and performance hit. For this reason, a user of the API who
 * is concerned about performance will get best results by minimizing
 * the number of calls to VMGuestLib_UpdateInfo().
 */

VMGuestLibError VMGuestLib_UpdateInfo(VMGuestLibHandle handle); // IN


/*
 * Session ID
 * 
 * This is used to detect changes in the "session" of a virtual
 * machine. "Session" in this context refers to the particular running
 * instance of this virtual machine on a given host. Moving a virtual
 * machine to another host using VMotion will cause a change in
 * session ID, as will suspending and resuming a virtual machine or
 * reverting to a snapshot.
 *
 * Any of the events above (VMotion, suspend/resume, snapshot revert)
 * are likely to render invalid any information previously retrieved
 * through this API, so the intention of the session ID is to provide
 * applications with a mechanism to detect those events and react
 * accordingly, e.g. by refreshing and resetting any state that relies
 * on validity of previously retrieved information.
 *
 * Use VMGuestLib_GetSessionId() to retrieve the ID for the current
 * session after calling VMGuestLib_UpdateInfo(). After a VMotion or
 * similar event, VMGuestLib_GetSessionId() will return a new value.
 * See code example below for an example of how to use this.
 *
 * If VMGuestLib_UpdateInfo() has never been called,
 * VMGUESTLIB_ERROR_NO_INFO is returned.
 *
 * The session ID should be considered opaque and cannot be compared
 * in any meaningful way with the session IDs from any other virtual
 * machine (e.g. to determine if two virtual machines are on the same
 * host).
 *
 * Here is simple pseudo-code (with no error checking) showing a naive
 * implementation of detecting stale information using the session ID.
 *
 * -----
 *
 * VMSessionId sid = 0;
 * Bool done = FALSE;
 *
 * while (!done) {
 *    VMSessionId tmp;
 *
 *    VMGuestLib_UpdateInfo();
 *    VMGuestLib_GetSessionId(&tmp);
 *    if (tmp != sid) {
 *       ResetStats();
 *       sid = tmp;
 *    }
 * }
 *
 * -----
 */


VMGuestLibError VMGuestLib_GetSessionId(VMGuestLibHandle handle,  // IN
                                        VMSessionId *id);         // OUT


/*
 * Specific Stat accessors. The values returned by these accessor
 * functions are up to date as of the last call to VMGuestLib_UpdateInfo().
 *
 * If VMGuestLib_UpdateInfo() has never been called,
 * VMGUESTLIB_ERROR_NO_INFO is returned.
 */


/* CPU */

/*
 * Retrieves the minimum processing power in MHz available to the virtual
 * machine. Assigning a cpuReservationMhz ensures that even as other virtual
 * machines on a single host consume shared processing power, there is
 * still a certain minimum amount for this virtual machine.
 */
VMGuestLibError VMGuestLib_GetCpuReservationMHz(VMGuestLibHandle handle,    // IN
                                                uint32 *cpuReservationMHz); // OUT

/*
 * Retrieves the maximum processing power in MHz available to the virtual
 * machine. Assigning a cpuLimitMHz ensures that this virtual machine never
 * consumes more than a certain amount of the available processor power. By
 * limiting the amount of processing power consumed, a portion of this
 * shared resource is available to other virtual machines.
 */
VMGuestLibError VMGuestLib_GetCpuLimitMHz(VMGuestLibHandle handle, // IN
                                          uint32 *cpuLimitMHz);    // OUT

/*
 * Retrieves the number of CPU shares allocated to the virtual machine.
 */
VMGuestLibError VMGuestLib_GetCpuShares(VMGuestLibHandle handle, // IN
                                        uint32 *cpuShares);      // OUT

/*
 * Retrieves the number of milliseconds during which the virtual machine
 * has been using the CPU. This value is always less than or equal to
 * elapsedMS. This value, in conjunction with elapsedMS, can be used to
 * estimate efective virtual machine CPU speed.
 */
VMGuestLibError VMGuestLib_GetCpuUsedMs(VMGuestLibHandle handle, // IN
                                        uint64 *cpuUsedMs);      // OUT

/*
 * Host Processor speed. This can be used along with CpuUsedMs and
 * elapsed time to estimate approximate effective VM CPU speed
 * over a time interval. The following pseudocode illustrates how
 * to make this calculation:
 *
 * ------------------------------------
 *
 * uint32 effectiveVMSpeed;
 * uint32 hostMhz;
 * uint64 elapsed1;
 * uint64 elapsed2;
 * uint64 used1;
 * uint64 used2;
 *
 *
 * VMGuestLib_UpdateInfo(handle);
 * VMGuestLib_GetHostProcessorSpeed(handle, &hostMhz);
 * VMGuestLib_GetElapsedMs(handle, &elapsed1);
 * VMGuestLib_GetUsedMs(handle, &used1);
 * ....
 * VMGuestLib_UpdateInfo(handle);
 * VMGuestLib_GetElapsedMs(handle, &elapsed2);
 * VMGuestLib_GetUsedMs(handle, &used2);
 *
 * effectiveVMSpeed = hostMhz * ((used2 - used1) / (elapsed2 - elapsed1));
 *
 *
 * ------------------------------------
 *
 * After this code executes, effectiveVMSpeed will be the approximate
 * average effective speed of the VM's virtual CPU over the time period
 * between the two calls to VMGuestLib_UpdateInfo().
 *
 */

VMGuestLibError VMGuestLib_GetHostProcessorSpeed(VMGuestLibHandle handle, // IN
                                                 uint32 *mhz);            // OUT


/* Memory */

/*
 * Retrieves the minimum amount of memory that is available to the virtual
 * machine. Assigning a cpuReservationMB ensures that even as other virtual
 * machines on a single host consume memory, there is still a certain
 * minimum amount for this virtual machine.
 */
VMGuestLibError VMGuestLib_GetMemReservationMB(VMGuestLibHandle handle,   // IN
                                               uint32 *memReservationMB); // OUT

/*
 * Retrieves the maximum amount of memory that is available to the virtual
 * machine. Assigning a cpuLimitMB ensures that this virtual machine never
 * consumes more than a certain amount of the available processor power. By
 * limiting the amount of processing power consumed, a portion of this
 * shared resource is available to other virtual machines.
 */
VMGuestLibError VMGuestLib_GetMemLimitMB(VMGuestLibHandle handle, // IN
                                         uint32 *memLimitMB);     // OUT

/*
 * Retrieves the number of memory shares allocated to the virtual machine.
 * This API returns an error if the memory shares exceeds the maximum
 * value of 32-bit unsigned integer (0xFFFFFFFF). Therefore,
 * VMGuestLib_GetMemShares64 API should be used instead of this API.
 */
VMGuestLibError VMGuestLib_GetMemShares(VMGuestLibHandle handle, // IN
                                        uint32 *memShares);      // OUT

/*
 * Retrieves the number of memory shares allocated to the virtual machine.
 */
VMGuestLibError VMGuestLib_GetMemShares64(VMGuestLibHandle handle, // IN
                                          uint64 *memShares64);    // OUT

/*
 * Retrieves the mapped memory size of this virtual machine. This
 * is the current total amount of guest memory that is backed by
 * physical memory. Note that this number may include pages of
 * memory shared between multiple virtual machines and thus may be
 * an overestimate of the amount of physical host memory "consumed"
 * by this virtual machine.
 */
VMGuestLibError VMGuestLib_GetMemMappedMB(VMGuestLibHandle handle,  // IN
                                          uint32 *memMappedSizeMB); // OUT

/*
 * Retrieves the estimated amount of memory the virtual machine is actively
 * using. This method returns an estimated working set size for the virtual
 * machine.
 */
VMGuestLibError VMGuestLib_GetMemActiveMB(VMGuestLibHandle handle, // IN
                                          uint32 *memActiveMB);    // OUT

/*
 * Retrieves the amount of overhead memory associated with this virtual
 * machine consumed on the host system.
 */
VMGuestLibError VMGuestLib_GetMemOverheadMB(VMGuestLibHandle handle, // IN
                                            uint32 *memOverheadMB);  // OUT

/*
 * Retrieves the amount of memory that has been reclaimed from this virtual
 * machine via the VMware Memory Balloon mechanism.
 */
VMGuestLibError VMGuestLib_GetMemBalloonedMB(VMGuestLibHandle handle, // IN
                                             uint32 *memBalloonedMB); // OUT

/*
 * Retrieves the amount of memory associated with this virtual machine that
 * has been swapped by the host system.
 */
VMGuestLibError VMGuestLib_GetMemSwappedMB(VMGuestLibHandle handle, // IN
                                           uint32 *memSwappedMB);   // OUT

/*
 * Retrieves the amount of physical memory associated with this virtual
 * machine that is copy-on-write (COW) shared on the host.
 */
VMGuestLibError VMGuestLib_GetMemSharedMB(VMGuestLibHandle handle, // IN
                                          uint32 *memSharedMB);    // OUT

/*
 * Retrieves the estimated amount of physical memory on the host saved
 * from copy-on-write (COW) shared guest physical memory.
 */
VMGuestLibError VMGuestLib_GetMemSharedSavedMB(VMGuestLibHandle handle,   // IN
                                               uint32 *memSharedSavedMB); // OUT

/*
 * Retrieves the estimated amount of physical host memory currently
 * consumed for this virtual machine's physical memory. This is the
 * same as (mapped memory) - (sharedSaved memory).
 */
VMGuestLibError VMGuestLib_GetMemUsedMB(VMGuestLibHandle handle, // IN
                                        uint32 *memUsedMB);      // OUT



/* Elapsed Time */

/*
 * Retrieves the number of milliseconds that have passed in real time since
 * the virtual machine started running on the current host system. The
 * elapsed time counter is reset any time the virtual machine is powered
 * on, resumed, or migrated via VMotion. This value, in conjunction with
 * cpuUsedMS, can be used to estimate effective virtual machine CPU speed.
 * The cpuUsedMS value is always less than or equal to this value.
 */
VMGuestLibError VMGuestLib_GetElapsedMs(VMGuestLibHandle handle, // IN
                                        uint64 *elapsedMs);      // OUT

/*
 * Resource Pool Path.
 *
 * Retrieves a string representation of the path to this virtual machine in
 * the resource pool namespace of the host system.
 *
 * pathBuffer is a pointer to a buffer that will receive the resource
 * pool path string. bufferSize is a pointer to the size of the
 * pathBuffer in bytes. If bufferSize is not large enough to
 * accomodate the path and NUL terminator, then
 * VMGUESTLIB_ERROR_BUFFER_TOO_SMALL is returned and bufferSize
 * contains the amount of memory needed (in bytes).
 */

VMGuestLibError VMGuestLib_GetResourcePoolPath(VMGuestLibHandle handle, // IN
                                               size_t *bufferSize,      // IN/OUT
                                               char *pathBuffer);       // OUT

/*
 * CPU stolen time. The time (in ms) that the VM was runnable but not scheduled
 * to run.
 */

VMGuestLibError VMGuestLib_GetCpuStolenMs(VMGuestLibHandle handle, // IN
                                          uint64 *cpuStolenMs);    // OUT
/*
 * Memory Target Size.
 */

VMGuestLibError VMGuestLib_GetMemTargetSizeMB(VMGuestLibHandle handle,  // IN
                                              uint64 *memTargetSizeMB); // OUT

/*
 * Number of physical CPU cores on the host machine.
 */

VMGuestLibError
VMGuestLib_GetHostNumCpuCores(VMGuestLibHandle handle,   // IN
                              uint32 *hostNumCpuCores);  // OUT

/*
 * Total CPU time used by host.
 */

VMGuestLibError
VMGuestLib_GetHostCpuUsedMs(VMGuestLibHandle handle,  // IN
                            uint64 *hostCpuUsedMs);   // OUT

/*
 * Total memory swapped out on the host.
 */

VMGuestLibError
VMGuestLib_GetHostMemSwappedMB(VMGuestLibHandle handle,     // IN
                               uint64 *hostMemSwappedMB);   // OUT

/*
 * Total COW (Copy-On-Write) memory on host.
 */

VMGuestLibError
VMGuestLib_GetHostMemSharedMB(VMGuestLibHandle handle,   // IN
                              uint64 *hostMemSharedMB);  // OUT

/*
 * Total consumed memory on host.
 */

VMGuestLibError
VMGuestLib_GetHostMemUsedMB(VMGuestLibHandle handle,  // IN
                            uint64 *hostMemUsedMB);   // OUT

/*
 * Total memory available to host OS kernel.
 */

VMGuestLibError
VMGuestLib_GetHostMemPhysMB(VMGuestLibHandle handle,  // IN
                            uint64 *hostMemPhysMB);   // OUT

/*
 * Total physical memory free on host.
 */

VMGuestLibError
VMGuestLib_GetHostMemPhysFreeMB(VMGuestLibHandle handle,    // IN
                                uint64 *hostMemPhysFreeMB); // OUT

/*
 * Total host kernel memory overhead.
 */

VMGuestLibError
VMGuestLib_GetHostMemKernOvhdMB(VMGuestLibHandle handle,     // IN
                                uint64 *hostMemKernOvhdMB);  // OUT

/*
 * Total mapped memory on host.
 */

VMGuestLibError
VMGuestLib_GetHostMemMappedMB(VMGuestLibHandle handle,  // IN
                              uint64 *hostMemMappedMB); // OUT

/*
 * Total unmapped memory on host.
 */

VMGuestLibError
VMGuestLib_GetHostMemUnmappedMB(VMGuestLibHandle handle,    // IN
                                uint64 *hostMemUnmappedMB); // OUT


/*
 * Semi-structured hypervisor stats collection, for troubleshooting.
 */
VMGuestLibError
VMGuestLib_StatGet(const char *encoding,  // IN
                   const char *stat,      // IN
                   char **reply,          // OUT
                   size_t *replySize);    // OUT
/*
 * To avoid a use after free error in SWIG-generated code, it is
 * necessary to present SWIG with a modified function prototype
 * for VMGuestLib_StatFree in which reply is of type "void *"
 * rather than "char *."
 */
#ifndef	SWIG
void VMGuestLib_StatFree(char *reply, size_t replySize);
#else
void VMGuestLib_StatFree(void *reply, size_t replySize);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _VM_GUEST_LIB_H_ */
