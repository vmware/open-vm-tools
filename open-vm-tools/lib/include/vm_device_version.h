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

#ifndef VM_DEVICE_VERSION_H
#define VM_DEVICE_VERSION_H

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#ifdef _WIN32
#ifdef __MINGW32__
#include "initguid.h"
#else
#include "guiddef.h"
#endif
#endif

/* LSILogic 53C1030 Parallel SCSI controller
 * LSILogic SAS1068 SAS controller
 */
#define PCI_VENDOR_ID_LSILOGIC          0x1000
#define PCI_DEVICE_ID_LSI53C1030        0x0030
#define PCI_DEVICE_ID_LSISAS1068        0x0054

/* Our own PCI IDs
 *    VMware SVGA II (Unified VGA)
 *    VMware SVGA (PCI Accelerator)
 *    VMware vmxnet (Idealized NIC)
 *    VMware vmxscsi (Abortive idealized SCSI controller)
 *    VMware chipset (Subsystem ID for our motherboards)
 *    VMware e1000 (Subsystem ID)
 *    VMware vmxnet3 (Uniform Pass Through NIC)
 *    VMware HD Audio codec
 *    VMware HD Audio controller
 */
#define PCI_VENDOR_ID_VMWARE                    0x15AD
#define PCI_DEVICE_ID_VMWARE_SVGA2              0x0405
#define PCI_DEVICE_ID_VMWARE_SVGA               0x0710
#define PCI_DEVICE_ID_VMWARE_NET                0x0720
#define PCI_DEVICE_ID_VMWARE_SCSI               0x0730
#define PCI_DEVICE_ID_VMWARE_VMCI               0x0740
#define PCI_DEVICE_ID_VMWARE_CHIPSET            0x1976
#define PCI_DEVICE_ID_VMWARE_82545EM            0x0750 /* single port */
#define PCI_DEVICE_ID_VMWARE_82546EB            0x0760 /* dual port   */
#define PCI_DEVICE_ID_VMWARE_EHCI               0x0770
#define PCI_DEVICE_ID_VMWARE_UHCI               0x0774
#define PCI_DEVICE_ID_VMWARE_XHCI               0x0778
#define PCI_DEVICE_ID_VMWARE_1394               0x0780
#define PCI_DEVICE_ID_VMWARE_BRIDGE             0x0790
#define PCI_DEVICE_ID_VMWARE_ROOTPORT           0x07A0
#define PCI_DEVICE_ID_VMWARE_VMXNET3            0x07B0
#define PCI_DEVICE_ID_VMWARE_VMXWIFI            0x07B8
#define PCI_DEVICE_ID_VMWARE_PVSCSI             0x07C0
#define PCI_DEVICE_ID_VMWARE_82574              0x07D0
#define PCI_DEVICE_ID_VMWARE_HDAUDIO_CODEC      0x1975
#define PCI_DEVICE_ID_VMWARE_HDAUDIO_CONTROLLER 0x1977

/* The hypervisor device might grow.  Please leave room
 * for 7 more subfunctions.
 */
#define PCI_DEVICE_ID_VMWARE_HYPER      0x0800
#define PCI_DEVICE_ID_VMWARE_VMI        0x0801

#define PCI_DEVICE_VMI_CLASS            0x05
#define PCI_DEVICE_VMI_SUBCLASS         0x80
#define PCI_DEVICE_VMI_INTERFACE        0x00
#define PCI_DEVICE_VMI_REVISION         0x01

/* From linux/pci_ids.h:
 *   AMD Lance Ethernet controller
 *   BusLogic SCSI controller
 *   Ensoniq ES1371 sound controller
 */
#define PCI_VENDOR_ID_AMD               0x1022
#define PCI_DEVICE_ID_AMD_VLANCE        0x2000
#define PCI_VENDOR_ID_BUSLOGIC			0x104B
#define PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC	0x0140
#define PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER	0x1040
#define PCI_VENDOR_ID_ENSONIQ           0x1274
#define PCI_DEVICE_ID_ENSONIQ_ES1371    0x1371

