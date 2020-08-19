#open-vm-tools 11.1.5 Release Notes

**Updated on: 18 AUG 2020**

Open-vm-tools | 18 AUG 2020 | Build 16724464

Check for additions and updates to these release notes.

What's in the Release Notes
---------------------------

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Guest Operating System Customization Support](#guestop)
*   [Interoperability Matrix](#interop)
*   [Resolved Issues](#resolvedissues)
*   [Known Issues](#knownissues)

What's New
----------

*   For issues fixed in this version, refer to [Resolved Issues](#resolvedissues) section.

End of Feature Support Notice
-----------------------------

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.x release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes; there will be no new feature support in these types of VMware Tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information about different types of VMware Tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

Guest Operating System Customization Support
--------------------------------------------

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

Interoperability Matrix
-----------------------

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products.

Resolved Issues
---------------

*   **In open-vm-tools, the sdmp-plugin scripts are updated to use the 'ss' command instead of the 'netstat' command to get inter-service communication information.**
    
    The 'netstat' command is deprecated and is unavailable in some new releases of Linux VMs. The credential-less service discovery feature by the vRealize Operations Manager product cannot be used in such VMs. To avoid this issue, the sdmp-plugin scripts are now updated to use the 'ss' command instead of the 'netstat' command.
    
    This issue is fixed in this release.

*   **The following issues reported on https://github.com/vmware/open-vm-tools/issues have been addressed:**

        https://github.com/vmware/open-vm-tools/issues/451
        https://github.com/vmware/open-vm-tools/issues/429
        https://github.com/vmware/open-vm-tools/issues/428

    These issues are fixed in this release.

Known Issues
------------

**Open-vm-tools Issues in VMware Workstation or Fusion**

*   **Shared Folders mount is unavailable on Linux VM.**
    
    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.
    
    Note: This issue is applicable for open-vm-tools running on Workstation and Fusion.
    
    Workaround:
    
    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface.  
    For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  
    For example, add the line:  
    vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow\_other    0    0
    
*   **Drag functionality fails to work in Ubuntu.**
    
    Drag functionality fails to work in Ubuntu 16.04.4 32-bit virtual machine installed using easy install. Also, failure of copy and paste functionality is observed in the same system.
    
    Note: This issue is applicable for open-vm-tools running on Workstation and Fusion.
    
    Workaround:
    
    *   Add the modprobe.blacklist=vmwgfx linux kernel boot option.
    *   To gain access to larger resolutions, remove svga.guestBackedPrimaryAware = "TRUE" option from the VMX file.

