/*********************************************************
 * Copyright (C) 1998-2021 VMware, Inc. All rights reserved.
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

#include "vm_basic_types.h"
#include "guest_os_tables.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * There's no practical max to the number of guests that can be defined in
 * the list below (guest IDs are limited to 2^32), but there is a maximum
 * of MAXGOSSET guests that can comprise a set, such as ALLLINUX, ALLDARWIN,
 * or ALLWIN64.
 *
 * Be conservative and only declare entries in this list if you need to refer
 * to the guest specifically in vmx/main/guest_os.c,
 * vmcore/vmx/main/monitorControl.c, or similar. Don't rely on every supported
 * guest having an entry in this list.
 */

typedef enum GuestOSType {
   GUEST_OS_BASE                = 0x5000,
   GUEST_OS_BASE_MINUS_ONE      = 0x4fff, /* So that ANY is equal to BASE */
#define GOT(_name) _name,
GUEST_OS_TYPE_GEN
#undef GOT
} GuestOSType;

/*
 * Maximum number of guests in a set, must be <= LIST_SIZE in geninfo.h
 */
#define MAXGOSSET 128

typedef enum GuestOSFamilyType {
   GUEST_OS_FAMILY_ANY         = 0x0000,
   GUEST_OS_FAMILY_LINUX       = 0x0001,
   GUEST_OS_FAMILY_WINDOWS     = 0x0002,
   GUEST_OS_FAMILY_WIN9X       = 0x0004,
   GUEST_OS_FAMILY_WINNT       = 0x0008,
   GUEST_OS_FAMILY_WIN2000     = 0x0010,
   GUEST_OS_FAMILY_WINXP       = 0x0020,
   GUEST_OS_FAMILY_WINNET      = 0x0040,
   GUEST_OS_FAMILY_NETWARE     = 0x0080,
   GUEST_OS_FAMILY_DARWIN      = 0x0100
} GuestOSFamilyType;

#define ALLOS                       GUEST_OS_ANY
#define BS(suf)                     GUEST_OS_##suf
#define GOS_IN_SET(gos, ...)        Gos_InSet(gos, __VA_ARGS__, 0)
#define GOS_IN_SET_ARRAY(gos, set)  Gos_InSetArray(gos, set)

Bool Gos_InSet(uint32 gos, ...);
Bool Gos_InSetArray(uint32 gos, const uint32 *set);

#define ALLWIN9X              BS(WIN95), BS(WIN98), BS(WINME)
#define ALLWIN2000            BS(WIN2000)

#define ALLWINXP32            BS(WINXP)
#define ALLWINXP64            BS(WINXPPRO_64)
#define ALLWINXP              ALLWINXP32, ALLWINXP64

#define ALLFREEBSD32          BS(FREEBSD),    BS(FREEBSD11),  \
                              BS(FREEBSD12),  BS(FREEBSD13),  \
                              BS(FREEBSD14)

#define ALLFREEBSD64          BS(FREEBSD_64),                           \
                              BS(FREEBSD11_64),  BS(FREEBSD12_64),      \
                              BS(FREEBSD13_64),  BS(FREEBSD13_ARM_64),  \
                              BS(FREEBSD14_64),  BS(FREEBSD14_ARM_64)

#define ALLFREEBSD            ALLFREEBSD32, ALLFREEBSD64

#define ALLWINNET32           BS(WINNET)
#define ALLWINNET64           BS(WINNET_64)
#define ALLWINNET             ALLWINNET32, ALLWINNET64

#define ALLWINLONGHORN32      BS(LONGHORN)
#define ALLWINLONGHORN64      BS(LONGHORN_64)
#define ALLWINLONGHORN        ALLWINLONGHORN32, ALLWINLONGHORN64

#define ALLWINVISTA32         BS(WINVISTA)
#define ALLWINVISTA64         BS(WINVISTA_64)
#define ALLWINVISTA           ALLWINVISTA32, ALLWINVISTA64

#define ALLWIN2008R2_64       BS(WIN2008R2_64)
#define ALLWIN2008R2          ALLWIN2008R2_64

#define ALLWINSEVEN32         BS(WIN_7)
#define ALLWINSEVEN64         BS(WIN_7_64)
#define ALLWINSEVEN           ALLWINSEVEN32, ALLWINSEVEN64

#define ALLWINEIGHTSERVER64   BS(WIN_8_SERVER_64)
#define ALLWINEIGHTSERVER     ALLWINEIGHTSERVER64

#define ALLWINEIGHTCLIENT32   BS(WIN_8)
#define ALLWINEIGHTCLIENT64   BS(WIN_8_64)
#define ALLWINEIGHTCLIENT     ALLWINEIGHTCLIENT32, ALLWINEIGHTCLIENT64

#define ALLWINEIGHT           ALLWINEIGHTSERVER, ALLWINEIGHTCLIENT

#define ALLWIN_10_SERVER64    BS(WIN_2016SRV_64), BS(WIN_2019SRV_64)

