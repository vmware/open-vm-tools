#open-vm-tools 11.2.0 Release Notes

**Updated on: 15 OCT 2020**

Open-vm-tools | 15 OCT 2020 | Build 16938113

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

*   Fixed issues mentioned in [Resolved Issues](#resolvedissues) section.

End of Feature Support Notice
-----------------------------

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.x release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes and no new feature support in these types of VMware Tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information on different types of VMware Tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

Guest Operating System Customization Support
--------------------------------------------

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

Interoperability Matrix
-----------------------

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. 

Resolved Issues
---------------

*   **In open-vm-tools (Linux only), a memory leak occurs in disk device mapping information for IDE, SATA or SAS (LSI Logic SAS) disks configured in the guest.** 
    
    Due to this issue, the memory usage of vmtoolsd system service gradually increases, which may impact system performance. This problem does not occur when mapping SCSI or NVMe devices.
    
    This issue is fixed in this release.
    
*   **The following issues and pull requests reported on https://github.com/vmware/open-vm-tools have been addressed:**

        https://github.com/vmware/open-vm-tools/issues/429
        https://github.com/vmware/open-vm-tools/pull/431
        https://github.com/vmware/open-vm-tools/pull/432
        https://github.com/vmware/open-vm-tools/issues/452

*   **A number of Coverity reported errors and false positives have been addressed.**

*   **A complete list of the granular changes that are in the open-vm-tools 11.2.0 release is available at:**

        https://github.com/vmware/open-vm-tools/blob/stable-11.2.x/open-vm-tools/ChangeLog

    The changes after March 31, 2020 are all the changes that have gone into the "devel" branch of open-vm-tools since the time that the "stable-11.1.x" branch was spun off.  Note these changes may include changes that were also released in the open-vm-tools 11.1.5 release.

Known Issues
------------

The known issues are grouped as follows.

**Open-vm-tools Issues in VMware Workstation or Fusion**

*   **Shared Folders mount is unavailable on Linux VM.**
    
    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, shared folders mount is not available on restart.
    
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