/* From linux/pci_ids.h:
 *    Intel 82439TX (430 HX North Bridge)
 *    Intel 82371AB (PIIX4 South Bridge)
 *    Intel 82443BX (440 BX North Bridge and AGP Bridge)
 *    Intel 82545EM (e1000, server adapter, single port)
 *    Intel 82546EB (e1000, server adapter, dual port)
 *    Intel HECI (as embedded in ich9m)
 */
#define PCI_VENDOR_ID_INTEL             0x8086
#define PCI_DEVICE_ID_INTEL_82439TX     0x7100
#define PCI_DEVICE_ID_INTEL_82371AB_0   0x7110
#define PCI_DEVICE_ID_INTEL_82371AB_2   0x7112
#define PCI_DEVICE_ID_INTEL_82371AB_3   0x7113
#define PCI_DEVICE_ID_INTEL_82371AB     0x7111
#define PCI_DEVICE_ID_INTEL_82443BX     0x7190
#define PCI_DEVICE_ID_INTEL_82443BX_1   0x7191
#define PCI_DEVICE_ID_INTEL_82443BX_2   0x7192 /* Used when no AGP support */
#define PCI_DEVICE_ID_INTEL_82545EM     0x100f
#define PCI_DEVICE_ID_INTEL_82546EB     0x1010
#define PCI_DEVICE_ID_INTEL_82574       0x10d3
#define PCI_DEVICE_ID_INTEL_82574_APPLE 0x10f6
#define PCI_DEVICE_ID_INTEL_HECI        0x2a74

#define E1000E_PCI_DEVICE_ID_CONFIG_STR "e1000e.pci.deviceID"
#define E1000E_PCI_SUB_VENDOR_ID_CONFIG_STR "e1000e.pci.subVendorID"
#define E1000E_PCI_SUB_DEVICE_ID_CONFIG_STR "e1000e.pci.subDeviceID"

/*
 * Intel HD Audio controller and Realtek ALC885 codec.
 */
#define PCI_DEVICE_ID_INTEL_631XESB_632XESB  0x269a
#define PCI_VENDOR_ID_REALTEK                0x10ec
#define PCI_DEVICE_ID_REALTEK_ALC885         0x0885


/*
 * Fresco Logic xHCI (USB 3.0) Controller
 */
#define PCI_VENDOR_ID_FRESCO            0x1B73
#define PCI_DEVICE_ID_FRESCO_FL1000     0x1000   // Original 1-port chip
#define PCI_DEVICE_ID_FRESCO_FL1009     0x1009   // New 2-port chip (Driver 3.0.98+)
#define PCI_DEVICE_ID_FRESCO_FL1400     0x1400   // Unknown (4-port? Dev hardware?)

/*
 * NEC/Renesas xHCI (USB 3.0) Controller
 */
#define PCI_VENDOR_ID_NEC               0x1033
#define PCI_DEVICE_ID_NEC_UPD720200     0x0194
#define PCI_REVISION_NEC_UPD720200      0x03
#define PCI_FIRMWARE_NEC_UPD720200      0x3015


/************* Strings for IDE Identity Fields **************************/
#define VIDE_ID_SERIAL_STR	"00000000000000000001"	/* Must be 20 Bytes */
#define VIDE_ID_FIRMWARE_STR	"00000001"		/* Must be 8 Bytes */

/* No longer than 40 Bytes */
#define VIDE_ATA_MODEL_STR PRODUCT_GENERIC_NAME " Virtual IDE Hard Drive"
#define VIDE_ATAPI_MODEL_STR PRODUCT_GENERIC_NAME " Virtual IDE CDROM Drive"

#define ATAPI_VENDOR_ID	"NECVMWar"		/* Must be 8 Bytes */
#define ATAPI_PRODUCT_ID PRODUCT_GENERIC_NAME " IDE CDROM"	/* Must be 16 Bytes */
#define ATAPI_REV_LEVEL	"1.00"			/* Must be 4 Bytes */

#define IDE_NUM_INTERFACES   2	/* support for two interfaces */
#define IDE_DRIVES_PER_IF    2

