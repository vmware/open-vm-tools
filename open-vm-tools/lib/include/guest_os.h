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

#ifndef _GUEST_OS_H_
#define _GUEST_OS_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/*
 * There's a max of 64 guests that can be defined in this list below.
 * Be conservative and only declare entries in this list if you need to refer
 * to the guest specifically in vmx/main/guest_os.c,
 * vmcore/vmx/main/monitorControl.c, or similar. Don't rely on every supported
 * guest having an entry in this list.
 */
typedef enum GuestOSType {
   GUEST_OS_BASE                = 0x5000,

   GUEST_OS_ANY                 = GUEST_OS_BASE + 0,
   GUEST_OS_DOS                 = GUEST_OS_BASE + 1,
   GUEST_OS_WIN31               = GUEST_OS_BASE + 2,
   GUEST_OS_WIN95               = GUEST_OS_BASE + 3,
   GUEST_OS_WIN98               = GUEST_OS_BASE + 4,
   GUEST_OS_WINME               = GUEST_OS_BASE + 5,
   GUEST_OS_WINNT               = GUEST_OS_BASE + 6,
   GUEST_OS_WIN2000             = GUEST_OS_BASE + 7,
   GUEST_OS_WINXP               = GUEST_OS_BASE + 8,
   GUEST_OS_WINXPPRO_64         = GUEST_OS_BASE + 9,
   GUEST_OS_WINNET              = GUEST_OS_BASE + 10,
   GUEST_OS_WINNET_64           = GUEST_OS_BASE + 11,
   GUEST_OS_LONGHORN            = GUEST_OS_BASE + 12,
   GUEST_OS_LONGHORN_64         = GUEST_OS_BASE + 13,
   GUEST_OS_WINVISTA            = GUEST_OS_BASE + 14,
   GUEST_OS_WINVISTA_64         = GUEST_OS_BASE + 15,
   GUEST_OS_UBUNTU              = GUEST_OS_BASE + 16,
   GUEST_OS_OTHER24XLINUX       = GUEST_OS_BASE + 17,
   GUEST_OS_OTHER24XLINUX_64    = GUEST_OS_BASE + 18,
   GUEST_OS_OTHER26XLINUX       = GUEST_OS_BASE + 19,
   GUEST_OS_OTHER26XLINUX_64    = GUEST_OS_BASE + 20,
   GUEST_OS_OTHERLINUX          = GUEST_OS_BASE + 21,
   GUEST_OS_OTHERLINUX_64       = GUEST_OS_BASE + 22,
   GUEST_OS_OS2                 = GUEST_OS_BASE + 23,
   GUEST_OS_OTHER               = GUEST_OS_BASE + 24,
   GUEST_OS_OTHER_64            = GUEST_OS_BASE + 25,
   GUEST_OS_FREEBSD             = GUEST_OS_BASE + 26,
   GUEST_OS_FREEBSD_64          = GUEST_OS_BASE + 27,
   GUEST_OS_NETWARE4            = GUEST_OS_BASE + 28,
   GUEST_OS_NETWARE5            = GUEST_OS_BASE + 29,
   GUEST_OS_NETWARE6            = GUEST_OS_BASE + 30,
   GUEST_OS_SOLARIS6            = GUEST_OS_BASE + 31,
   GUEST_OS_SOLARIS7            = GUEST_OS_BASE + 32,
   GUEST_OS_SOLARIS8            = GUEST_OS_BASE + 33,
   GUEST_OS_SOLARIS9            = GUEST_OS_BASE + 34,
   GUEST_OS_SOLARIS10           = GUEST_OS_BASE + 35,
   GUEST_OS_SOLARIS10_64        = GUEST_OS_BASE + 36,
   GUEST_OS_VMKERNEL            = GUEST_OS_BASE + 37,
   GUEST_OS_DARWIN              = GUEST_OS_BASE + 38,
   GUEST_OS_DARWIN_64           = GUEST_OS_BASE + 39,
   GUEST_OS_OPENSERVER5         = GUEST_OS_BASE + 40,
   GUEST_OS_OPENSERVER6         = GUEST_OS_BASE + 41,
   GUEST_OS_UNIXWARE7           = GUEST_OS_BASE + 42,
} GuestOSType;


