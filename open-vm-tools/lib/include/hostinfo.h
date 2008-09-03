/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * hostinfo.h --
 *
 *      Interface to host-specific information functions
 *   
 */

#if !defined(_HOSTINFO_H_)
#define _HOSTINFO_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "x86cpuid.h"
#include "unicodeTypes.h"

extern Unicode Hostinfo_NameGet(void);	/* don't free result */
extern Unicode Hostinfo_HostName(void);	/* free result */

extern void Hostinfo_MachineID(uint32 *hostNameHash,
                               uint64 *hostHardwareID);

extern Bool Hostinfo_GetMemoryInfoInPages(unsigned int *minSize,
                                          unsigned int *maxSize,
				          unsigned int *currentSize);
#ifdef VMX86_SERVER
extern Bool Hostinfo_GetCOSMemoryInfoInPages(unsigned int *minSize,
                                             unsigned int *maxSize,
                                             unsigned int *currentSize);
#endif
extern Bool Hostinfo_GetRatedCpuMhz(int32 cpuNumber,
                                    uint32 *mHz);
extern char* Hostinfo_GetCpuDescription(uint32 cpuNumber);
extern void Hostinfo_GetTimeOfDay(VmTimeType *time);
extern VmTimeType Hostinfo_SystemUpTime(void);
extern VmTimeType Hostinfo_RawSystemTimerUS(void);
extern VmTimeType Hostinfo_SystemTimerUS(void);
extern int Hostinfo_OSVersion(int i);
extern const char *Hostinfo_OSVersionString(void);
extern Bool Hostinfo_OSIsSMP(void);
#if defined(_WIN32)
extern Bool Hostinfo_OSIsWinNT(void);
extern Bool Hostinfo_OSIsWow64(void);
#endif
extern Bool Hostinfo_TouchBackDoor(void);
extern Bool Hostinfo_TouchXen(void);

#define HGMP_PRIVILEGE    0
#define HGMP_NO_PRIVILEGE 1
extern Unicode Hostinfo_GetModulePath(uint32 priv);


#if !defined(_WIN32)
extern void Hostinfo_ResetProcessState(const int *keepFds, size_t numKeepFds);
extern int Hostinfo_Execute(const char *command, char * const *args,
			    Bool wait);
#endif

extern Unicode Hostinfo_GetUser(void);
extern void Hostinfo_LogMemUsage(void);


/*
 * HostInfoCpuIdInfo --
 *
 *      Contains cpuid information for a CPU.
 */

typedef struct {
   CpuidVendors vendor;

   uint32 version;
   uint8 family;
   uint8 model;
   uint8 stepping;
   uint8 type;

   uint32 features;
   uint32 extfeatures;

   uint32 numPhysCPUs;
   uint32 numCores;
   uint32 numLogCPUs;
} HostinfoCpuIdInfo;


extern uint32 Hostinfo_NumCPUs(void);
extern Bool Hostinfo_GetCpuid(HostinfoCpuIdInfo *info);

#if defined(VMX86_SERVER)
extern Bool Hostinfo_HTDisabled(void);
#endif

#if defined(_WIN32)
Bool Hostinfo_GetPCFrequency(uint64 *pcHz);
Bool Hostinfo_GetMhzOfProcessor(int32 processorNumber, 
				uint32 *currentMhz, uint32 *maxMhz);
uint64 Hostinfo_SystemIdleTime(void);
Bool Hostinfo_GetAllCpuid(CPUIDQuery *query);
#endif
void Hostinfo_LogLoadAverage(void);
Bool Hostinfo_GetLoadAverage(uint32 *l);


#endif /* ifndef _HOSTINFO_H_ */
