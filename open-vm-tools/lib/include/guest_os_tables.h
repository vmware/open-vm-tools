/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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

#ifndef _GUEST_OS_TABLES_H_
#define _GUEST_OS_TABLES_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define GUEST_OS_TYPE_GEN                                                  \
   GOT(GUEST_OS_ANY)                                                       \
   GOT(GUEST_OS_DOS)                                                       \
   GOT(GUEST_OS_WIN31)                                                     \
   GOT(GUEST_OS_WIN95)                                                     \
   GOT(GUEST_OS_WIN98)                                                     \
   GOT(GUEST_OS_WINME)                                                     \
   GOT(GUEST_OS_WINNT)                                                     \
   GOT(GUEST_OS_WIN2000)                                                   \
   GOT(GUEST_OS_WINXP)                                                     \
   GOT(GUEST_OS_WINXPPRO_64)                                               \
   GOT(GUEST_OS_WINNET)                                                    \
   GOT(GUEST_OS_WINNET_64)                                                 \
   GOT(GUEST_OS_LONGHORN)                                                  \
   GOT(GUEST_OS_LONGHORN_64)                                               \
   GOT(GUEST_OS_WINVISTA)                                                  \
   GOT(GUEST_OS_WINVISTA_64)                                               \
   GOT(GUEST_OS_WINSEVEN)          /* Windows 7 */                         \
   GOT(GUEST_OS_WINSEVEN_64)       /* Windows 7 */                         \
   GOT(GUEST_OS_WIN2008R2_64)      /* Server 2008 R2 */                    \
   GOT(GUEST_OS_WINEIGHT)          /* Windows 8 */                         \
   GOT(GUEST_OS_WINEIGHT_64)       /* Windows 8 x64 */                     \
   GOT(GUEST_OS_WINEIGHTSERVER_64) /* Windows 8 Server X64 */              \
   GOT(GUEST_OS_WINTEN)            /* Windows 10 */                        \
   GOT(GUEST_OS_WINTEN_64)         /* Windows 10 x64 */                    \
   GOT(GUEST_OS_WIN_2016SRV_64)    /* Windows Server 2016 X64 */           \
   GOT(GUEST_OS_WIN_2019SRV_64)    /* Windows Server 2019 X64 */           \
   GOT(GUEST_OS_HYPER_V)           /* Microsoft Hyper-V */                 \
   GOT(GUEST_OS_OS2)                                                       \
   GOT(GUEST_OS_ECOMSTATION)       /* OS/2 variant; 1.x */                 \
   GOT(GUEST_OS_ECOMSTATION2)      /* OS/2 variant; 2.x */                 \
   GOT(GUEST_OS_OTHERLINUX)                                                \
   GOT(GUEST_OS_OTHERLINUX_64)                                             \
   GOT(GUEST_OS_OTHER24XLINUX)                                             \
   GOT(GUEST_OS_OTHER24XLINUX_64)                                          \
   GOT(GUEST_OS_OTHER26XLINUX)                                             \
   GOT(GUEST_OS_OTHER26XLINUX_64)                                          \
   GOT(GUEST_OS_OTHER3XLINUX)      /* Linux 3.x */                         \
   GOT(GUEST_OS_OTHER3XLINUX_64)   /* Linux 3.x X64 */                     \
   GOT(GUEST_OS_OTHER4XLINUX)      /* Linux 4.x */                         \
   GOT(GUEST_OS_OTHER4XLINUX_64)   /* Linux 4.x X64 */                     \
   GOT(GUEST_OS_OTHER5XLINUX)      /* Linux 5.x and later */               \
   GOT(GUEST_OS_OTHER5XLINUX_64)   /* Linux 5.x and later X64 */           \
   GOT(GUEST_OS_OTHER)                                                     \
   GOT(GUEST_OS_OTHER_64)                                                  \
   GOT(GUEST_OS_UBUNTU)                                                    \
   GOT(GUEST_OS_DEBIAN)                                                    \
   GOT(GUEST_OS_DEBIAN_64)                                                 \
   GOT(GUEST_OS_RHEL)                                                      \
   GOT(GUEST_OS_RHEL_64)                                                   \
   GOT(GUEST_OS_FREEBSD)                                                   \
   GOT(GUEST_OS_FREEBSD_64)                                                \
   GOT(GUEST_OS_FREEBSD11)                                                 \
   GOT(GUEST_OS_FREEBSD11_64)                                              \
   GOT(GUEST_OS_FREEBSD12)                                                 \
   GOT(GUEST_OS_FREEBSD12_64)                                              \
   GOT(GUEST_OS_SOLARIS_6_AND_7)                                           \
   GOT(GUEST_OS_SOLARIS8)                                                  \
   GOT(GUEST_OS_SOLARIS9)                                                  \
   GOT(GUEST_OS_SOLARIS10)                                                 \
   GOT(GUEST_OS_SOLARIS10_64)                                              \
   GOT(GUEST_OS_SOLARIS11_64)                                              \
   GOT(GUEST_OS_DARWIN9)           /* Mac OS 10.5 */                       \
   GOT(GUEST_OS_DARWIN9_64)                                                \
   GOT(GUEST_OS_DARWIN10)          /* Mac OS 10.6 */                       \
   GOT(GUEST_OS_DARWIN10_64)                                               \
   GOT(GUEST_OS_DARWIN11)          /* Mac OS 10.7 */                       \
   GOT(GUEST_OS_DARWIN11_64)                                               \
   GOT(GUEST_OS_DARWIN12_64)       /* Mac OS 10.8 */                       \
   GOT(GUEST_OS_DARWIN13_64)       /* Mac OS 10.9 */                       \
   GOT(GUEST_OS_DARWIN14_64)       /* Mac OS 10.10 */                      \
   GOT(GUEST_OS_DARWIN15_64)       /* Mac OS 10.11 */                      \
   GOT(GUEST_OS_DARWIN16_64)       /* Mac OS 10.12 */                      \
   GOT(GUEST_OS_DARWIN17_64)       /* Mac OS 10.13 */                      \
   GOT(GUEST_OS_DARWIN18_64)       /* Mac OS 10.14 */                      \
   GOT(GUEST_OS_DARWIN19_64)       /* Mac OS 10.15 */                      \
   GOT(GUEST_OS_DARWIN20_64)       /* Mac OS 10.16 */                      \
   GOT(GUEST_OS_OPENSERVER_5_AND_6)                                        \
   GOT(GUEST_OS_UNIXWARE7)                                                 \
   GOT(GUEST_OS_NETWARE4)                                                  \
   GOT(GUEST_OS_NETWARE5)                                                  \
   GOT(GUEST_OS_NETWARE6)                                                  \
   GOT(GUEST_OS_VMKERNEL)          /* ESX 4.x */                           \
   GOT(GUEST_OS_VMKERNEL5)         /* ESX 5.x */                           \
   GOT(GUEST_OS_VMKERNEL6)         /* ESX 6 */                             \
   GOT(GUEST_OS_VMKERNEL65)        /* ESX 6.5 */                           \
   GOT(GUEST_OS_VMKERNEL7)         /* ESX 7 and later */                   \
   GOT(GUEST_OS_PHOTON_64)         /* VMware Photon IA 64-bit */           \
   GOT(GUEST_OS_ORACLE)                                                    \
   GOT(GUEST_OS_ORACLE_64)                                                 \
   GOT(GUEST_OS_ORACLE6)                                                   \
   GOT(GUEST_OS_ORACLE6_64)                                                \
   GOT(GUEST_OS_ORACLE7_64)                                                \
   GOT(GUEST_OS_ORACLE8_64)                                                \
   GOT(GUEST_OS_CENTOS)                                                    \
   GOT(GUEST_OS_CENTOS_64)                                                 \
   GOT(GUEST_OS_CENTOS6)                                                   \
   GOT(GUEST_OS_CENTOS6_64)                                                \
   GOT(GUEST_OS_CENTOS7_64)                                                \
   GOT(GUEST_OS_CENTOS8_64)                                                \
   GOT(GUEST_OS_AMAZONLINUX2_64)                                           \
   GOT(GUEST_OS_CRXSYS1_64)        /* VMware CRX system VM 1.0 64-bit */   \
   GOT(GUEST_OS_CRXPOD1_64)        /* VMware CRX pod VM 1.0 64-bit */      \
   GOT(GUEST_OS_LINUX_MINT_64)


