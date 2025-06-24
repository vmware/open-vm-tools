/*********************************************************
 * Copyright (c) 1998-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
#ifndef VM_PRODUCT_VERSIONS_H
#define VM_PRODUCT_VERSIONS_H

/*
 *
 * This file contains sections delineated by marker tags like  MARKER_XXX_...
 * These tags are used to generate vm_product_versions_fast.h, which is a
 * version header that enables faster compiles.
 *
 * When BUILD_NUMBER and similar macros are defined as numbers, builds become
 * slow as the consumer can't be cached (the crypto-hash for the C compiler's
 * inputs changes each build). For cases where that is still necessary,
 * vm_product_versions_slow.h can be used. Note: the fast headers require
 * linking to bora/lib/version.
 *
 * to avoid multiple copy of similar header, xx_fast.h code is generated from
 * this file using these marker tag.
 *
 */

//MARKER_FAST_UNCOMMENT_START
//#ifdef __cplusplus
//extern "C" {
//#endif
//
//extern unsigned int gProductVersion[];
//extern unsigned int gVieFileVersion[];
//extern unsigned int gVpxVersionNumeric[];
//extern unsigned int gNetdumpFileVersion[];
//extern unsigned int gFoundryFileVersion[];
//
//extern unsigned int gOvftoolFileVersion[];
//extern const char gEsxReleaseVersionString[];
//
//#ifdef __cplusplus
//}
//#endif
//MARKER_FAST_UNCOMMENT_END

/*
 * NOTE: Some of the macro expansions in this file require information
 *       from the generated file, 'buildNumber.h'.  For those
 *       expansions, and for those expansions only, you must include
 *       "vm_version.h" first.
 */

/*
 * Used in .rc files on the Win32 platform. We must use PRODUCT_BUILD_NUMBER
 * in numeric Win32 version numbers to stay below the 65k (circa) limit.
 *
 * When building the Tools, we make an effort to follow the "internal" Tools
 * version. Otherwise we use a hard-coded value for Workstation and a different
 * hard-coded value for every other product.
 */

//MARKER_FAST_REMOVE_START
#if defined(VMX86_VMRC) /* check VMX86_VMRC before VMX86_DESKTOP */
   #define PRODUCT_VERSION    13,0,0,0
#elif defined(VMX86_TOOLS)
   #define PRODUCT_VERSION    TOOLS_VERSION_EXT_CURRENT_CSV
#elif defined(VMX86_VLICENSE)
   #define PRODUCT_VERSION    1,1,5,PRODUCT_BUILD_NUMBER_NUMERIC
#elif defined(VMX86_VPX)
   /* this should be kept in sync with the corresponding vpx branch. */
   #define PRODUCT_VERSION    9,1,0,PRODUCT_BUILD_NUMBER_NUMERIC
// VMX86_DESKTOP must be last because it is the default and is always defined.
#elif defined(VMX86_DESKTOP)
   // WORKSTATION_VERSION_NUMBER below has to match this
   #define PRODUCT_VERSION    17,0,0,PRODUCT_BUILD_NUMBER_NUMERIC
#elif defined(VMX86_SYSIMAGE)
   #define PRODUCT_VERSION    TOOLS_VERSION_EXT_CURRENT_CSV
   #define SYSIMAGE_VERSION TOOLS_VERSION_CURRENT_STR
   #define SYSIMAGE_VERSION_EXT_STR TOOLS_VERSION_EXT_CURRENT_STR
#else
   /* Generic catch-all. */
   #define PRODUCT_VERSION    0,0,0,PRODUCT_BUILD_NUMBER_NUMERIC
#endif
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define PRODUCT_VERSION gProductVersion
//MARKER_FAST_UNCOMMENT_END

