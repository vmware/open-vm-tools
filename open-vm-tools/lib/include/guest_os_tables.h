/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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
   GOT(GUEST_OS_OTHER3XLINUX)      /* Linux 3.x and later */               \
   GOT(GUEST_OS_OTHER3XLINUX_64)   /* Linux 3.x and later X64 */           \
   GOT(GUEST_OS_OTHERLINUX)                                                \
   GOT(GUEST_OS_OTHERLINUX_64)                                             \
   GOT(GUEST_OS_OTHER)                                                     \
   GOT(GUEST_OS_OTHER_64)                                                  \
   GOT(GUEST_OS_UBUNTU)                                                    \
   GOT(GUEST_OS_DEBIAN)                                                    \
   GOT(GUEST_OS_DEBIAN_64)                                                 \
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
   GOT(GUEST_OS_DARWIN15_64)       /* Mac OS 10.11 */                      \
   GOT(GUEST_OS_DARWIN16_64)       /* Mac OS 10.12 */                      \
   GOT(GUEST_OS_OPENSERVER_5_AND_6)                                        \
   GOT(GUEST_OS_UNIXWARE7)                                                 \
   GOT(GUEST_OS_NETWARE4)                                                  \
   GOT(GUEST_OS_NETWARE5)                                                  \
   GOT(GUEST_OS_NETWARE6)                                                  \
   GOT(GUEST_OS_VMKERNEL)          /* ESX 4.x */                           \
   GOT(GUEST_OS_VMKERNEL5)         /* ESX 5.x */                           \
   GOT(GUEST_OS_VMKERNEL6)         /* ESX 6 */                             \
   GOT(GUEST_OS_VMKERNEL65)        /* ESX 6.5 and later */                 \
   GOT(GUEST_OS_PHOTON_64)         /* VMware Photon IA 64-bit */           \
   GOT(GUEST_OS_ORACLE)                                                    \
   GOT(GUEST_OS_ORACLE_64)                                                 \
   GOT(GUEST_OS_ORACLE6)                                                   \
   GOT(GUEST_OS_ORACLE6_64)                                                \
   GOT(GUEST_OS_ORACLE7_64)                                                \
   GOT(GUEST_OS_CENTOS)                                                    \
   GOT(GUEST_OS_CENTOS_64)                                                 \
   GOT(GUEST_OS_CENTOS6)                                                   \
   GOT(GUEST_OS_CENTOS6_64)                                                \
   GOT(GUEST_OS_CENTOS7_64)                                                \


