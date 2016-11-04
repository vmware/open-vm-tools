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

/*
 * deployPkgFormat.h --
 *
 *      A deployment package format used primarily to
 *      upload and install software in a guest OS.
 *
 *      The package can be a file, or a section embedded inside
 *      of another file or raw block device.
 */

#ifndef _DEPLOY_PKG_FORMAT_H_
#define _DEPLOY_PKG_FORMAT_H_

#define VMWAREDEPLOYPKG_SIGNATURE_LENGTH 16
#define VMWAREDEPLOYPKG_SIGNATURE "VMWAREDEPLOYPKG_"

#define VMWAREDEPLOYPKG_CMD_LENGTH 456
#define VMWAREDEPLOYPKG_SEED_LENGTH 8

#define VMWAREDEPLOYPKG_PAYLOAD_TYPE_CAB 0         // cabinet file
#define VMWAREDEPLOYPKG_PAYLOAD_TYPE_ZIP 1         // zip
#define VMWAREDEPLOYPKG_PAYLOAD_TYPE_GZIPPED_TAR 2 // tar.gz

// XXX Delete this - it's redundant
typedef enum {
   Cabinet,                // cabinet file
   Zip,
   GzippedTar              // tar.gz
} VMwareDeployPkgPayloadType;

#define VMWAREDEPLOYPKG_CURRENT_MAJOR_VERSION 1
#define VMWAREDEPLOYPKG_CURRENT_MINOR_VERSION 0

#include "vm_basic_types.h"

#define VMWAREDEPLOYPKG_HEADER_FLAGS_NONE 0
#define VMWAREDEPLOYPKG_HEADER_FLAGS_SKIP_REBOOT 1

#ifdef _WIN32
#include "pshpack4.h" // 4 byte alignment assumed.
#endif

/*
 * VMware deployment package header. 4 byte alignment assumed.
 * The header size is exactly 512 bytes to make it easier to
 * embed in a disk device, such as a partition.
 *
 * The payload is extracted and expanded into a temporary folder.
 * During expansion, original relative path names are preserved.
 * The specified command is then executed on the host.
 * Its working directory is the extraction folder.
 * The command string may contain OS-specific environment
 * variables.
 * In addition, the variable VMWAREPKGDIR is defined to be
 * the location of the extraction folder.
 *
 * (Request for comment: is VMWAREPKGDIR really necessary?
 *  Remove from spec if not.)
 *
 * The field seed is used by the password obfuscation library to hide details
 * that are required for obfuscating password in the config file.
 *
 * Command string example:
 * deploy.bat -opt1 myfile.exe foo.xml "%WINDIR%\system32"
 *
 * Seed is a piece of information used by the obfuscation code to compute
 * cryptography keys.
 *
 * The extraction folder is deleted after the command returns.
 * A return value of zero indicates successful deployment.
 *
 * Here is the approximate layout. Do not make assumptions about the 
 * exact location and relative position of the individual sections.
 * Use the offset and length fields from the header instead.
 *
 * <pre>
 *
 *         +-------------------------+
 *         |         header          |
 *         +-------------------------+
 *         |         padding         |
 *         +-------------------------+
 *         |        payload          |
 *         +-------------------------+
 *         |      (seed+command)     |
 *         |         padding         |
 *         +-------------------------+      
 * </pre>
 */

typedef struct
{
   char signature[VMWAREDEPLOYPKG_SIGNATURE_LENGTH]; // Not null terminated.
   uint8 majorVersion;
   uint8 minorVersion;
   uint8 payloadType;
   uint8 reserved;

   /*
    * Structs are aligned to word length. For 32 bit architecture it is 4 bytes
    * aligned and for 64 bit it is 8 bytes aligned. To make sure 64 bit field
    * "pkgLength" starts at the same place in both 32 and 64 bit architecture,
    * we need to add a 4 byte padding. If the padding is not present, package
    * created in 32 bit architecture will not be read correctly in 64 bit
    * architecture and vice-versa.
    *
    * NOTE: Positioning and size of the pad will change if fields are
    * added/removed.
    */
   uint32 archPadding;

   uint64 pkgLength;     // Total length of package including header, offset 20.
   uint64 payloadOffset; // Relative to beginning of header, offset 28.
   uint64 payloadLength; // Length of payload, offset 36.

   /*
    * Command string and padding, null terminated, offset 44.
    * This padding makes the header sector-aligned, making it easier
    * to embed in disks and disk partitions.
    * This string may contain OS-specific env variables, e.g. %SYSTEMDRIVE%.
    */

   char seed[VMWAREDEPLOYPKG_SEED_LENGTH];
   char command[VMWAREDEPLOYPKG_CMD_LENGTH];

} VMwareDeployPkgHdr;

#ifdef _WIN32
#include "poppack.h"
#endif

#endif // _DEPLOY_PKG_FORMAT_H_
