/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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


#ifndef VM_PRODUCT_H
#define VM_PRODUCT_H

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"


/****
 ****
 **** PLEASE use the various PRODUCT_* and *_NAME defines as though
 **** they were variables -- do not embed them in format strings,
 **** since they could contain a "%" sign or actually be a variable
 **** someday.
 ****
 ****/


/*
 * This name should be used when referring to the company
 */
#define COMPANY_NAME       "VMware, Inc."


/*
 * This generic name should be used when referring to any product of the
 * VMware product line, like VMware Workstation, VMware Server, and so
 * on...
 */
#define PRODUCT_GENERIC_NAME "VMware"
#define PRODUCT_GENERIC_NAME_UPPER "VMWARE"
#define PRODUCT_GENERIC_NAME_LOWER "vmware"


/*
 * Brief names are used when the VMware prefix is not wanted.
 */
#define PRODUCT_SCALABLE_SERVER_BRIEF_NAME "ESX"
#define PRODUCT_ESXI_BRIEF_NAME "ESXi"
#define PRODUCT_WORKSTATION_BRIEF_NAME "Workstation"
#define PRODUCT_WORKSTATION_ENTERPRISE_BRIEF_NAME \
         PRODUCT_WORKSTATION_BRIEF_NAME " " "ACE Edition"
#define PRODUCT_WORKSTATION_SERVER_BRIEF_NAME "Workstation Server"
#define PRODUCT_PLAYER_BRIEF_NAME "Player"
#define PRODUCT_ACE_PLAYER_BRIEF_NAME "ACE " PRODUCT_PLAYER_BRIEF_NAME
#define PRODUCT_MAC_DESKTOP_BRIEF_NAME "Fusion"
#define PRODUCT_ACE_MANAGEMENT_SERVER_BRIEF_NAME "ACE Management Server"
#define PRODUCT_VMRC_BRIEF_NAME "Remote Console"
#define PRODUCT_GANTRY_BRIEF_NAME "AppCatalyst"


/*
 * Product names include the formal VMware prefix.
 */
#define MAKE_NAME(_brief) PRODUCT_GENERIC_NAME " " _brief

/*
 * This name should be used when referring to VMware Tools
 */
#define VMWARE_TOOLS_SHORT_NAME MAKE_NAME("Tools")

#define PRODUCT_SCALABLE_SERVER_NAME MAKE_NAME(PRODUCT_SCALABLE_SERVER_BRIEF_NAME)
#define PRODUCT_ESX_SMP_NAME MAKE_NAME("Virtual SMP for ESX Server")
#define PRODUCT_WORKSTATION_NAME MAKE_NAME(PRODUCT_WORKSTATION_BRIEF_NAME)
#define PRODUCT_WORKSTATION_ENTERPRISE_NAME MAKE_NAME(PRODUCT_WORKSTATION_ENTERPRISE_BRIEF_NAME)
#define PRODUCT_WORKSTATION_SERVER_NAME MAKE_NAME(PRODUCT_WORKSTATION_SERVER_BRIEF_NAME)
#define PRODUCT_MUI_NAME MAKE_NAME("Management Interface")
#define PRODUCT_CONSOLE_NAME MAKE_NAME("Server Console")
#define PRODUCT_PLAYER_NAME MAKE_NAME(PRODUCT_PLAYER_BRIEF_NAME)
#define PRODUCT_PLAYER_NAME_FOR_LICENSE PRODUCT_PLAYER_NAME
#define PRODUCT_ACE_PLAYER_NAME MAKE_NAME(PRODUCT_ACE_PLAYER_BRIEF_NAME)
#define PRODUCT_ACE_MANAGEMENT_SERVER_NAME MAKE_NAME(PRODUCT_ACE_MANAGEMENT_SERVER_BRIEF_NAME)
#define PRODUCT_MAC_DESKTOP_NAME_FOR_LICENSE "VMware Fusion for Mac OS"
#define PRODUCT_VMRC_NAME MAKE_NAME(PRODUCT_VMRC_BRIEF_NAME)
#define PRODUCT_VMRC_NAME_FOR_LICENSE PRODUCT_VMRC_NAME
#define PRODUCT_GANTRY_NAME MAKE_NAME(PRODUCT_GANTRY_BRIEF_NAME)
#define PRODUCT_GANTRY_NAME_FOR_LICENSE PRODUCT_GANTRY_NAME

