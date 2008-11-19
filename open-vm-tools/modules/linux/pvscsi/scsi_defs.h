/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * scsi_defs.h
 *
 * General SCSI definitions
 */

#ifndef _SCSI_DEFS_H_
#define _SCSI_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_basic_defs.h"  // for offsetof()

/*
 * Non-exhaustive list of SCSI operation codes.  Note that
 * some codes are defined differently according to the target
 * device.  Also, codes may have slightly different meanings
 * and/or names based on the version of the SCSI spec.
 *
 * NB: Command descriptions come from the "SCSI Book" and not
 *     from the SCSI specifications (YMMV).
 */
#define SCSI_CMD_TEST_UNIT_READY       0x00	// test if LUN ready to accept a command
#define SCSI_CMD_REZERO_UNIT	       0x01	// seek to track 0
#define SCSI_CMD_REQUEST_SENSE	       0x03	// return detailed error information
#define SCSI_CMD_FORMAT_UNIT	       0x04	//
#define SCSI_CMD_READ_BLOCKLIMITS      0x05	//
#define SCSI_CMD_REASSIGN_BLOCKS       0x07	// 
#define SCSI_CMD_INIT_ELEMENT_STATUS   0x07     // Media changer
#define SCSI_CMD_READ6		       0x08	// read w/ limited addressing
#define SCSI_CMD_WRITE6		       0x0a	// write w/ limited addressing
#define SCSI_CMD_PRINT		       0x0a	// print data
#define SCSI_CMD_SEEK6		       0x0b	// seek to LBN
#define SCSI_CMD_SLEW_AND_PRINT	       0x0b	// advance and print
#define SCSI_CMD_READ_REVERSE	       0x0f	// read backwards
#define SCSI_CMD_WRITE_FILEMARKS       0x10	// 
#define SCSI_CMD_SYNC_BUFFER	       0x10	// print contents of buffer
#define SCSI_CMD_SPACE		       0x11	// 
#define SCSI_CMD_INQUIRY	       0x12	// return LUN-specific information
#define SCSI_CMD_RECOVER_BUFFERED      0x14	// recover buffered data
#define SCSI_CMD_MODE_SELECT	       0x15	// set device parameters
#define SCSI_CMD_RESERVE_UNIT	       0x16	// make LUN accessible only to certain initiators
#define SCSI_CMD_RELEASE_UNIT	       0x17	// make LUN accessible to other initiators
#define SCSI_CMD_COPY		       0x18	// autonomous copy from/to another device
#define SCSI_CMD_ERASE		       0x19	// 
#define SCSI_CMD_MODE_SENSE	       0x1a	// read device parameters
#define SCSI_CMD_START_UNIT	       0x1b	// load/unload medium
#define SCSI_CMD_SCAN		       0x1b	// perform scan
#define SCSI_CMD_STOP_PRINT	       0x1b	// interrupt printing
#define SCSI_CMD_RECV_DIAGNOSTIC       0x1c	// read self-test results
#define SCSI_CMD_SEND_DIAGNOSTIC       0x1d	// initiate self-test
#define SCSI_CMD_MEDIUM_REMOVAL	       0x1e	// lock/unlock door
#define SCSI_CMD_READ_FORMAT_CAPACITIES 0x23	// read format capacities
#define SCSI_CMD_SET_WINDOW	       0x24	// set scanning window
#define SCSI_CMD_GET_WINDOW	       0x25	// get scanning window
#define SCSI_CMD_READ_CAPACITY	       0x25	// read number of logical blocks
#define SCSI_CMD_READ10		       0x28	// read
#define SCSI_CMD_READ_GENERATION       0x29	// read max generation address of LBN
#define SCSI_CMD_WRITE10	       0x2a	// write
#define SCSI_CMD_SEEK10		       0x2b	// seek LBN
#define SCSI_CMD_POSITION_TO_ELEMENT   0x2b     // media changer
#define SCSI_CMD_ERASE10               0x2c
#define SCSI_CMD_READ_UPDATED_BLOCK    0x2d	// read specific version of changed block
#define SCSI_CMD_WRITE_VERIFY	       0x2e	// write w/ verify of success
#define SCSI_CMD_VERIFY		       0x2f	// verify success
#define SCSI_CMD_SEARCH_DATA_HIGH      0x30	// search for data pattern
#define SCSI_CMD_SEARCH_DATA_EQUAL     0x31	// search for data pattern
#define SCSI_CMD_SEARCH_DATA_LOW       0x32	// search for data pattern
#define SCSI_CMD_SET_LIMITS	       0x33	// define logical block boundaries
#define SCSI_CMD_PREFETCH	       0x34	// read data into buffer
#define SCSI_CMD_READ_POSITION	       0x34	// read current tape position
#define SCSI_CMD_SYNC_CACHE	       0x35	// re-read data into buffer
#define SCSI_CMD_LOCKUNLOCK_CACHE      0x36	// lock/unlock data in cache
#define SCSI_CMD_READ_DEFECT_DATA      0x37	// 
#define SCSI_CMD_MEDIUM_SCAN	       0x38	// search for free area
#define SCSI_CMD_COMPARE	       0x39	// compare data
#define SCSI_CMD_COPY_VERIFY	       0x3a	// autonomous copy w/ verify
#define SCSI_CMD_WRITE_BUFFER	       0x3b	// write data buffer
#define SCSI_CMD_READ_BUFFER	       0x3c	// read data buffer
#define SCSI_CMD_UPDATE_BLOCK	       0x3d	// substitute block with an updated one
#define SCSI_CMD_READ_LONG	       0x3e	// read data and ECC
#define SCSI_CMD_WRITE_LONG	       0x3f	// write data and ECC
#define SCSI_CMD_CHANGE_DEF	       0x40	// set SCSI version
#define SCSI_CMD_WRITE_SAME	       0x41	// 
#define SCSI_CMD_READ_SUBCHANNEL       0x42	// read subchannel data and status
#define SCSI_CMD_READ_TOC	       0x43	// read contents table
#define SCSI_CMD_READ_HEADER	       0x44	// read LBN header
#define SCSI_CMD_PLAY_AUDIO10	       0x45	// audio playback
#define SCSI_CMD_GET_CONFIGURATION     0x46	// get configuration (SCSI-3)
#define SCSI_CMD_PLAY_AUDIO_MSF	       0x47	// audio playback starting at MSF address
#define SCSI_CMD_PLAY_AUDIO_TRACK      0x48	// audio playback starting at track/index
#define SCSI_CMD_PLAY_AUDIO_RELATIVE   0x49	// audio playback starting at relative track
#define SCSI_CMD_GET_EVENT_STATUS_NOTIFICATION 0x4a
#define SCSI_CMD_PAUSE		       0x4b	// audio playback pause/resume
#define SCSI_CMD_LOG_SELECT	       0x4c	// select statistics
#define SCSI_CMD_LOG_SENSE	       0x4d	// read statistics
#define SCSI_CMD_STOP_PLAY	       0x4e	// audio playback stop
#define SCSI_CMD_READ_DISC_INFO        0x51     // info on CDRs
#define SCSI_CMD_READ_TRACK_INFO       0x52     // track info on CDRs
#define SCSI_CMD_RESERVE_TRACK         0x53     // leave space for data on CDRs
#define SCSI_CMD_SEND_OPC_INFORMATION  0x54     // Optimum Power Calibration
#define SCSI_CMD_MODE_SELECT10	       0x55	// set device parameters
#define SCSI_CMD_RESERVE_UNIT10        0x56     //
#define SCSI_CMD_RELEASE_UNIT10        0x57     //
#define SCSI_CMD_REPAIR_TRACK          0x58
#define SCSI_CMD_MODE_SENSE10	       0x5a	// read device parameters
#define SCSI_CMD_CLOSE_SESSION         0x5b     // close area/sesssion (recordable)
#define SCSI_CMD_READ_BUFFER_CAPACITY  0x5c     // CDR burning info.
#define SCSI_CMD_SEND_CUE_SHEET        0x5d     // (CDR Related?)
#define SCSI_CMD_PERSISTENT_RESERVE_IN 0x5e     //
#define SCSI_CMD_PERSISTENT_RESERVE_OUT 0x5f    //
#define SCSI_CMD_XDWRITE_EXTENDED      0x80
#define SCSI_CMD_REBUILD               0x81
#define SCSI_CMD_REGENERATE            0x82
#define SCSI_CMD_EXTENDED_COPY         0x83     // extended copy
#define SCSI_CMD_RECEIVE_COPY_RESULTS  0x84     // receive copy results
#define SCSI_CMD_READ16	               0x88     // read data
#define SCSI_CMD_WRITE16               0x8a     // write data
#define SCSI_CMD_ORWRITE16             0x8b
#define SCSI_CMD_READ_ATTRIBUTE        0x8c     // read attribute
#define SCSI_CMD_WRITE_ATTRIBUTE       0x8d     // write attribute
#define SCSI_CMD_WRITE_VERIFY16        0x8e
#define SCSI_CMD_VERIFY16              0x8f
#define SCSI_CMD_PREFETCH16            0x90
#define SCSI_CMD_SYNC_CACHE16          0x91
#define SCSI_CMD_WRITE_SAME16          0x93
#define SCSI_CMD_READ_CAPACITY16       0x9e     // read number of logical blocks
#define SCSI_CMD_WRITE_LONG16          0x9f
#define SCSI_CMD_REPORT_LUNS           0xa0     // 
#define SCSI_CMD_BLANK                 0xa1     // erase RW media
#define SCSI_CMD_SECURITY_PROTOCOL_IN  0xa2     // 
#define SCSI_CMD_MAINTENANCE_IN        0xa3	// service actions define reports
#define SCSI_CMD_MAINTENANCE_OUT       0xa4	// service actions define changes
#define SCSI_CMD_SEND_KEY	       0xa3
#define SCSI_CMD_REPORT_KEY	       0xa4	// report key (SCSI-3)
#define SCSI_CMD_MOVE_MEDIUM	       0xa5	// 
#define SCSI_CMD_PLAY_AUDIO12	       0xa5	// audio playback
#define SCSI_CMD_EXCHANGE_MEDIUM       0xa6     //
#define SCSI_CMD_LOADCD		       0xa6     //
#define SCSI_CMD_SET_READ_AHEAD        0xa7
#define SCSI_CMD_READ12		       0xa8	// read (SCSI-3)
#define SCSI_CMD_PLAY_TRACK_RELATIVE   0xa9	// audio playback starting at relative track
#define SCSI_CMD_WRITE12	       0xaa	// write data
#define SCSI_CMD_READ_MEDIA_SERIAL_NUMBER 0xab  //
#define SCSI_CMD_ERASE12	       0xac	// erase logical block
#define SCSI_CMD_GET_PERFORMANCE       0xac	//
#define SCSI_CMD_READ_DVD_STRUCTURE    0xad	// read DVD structure (SCSI-3)
#define SCSI_CMD_WRITE_VERIFY12	       0xae	// write logical block, verify success
#define SCSI_CMD_VERIFY12	       0xaf	// verify data
#define SCSI_CMD_SEARCH_DATA_HIGH12    0xb0	// search data pattern
#define SCSI_CMD_SEARCH_DATA_EQUAL12   0xb1	// search data pattern
#define SCSI_CMD_SEARCH_DATA_LOW12     0xb2	// search data pattern
#define SCSI_CMD_SET_LIMITS12	       0xb3	// set block limits
#define SCSI_CMD_REQUEST_VOLUME_ELEMENT_ADDR 0xb5 //
#define SCSI_CMD_SECURITY_PROTOCOL_OUT 0xb5
#define SCSI_CMD_SEND_VOLUME_TAG       0xb6     //
#define SCSI_CMD_SET_STREAMING         0xb6     // For avoiding over/underrun
#define SCSI_CMD_READ_DEFECT_DATA12    0xb7	// read defect data information
#define SCSI_CMD_READ_ELEMENT_STATUS   0xb8	// read element status
#define SCSI_CMD_SELECT_CDROM_SPEED    0xb8	// set data rate
#define SCSI_CMD_READ_CD_MSF	       0xb9	// read CD information (all formats, MSF addresses)
#define SCSI_CMD_AUDIO_SCAN	       0xba	// fast audio playback
#define SCSI_CMD_SET_CDROM_SPEED       0xbb     // (proposed)
#define SCSI_CMD_SEND_CDROM_XA_DATA    0xbc
#define SCSI_CMD_PLAY_CD	       0xbc
#define SCSI_CMD_MECH_STATUS           0xbd
#define SCSI_CMD_READ_CD	       0xbe	// read CD information (all formats, MSF addresses)
#define SCSI_CMD_SEND_DVD_STRUCTURE    0xbf	// burning DVDs?