#define ALLWIN_10_SERVER      ALLWIN_10_SERVER64

#define ALLWIN_10_CLIENT32    BS(WIN_10)
#define ALLWIN_10_CLIENT64    BS(WIN_10_64), BS(WIN_10_ARM_64)
#define ALLWIN_10_CLIENT      ALLWIN_10_CLIENT32, ALLWIN_10_CLIENT64

#define ALLWIN_10_32          ALLWIN_10_CLIENT32
#define ALLWIN_10_64          ALLWIN_10_CLIENT64, ALLWIN_10_SERVER
#define ALLWIN_10             ALLWIN_10_CLIENT,   ALLWIN_10_SERVER

#define ALLWIN_11_SERVER64    BS(WIN_2022SRV_64), BS(WIN_2025SRV_64)
#define ALLWIN_11_SERVER      ALLWIN_11_SERVER64

#define ALLWIN_11_CLIENT64    BS(WIN_11_64), BS(WIN_11_ARM_64)
#define ALLWIN_11_CLIENT      ALLWIN_11_CLIENT64

#define ALLWIN_11_64          ALLWIN_11_CLIENT64, ALLWIN_11_SERVER
#define ALLWIN_11             ALLWIN_11_CLIENT64, ALLWIN_11_SERVER

#define ALLWIN_12_CLIENT64    BS(WIN_12_64), BS(WIN_12_ARM_64)
#define ALLWIN_12_CLIENT      ALLWIN_12_CLIENT64

#define ALLWIN_12             ALLWIN_12_CLIENT64

#define ALLHYPER_V            BS(HYPER_V)

#define ALLWINVISTA_OR_HIGHER ALLWINVISTA,         ALLWINLONGHORN,     \
                              ALLWIN2008R2,        ALLWINSEVEN,        \
                              ALLWINEIGHTSERVER,   ALLWINEIGHTCLIENT,  \
                              ALLWIN_10_SERVER,    ALLWIN_10_CLIENT,   \
                              ALLWIN_11_SERVER,    ALLWIN_11_CLIENT,   \
                              ALLWIN_12_CLIENT,    ALLHYPER_V

#define ALLWINNT32            BS(WINNT),           ALLWIN2000,          \
                              ALLWINXP32,          ALLWINNET32,         \
                              ALLWINVISTA32,       ALLWINLONGHORN32,    \
                              ALLWINSEVEN32,       ALLWINEIGHTCLIENT32, \
                              ALLWIN_10_CLIENT32

#define ALLWINNT64            ALLWINXP64,            ALLWINNET64,       \
                              ALLWINVISTA64,         ALLWINLONGHORN64,  \
                              ALLWINSEVEN64,         ALLWIN2008R2_64,   \
                              ALLWINEIGHTCLIENT64,   ALLWINEIGHTSERVER, \
                              ALLWIN_10_CLIENT64,    ALLWIN_10_SERVER,  \
                              ALLWIN_11_CLIENT64,    ALLWIN_11_SERVER,  \
                              ALLWIN_12_CLIENT64,    ALLHYPER_V

#define ALLWINNT              ALLWINNT32, ALLWINNT64

#define ALLWIN32              ALLWIN9X, ALLWINNT32
#define ALLWIN64              ALLWINNT64
#define ALLWIN                ALLWIN32, ALLWIN64

#define ALLOTHER              BS(OTHER), BS(OTHER_64)

#define ALLSOLARIS11_OR_HIGHER \
                              BS(SOLARIS11_64)

#define ALLSOLARIS10_OR_HIGHER \
                              BS(SOLARIS10), BS(SOLARIS10_64), \
                              ALLSOLARIS11_OR_HIGHER

#define ALLSOLARIS            BS(SOLARIS_6_AND_7),    \
                              BS(SOLARIS8),           \
                              BS(SOLARIS9),           \
                              ALLSOLARIS10_OR_HIGHER

#define ALLNETWARE            BS(NETWARE4), BS(NETWARE5), BS(NETWARE6)

#define ALLPHOTON             BS(PHOTON_64), BS(PHOTON_ARM_64)

#define ALL26XLINUX32         BS(DEBIAN),  BS(RHEL),   \
                              BS(UBUNTU),  BS(CENTOS), \
                              BS(ORACLE),  BS(OTHER26XLINUX)

#define ALL26XLINUX64         BS(DEBIAN_64),  BS(RHEL_64),   \
                              BS(UBUNTU_64),  BS(CENTOS_64), \
                              BS(ORACLE_64),  BS(OTHER26XLINUX_64)

#define ALL3XLINUX32          BS(OTHER3XLINUX), BS(CENTOS6), BS(ORACLE6)

#define ALL3XLINUX64          BS(OTHER3XLINUX_64),            \
                              BS(CENTOS6_64), BS(CENTOS7_64), \
                              BS(ORACLE6_64), BS(ORACLE7_64)

#define ALL4XLINUX32          BS(OTHER4XLINUX)