typedef enum GuestOSFamilyType {
   GUEST_OS_FAMILY_ANY         = 0x0000,
   GUEST_OS_FAMILY_LINUX       = 0x0001,
   GUEST_OS_FAMILY_WINDOWS     = 0x0002,
   GUEST_OS_FAMILY_WIN9X       = 0x0004,
   GUEST_OS_FAMILY_WINNT       = 0x0008,
   GUEST_OS_FAMILY_WIN2000     = 0x0010,
   GUEST_OS_FAMILY_WINXP       = 0x0020,
   GUEST_OS_FAMILY_WINNET      = 0x0040,
   GUEST_OS_FAMILY_NETWARE     = 0x0080
} GuestOSFamilyType;

#define B(guest)	((uint64) 1 << ((guest) - GUEST_OS_BASE))
#define BS(suf)		B(GUEST_OS_##suf)
#define ALLWIN9X	(BS(WIN95) | BS(WIN98) | BS(WINME))
#define ALLWIN2000	BS(WIN2000)

#define ALLWINXP32	BS(WINXP)
#define ALLWINXP64	BS(WINXPPRO_64)
#define ALLWINXP        (ALLWINXP32 | ALLWINXP64)

#define ALLFREEBSD      BS(FREEBSD) | BS(FREEBSD_64)

#define ALLWINNET32	BS(WINNET)
#define ALLWINNET64	BS(WINNET_64)
#define ALLWINNET	(ALLWINNET32 | ALLWINNET64)

#define ALLWINVISTA32   (BS(LONGHORN) | BS(WINVISTA))
#define ALLWINVISTA64   (BS(LONGHORN_64) | BS(WINVISTA_64))
#define ALLWINVISTA     (ALLWINVISTA32 | ALLWINVISTA64)

#define ALLWINNT32	(BS(WINNT) | ALLWIN2000 | ALLWINXP32 | ALLWINNET32 | \
                      ALLWINVISTA32)
#define ALLWINNT64	(ALLWINXP64 | ALLWINNET64 | ALLWINVISTA64)
#define ALLWINNT	(ALLWINNT32 | ALLWINNT64)

#define ALLWIN32	(ALLWIN9X | ALLWINNT32)
#define ALLWIN64	 ALLWINNT64
#define ALLWIN          (ALLWIN32 | ALLWIN64)
#define ALLSOLARIS      (BS(SOLARIS6) | BS(SOLARIS7) | BS(SOLARIS8) | \
                         BS(SOLARIS9) | BS(SOLARIS10) | BS(SOLARIS10_64))
#define ALLNETWARE      (BS(NETWARE4) | BS(NETWARE5) | BS(NETWARE6))
#define ALLLINUX32      (BS(UBUNTU) | BS(OTHER24XLINUX) | BS(VMKERNEL) | \
                         BS(OTHER26XLINUX) | BS(OTHERLINUX))
#define ALLLINUX64      (BS(OTHERLINUX_64) | BS(OTHER24XLINUX_64) | \
                         BS(OTHER26XLINUX_64))
#define ALLLINUX        (ALLLINUX32 | ALLLINUX64)
#define ALLDARWIN       (BS(DARWIN) | BS(DARWIN_64))
#define ALL64           (ALLWIN64 | ALLLINUX64 | BS(LONGHORN_64) | \
                         BS(SOLARIS10_64) | BS(FREEBSD_64) | \
                         BS(WINVISTA_64) | BS(DARWIN_64) | BS(OTHER_64))


/*
 * These constants are generated by GuestInfoGetOSName which is in
 * the bora-vmsoft subtree.
 */

