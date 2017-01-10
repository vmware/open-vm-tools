#open-vm-tools 10.1.0 Release Notes 

Updated on 26 OCT 2016
##What's in the Release Notes
The release notes cover the following topics: 

- What's New
- Internationalization
- Compatibility
- Installation and Upgrades for This Release
- Known Issues

##What's New 
VMware Tools is a suite of utilities that enhances the performance of the virtual machine's guest operating system and improves management of the virtual machine. Read about the new and enhanced features in this release below:

- **vmware-namespace-cmd**: Added vmware-namespace-cmd command line utility that exposes set/get commands for the namespace database in the VMX.
- **gtk3 support**: open-vm-tools has been updated to use gtk3 libraries.
- **Common Agent Framework (CAF)**: CAF provides the basic services necessary to simplify secure and efficient management of agents inside virtual machines.
- **xmlsec1**: Changed guest authentication to xmlsec1.
- **FreeBSD**: Changes to support open-vm-tools on FreeBSD.
- **Automatic Linux Kernel Modules**: Automatic rebuilding of kernel modules is enabled by default.
- **New sub-command**: Added a new sub-command to push updated network information to the host on demand.
- **udev-rules**: Added udev rules for configuring SCSI timeout in the guest.
- **Ubuntu 16.10**: Fixes for running on Ubuntu 16.10.
- **Quiesced Snapshot**: Fix for quiesced snapshot failure leaving guest file system quiesced.

## Internationalization 
open-vm-tools 10.1.0 supports the following languages:

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
open-vm-tools 10.1.0 is compatible with all supported versions of VMware vSphere, VMware Workstation 12.5 and VMware Fusion 8.5.
## Installation and Upgrades for This Release 
The steps to install open-vm-tools vary depending on your VMware product and the guest operating system you have installed. For general steps to install open-vm-tools in most VMware products, see https://github.com/vmware/open-vm-tools/blob/master/README.md
## Known Issues 
The known issues are as follows:

- **Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04.**

    Common Agent Framework in open-vm-tools fails to build in Ubuntu 14.04 with rabbitmq-c version lower than 0.8.0

    Workaround: Upgrade rabbitmq-c to version 0.8.0 or higher for TLS1.2 support in Common Agent Framework

- **vmusr plug-ins do not load on Solaris 10 Update 11.**

    While running VMware Tools 10.1 on Solaris 10 U11 guest operating systems, the following vmusr plug-ins are not loaded:
        libdesktopEvents.so
        libdndcp.so
        libresolutionSet.so

    This issue might also occur in Solaris version 11.2.

    Workaround: Upgrade to Solaris 11.3. 