/*
 * A workaround for a specific scanner (NIKON LS-2000).  
 * Can be removed once Linux backend uses 2.4.x interface
 */
#define SCSI_CMD_VENDOR_NIKON_UNKNOWN  0xe1

#define SCSI_SENSE_KEY_NONE	       0x0   // there is no sense information
#define SCSI_SENSE_KEY_RECOVERED_ERROR 0x1   // the last command completed succesfully but used error correction in the process
#define SCSI_SENSE_KEY_NOT_READY       0x2   // the addressed LUN is not ready to be accessed
#define SCSI_SENSE_KEY_MEDIUM_ERROR    0x3   // the target detected a data error on the medium
#define SCSI_SENSE_KEY_HARDWARE_ERROR  0x4   // the target detected a hardware error during a command or self-test
#define SCSI_SENSE_KEY_ILLEGAL_REQUEST 0x5   // either the command or the parameter list contains an error
#define SCSI_SENSE_KEY_UNIT_ATTENTION  0x6   // the LUN has been reset (bus reset of medium change)
#define SCSI_SENSE_KEY_DATA_PROTECT    0x7   // access to the data is blocked
#define SCSI_SENSE_KEY_BLANK_CHECK     0x8   // reached an unexpected written or unwritten region of the medium
#define SCSI_SENSE_KEY_COPY_ABORTED    0xa   // COPY, COMPARE, or COPY AND VERIFY was aborted
#define SCSI_SENSE_KEY_ABORTED_CMD     0xb   // the target aborted the command
#define SCSI_SENSE_KEY_EQUAL	       0xc   // comparison for SEARCH DATA was unsuccessful
#define SCSI_SENSE_KEY_VOLUME_OVERFLOW 0xd   // the medium is full
#define SCSI_SENSE_KEY_MISCOMPARE      0xe   // source and data on the medium do not agree

/*
 * The Additional Sense Code - ASC             and
 *     Additional Sense Code Qualifiers - ASCQ
 * always come in pairs. 
 *
 * Note:
 *     These values are found at senseBuffer[12} and senseBuffer[13].
 *     You may see references to these in legacy code. New code should make an
 *     attempt to use the ASC/ASCQ syntax.
 */
#define SCSI_ASC_LU_NOT_READY                                   0x04  // logical unit not ready
#define SCSI_ASC_LU_NOT_READY_ASCQ_UNIT_BECOMING_READY               0x01
#define SCSI_ASC_LU_NOT_READY_ASCQ_INIT_CMD_REQUIRED                 0x02  // initializing command required
#define SCSI_ASC_LU_NOT_READY_ASCQ_MANUAL_INTERVENTION_REQUIRED      0x03
#define SCSI_ASC_LU_NOT_READY_ASCQ_TARGET_PORT_IN_TRANSITION	     0x0a // an ascq
#define SCSI_ASC_LU_NOT_READY_ASCQ_TARGET_PORT_IN_STANDBY_MODE       0x0b // an ascq
#define SCSI_ASC_LU_NO_RESPONSE_TO_SELECTION                    0x05  // logical unit doesn't respond to selection
#define SCSI_ASC_NO_REFERENCE_POSITION_FOUND                    0x06
#define SCSI_ASC_WRITE_ERROR                                    0x0c  // Write error
#define SCSI_ASC_UNRECOVERED_READ_ERROR                         0x11  // Unrecovered read error
#define SCSI_ASC_PARAM_LIST_LENGTH_ERROR                        0x1a  // parameter list length error
#define SCSI_ASC_INVALID_COMMAND_OPERATION      		0x20  // invalid command operation code
#define SCSI_ASC_INVALID_FIELD_IN_CDB                           0x24
#define SCSI_ASC_LU_NOT_SUPPORTED                               0x25  // LU has been removed
#define SCSI_ASC_INVALID_FIELD_IN_PARAMETER_LIST                0x26
#define SCSI_ASC_WRITE_PROTECTED			 	0x27  // device is write protected
#define SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED                        0x28  // after changing medium
#define SCSI_ASC_POWER_ON_OR_RESET     				0x29  // device power-on or SCSI reset
#define SCSI_ASC_ASYMMETRIC_ACCESS_STATE_CHANGED                0x2a
#define SCSI_ASC_INCOMPATIBLE_MEDIUM                            0x30  // Generic bad medium error
#define SCSI_ASC_SAVING_PARAMS_NOT_SUPPORTED                    0x39  // Saving parameters not supported
#define SCSI_ASC_MEDIUM_NOT_PRESENT                             0x3a  // changing medium
#define SCSI_ASC_MEDIUM_NOT_PRESENT_ASCQ_TRAY_OPEN              0x02 // an ascq
#define SCSI_ASC_INVALID_MESSAGE_ERROR                          0x49
#define SCSI_ASC_COMMAND_PHASE_ERROR                            0x4a
#define SCSI_ASC_DATA_PHASE_ERROR                               0x4b
#define SCSI_ASC_MEDIUM_REMOVAL_FAILED                          0x53 // w/ 0x4 it is failed, 0x5 is prevented
#define SCSI_ASC_INSUFFICIENT_REGISTRATION_RESOURCES            0x55 // during persistent reservations
#define SCSI_ASCQ_INSUFFICIENT_REGISTRATION_RESOURCES           0x04 
#define SCSI_ASCQ_ASYMMETRIC_ACCESS_STATE_CHANGED               0x06
#define SCSI_ASCQ_TARGET_PORT_IN_STANDBY_STATE                  0x0b
#define SCSI_ASCQ_TARGET_PORT_IN_UNAVAILABLE_STATE              0x0c
#define SCSI_ASC_INVALID_MODE_FOR_THIS_TRACK                    0x64 

#define SCSI_TAG_ENABLE		0x20	// Set to indicate tag is valid
#define SCSI_TAG_SIMPLE		(SCSI_TAG_ENABLE|0x0)	// No constraint
#define SCSI_TAG_HEAD		(SCSI_TAG_ENABLE|0x1)	// Always first
#define SCSI_TAG_ORDER		(SCSI_TAG_ENABLE|0x2)	// Syncronizing

#define SCSI_CMD_START_UNIT_START_BIT 0x01   // Value of Start bit for SCSI_CMD_START_UNIT

/*
 * SCSI Command Data Blocks (CDBs) come in at least four flavors:
 *
 * 1. 6-byte commands were originally spec'd and limit the addressable
 *    storage to 1GByte (21 bits x 512 bytes/logical block).
 * 2. 10-byte commands first appeared in SCSI-2; they have a 32-bit
 *    logical block number range but transfers are limited to 64KB.
 * 3. 12-byte commands also appeared in SCSI-2; they differ mainly
 *    int that large amounts of data may be transferred (32-bit data length).
 * 4. 16-byte commands were added in SCSI-3; they have additional space
 *    for unspecified command data.
 *
 * We do not support 16-byte CDB's, only 6-, 10-, and 12-byte versions.
 */
typedef struct {
   uint32   opcode:8,	// operation code
	    lun:3,	// logical unit number
	    lbn:21;	// logical block number
   uint8    len;	// data length
   uint8    ctrl;	// control byte
} SCSICDB6;
typedef
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;
   uint8       :5,
	    lun:3;
   uint32   lbn;
   uint8    reserved;
   uint16   len;
   uint8    ctrl;
}
#include "vmware_pack_end.h"
SCSICDB10;
typedef
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;
   uint8       :5,
	    lun:3;
   uint32   lbn;
   uint32   len;
   uint8    reserved;
   uint8    ctrl;
}
#include "vmware_pack_end.h"
SCSICDB12;

/*
 * Format of INQUIRY request and response blocks.
 * These are defined here because many SCSI devices
 * need them.
 */
typedef struct {
   uint8 opcode;           // INQUIRY (0x12)
   uint8 evpd  :1,         // enhanced vital product data
         cmddt :1,         // command support data (new in SCSI-3)
         resv12:3,
         lun   :3;
   uint8 pagecode;         // only valid when cmddt or evdp is set
   uint8 reserved;        
   uint8 len;
   uint8 ctrl;
} SCSIInquiryCmd;

/* 
 * Format of the SCSI-3 INQUIRY command as defined in SPC-3 
 */
typedef struct {
   uint8 opcode;           // INQUIRY (0x12)
   uint8 evpd  :1,         // Enhanced Vital Product Data
         obslt :1,         // Obsolete as per SPC-3
         resv  :6;         // The remaining bits are all RESERVED
   uint8 pagecode;         // Only valid when evpd is set
   uint8 lenMSB;           // The SPC-3 spec has a 2 byte len field
   uint8 len;
   uint8 ctrl;
} SCSI3InquiryCmd;