/* Linux */
#define STR_OS_ANNVIX "Annvix" 
#define STR_OS_ARCH "Arch" 
#define STR_OS_ARKLINUX "Arklinux" 
#define STR_OS_AUROX "Aurox" 
#define STR_OS_ASIANUX "asianux" 
#define STR_OS_BLACKCAT "BlackCat" 
#define STR_OS_COBALT "Cobalt" 
#define STR_OS_CONECTIVA "Conectiva" 
#define STR_OS_DEBIAN "Debian" 
#define STR_OS_FEDORA "Fedora" 
#define STR_OS_GENTOO "Gentoo" 
#define STR_OS_IMMUNIX "Immunix" 
#define STR_OS_LINUX "linux" 
#define STR_OS_LINUX_FROM_SCRATCH "Linux-From-Scratch" 
#define STR_OS_LINUX_FULL "Other Linux"
#define STR_OS_LINUX_PPC "Linux-PPC" 
#define STR_OS_MANDRAKE "mandrake" 
#define STR_OS_MANDRAKE_FULL "Mandrake Linux"   
#define STR_OS_MANDRIVA "mandriva"    
#define STR_OS_MKLINUX "MkLinux"    
#define STR_OS_NOVELL "nld9"    
#define STR_OS_NOVELL_FULL "Novell Linux Desktop 9" 
#define STR_OS_OTHER "otherlinux"    
#define STR_OS_OTHER_24 "other24xlinux"    
#define STR_OS_OTHER_24_FULL "Other Linux 2.4.x kernel" 
#define STR_OS_OTHER_26 "other26xlinux"    
#define STR_OS_OTHER_26_FULL "Other Linux 2.6.x kernel" 
#define STR_OS_OTHER_FULL "Other Linux"   
#define STR_OS_PLD "PLD"    
#define STR_OS_RED_HAT  "redhat"   
#define STR_OS_RED_HAT_EN "rhel"    
#define STR_OS_RED_HAT_EN_2 "rhel2"    
#define STR_OS_RED_HAT_EN_2_FULL "Red Hat Enterprise Linux 2"
#define STR_OS_RED_HAT_EN_3 "rhel3"    
#define STR_OS_RED_HAT_EN_3_FULL "Red Hat Enterprise Linux 3"
#define STR_OS_RED_HAT_EN_4 "rhel4"    
#define STR_OS_RED_HAT_EN_4_FULL "Red Hat Enterprise Linux 4"
#define STR_OS_RED_HAT_FULL "Red Hat Linux"  
#define STR_OS_SLACKWARE "Slackware"    
#define STR_OS_SMESERVER "SMEServer"    
#define STR_OS_SUN_DESK "sjds"    
#define STR_OS_SUN_DESK_FULL "Sun Java Desktop System" 
#define STR_OS_SUSE "suse"    
#define STR_OS_SUSE_EN "sles"    
#define STR_OS_SUSE_EN_FULL "SUSE Linux Enterprise Server" 
#define STR_OS_SUSE_FULL "SUSE Linux"   
#define STR_OS_TINYSOFA "Tiny Sofa"   
#define STR_OS_TURBO "turbolinux"    
#define STR_OS_TURBO_FULL "Turbolinux"    
#define STR_OS_UBUNTU "Ubuntu" 
#define STR_OS_ULTRAPENGUIN "UltraPenguin" 
#define STR_OS_UNITEDLINUX "UnitedLinux" 
#define STR_OS_VALINUX "VALinux" 
#define STR_OS_YELLOW_DOG "Yellow Dog"