#define PRODUCT_VMLS_SHORT_NAME "VMLS"
#define PRODUCT_VMLS_NAME MAKE_NAME("License Server")

#define PRODUCT_VLICENSE_SHORT_NAME "VLICENSE"
#define PRODUCT_VLICENSE_NAME MAKE_NAME("License Infrastructure")

#define PRODUCT_P2V_SHORT_NAME "P2V"
#define PRODUCT_P2V_NAME MAKE_NAME("P2V Assistant")

#define PRODUCT_V2V_SHORT_NAME "V2V"
#define PRODUCT_V2V_NAME MAKE_NAME("Virtual Machine Importer")

#define PRODUCT_SYSIMAGE_SHORT_NAME "SysImage"
#define PRODUCT_SYSIMAGE_NAME MAKE_NAME("System Image Framework")

#define PRODUCT_VCB_SHORT_NAME "VCB"
#define PRODUCT_VCB_NAME MAKE_NAME("Consolidated Backup")

#define PRODUCT_VPX_NAME MAKE_NAME("VirtualCenter")

#define PRODUCT_VPXA_NAME PRODUCT_VPX_NAME " Agent"

#define PRODUCT_FDM_NAME MAKE_NAME("Fault Domain Manager")

#define PRODUCT_HA_NAME MAKE_NAME("High Availability Extension")

#define PRODUCT_WBC_NAME MAKE_NAME("WebCenter")

#define PRODUCT_SDK_NAME MAKE_NAME("SDK")

#define PRODUCT_DDK_NAME MAKE_NAME("ESX DDK")

#define PRODUCT_NGCINSTALLER_NAME MAKE_NAME("vSphere Web Client")

#define PRODUCT_SSOINSTALLER_NAME MAKE_NAME("Single Sign On")

#define PRODUCT_SSOREGMM_NAME MAKE_NAME("vCenter Registration Tool")

// XXX I think these are dead and can be removed -clayton
// #define PRODUCT_VDM_SHORT_NAME "VDM"
// #define PRODUCT_VDM_NAME MAKE_NAME("Virtual Desktop Manager")

#define PRODUCT_VDDK_SHORT_NAME "VDDK"
#define PRODUCT_VDDK_NAME MAKE_NAME("Virtual Disk Development Kit")

#define PRODUCT_VDM_CLIENT_NAME MAKE_NAME("Horizon Client")
#define PRODUCT_VDM_CLIENT_NAME_FOR_LICENSE PRODUCT_VDM_CLIENT_NAME

#define PRODUCT_XVP_SHORT_NAME "XVP"
#define PRODUCT_XVP_NAME MAKE_NAME("vCenter XVP Manager")
#define PRODUCT_RMKSCONTAINER_NAME MAKE_NAME("Remote MKS Container")

#define PRODUCT_HBR_SERVER_NAME MAKE_NAME("vSphere Replication Server")

#define PRODUCT_VIEW_SHORT_NAME "Horizon View"
#define PRODUCT_VIEW_NAME MAKE_NAME("Horizon View")
#define PRODUCT_VIEW_NAME_FOR_LICENSE PRODUCT_VIEW_NAME

#define PRODUCT_FLEX_GENERIC_SHORT_NAME "Horizon FLEX"
#define PRODUCT_FLEX_GENERIC_NAME MAKE_NAME(PRODUCT_FLEX_GENERIC_SHORT_NAME)

#define PRODUCT_FLEX_SERVER_SHORT_NAME PRODUCT_FLEX_GENERIC_SHORT_NAME " Server"
#define PRODUCT_FLEX_SERVER_NAME MAKE_NAME(PRODUCT_FLEX_SERVER_SHORT_NAME)

#define PRODUCT_VMCF_NAME MAKE_NAME("VMCF")

// XXX VMvisor is the underlying technology for possibly several products,
// XXX not the product. Fix when names are decided.
// #define PRODUCT_VMVISOR_NAME MAKE_NAME("VMvisor")
// XXX Only one product for now so just hardcode it.
#define PRODUCT_VMVISOR_NAME MAKE_NAME(PRODUCT_SCALABLE_SERVER_BRIEF_NAME "i")

