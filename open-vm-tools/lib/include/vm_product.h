/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


#ifndef VM_PRODUCT_H
#define VM_PRODUCT_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
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
#define PRODUCT_SCALABLE_SERVER_BRIEF_NAME "ESX Server"
#define PRODUCT_WGS_BRIEF_NAME "Server"
#define PRODUCT_GSX_BRIEF_NAME "GSX Server"
#define PRODUCT_WORKSTATION_BRIEF_NAME "Workstation"
#define PRODUCT_WORKSTATION_ENTERPRISE_BRIEF_NAME \
         PRODUCT_WORKSTATION_BRIEF_NAME " " "ACE Edition"
#define PRODUCT_PLAYER_BRIEF_NAME "Player"
#define PRODUCT_MAC_DESKTOP_BRIEF_NAME "Fusion"
#define PRODUCT_ACE_MANAGEMENT_SERVER_BRIEF_NAME "ACE Management Server"


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
#define PRODUCT_WGS_NAME MAKE_NAME(PRODUCT_WGS_BRIEF_NAME)
#define PRODUCT_GSX_NAME MAKE_NAME(PRODUCT_GSX_BRIEF_NAME)
#define PRODUCT_GSX_SMP_NAME MAKE_NAME("Virtual SMP for GSX Server")
#define PRODUCT_WORKSTATION_NAME MAKE_NAME(PRODUCT_WORKSTATION_BRIEF_NAME)
#define PRODUCT_WORKSTATION_ENTERPRISE_NAME MAKE_NAME(PRODUCT_WORKSTATION_ENTERPRISE_BRIEF_NAME)
#define PRODUCT_MUI_NAME MAKE_NAME("Management Interface")
#define PRODUCT_CONSOLE_NAME MAKE_NAME("Server Console")
#define PRODUCT_PLAYER_NAME MAKE_NAME(PRODUCT_PLAYER_BRIEF_NAME)
#define PRODUCT_PLAYER_NAME_FOR_LICENSE PRODUCT_PLAYER_NAME
#define PRODUCT_ACE_MANAGEMENT_SERVER_NAME MAKE_NAME(PRODUCT_ACE_MANAGEMENT_SERVER_BRIEF_NAME)

#define PRODUCT_VMLS_SHORT_NAME "VMLS"
#define PRODUCT_VMLS_NAME MAKE_NAME("License Server")

#define PRODUCT_LICENSE_SHORT_NAME "LICENSE"
#define PRODUCT_LICENSE_NAME MAKE_NAME("License Infrastructure")

#define PRODUCT_P2V_SHORT_NAME "P2V"
#define PRODUCT_P2V_NAME MAKE_NAME("P2V Assistant")

#define PRODUCT_V2V_SHORT_NAME "V2V"
#define PRODUCT_V2V_NAME MAKE_NAME("Virtual Machine Importer")

#define PRODUCT_SYSIMAGE_SHORT_NAME "SysImage"
#define PRODUCT_SYSIMAGE_NAME MAKE_NAME("System Image Framework")

#define PRODUCT_VCB_SHORT_NAME "VCB"
#define PRODUCT_VCB_NAME MAKE_NAME("Consolidated Backup")

#define PRODUCT_API_SCRIPTING_COM_SHORT_NAME "VmCOM"
#define PRODUCT_API_SCRIPTING_PERL_SHORT_NAME "VmPerl"

#define PRODUCT_API_SCRIPTING_COM_NAME MAKE_NAME(PRODUCT_API_SCRIPTING_COM_SHORT_NAME " Scripting API")

#define PRODUCT_API_SCRIPTING_PERL_NAME MAKE_NAME(PRODUCT_API_SCRIPTING_PERL_SHORT_NAME " Scripting API")

#define PRODUCT_VPX_NAME MAKE_NAME("VirtualCenter")

#define PRODUCT_WBC_NAME MAKE_NAME("WebCenter")

#define PRODUCT_SDK_NAME MAKE_NAME("SDK")

#define PRODUCT_DDK_NAME MAKE_NAME("ESX DDK")