typedef struct {
   uint8 devclass    :5,   // SCSI device class
#define SCSI_CLASS_DISK	   0x00	    // disk drive
#define SCSI_CLASS_TAPE	   0x01	    // tape drive
#define SCSI_CLASS_PRINTER 0x02	    // printer
#define SCSI_CLASS_CPU	   0x03	    // processor device
#define SCSI_CLASS_WORM	   0x04	    // WORM drive
#define SCSI_CLASS_CDROM   0x05	    // CD-ROM drive
#define SCSI_CLASS_SCANNER 0x06	    // scanner
#define SCSI_CLASS_OPTICAL 0x07	    // optical disk
#define SCSI_CLASS_MEDIA   0x08	    // media changer
#define SCSI_CLASS_COM	   0x09	    // communication device
#define IDE_CLASS_CDROM    0x0a	    // IDE CD-ROM drive
#define IDE_CLASS_OTHER    0x0b	    // Generic IDE
#define SCSI_CLASS_RAID	   0x0c	    // RAID controller (SCSI-3, reserved in SCSI-2)
#define SCSI_CLASS_SES 	   0x0d	    // SCSI Enclosure Services device (t10 SES)
#define SCSI_CLASS_UNKNOWN 0x1f	    // unknown device
	 pqual	     :3;   // peripheral qualifier
#define SCSI_PQUAL_CONNECTED	 0  // device described is connected to the LUN
#define SCSI_PQUAL_NOTCONNECTED	 1  // target supports such a device, but none is connected
#define SCSI_PQUAL_NODEVICE	 3  // target does not support a physical device for this LUN
   uint8    :7,		   // reserved for SCSI-1
	 rmb:1;		   // removable bit
   uint8 ansi	     :3,   // ANSI version
#define SCSI_ANSI_SCSI1	     0x0   // device supports SCSI-1
#define SCSI_ANSI_CCS	     0x1   // device supports the CCS
#define SCSI_ANSI_SCSI2	     0x2   // device supports SCSI-2
#define SCSI_ANSI_SCSI3_SPC  0x3   // device supports SCSI-3 version SPC
#define SCSI_ANSI_SCSI3_SPC2 0x4   // device supports SCSI-3 version SPC-2
#define SCSI_ANSI_SCSI3_SPC3 0x5   // device supports SCSI-3 version SPC-3
#define SCSI_ANSI_SCSI3_SPC4 0x6   // device supports SCSI-3 version SPC-4
	 ecma	     :3,   // ECMA version
	 iso	     :2;   // ISO version
   uint8 dataformat  :4,   // format of the following standard data
		     :1,
	 naca	     :1,
	 tio	     :1,   // device supports TERMINATE I/O PROCESS message
	 aen	     :1;   // asynchronous event notification capability
   uint8 optlen;	   // length of additional data that follows
   uint8	     :8;
#define SCSI_TPGS_NONE                       0x0
#define SCSI_TPGS_IMPLICIT_ONLY              0x1
#define SCSI_TPGS_IMPLICIT		     SCSI_TPGS_IMPLICIT_ONLY 
#define SCSI_TPGS_EXPLICIT_ONLY              0x2
#define SCSI_TPGS_EXPLICIT                   SCSI_TPGS_EXPLICIT_ONLY
#define SCSI_TPGS_BOTH_IMPLICIT_AND_EXPLICIT 0x3
#define SCSI_TPGS_BOTH                       SCSI_TPGS_BOTH_IMPLICIT_AND_EXPLICIT
   uint8 adr16	     :1,   // device supports 16-bit wide SCSI addresses
	 adr32	     :1,   // device supports 32-bit wide SCSI addresses
	 arq	     :1,
	 mchngr	     :1,   // device has attached media changer (SCSI-3)
	 dualp	     :1,   // device is dual-ported (SCSI-3)
         port	     :1,   // port A or port B when dual-ported (SCSI-3)
		     :2;
   uint8 sftr	     :1,   // device supports soft reset capability
	 que	     :1,   // device supports tagged commands
	 trndis	     :1,   // device supports transfer disable messages (SCSI-3)
	 link	     :1,   // device supports linked commands
	 sync	     :1,   // device supports synchronous transfers
	 w16	     :1,   // device supports 16-bit wide SCSI data transfers
         w32	     :1,   // device supports 32-bit wide SCSI data transfers
	 rel	     :1;   // device supports relative addressing
   uint8 manufacturer[8];  // manufacturer's name in ascii
   uint8 product[16];	   // product name in ascii
   uint8 revision[4];	   // product version number in ascii
   uint8 vendor1[20];	   // vendor unique data (opaque)
   uint8 reserved[40];
} SCSIInquiryResponse;	   // standard INQUIRY response format

/*
 * Same as SCSIInquiryResponse, except that only 36 bytes long.  See above
 * for some defines.  You should use this one and not one above unless you
 * need vendor1/reserved fields.
 */
typedef struct {
   uint8 devclass    :5,   // SCSI device class
	 pqual	     :3;   // peripheral qualifier
   uint8    :7,		   // reserved for SCSI-1
	 rmb:1;		   // removable bit
   uint8 ansi	     :3,   // ANSI version
	 ecma	     :3,   // ECMA version
	 iso	     :2;   // ISO version
   uint8 dataformat  :4,   // format of the following standard data
		     :1,
	 naca	     :1,
	 tio	     :1,   // device supports TERMINATE I/O PROCESS message
	 aen	     :1;   // asynchronous event notification capability
   uint8 optlen;	   // length of additional data that follows
   uint8	     :8;
   uint8 adr16	     :1,   // device supports 16-bit wide SCSI addresses
	 adr32	     :1,   // device supports 32-bit wide SCSI addresses
	 arq	     :1,
	 mchngr	     :1,   // device has attached media changer (SCSI-3)
	 dualp	     :1,   // device is dual-ported (SCSI-3)
         port	     :1,   // port A or port B when dual-ported (SCSI-3)
		     :2;
   uint8 sftr	     :1,   // device supports soft reset capability
	 que	     :1,   // device supports tagged commands
	 trndis	     :1,   // device supports transfer disable messages (SCSI-3)
	 link	     :1,   // device supports linked commands
	 sync	     :1,   // device supports synchronous transfers
	 w16	     :1,   // device supports 16-bit wide SCSI data transfers
         w32	     :1,   // device supports 32-bit wide SCSI data transfers
	 rel	     :1;   // device supports relative addressing
   uint8 manufacturer[8];  // manufacturer's name in ascii
   uint8 product[16];	   // product name in ascii
   uint8 revision[4];	   // product version number in ascii
} SCSIInquiry36Response;   // standard INQUIRY response format

#define SCSI_STANDARD_INQUIRY_MIN_LENGTH 36

#define SCSI_INQ_PAGE_0x00 0x00
#define SCSI_INQ_PAGE_0x80 0x80
#define SCSI_INQ_PAGE_0x83 0x83

/*
 * The following structures define the Page format supported by the
 * vscsi layer in vmkernel. The SPC-3 r23 spec defines a very generic
 * layout of these pages, however the structures here are customized 
 * for vmkernel.
 */
typedef
#include "vmware_pack_begin.h"
struct SCSIInqPage00ResponseHeader {
   uint8  devClass	:5,
   	  pQual		:3;
   uint8  pageCode;
   uint8  reserved1;
   uint8  pageLength;
}
#include "vmware_pack_end.h"
SCSIInqPage00ResponseHeader;

typedef
#include "vmware_pack_begin.h"
struct SCSIInqPage80ResponseHeader {
   uint8  devClass	:5,
   	  pQual		:3;
   uint8  pageCode;
   uint8  reserved1;
   uint8  pageLength;
}
#include "vmware_pack_end.h"
SCSIInqPage80ResponseHeader;

// Inquiry page 0x83: Identifier Type
#define SCSI_IDENTIFIERTYPE_VENDOR_SPEC	0x0
#define SCSI_IDENTIFIERTYPE_T10	        0x1
#define SCSI_IDENTIFIERTYPE_EUI		0x2
#define SCSI_IDENTIFIERTYPE_NAA		0x3
#define SCSI_IDENTIFIERTYPE_RTPI	0x4
#define SCSI_IDENTIFIERTYPE_TPG	        0x5
#define SCSI_IDENTIFIERTYPE_LUG	        0x6
#define SCSI_IDENTIFIERTYPE_MD5	        0x7
#define SCSI_IDENTIFIERTYPE_SNS       	0x8
#define SCSI_IDENTIFIERTYPE_RESERVED   	0x9
#define SCSI_IDENTIFIERTYPE_MAX         SCSI_IDENTIFIERTYPE_RESERVED

// Inquiry page 0x83: Transport Layer
#define SCSI_PROTOCOLID_FCP2		0x0
#define SCSI_PROTOCOLID_SPI5	        0x1
#define SCSI_PROTOCOLID_SSAS3P		0x2
#define SCSI_PROTOCOLID_SBP3		0x3
#define SCSI_PROTOCOLID_SRP	        0x4
#define SCSI_PROTOCOLID_ISCSI		0x5
#define SCSI_PROTOCOLID_SAS		0x6
#define SCSI_PROTOCOLID_ADT		0x7
#define SCSI_PROTOCOLID_ATA		0x8
#define SCSI_PROTOCOLID_RESERVED	0xE
#define SCSI_PROTOCOLID_NO_PROTOCOL	0xF

// Inquiry page 0x83: UUID Encoding
#define SCSI_CODESET_BINARY		0x1
#define SCSI_CODESET_ASCII		0x2
#define SCSI_CODESET_UTF8		0x3
#define SCSI_CODESET_RESERVED		0xF

// Inquiry page 0x83: UUID Entity
#define SCSI_ASSOCIATION_LUN		0x0
#define SCSI_ASSOCIATION_TARGET_PORT	0x1
#define SCSI_ASSOCIATION_TARGET_DEVICE	0x2
#define SCSI_ASSOCIATION_RESERVED	0x3

typedef
#include "vmware_pack_begin.h"
struct SCSIInqPage83ResponseHeader {
   uint8  devClass	:5,
          pQual		:3;
   uint8  pageCode;
   uint16 pageLength;
}
#include "vmware_pack_end.h"
SCSIInqPage83ResponseHeader;

typedef
#include "vmware_pack_begin.h"
struct SCSIInqPage83ResponseDescriptor {
   /* Identification Descriptor follows */
   uint8  codeSet     :4,
          protocolId  :4;
   uint8  idType      :4,
          association :2,
          reserved1   :1,
          piv         :1;
   uint8  reserved2;
   uint8  idLength;
}
#include "vmware_pack_end.h"
SCSIInqPage83ResponseDescriptor;

typedef
#include "vmware_pack_begin.h"
struct SCSIInquiryVPDResponseHeader {
   uint8 devclass    :5,   // SCSI device class
         pqual       :3;   // peripheral qualifier
   uint8 pageCode;         // 0
   uint8 reserved;
   uint8 payloadLen;       // Number of additional bytes
}
#include "vmware_pack_end.h"
SCSIInquiryVPDResponseHeader;

typedef
#include "vmware_pack_begin.h"
struct SCSIReportLunsCmd {
   uint8 opcode;
   uint8 reserved1;
   uint8 selectReport;
   uint8 reserved2[3];
   uint32 allocLen;
   uint16 reserved3;
}
#include "vmware_pack_end.h"
SCSIReportLunsCmd;

typedef
#include "vmware_pack_begin.h"
struct SCSIReportLunsResponse {
   uint32 len;
   uint32 reserved;
   struct {
      uint8  addressMethod:3,
         busIdentifier:5;
      uint8  singleLevelLun;
      uint16 secondLevelLun;
      uint16 thirdLevelLun;
      uint16 fourthLevelLun;
   } lun[1];
}
#include "vmware_pack_end.h"
SCSIReportLunsResponse;

#define SCSI_REPORT_LUNS_RESPONSE_LEN(n) (sizeof(SCSIReportLunsResponse) + ((n)-1) * sizeof(((SCSIReportLunsResponse*)0)->lun[0]))

/*
 * Format of 6- and 10-byte versions of the MODE SELECT
 * and MODE SENSE request and response blocks.
 * These are defined here because multiple SCSI devices
 * may need them.
 */