/************* Strings for SCSI Identity Fields **************************/
#define SCSI_DISK_MODEL_STR PRODUCT_GENERIC_NAME " Virtual SCSI Hard Drive"
#define SCSI_DISK_VENDOR_NAME COMPANY_NAME
#define SCSI_DISK_REV_LEVEL "1.0"
#define SCSI_CDROM_MODEL_STR PRODUCT_GENERIC_NAME " Virtual SCSI CDROM Drive"
#define SCSI_CDROM_VENDOR_NAME COMPANY_NAME
#define SCSI_CDROM_REV_LEVEL "1.0"

/************* SCSI implementation limits ********************************/
#define SCSI_MAX_CONTROLLERS	 4	  // Need more than 1 for MSCS clustering
#define	SCSI_MAX_DEVICES	 16	  // BT-958 emulates only 16
#define PVSCSI_MAX_DEVICES       255      // 255 (including the controller)
/*
 * VSCSI_BV_INTS is the number of uint32's needed for a bit vector 
 * to cover all scsi devices per target.
 */
#define VSCSI_BV_INTS            CEILING(PVSCSI_MAX_DEVICES, 8 * sizeof (uint32))
#define SCSI_IDE_CHANNEL         SCSI_MAX_CONTROLLERS
#define SCSI_IDE_HOSTED_CHANNEL  (SCSI_MAX_CONTROLLERS + 1)
#define SCSI_MAX_CHANNELS        (SCSI_MAX_CONTROLLERS + 2)

/************* Strings for the VESA BIOS Identity Fields *****************/
#define VBE_OEM_STRING COMPANY_NAME " SVGA"
#define VBE_VENDOR_NAME COMPANY_NAME
#define VBE_PRODUCT_NAME PRODUCT_GENERIC_NAME

/************* PCI implementation limits ********************************/
#define PCI_MAX_BRIDGES         15

/************* Ethernet implementation limits ***************************/
#define MAX_ETHERNET_CARDS      10

/********************** Floppy limits ***********************************/
#define MAX_FLOPPY_DRIVES      2

/************* PCI Passthrough implementation limits ********************/
#define MAX_PCI_PASSTHRU_DEVICES 6

/************* USB implementation limits ********************************/
#define MAX_USB_DEVICES_PER_HOST_CONTROLLER 127

/************* Strings for Host USB Driver *******************************/

#ifdef _WIN32

/*
 * Globally unique ID for the VMware device interface. Define INITGUID before including
 * this header file to instantiate the variable.
 */
DEFINE_GUID(GUID_DEVICE_INTERFACE_VMWARE_USB_DEVICES, 
0x2da1fe75, 0xaab3, 0x4d2c, 0xac, 0xdf, 0x39, 0x8, 0x8c, 0xad, 0xa6, 0x65);

/*
 * Globally unique ID for the VMware device setup class.
 */
DEFINE_GUID(GUID_CLASS_VMWARE_USB_DEVICES, 
0x3b3e62a5, 0x3556, 0x4d7e, 0xad, 0xad, 0xf5, 0xfa, 0x3a, 0x71, 0x2b, 0x56);

/*
 * This string defines the device ID string of a VMware USB device.
 * The format is USB\Vid_XXXX&Pid_YYYY, where XXXX and YYYY are the
 * hexadecimal representations of the vendor and product ids, respectively.
 *
 * The official vendor ID for VMware, Inc. is 0x0E0F.
 * The product id for USB generic devices is 0x0001.
 */
#define USB_VMWARE_DEVICE_ID_WIDE L"USB\\Vid_0E0F&Pid_0001"
#define USB_DEVICE_ID_LENGTH (sizeof(USB_VMWARE_DEVICE_ID_WIDE) / sizeof(WCHAR))

#ifdef UNICODE
#define USB_PNP_SETUP_CLASS_NAME L"VMwareUSBDevices"
#define USB_PNP_DRIVER_NAME L"vmusb"
#else
#define USB_PNP_SETUP_CLASS_NAME "VMwareUSBDevices"
#define USB_PNP_DRIVER_NAME "vmusb"
#endif
#endif

#endif /* VM_DEVICE_VERSION_H */
