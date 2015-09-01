#open-vm-tools 10.0.0 Release Notes 

Updated on 1 SEP 2015
##What's in the Release Notes
The release notes cover the following topics: 

- What's New
- Internationalization
- Compatibility
- Installation and Upgrades for This Release
- Known Issues

##What's New 
VMware Tools is a suite of utilities that enhances the performance of the virtual machine's guest operating system and improves management of the virtual machine. Read about the new and enhanced features in this release below:

- **Common versioning**: Infrastructure changes to enable reporting of the true version of open-vm-tools. This feature is dependent on host support. 
- **Quiesced snapshots enhancements for Linux guests running IO workload**: Robustness related enhancements in quiesced snapshot operation. The _vmtoolsd_ service supports caching of log messages when guest IO has been quiesced. Enhancements in the _vmbackup_ plugin use a separate thread to quiesce the guest OS to avoid timeout issues due to heavy I/O in the guest. 
- **Shared Folders**: For Linux distributions with kernel version 4.0.0 and higher, there is a new FUSE based Shared Folders client which is used as a replacement for the kernel mode client. 
- **ESXi Serviceability**: Default _vmtoolsd_ logging is directed to a file instead of syslog.  _vmware-toolbox-cmd_ is enhanced for setting _vmtoolsd_ logging levels.
- **GuestInfo Enhancements**: Plugin enhancements to report more than 64 IP addresses from the guest. These enhancements will be available only after upgrading the host because the guest IP addresses limit also exists on the host side.

## Internationalization 
open-vm-tools 10.0.0 supports the following languages:

- English 
- French 
- German 
- Spanish 
- Italian 
- Japanese 
- Korean 
- Simplified Chinese 
- Traditional Chinese

## Compatibility 
open-vm-tools 10.0.0 is compatible with all supported versions of VMware vSphere, VMware Workstation 12.0 and VMware Fusion 8.0.
## Installation and Upgrades for This Release 
The steps to install open-vm-tools vary depending on your VMware product and the guest operating system you have installed. For general steps to install open-vm-tools in most VMware products, see https://github.com/vmware/open-vm-tools/blob/master/README.md
## Known Issues 
The known issues are as follows:

- **The status of IPv6 address is displayed as "unknown"**

	The status of IPv6 address from vim-cmd is displayed as "unknown" even when the address is valid.

	Workaround: None 
- **TextCopyPaste between host and guest systems fail**

	Copy and Paste of text between host and guest systems fail if the text size 50KB or higher.
 
	Workaround: Copy and Paste smaller amounts of text. 
- **Definition of the field _ipAddress_ in guestinfo is ambiguous**

	The field _ipAddress_ is defined as "Primary IP address assigned to the guest operating system, if known".
 
	Workaround: The field _ipAddress_ in this context for Linux is defined as the first IP address fetched by open-vm-tools.