// XXX VMvisor is the underlying technology for possibly several products,
// XXX not the product. Fix when names are decided.
#define PRODUCT_VMVISOR_NAME MAKE_NAME("VMvisor")

#define PRODUCT_VMRC_SHORT_NAME "VMRC"
#define PRODUCT_VMRC_NAME MAKE_NAME("Virtual Machine Remote Console")
#define PRODUCT_VMRC_PLUGIN_NAME PRODUCT_VMRC_NAME " Plug-in"
#define PRODUCT_VMRC_DESCRIPTION "Enables remote interaction with virtual machines."
#ifdef _WIN32
#define PRODUCT_VMRC_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "-vmrc.exe"
#else
#define PRODUCT_VMRC_EXECUTABLE PRODUCT_GENERIC_NAME_LOWER "-vmrc"
#endif

/*
 * TODO: This properly lives in productState, but we need it here to
 * define DEFAULT_LIBDIRECTORY.  If that can be moved to productState,
 * it's no longer needed here.
 */
#define PRODUCT_MAC_DESKTOP_NAME MAKE_NAME(PRODUCT_MAC_DESKTOP_BRIEF_NAME)


#if !(   defined(VMX86_SERVER)   \
      || defined(VMX86_WGS)      \
      || defined(VMX86_DESKTOP)  \
      || defined(VMX86_ENTERPRISE_DESKTOP) \
      || defined(VMX86_MUI)  	   \
      || defined(VMX86_API)      \
      || defined(VMX86_VPX)      \
      || defined(VMX86_WBC)      \
      || defined(VMX86_SDK)      \
      || defined(VMX86_TOOLS)    \
      || defined(VMX86_V2V)      \
      || defined(VMX86_SYSIMAGE) \
      || defined(VMX86_VCB)      \
      || defined(VMX86_VMLS)     \
      || defined(VMX86_LICENSE)  \
      || defined(VMX86_P2V)      \
      || defined(VMX86_DDK))
#   if defined(_WIN32)
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
#elif defined(VMX86_WGS_MIGRATION)
# define PRODUCT_SHORT_NAME PRODUCT_WGS_MIGRATION_NAME
#elif defined(VMX86_WGS)
#  if defined(VMX86_CONSOLE)
#     define PRODUCT_SHORT_NAME PRODUCT_CONSOLE_NAME
#  else
#     define PRODUCT_SHORT_NAME PRODUCT_WGS_NAME
#  endif
#elif defined(VMX86_MUI)
# define PRODUCT_SHORT_NAME PRODUCT_MUI_NAME
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
#elif defined(VMX86_API)
#  ifdef _WIN32
#     define PRODUCT_SHORT_NAME PRODUCT_API_SCRIPTING_COM_NAME
#  else
#     define PRODUCT_SHORT_NAME PRODUCT_API_SCRIPTING_PERL_NAME
#  endif
#elif defined(VMX86_VPX)
# define PRODUCT_SHORT_NAME PRODUCT_VPX_NAME
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
#elif defined(VMX86_LICENSE)
# define PRODUCT_SHORT_NAME PRODUCT_LICENSE_NAME
#elif defined(VMX86_DDK)
# define PRODUCT_SHORT_NAME PRODUCT_DDK_NAME
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

#   if defined(VMX86_SERVER)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE PRODUCT_ESX_SMP_NAME
#   elif defined(VMX86_DESKTOP)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME " for " PRODUCT_OS
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_WGS_MIGRATION)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME " for " PRODUCT_OS
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_WGS)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_GSX_NAME " for " PRODUCT_OS
#      define PRODUCT_SMP_NAME_FOR_LICENSE PRODUCT_GSX_SMP_NAME " for " PRODUCT_OS
#   elif defined(VMX86_SYSIMAGE)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   else   /* It is a product that doesn't use a license */
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   endif

/* Default for the 'libdir' config variable */
/*
 * The APIs are installed as separate products and must have their own
 * configuration and library directories.  The remote console checks at
 * run time, and the MUI is not really a separate product.
 */
#   if defined(__APPLE__)
#      define VMWARE_HOST_DIRECTORY_PREFIX \
          "/Library/Application Support/" PRODUCT_SHORT_NAME
