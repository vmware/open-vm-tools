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
#ifdef __linux__
extern Bool Hostinfo_GetSwapInfoInPages(unsigned int *totalSwap,
                                        unsigned int *freeSwap);
#endif
extern Bool Hostinfo_GetRatedCpuMhz(int32 cpuNumber,
                                    uint32 *mHz);
extern char *Hostinfo_GetCpuDescription(uint32 cpuNumber);
extern void Hostinfo_GetTimeOfDay(VmTimeType *time);
extern VmTimeType Hostinfo_SystemUpTime(void);
extern VmTimeType Hostinfo_SystemTimerNS(void);

static INLINE VmTimeType
Hostinfo_SystemTimerUS(void)
{
   return Hostinfo_SystemTimerNS() / 1000ULL;
}

static INLINE VmTimeType
Hostinfo_SystemTimerMS(void)
{
   return Hostinfo_SystemTimerNS() / 1000000ULL;
}

extern int Hostinfo_OSVersion(unsigned int i);
extern int Hostinfo_GetSystemBitness(void);
extern const char *Hostinfo_OSVersionString(void);

extern Bool Hostinfo_GetOSName(uint32 outBufFullLen,
                               uint32 outBufLen,
                               char *osNameFull,
                               char *osName);

extern Bool Hostinfo_OSIsSMP(void);
#if defined(_WIN32)
extern Bool Hostinfo_OSIsWinNT(void);
extern Bool Hostinfo_OSIsWow64(void);
DWORD Hostinfo_OpenProcessBits(void);
#endif
extern Bool Hostinfo_NestingSupported(void);
extern Bool Hostinfo_VCPUInfoBackdoor(unsigned bit);
extern Bool Hostinfo_SLC64Supported(void);
extern Bool Hostinfo_SynchronizedVTSCs(void);
extern Bool Hostinfo_NestedHVReplaySupported(void);
extern Bool Hostinfo_TouchBackDoor(void);
extern Bool Hostinfo_TouchVirtualPC(void);
extern Bool Hostinfo_TouchXen(void);
extern char *Hostinfo_HypervisorCPUIDSig(void);

#define HGMP_PRIVILEGE    0
#define HGMP_NO_PRIVILEGE 1
extern Unicode Hostinfo_GetModulePath(uint32 priv);
extern char *Hostinfo_GetLibraryPath(void *addr);

#if !defined(_WIN32)
extern void Hostinfo_ResetProcessState(const int *keepFds, size_t numKeepFds);
extern int Hostinfo_Execute(const char *command, char * const *args,
			    Bool wait);
typedef enum HostinfoDaemonizeFlags {
   HOSTINFO_DAEMONIZE_DEFAULT = 0,
   HOSTINFO_DAEMONIZE_NOCHDIR = (1 << 0),
   HOSTINFO_DAEMONIZE_NOCLOSE = (1 << 1),
   HOSTINFO_DAEMONIZE_EXIT    = (1 << 2),
} HostinfoDaemonizeFlags;
extern Bool Hostinfo_Daemonize(const char *path,
                               char * const *args,
                               HostinfoDaemonizeFlags flags,
                               const char *pidPath,
                               const int *openFds,
                               size_t numFds);
#endif

extern Unicode Hostinfo_GetUser(void);
extern void Hostinfo_LogMemUsage(void);


/*
 * HostInfoCpuIdInfo --
 *
 *      Contains cpuid information for a CPU.
 */

typedef struct {
   CpuidVendor vendor;

   uint32 version;
   uint8 family;
   uint8 model;
   uint8 stepping;
   uint8 type;

   uint32 features;
   uint32 extfeatures;
} HostinfoCpuIdInfo;


extern uint32 Hostinfo_NumCPUs(void);
extern char *Hostinfo_GetCpuidStr(void);
extern Bool Hostinfo_GetCpuid(HostinfoCpuIdInfo *info);

#if !defined(VMX86_SERVER)
extern Bool Hostinfo_CPUCounts(uint32 *logical, uint32 *cores, uint32 *pkgs);
#endif

#if defined(_WIN32)
typedef enum {
   OS_WIN95                  = 1,
   OS_WIN98                  = 2,
   OS_WINME                  = 3,
   OS_WINNT                  = 4,
   OS_WIN2K                  = 5,
   OS_WINXP                  = 6,
   OS_WIN2K3                 = 7,
   OS_VISTA                  = 8,
   OS_WINSEVEN               = 9,    // Windows 7
   OS_UNKNOWN                = 99999 // last, highest value
} OS_TYPE;

typedef enum {
   OS_DETAIL_WIN95           = 1,
   OS_DETAIL_WIN98           = 2,
   OS_DETAIL_WINME           = 3,
   OS_DETAIL_WINNT           = 4,
   OS_DETAIL_WIN2K           = 5,
   OS_DETAIL_WIN2K_PRO       = 6,
   OS_DETAIL_WIN2K_SERV      = 7,
   OS_DETAIL_WIN2K_ADV_SERV  = 8,
   OS_DETAIL_WINXP           = 9,
   OS_DETAIL_WINXP_HOME      = 10,
   OS_DETAIL_WINXP_PRO       = 11,
   OS_DETAIL_WINXP_X64_PRO   = 12,
   OS_DETAIL_WIN2K3          = 13,
   OS_DETAIL_WIN2K3_WEB      = 14,
   OS_DETAIL_WIN2K3_ST       = 15,
   OS_DETAIL_WIN2K3_EN       = 16,
   OS_DETAIL_WIN2K3_BUS      = 17,
   OS_DETAIL_VISTA           = 18,
   OS_DETAIL_WIN2K8          = 19,
   OS_DETAIL_WINSEVEN        = 20,    // Windows 7
   OS_DETAIL_WIN2K8R2        = 21,
   OS_DETAIL_UNKNOWN         = 99999  // last, highest value
} OS_DETAIL_TYPE;

/* generic names (to protect the future) but Windows specific for now */
OS_TYPE Hostinfo_GetOSType(void);
OS_DETAIL_TYPE Hostinfo_GetOSDetailType(void);

Bool Hostinfo_GetPCFrequency(uint64 *pcHz);
Bool Hostinfo_GetMhzOfProcessor(int32 processorNumber,
				uint32 *currentMhz, uint32 *maxMhz);
uint64 Hostinfo_SystemIdleTime(void);
Bool Hostinfo_GetAllCpuid(CPUIDQuery *query);
#endif
void Hostinfo_LogLoadAverage(void);
Bool Hostinfo_GetLoadAverage(uint32 *l);

#ifdef __APPLE__
size_t Hostinfo_GetKernelZoneElemSize(char const *name);
#endif

#ifdef _WIN32
static INLINE Bool
Hostinfo_AtLeastVista(void)
{
   return (Hostinfo_GetOSType() >= OS_VISTA);
}
#endif

#endif /* ifndef _HOSTINFO_H_ */
