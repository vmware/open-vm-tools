#          Open-vm-tools 11.3.0 Release Notes

Updated on: 17 JUN 2021

Open-vm-tools | 17 JUN 2021 | Build 17901274

Check back for additions and updates to these release notes.

What's in the Release Notes
---------------------------

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Internationalization](#i18n)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Guest Operating System Customization Support](#guestop)
*   [Interoperability Matrix](#interop)
*   [Resolved Issues](#resolvedissues)
*   [Known Issues](#knownissues)

What's New
----------

*   A small command line tool, **vmwgfxctrl**, has been added to open-vm-tools for Linux that can be used to control various aspects of the vmwgfx Linux kernel module.  Currently it can both display and set the current topology of the vmwgfx kernel driver.  It is useful when trying to configure custom resolutions on recent Linux distributions, including multi-monitor setups.
*   A command line tool, **vmware-alias-import**, has been added to open-vm-tools that can be used to import vgauth config data and apply it to the running vgauth service.

Internationalization
--------------------

Open-vm-tools 11.3.0 is available in the following languages:

*   English
*   French
*   German
*   Spanish
*   Italian
*   Japanese
*   Korean
*   Simplified Chinese
*   Traditional Chinese

End of Feature Support Notice
-----------------------------

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.x release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes and no new feature support in these types of VMware Tools (tar tools and OSP's).  It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information on different types of VMware Tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

Guest Operating System Customization Support
--------------------------------------------

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

Interoperability Matrix
-----------------------

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products.

Resolved Issues
---------------

*   **Open-vm-tools does not report "per second rate" type of guest stats for Linux VMs with some non en-US locales.**
    
    For Linux VMs with some non en-US locales (such as es\_ES locale), open-vm-tools does not report "per second rate" type of guest stats on vROps. For example, Guest|Page In Rate per second.
    
    This issue is fixed in 11.3.0 open-vm-tools release. For earlier versions of open-vm-tools, the workaround is:
    
    Add export LANG=en\_US.UTF-8 in vmtoolsd daemon start script.
    
*   A number of Coverity reported issues have been addressed and some false positives have been annotated.

*   **The following issues and pull requests reported on [github.com/vmware/open-vm-tools](https://github.com/vmware/open-vm-tools) have been addressed:**
<br>
    > [https://github.com/vmware/open-vm-tools/issues/509](https://github.com/vmware/open-vm-tools/issues/509) <br>
    > [https://github.com/vmware/open-vm-tools/pull/505](https://github.com/vmware/open-vm-tools/pull/505) <br>
    > [https://github.com/vmware/open-vm-tools/issues/500](https://github.com/vmware/open-vm-tools/issues/500) <br>
    > [https://github.com/vmware/open-vm-tools/issues/481](https://github.com/vmware/open-vm-tools/issues/481) <br>
    > [https://github.com/vmware/open-vm-tools/pull/474](https://github.com/vmware/open-vm-tools/pull/474) <br>
    > [https://github.com/vmware/open-vm-tools/issues/446](https://github.com/vmware/open-vm-tools/issues/446) <br>

*   **A complete list of the granular changes that are in the open-vm-tools 11.3.0 release is available at:**
<br>
    > [https://github.com/vmware/open-vm-tools/blob/stable-11.3.0/open-vm-tools/ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-11.3.0/open-vm-tools/ChangeLog)

Known Issues
------------

**Open-vm-tools Issues in VMware Workstation or Fusion**

*   **Shared Folders mount is unavailable on Linux VM.**
    
    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.
    
    Note: This issue is applicable to open-vm-tools running on Workstation and Fusion.
    
    Workaround:
    
    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface.  For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:  

    `
    vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0
    `
    
*   **Drag functionality fails to work in Ubuntu 16.04.**
    
    Drag functionality fails to work in an Ubuntu 16.04.4 32-bit virtual machine installed using Easy Install.  Also, failure of the copy and paste functionality is observed in the same system.
    
    Note: This issue is applicable to open-vm-tools running on Workstation and Fusion.
    
    Workaround:
    
    *   Add the modprobe.blacklist=vmwgfx linux kernel boot option.
    *   To gain access to larger resolutions, remove svga.guestBackedPrimaryAware = "TRUE" option from the VMX file.