#define ALL4XLINUX64          BS(OTHER4XLINUX_64), BS(PHOTON_64),       \
                              BS(CENTOS8_64),      BS(CENTOS9_64),      \
                              BS(ORACLE8_64),      BS(ORACLE9_64),      \
                              BS(CRXSYS1_64),      BS(CRXPOD1_64),      \
                              BS(AMAZONLINUX2_64), BS(AMAZONLINUX3_64), \
                              BS(LINUX_MINT_64)

#define ALL5XLINUX32          BS(OTHER5XLINUX)

#define ALL5XLINUX64          BS(OTHER5XLINUX_64), BS(OTHER5XLINUX_ARM_64), \
                              BS(DEBIAN_ARM_64),   BS(UBUNTU_ARM_64),       \
                              BS(RHEL9_64),        BS(RHEL9_ARM_64),        \
                              BS(ROCKY_LINUX_64),  BS(ROCKY_LINUX_ARM_64),  \
                              BS(ALMA_LINUX_64),   BS(ALMA_LINUX_ARM_64)

#define ALL6XLINUX32          BS(OTHER6XLINUX)

#define ALL6XLINUX64          BS(OTHER6XLINUX_64), BS(OTHER6XLINUX_ARM_64)

#define ALLVMKERNEL           BS(VMKERNEL),   BS(VMKERNEL5),      \
                              BS(VMKERNEL6),  BS(VMKERNEL65),     \
                              BS(VMKERNEL7),  BS(VMKERNEL7_ARM),  \
                              BS(VMKERNEL8),  BS(VMKERNEL8_ARM)

#define ALLLINUX32            BS(VMKERNEL),      BS(OTHERLINUX), \
                              BS(OTHER24XLINUX), ALL26XLINUX32,  \
                              ALL3XLINUX32,      ALL4XLINUX32,   \
                              ALL5XLINUX32,      ALL6XLINUX32

#define ALLLINUX64            BS(OTHERLINUX_64), BS(OTHER24XLINUX_64), \
                              ALL26XLINUX64,     ALL3XLINUX64,         \
                              ALL4XLINUX64,      ALL5XLINUX64,         \
                              ALL6XLINUX64,      ALLPHOTON

#define ALLLINUX              ALLLINUX32, ALLLINUX64

#define ALLDARWIN32           BS(DARWIN9), BS(DARWIN10), BS(DARWIN11)

#define ALLDARWIN64           BS(DARWIN9_64),  BS(DARWIN10_64),  \
                              BS(DARWIN11_64), BS(DARWIN12_64),  \
                              BS(DARWIN13_64), BS(DARWIN14_64),  \
                              BS(DARWIN15_64), BS(DARWIN16_64),  \
                              BS(DARWIN17_64), BS(DARWIN18_64),  \
                              BS(DARWIN19_64), BS(DARWIN20_64),  \
                              BS(DARWIN21_64), BS(DARWIN22_64),  \
                              BS(DARWIN23_64)

#define ALLDARWIN             ALLDARWIN32, ALLDARWIN64

#define ALL64                 ALLLINUX64,        ALLWIN64,          \
                              ALLFREEBSD64,      ALLDARWIN64,       \
                              ALLPHOTON,         ALLVMKERNEL,       \
                              BS(SOLARIS10_64),  BS(SOLARIS11_64),  \
                              BS(OTHER_64),      BS(OTHER_ARM_64)

#define ALLECOMSTATION        BS(ECOMSTATION), BS(ECOMSTATION2)

#define ALLOS2                BS(OS2), ALLECOMSTATION

#define ALLCRX                BS(CRXSYS1_64), BS(CRXPOD1_64)

#define ALLARM                BS(WIN_10_ARM_64),        BS(WIN_11_ARM_64),       \
                              BS(WIN_12_ARM_64),        BS(RHEL9_ARM_64),        \
                              BS(UBUNTU_ARM_64),        BS(PHOTON_ARM_64),       \
                              BS(VMKERNEL7_ARM),        BS(VMKERNEL8_ARM),       \
                              BS(OTHER_ARM_64),         BS(OTHER5XLINUX_ARM_64), \
                              BS(OTHER6XLINUX_ARM_64),  BS(DEBIAN_ARM_64),       \
                              BS(FREEBSD13_ARM_64),     BS(FREEBSD14_ARM_64),    \
                              BS(ALMA_LINUX_ARM_64),    BS(ROCKY_LINUX_ARM_64)

/*
 * Architecture prefixes. No prefix implies the X86 architecture.
 */

#define STR_OS_ARM_PREFIX          "arm-"
#define STR_OS_RISCV_PREFIX        "riscv-"

/* vmkernel (ESX) */
#define STR_OS_VMKERNEL            "vmkernel"

