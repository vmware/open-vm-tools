/*********************************************************
 * Copyright (C) 1998-2015 VMware, Inc. All rights reserved.
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
   GOT(GUEST_OS_WINTENSERVER_64)   /* Windows 10 Server X64 */             \
   GOT(GUEST_OS_HYPER_V)           /* Microsoft Hyper-V */                 \
   GOT(GUEST_OS_OS2)                                                       \
   GOT(GUEST_OS_ECOMSTATION)       /* OS/2 variant; 1.x */                 \
   GOT(GUEST_OS_ECOMSTATION2)      /* OS/2 variant; 2.x */                 \
   GOT(GUEST_OS_OTHER24XLINUX)                                             \
   GOT(GUEST_OS_OTHER24XLINUX_64)                                          \
   GOT(GUEST_OS_OTHER26XLINUX)                                             \
   GOT(GUEST_OS_OTHER26XLINUX_64)                                          \
   GOT(GUEST_OS_OTHER3XLINUX)                                              \
   GOT(GUEST_OS_OTHER3XLINUX_64)                                           \
   GOT(GUEST_OS_OTHERLINUX)                                                \
   GOT(GUEST_OS_OTHERLINUX_64)                                             \
   GOT(GUEST_OS_OTHER)                                                     \
   GOT(GUEST_OS_OTHER_64)                                                  \
   GOT(GUEST_OS_UBUNTU)                                                    \
   GOT(GUEST_OS_DEBIAN45)                                                  \
   GOT(GUEST_OS_DEBIAN45_64)                                               \
   GOT(GUEST_OS_RHEL)                                                      \
   GOT(GUEST_OS_RHEL_64)                                                   \
   GOT(GUEST_OS_FREEBSD)                                                   \
   GOT(GUEST_OS_FREEBSD_64)                                                \
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
   GOT(GUEST_OS_OPENSERVER_5_AND_6)                                        \
   GOT(GUEST_OS_UNIXWARE7)                                                 \
   GOT(GUEST_OS_NETWARE4)                                                  \
   GOT(GUEST_OS_NETWARE5)                                                  \
   GOT(GUEST_OS_NETWARE6)                                                  \
   GOT(GUEST_OS_VMKERNEL)          /* ESX 4.x */                           \
   GOT(GUEST_OS_VMKERNEL5)         /* ESX 5.x */                           \
   GOT(GUEST_OS_VMKERNEL6)         /* ESX 6.x and later */                 \
   GOT(GUEST_OS_PHOTON_64)         /* VMware Photon IA 64-bit */           \