#if defined(__linux__) || defined(__FreeBSD__)
#define PRODUCT_NETDUMP_NAME PRODUCT_GENERIC_NAME_LOWER "-netdumper"
#else
#define PRODUCT_NETDUMP_NAME PRODUCT_VMVISOR_NAME " dump collector"
#endif

/*
 * VMware USB Arbitration Service version definitions
 */

#define PRODUCT_USB_ARBITRATOR_SHORT_NAME "vmware-usbarbitrator"
#define PRODUCT_USB_ARBITRATOR_NAME MAKE_NAME("USB Arbitration Service")
#ifdef _WIN32
#define PRODUCT_USB_ARBITRATOR_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "-usbarbitrator.exe"
#else
#define PRODUCT_USB_ARBITRATOR_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "-usbarbitrator"
#endif

// VMware Remote Console (VMRC)
#define PRODUCT_VMRC_UPPER "VMRC"
#define PRODUCT_VMRC_LOWER "vmrc"

/*
 * TODO: This properly lives in productState, but we need it here to
 * define DEFAULT_LIBDIRECTORY.  If that can be moved to productState,
 * it's no longer needed here.
 */
#define PRODUCT_MAC_DESKTOP_NAME MAKE_NAME(PRODUCT_MAC_DESKTOP_BRIEF_NAME)


#if !(   defined(VMX86_SERVER)   \
      || defined(VMX86_DESKTOP)  \
      || defined(VMX86_ENTERPRISE_DESKTOP) \
      || defined(VMX86_HORIZON_VIEW)     \
      || defined(VMX86_MUI)      \
      || defined(VMX86_VPX)      \
      || defined(VMX86_WBC)      \
      || defined(VMX86_SDK)      \
      || defined(VMX86_TOOLS)    \
      || defined(VMX86_V2V)      \
      || defined(VMX86_SYSIMAGE) \
      || defined(VMX86_VCB)      \
      || defined(VMX86_VMLS)     \
      || defined(VMX86_VLICENSE) \
      || defined(VMX86_P2V)      \
      || defined(VMX86_DDK)      \
      || defined(VMX86_VDDK)     \
      || defined(VMX86_NETDUMP) \
      || defined(VMX86_HBR_SERVER) \
      || defined(VMX86_VMCF) \
      || defined(VMX86_GANTRY) \
      || defined(VMX86_VMRC))
#   if defined(_WIN32) || defined(__APPLE__)
      /*
       * XXX Make the product be Workstation by default if none of the defines
       * XXX above are not defined in defs-globaldefs.mk -- Edward A. Waugh
       */
#      define VMX86_DESKTOP
#   else
#      error Unknown product
#   endif
#endif