/*
 * Mappings between VIM guest OS keys and the rest of the civilized world.
 *
 * Format: GOKM(vmxKey, vimKey, reversible)
 */
#define GUEST_OS_KEY_MAP \
   /* Windows guests */ \
   GOKM("win31",                                win31Guest,              TRUE) \
   GOKM("win95",                                win95Guest,              TRUE) \
   GOKM("win98",                                win98Guest,              TRUE) \
   GOKM("winMe",                                winMeGuest,              TRUE) \
   GOKM("winNT",                                winNTGuest,              TRUE) \
   GOKM("nt4",                                  winNTGuest,              FALSE) \
   GOKM("win2000",                              win2000ProGuest,         FALSE) \
   GOKM("win2000Pro",                           win2000ProGuest,         TRUE) \
   GOKM("win2000Serv",                          win2000ServGuest,        TRUE) \
   GOKM("win2000AdvServ",                       win2000AdvServGuest,     TRUE) \
   GOKM("winXPHome",                            winXPHomeGuest,          TRUE) \
   GOKM("whistler",                             winXPHomeGuest,          FALSE) \
   GOKM("winXPPro",                             winXPProGuest,           TRUE) \
   GOKM("winXPPro-64",                          winXPPro64Guest,         TRUE) \
   GOKM("winNetWeb",                            winNetWebGuest,          TRUE) \
   GOKM("winNetStandard",                       winNetStandardGuest,     TRUE) \
   GOKM("winNetEnterprise",                     winNetEnterpriseGuest,   TRUE) \
   GOKM("winNetDatacenter",                     winNetDatacenterGuest,   TRUE) \
   GOKM("winNetBusiness",                       winNetBusinessGuest,     TRUE) \
   GOKM("winNetStandard-64",                    winNetStandard64Guest,   TRUE) \
   GOKM("winNetEnterprise-64",                  winNetEnterprise64Guest, TRUE) \
   GOKM("winNetDatacenter-64",                  winNetDatacenter64Guest, TRUE) \
   GOKM("longhorn",                             winLonghornGuest,        TRUE) \
   GOKM("longhorn-64",                          winLonghorn64Guest,      TRUE) \
   GOKM("winvista",                             winVistaGuest,           TRUE) \
   GOKM("winvista-64",                          winVista64Guest,         TRUE) \
   GOKM("windows7",                             windows7Guest,           TRUE) \
   GOKM("windows7-64",                          windows7_64Guest,        TRUE) \
   GOKM("windows7srv-64",                       windows7Server64Guest,   TRUE) \
   GOKM("windows8",                             windows8Guest,           TRUE) \
   GOKM("windows8-64",                          windows8_64Guest,        TRUE) \
   GOKM("windows8srv-64",                       windows8Server64Guest,   TRUE) \
   GOKM("windows9",                             windows9Guest,           TRUE) \
   GOKM("windows9-64",                          windows9_64Guest,        TRUE) \
   GOKM("windows9srv-64",                       windows9Server64Guest,   TRUE) \
   GOKM("windows2019srv-64",                    windows2019srv_64Guest,  TRUE) \
   GOKM("winHyperV",                            windowsHyperVGuest,      TRUE) \
   GOKM("winServer2008Cluster-32",              winLonghornGuest,        FALSE) \
   GOKM("winServer2008Datacenter-32",           winLonghornGuest,        FALSE) \
   GOKM("winServer2008DatacenterCore-32",       winLonghornGuest,        FALSE) \
   GOKM("winServer2008Enterprise-32",           winLonghornGuest,        FALSE) \
   GOKM("winServer2008EnterpriseCore-32",       winLonghornGuest,        FALSE) \
   GOKM("winServer2008EnterpriseItanium-32",    winLonghornGuest,        FALSE) \
   GOKM("winServer2008SmallBusiness-32",        winLonghornGuest,        FALSE) \
   GOKM("winServer2008SmallBusinessPremium-32", winLonghornGuest,        FALSE) \
   GOKM("winServer2008Standard-32",             winLonghornGuest,        FALSE) \
   GOKM("winServer2008StandardCore-32",         winLonghornGuest,        FALSE) \
   GOKM("winServer2008MediumManagement-32",     winLonghornGuest,        FALSE) \
   GOKM("winServer2008MediumMessaging-32",      winLonghornGuest,        FALSE) \
   GOKM("winServer2008MediumSecurity-32",       winLonghornGuest,        FALSE) \
   GOKM("winServer2008ForSmallBusiness-32",     winLonghornGuest,        FALSE) \
   GOKM("winServer2008StorageEnterprise-32",    winLonghornGuest,        FALSE) \
   GOKM("winServer2008StorageExpress-32",       winLonghornGuest,        FALSE) \
   GOKM("winServer2008StorageStandard-32",      winLonghornGuest,        FALSE) \
   GOKM("winServer2008StorageWorkgroup-32",     winLonghornGuest,        FALSE) \
   GOKM("winServer2008Web-32",                  winLonghornGuest,        FALSE) \
   GOKM("winServer2008Cluster-64",              winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008Datacenter-64",           winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008DatacenterCore-64",       winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008Enterprise-64",           winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008EnterpriseCore-64",       winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008EnterpriseItanium-64",    winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008SmallBusiness-64",        winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008SmallBusinessPremium-64", winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008Standard-64",             winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008StandardCore-64",         winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008MediumManagement-64",     winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008MediumMessaging-64",      winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008MediumSecurity-64",       winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008ForSmallBusiness-64",     winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008StorageEnterprise-64",    winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008StorageExpress-64",       winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008StorageStandard-64",      winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008StorageWorkgroup-64",     winLonghorn64Guest,      FALSE) \
   GOKM("winServer2008Web-64",                  winLonghorn64Guest,      FALSE) \
   GOKM("winVistaUltimate-32",                  winVistaGuest,           FALSE) \
   GOKM("winVistaHomePremium-32",               winVistaGuest,           FALSE) \
   GOKM("winVistaHomeBasic-32",                 winVistaGuest,           FALSE) \
   GOKM("winVistaEnterprise-32",                winVistaGuest,           FALSE) \
   GOKM("winVistaBusiness-32",                  winVistaGuest,           FALSE) \
   GOKM("winVistaStarter-32",                   winVistaGuest,           FALSE) \
   GOKM("winVistaUltimate-64",                  winVista64Guest,         FALSE) \
   GOKM("winVistaHomePremium-64",               winVista64Guest,         FALSE) \
   GOKM("winVistaHomeBasic-64",                 winVista64Guest,         FALSE) \
   GOKM("winVistaEnterprise-64",                winVista64Guest,         FALSE) \
   GOKM("winVistaBusiness-64",                  winVista64Guest,         FALSE) \
   GOKM("winVistaStarter-64",                   winVista64Guest,         FALSE) \
   /* Linux guests */ \
   GOKM("redhat",                               redhatGuest,             TRUE) \
   GOKM("rhel2",                                rhel2Guest,              TRUE) \
   GOKM("rhel3",                                rhel3Guest,              TRUE) \
   GOKM("rhel3-64",                             rhel3_64Guest,           TRUE) \
   GOKM("rhel4",                                rhel4Guest,              TRUE) \
   GOKM("rhel4-64",                             rhel4_64Guest,           TRUE) \
   GOKM("rhel5",                                rhel5Guest,              TRUE) \
   GOKM("rhel5-64",                             rhel5_64Guest,           TRUE) \
   GOKM("rhel6",                                rhel6Guest,              TRUE) \
   GOKM("rhel6-64",                             rhel6_64Guest,           TRUE) \
   GOKM("rhel7",                                rhel7Guest,              TRUE) \
   GOKM("rhel7-64",                             rhel7_64Guest,           TRUE) \
   GOKM("rhel8-64",                             rhel8_64Guest,           TRUE) \
   GOKM("centos",                               centosGuest,             TRUE) \
   GOKM("centos-64",                            centos64Guest,           TRUE) \
   GOKM("centos6",                              centos6Guest,            TRUE) \
   GOKM("centos6-64",                           centos6_64Guest,         TRUE) \
   GOKM("centos7",                              centos7Guest,            FALSE) \
   GOKM("centos7-64",                           centos7_64Guest,         TRUE) \
   GOKM("centos8-64",                           centos8_64Guest,         TRUE) \
   GOKM("oraclelinux",                          oracleLinuxGuest,        TRUE) \
   GOKM("oraclelinux-64",                       oracleLinux64Guest,      TRUE) \
   GOKM("oraclelinux6",                         oracleLinux6Guest,       TRUE) \
   GOKM("oraclelinux6-64",                      oracleLinux6_64Guest,    TRUE) \
   GOKM("oraclelinux7",                         oracleLinux7Guest,       FALSE) \
   GOKM("oraclelinux7-64",                      oracleLinux7_64Guest,    TRUE) \
   GOKM("oraclelinux8-64",                      oracleLinux8_64Guest,    TRUE) \
   GOKM("suse",                                 suseGuest,               TRUE) \
   GOKM("suse-64",                              suse64Guest,             TRUE) \
   GOKM("sles",                                 slesGuest,               TRUE) \
   GOKM("sles-64",                              sles64Guest,             TRUE) \
   GOKM("sles10",                               sles10Guest,             TRUE) \
   GOKM("sles10-64",                            sles10_64Guest,          TRUE) \
   GOKM("sles11",                               sles11Guest,             TRUE) \
   GOKM("sles11-64",                            sles11_64Guest,          TRUE) \
   GOKM("sles12",                               sles12Guest,             TRUE) \
   GOKM("sles12-64",                            sles12_64Guest,          TRUE) \
   GOKM("sles15-64",                            sles15_64Guest,          TRUE) \
   GOKM("mandrake",                             mandrakeGuest,           TRUE) \
   GOKM("mandrake-64",                          mandriva64Guest,         FALSE) \
   GOKM("mandriva",                             mandrivaGuest,           TRUE) \
   GOKM("mandriva-64",                          mandriva64Guest,         TRUE) \
   GOKM("turbolinux",                           turboLinuxGuest,         TRUE) \
   GOKM("turbolinux-64",                        turboLinux64Guest,       TRUE) \
   GOKM("ubuntu",                               ubuntuGuest,             TRUE) \
   GOKM("ubuntu-64",                            ubuntu64Guest,           TRUE) \
   GOKM("debian4",                              debian4Guest,            TRUE) \
   GOKM("debian4-64",                           debian4_64Guest,         TRUE) \
   GOKM("debian5",                              debian5Guest,            TRUE) \
   GOKM("debian5-64",                           debian5_64Guest,         TRUE) \
   GOKM("debian6",                              debian6Guest,            TRUE) \
   GOKM("debian6-64",                           debian6_64Guest,         TRUE) \
   GOKM("debian7",                              debian7Guest,            TRUE) \
   GOKM("debian7-64",                           debian7_64Guest,         TRUE) \
   GOKM("debian8",                              debian8Guest,            TRUE) \
   GOKM("debian8-64",                           debian8_64Guest,         TRUE) \
   GOKM("debian9",                              debian9Guest,            TRUE) \
   GOKM("debian9-64",                           debian9_64Guest,         TRUE) \
   GOKM("debian10",                             debian10Guest,           TRUE) \
   GOKM("debian10-64",                          debian10_64Guest,        TRUE) \
   GOKM("asianux3",                             asianux3Guest,           TRUE) \
   GOKM("asianux3-64",                          asianux3_64Guest,        TRUE) \
   GOKM("asianux4",                             asianux4Guest,           TRUE) \
   GOKM("asianux4-64",                          asianux4_64Guest,        TRUE) \
   GOKM("asianux5-64",                          asianux5_64Guest,        TRUE) \
   GOKM("asianux7-64",                          asianux7_64Guest,        TRUE) \
   GOKM("asianux8-64",                          asianux8_64Guest,        TRUE) \
   GOKM("nld9",                                 nld9Guest,               TRUE) \
   GOKM("oes",                                  oesGuest,                TRUE) \
   GOKM("sjds",                                 sjdsGuest,               TRUE) \
   GOKM("opensuse",                             opensuseGuest,           TRUE) \
   GOKM("opensuse-64",                          opensuse64Guest,         TRUE) \
   GOKM("fedora",                               fedoraGuest,             TRUE) \
   GOKM("fedora-64",                            fedora64Guest,           TRUE) \
   GOKM("coreos-64",                            coreos64Guest,           TRUE) \
   GOKM("vmware-photon-64",                     vmwarePhoton64Guest,     TRUE) \
   GOKM("other24xlinux",                        other24xLinuxGuest,      TRUE) \
   GOKM("other24xlinux-64",                     other24xLinux64Guest,    TRUE) \
   GOKM("other26xlinux",                        other26xLinuxGuest,      TRUE) \
   GOKM("other26xlinux-64",                     other26xLinux64Guest,    TRUE) \
   GOKM("other3xlinux",                         other3xLinuxGuest,       TRUE) \
   GOKM("other3xlinux-64",                      other3xLinux64Guest,     TRUE) \
   GOKM("other4xlinux",                         other4xLinuxGuest,       TRUE) \
   GOKM("other4xlinux-64",                      other4xLinux64Guest,     TRUE) \
   GOKM("linux",                                otherLinuxGuest,         FALSE) \
   GOKM("otherlinux",                           otherLinuxGuest,         TRUE) \
   GOKM("otherlinux-64",                        otherLinux64Guest,       TRUE) \
   GOKM("genericlinux",                         genericLinuxGuest,       TRUE) \
   GOKM("amazonlinux2-64",                      amazonlinux2_64Guest,    TRUE) \
   GOKM("CRXPod1-64",                           crxPod1Guest,            TRUE) \
   /* Netware guests */ \
   GOKM("netware4",                             netware4Guest,           TRUE) \
   GOKM("netware5",                             netware5Guest,           TRUE) \
   GOKM("netware6",                             netware6Guest,           TRUE) \
   /* Solaris guests */ \
   GOKM("solaris6",                             solaris6Guest,           TRUE) \
   GOKM("solaris7",                             solaris7Guest,           TRUE) \
   GOKM("solaris8",                             solaris8Guest,           TRUE) \
   GOKM("solaris9",                             solaris9Guest,           TRUE) \
   GOKM("solaris10",                            solaris10Guest,          TRUE) \
   GOKM("solaris10-64",                         solaris10_64Guest,       TRUE) \
   GOKM("solaris11-64",                         solaris11_64Guest,       TRUE) \
   /* macOS guests */ \
   GOKM("darwin",                               darwinGuest,             TRUE) \
   GOKM("darwin-64",                            darwin64Guest,           TRUE) \
   GOKM("darwin10",                             darwin10Guest,           TRUE) \
   GOKM("darwin10-64",                          darwin10_64Guest,        TRUE) \
   GOKM("darwin11",                             darwin11Guest,           TRUE) \
   GOKM("darwin11-64",                          darwin11_64Guest,        TRUE) \
   GOKM("darwin12-64",                          darwin12_64Guest,        TRUE) \
   GOKM("darwin13-64",                          darwin13_64Guest,        TRUE) \
   GOKM("darwin14-64",                          darwin14_64Guest,        TRUE) \
   GOKM("darwin15-64",                          darwin15_64Guest,        TRUE) \
   GOKM("darwin16-64",                          darwin16_64Guest,        TRUE) \
   GOKM("darwin17-64",                          darwin17_64Guest,        TRUE) \
   GOKM("darwin18-64",                          darwin18_64Guest,        TRUE) \
   GOKM("darwin19-64",                          darwin19_64Guest,        TRUE) \
   /* ESX guests */ \
   GOKM("vmkernel",                             vmkernelGuest,           TRUE) \
   GOKM("vmkernel5",                            vmkernel5Guest,          TRUE) \
   GOKM("vmkernel6",                            vmkernel6Guest,          TRUE) \
   GOKM("vmkernel65",                           vmkernel65Guest,         TRUE) \
   GOKM("vmkernel7",                            vmkernel7Guest,          TRUE) \
   /* Other guests */ \
   GOKM("dos",                                  dosGuest,                TRUE) \
   GOKM("os2",                                  os2Guest,                TRUE) \
   GOKM("os2experimental",                      os2Guest,                FALSE) \
   GOKM("eComStation",                          eComStationGuest,        TRUE) \
   GOKM("eComStation2",                         eComStation2Guest,       TRUE) \
   GOKM("freeBSD",                              freebsdGuest,            TRUE) \
   GOKM("freeBSD-64",                           freebsd64Guest,          TRUE) \
   GOKM("freeBSD11",                            freebsd11Guest,          TRUE) \
   GOKM("freeBSD11-64",                         freebsd11_64Guest,       TRUE) \
   GOKM("freeBSD12",                            freebsd12Guest,          TRUE) \
   GOKM("freeBSD12-64",                         freebsd12_64Guest,       TRUE) \
   GOKM("openserver5",                          openServer5Guest,        TRUE) \
   GOKM("openserver6",                          openServer6Guest,        TRUE) \
   GOKM("unixware7",                            unixWare7Guest,          TRUE) \
   GOKM("other",                                otherGuest,              TRUE) \
   GOKM("other-64",                             otherGuest64,            TRUE) \


#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