/* Linux */
#define	STR_OS_ALMA_LINUX          "almaLinux"
#define STR_OS_AMAZON_LINUX        "amazonlinux"
#define STR_OS_ANNVIX              "Annvix"
#define STR_OS_ARCH                "Arch"
#define STR_OS_ARKLINUX            "Arklinux"
#define STR_OS_ASIANUX             "asianux"
#define STR_OS_AUROX               "Aurox"
#define STR_OS_BLACKCAT            "BlackCat"
#define STR_OS_CENTOS              "centos"
#define STR_OS_CRXPOD              "CRXPod"
#define STR_OS_CRXSYS              "CRXSys"
#define STR_OS_COBALT              "Cobalt"
#define STR_OS_CONECTIVA           "Conectiva"
#define STR_OS_DEBIAN              "debian"
#define STR_OS_FEDORA              "Fedora"
#define STR_OS_FLATCAR             "flatcar"
#define STR_OS_GENTOO              "Gentoo"
#define STR_OS_IMMUNIX             "Immunix"
#define STR_OS_LINUX               "linux"
#define STR_OS_LINUX_FROM_SCRATCH "Linux-From-Scratch"
#define STR_OS_LINUX_FULL         "Other Linux"
#define STR_OS_LINUX_MINT         "linuxMint"
#define STR_OS_LINUX_PPC          "Linux-PPC"
#define STR_OS_MANDRAKE           "mandrake"
#define STR_OS_MANDRAKE_FULL      "Mandrake Linux"
#define STR_OS_MANDRIVA           "mandriva"
#define STR_OS_MKLINUX            "MkLinux"
#define STR_OS_NOVELL             "nld"
#define STR_OS_NOVELL_FULL        "Novell Linux Desktop 9"
#define STR_OS_ORACLE             "oraclelinux"
#define STR_OS_OTHER_LINUX        "otherlinux"
#define STR_OS_OTHER_LINUX_FULL   "Other Linux"
#define STR_OS_OTHER              "other"
#define STR_OS_OTHER_FULL         "Other OSes"
#define STR_OS_OTHER_LINUX        "otherlinux"
#define STR_OS_OTHER_LINUXFULL    "Other Linux"
#define STR_OS_OTHER_24           "other24xlinux"
#define STR_OS_OTHER_24_FULL      "Other Linux 2.4.x kernel"
#define STR_OS_OTHER_26           "other26xlinux"
#define STR_OS_OTHER_26_FULL      "Other Linux 2.6.x kernel"
#define STR_OS_OTHER_3X           "other3xlinux"
#define STR_OS_OTHER_3X_FULL      "Other Linux 3.x kernel"
#define STR_OS_OTHER_4X           "other4xlinux"
#define STR_OS_OTHER_4X_FULL      "Other Linux 4.x kernel"
#define STR_OS_OTHER_5X           "other5xlinux"
#define STR_OS_OTHER_5X_FULL      "Other Linux 5.x kernel"
#define STR_OS_OTHER_6X           "other6xlinux"
#define STR_OS_OTHER_6X_FULL      "Other Linux 6.x and later kernel"
#define STR_OS_PHOTON             "vmware-photon"
#define STR_OS_PHOTON_FULL        "VMware Photon OS"
#define STR_OS_PLD                "PLD"
#define STR_OS_RED_HAT            "redhat"
#define STR_OS_RED_HAT_EN         "rhel"
#define STR_OS_RED_HAT_FULL       "Red Hat Linux"
#define	STR_OS_ROCKY_LINUX        "rockyLinux"
#define STR_OS_SLACKWARE          "Slackware"
#define STR_OS_SLES               "sles"
#define STR_OS_SUSE               "suse"
#define STR_OS_SUSE_FULL          "SUSE Linux"
#define STR_OS_OPENSUSE           "opensuse"
#define STR_OS_SMESERVER          "SMEServer"
#define STR_OS_SUN_DESK           "sjds"
#define STR_OS_SUN_DESK_FULL      "Sun Java Desktop System"
#define STR_OS_TINYSOFA           "Tiny Sofa"
#define STR_OS_TURBO              "turbolinux"
#define STR_OS_TURBO_FULL         "Turbolinux"
#define STR_OS_UBUNTU             "ubuntu"
#define STR_OS_ULTRAPENGUIN       "UltraPenguin"
#define STR_OS_UNITEDLINUX        "UnitedLinux"
#define STR_OS_VALINUX            "VALinux"
#define STR_OS_YELLOW_DOG         "Yellow Dog"
#define STR_OS_ECOMSTATION        "eComStation"