/*
 * The VIE components are shared by different products and may be updated by newer
 * installers. Since the installer replaces the component files based on the version
 * resource, it's important that the file version be monotonically increasing. As
 * a result, these components need their own file version number that is
 * independent of the VMX86_XXX macros. This goes into the FILEVERSION property of
 * the version resources. The first release of this stuff was with VPX which had a
 * FILEVERSION of 1,0,0,PRODUCT_BUILD_NUMBER_NUMERIC
 *
 * P2VA 2.0     : 2,1,2
 * VPX 1.2      : 2,1,3
 * V2V 1.0      : 2.2.0
 * SYSIMAGE 1.0 : 2.2.1 or later (TBD)
 * Symantec     : 2.2.2 7/2005
 * VC 2.0       : 2.2.3
 * P2V 2.1      : 2.2.4 (also used for WS55 betas and RC)
 * V2V 1.5      : 2.2.5 V2V 1.5 released with WS55
 * WS 5.1       : 2.2.5 to be set when WS55 branches
 * VCB 1.0      : e.x.p esx-dali: first release with vmacore + vstor3Bus
 * VCB 1.0.1    : 3.0.1 includes security fix for registry alteration vulnerability
 * VCB 1.1      : 3.1
 * VMI 2.0      : 3.1.0
 * P2VA 3.0     : 3.?.?
 */
//MARKER_FAST_REMOVE_START
#define VIE_FILEVERSION 6,5,0,PRODUCT_BUILD_NUMBER_NUMERIC
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define VIE_FILEVERSION gVieFileVersion
//MARKER_FAST_UNCOMMENT_END

/*
 * This string can be a little more "free form".  The license
 * manager doesn't depend on it.  This is the version that will
 * be used by the build process, the UI, etc.  Things people see.
 *
 * If platforms are on different version numbers, manage it here.
 *
 * Manage version numbers for each product here.
 *
 *  NOTE:  BE AWARE that Scons/Makefiles and build scripts depend
 *         on these defines.
 *
 *         In particular, only the first quoted token after the
 *         macro name will be used for the macro value by the build
 *         system.  Also, if VERSION_MAJOR, VERSION_MINOR, and
 *         VERSION_MAINT macros are defined, they override the
 *         VERSION macro in the build system.
 */

/* ESX release versioning scheme:
 *
 * <major>.<minor>.<maintenance>.<patch>
 *
 * This maps to MACROs:
 *
 * <MAJOR>.<MINOR>.<MAINT>.<PATCH>
 *
 * The reason of the mismatches is that scons assumes all products would define
 * macros in the same style.
 *
 * Rules for updating macros:
 *
 * Set MINOR to 0 for all initial GA/pre-release build.  Increment for
 * each on-prem update release.
 *
 * Set MAINT to 0 for all update release builds.  Increment it for
 * each on-prem maintenance release.
 *
 * Set PATCH to 0 for all initial maintenance builds.  Increment it for
 * each build that will be delivered externally.
 *
 * THEORETICAL EXAMPLES:
 *
 * 9.0.0.0: Pre-release/GA
 * 9.1.0.0: Update 1
 * 9.1.1.2000: Emergency Patch 20
 * 9.1.1.2010: Hot Patch 10 on Emergency Patch 20
 */
#define ESX_VERSION_MAJOR "9"
#define ESX_VERSION_MINOR "1" // Minor or update version
#define ESX_VERSION_MAINT "0" // 0 = Pre-release/GA, 3 = maintenance release 3
#define ESX_VERSION_UPDATE ESX_VERSION_MINOR // ESX's update level: 9.0 semantic

#define ESX_VERSION ESX_VERSION_MAJOR "." ESX_VERSION_MINOR "." \
                    ESX_VERSION_MAINT
#define ESX_VERSION_THIRD_PARTY ESX_VERSION_MAJOR ESX_VERSION_MINOR \
                                ESX_VERSION_MAINT

#define ESX_RELEASE_UPDATE "0" // Dummy; Retired in 9.0

/* Major/minor/maintenance release: 0
 * Emergency patch: 4 digit EP00
 * Hot patch: 4 digit EPHP
 * Initial value: 0
 */
#define ESX_RELEASE_PATCH "0"
#define ESX_RELEASE ESX_RELEASE_PATCH