#if defined(VMVISOR)
# define PRODUCT_SHORT_NAME PRODUCT_VMVISOR_NAME
#elif defined(VMX86_SERVER)
# define PRODUCT_SHORT_NAME PRODUCT_SCALABLE_SERVER_NAME
#elif defined(VMX86_CONSOLE)
# define PRODUCT_SHORT_NAME PRODUCT_CONSOLE_NAME
#elif defined(VMX86_MUI)
# define PRODUCT_SHORT_NAME PRODUCT_MUI_NAME
#elif defined(VMX86_VMRC) /* check VMX86_VMRC before VMX86_DESKTOP */
# define PRODUCT_SHORT_NAME PRODUCT_VMRC_NAME
#elif defined(VMX86_GANTRY)
# define PRODUCT_SHORT_NAME PRODUCT_GANTRY_NAME
#elif defined(VMX86_ENTERPRISE_DESKTOP)
# define PRODUCT_SHORT_NAME PRODUCT_WORKSTATION_ENTERPRISE_NAME
#elif defined(VMX86_DESKTOP)
# if defined(__APPLE__)
#  define PRODUCT_SHORT_NAME PRODUCT_MAC_DESKTOP_NAME
# else
#  define PRODUCT_SHORT_NAME PRODUCT_WORKSTATION_NAME
# endif
#elif defined(VMX86_TOOLS)
# define PRODUCT_SHORT_NAME VMWARE_TOOLS_SHORT_NAME
#elif defined(VMX86_VPX)
#  if defined(CSI_HA)
#     define PRODUCT_SHORT_NAME PRODUCT_HA_NAME
#  elif defined(CSI_FDM)
#     define PRODUCT_SHORT_NAME PRODUCT_FDM_NAME
#  elif defined(VPXA)
#     define PRODUCT_SHORT_NAME PRODUCT_VPXA_NAME
#  elif defined(XVP)
#     define PRODUCT_SHORT_NAME PRODUCT_XVP_NAME
#  elif defined(INSTALL_NGC)
#     define PRODUCT_SHORT_NAME PRODUCT_NGCINSTALLER_NAME
#  elif defined(INSTALL_SSO)
#     define PRODUCT_SHORT_NAME PRODUCT_SSOINSTALLER_NAME
#  elif defined(INSTALL_SSOREGMM)
#     define PRODUCT_SHORT_NAME PRODUCT_SSOREGMM_NAME
#  else
#     define PRODUCT_SHORT_NAME PRODUCT_VPX_NAME
#  endif
#elif defined(VMX86_WBC)
# define PRODUCT_SHORT_NAME PRODUCT_WBC_NAME
#elif defined(VMX86_SDK)
# define PRODUCT_SHORT_NAME PRODUCT_SDK_NAME
#elif defined(VMX86_P2V)
# define PRODUCT_SHORT_NAME PRODUCT_P2V_NAME
#elif defined(VMX86_V2V)
# define PRODUCT_SHORT_NAME PRODUCT_V2V_NAME
#elif defined(VMX86_SYSIMAGE)
# define PRODUCT_SHORT_NAME PRODUCT_SYSIMAGE_NAME
#elif defined(VMX86_VCB)
# define PRODUCT_SHORT_NAME PRODUCT_VCB_NAME
#elif defined(VMX86_VMLS)
# define PRODUCT_SHORT_NAME PRODUCT_VMLS_NAME
#elif defined(VMX86_VLICENSE)
# define PRODUCT_SHORT_NAME PRODUCT_VLICENSE_NAME
#elif defined(VMX86_DDK)
# define PRODUCT_SHORT_NAME PRODUCT_DDK_NAME
#elif defined(VMX86_VDDK)
# define PRODUCT_SHORT_NAME PRODUCT_VDDK_NAME
#elif defined(VMX86_NETDUMP)
# define PRODUCT_SHORT_NAME PRODUCT_NETDUMP_NAME
#elif defined(VMX86_HBR_SERVER)
# define PRODUCT_SHORT_NAME PRODUCT_HBR_SERVER_NAME
#elif defined(VMX86_HORIZON_VIEW)
# define PRODUCT_SHORT_NAME PRODUCT_VIEW_NAME
#elif defined(VMX86_VMCF)
# define PRODUCT_SHORT_NAME PRODUCT_VMCF_NAME
#endif


/*
 * Names of programs
 */

#if defined(VMX86_CONSOLE)
   #if defined(__linux__) || defined(__FreeBSD__)
   #define VMWARE_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "-console"
   #else
   #define VMWARE_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "Console.exe"
   #endif
#else
#define VMWARE_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER
#endif

#define VMWARE_VMX_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "-vmx"
#define CCAGENT_DISPLAY_NAME   PRODUCT_VPX_NAME " Agent"
#if defined(__linux__) || defined(__FreeBSD__)
#   define VMAUTHD_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "-authd"
#else
#   define VMAUTHD_DISPLAY_NAME   "VMware Authorization Service"
#   define VMSERVERD_DISPLAY_NAME "VMware Registration Service"
#   define VMNAT_DISPLAY_NAME     "VMware NAT Service"
#   define TOOLS_SERVICE_DISPLAY_NAME  "VMware Tools Service"
#   define TOOLS_SERVICE_NAME          "VMTools"
#   define VMAUTHD_SERVICE_NAME   "VMAuthdService"
#endif


/*
 * Configuration paths
 */

#ifndef _WIN32
#   define PRODUCT_NAME PRODUCT_SHORT_NAME
/*
 * Checked against the ProductID field of licenses.  This ensures that
 * a license intended for one flavor of the product will not allow
 * another flavor of the product to run.
 */
