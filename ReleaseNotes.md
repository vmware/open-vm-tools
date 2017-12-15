# open-vm-tools 10.2.0 Release Notes

Updated on 14 Dec 2017
##What's in the Release Notes
The release notes cover the following topics: 

- What's New
- Internationalization
- Compatibility
- Installation and Upgrades for This Release
- Known Issues

## What's New

- **FreeBSD support**: freebsd.iso is not available for VMware Tools 10.2.0 and later as it has been discontinued in favor of open-vm-tools. For more information, see Compatibility Notes section of this release notes.

## Internationalization 
open-vm-tools 10.2.0 supports the following languages:

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
- open-vm-tools 10.2.0 is compatible with all supported versions of VMware vSphere ESXi 5.5 and later, VMware Workstation 14.0 and VMware Fusion 10.0. See VMware Compatibility Guide for more information.
- Starting with VMware Tools version 10.2.0, Perl script based VMware Tools installation for FreeBSD has been discontinued. Going forward, FreeBSD systems are supported only through the open-vm-tools packages directly available from FreeBSD package repositories. FreeBSD packages for open-vm-tools 10.1.0 and later are available from FreeBSD package repositories.

## Installation and Upgrades for This Release 
The steps to install open-vm-tools vary depending on your VMware product and the guest operating system you have installed. For general steps to install open-vm-tools in most VMware products, see https://github.com/vmware/open-vm-tools/blob/master/README.md

## Resolved Issues 

* **Summary page of the VM does not list the IP address of the VMs in the right order**

    The configuration option to exclude network interfaces from GuestInfo and set primary and low priority network interfaces is added to the tools.conf configuration file.
This issue is resolved in this release.

* **Guest authentication fails with a SystemError fault when the requested password is expired**

    Attempting to authenticate with an expired password, for example when attempting Guest Operations, fails with a SystemError fault.
This issue is resolved in this release. Authentication with an expired password now fails with an InvalidGuestLogin fault in order to provide a more precise error code for such a case.

* **The free space reported in vim.vm.GuestInfo.DiskInfo for a Linux guest does not match with df command in the guest**

    Prior to VMware Tools version 10.2.0, the free space reported in vim.vm.GuestInfo.DiskInfo for a Linux guest included file system specific reserved blocks. This led to guest file system usage in vSphere clients reporting more free space than what was reported by df command in the guest. This issue has been resolved in this release by not including the file system specific reserved blocks in the free space reported in vim.vm.GuestInfo.DiskInfo for Linux guests by default. The default behavior can be reversed with a configuration in ```/etc/vmware-tools/tools.conf``` file in the Linux guest operating systems:
```
    [guestinfo] 
    diskinfo-include-reserved=true
```

* **VMware user process might not restart after upgrades of open-vm-tools**

    When the VMware user process receives a SIGUSR2, it restarts itself by executing vmware-user and terminates itself. This is used on upgrades to ensure that the latest version of vmtoolsd is running. vmware-user was not available in open-vm-tools..
This issue is fixed in this release.

## Known Issues

* **Shared folder shows empty on Ubuntu 17.04 with open-vm-tools**.
    On rebooting Ubuntu 17.04 with open-vm-tools installed, the shared folders /mnt directory is empty. This issue is observed even after installing Ubuntu 17.04 using easy install, enabling shared folders in VM settings and selecting Always Enabled.
Workaround: Disable Shared Folders in the interface and enable after the VM is powered on with VMware Tools running.