// ESX release versioning string -
//    <major>.<minor>.<maint>-<patch>.<buildid>
//MARKER_FAST_REMOVE_START
#define ESX_RELEASE_VERSION_STR ESX_VERSION "-" ESX_RELEASE "." \
                                XSTR(BUILD_NUMBER_NUMERIC)
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define ESX_RELEASE_VERSION_STR gEsxReleaseVersionString
//MARKER_FAST_UNCOMMENT_END

#define WORKSTATION_RELEASE_DESCRIPTION ""
#define P2V_VERSION "e.x.p"
#define P2V_FILE_VERSION 3,0,0,0

/*
 * HEADS UP:  Don't merge patch version bumps (e.g. x.y.0 -> x.y.1) to CBS
 * branches (*-main), 'cuz it breaks stuff in VIX land.  See bug 939456.
 * necessary VIX changes also need to be done. See bug 939456.
 *
 * ALSO, leave FOO_VERSION at e.x.p on all EXCEPT release branches.
 */
#define WORKSTATION_VERSION_NUMBER "17.0.0" /* this version number should always match real WS version number */
#define WORKSTATION_VERSION "e.x.p"
#define PLAYER_VERSION_NUMBER "17.0.0" /* this version number should always match real Player version number */
#define PLAYER_VERSION "e.x.p"

#define THINPRINT_VERSION "1.1.2"

/*
 * In the *-main branches, FUSION_VERSION should always be set to "e.x.p".
 * In a Fusion release branch, when you modify FUSION_VERSION, check that the
 * computation of 'lastVersion' in
 * bora/install/desktop/macos/makedmg.sh::GenerateDescriptorXML() does what you
 * want.
 */
#define FUSION_VERSION "e.x.p"

#define VIM_VERSION "9.1.0"
/*
 *For smooth version bump up for quaterly releases, we need to have a fallback
 *mechanism to previous version in all those components which perform version
 *specific tasks. Assuming that components can create version specific
 *functions if they can't eliminate version specific code, a default behavior
 *to fallback to previous version will decouple the component's version
 *specific work from version bump up activity in this file. We e.g. change
 *current version to 6.7 in this file, those functions which are written for
 *6.6 will be used until new functions for 6.7 arent available.
 *This way version bump up activity will ideally need just these steps
 *1. Change current version
 *2. Add a row for fresh previous version
*/
/*
 *VCENTER_PREVIOUS_VERSIONS
 *Macro to store all vCenter previous versions. This will be used by component
 *to move to suitale previous product version related functions is current or
 *specific is not available.
 *Please keep the version order correct for avoiding any potential issue
 */
#define VCENTER_PREVIOUS_VERSIONS \
"4.0.0,\
4.1.0,\
5.0.0,\
5.1.0,\
5.5.0,\
6.0.0,\
6.5.0"
// Put VPX_VERSION first, because vpx/make/defs.mk doesn't check for suffix.
#define VPX_VERSION "9.1.0"
#define VPX_VERSION_MAJOR "9"
#define VPX_VERSION_MINOR "1"
#define VPX_VERSION_MAINT "0"
#define VPX_VERSION_PATCH "0"
#define VPX_VERSION_THIRD_PARTY VPX_VERSION_MAJOR VPX_VERSION_MINOR \
                                VPX_VERSION_MAINT
//MARKER_FAST_REMOVE_START
#define VPX_VERSION_NUMERIC 9,1,0,PRODUCT_BUILD_NUMBER_NUMERIC
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define VPX_VERSION_NUMERIC gVpxVersionNumeric
//MARKER_FAST_UNCOMMENT_END

// Last supported ESX version by VC.
#define VPX_MIN_HOST_VERSION "8.0.0"

#define MAX_SUPPORTED_VI_VERSION "6.6" //from ovfTool/src/supportedVersions.h
#define VCDB_CURRENT_SCHEMA_VERSION           910 // from PitCADatabase.h

#define VPX_RELEASE_UPDATE "0" /* 0 = Pre-release/GA, 1 = Update 1 */
#define VPX_RELEASE_PATCH "0"  /* 0 = experimental */
#define VPX_RELEASE VPX_RELEASE_UPDATE "." VPX_RELEASE_PATCH