typedef struct {
   uint8    opcode;	   // operation code
   uint8	  :3,
	    dbd	  :1,	   // disable block descriptors
		  :1,
	    lun	  :3;	   // logical unit number
   uint8    page	  :6,	   // page code
#define SCSI_MS_PAGE_VENDOR   0x00     // vendor-specific (ALL)
#define SCSI_MS_PAGE_RWERROR  0x01     // read/write error (DISK/TAPE/CDROM/OPTICAL)
#define SCSI_MS_PAGE_CONNECT  0x02     // disconnect/connect (ALL)
#define SCSI_MS_PAGE_FORMAT   0x03     // format (DISK)
#define SCSI_MS_PAGE_PARALLEL 0x03     // parallel interface (PRINTER)
#define SCSI_MS_PAGE_UNITS    0x03     // measurement units (SCANNER)
#define SCSI_MS_PAGE_GEOMETRY 0x04     // rigid disk geometry (DISK)
#define SCSI_MS_PAGE_SERIAL   0x04     // serial interface (PRINTER)
#define SCSI_MS_PAGE_FLEXIBLE 0x05     // flexible disk geometry (DISK)
#define SCSI_MS_PAGE_PRINTER  0x05     // printer operations (PRINTER)
#define SCSI_MS_PAGE_OPTICAL  0x06     // optical memory (OPTICAL)
#define SCSI_MS_PAGE_VERIFY   0x07     // verification error (DISK/CDROM/OPTICAL)
#define SCSI_MS_PAGE_CACHE    0x08     // cache (DISK/CDROM/OPTICAL)
#define SCSI_MS_PAGE_PERIPH   0x09     // peripheral device (ALL)
#define SCSI_MS_PAGE_CONTROL  0x0a     // control mode (ALL)
#define SCSI_MS_PAGE_MEDIUM   0x0b     // medium type (DISK/CDROM/OPTICAL)
#define SCSI_MS_PAGE_NOTCH    0x0c     // notch partitions (DISK)
#define SCSI_MS_PAGE_CDROM    0x0d     // CD-ROM (CDROM)
#define SCSI_MS_PAGE_CDAUDIO  0x0e     // CD-ROM audio (CDROM)
#define SCSI_MS_PAGE_COMPRESS 0x0f     // data compression (TAPE)
#define SCSI_MS_PAGE_CONFIG   0x10     // device configuration (TAPE)
#define SCSI_MS_PAGE_EXCEPT   0x1c     // informal exception (ALL:SCSI-3)
#define SCSI_MS_PAGE_CDCAPS   0x2a     // CD-ROM capabilities and mechanical status (CDROM)
// more defined...
#define SCSI_MS_PAGE_ALL      0x3f     // all available pages (ALL)
	    pcf	  :2;	   // page control field
#define SCSI_MS_PCF_CURRENT   0x00     // current values
#define SCSI_MS_PCF_VOLATILE  0x01     // changeable values
#define SCSI_MS_PCF_DEFAULT   0x02     // default values
#define SCSI_MS_PCF_SAVED     0x03     // saved values
   uint8    subpage;
   uint8    length;  // data length
   uint8    ctrl;	   // control byte
} SCSIModeSenseCmd;


/*
 * FORMAT UNIT command
 */
typedef
#include "vmware_pack_begin.h"
struct {
   uint8  opcode;	   // FORMAT UNIT (0x4)
   uint8  dlf    :3,	   // defect list format
	  cmplst :1,	   // complete list
	  fmtdata:1,	   // format data
	  lun    :3;
   uint8  vendor;
   uint16 interleave;
   uint8  control;
}
#include "vmware_pack_end.h"
SCSIFormatCmd;

/*
 * Format Defect List header
 */
typedef struct {
   uint8 reserved;	   
   uint8 fov   :1,	   // Format options valid
	 dpry  :1,	   // disable primary
  	 dcrt  :1,         // disable certification
	 stpf  :1,         // stop format
	 ip    :1,         // initialization pattern
 	 dsp   :1,         // disable saving parameters
	 immed :1,         // immediate 
	 vs    :1;
   uint16 length;	   // Defect list length
} SCSIDefectListHdr;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;	   // operation code
   uint8	  :3,
	    dbd	  :1,	   // disable block descriptors
		  :1,
	    lun	  :3;	   // logical unit number
   uint8    page  :6,	   // page code
	    pcf	  :2;	   // page control field
   uint8    reserved[4];
   uint16   length;	   // data length
   uint8    ctrl;	   // control byte
}
#include "vmware_pack_end.h"
SCSIModeSense10Cmd;

typedef struct {
   uint8    opcode;	   // operation code
   uint8    sp	  :1,	   // save pages
		  :3,
	    pf	  :1,	   // page format
	    lun	  :3;	   // logical unit number
   uint8    reserved[2];
   uint8    len;	   // data length
   uint8    ctrl;	   // control byte
} SCSIModeSelectCmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;	   // operation code
   uint8    sp	  :1,	   // save pages
		  :3,
	    pf	  :1,	   // page format
	    lun	  :3;	   // logical unit number
   uint8    reserved[5];
   uint16   len;	   // data length
   uint8    ctrl;	   // control byte
}
#include "vmware_pack_end.h"
SCSIModeSelect10Cmd;

typedef struct {
   uint8    len;	   // data length
   uint8    mediaType;
   uint8    devSpecific;   // device specific
   uint8    bdLen;	   // block descriptor length
} SCSIModeHeader6;

typedef struct {
   uint16   len;	   // data length
   uint8    mediaType;
   uint8    devSpecific;   // device specific
   uint16   reserved;
   uint16   bdLen;	   // block descriptor length
} SCSIModeHeader10;

typedef struct {
   uint8 reserved1:4;
   uint8 dpofua:1;         // disable page out / force unit access
   uint8 reserved2:2;
   uint8 wp:1;             // write protected
} SCSIBlockModeSenseDeviceParameter;

/*
 * Command structure for a SCSI Reserve command.
 */
typedef
#include "vmware_pack_begin.h"
struct {
   uint8 opcode:8;	// operation code
   uint8 ext:1,
      tid:3,
      tparty:1,
      lun:3;		// logical unit number
   uint8    resid;
   uint16   extlen;
   uint8    control;
}
#include "vmware_pack_end.h"
SCSIReserveCmd;

/*
 * There are three mandatory mode parameter pages for all device
 * types (a fourth is added in SCSI-3).  The following structures
 * define these pages as sent+received with MODE SENSE and SELECT.
 */
typedef struct {	   // connect/disconnect page
   uint8    page  :6,	   // page code: 0x02
		  :1,
	    ps	  :1;
   uint8    len;	   // page length (0x0e)
   uint8    bufFull;
   uint8    bufEmpty;
   uint16   maxBusInactiveTime;
   uint16   maxBusFreeTime;
   uint16   maxConnectTime;
   uint16   maxBurstLength;
   uint8    dtdc  :3,
	    dimm  :1,	   // disconnect immediate (SCSI-3)
		  :3,
	    emdp  :1;	   // enable MODIFY DATA POINTER (SCSI-3)
} SCSIConnectPage;

typedef struct {	   // peripheral device page
   uint8    page  :6,	   // page code: 0x09
		  :1,
	    ps	  :1;
   uint8    len;	   // page length (n-1)
   uint16   ifID;	   // physical interface identifier
   uint8    reserved[4];
   uint8    undefined[1];  // variable-length vendor-specific data
} SCSIPeriphPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x0a
		  :1,
	    ps	  :1;
   uint8    len;	   // page length (0x06)
   uint8    rlec  :1,
	    gltsd :1,
		  :2,
		  :4;
   uint8    dque  :1,	   // disable tagged queuing
	    qerr  :1,	   // 
		  :2,
	    qalg  :4;	   // queue algorithm
   uint8    eaenp :1,	   // error AEN permission
	    uaaenp:1,	   // unit attention AEN permission
	    raenp :1,	   // ready AEN permission
		  :4,
	    eeca  :1;	   // 
   uint8    reserved;
   uint16   aenWaitTime;   // AEN waiting time after initialization
   uint16   busyTimeout;   // busy timeout in 100ms (SCSI-3)
} SCSIControlPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x09
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x06
   uint8    dcr	  :1,	   // error recover parameters
	    dte	  :1,	   // diable transfer on seeing recovered error
	    per	  :1,	   // post error: report recovered errors
		  :1,
	    rc	  :1,	   // read continuous: don't delay data transfer to correct errors
	    tb	  :1,	   // transfer block when unrecovered
		  :2;
   uint8    readRetries;   // read retry count
   uint8    reserved[4];
} SCSIRWErrorPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x0d
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x06
   uint8	  :8;
   uint8    inactive:4,	   // head inactivity timeout
		  :4;
   uint16   secsPerMinute; // number of MSF seconds per MSF minute
   uint16   framesPerSec;  // number of MSF frames per MSF second
} SCSICDROMPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x0e
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x0e
   uint8	  :1,
	    sotb  :1,
	    immediate:1,
		  :5;
   uint8	  :8;
   uint8	  :8;
   uint8    lbaFactor:4,
		  :3,
	    aprv  :1;
   uint16   lbaPerSec;	   // number of LBAs per second
   uint8    port0 :4,	   // output port 0 select
		  :4;
   uint8    port0Volume;
   uint8    port1 :4,	   // output port 1 select
		  :4;
   uint8    port1Volume;
   uint8    port2 :4,	   // output port 2 select
		  :4;
   uint8    port2Volume;
   uint8    port3 :4,	   // output port 3 select
		  :4;
   uint8    port3Volume;
} SCSICDROMAudioPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x2a
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x12
   uint8    cdrRd	:1,// CD-R read per Orange Book Part II
	    cdeRd	:1,// CD-E read per Orange Book Part III
	    method2	:1,// CD-R media w/ Addressing Method 2
			:5;
   uint8    cdrWr	:1,// CD-R write per Orange Book Part II
	    cdeWr	:1,// CD-E write per Orange Book Part III
			:6;
   uint8    audioPlay	:1,// drive is capable of audio play
	    composite	:1,// drive is capable of delivering composite audio+video
	    digPort1	:1,// drive supports digital output (IEC958) on port 1
	    digPort2    :1,// drive supports digital output on port 2
	    mode2Form1	:1,// drive reads Mode 2 Form 1 (XA) format
	    mode2Form2	:1,// drive reads Mode 2 Form 2 format
	    multiSession:1,// drive reads multi-session or Photo-CD discs
			:1;
   uint8    cdDA	:1,// CD-DA commands (Red Book) supported
	    daAccu	:1,// CD-DA stream is accurate
	    rwSupported	:1,// R-W supported
	    rwDeinter	:1,// R-W subchannel data de-interleaved and corrected
	    c2Ptrs	:1,// C2 Error Pointers supported
	    isrc	:1,// drive returns International Standard Recording Code Info
	    upc		:1,// drive returns Media Catalog Number
			:1;
   uint8    lock	:1,// PREVENT/ALLOW commands lock media into drive
	    lockState	:1,// current state of drive
	    jumpers	:1,// state of prevent/allow jumpers
	    eject	:1,// drive can eject disc via START/STOP command
			:1,
	    loadType	:3;// loading mechanism type
   uint8    sv	  	:1,// separate volume
	    scm		:1,// separate channel mute
	    sdp		:1,// supports disc present in MECHANISM STATUS command
	    sss		:1,// s/w slot selection w/ LOAD/UNLOCK command
			:4;
   uint16   maxSpeed;	   // maximum speed supported (in KB/s)
   uint16   numVolLevels;  // number of volume levels supported
   uint16   bufSize;	   // buffer size supported by drive (KBytes)
   uint16   curSpeed;	   // current speed selected (in KB/s)
   uint8    reserved;
   uint8		:1,// format of digital data output
	    bck		:1,// data is valid on the falling edge of BCK
	    rck		:1,// HIGH on LRCK indicates left channel
	    lsbf	:1,// LSB first
	    length	:2,
			:2;
   uint8    reserved2[2];
} SCSICDROMCapabilitiesPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x03
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x16
   uint16   tracksPerZone;
   uint16   repSectorsPerZone;
   uint16   repTracksPerZone;
   uint16   replTracksPerLUN;
   uint16   sectorsPerTrack;
   uint16   bytesPerSector;
   uint16   interleave;
   uint16   trackSkew;
   uint16   cylinderSkew;
   uint8	  :3,
	    surf  :1,
	    rmb	  :1,
	    hsec  :1,
	    ssec  :1;
   uint8    reserved[3];
} SCSIFormatPage;

