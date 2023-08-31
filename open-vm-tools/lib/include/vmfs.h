/*********************************************************
 * Copyright (c) 2003-2021 VMware, Inc. All rights reserved.
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
 * vmfs.h --
 *
 *	assorted vmfs related helper functions needed by userlevel.
 */

#ifndef __VMFS_H__
#define __VMFS_H__

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "unicodeTypes.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Consolidate all path stuff here so it's consistent between user and kernel
#define VMFS_ROOT_DIR_NAME              "vmfs"
#define VMFS_ROOT_DIR_PATH              "/vmfs/"
                                                                                
#define DEVFS_DIR_NAME                  "devices"
#define VCFS_DIR_NAME                   "volumes"
#define DELTADISK_DIR_NAME		"deltadisks"
#define CBT_DIR_NAME		        "cbt"
#define MULTI_EXTENT_DIR_NAME		"multiextent"
#define FILE_DIR_NAME                   "file"
#define RAMDISK_DIR_NAME                "ramdisk"
#define SVM_DIR_NAME                    "svm"
#define VFLASH_DIR_NAME                 "vflash"
#define VDFM_DIR_NAME                   "vdfm"

#define DEVFS_MOUNT_POINT               VMFS_ROOT_DIR_PATH DEVFS_DIR_NAME
#define VCFS_MOUNT_POINT                VMFS_ROOT_DIR_PATH VCFS_DIR_NAME

#define DEVFS_MOUNT_PATH                DEVFS_MOUNT_POINT "/"
#define VCFS_MOUNT_PATH                 VCFS_MOUNT_POINT "/"

#define DELTADISK_MOUNT_POINT	        DEVFS_MOUNT_PATH DELTADISK_DIR_NAME
#define DELTADISK_MOUNT_PATH		DELTADISK_MOUNT_POINT "/"

#define CBT_MOUNT_POINT	                DEVFS_MOUNT_PATH CBT_DIR_NAME
#define CBT_MOUNT_PATH		        CBT_MOUNT_POINT "/"

#define FILE_MOUNT_POINT	        DEVFS_MOUNT_PATH FILE_DIR_NAME
#define FILE_MOUNT_PATH		        FILE_MOUNT_POINT "/"

#define RAMDISK_MOUNT_POINT	        DEVFS_MOUNT_PATH RAMDISK_DIR_NAME
#define RAMDISK_MOUNT_PATH		RAMDISK_MOUNT_POINT "/"

#define SVM_MOUNT_POINT	                DEVFS_MOUNT_PATH SVM_DIR_NAME
#define SVM_MOUNT_PATH		        SVM_MOUNT_POINT "/"

#define VFLASH_MOUNT_POINT              DEVFS_MOUNT_PATH VFLASH_DIR_NAME
#define VFLASH_MOUNT_PATH               VFLASH_MOUNT_POINT "/"

#define VDFM_MOUNT_POINT                DEVFS_MOUNT_PATH VDFM_DIR_NAME
#define VDFM_MOUNT_PATH                 VDFM_MOUNT_POINT "/"

#define CDROM_DRIVER_STRING             "cdrom"
#define PSA_STOR_DISK_DRIVER_STRING     "disks"
#define SCSI_GENERIC_DRIVER_STRING      "genscsi"
#define OLD_SCSI_GENERIC_DRIVER_STRING  "generic"
#define COW_DRIVER_NAME                 "deltadisks"
#define MULTI_EXTENT_DRIVER_NAME        "multiextent"

#define FDS_DRIVER_ALL_STRING           "fdsall"

#define CDROM_MOUNT_POINT               DEVFS_MOUNT_PATH CDROM_DRIVER_STRING
#define DISKS_MOUNT_POINT               DEVFS_MOUNT_PATH PSA_STOR_DISK_DRIVER_STRING
#define GENERIC_SCSI_MOUNT_POINT        DEVFS_MOUNT_PATH SCSI_GENERIC_DRIVER_STRING
#define MULTI_EXTENT_MOUNT_POINT	DEVFS_MOUNT_PATH MULTI_EXTENT_DIR_NAME
#define CDROM_MOUNT_PATH                CDROM_MOUNT_POINT "/"
#define DISKS_MOUNT_PATH                DISKS_MOUNT_POINT "/"
#define GENERIC_SCSI_MOUNT_PATH         GENERIC_SCSI_MOUNT_POINT "/"
#define MULTI_EXTENT_MOUNT_PATH		MULTI_EXTENT_MOUNT_POINT "/"

#define VISOR_DEVFS_MOUNT_PATH          "/dev/"
#define VISOR_CDROM_MOUNT_POINT         VISOR_DEVFS_MOUNT_PATH CDROM_DRIVER_STRING
#define VISOR_DISKS_MOUNT_POINT         VISOR_DEVFS_MOUNT_PATH PSA_STOR_DISK_DRIVER_STRING
#define VISOR_GENERIC_SCSI_MOUNT_POINT  VISOR_DEVFS_MOUNT_PATH SCSI_GENERIC_DRIVER_STRING
#define VISOR_CDROM_MOUNT_PATH          VISOR_CDROM_MOUNT_POINT "/"
#define VISOR_DISKS_MOUNT_PATH          VISOR_DISKS_MOUNT_POINT "/"
#define VISOR_GENERIC_SCSI_MOUNT_PATH   VISOR_GENERIC_SCSI_MOUNT_POINT "/"

typedef enum {
   VMFS_SYMBOLIC,
   VMFS_SCSI_DEV,
   VMFS_COS_SYMBOLIC,
   VMFS_COS_SCSI_DEV,
} Vmfs_VolNameType;

#if defined(VMX86_SERVER)
char *Vmfs_GetCOSFileName(const char *vmfsFile);
#endif /* VM86_SERVER */

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* __VMFS_H__ */