/* expected database version for current release */
#define VPXD_VDB_DB_VERSION_ID            910
#define VPXD_VDB_DB_VERSION_VALUE         "VirtualCenter Database 9.1"

/*
 * OSM Release Version for OSS/TP usage tracking and ticket filing
 */
#define VPX_OSM_VERSION "latest"

// Virtual Appliance Patch Version Number
// This is the last component of the VCSA w.x.y.z version number
// While patching / minor update this number is used by VCSA
// to validate a patch iso .
// Changing the version is required when CPD releases an update.
#define VA_PATCH_VERSION  "5100"

// esxcli
#define ESXCLI_VERSION "8.0.0"

/* Integrity is bundled with VPX so use VPX_VERSION*/
#define INTEGRITY_VERSION VPX_VERSION
#define SVA_VERSION "1.0.0"
#define SSO_VERSION "1.0.0"
#define SDK_VERSION "4.1.0"
#define FOUNDRY_VERSION "1.17.0"

//MARKER_FAST_REMOVE_START
#define FOUNDRY_FILE_VERSION 1,17,0,PRODUCT_BUILD_NUMBER_NUMERIC
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define FOUNDRY_FILE_VERSION gFoundryFileVersion
//MARKER_FAST_UNCOMMENT_END

#define VLICENSE_VERSION "1.1.5"
#define DDK_VERSION "e.x.p"
#define VIPERL_VERSION "7.0.0"
#define RCLI_VERSION "7.0.0"
#define NETDUMP_VERSION        "5.1.0"

//MARKER_FAST_REMOVE_START
#define NETDUMP_FILE_VERSION    5,1,0,PRODUCT_BUILD_NUMBER_NUMERIC
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define NETDUMP_FILE_VERSION  gNetdumpFileVersion
//MARKER_FAST_UNCOMMENT_END

#define VMRC_VERSION          "13.0.0.0"

#define VDDK_VERSION          "9.1.0.0"
#define VDDK_VERSION_MAJOR    9
#define VDDK_VERSION_MINOR    1
#define VDDK_VERSION_MAINT    0
#define VDDK_VERSION_PATCH    0

//MARKER_FAST_REMOVE_START
#define VDDK_FILE_VERSION     VDDK_VERSION_MAJOR,VDDK_VERSION_MINOR,\
                              VDDK_VERSION_MAINT,VDDK_VERSION_PATCH
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define VDDK_FILE_VERSION gVddkFileVersion
//MARKER_FAST_UNCOMMENT_END

#define OVFTOOL_VERSION "5.1.0"
#define VCSA_INSTALLER_VERSION "1.0.0"

//MARKER_FAST_REMOVE_START
#define OVFTOOL_FILE_VERSION 5,1,0,PRODUCT_BUILD_NUMBER_NUMERIC
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#define OVFTOOL_FILE_VERSION gOvftoolFileVersion
//MARKER_FAST_UNCOMMENT_END

#define VGAUTH_VERSION "1.0.0"
#define COMMON_AGENT_VERSION "e.x.p"
#define VIEWY_VERSION "e.x.p"
#define VMCFSDK_VERSION "e.x.p"
#define PCOIP_VERSION "e.x.p"
#define HOSTD_VERSION "e.x.p"
#define RECOVERYLIBS_VERSION "2.0.0"
#define PRECHECK_VERSION "e.x.p"
#define VHSESDK_VERSION "1.0.0"
#define VIEWVC_VERSION "14.0.2"
#define WCP_VERSION "9.0.0.0"
#define VSTATS_VERSION "0.0.1"
#define XVP_VERSION "1.0.0"

#ifndef MAKESTR
#define MAKESTR(x) #x
#define XSTR(x) MAKESTR(x)
#endif

/*
 * The current Tools version, derived from vm_tools_version.h. Do not modify this.
 */
#define TOOLS_VERSION TOOLS_VERSION_CURRENT_STR

#ifdef VMX86_VPX
#define VIM_API_TYPE "VirtualCenter"
#else
#define VIM_API_TYPE "HostAgent"
#endif