typedef uint8 uint24[3];

typedef struct {
   uint8    page  :6,	   // page code: 0x04
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x16
   uint24   cylinders;	   // number of cylinders
   uint8    heads;	   // number of heads
   uint24   writeCompCylinder; // starting cylinder for write compensation
   uint24   writeCurCylinder; // starting cylinder for reduce write current
   uint16   stepRate;
   uint24   landingZone;   // cylinder number of landing zone
   uint8    rpl	  :1,
		  :7;
   uint8    rotOffset;	   // rotational offset
   uint8	  :8;
   uint16   rotRate;	   // medium rotation rate
   uint8    reserved[2];
} SCSIGeometryPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x08
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x0a (0x12 for SCSI-3)
   uint8    rcd	  :1,
	    mf	  :1,
	    wce	  :1,
		  :5;
   uint8    readPri:4,	   // read retention priority
	    writePri:4;	   // write retention priority
   uint16   prefetchDisable;// disable pre-fetch transfer length
   uint16   prefetchMin;   // pre-fetch minimum
   uint16   prefetchMax;   // pre-fetch maximum
   uint16   prefetchAbsMax;// absolute pre-fetch maximum
} SCSICachePage;

typedef struct {
   uint8    page  :6,	   // page code: 0x08
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x16
   uint8	  :6,
	    lpn	  :1,
	    nd	  :1;
   uint8	  :8;
   uint16   maxNotches;	   // maximum number of notches
   uint16   activeNotch;
   uint32   activeStart;   // beginning of active notch
   uint32   activeEnd;	   // end of active notch
} SCSINotchPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x06
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x02
   uint8    rubr  :1,
		  :7;
   uint8	  :8;
} SCSIOpticalPage;
typedef struct {
   uint8    page  :6,	   // page code: 0x0f
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x0e
   uint8	  :6,
	    dcc	  :1,
	    dce	  :1;
   uint8	  :5,
	    red	  :2,
	    dde	  :1;
   uint8  compAlg[4];
   uint8  decompAlg[4];
   uint8  reserved[4];
} SCSICompressionPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x10
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x0e
   uint8    format:5,	   // active format
	    car	  :1,
	    cap	  :1,
		  :1;
   uint8    partition;	   // active partition
   uint8    wbeRatio;	   // write buffer empty ratio
   uint8    rbeRatio;	   // read buffer empty ratio
   uint16   writeDelay;
   uint8    rew	  :1,
	    rb0   :1,
	    sofc  :2,
	    avc   :1,
	    rsmk  :1,
	    bis   :1,
	    dbr	  :1;
   uint8    gapSize;
   uint8	  :3,
	    sew	  :1,
	    eeg	  :1,
	    eod	  :3;
   uint24   bufSizeAtEW;
   uint8    compression;
   uint8	  :8;
} SCSIDeviceConfigPage;

typedef struct {
   uint8    page  :6,	   // page code: 0x03
		  :1,
	    ps	  :1;
   uint8    len;	   // page length 0x06
   uint8    unit;	   // measurement unit
   uint8	  :8;
   uint16   divisor;
   uint16	  :16;
} SCSIUnitsPage;


/*
 * Format of START STOP UNIT (0x1b).
 */
typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 opcode;	// 0x1b
   uint8 immed:1,
          rsvd:7;
   uint8 reserved[2];
   uint8  start:1,
           loej:1,      // load/eject
          rsvd1:2,
          power:4;
   uint8 control;
}
#include "vmware_pack_end.h"
SCSIStartStopUnitCmd;


/*
 * Format of ALLOW PREVENT MEDIUM REMOVAL (0x1e).
 */
typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 opcode;	// 0x1e
   uint8 reserved[3];
   uint8 prevent:2,
                :6;
   uint8 control;
}
#include "vmware_pack_end.h"
SCSIMediumRemovalCmd;


/*
 * Format of READ CAPACITY (10) and (16) request and response blocks.
 * These are defined here because multiple SCSI devices
 * need them.
 */
typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 opcode;	// 0x25
   uint8 rel   :1,
	       :4,
	 lun   :3;
#define SCSI_RW10_MAX_LBN 0xffffffffu
   uint32 lbn;
   uint8 reserved[2];
   uint8 pmi   :1,
	       :7;
   uint8 control;
}
#include "vmware_pack_end.h"
SCSIReadCapacityCmd;

typedef struct {
   uint32 lbn;
   uint32 blocksize;
} SCSIReadCapacityResponse;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 opcode;	// 0x9e
#define SCSI_READ_CAPACITY16_SERVICE_ACTION 0x10
   uint8 action:5,
	       :3;
   uint64 lbn;
   uint32 len;
   uint8 pmi   :1,
	 rel   :1,
	       :6;
   uint8 control;
}
#include "vmware_pack_end.h"
SCSIReadCapacity16Cmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint64 lbn;
   uint32 blocksize;
}
#include "vmware_pack_end.h"
SCSIReadCapacity16Response;

/*
 * Format of READ/WRITE (6), (10), (12) and (16)
 * request. These are defined here because multiple SCSI
 * devices need them.
 */
typedef 
#include "vmware_pack_begin.h"
struct {
   uint32 opcode:8,
	  lun:3,
	  lbn:21;
   uint8  length;
   uint8  control;
}
#include "vmware_pack_end.h"
SCSIReadWrite6Cmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 opcode;
   uint8 rel   :1,
	       :2,
	 flua  :1,
	 dpo   :1,
	 lun   :3;
   uint32 lbn;
   uint8 reserved;
   uint16 length;
   uint8 control;
}
#include "vmware_pack_end.h"
SCSIReadWrite10Cmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 opcode;
   uint8 rel   :1,
	       :2,
	 flua  :1,
	 dpo   :1,
	 lun   :3;
   uint32 lbn;
   uint32 length;
   uint8 reserved;
   uint8 control;
}
#include "vmware_pack_end.h"
SCSIReadWrite12Cmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 opcode;
   uint8 rel   :1,
	       :2,
	 flua  :1,
	 dpo   :1,
	       :3;
   uint64 lbn;
   uint32 length;
   uint8 reserved;
   uint8 control;
}
#include "vmware_pack_end.h"
SCSIReadWrite16Cmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;
   uint8    xtnt  :1,           // extent-based reservation
            ptyID :3,           // 3rd party reservation ID
            pty          :1,    // 3rd party reservation
            lun          :3;    // logical unit number
   uint8    resvID;             // SCSI-3: reservation ID
   uint16   resvListLen;        // SCSI-3: reservation list length
   uint8    ctrl;               // control byte
}
#include "vmware_pack_end.h"
SCSIReserveUnitCmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;
   uint8    xtnt  :1,           // extent-based reservation
            ptyID :3,           // 3rd party reservation ID
            pty   :1,           // 3rd party reservation
            lun   :3;           // logical unit number
   uint8    resvID;             // SCSI-3: reservation ID
   uint8    reserved[2];
   uint8    ctrl;               // control byte
} 
#include "vmware_pack_end.h"
SCSIReleaseUnitCmd;

typedef 
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;
   uint8    uniO  :1,           // unit offline
            devO  :1,           // device offline
            st    :1,           // self-test
                  :1,
            pf    :1,           // page format
            lun   :3;           // logical unit number
   uint8    reserved;
   uint16   len;                // data length
   uint8    ctrl;               // control byte
}
#include "vmware_pack_end.h"
SCSISendDiagnosticCmd;

typedef
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;
   uint8    relAdr  :1,         // relative address
            bytChk  :1,         // byte
            blkvfy  :1,         // blank blocks verification, scsi-3
                    :1,
            dpo     :1,         // cache control bit
            lun     :3;         // logical unit number
   uint32   lbn;                // logical block address
   uint8    reserved;
   uint16   len;                // verification length
   uint8    ctrl;               // control byte
}
#include "vmware_pack_end.h"
SCSIVerify10Cmd;

typedef
#include "vmware_pack_begin.h"
struct {
   uint8    opcode;
   uint8    polled   :1,        // asynchronous or not
                     :7;
   uint8    reserved0[2];
#define SCSI_GESN_CLASS_RSVD0           (1 << 0)
#define SCSI_GESN_CLASS_OP_CHANGE       (1 << 1)
#define SCSI_GESN_CLASS_POW_MGMT        (1 << 2)
#define SCSI_GESN_CLASS_EXT_REQ         (1 << 3)
#define SCSI_GESN_CLASS_MEDIA           (1 << 4)
#define SCSI_GESN_CLASS_MULTI_HOST      (1 << 5)
#define SCSI_GESN_CLASS_DEV_BUSY        (1 << 6)
#define SCSI_GESN_CLASS_RSVD1           (1 << 7)
   uint8    notifyClassReq;      // the class of events we are interested in
   uint8    reserved1[2];
   uint16   length;             // allocation length
   uint8    control;
}
#include "vmware_pack_end.h"
SCSIGetEventStatusNotificationCmd;

/*
 * Format of Persistent Reservation Commands per SPC-3 r23, required for 
 * virtualizing reservations.
 */

/* Persistent Reserve IN service actions */
typedef enum {
   READ_KEYS                      = 0x0,
   READ_RESERVATION               = 0x1,
   REPORT_CAPABILITIES            = 0x2,
   READ_FULL_STATUS               = 0x3
} SCSIPersistentReserveInServiceAction;

/* 
 * Persistent reservation type codes 
 */
typedef enum {
   WRITE_EXCL                     = 0x1,
   EXCL_ACCESS                    = 0x3,
   WRITE_EXCL_REG_ONLY            = 0x5,
   EXCL_ACCESS_REG_ONLY           = 0x6,
   WRITE_EXCL_ALL_REG             = 0x7,
   EXCL_ACCESS_ALL_REG            = 0x8
} SCSIPersistentReserveTypeCode;

typedef 
#include "vmware_pack_begin.h"
struct { 
   uint8  opcode; 
   uint8  serviceAction :5,
          reserved      :3;
   uint8  reserved1[5];
   uint16 allocationLength;
   uint8  control;
} 
#include "vmware_pack_end.h"
SCSIPersistentReserveInCmd;

/* Persistent Reserve Out Service Actions */
typedef enum {
   REGISTER                          = 0x0,
   PRESERVE                          = 0x1,
   PRELEASE                          = 0x2,
   CLEAR                             = 0x3,
   PREEMPT                           = 0x4,
   PREEMPT_AND_ABORT                 = 0x5,
   REGISTER_AND_IGNORE_EXISTING_KEY  = 0x6,
   REGISTER_AND_MOVE                 = 0x7
} SCSIPersistentReserveOutServiceAction;