/* This list must be sorted alphabetically (non-case-sensitive) by gos name. */
#define GUEST_OS_LIST_GEN                                                   \
   GOSL(STR_OS_ASIANUX_3,                    GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_ASIANUX_3 "-64",              GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_ASIANUX_4,                    GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_ASIANUX_4 "-64",              GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_ASIANUX_5 "-64",              GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_CENTOS,                       GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_CENTOS "-64",                 GUEST_OS_OTHER26XLINUX_64)     \
   GOSL("coreos-64",                         GUEST_OS_OTHER3XLINUX_64)      \
   GOSL(STR_OS_MACOS,                        GUEST_OS_DARWIN9)              \
   GOSL(STR_OS_MACOS "-64",                  GUEST_OS_DARWIN9_64)           \
   GOSL(STR_OS_MACOS "10",                   GUEST_OS_DARWIN10)             \
   GOSL(STR_OS_MACOS "10-64",                GUEST_OS_DARWIN10_64)          \
   GOSL(STR_OS_MACOS "11",                   GUEST_OS_DARWIN11)             \
   GOSL(STR_OS_MACOS "11-64",                GUEST_OS_DARWIN11_64)          \
   GOSL(STR_OS_MACOS "12-64",                GUEST_OS_DARWIN12_64)          \
   GOSL(STR_OS_MACOS "13-64",                GUEST_OS_DARWIN13_64)          \
   GOSL(STR_OS_MACOS "14-64",                GUEST_OS_DARWIN14_64)          \
   GOSL(STR_OS_DEBIAN_4,                     GUEST_OS_DEBIAN45)             \
   GOSL(STR_OS_DEBIAN_4 "-64",               GUEST_OS_DEBIAN45_64)          \
   GOSL(STR_OS_DEBIAN_5,                     GUEST_OS_DEBIAN45)             \
   GOSL(STR_OS_DEBIAN_5 "-64",               GUEST_OS_DEBIAN45_64)          \
   GOSL(STR_OS_DEBIAN_6,                     GUEST_OS_DEBIAN45)             \
   GOSL(STR_OS_DEBIAN_6 "-64",               GUEST_OS_DEBIAN45_64)          \
   GOSL(STR_OS_DEBIAN_7,                     GUEST_OS_DEBIAN45)             \
   GOSL(STR_OS_DEBIAN_7 "-64",               GUEST_OS_DEBIAN45_64)          \
   GOSL(STR_OS_DEBIAN_8,                     GUEST_OS_DEBIAN45)             \
   GOSL(STR_OS_DEBIAN_8 "-64",               GUEST_OS_DEBIAN45_64)          \
   GOSL("dos",                               GUEST_OS_DOS)                  \
   GOSL(STR_OS_ECOMSTATION,                  GUEST_OS_ECOMSTATION)          \
   GOSL(STR_OS_ECOMSTATION "2",              GUEST_OS_ECOMSTATION2)         \
   GOSL("fedora",                            GUEST_OS_OTHER26XLINUX)        \
   GOSL("fedora-64",                         GUEST_OS_OTHER26XLINUX_64)     \
   GOSL("freeBSD",                           GUEST_OS_FREEBSD)              \
   GOSL("freeBSD-64",                        GUEST_OS_FREEBSD_64)           \
   GOSL("linux",                             GUEST_OS_OTHERLINUX) /* old */ \
   GOSL(STR_OS_WIN_LONG,                     GUEST_OS_LONGHORN)             \
   GOSL("longhorn-64",                       GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_MANDRAKE,                     GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_MANDRAKE "-64",               GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_MANDRIVA,                     GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_MANDRIVA "-64",               GUEST_OS_OTHER26XLINUX_64)     \
   GOSL("netware4",                          GUEST_OS_NETWARE4)             \
   GOSL("netware5",                          GUEST_OS_NETWARE5)             \
   GOSL("netware6",                          GUEST_OS_NETWARE6)             \
   GOSL(STR_OS_NOVELL,                       GUEST_OS_OTHER26XLINUX)        \
   GOSL("nt4",                               GUEST_OS_WINNT)   /* old */    \
   GOSL("oes",                               GUEST_OS_OTHER26XLINUX)        \
   GOSL("openserver5",                       GUEST_OS_OPENSERVER_5_AND_6)   \
   GOSL("openserver6",                       GUEST_OS_OPENSERVER_5_AND_6)   \
   GOSL(STR_OS_OPENSUSE,                     GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_OPENSUSE "-64",               GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_ORACLE,                       GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_ORACLE "-64",                 GUEST_OS_OTHER26XLINUX_64)     \
   GOSL("os2",                               GUEST_OS_OS2)                  \
   GOSL("os2experimental",                   GUEST_OS_OS2)                  \
   GOSL("other",                             GUEST_OS_OTHER)                \
   GOSL("other-64",                          GUEST_OS_OTHER_64)             \
   GOSL(STR_OS_OTHER_24,                     GUEST_OS_OTHER24XLINUX)        \
   GOSL(STR_OS_OTHER_24 "-64",               GUEST_OS_OTHER24XLINUX_64)     \
   GOSL(STR_OS_OTHER_26,                     GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_OTHER_26 "-64",               GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_OTHER_3X,                     GUEST_OS_OTHER3XLINUX)         \
   GOSL(STR_OS_OTHER_3X "-64",               GUEST_OS_OTHER3XLINUX_64)      \
   GOSL(STR_OS_OTHER,                        GUEST_OS_OTHERLINUX)           \
   GOSL(STR_OS_OTHER "-64",                  GUEST_OS_OTHERLINUX_64)        \
   GOSL(STR_OS_RED_HAT,                      GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_RED_HAT_EN "2",               GUEST_OS_OTHER24XLINUX)        \
   GOSL(STR_OS_RED_HAT_EN "3",               GUEST_OS_OTHER24XLINUX)        \
   GOSL(STR_OS_RED_HAT_EN "3-64",            GUEST_OS_OTHER24XLINUX_64)     \
   GOSL(STR_OS_RED_HAT_EN "4",               GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_RED_HAT_EN "4-64",            GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_RED_HAT_EN "5",               GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_RED_HAT_EN "5-64",            GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_RED_HAT_EN "6",               GUEST_OS_RHEL)                 \
   GOSL(STR_OS_RED_HAT_EN "6-64",            GUEST_OS_RHEL_64)              \
   GOSL(STR_OS_RED_HAT_EN "7",               GUEST_OS_RHEL)                 \
   GOSL(STR_OS_RED_HAT_EN "7-64",            GUEST_OS_RHEL_64)              \
   GOSL(STR_OS_SUN_DESK,                     GUEST_OS_OTHER24XLINUX)        \
   GOSL(STR_OS_SLES,                         GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_SLES "-64",                   GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_SLES "10",                    GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_SLES "10-64",                 GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_SLES "11",                    GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_SLES "11-64",                 GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_SLES "12",                    GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_SLES "12-64",                 GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_SOLARIS "10",                 GUEST_OS_SOLARIS10)            \
   GOSL(STR_OS_SOLARIS "10-64",              GUEST_OS_SOLARIS10_64)         \
   GOSL(STR_OS_SOLARIS "11",                 GUEST_OS_SOLARIS10)            \
   GOSL(STR_OS_SOLARIS "11-64",              GUEST_OS_SOLARIS10_64)         \
   GOSL(STR_OS_SOLARIS "6",                  GUEST_OS_SOLARIS_6_AND_7)      \
   GOSL(STR_OS_SOLARIS "7",                  GUEST_OS_SOLARIS_6_AND_7)      \
   GOSL(STR_OS_SOLARIS "8",                  GUEST_OS_SOLARIS8)             \
   GOSL(STR_OS_SOLARIS "9",                  GUEST_OS_SOLARIS9)             \
   GOSL(STR_OS_SUSE,                         GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_SUSE "-64",                   GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_TURBO,                        GUEST_OS_OTHER26XLINUX)        \
   GOSL(STR_OS_TURBO "-64",                  GUEST_OS_OTHER26XLINUX_64)     \
   GOSL(STR_OS_UBUNTU,                       GUEST_OS_UBUNTU)               \
   GOSL(STR_OS_UBUNTU "-64",                 GUEST_OS_OTHER26XLINUX_64)     \
   GOSL("unixware7",                         GUEST_OS_UNIXWARE7)            \
   GOSL("vmkernel",                          GUEST_OS_VMKERNEL)             \
   GOSL("vmkernel5",                         GUEST_OS_VMKERNEL5)            \
   GOSL("vmkernel6",                         GUEST_OS_VMKERNEL6)            \
   GOSL(STR_OS_PHOTON "-64",                 GUEST_OS_PHOTON_64)            \
   GOSL("whistler",                          GUEST_OS_WINXP)   /* old */    \
   GOSL("win2000",                           GUEST_OS_WIN2000) /* old */    \
   GOSL(STR_OS_WIN_2000_ADV_SERV,            GUEST_OS_WIN2000)              \
   GOSL(STR_OS_WIN_2000_PRO,                 GUEST_OS_WIN2000)              \
   GOSL(STR_OS_WIN_2000_SERV,                GUEST_OS_WIN2000)              \
   GOSL(STR_OS_WIN_31,                       GUEST_OS_WIN31)                \
   GOSL(STR_OS_WIN_95,                       GUEST_OS_WIN95)                \
   GOSL(STR_OS_WIN_98,                       GUEST_OS_WIN98)                \
   GOSL(STR_OS_WIN_SEVEN,                    GUEST_OS_WINSEVEN)             \
   GOSL(STR_OS_WIN_SEVEN_X64,                GUEST_OS_WINSEVEN_64)          \
   GOSL("windows7Server64Guest",             GUEST_OS_WIN2008R2_64)         \
   GOSL(STR_OS_WIN_2008R2_X64,               GUEST_OS_WIN2008R2_64)         \
   GOSL(STR_OS_WIN_EIGHT,                    GUEST_OS_WINEIGHT)             \
   GOSL(STR_OS_WIN_EIGHT_X64,                GUEST_OS_WINEIGHT_64)          \
   GOSL(STR_OS_WIN_EIGHTSERVER_X64,          GUEST_OS_WINEIGHTSERVER_64)    \
   GOSL(STR_OS_WIN_TEN,                      GUEST_OS_WINTEN)               \
   GOSL(STR_OS_WIN_TEN_X64,                  GUEST_OS_WINTEN_64)            \
   GOSL(STR_OS_WIN_TENSERVER_X64,            GUEST_OS_WINTENSERVER_64)      \
   GOSL(STR_OS_HYPER_V,                      GUEST_OS_HYPER_V)              \
   GOSL("winLonghorn64Guest",                GUEST_OS_LONGHORN_64)          \
   GOSL("winLonghornGuest",                  GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_ME,                       GUEST_OS_WINME )               \
   GOSL(STR_OS_WIN_NET_BUS,                  GUEST_OS_WINNET)               \
   GOSL("winNetDatacenter",                  GUEST_OS_WINNET)               \
   GOSL("winNetDatacenter-64",               GUEST_OS_WINNET_64)            \
   GOSL(STR_OS_WIN_NET_EN,                   GUEST_OS_WINNET)               \
   GOSL("winNetEnterprise-64",               GUEST_OS_WINNET_64)            \
   GOSL(STR_OS_WIN_NET_ST,                   GUEST_OS_WINNET)               \
   GOSL("winNetStandard-64",                 GUEST_OS_WINNET_64)            \
   GOSL(STR_OS_WIN_NET_WEB,                  GUEST_OS_WINNET)               \
   GOSL(STR_OS_WIN_NT,                       GUEST_OS_WINNT)                \
   GOSL(STR_OS_WIN_2008_CLUSTER,             GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_CLUSTER_X64,         GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_DATACENTER,          GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_DATACENTER_X64,      GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_DATACENTER_CORE,     GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_DATACENTER_CORE_X64, GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_ENTERPRISE,          GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_ENTERPRISE_X64,      GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_ENTERPRISE_CORE,     GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_ENTERPRISE_CORE_X64, GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS,      GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS_X64,  GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM, GUEST_OS_LONGHORN)          \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM_X64, GUEST_OS_LONGHORN_64)   \
   GOSL(STR_OS_WIN_2008_STANDARD,            GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_STANDARD_X64,        GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_STANDARD_CORE,       GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_STANDARD_CORE_X64,   GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_2008_WEB_SERVER,          GUEST_OS_LONGHORN)             \
   GOSL(STR_OS_WIN_2008_WEB_SERVER_X64,      GUEST_OS_LONGHORN_64)          \
   GOSL(STR_OS_WIN_VISTA,                    GUEST_OS_WINVISTA)             \
   GOSL(STR_OS_WIN_VISTA_X64,                GUEST_OS_WINVISTA_64)          \
   GOSL(STR_OS_WIN_XP_HOME,                  GUEST_OS_WINXP)                \
   GOSL(STR_OS_WIN_XP_PRO,                   GUEST_OS_WINXP)                \
   GOSL(STR_OS_WIN_XP_PRO_X64,               GUEST_OS_WINXPPRO_64)          \

#endif