#define VIM_EESX_PRODUCT_LINE_ID "embeddedEsx"
#define VIM_ESX_PRODUCT_LINE_ID "esx"
#define VIM_ESXIO_PRODUCT_LINE_ID "esxio"
#define VIM_WS_PRODUCT_LINE_ID "ws"

#if defined(VMX86_VMRC) /* check VMX86_VMRC before VMX86_DESKTOP */
#  define PRODUCT_VERSION_NUMBER VMRC_VERSION
#elif defined(VMX86_SERVER)
#  define PRODUCT_VERSION_NUMBER ESX_VERSION
#elif defined(VMX86_VPX)
#  if defined(XVP)
#     define PRODUCT_VERSION_NUMBER XVP_VERSION
#  else
#     define PRODUCT_VERSION_NUMBER VPX_VERSION
#  endif
#elif defined(VMX86_SDK)
#  define PRODUCT_VERSION_NUMBER SDK_VERSION
#elif defined(VMX86_P2V)
#  define PRODUCT_VERSION_NUMBER P2V_VERSION
#elif defined(VMX86_VIPERL)
#  define PRODUCT_VERSION_NUMBER VIPERL_VERSION
#elif defined(VMX86_SYSIMAGE)
#  define PRODUCT_VERSION_NUMBER SYSIMAGE_VERSION
#elif defined(VMX86_VCB)
#  define PRODUCT_VERSION_NUMBER VCB_VERSION
#elif defined(VMX86_FOUNDRY)
#  define PRODUCT_VERSION_NUMBER FOUNDRY_VERSION
#elif defined(VMX86_VLICENSE)
#  define PRODUCT_VERSION_NUMBER VLICENSE_VERSION
#elif defined(VMX86_DDK)
#  define PRODUCT_VERSION_NUMBER DDK_VERSION
#elif defined(VMX86_TOOLS)
#  define PRODUCT_VERSION_NUMBER TOOLS_VERSION
#elif defined(VMX86_VDDK)
#  define PRODUCT_VERSION_NUMBER VDDK_VERSION
#elif defined(VMX86_HBR_SERVER)
#  define PRODUCT_VERSION_NUMBER ESX_VERSION
#elif defined(VMX86_INTEGRITY)
#  define PRODUCT_VERSION_NUMBER INTEGRITY_VERSION
#elif defined(VMX86_VGAUTH)
#  define PRODUCT_VERSION_NUMBER VGAUTH_VERSION
 // VMX86_DESKTOP must be last because it is the default and is always defined.
#elif defined(VMX86_DESKTOP)
#  if defined(__APPLE__)
#    define PRODUCT_VERSION_NUMBER FUSION_VERSION
#  else
#    define PRODUCT_VERSION_NUMBER WORKSTATION_VERSION
#  endif
#endif

/*
 * Continue to just append BUILD_NUMBER here, PRODUCT_BUILD_NUMBER is
 * not needed in the string.
 */

//MARKER_FAST_REMOVE_START
#define PRODUCT_VERSION_STRING PRODUCT_VERSION_NUMBER " " BUILD_NUMBER
//MARKER_FAST_REMOVE_END
//MARKER_FAST_UNCOMMENT_START
//#ifdef __cplusplus
//extern "C" {
//#endif
//extern const char gProductVersionString[];
//#ifdef __cplusplus
//}
//#endif
//#define PRODUCT_VERSION_STRING gProductVersionString
//MARKER_FAST_UNCOMMENT_END
/*
 * The license manager requires that PRODUCT_VERSION_STRING matches the
 * following pattern: <x>[.<y>][.<z>].
 *
 * If platforms are on different version numbers, manage it here.
 */

/*
 * Note: changing PRODUCT_NAME_FOR_LICENSE and PRODUCT_LICENSE_VERSION
 * or macros it cleverly depends on (such as PRODUCT_NAME) requires a
 * coordinated dormant license file change. Otherwise licensing for
 * that product may break because the Licensecheck API is being passed
 * a parameter that no longer match the content of the dormant license
 * file.
 */