typedef 
#include "vmware_pack_begin.h"
struct {
   uint8  opcode;
   uint8  serviceAction :5,
          reserved      :3;
   uint8  type          :4,
          scope         :4;
   uint8  reserved1[2];
   uint32 parameterListLength; 
   uint8  control;
} 
#include "vmware_pack_end.h"
SCSIPersistentReserveOutCmd;

typedef
#include "vmware_pack_begin.h"
struct {
   uint64 reservationKey;
   uint64 serviceActionResKey;
   uint8  obsolete1[4];
   uint8  aptpl          :1,
          reserved1      :1,
          all_tg_pt      :1,
          spec_i_pt      :1,
          reserved2      :4;
   uint8  reserved3;
   uint8  obsolete2[2];
   /*
    * Per SPC-3 r23, the parameter list length shall be 24 bytes in length if the
    * following are true:
    *  a. the SPEC_I_PT but is set to 0
    *  b. service action is not REGISTER AND MOVE
    * 
    * This is currently the only supported mode in the vmkernel,
    * so no additional parameter data is included in this struct
    */
} 
#include "vmware_pack_end.h"
SCSIPersistentReserveOutPList;

typedef
#include "vmware_pack_begin.h"
struct {
   uint32 prGeneration;
   uint32 additionalLength;
   uint64 reservationKey;
   uint8  obsolete[4];
   uint8  reserved;
   uint8  type      :4,
          scope     :4;
   uint8 obsolete1[2];
} 
#include "vmware_pack_end.h"
SCSIPRReadReservationResp;


/*
 * Format of the sense data structure maintained in each SCSI
 * device.  Devices should fill in this data structure whenever
 * they return a CHECK status for a SCSI command.  The contents
 * is returned to the initiator either through the adapter doing
 * an auto-sense request or the initiator doing an explicit
 * REQUEST SENSE SCSI operation.  A device keeps only one copy
 * of sense data at a time; the base SCSI device support invalidates
 * this data structure before each SCSI operation as needed.
 */
typedef 
#include "vmware_pack_begin.h"
struct {
   uint8 error	  :7,	   // 0x70 for current command, 0x71 for earlier command
#define SCSI_SENSE_ERROR_CURCMD  0x70	    // sense data is for "current command"
#define SCSI_SENSE_ERROR_PREVCMD 0x71	    // sense data is for an earlier command
	 valid	  :1;	   // sense data valid
/* NB: Please Note that the valid bit above does NOT tell you whether
 * the sense is actually valid and thus the name is really badly chosen
 * (even though it is the official name from the SCSI II specification).
 * The SCSI II spec. states "A valid bit of zero indicates that the
 * information field is not as defined in this International Standard".
 * we have seen that many tape drives are capable of returning sense
 * without this bit set....
 */
   uint8 segment;	   // segment number
   uint8 key	  :4,	   // sense key
		  :1,
	 ili	  :1,
	 eom	  :1,
	 filmrk	  :1;
   uint8 info[4];	   // general information
   uint8 optLen;	   // length of optional data that follows
   uint8 cmdInfo[4];	   // command-specific information
   uint8 code;		   // sense code
   uint8 xcode;		   // extended sense code
   uint8 fru;		   // 
   uint8 bitpos	  :3,
	 bpv	  :1,
		  :2,
	 cd	  :1,	   // 1 if error in command, 0 if in data
	 sksv	  :1;	   // sense key specific data is valid
   uint16 epos;		   // offset of first byte in error
   
   // Some vendors want to return additional data which
   // requires a sense buffer of up to 64 bytes.
   uint8 additional[46];
}
#include "vmware_pack_end.h"
SCSISenseData;


/*
 * Read (DVD) Disc Structure definitions.
 */
typedef
#include "vmware_pack_begin.h"
struct {
   uint8  opcode;
#define SCSI_RDS_MT_DVD  0x0
#define SCSI_RDS_MT_BD   0x1
   uint8  mediaType:4,
                   :4;
   uint32 address;
   uint8  layerNumber;
                                                /* Layer, Address */
#define SCSI_RDS_GDS_AACS_VOLUME_ID        0x80
#define SCSI_RDS_GDS_AACS_MEDIA_SERIAL_NUM 0x81
#define SCSI_RDS_GDS_AACS_MEDIA_ID         0x82
#define SCSI_RDS_GDS_AACS_MEDIA_KEY        0x83 /* Layer number, Pack Number */
#define SCSI_RDS_GDS_LAYERS_LIST           0x90
#define SCSI_RDS_GDS_WRITE_PROTECT         0xC0
#define SCSI_RDS_GDS_CAPABILITY_LIST       0xFF

#define SCSI_RDS_DVD_PHYSICAL_INFO_LEADIN  0x00 /* Layer, - */
#define SCSI_RDS_DVD_COPYRIGHT_INFO_LEADIN 0x01 /* Layer, - */
#define SCSI_RDS_DVD_DISC_KEY              0x02
#define SCSI_RDS_DVD_BURST_CUTTING_AREA    0x03
#define SCSI_RDS_DVD_DISC_MANUFACTURING    0x04 /* Layer, - */
#define SCSI_RDS_DVD_COPYRIGHT_INFO_SECTOR 0x05 /* -, LBA */
#define SCSI_RDS_DVD_MEDIA_ID              0x06
#define SCSI_RDS_DVD_MEDIA_KEY             0x07 /* -, Pack Number */
#define SCSI_RDS_DVD_DVDRAM_DDS_INFO       0x08
#define SCSI_RDS_DVD_DVDRAM_MEDIUM_STATUS  0x09
#define SCSI_RDS_DVD_DVDRAM_SPARE_AREA     0x0A
#define SCSI_RDS_DVD_DVDRAM_RECORDING_TYPE 0x0B
#define SCSI_RDS_DVD_RMD_BORDEROUT         0x0C
#define SCSI_RDS_DVD_RMD_SECTOR            0x0D /* -, Start Field Number of RMA blocks */
#define SCSI_RDS_DVD_PRERECORDED_LEADIN    0x0E
#define SCSI_RDS_DVD_DVDR_MEDIA_ID         0x0F
#define SCSI_RDS_DVD_DVDR_PHYSICAL_INFO    0x10 /* Layer, - */
#define SCSI_RDS_DVD_ADIP_INFO             0x11 /* Layer, - */
#define SCSI_RDS_DVD_HDDVD_CPI             0x12 /* Layer, - */
#define SCSI_RDS_DVD_HDVD_COPYRIGHT_DATA   0x15 /* Layer, Start Copyright Sector */
#define SCSI_RDS_DVD_HDDVDR_MEDIUM_STATUS  0x19
#define SCSI_RDS_DVD_HDDVDR_RMD            0x1A

#define SCSI_RDS_DVD_DL_LAYER_CAPACITY     0x20
#define SCSI_RDS_DVD_DL_MIDDLE_ZONE_START  0x21
#define SCSI_RDS_DVD_DL_JUMP_INTERVAL_SIZE 0x22
#define SCSI_RDS_DVD_DL_MANUAL_LAYER_JUMP  0x23
#define SCSI_RDS_DVD_DL_REMAPPING          0x24 /* -, Anchor Point Number */

#define SCSI_RDS_DVD_DCB_IDENTIFIER        0x30 /* Session Number, Content Descriptor */
#define SCSI_RDS_DVD_MTA_ECC               0x31 /* -, PSN */

#define SCSI_RDS_BD_DI                     0x00
#define SCSI_RDS_BD_DDS                    0x08
#define SCSI_RDS_BD_CARTRIDGE_STATUS       0x09
#define SCSI_RDS_BD_SPARE_AREA             0x0A
#define SCSI_RDS_BD_RAW_DFL                0x12 /* -, Offset */
#define SCSI_RDS_BD_PAC                    0x30 /* -, ID and Format Number */
   uint8  format;
   uint16 length;
   uint8      :6,
          agid:2;
   uint8  control;
}
#include "vmware_pack_end.h"
SCSIReadDiscStructureCmd;

typedef
#include "vmware_pack_begin.h"
struct {
   uint16 length;
   uint16 rsvd;
   uint8  partVersion:4,
#define SCSI_RDS_DC_DVD_ROM        0x0
#define SCSI_RDS_DC_DVD_RAM        0x1
#define SCSI_RDS_DC_DVD_R          0x2
#define SCSI_RDS_DC_DVD_RW         0x3
#define SCSI_RDS_DC_HD_DVD_ROM     0x4
#define SCSI_RDS_DC_HD_DVD_RAM     0x5
#define SCSI_RDS_DC_HD_DVD_R       0x6
#define SCSI_RDS_DC_DVD_PLUS_RW    0x9
#define SCSI_RDS_DC_DVD_PLUS_R     0xA
#define SCSI_RDS_DC_DVD_PLUS_RW_DL 0xD
#define SCSI_RDS_DC_DVD_PLUS_R_DL  0xE
          diskCategory:4;
#define SCSI_RDS_MR_1X             0x0
#define SCSI_RDS_MR_2X             0x1
#define SCSI_RDS_MR_4X             0x2
#define SCSI_RDS_MR_8X             0x3
#define SCSI_RDS_MR_16X            0x4
#define SCSI_RDS_MR_UNSPECIFIED    0xF
   uint8  maximumRate:4,
#define SCSI_RDS_DS_120MM          0x0
#define SCSI_RDS_DS_80MM           0x1
          discSize:4;
/* layerType is bitvector */
#define SCSI_RDS_LT_EMBOSSED       0x1
#define SCSI_RDS_LT_RECORDABLE     0x2
#define SCSI_RDS_LT_REWRITEABLE    0x4
   uint8  layerType:4,
          track:1,
#define SCSI_RDS_LAYERS_SL         0x0
#define SCSI_RDS_LAYERS_DL         0x1
          layers:2,
          :1;
#define SCSI_RDS_TD_740NM          0x0
#define SCSI_RDS_TD_800NM          0x1
#define SCSI_RDS_TD_615NM          0x2
#define SCSI_RDS_TD_400NM          0x3
#define SCSI_RDS_TD_340NM          0x4
   uint8  trackDensity:4,
#define SCSI_RDS_LD_267NM          0x0
#define SCSI_RDS_LD_293NM          0x1
#define SCSI_RDS_LD_420NM          0x2
#define SCSI_RDS_LD_285NM          0x4
#define SCSI_RDS_LD_153NM          0x5
#define SCSI_RDS_LD_135NM          0x6
#define SCSI_RDS_LD_353NM          0x8
          linearDensity:4;
#define SCSI_RDS_STARTPSN_DVD      0x030000
#define SCSI_RDS_STARTPSN_DVDRAM   0x031000
#define SCSI_RDS_MAXSIZE_DVD       0xF80000
   uint32 startPSN;
   uint32 endPSN;
   uint32 endPSNLayer0;
   uint8  :7,
          bca:1;
   uint8  rsvd2[2048 - 17];
}
#include "vmware_pack_end.h"
SCSIRDSDVDPhysicalInfoLeadin;


/*
 * Host and device status definitions.
 *
 * These mimic the BusLogic adapter-specific definitions but are
 * intended to be adapter-independent (i.e. adapters that don't
 * define these values directly or define them with different values
 * must map them to known values).
 */

/*
 * host adapter status/error codes
 */