#   if defined(__APPLE__)
#      define PRODUCT_OS "Mac OS"
#   else
#      define PRODUCT_OS "Linux"
#   endif

/*
 * Note: changing PRODUCT_NAME_FOR_LICENSE and PRODUCT_LICENSE_VERSION
 * or macros it cleverly depends on (such as PRODUCT_NAME) requires a
 * coordinated dormant license file change. Otherwise licensing for
 * that product may break because the Licensecheck API is being passed
 * a parameter that no longer match the content of the dormant license
 * file.
 */
#   if defined(VMX86_SERVER)
#      define PRODUCT_NAME_FOR_LICENSE "VMware ESX Server"
#      define PRODUCT_SMP_NAME_FOR_LICENSE PRODUCT_ESX_SMP_NAME
#   elif defined(VMX86_VMRC) /* check VMX86_VMRC before VMX86_DESKTOP */
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_VMRC_NAME_FOR_LICENSE
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_GANTRY)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_GANTRY_NAME_FOR_LICENSE
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_DESKTOP)
#      if defined(__APPLE__)
#         define PRODUCT_NAME_FOR_LICENSE PRODUCT_MAC_DESKTOP_NAME_FOR_LICENSE
#      else
#         define PRODUCT_NAME_FOR_LICENSE "VMware Workstation"
#      endif
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_VPX)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME " Server"
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_SYSIMAGE)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_NETDUMP)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NETDUMP_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" //None
#   else   /* It is a product that doesn't use a license */
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   endif

/*
 * VMWARE_HOST_DIRECTORY is for host-specific configuration files.
 * DEFAULT_LIBDIRECTORY is the default for the 'libdir' config variable.
 *
 * The remote console checks at run time, and the MUI is not really a separate
 * product.
 */
#   if defined(__APPLE__)
#      if defined VMX86_GANTRY
#         define VMWARE_HOST_DIRECTORY_PREFIX \
             "/Library/Preferences/" PRODUCT_SHORT_NAME
#      elif defined VMX86_DESKTOP
#         define VMWARE_HOST_DIRECTORY_PREFIX \
             "/Library/Preferences/" PRODUCT_SHORT_NAME
#      else
#         define VMWARE_HOST_DIRECTORY_PREFIX \
             "/Library/Application Support/" PRODUCT_SHORT_NAME
#      endif
#   endif

#   if defined (VMX86_CONSOLE)
#      if defined(__APPLE__)
#         define VMWARE_HOST_DIRECTORY VMWARE_HOST_DIRECTORY_PREFIX " Console"
#      else
#         define VMWARE_HOST_DIRECTORY "/etc/" PRODUCT_GENERIC_NAME_LOWER "-console"
#         define DEFAULT_LIBDIRECTORY "/usr/lib/" PRODUCT_GENERIC_NAME_LOWER "-console"
#      endif
#   else
#      if defined(__APPLE__)
#         define VMWARE_HOST_DIRECTORY VMWARE_HOST_DIRECTORY_PREFIX
#      else
#         define VMWARE_HOST_DIRECTORY "/etc/" PRODUCT_GENERIC_NAME_LOWER
#         define DEFAULT_LIBDIRECTORY "/usr/lib/" PRODUCT_GENERIC_NAME_LOWER
#      endif
#   endif

#   if defined(__APPLE__)
#      if defined VMX86_DESKTOP
/* On Mac OS, use Location_Get() instead of DEFAULT_LIBDIRECTORY. */
#         define DEFAULT_LIBDIRECTORY \
             "/dev/null/Non-existing DEFAULT_LIBDIRECTORY"
#      else
#         define DEFAULT_LIBDIRECTORY VMWARE_HOST_DIRECTORY
#      endif
#   endif

/* For user-specific files. */
#   if defined(__APPLE__)
#      define VMWARE_USER_DIRECTORY "~/Library/Preferences/" PRODUCT_SHORT_NAME
#   else
#      define VMWARE_USER_DIRECTORY "~/." PRODUCT_GENERIC_NAME_LOWER
#   endif