#define PRODUCT_MAC_DESKTOP_VERSION_STRING_FOR_LICENSE "13.0"
#define PRODUCT_PLAYER_VERSION_STRING_FOR_LICENSE "17.0"
#define PRODUCT_VMRC_VERSION_STRING_FOR_LICENSE "13.0"

#if defined(VMX86_TOOLS)
/* This product doesn't use a license */
#  define PRODUCT_VERSION_STRING_FOR_LICENSE ""
#  define PRODUCT_LICENSE_VERSION "0.0"
#else
#  if defined(VMX86_SERVER)
#    define PRODUCT_LICENSE_VERSION "9.0"
#  elif defined(VMX86_VMRC) /* check VMX86_VMRC before VMX86_DESKTOP */
#    define PRODUCT_LICENSE_VERSION PRODUCT_VMRC_VERSION_STRING_FOR_LICENSE
#  elif defined(VMX86_VPX)
#    define PRODUCT_LICENSE_VERSION "9.0"
#    define PRODUCT_LICENSE_FILE_VERSION "9.0.1.0"
#  elif defined(VMX86_SDK)
#    define PRODUCT_LICENSE_VERSION "1.0"
#  elif defined(VMX86_P2V)
#    define PRODUCT_LICENSE_VERSION "1.0"
// VMX86_DESKTOP must be last because it is the default and is always defined.
#  elif defined(VMX86_DESKTOP)
#    if defined(__APPLE__)
#      define PRODUCT_LICENSE_VERSION PRODUCT_MAC_DESKTOP_VERSION_STRING_FOR_LICENSE
#    else
#      define PRODUCT_LICENSE_VERSION "17.0"
#    endif
#  else
#    define PRODUCT_LICENSE_VERSION "0.0"
#  endif
#  define PRODUCT_VERSION_STRING_FOR_LICENSE PRODUCT_LICENSE_VERSION
#endif
#define PRODUCT_ESX_LICENSE_VERSION "9.0"
#define PRODUCT_ESX_LICENSE_FILE_VERSION "9.0.1.0"

/*
 * The configuration file version string should be changed
 * whenever we make incompatible changes to the config file
 * format or to the meaning of settings.  When we do this,
 * we must also add code that detects the change and can
 * convert an old config file to a new one.
 *
 * In practice, config.version is no longer modified. Instead
 * we avoid making incompatible changes to the config file
 * format and the meaning of an individual setting is never
 * changed.
 */

#define CONFIG_VERSION_VARIABLE         "config.version"

/*
 * PREF_VERSION_VARIABLE somehow cannot be written through Dictionary_Write
 * (there is a bug after the first reload). So it's not used.
 */
/* #define PREF_VERSION_VARIABLE        "preferences.version"*/

#define CONFIG_VERSION_DEFAULT          1    /* if no version in file*/
#define CONFIG_VERSION                  8

#define CONFIG_VERSION_UNIFIEDSVGAME    3    /* Merged (S)VGA for WinME*/
#define CONFIG_VERSION_UNIFIEDSVGA      4    /* Merged (S)VGA enabled.  -J.*/
#define CONFIG_VERSION_440BX            5    /* 440bx becomes default */
#define CONFIG_VERSION_NEWMACSTYLE      3    /* ethernet?.oldMACStyle */
#define CONFIG_VERSION_WS2              2    /* config version of WS2.0.x */
#define CONFIG_VERSION_MIGRATION        6    /* migration work for WS3 */
#define CONFIG_VERSION_ESX2             6    /* config version of ESX 2.x */
#define CONFIG_VERSION_UNDOPOINT        7    /* Undopoint paradigm (WS40) */
#define CONFIG_VERSION_WS4              7    /* config version of WS4.0.x */
#define CONFIG_VERSION_MSNAP            8    /* Multiple Snapshots */
#define CONFIG_VERSION_WS5              8    /* WS5.0 */

/*
 * Product version strings allows UIs to refer to a single place for specific
 * versions of product names.  These do not include a "VMware" prefix.
 */