/* This list must be sorted alphabetically (non-case-sensitive) by gos name. */
#define GUEST_OS_LIST_GEN                                                                             \
   GOSL(STR_OS_ASIANUX_3,                    GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_ASIANUX_3 "-64",              GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_ASIANUX_4,                    GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_ASIANUX_4 "-64",              GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_ASIANUX_5 "-64",              GUEST_OS_OTHER3XLINUX_64,        "linux.iso")            \
   GOSL(STR_OS_ASIANUX_7 "-64",              GUEST_OS_OTHER3XLINUX_64,        "linux.iso")            \
   GOSL(STR_OS_CENTOS,                       GUEST_OS_CENTOS,                 "linux.iso")            \
   GOSL(STR_OS_CENTOS "-64",                 GUEST_OS_CENTOS_64,              "linux.iso")            \
   GOSL(STR_OS_CENTOS "6",                   GUEST_OS_CENTOS6,                "linux.iso")            \
   GOSL(STR_OS_CENTOS "6-64",                GUEST_OS_CENTOS6_64,             "linux.iso")            \
   GOSL(STR_OS_CENTOS "7-64",                GUEST_OS_CENTOS7_64,             "linux.iso")            \
   GOSL("coreos-64",                         GUEST_OS_OTHER3XLINUX_64,        NULL)                   \
   GOSL(STR_OS_MACOS,                        GUEST_OS_DARWIN9,                "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "-64",                  GUEST_OS_DARWIN9_64,             "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "10",                   GUEST_OS_DARWIN10,               "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "10-64",                GUEST_OS_DARWIN10_64,            "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "11",                   GUEST_OS_DARWIN11,               "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "11-64",                GUEST_OS_DARWIN11_64,            "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "12-64",                GUEST_OS_DARWIN12_64,            "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "13-64",                GUEST_OS_DARWIN13_64,            "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "14-64",                GUEST_OS_DARWIN14_64,            "darwinPre15.iso")      \
   GOSL(STR_OS_MACOS "15-64",                GUEST_OS_DARWIN15_64,            "darwin.iso")           \
   GOSL(STR_OS_MACOS "16-64",                GUEST_OS_DARWIN16_64,            "darwin.iso")           \
   GOSL(STR_OS_DEBIAN_10,                    GUEST_OS_DEBIAN,                 "linux.iso")            \
   GOSL(STR_OS_DEBIAN_10 "-64",              GUEST_OS_DEBIAN_64,              "linux.iso")            \
   GOSL(STR_OS_DEBIAN_4,                     GUEST_OS_DEBIAN,                 "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_DEBIAN_4 "-64",               GUEST_OS_DEBIAN_64,              "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_DEBIAN_5,                     GUEST_OS_DEBIAN,                 "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_DEBIAN_5 "-64",               GUEST_OS_DEBIAN_64,              "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_DEBIAN_6,                     GUEST_OS_DEBIAN,                 "linux.iso")            \
   GOSL(STR_OS_DEBIAN_6 "-64",               GUEST_OS_DEBIAN_64,              "linux.iso")            \
   GOSL(STR_OS_DEBIAN_7,                     GUEST_OS_DEBIAN,                 "linux.iso")            \
   GOSL(STR_OS_DEBIAN_7 "-64",               GUEST_OS_DEBIAN_64,              "linux.iso")            \
   GOSL(STR_OS_DEBIAN_8,                     GUEST_OS_DEBIAN,                 "linux.iso")            \
   GOSL(STR_OS_DEBIAN_8 "-64",               GUEST_OS_DEBIAN_64,              "linux.iso")            \
   GOSL(STR_OS_DEBIAN_9,                     GUEST_OS_DEBIAN,                 "linux.iso")            \
   GOSL(STR_OS_DEBIAN_9 "-64",               GUEST_OS_DEBIAN_64,              "linux.iso")            \
   GOSL("dos",                               GUEST_OS_DOS,                    NULL)                   \
   GOSL(STR_OS_ECOMSTATION,                  GUEST_OS_ECOMSTATION,            NULL)                   \
   GOSL(STR_OS_ECOMSTATION "2",              GUEST_OS_ECOMSTATION2,           NULL)                   \
   GOSL("fedora",                            GUEST_OS_OTHER26XLINUX,          "linux.iso")            \
   GOSL("fedora-64",                         GUEST_OS_OTHER26XLINUX_64,       "linux.iso")            \
   GOSL("freeBSD",                           GUEST_OS_FREEBSD,                "freebsd.iso")          \
   GOSL("freeBSD-64",                        GUEST_OS_FREEBSD_64,             "freebsd.iso")          \
   GOSL("linux",                             GUEST_OS_OTHERLINUX,             "linuxPreGlibc25.iso") /* old */ \
   GOSL(STR_OS_WIN_LONG,                     GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL("longhorn-64",                       GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_MANDRAKE,                     GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_MANDRAKE "-64",               GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_MANDRIVA,                     GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_MANDRIVA "-64",               GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL("netware4",                          GUEST_OS_NETWARE4,               "netware.iso")          \
   GOSL("netware5",                          GUEST_OS_NETWARE5,               "netware.iso")          \
   GOSL("netware6",                          GUEST_OS_NETWARE6,               "netware.iso")          \
   GOSL(STR_OS_NOVELL,                       GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL("nt4",                               GUEST_OS_WINNT,                  "winPre2k.iso") /* old */ \
   GOSL("oes",                               GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL("openserver5",                       GUEST_OS_OPENSERVER_5_AND_6,     NULL)                   \
   GOSL("openserver6",                       GUEST_OS_OPENSERVER_5_AND_6,     NULL)                   \
   GOSL(STR_OS_OPENSUSE,                     GUEST_OS_OTHER26XLINUX,          "linux.iso")            \
   GOSL(STR_OS_OPENSUSE "-64",               GUEST_OS_OTHER26XLINUX_64,       "linux.iso")            \
   GOSL(STR_OS_ORACLE,                       GUEST_OS_ORACLE,                 "linux.iso")            \
   GOSL(STR_OS_ORACLE "-64",                 GUEST_OS_ORACLE_64,              "linux.iso")            \
   GOSL(STR_OS_ORACLE "6",                   GUEST_OS_ORACLE6,                "linux.iso")            \
   GOSL(STR_OS_ORACLE "6-64",                GUEST_OS_ORACLE6_64,             "linux.iso")            \
   GOSL(STR_OS_ORACLE "7-64",                GUEST_OS_ORACLE7_64,             "linux.iso")            \
   GOSL("os2",                               GUEST_OS_OS2,                    NULL)                   \
   GOSL("os2experimental",                   GUEST_OS_OS2,                    NULL)                   \
   GOSL("other",                             GUEST_OS_OTHER,                  NULL)                   \
   GOSL("other-64",                          GUEST_OS_OTHER_64,               NULL)                   \
   GOSL(STR_OS_OTHER_24,                     GUEST_OS_OTHER24XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_OTHER_24 "-64",               GUEST_OS_OTHER24XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_OTHER_26,                     GUEST_OS_OTHER26XLINUX,          "linux.iso")            \
   GOSL(STR_OS_OTHER_26 "-64",               GUEST_OS_OTHER26XLINUX_64,       "linux.iso")            \
   GOSL(STR_OS_OTHER_3X,                     GUEST_OS_OTHER3XLINUX,           "linux.iso")            \
   GOSL(STR_OS_OTHER_3X "-64",               GUEST_OS_OTHER3XLINUX_64,        "linux.iso")            \
   GOSL(STR_OS_OTHER,                        GUEST_OS_OTHERLINUX,             "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_OTHER "-64",                  GUEST_OS_OTHERLINUX_64,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_RED_HAT,                      GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_RED_HAT_EN "2",               GUEST_OS_OTHER24XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_RED_HAT_EN "3",               GUEST_OS_OTHER24XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_RED_HAT_EN "3-64",            GUEST_OS_OTHER24XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_RED_HAT_EN "4",               GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_RED_HAT_EN "4-64",            GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_RED_HAT_EN "5",               GUEST_OS_OTHER26XLINUX,          "linux.iso")            \
   GOSL(STR_OS_RED_HAT_EN "5-64",            GUEST_OS_OTHER26XLINUX_64,       "linux.iso")            \
   GOSL(STR_OS_RED_HAT_EN "6",               GUEST_OS_RHEL,                   "linux.iso")            \
   GOSL(STR_OS_RED_HAT_EN "6-64",            GUEST_OS_RHEL_64,                "linux.iso")            \
   GOSL(STR_OS_RED_HAT_EN "7",               GUEST_OS_RHEL,                   "linux.iso")            \
   GOSL(STR_OS_RED_HAT_EN "7-64",            GUEST_OS_RHEL_64,                "linux.iso")            \
   GOSL(STR_OS_SUN_DESK,                     GUEST_OS_OTHER24XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_SLES,                         GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_SLES "-64",                   GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_SLES "10",                    GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_SLES "10-64",                 GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_SLES "11",                    GUEST_OS_OTHER26XLINUX,          "linux.iso")            \
   GOSL(STR_OS_SLES "11-64",                 GUEST_OS_OTHER26XLINUX_64,       "linux.iso")            \
   GOSL(STR_OS_SLES "12",                    GUEST_OS_OTHER26XLINUX,          "linux.iso")            \
   GOSL(STR_OS_SLES "12-64",                 GUEST_OS_OTHER26XLINUX_64,       "linux.iso")            \
   GOSL(STR_OS_SOLARIS "10",                 GUEST_OS_SOLARIS10,              "solaris.iso")          \
   GOSL(STR_OS_SOLARIS "10-64",              GUEST_OS_SOLARIS10_64,           "solaris.iso")          \
   GOSL(STR_OS_SOLARIS "11",                 GUEST_OS_SOLARIS10,              "solaris.iso")          \
   GOSL(STR_OS_SOLARIS "11-64",              GUEST_OS_SOLARIS10_64,           "solaris.iso")          \
   GOSL(STR_OS_SOLARIS "6",                  GUEST_OS_SOLARIS_6_AND_7,        "solaris.iso")          \
   GOSL(STR_OS_SOLARIS "7",                  GUEST_OS_SOLARIS_6_AND_7,        "solaris.iso")          \
   GOSL(STR_OS_SOLARIS "8",                  GUEST_OS_SOLARIS8,               "solaris.iso")          \
   GOSL(STR_OS_SOLARIS "9",                  GUEST_OS_SOLARIS9,               "solaris.iso")          \
   GOSL(STR_OS_SUSE,                         GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_SUSE "-64",                   GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_TURBO,                        GUEST_OS_OTHER26XLINUX,          "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_TURBO "-64",                  GUEST_OS_OTHER26XLINUX_64,       "linuxPreGlibc25.iso")  \
   GOSL(STR_OS_UBUNTU,                       GUEST_OS_UBUNTU,                 "linux.iso")            \
   GOSL(STR_OS_UBUNTU "-64",                 GUEST_OS_OTHER26XLINUX_64,       "linux.iso")            \
   GOSL("unixware7",                         GUEST_OS_UNIXWARE7,              NULL)                   \
   GOSL("vmkernel",                          GUEST_OS_VMKERNEL,               NULL)                   \
   GOSL("vmkernel5",                         GUEST_OS_VMKERNEL5,              NULL)                   \
   GOSL("vmkernel6",                         GUEST_OS_VMKERNEL6,              NULL)                   \
   GOSL("vmkernel65",                        GUEST_OS_VMKERNEL65,             NULL)                   \
   GOSL(STR_OS_PHOTON "-64",                 GUEST_OS_PHOTON_64,              NULL)                   \
   GOSL("whistler",                          GUEST_OS_WINXP,                  "winPreVista.iso") /* old */ \
   GOSL("win2000",                           GUEST_OS_WIN2000,                "winPreVista.iso") /* old */ \
   GOSL(STR_OS_WIN_2000_ADV_SERV,            GUEST_OS_WIN2000,                "winPreVista.iso")      \
   GOSL(STR_OS_WIN_2000_PRO,                 GUEST_OS_WIN2000,                "winPreVista.iso")      \
   GOSL(STR_OS_WIN_2000_SERV,                GUEST_OS_WIN2000,                "winPreVista.iso")      \
   GOSL(STR_OS_WIN_31,                       GUEST_OS_WIN31,                  "winPre2k.iso")         \
   GOSL(STR_OS_WIN_95,                       GUEST_OS_WIN95,                  "winPre2k.iso")         \
   GOSL(STR_OS_WIN_98,                       GUEST_OS_WIN98,                  "winPre2k.iso")         \
   GOSL(STR_OS_WIN_SEVEN,                    GUEST_OS_WINSEVEN,               "windows.iso")          \
   GOSL(STR_OS_WIN_SEVEN_X64,                GUEST_OS_WINSEVEN_64,            "windows.iso")          \
   GOSL("windows7Server64Guest",             GUEST_OS_WIN2008R2_64,           "windows.iso")          \
   GOSL(STR_OS_WIN_2008R2_X64,               GUEST_OS_WIN2008R2_64,           "windows.iso")          \
   GOSL(STR_OS_WIN_EIGHT,                    GUEST_OS_WINEIGHT,               "windows.iso")          \
   GOSL(STR_OS_WIN_EIGHT_X64,                GUEST_OS_WINEIGHT_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_EIGHTSERVER_X64,          GUEST_OS_WINEIGHTSERVER_64,      "windows.iso")          \
   GOSL(STR_OS_WIN_TEN,                      GUEST_OS_WINTEN,                 "windows.iso")          \
   GOSL(STR_OS_WIN_TEN_X64,                  GUEST_OS_WINTEN_64,              "windows.iso")          \
   GOSL(STR_OS_WIN_TENSERVER_X64,            GUEST_OS_WINTENSERVER_64,        "windows.iso")          \
   GOSL(STR_OS_HYPER_V,                      GUEST_OS_HYPER_V,                NULL)                   \
   GOSL("winLonghorn64Guest",                GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL("winLonghornGuest",                  GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_ME,                       GUEST_OS_WINME,                  "winPre2k.iso")         \
   GOSL(STR_OS_WIN_NET_BUS,                  GUEST_OS_WINNET,                 "winPreVista.iso")      \
   GOSL("winNetDatacenter",                  GUEST_OS_WINNET,                 "winPreVista.iso")      \
   GOSL("winNetDatacenter-64",               GUEST_OS_WINNET_64,              "winPreVista.iso")      \
   GOSL(STR_OS_WIN_NET_EN,                   GUEST_OS_WINNET,                 "winPreVista.iso")      \
   GOSL("winNetEnterprise-64",               GUEST_OS_WINNET_64,              "winPreVista.iso")      \
   GOSL(STR_OS_WIN_NET_ST,                   GUEST_OS_WINNET,                 "winPreVista.iso")      \
   GOSL("winNetStandard-64",                 GUEST_OS_WINNET_64,              "winPreVista.iso")      \
   GOSL(STR_OS_WIN_NET_WEB,                  GUEST_OS_WINNET,                 "winPreVista.iso")      \
   GOSL(STR_OS_WIN_NT,                       GUEST_OS_WINNT,                  "winPre2k.iso")         \
   GOSL(STR_OS_WIN_2008_CLUSTER,             GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_CLUSTER_X64,         GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_DATACENTER,          GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_DATACENTER_X64,      GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_DATACENTER_CORE,     GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_DATACENTER_CORE_X64, GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_ENTERPRISE,          GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_ENTERPRISE_X64,      GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_ENTERPRISE_CORE,     GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_ENTERPRISE_CORE_X64, GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS,      GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS_X64,  GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM, GUEST_OS_LONGHORN,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_SMALL_BUSINESS_PREMIUM_X64, GUEST_OS_LONGHORN_64,     "windows.iso")          \
   GOSL(STR_OS_WIN_2008_STANDARD,            GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_STANDARD_X64,        GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_STANDARD_CORE,       GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_STANDARD_CORE_X64,   GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_2008_WEB_SERVER,          GUEST_OS_LONGHORN,               "windows.iso")          \
   GOSL(STR_OS_WIN_2008_WEB_SERVER_X64,      GUEST_OS_LONGHORN_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_VISTA,                    GUEST_OS_WINVISTA,               "windows.iso")          \
   GOSL(STR_OS_WIN_VISTA_X64,                GUEST_OS_WINVISTA_64,            "windows.iso")          \
   GOSL(STR_OS_WIN_XP_HOME,                  GUEST_OS_WINXP,                  "winPreVista.iso")      \
   GOSL(STR_OS_WIN_XP_PRO,                   GUEST_OS_WINXP,                  "winPreVista.iso")      \
   GOSL(STR_OS_WIN_XP_PRO_X64,               GUEST_OS_WINXPPRO_64,            "winPreVista.iso")      \

#endif