/* Windows */
#define STR_OS_WIN_31                   "win31"
#define STR_OS_WIN_31_FULL              "Windows 3.1"
#define STR_OS_WIN_95                   "win95"
#define STR_OS_WIN_95_FULL              "Windows 95"
#define STR_OS_WIN_98                   "win98"
#define STR_OS_WIN_98_FULL              "Windows 98"
#define STR_OS_WIN_ME                   "winMe"
#define STR_OS_WIN_ME_FULL              "Windows Me"
#define STR_OS_WIN_NT                   "winNT"
#define STR_OS_WIN_NT_FULL              "Windows NT"
#define STR_OS_WIN_2000_PRO             "win2000Pro"
#define STR_OS_WIN_2000_PRO_FULL        "Windows 2000 Professional"
#define STR_OS_WIN_2000_SERV            "win2000Serv"
#define STR_OS_WIN_2000_SERV_FULL       "Windows 2000 Server"
#define STR_OS_WIN_2000_ADV_SERV        "win2000AdvServ"
#define STR_OS_WIN_2000_ADV_SERV_FULL   "Windows 2000 Advanced Server"
#define STR_OS_WIN_2000_DATACENT_SERV   "win2000DataCentServ"
#define STR_OS_WIN_2000_DATACENT_SERV_FULL "Windows 2000 Data Center Server"
#define STR_OS_WIN_XP_HOME              "winXPHome"
#define STR_OS_WIN_XP_HOME_FULL         "Windows XP Home Edition"
#define STR_OS_WIN_XP_PRO               "winXPPro"
#define STR_OS_WIN_XP_PRO_FULL          "Windows XP Professional"
#define STR_OS_WIN_XP_PRO_X64           "winXPPro-64"
#define STR_OS_WIN_XP_PRO_X64_FULL      "Windows XP Professional x64 Edition"
#define STR_OS_WIN_NET_WEB              "winNetWeb"
#define STR_OS_WIN_NET_WEB_FULL         "Windows Server 2003 Web Edition"
#define STR_OS_WIN_NET_ST               "winNetStandard"
#define STR_OS_WIN_NET_ST_FULL          "Windows Server 2003 Standard Edition"
#define STR_OS_WIN_NET_EN               "winNetEnterprise"
#define STR_OS_WIN_NET_EN_FULL          "Windows Server 2003 Enterprise Edition"
#define STR_OS_WIN_NET_BUS              "winNetBusiness"
#define STR_OS_WIN_NET_BUS_FULL         "Windows Server 2003 Small Business"
#define STR_OS_WIN_NET_COMPCLUSTER      "winNetComputeCluster"
#define STR_OS_WIN_NET_COMPCLUSTER_FULL "Windows Server 2003 Compute Cluster Edition"
#define STR_OS_WIN_NET_STORAGESERVER    "winNetStorageSvr"
#define STR_OS_WIN_NET_STORAGESERVER_FULL "Windows Storage Server 2003"
#define STR_OS_WIN_NET_DC_FULL          "Windows Server 2003 Datacenter Edition"
#define STR_OS_WIN_NET_DC               "winNetDatacenter"
#define STR_OS_WIN_LONG                 "longhorn"
#define STR_OS_WIN_VISTA                "winVista"
#define STR_OS_WIN_VISTA_FULL           "Windows Vista"
#define STR_OS_WIN_VISTA_X64            "winVista-64"
#define STR_OS_WIN_VISTA_X64_FULL       "Windows Vista x64 Edition"
#define STR_OS_WIN_VISTA_ULTIMATE       "winVistaUltimate-32"
#define STR_OS_WIN_VISTA_ULTIMATE_FULL  "Windows Vista Ultimate Edition"
#define STR_OS_WIN_VISTA_HOME_PREMIUM   "winVistaHomePremium-32"
#define STR_OS_WIN_VISTA_HOME_PREMIUM_FULL "Windows Vista Home Premium Edition"
#define STR_OS_WIN_VISTA_HOME_BASIC     "winVistaHomeBasic-32"
#define STR_OS_WIN_VISTA_HOME_BASIC_FULL "Windows Vista Home Basic Edition"
#define STR_OS_WIN_VISTA_ENTERPRISE     "winVistaEnterprise-32"
#define STR_OS_WIN_VISTA_ENTERPRISE_FULL "Windows Vista Enterprise Edition"
#define STR_OS_WIN_VISTA_BUSINESS       "winVistaBusiness-32"
#define STR_OS_WIN_VISTA_BUSINESS_FULL  "Windows Vista Business Edition"
#define STR_OS_WIN_VISTA_STARTER        "winVistaStarter-32"
#define STR_OS_WIN_VISTA_STARTER_FULL   "Windows Vista Starter Edition"
#define STR_OS_WIN_2008_CLUSTER         "winServer2008Cluster-32"
#define STR_OS_WIN_2008_CLUSTER_FULL    "Windows Server 2008 Cluster Server Edition"
#define STR_OS_WIN_2008_DATACENTER      "winServer2008Datacenter-32"
#define STR_OS_WIN_2008_DATACENTER_FULL "Windows Server 2008 Datacenter Edition"
#define STR_OS_WIN_2008_DATACENTER_CORE "winServer2008DatacenterCore-32"
#define STR_OS_WIN_2008_DATACENTER_CORE_FULL "Windows Server 2008 Datacenter Edition (core installation)"
#define STR_OS_WIN_2008_ENTERPRISE      "winServer2008Enterprise-32"
#define STR_OS_WIN_2008_ENTERPRISE_FULL "Windows Server 2008 Enterprise Edition"
#define STR_OS_WIN_2008_ENTERPRISE_CORE "winServer2008EnterpriseCore-32"
#define STR_OS_WIN_2008_ENTERPRISE_CORE_FULL "Windows Server 2008 Enterprise Edition (core installation)"
#define STR_OS_WIN_2008_ENTERPRISE_ITANIUM "winServer2008EnterpriseItanium-32"
#define STR_OS_WIN_2008_ENTERPRISE_ITANIUM_FULL "Windows Server 2008 Enterprise Edition for Itanium-based Systems"
#define STR_OS_WIN_2008_MEDIUM_MANAGEMENT "winServer2008MediumManagement-32"
#define STR_OS_WIN_2008_MEDIUM_MANAGEMENT_FULL "Windows Essential Business Server Management Server"
#define STR_OS_WIN_2008_MEDIUM_MESSAGING "winServer2008MediumMessaging-32"
#define STR_OS_WIN_2008_MEDIUM_MESSAGING_FULL "Windows Essential Business Server Messaging Server"
#define STR_OS_WIN_2008_MEDIUM_SECURITY "winServer2008MediumSecurity-32"
#define STR_OS_WIN_2008_MEDIUM_SECURITY_FULL "Windows Essential Business Server Security Server"
#define STR_OS_WIN_2008_SERVER_FOR_SMALLBUSINESS "winServer2008ForSmallBusiness-32"
#define STR_OS_WIN_2008_SERVER_FOR_SMALLBUSINESS_FULL "Windows Server 2008 for Windows Essential Server Solutions"
#define STR_OS_WIN_2008_SMALL_BUSINESS  "winServer2008SmallBusiness-32"
#define STR_OS_WIN_2008_SMALL_BUSINESS_FULL "Windows Server 2008 Small Business Server"
#define STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM "winServer2008SmallBusinessPremium-32"
#define STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM_FULL "Windows Server 2008 Small Business Server Premium Edition"
#define STR_OS_WIN_2008_STANDARD        "winServer2008Standard-32"
#define STR_OS_WIN_2008_STANDARD_FULL   "Windows Server 2008 Standard Edition"
#define STR_OS_WIN_2008_STANDARD_CORE   "winServer2008StandardCore-32"
#define STR_OS_WIN_2008_STANDARD_CORE_FULL "Windows Server 2008 Standard Edition (core installation)"
#define STR_OS_WIN_2008_STORAGE_ENTERPRISE "winServer2008StorageEnterprise-32"
#define STR_OS_WIN_2008_STORAGE_ENTERPRISE_FULL "Windows Server 2008 Storage Server Enterprise"
#define STR_OS_WIN_2008_STORAGE_EXPRESS  "winServer2008StorageExpress-32"
#define STR_OS_WIN_2008_STORAGE_EXPRESS_FULL "Windows Server 2008 Storage Server Express"
#define STR_OS_WIN_2008_STORAGE_STANDARD "winServer2008StorageStandard-32"
#define STR_OS_WIN_2008_STORAGE_STANDARD_FULL "Windows Server 2008 Storage Server Standard"
#define STR_OS_WIN_2008_STORAGE_WORKGROUP "winServer2008StorageWorkgroup-32"
#define STR_OS_WIN_2008_STORAGE_WORKGROUP_FULL "Windows Server 2008 Storage Server Workgroup"
#define STR_OS_WIN_2008_WEB_SERVER       "winServer2008Web-32"
#define STR_OS_WIN_2008_WEB_SERVER_FULL  "Windows Server 2008 Web Server Edition"