#define PRODUCT_VERSION_SCALABLE_SERVER_1 PRODUCT_SCALABLE_SERVER_BRIEF_NAME " 1.x"
#define PRODUCT_VERSION_SCALABLE_SERVER_2 PRODUCT_SCALABLE_SERVER_BRIEF_NAME " 2.x"
#define PRODUCT_VERSION_SCALABLE_SERVER_3 PRODUCT_SCALABLE_SERVER_BRIEF_NAME " 3.x"
#define PRODUCT_VERSION_SCALABLE_SERVER_30 PRODUCT_SCALABLE_SERVER_BRIEF_NAME " 3.0"
#define PRODUCT_VERSION_SCALABLE_SERVER_31 PRODUCT_SCALABLE_SERVER_BRIEF_NAME " 3.5"
#define PRODUCT_VERSION_SCALABLE_SERVER_40 PRODUCT_ESXI_BRIEF_NAME " 4.x"
#define PRODUCT_VERSION_SCALABLE_SERVER_50 PRODUCT_ESXI_BRIEF_NAME " 5.0"
#define PRODUCT_VERSION_SCALABLE_SERVER_51 PRODUCT_ESXI_BRIEF_NAME " 5.1"
#define PRODUCT_VERSION_SCALABLE_SERVER_55 PRODUCT_ESXI_BRIEF_NAME " 5.5"
#define PRODUCT_VERSION_SCALABLE_SERVER_60 PRODUCT_ESXI_BRIEF_NAME " 6.0"
#define PRODUCT_VERSION_SCALABLE_SERVER_65 PRODUCT_ESXI_BRIEF_NAME " 6.5"
#define PRODUCT_VERSION_SCALABLE_SERVER_67 PRODUCT_ESXI_BRIEF_NAME " 6.7"
#define PRODUCT_VERSION_SCALABLE_SERVER_70 PRODUCT_ESXI_BRIEF_NAME " 7.0"
#define PRODUCT_VERSION_SCALABLE_SERVER_701 PRODUCT_ESXI_BRIEF_NAME " 7.0 U1"
#define PRODUCT_VERSION_SCALABLE_SERVER_702 PRODUCT_ESXI_BRIEF_NAME " 7.0 U2"
#define PRODUCT_VERSION_WGS_1 "Server 1.x"
#define PRODUCT_VERSION_WGS_2 "Server 2.x"
#define PRODUCT_VERSION_GSX_3 "GSX Server 3.x"
#define PRODUCT_VERSION_WORKSTATION_4 PRODUCT_WORKSTATION_BRIEF_NAME " 4.x"
#define PRODUCT_VERSION_WORKSTATION_5 PRODUCT_WORKSTATION_BRIEF_NAME " 5.x"
#define PRODUCT_VERSION_WORKSTATION_6 PRODUCT_WORKSTATION_BRIEF_NAME " 6.0"
#define PRODUCT_VERSION_WORKSTATION_65 PRODUCT_WORKSTATION_BRIEF_NAME " 6.5"
#define PRODUCT_VERSION_WORKSTATION_7 PRODUCT_WORKSTATION_BRIEF_NAME " 7.x"
#define PRODUCT_VERSION_WORKSTATION_80 PRODUCT_WORKSTATION_BRIEF_NAME " 8.x"
#define PRODUCT_VERSION_WORKSTATION_90 PRODUCT_WORKSTATION_BRIEF_NAME " 9.x"
#define PRODUCT_VERSION_WORKSTATION_100 PRODUCT_WORKSTATION_BRIEF_NAME " 10.x"
#define PRODUCT_VERSION_WORKSTATION_110 PRODUCT_WORKSTATION_BRIEF_NAME " 11.x"
#define PRODUCT_VERSION_WORKSTATION_120 PRODUCT_WORKSTATION_BRIEF_NAME " 12.x"
// Workstation 13.x is skipped.
#define PRODUCT_VERSION_WORKSTATION_140 PRODUCT_WORKSTATION_BRIEF_NAME " 14.x"
#define PRODUCT_VERSION_WORKSTATION_150 PRODUCT_WORKSTATION_BRIEF_NAME " 15.x"
#define PRODUCT_VERSION_WORKSTATION_160 PRODUCT_WORKSTATION_BRIEF_NAME " 16.x"
#define PRODUCT_VERSION_WORKSTATION_162 PRODUCT_WORKSTATION_BRIEF_NAME " 16.2.x"
#define PRODUCT_VERSION_WORKSTATION_170 PRODUCT_WORKSTATION_BRIEF_NAME " 17.x"
#define PRODUCT_VERSION_WORKSTATION_175 PRODUCT_WORKSTATION_BRIEF_NAME " 17.5 or later"
#define PRODUCT_VERSION_PLAYER_1 PRODUCT_PLAYER_BRIEF_NAME " 1.x"
#define PRODUCT_VERSION_MAC_DESKTOP_1 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 1.1"
#define PRODUCT_VERSION_MAC_DESKTOP_2 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 2.x"
#define PRODUCT_VERSION_MAC_DESKTOP_3 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 3.x"
#define PRODUCT_VERSION_MAC_DESKTOP_40 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 4.x"
#define PRODUCT_VERSION_MAC_DESKTOP_50 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 5.x"
#define PRODUCT_VERSION_MAC_DESKTOP_60 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 6.x"
#define PRODUCT_VERSION_MAC_DESKTOP_70 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 7.x"
#define PRODUCT_VERSION_MAC_DESKTOP_80 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 8.x"
// Fusion 9.x is skipped.
#define PRODUCT_VERSION_MAC_DESKTOP_100 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 10.x"
#define PRODUCT_VERSION_MAC_DESKTOP_110 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 11.x"
#define PRODUCT_VERSION_MAC_DESKTOP_120 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 12.x"
#define PRODUCT_VERSION_MAC_DESKTOP_122 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 12.2.x"
#define PRODUCT_VERSION_MAC_DESKTOP_130 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 13.x"
#define PRODUCT_VERSION_MAC_DESKTOP_135 PRODUCT_MAC_DESKTOP_BRIEF_NAME " 13.5 or later"