/* Windows */
#define STR_OS_WIN_31 "win31"
#define STR_OS_WIN_31_FULL "Windows 3.1"
#define STR_OS_WIN_95 "win95"
#define STR_OS_WIN_95_FULL "Windows 95"
#define STR_OS_WIN_98 "win98"
#define STR_OS_WIN_98_FULL "Windows 98"
#define STR_OS_WIN_ME "winMe"
#define STR_OS_WIN_ME_FULL "Windows Me"
#define STR_OS_WIN_NT "winNT"
#define STR_OS_WIN_NT_FULL "Windows NT"
#define STR_OS_WIN_2000_PRO "win2000Pro"
#define STR_OS_WIN_2000_PRO_FULL "Windows 2000 Professional"
#define STR_OS_WIN_2000_SERV   "win2000Serv"
#define STR_OS_WIN_2000_SERV_FULL "Windows 2000 Server"
#define STR_OS_WIN_2000_ADV_SERV "win2000AdvServ"
#define STR_OS_WIN_2000_ADV_SERV_FULL "Windows 2000 Advanced Server"
#define STR_OS_WIN_2000_DATACENT_SERV "win2000DataCentServ"
#define STR_OS_WIN_2000_DATACENT_SERV_FULL "Windows 2000 Data Center Server"
#define STR_OS_WIN_XP_HOME "winXPHome"
#define STR_OS_WIN_XP_HOME_FULL "Windows XP Home Edition"
#define STR_OS_WIN_XP_PRO   "winXPPro"
#define STR_OS_WIN_XP_PRO_FULL "Windows XP Professional"
#define STR_OS_WIN_XP_PRO_X64   "winXPPro-64"
#define STR_OS_WIN_XP_PRO_X64_FULL "Windows XP Professional x64 Edition"
#define STR_OS_WIN_NET_WEB  "winNetWeb" 
#define STR_OS_WIN_NET_WEB_FULL "Windows Server 2003 Web Edition"
#define STR_OS_WIN_NET_ST "winNetStandard"
#define STR_OS_WIN_NET_ST_FULL "Windows Server 2003 Standard Edition"
#define STR_OS_WIN_NET_EN "winNetEnterprise"
#define STR_OS_WIN_NET_EN_FULL "Windows Server 2003 Enterprise Edition"
#define STR_OS_WIN_NET_BUS "winNetBusiness"
#define STR_OS_WIN_NET_BUS_FULL "Windows Server 2003 Small Business"
#define STR_OS_WIN_NET_COMPCLUSTER "winNetComputeCluster"
#define STR_OS_WIN_NET_COMPCLUSTER_FULL "Windows Server 2003 Compute Cluster Edition"
#define STR_OS_WIN_NET_STORAGESERVER "winNetStorageSvr"
#define STR_OS_WIN_NET_STORAGESERVER_FULL "Windows Storage Server 2003"
#define STR_OS_WIN_NET_DC_FULL "Windows Server 2003 Datacenter Edition"
#define STR_OS_WIN_NET_DC "winNetDatacenter"
#define STR_OS_WIN_LONG "longhorn"
#define STR_OS_WIN_LONG_FULL "Longhorn (experimental)"
#define STR_OS_WIN_VISTA "winVista"
#define STR_OS_WIN_VISTA_FULL "Windows Vista"
#define STR_OS_WIN_VISTA_X64 "winVista-64"
#define STR_OS_WIN_VISTA_X64_FULL "Windows Vista x64 Edition"
#define STR_OS_WIN_VISTA_ULTIMATE "winVistaUltimate-32"
#define STR_OS_WIN_VISTA_ULTIMATE_FULL "Windows Vista Ultimate Edition"
#define STR_OS_WIN_VISTA_HOME_PREMIUM "winVistaHomePremium-32"
#define STR_OS_WIN_VISTA_HOME_PREMIUM_FULL "Windows Vista Home Premium Edition"
#define STR_OS_WIN_VISTA_HOME_BASIC "winVistaHomeBasic-32"
#define STR_OS_WIN_VISTA_HOME_BASIC_FULL "Windows Vista Home Basic Edition"
#define STR_OS_WIN_VISTA_ENTERPRISE "winVistaEnterprise-32"
#define STR_OS_WIN_VISTA_ENTERPRISE_FULL "Windows Vista Enterprise Edition"
#define STR_OS_WIN_VISTA_BUSINESS "winVistaBusiness-32"
#define STR_OS_WIN_VISTA_BUSINESS_FULL "Windows Vista Business Edition"
#define STR_OS_WIN_VISTA_STARTER "winVistaStarter-32"
#define STR_OS_WIN_VISTA_STARTER_FULL "Windows Vista Starter Edition"
#define STR_OS_WIN_2008_CLUSTER "winServer2008Cluster-32"
#define STR_OS_WIN_2008_CLUSTER_FULL "Windows Server 2008 Cluster Server Edition"
#define STR_OS_WIN_2008_DATACENTER "winServer2008Datacenter-32"
#define STR_OS_WIN_2008_DATACENTER_FULL "Windows Server 2008 Datacenter Edition"
#define STR_OS_WIN_2008_DATACENTER_CORE "winServer2008DatacenterCore-32"
#define STR_OS_WIN_2008_DATACENTER_CORE_FULL "Windows Server 2008 Datacenter Edition (core installation)"
#define STR_OS_WIN_2008_ENTERPRISE "winServer2008Enterprise-32"
#define STR_OS_WIN_2008_ENTERPRISE_FULL "Windows Server 2008 Enterprise Edition"
#define STR_OS_WIN_2008_ENTERPRISE_CORE "winServer2008EnterpriseCore-32"
#define STR_OS_WIN_2008_ENTERPRISE_CORE_FULL "Windows Server 2008 Enterprise Edition (core installation)"
#define STR_OS_WIN_2008_ENTERPRISE_ITANIUM "winServer2008EnterpriseItanium-32"
#define STR_OS_WIN_2008_ENTERPRISE_ITANIUM_FULL "Windows Server 2008 Enterprise Edition for Itanium-based Systems"
#define STR_OS_WIN_2008_SMALL_BUSINESS "winServer2008SmallBusiness-32"
#define STR_OS_WIN_2008_SMALL_BUSINESS_FULL "Windows Server 2008 Small Business Server"
#define STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM "winServer2008SmallBusinessPremium-32"
#define STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM_FULL "Windows Server 2008 Small Business Server Premium Edition"
#define STR_OS_WIN_2008_STANDARD "winServer2008Standard-32"
#define STR_OS_WIN_2008_STANDARD_FULL "Windows Server 2008 Standard Edition"
#define STR_OS_WIN_2008_STANDARD_CORE "winServer2008StandardCore-32"
#define STR_OS_WIN_2008_STANDARD_CORE_FULL "Windows Server 2008 Standard Edition (core installation)"
#define STR_OS_WIN_2008_WEB_SERVER "winServer2008Web-32"
#define STR_OS_WIN_2008_WEB_SERVER_FULL "Windows Server 2008 Web Server Edition"
#define STR_OS_WIN_VISTA_ULTIMATE_X64 "winVistaUltimate-64"
#define STR_OS_WIN_VISTA_HOME_PREMIUM_X64 "winVistaHomePremium-64"
#define STR_OS_WIN_VISTA_HOME_BASIC_X64 "winVistaHomeBasic-64"
#define STR_OS_WIN_VISTA_ENTERPRISE_X64 "winVistaEnterprise-64"
#define STR_OS_WIN_VISTA_BUSINESS_X64 "winVistaBusiness-64"
#define STR_OS_WIN_VISTA_STARTER_X64 "winVistaStarter-64"
#define STR_OS_WIN_2008_CLUSTER_X64 "winServer2008Cluster-64"
#define STR_OS_WIN_2008_DATACENTER_X64 "winServer2008Datacenter-64"
#define STR_OS_WIN_2008_DATACENTER_CORE_X64 "winServer2008DatacenterCore-64"
#define STR_OS_WIN_2008_ENTERPRISE_X64 "winServer2008Enterprise-64"
#define STR_OS_WIN_2008_ENTERPRISE_CORE_X64 "winServer2008EnterpriseCore-64"
#define STR_OS_WIN_2008_ENTERPRISE_ITANIUM_X64 "winServer2008EnterpriseItanium-64"
#define STR_OS_WIN_2008_SMALL_BUSINESS_X64 "winServer2008SmallBusiness-64"
#define STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM_X64 "winServer2008SmallBusinessPremium-64"
#define STR_OS_WIN_2008_STANDARD_X64 "winServer2008Standard-64"
#define STR_OS_WIN_2008_STANDARD_CORE_X64 "winServer2008StandardCore-64"
#define STR_OS_WIN_2008_WEB_SERVER_X64 "winServer2008Web-64"

/* Modifiers for Windows Vista and Windows Server 2008 */
#define STR_OS_WIN_32_BIT_EXTENSION ", 32-bit"
#define STR_OS_WIN_64_BIT_EXTENSION ", 64-bit"

/* FreeBSD */
#define STR_OS_FREEBSD "FreeBSD"

/* Solaris */
#define STR_OS_SOLARIS "Solaris"

/* All */
#define STR_OS_64BIT_SUFFIX "-64"
#define STR_OS_64BIT_SUFFIX_FULL " (64 bit)"
#define STR_OS_EMPTY ""

#endif