#   define VMWARE_MODULE_NAME "/dev/vmmon"
#   define VMWARE_CONFIG PRODUCT_GENERIC_NAME_LOWER "-config.pl"
#   define VMWARE_CONNECT_SOCKET_DIRECTORY "/var/run/" PRODUCT_GENERIC_NAME_LOWER

#else

/* PRODUCT_SHORT_NAME and PRODUCT_FULL_NAME are used to display the name
   depending on how much space we have */
#   define PRODUCT_FULL_NAME PRODUCT_SHORT_NAME
#   define PRODUCT_NAME PRODUCT_FULL_NAME

/* Directory name in the registry */
#define PRODUCT_REG_NAME PRODUCT_NAME

/*
 * Checked against the ProductID field of licenses. This ensures that
 * a license intended for one flavor of the product will not allow
 * another flavor of the product to run.
 */
#   if defined(VMX86_VMRC) /* check VMX86_VMRC before VMX86_DESKTOP */
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_VMRC_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_FLEX) /* check VMX86_FLEX before VMX86_DESKTOP */
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_FLEX_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_DESKTOP)
#      if defined(__APPLE__)
#         define PRODUCT_NAME_FOR_LICENSE PRODUCT_MAC_DESKTOP_NAME_FOR_LICENSE
#      else
#         define PRODUCT_NAME_FOR_LICENSE "VMware Workstation"
#      endif
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_VPX)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME " Server"
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   else
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_REG_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   endif

#define PRIVATE_REG_KEY "Private"

#endif

/*
 * Used when referring to an unspecified member of the VMware product line
 * ex. "This file was created by an incompatible version of PRODUCT_LINE_NAME"
 */
#define PRODUCT_LINE_NAME PRODUCT_GENERIC_NAME " software"

#define PRODUCT_REG_PATH "SOFTWARE\\" COMPANY_NAME "\\" PRODUCT_REG_NAME
#define PRIVATE_REG_PATH PRODUCT_REG_PATH "\\" PRIVATE_REG_KEY

/*
 * Defines used primarily in Tools, but perhaps useful elsewhere.  Only error
 * on unrecognized platform during Tools builds.  Note that NetWare must come
 * before linux below since it uses the Linux gcc which automatically defines
 * linux; the other platforms don't have this issue.
 */
#ifdef N_PLAT_NLM
#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for NetWare"
#elif defined(linux)
#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for Linux"
#elif defined(_WIN32)
#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for Windows"
#elif defined(__FreeBSD__)
#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for FreeBSD"
#elif defined(sun)
#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for Solaris"
#elif defined(__APPLE__)
#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for Mac OS X"
#elif defined __ANDROID__
#  define PRODUCT_NAME_PLATFORM         PRODUCT_NAME " for Android"
#else
#  ifdef VMX86_TOOLS
#    error "Define a product string for this platform."
#  endif
#endif


/*
 * This is for ACE Management Server
 * Since there is no separate product defined for Ace Mgmt Server
 * (i.e. PRODUCT=xxx when running makefile), we can not used the
 * generic PRODUCT_NAME_STRING_FOR_LICENSE definition.
 * As a result, the specific ACE_MGMT_SERVER_PRODUCT_NAME_FOR_LICENSE
 * is used instead.
 * A similar reason is used also for the PRODUCT_VERSION_STRING_FOR_LICENSE
 * definition in the vm_version.h
 */

#define ACE_MGMT_SERVER_PRODUCT_NAME_FOR_LICENSE      "VMware ACE Management Server"

/*
 * For Host Agent (hostd)
 */

#define HOST_AGENT_PRODUCT_NAME     PRODUCT_NAME " Host Agent"

/* Used by bora/vim/lib/vmgina module.
 * @todo Use this also in /bora/install/msi/InstUtil/desktop/vmInstUtil.cpp
 *       to guarantee that the service is installed with exactly this name.
 */
#define HOST_AGENT_SERVICE_NAME     "VMwareHostd"

// Name of the environment variable that controls which proxy server to use
// while opening connections to vCenter and vSphere servers. Currently vCloud
// VMRC uses it.
#define VMWARE_HTTPSPROXY  "VMWARE_HTTPSPROXY"

#endif /* VM_PRODUCT_H */