typedef enum {
   BTSTAT_SUCCESS       = 0x00,  // CCB complete normally with no errors
   BTSTAT_LINKED_COMMAND_COMPLETED           = 0x0a,
   BTSTAT_LINKED_COMMAND_COMPLETED_WITH_FLAG = 0x0b,
   BTSTAT_DATA_UNDERRUN = 0x0c,
   BTSTAT_SELTIMEO      = 0x11,  // SCSI selection timeout
   BTSTAT_DATARUN       = 0x12,  // data overrun/underrun
   BTSTAT_BUSFREE       = 0x13,  // unexpected bus free
   BTSTAT_INVPHASE      = 0x14,  // invalid bus phase or sequence requested by target
   BTSTAT_INVCODE       = 0x15,  // invalid action code in outgoing mailbox
   BTSTAT_INVOPCODE     = 0x16,  // invalid operation code in CCB
   BTSTAT_LUNMISMATCH   = 0x17,  // linked CCB has different LUN from first CCB
   BTSTAT_INVPARAM      = 0x1a,  // invalid parameter in CCB or segment list
   BTSTAT_SENSFAILED    = 0x1b,  // auto request sense failed
   BTSTAT_TAGREJECT     = 0x1c,  // SCSI II tagged queueing message rejected by target
   BTSTAT_BADMSG        = 0x1d,  // unsupported message received by the host adapter
   BTSTAT_HAHARDWARE    = 0x20,  // host adapter hardware failed
   BTSTAT_NORESPONSE    = 0x21,  // target did not respond to SCSI ATN, sent a SCSI RST
   BTSTAT_SENTRST       = 0x22,  // host adapter asserted a SCSI RST
   BTSTAT_RECVRST       = 0x23,  // other SCSI devices asserted a SCSI RST
   BTSTAT_DISCONNECT    = 0x24,  // target device reconnected improperly (w/o tag)
   BTSTAT_BUSRESET      = 0x25,  // host adapter issued BUS device reset
   BTSTAT_ABORTQUEUE    = 0x26,  // abort queue generated
   BTSTAT_HASOFTWARE    = 0x27,  // host adapter software error
   BTSTAT_HATIMEOUT     = 0x30,  // host adapter hardware timeout error
   BTSTAT_SCSIPARITY    = 0x34,  // SCSI parity error detected
} HostBusAdapterStatus;

// scsi device status values
typedef enum {
   SDSTAT_GOOD                    = 0x00, // no errors
   SDSTAT_CHECK                   = 0x02, // check condition
   SDSTAT_CONDITION_MET           = 0x04, // condition met
   SDSTAT_BUSY                    = 0x08, // device busy
   SDSTAT_INTERMEDIATE            = 0x10, 
   SDSTAT_INTERMEDIATE_CONDITION  = 0x14,
   SDSTAT_RESERVATION_CONFLICT    = 0x18, // device reserved by another host
   SDSTAT_COMMAND_TERMINATED      = 0x22,
   SDSTAT_TASK_SET_FULL           = 0x28, 
   SDSTAT_ACA_ACTIVE              = 0x30, 
   SDSTAT_TASK_ABORTED            = 0x40, 
} SCSIDeviceStatus;

typedef enum {
   SCSI_XFER_AUTO     = 0,    // transfer direction depends on opcode
   SCSI_XFER_TOHOST   = 1,    // data is from device -> adapter
   SCSI_XFER_TODEVICE = 2,    // data is from adapter -> device
   SCSI_XFER_NONE     = 3     // data transfer is suppressed
} SCSIXferType;

typedef enum {
   SCSI_EMULATE               = 0,   // emulate this command
   SCSI_DONT_EMULATE          = 1,   // do not emulate this command but log a message
   SCSI_DONT_EMULATE_DONT_LOG = 2    // do not emulate this command or log a message
} SCSIEmulation;

#define HBA_SCSI_ID  7                    //default HBA SCSI ID

/*
 *---------------------------------------------------------------------------
 * 
 * SCSICdb_IsRead --
 *
 *      This function returns TRUE if the scsi command passed as an argument is
 *      a read.
 *
 * Results:
 *      TRUE/FALSE
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE Bool
SCSICdb_IsRead(uint8 cdb0)        // IN
{
   return cdb0 == SCSI_CMD_READ6 
       || cdb0 == SCSI_CMD_READ10
       || cdb0 == SCSI_CMD_READ12
       || cdb0 == SCSI_CMD_READ16;
}


/*
 *---------------------------------------------------------------------------
 * 
 * SCSICdb_IsWrite --
 *
 *      This function returns TRUE if the scsi command passed as an argument is
 *      a write.
 *
 * Results:
 *      TRUE/FALSE
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE Bool
SCSICdb_IsWrite(uint8 cdb0)       // IN
{
   return cdb0 == SCSI_CMD_WRITE6 
       || cdb0 == SCSI_CMD_WRITE10
       || cdb0 == SCSI_CMD_WRITE12
       || cdb0 == SCSI_CMD_WRITE16;
}


/*
 *---------------------------------------------------------------------------
 *
 * SCSICdb_IsRW --
 *
 *      This function returns TRUE if the scsi command passed as an argument is
 *      a read or write.
 *
 * Results:
 *      TRUE/FALSE
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE Bool
SCSICdb_IsRW(uint8 cdb0)        // IN
{
   return SCSICdb_IsRead(cdb0) || SCSICdb_IsWrite(cdb0);
}


/*
 *---------------------------------------------------------------------------
 * 
 * SCSICdb_GetLengthFieldOffset --
 *
 *      Returns the offset in bytes of the 'length' field in the CDB
 *      of a given command.
 *
 * Results:
 *      Offset of 'length' field.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE uint16
SCSICdb_GetLengthFieldOffset(uint8 cmd)
{
   switch (cmd) {
   case SCSI_CMD_READ10:
   case SCSI_CMD_WRITE10:
      return offsetof(SCSIReadWrite10Cmd, length);
   case SCSI_CMD_READ6:
   case SCSI_CMD_WRITE6:
      return offsetof(SCSIReadWrite6Cmd, length);
   case SCSI_CMD_READ16:
   case SCSI_CMD_WRITE16:
      return offsetof(SCSIReadWrite16Cmd, length);
   case SCSI_CMD_READ12:
   case SCSI_CMD_WRITE12:
      return offsetof(SCSIReadWrite12Cmd, length);
   default:
      return 0;
   }
}

/*
 *---------------------------------------------------------------------------
 *
 * SCSI3InquiryLen --
 *
 *      Returns 16-bit allocation length specified in SCSI3 Inquriy CMD cmd 
 *
 * Results:
 *      16-bit allocation length.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE uint16
SCSI3InquiryLen(SCSI3InquiryCmd *inqCmd)        // IN
{
  return (inqCmd->lenMSB << 8) + inqCmd->len;
}


typedef 
#include "vmware_pack_begin.h"
struct SCSICmdInfo {
   uint8 code;
   uint8 xferType;
   char *name;
   uint8 emulation;
}
#include "vmware_pack_end.h"
SCSICmdInfo;

/* This array contains the data below defined in SCSI_CMD_INFO_DATA, but
 * can't assign the data here because it would be included in all .o, so
 * it should be initialized in one .o file for each part of the product.
 * In vmm, this is currently initialized in buslogicMdev.c.
 * In vmx, this is currently initialized in usbAnalyzer.c.
 * In vmkernel, this is currently initialized in vmk_scsi.c.
 */
extern SCSICmdInfo scsiCmdInfo[256];
#define SCSI_CMD_GET_CODE(cmd)     (scsiCmdInfo[cmd].code)
#define SCSI_CMD_GET_XFERTYPE(cmd) (scsiCmdInfo[cmd].xferType)
#define SCSI_CMD_GET_NAME(cmd)     (scsiCmdInfo[cmd].name)
#define SCSI_CMD_GET_EMULATION(cmd) (scsiCmdInfo[cmd].emulation)