/* Windows 64-bit */
#define STR_OS_WIN_VISTA_ULTIMATE_X64         "winVistaUltimate-64"
#define STR_OS_WIN_VISTA_HOME_PREMIUM_X64     "winVistaHomePremium-64"
#define STR_OS_WIN_VISTA_HOME_BASIC_X64       "winVistaHomeBasic-64"
#define STR_OS_WIN_VISTA_ENTERPRISE_X64       "winVistaEnterprise-64"
#define STR_OS_WIN_VISTA_BUSINESS_X64         "winVistaBusiness-64"
#define STR_OS_WIN_VISTA_STARTER_X64          "winVistaStarter-64"

#define STR_OS_WIN_2008_CLUSTER_X64           "winServer2008Cluster-64"
#define STR_OS_WIN_2008_DATACENTER_X64        "winServer2008Datacenter-64"
#define STR_OS_WIN_2008_DATACENTER_CORE_X64   "winServer2008DatacenterCore-64"
#define STR_OS_WIN_2008_ENTERPRISE_X64        "winServer2008Enterprise-64"
#define STR_OS_WIN_2008_ENTERPRISE_CORE_X64   "winServer2008EnterpriseCore-64"
#define STR_OS_WIN_2008_MEDIUM_MANAGEMENT_X64 "winServer2008MediumManagement-64"
#define STR_OS_WIN_2008_MEDIUM_MESSAGING_X64  "winServer2008MediumMessaging-64"
#define STR_OS_WIN_2008_MEDIUM_SECURITY_X64   "winServer2008MediumSecurity-64"
#define STR_OS_WIN_2008_SERVER_FOR_SMALLBUSINESS_X64 "winServer2008ForSmallBusiness-64"
#define STR_OS_WIN_2008_SMALL_BUSINESS_X64    "winServer2008SmallBusiness-64"
#define STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM_X64 "winServer2008SmallBusinessPremium-64"
#define STR_OS_WIN_2008_STANDARD_X64          "winServer2008Standard-64"
#define STR_OS_WIN_2008_STANDARD_CORE_X64     "winServer2008StandardCore-64"
#define STR_OS_WIN_2008_STORAGE_ENTERPRISE_X64 "winServer2008StorageEnterprise-64"
#define STR_OS_WIN_2008_STORAGE_EXPRESS_X64   "winServer2008StorageExpress-64"
#define STR_OS_WIN_2008_STORAGE_STANDARD_X64  "winServer2008StorageStandard-64"
#define STR_OS_WIN_2008_STORAGE_WORKGROUP_X64 "winServer2008StorageWorkgroup-64"
#define STR_OS_WIN_2008_WEB_SERVER_X64        "winServer2008Web-64"