#   endif

#   if defined(VMX86_API)
#      if defined(__APPLE__)
#         define VMWARE_HOST_DIRECTORY VMWARE_HOST_DIRECTORY_PREFIX " API"
#      else
#         define VMWARE_HOST_DIRECTORY "/etc/" PRODUCT_GENERIC_NAME_LOWER "-api"
#         define DEFAULT_LIBDIRECTORY "/usr/lib/" PRODUCT_GENERIC_NAME_LOWER "-api"
#      endif
#   elif defined (VMX86_CONSOLE)
#      if defined(__APPLE__)
#         define VMWARE_HOST_DIRECTORY VMWARE_HOST_DIRECTORY_PREFIX " Console"
#      else
#         define VMWARE_HOST_DIRECTORY "/etc/" PRODUCT_GENERIC_NAME_LOWER "-console"
#         define DEFAULT_LIBDIRECTORY "/usr/lib/" PRODUCT_GENERIC_NAME_LOWER "-console"
#      endif
#   else
#      if defined(__APPLE__)
#         define VMWARE_HOST_DIRECTORY VMWARE_HOST_DIRECTORY_PREFIX
#      elif defined(__ACESC_LICENSE__)
/*
 * This definition (__ACESC_LICENSE__) is used by the acesc licensing.
 * The licensing API uses VMWARE_HOST_DIRECTORY definition to save the activated
 * license (as well as searching).
 * In our case, the ACESC will use this customized directory, instead of the common
 * '/etc/vmware'.
 * The main motivation for this is that the acesc configuration application is
 * a web application (cgi app). Using the common directory, the apache process
 * does not have enough permission to write into the directory (/etc/vmware).
 * Instead of making the /etc/vmware writeable by everybody, we just create another
 * subdirectory (/etc/vmware/acesc).
 */
#         define VMWARE_HOST_DIRECTORY "/etc/vmware/acesc"
#         define DEFAULT_LIBDIRECTORY "/usr/lib/" PRODUCT_GENERIC_NAME_LOWER
#      else
#         define VMWARE_HOST_DIRECTORY "/etc/" PRODUCT_GENERIC_NAME_LOWER
#         define DEFAULT_LIBDIRECTORY "/usr/lib/" PRODUCT_GENERIC_NAME_LOWER
#      endif
#   endif

#   if defined(__APPLE__)
/*
 * Mac OS hosts don't distinguish between an /etc and /usr equivalent,
 * so put both in DEFAULT_LIBDIRECTORY.  Note: If there are filename
 * clashes, we'll need to distinguish the two.
 */
#      define DEFAULT_LIBDIRECTORY VMWARE_HOST_DIRECTORY
#   endif

/* For host specific files */
#   if defined(__APPLE__)
#      define VMWARE_USER_SUBDIRECTORY "Library/Preferences/" PRODUCT_SHORT_NAME
#   else
#      define VMWARE_USER_SUBDIRECTORY "." PRODUCT_GENERIC_NAME_LOWER
#   endif
/* For user specific files */
#   define VMWARE_USER_DIRECTORY "~/" VMWARE_USER_SUBDIRECTORY
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
#   if defined(VMX86_DESKTOP)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME " for Win32"
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_WGS_MIGRATION)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_NAME " for Win32"
#      define PRODUCT_SMP_NAME_FOR_LICENSE "" // None
#   elif defined(VMX86_WGS)
#      define PRODUCT_NAME_FOR_LICENSE PRODUCT_GSX_NAME " for Win32"
#      define PRODUCT_SMP_NAME_FOR_LICENSE PRODUCT_GSX_SMP_NAME " for Win32"
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

#define HOST_AGENT_PRODUCT_NAME		PRODUCT_NAME " Host Agent"

/* Used by bora/vim/lib/vmgina module.
 * @todo Use this also in /bora/install/msi/InstUtil/desktop/vmInstUtil.cpp
 *       to guarantee that the service is installed with exactly this name.
 */
#define HOST_AGENT_SERVICE_NAME		"VMwareHostd"

#endif /* VM_PRODUCT_H */
