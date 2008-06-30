/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * guestInfo.c ---
 *
 *	Provides interface to information about the guest, such as hostname,
 *	NIC/IP address information, etc.
 */


#include "guestInfoInt.h"


/*
 * Global functions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfo_GetFqdn --
 *
 *      Returns the guest's hostname.
 *
 * Results:
 *      Returns TRUE on success, FALSE on failure.
 *      Returns the guest's fully qualified domain name in fqdn.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfo_GetFqdn(int outBufLen,        // IN: sizeof fqdn
                  char fqdn[])          // OUT: buffer to store hostname
{
   return GuestInfoGetFqdn(outBufLen, fqdn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfo_GetNicInfo --
 *
 *      Returns the guest's hostname.
 *
 * Results:
 *      Return MAC addresses of all the NICs in the guest and their
 *      corresponding IP addresses.
 *
 *      Returns TRUE on success and FALSE on failure.
 *      Return MAC addresses of all NICs and their corresponding IPs.
 *
 * Side effects:
 *      Memory is allocated for each NIC, as well as IP addresses of all NICs
 *      on successful return.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfo_GetNicInfo(GuestNicList *nicInfo)  // OUT: storage for NIC information
{
   return GuestInfoGetNicInfo(nicInfo);
}


/*
 *----------------------------------------------------------------------
 *
 * GuestInfo_GetDiskInfo --
 *
 *      Get disk information.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *	Allocates memory for di->partitionList.
 *
 *----------------------------------------------------------------------
 */

Bool
GuestInfo_GetDiskInfo(PGuestDiskInfo di)     // IN/OUT
{
   return GuestInfoGetDiskInfo(di);
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestInfoGetOSName --
 *
 *      Return OS version information.
 *
 *      osFullName format:
 *        Linux/POSIX:
 *          <OS NAME> <OS RELEASE> <SPECIFIC_DISTRO_INFO>
 *        An example of such string would be:
 *          Linux 2.4.18-3 Red Hat Linux release 7.3 (Valhalla)
 *
 *        Win32:
 *          <OS NAME> <SERVICE PACK> (BUILD <BUILD_NUMBER>)
 *        An example of such string would be:
 *          Windows XP Professional Service Pack 2 (Build 2600)
 *
 *      osName contains an os name in the same format that is used
 *      in .vmx file.
 *
 * Results:
 *      Returns TRUE on success and FALSE on failure.
 *      Returns the guest's full OS name (osNameFull)
 *      Returns the guest's OS name in the same format as .vmx file (osName)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestInfo_GetOSName(unsigned int outBufFullLen, // IN
                    unsigned int outBufLen,     // IN
                    char *osNameFull,           // OUT
                    char *osName)               // OUT
{
   return GuestInfoGetOSName(outBufFullLen, outBufLen, osNameFull, osName);
}