/* All */
#define STR_OS_64BIT_SUFFIX "-64"
#define STR_OS_64BIT_SUFFIX_FULL " (64 bit)"
#define STR_OS_EMPTY ""

/* Windows 7 */

#define STR_OS_WINDOWS       "windows"

#define STR_OS_WIN_SEVEN     STR_OS_WINDOWS "7"
#define STR_OS_WIN_SEVEN_X64 STR_OS_WIN_SEVEN STR_OS_64BIT_SUFFIX

#define STR_OS_WIN_SEVEN_GENERIC           "Windows 7"
#define STR_OS_WIN_SEVEN_STARTER_FULL      "Windows 7 Starter"
#define STR_OS_WIN_SEVEN_HOME_BASIC_FULL   "Windows 7 Home Basic"
#define STR_OS_WIN_SEVEN_HOME_PREMIUM_FULL "Windows 7 Home Premium"
#define STR_OS_WIN_SEVEN_ULTIMATE_FULL     "Windows 7 Ultimate"
#define STR_OS_WIN_SEVEN_PROFESSIONAL_FULL "Windows 7 Professional"
#define STR_OS_WIN_SEVEN_ENTERPRISE_FULL   "Windows 7 Enterprise"

/* Windows Server 2008 R2 (based on Windows 7) */

#define STR_OS_WIN_2008R2_X64 STR_OS_WINDOWS "7srv" STR_OS_64BIT_SUFFIX

#define STR_OS_WIN_2008R2_FOUNDATION_FULL "Windows Server 2008 R2 Foundation Edition"
#define STR_OS_WIN_2008R2_STANDARD_FULL   "Windows Server 2008 R2 Standard Edition"
#define STR_OS_WIN_2008R2_ENTERPRISE_FULL "Windows Server 2008 R2 Enterprise Edition"
#define STR_OS_WIN_2008R2_DATACENTER_FULL "Windows Server 2008 R2 Datacenter Edition"
#define STR_OS_WIN_2008R2_WEB_SERVER_FULL "Windows Web Server 2008 R2 Edition"

/* Windows 8 */

#define STR_OS_WIN_EIGHT               STR_OS_WINDOWS "8"
#define STR_OS_WIN_EIGHT_X64           STR_OS_WIN_EIGHT STR_OS_64BIT_SUFFIX

#define STR_OS_WIN_EIGHT_GENERIC_FULL        "Windows 8%s"
#define STR_OS_WIN_EIGHTSERVER_GENERIC_FULL  "Windows Server%s 2012"
#define STR_OS_WIN_EIGHT_FULL                "Windows 8%s"
#define STR_OS_WIN_EIGHT_PRO_FULL            "Windows 8%s Pro"
#define STR_OS_WIN_EIGHT_ENTERPRISE_FULL     "Windows 8%s Enterprise"


/* Windows Server 2012 */

#define STR_OS_WIN_EIGHTSERVER_X64 STR_OS_WINDOWS "8srv" STR_OS_64BIT_SUFFIX 

#define STR_OS_WIN_2012_FOUNDATION_FULL      "Windows Server 2012%s Foundation Edition"
#define STR_OS_WIN_2012_ESSENTIALS_FULL      "Windows Server 2012%s Essentials Edition"
#define STR_OS_WIN_2012_STANDARD_FULL        "Windows Server 2012%s Standard Edition"
#define STR_OS_WIN_2012_ENTERPRISE_FULL      "Windows Server 2012%s Enterprise Edition"
#define STR_OS_WIN_2012_DATACENTER_FULL      "Windows Server 2012%s Datacenter Edition"
#define STR_OS_WIN_2012_STORAGESERVER_FULL   "Windows Server 2012%s Storage Server"
#define STR_OS_WIN_2012_WEB_SERVER_FULL      "Windows Web Server 2012%s Edition"
#define STR_OS_WIN_2012_MULTIPOINT_STANDARD_FULL  "Windows MultiPoint Server 2012%s Standard"
#define STR_OS_WIN_2012_MULTIPOINT_PREMIUM_FULL   "Windows MultiPoint Server 2012%s Premium"