#define SCSI_CMD_INFO_DATA \
   {SCSI_CMD_TEST_UNIT_READY,  SCSI_XFER_NONE,     "TEST UNIT READY", SCSI_EMULATE}, \
   {SCSI_CMD_REZERO_UNIT,      SCSI_XFER_NONE,     "REWIND/REZERO UNIT", SCSI_DONT_EMULATE}, \
   {0x02,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_REQUEST_SENSE,    SCSI_XFER_TOHOST,   "REQUEST SENSE", SCSI_EMULATE},	\
   {SCSI_CMD_FORMAT_UNIT,      SCSI_XFER_TODEVICE, "FORMAT UNIT", SCSI_EMULATE},	\
   {SCSI_CMD_READ_BLOCKLIMITS, SCSI_XFER_TOHOST,   "READ BLOCK LIMITS", SCSI_DONT_EMULATE},\
   {0x06,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_INIT_ELEMENT_STATUS, SCSI_XFER_AUTO,  NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ6,            SCSI_XFER_TOHOST,   "READ(6)", SCSI_EMULATE},		\
   {0x09,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_WRITE6,           SCSI_XFER_TODEVICE, "WRITE(6)", SCSI_EMULATE},	\
   {SCSI_CMD_SLEW_AND_PRINT,   SCSI_XFER_TODEVICE, NULL, SCSI_DONT_EMULATE},               \
   {0x0c,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x0d,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x0e,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ_REVERSE,     SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_SYNC_BUFFER,      SCSI_XFER_NONE,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_SPACE,            SCSI_XFER_NONE,     "SPACE", SCSI_DONT_EMULATE},            \
   {SCSI_CMD_INQUIRY,          SCSI_XFER_TOHOST,   "INQUIRY", SCSI_EMULATE}, \
   {0x13,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_RECOVER_BUFFERED, SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_MODE_SELECT,      SCSI_XFER_TODEVICE, "MODE SELECT(6)", SCSI_DONT_EMULATE},  \
   {SCSI_CMD_RESERVE_UNIT,     SCSI_XFER_NONE,     "RESERVE(6)", SCSI_EMULATE},	\
   {SCSI_CMD_RELEASE_UNIT,     SCSI_XFER_NONE,     "RELEASE(6)", SCSI_EMULATE},	\
   {SCSI_CMD_COPY,             SCSI_XFER_AUTO,     "COPY AND VERIFY", SCSI_DONT_EMULATE},  \
   {SCSI_CMD_ERASE,            SCSI_XFER_NONE,     "ERASE", SCSI_DONT_EMULATE},            \
   {SCSI_CMD_MODE_SENSE,       SCSI_XFER_TOHOST,   "MODE SENSE(6)", SCSI_EMULATE},   \
   {SCSI_CMD_SCAN,             SCSI_XFER_TODEVICE, NULL, SCSI_EMULATE},		\
   {SCSI_CMD_RECV_DIAGNOSTIC,  SCSI_XFER_AUTO,     "RECEIVE DIAGNOSTIC RESULTS", SCSI_DONT_EMULATE}, \
   {SCSI_CMD_SEND_DIAGNOSTIC,  SCSI_XFER_TODEVICE, "SEND DIAGNOSTIC", SCSI_DONT_EMULATE},  \
   {SCSI_CMD_MEDIUM_REMOVAL,   SCSI_XFER_NONE,     "LOCK/UNLOCK DOOR", SCSI_DONT_EMULATE_DONT_LOG}, \
   {0x1f,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x20,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x21,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x22,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ_FORMAT_CAPACITIES, SCSI_XFER_TOHOST, "READ FORMAT CAPACITIES", SCSI_DONT_EMULATE}, \
   {SCSI_CMD_SET_WINDOW,       SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ_CAPACITY,    SCSI_XFER_TOHOST,   "READ CAPACITY", SCSI_EMULATE},	\
   {0x26,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x27,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ10,           SCSI_XFER_TOHOST,   "READ(10)", SCSI_EMULATE},	\
   {SCSI_CMD_READ_GENERATION,  SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_WRITE10,          SCSI_XFER_TODEVICE, "WRITE(10)", SCSI_EMULATE},	\
   {SCSI_CMD_SEEK10,           SCSI_XFER_NONE,     NULL, SCSI_DONT_EMULATE},               \
   {0x2c,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ_UPDATED_BLOCK, SCSI_XFER_AUTO,   NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_WRITE_VERIFY,     SCSI_XFER_AUTO,     "WRITE VERIFY", SCSI_DONT_EMULATE},     \
   {SCSI_CMD_VERIFY,           SCSI_XFER_NONE,     "VERIFY", SCSI_EMULATE},		\
   {SCSI_CMD_SEARCH_DATA_HIGH, SCSI_XFER_AUTO,     "SEARCH HIGH", SCSI_DONT_EMULATE},      \
   {SCSI_CMD_SEARCH_DATA_EQUAL, SCSI_XFER_AUTO,     "SEARCH EQUAL", SCSI_DONT_EMULATE},    \
   {SCSI_CMD_SEARCH_DATA_LOW,  SCSI_XFER_AUTO,     "SEARCH LOW", SCSI_DONT_EMULATE},       \
   {SCSI_CMD_SET_LIMITS,       SCSI_XFER_AUTO,     "SET LIMITS", SCSI_DONT_EMULATE},       \
   {SCSI_CMD_READ_POSITION,    SCSI_XFER_TOHOST,   NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_SYNC_CACHE,       SCSI_XFER_NONE,     "SYNC CACHE", SCSI_EMULATE},	\
   {SCSI_CMD_LOCKUNLOCK_CACHE, SCSI_XFER_AUTO,     "LOCK/UNLOCK CACHE", SCSI_DONT_EMULATE},\
   {SCSI_CMD_READ_DEFECT_DATA, SCSI_XFER_AUTO,     "READ DEFECT DATA", SCSI_DONT_EMULATE}, \
   {SCSI_CMD_MEDIUM_SCAN,      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_COMPARE,          SCSI_XFER_AUTO,     "COMPARE", SCSI_DONT_EMULATE},          \
   {SCSI_CMD_COPY_VERIFY,      SCSI_XFER_AUTO,     "COPY AND VERIFY", SCSI_DONT_EMULATE},  \
   {SCSI_CMD_WRITE_BUFFER,     SCSI_XFER_AUTO,     "WRITE BUFFER", SCSI_DONT_EMULATE_DONT_LOG},	\
   {SCSI_CMD_READ_BUFFER,      SCSI_XFER_AUTO,     "READ BUFFER", SCSI_DONT_EMULATE_DONT_LOG},	\
   {SCSI_CMD_UPDATE_BLOCK,     SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ_LONG,        SCSI_XFER_AUTO,     "READ LONG", SCSI_DONT_EMULATE},        \
   {SCSI_CMD_WRITE_LONG,       SCSI_XFER_AUTO,     "WRITE LONG", SCSI_DONT_EMULATE},       \
   {SCSI_CMD_CHANGE_DEF,       SCSI_XFER_NONE,     "CHANGE DEFINITION", SCSI_DONT_EMULATE},\
   {SCSI_CMD_WRITE_SAME,       SCSI_XFER_AUTO,     "WRITE SAME", SCSI_DONT_EMULATE},       \
   {SCSI_CMD_READ_SUBCHANNEL,  SCSI_XFER_TOHOST,   "READ SUBCHANNEL", SCSI_DONT_EMULATE},  \
   {SCSI_CMD_READ_TOC,         SCSI_XFER_TOHOST,   "READ TOC", SCSI_DONT_EMULATE},         \
   {SCSI_CMD_READ_HEADER,      SCSI_XFER_TOHOST,   "READ HEADER", SCSI_DONT_EMULATE},      \
   {SCSI_CMD_PLAY_AUDIO10,     SCSI_XFER_NONE,     "PLAY AUDIO(10)", SCSI_DONT_EMULATE},   \
   {SCSI_CMD_GET_CONFIGURATION, SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},              \
   {SCSI_CMD_PLAY_AUDIO_MSF,   SCSI_XFER_NONE,     "PLAY AUDIO MSF", SCSI_DONT_EMULATE},   \
   {SCSI_CMD_PLAY_AUDIO_TRACK, SCSI_XFER_AUTO,     "PLAY AUDIO TRACK", SCSI_DONT_EMULATE}, \
   {SCSI_CMD_PLAY_AUDIO_RELATIVE, SCSI_XFER_AUTO,  "PLAY AUDIO RELATIVE", SCSI_DONT_EMULATE}, \
   {SCSI_CMD_GET_EVENT_STATUS_NOTIFICATION,	SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},  \
   {SCSI_CMD_PAUSE,            SCSI_XFER_NONE,     "PAUSE/RESUME", SCSI_DONT_EMULATE},     \
   {SCSI_CMD_LOG_SELECT,       SCSI_XFER_TODEVICE, "LOG SELECT", SCSI_DONT_EMULATE},       \
   {SCSI_CMD_LOG_SENSE,        SCSI_XFER_TOHOST,   "LOG SENSE", SCSI_DONT_EMULATE},        \
   {SCSI_CMD_STOP_PLAY,        SCSI_XFER_NONE,     "STOP PLAY", SCSI_DONT_EMULATE},        \
   {0x4f,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x50,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ_DISC_INFO,   SCSI_XFER_TOHOST,   "CDR INFO", SCSI_DONT_EMULATE},         \
   {SCSI_CMD_READ_TRACK_INFO,  SCSI_XFER_TOHOST,   "TRACK INFO", SCSI_DONT_EMULATE},       \
   {SCSI_CMD_RESERVE_TRACK,    SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x54,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_MODE_SELECT10,    SCSI_XFER_TODEVICE, "MODE SELECT(10)", SCSI_DONT_EMULATE},  \
   {SCSI_CMD_RESERVE_UNIT10,   SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_RELEASE_UNIT10,   SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x58,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0x59,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_MODE_SENSE10,     SCSI_XFER_TOHOST,   "MODE SENSE(10)", SCSI_DONT_EMULATE},   \
   {SCSI_CMD_CLOSE_SESSION,    SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_READ_BUFFER_CAPACITY, SCSI_XFER_AUTO, NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_SEND_CUE_SHEET,   SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {SCSI_CMD_PERSISTENT_RESERVE_IN, SCSI_XFER_TOHOST,     "PERSISTENT RESERVE IN", SCSI_EMULATE}, \
   {SCSI_CMD_PERSISTENT_RESERVE_OUT, SCSI_XFER_TODEVICE,     "PERSISTENT RESERVE OUT", SCSI_EMULATE},         \
   {0x60,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x61,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x62,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x63,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x64,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x65,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x66,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x67,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x68,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x69,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x6a,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x6b,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x6c,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x6d,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x6e,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x6f,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x70,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x71,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x72,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x73,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x74,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x75,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x76,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x77,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x78,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x79,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x7a,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x7b,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x7c,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x7d,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x7e,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x7f,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x80,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x81,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x82,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x83,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x84,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x85,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x86,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x87,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_READ16,           SCSI_XFER_TOHOST,   "READ(16)", SCSI_EMULATE},		\
   {0x89,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},		\
   {SCSI_CMD_WRITE16,          SCSI_XFER_TODEVICE, "WRITE(16)", SCSI_EMULATE},		\
   {0x8b,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x8c,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x8d,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x8e,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_VERIFY16,         SCSI_XFER_NONE,     "VERIFY(16)", SCSI_EMULATE},	\
   {0x90,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x91,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x92,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x93,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x94,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x95,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x96,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x97,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x98,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x99,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x9a,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x9b,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x9c,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0x9d,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_READ_CAPACITY16,  SCSI_XFER_TOHOST,   "READ CAPACITY 16", SCSI_EMULATE},		\
   {0x9f,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_REPORT_LUNS,      SCSI_XFER_AUTO,     "REPORT LUNS", SCSI_EMULATE},\
   {SCSI_CMD_BLANK,            SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xa2,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_SEND_KEY,         SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_REPORT_KEY,       SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_PLAY_AUDIO12,     SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_LOADCD,           SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xa7,                      SCSI_XFER_AUTO,     "MOVE MEDIUM", SCSI_DONT_EMULATE},	\
   {SCSI_CMD_READ12,           SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_PLAY_TRACK_RELATIVE, SCSI_XFER_AUTO,  NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_WRITE12,          SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xab,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_ERASE12,          SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_READ_DVD_STRUCTURE, SCSI_XFER_AUTO,   NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_WRITE_VERIFY12,   SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_VERIFY12,         SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_SEARCH_DATA_HIGH12, SCSI_XFER_AUTO,   NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_SEARCH_DATA_EQUAL12, SCSI_XFER_AUTO,  NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_SEARCH_DATA_LOW12, SCSI_XFER_AUTO,    NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_SET_LIMITS12,     SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xb4,                      SCSI_XFER_AUTO,     "READ ELEMENT STATUS", SCSI_DONT_EMULATE}, \
   {SCSI_CMD_REQUEST_VOLUME_ELEMENT_ADDR, SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},    \
   {SCSI_CMD_SET_STREAMING,    SCSI_XFER_TODEVICE, "SET STREAMING", SCSI_DONT_EMULATE},	\
   {SCSI_CMD_READ_DEFECT_DATA12, SCSI_XFER_AUTO,   NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_SELECT_CDROM_SPEED, SCSI_XFER_AUTO,   NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_READ_CD_MSF,      SCSI_XFER_TOHOST,   "READ CD MSF", SCSI_DONT_EMULATE},      \
   {SCSI_CMD_AUDIO_SCAN,       SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_SET_CDROM_SPEED,  SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_PLAY_CD,          SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {SCSI_CMD_MECH_STATUS,      SCSI_XFER_TOHOST,   "MECHANISM STATUS", SCSI_DONT_EMULATE}, \
   {SCSI_CMD_READ_CD,          SCSI_XFER_TOHOST,   "READ CD MSF", SCSI_DONT_EMULATE},      \
   {SCSI_CMD_SEND_DVD_STRUCTURE, SCSI_XFER_AUTO,   NULL, SCSI_DONT_EMULATE},	\
   {0xc0,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc1,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc2,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc3,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc4,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc5,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc6,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc7,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc8,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xc9,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xca,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xcb,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xcc,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xcd,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xce,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xcf,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd0,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd1,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd2,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd3,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd4,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd5,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd6,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd7,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd8,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xd9,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xda,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xdb,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xdc,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xdd,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xde,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xdf,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe0,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe1,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe2,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe3,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe4,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe5,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe6,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe7,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe8,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xe9,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xea,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xeb,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xec,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xed,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xee,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xef,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf0,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf1,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf2,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf3,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf4,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf5,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf6,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf7,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf8,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xf9,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xfa,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xfb,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},	\
   {0xfc,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0xfd,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0xfe,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               \
   {0xff,                      SCSI_XFER_AUTO,     NULL, SCSI_DONT_EMULATE},               

#endif