/*
 * VDFS Versions
 */
#define VDFS_VERSION_MAJOR "0"
#define VDFS_VERSION_MINOR "1"
#define VDFS_VERSION_MAINT "0"
#define VDFS_VERSION VDFS_VERSION_MAJOR "." VDFS_VERSION_MINOR "." \
                    VDFS_VERSION_MAINT
#define VDFS_RELEASE_UPDATE "0" /* 0 = Pre-release/GA, 1 = Update 1 */
#define VDFS_RELEASE_PATCH "0"  /* 0 = experimental */
#define VDFS_RELEASE VDFS_RELEASE_UPDATE "." VDFS_RELEASE_PATCH

/*
 * ZDOM Versions
 */
#define ZDOM_VERSION_MAJOR "0"
#define ZDOM_VERSION_MINOR "1"
#define ZDOM_VERSION_MAINT "0"
#define ZDOM_VERSION ZDOM_VERSION_MAJOR "." ZDOM_VERSION_MINOR "." \
                    ZDOM_VERSION_MAINT
#define ZDOM_RELEASE_UPDATE "0" /* 0 = Pre-release/GA, 1 = Update 1 */
#define ZDOM_RELEASE_PATCH "0"  /* 0 = experimental */
#define ZDOM_RELEASE ZDOM_RELEASE_UPDATE "." ZDOM_RELEASE_PATCH

/*
 * vsansky Versions
 */
#define VSANSKY_VERSION_MAJOR "0"
#define VSANSKY_VERSION_MINOR "1"
#define VSANSKY_VERSION_MAINT "0"
#define VSANSKY_VERSION VSANSKY_VERSION_MAJOR "." VSANSKY_VERSION_MINOR "." \
                       VSANSKY_VERSION_MAINT
#define VSANSKY_RELEASE_UPDATE "0" /* 0 = Pre-release/GA, 1 = Update 1 */
#define VSANSKY_RELEASE_PATCH "0"  /* 0 = experimental */
#define VSANSKY_RELEASE VSANSKY_RELEASE_UPDATE "." VSANSKY_RELEASE_PATCH

#endif