/*
 * Windows on Arm
 *
 * Window on Arm support starts with Windows 10.
 */

/*
 * Windows 10
 *
 * Microsoft renamed Windows 9 to Windows 10 at the last minute; Windows 9 was
 * never officially released. We retain the Windows 9 identifier strings as
 * Windows 10 to ensure that things continue to work.
 */

#define STR_OS_WIN_10        STR_OS_WINDOWS "9"
#define STR_OS_WIN_10_X64    STR_OS_WIN_10 STR_OS_64BIT_SUFFIX

#define STR_OS_WIN_10_GENERIC_FULL         "Windows 10"
#define STR_OS_WIN_10_HOME_FULL            "Windows 10 Home"
#define STR_OS_WIN_10_EDUCATION_FULL       "Windows 10 Education"
#define STR_OS_WIN_10_ENTERPRISE_FULL      "Windows 10 Enterprise"
#define STR_OS_WIN_10_PRO_WORKSTATION_FULL "Windows 10 Pro for Workstations"
#define STR_OS_WIN_10_PRO_FULL             "Windows 10 Pro"
#define STR_OS_WIN_10_IOTCORE_FULL         "Windows 10 IoT Core"

/*
 * Windows 11
 */

#define STR_OS_WIN_11        STR_OS_WINDOWS "11"
#define STR_OS_WIN_11_X64    STR_OS_WIN_11 STR_OS_64BIT_SUFFIX

#define STR_OS_WIN_11_GENERIC_FULL         "Windows 11"
#define STR_OS_WIN_11_HOME_FULL            "Windows 11 Home"
#define STR_OS_WIN_11_EDUCATION_FULL       "Windows 11 Education"
#define STR_OS_WIN_11_ENTERPRISE_FULL      "Windows 11 Enterprise"
#define STR_OS_WIN_11_PRO_WORKSTATION_FULL "Windows 11 Pro for Workstations"
#define STR_OS_WIN_11_PRO_FULL             "Windows 11 Pro"
#define STR_OS_WIN_11_IOTCORE_FULL         "Windows 11 IoT Core"

/*
 * Windows 12
 */

#define STR_OS_WIN_12        STR_OS_WINDOWS "12"
#define STR_OS_WIN_12_X64    STR_OS_WIN_12 STR_OS_64BIT_SUFFIX

/* No full names known yet */

/* Windows Server 2016 */

#define STR_OS_WIN_2016SRV_X64 STR_OS_WINDOWS "9srv" STR_OS_64BIT_SUFFIX

/* Windows Server 2019 */

#define STR_OS_WIN_2019SRV_X64 STR_OS_WINDOWS "2019srv" STR_OS_64BIT_SUFFIX

/* Windows Server 2022 */

#define STR_OS_WIN_2022SRV_X64 STR_OS_WINDOWS "2019srvNext" STR_OS_64BIT_SUFFIX

/* Windows Server 2025 */

#define STR_OS_WIN_2025SRV_X64 STR_OS_WINDOWS "2022srvNext" STR_OS_64BIT_SUFFIX

/* THIS SPACE FOR RENT (Windows 10 and later official server variant names) */

#define STR_OS_WIN_10_SERVER_2016_GENERIC_FULL "Windows Server 2016"
#define STR_OS_WIN_10_SERVER_2019_GENERIC_FULL "Windows Server 2019"
#define STR_OS_WIN_11_SERVER_2022_GENERIC_FULL "Windows Server 2022"
#define STR_OS_WIN_11_SERVER_2025_GENERIC_FULL "Windows Server 2025"

/* Microsoft Hyper-V */
#define STR_OS_HYPER_V      "winHyperV"
#define STR_OS_HYPER_V_FULL "Hyper-V Server"

/* Windows Future/Unknown */

#define STR_OS_WIN_UNKNOWN                   "windowsUnknown"
#define STR_OS_WIN_UNKNOWN_X64               STR_OS_WIN_UNKNOWN STR_OS_64BIT_SUFFIX
#define STR_OS_WIN_UNKNOWN_GENERIC           "Windows Unknown"

/* Modifiers for Windows Vista, Windows Server 2008, and later. */
#define STR_OS_WIN_32_BIT_EXTENSION ", 32-bit"
#define STR_OS_WIN_64_BIT_EXTENSION ", 64-bit"

/* FreeBSD */
#define STR_OS_FREEBSD "freeBSD"

/* Solaris */
#define STR_OS_SOLARIS "solaris"

/* Netware */
#define STR_OS_NETWARE "netware"

/* Mac OS */
#define STR_OS_MACOS "darwin"

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
