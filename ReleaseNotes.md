**Updated on: 19 SEPT 2019**

VMware Tools | 19 SEP 2019 | Build 14549434

Check for additions and updates to these release notes.

What's in the Release Notes
---------------------------

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Before You Begin](#beforeyoubegin)
*   [Internationalization](#i18n)
*   [Product Support Notice](#productsupport)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Compatibility Notes](#compatibility)
*   [Guest Operating System Customization Support](#guestop)
*   [Interoperability Matrix](#interop)
*   [Known Issues](#knownissues)

What's New
----------

*   Added appInfo to publish information about running applications inside the guest. For more details, see [VMware Tools Services](https://docs.vmware.com/en/VMware-Tools/11.0.0/com.vmware.vsphere.vmwaretools.doc/GUID-0BD592B1-A300-4C09-808A-BB447FAE2C2A.html).
*   Provided sample tool.conf for ease of administration. For details, see [Configuration File Location](https://docs.vmware.com/en/VMware-Tools/11.0.0/com.vmware.vsphere.vmwaretools.doc/GUID-EA16729B-43C9-4DF9-B780-9B358E71B4AB.html).

Before You Begin
----------------

**Important note about upgrading to ESXi 5.5 Update 3b or later**

Resolution on incompatibility and general guidelines: While upgrading ESXi hosts to ESXi 5.5 Update 3b or ESXi 6.0 Update 1 or later, and using older versions of Horizon View Agent, refer to the knowledge base articles:

*   [Connecting to View desktops with Horizon View Agent 5.3.5 or earlier hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144438)
*   [Connecting to View desktops with Horizon View Agent 6.0.x or 6.1.x hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144518)
*   [Connecting to View desktops with Horizon View Agent 6.1.x hosted on ESXi 6.0 Update 1 or later fails with a black screen.](http://kb.vmware.com/kb/2144453)

Internationalization
--------------------

open-vm-tools 11.0.0 is available in the following languages:

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

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.5 release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes and no new feature support in these types of VMware Tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information on different types of VMware Tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

Compatibility Notes
-------------------

*   Starting with VMware Tools version 10.2.0, Perl script-based VMware Tools installation for FreeBSD has been discontinued. FreeBSD systems are supported only through the open-vm-tools packages directly available from FreeBSD package repositories. FreeBSD packages for open-vm-tools 10.1.0 and later are available from FreeBSD package repositories.

Guest Operating System Customization Support
--------------------------------------------

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

Interoperability Matrix
-----------------------

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. 

Known Issues
------------

*   **Suspend Guest of Linux VM using any version of open-vm-tools may fail with some versions of SELinux.**
    
    A "Suspend Guest" operation on a Linux guest running any version of open-vm-tools and with SELinux enabled may stall and ultimately fail.
    
    The failure may appear as:  
      - a "Failed to suspend the virtual machine" message display.  
      - nothing happened and the "Suspend Guest" button is reactivated. IPv4 connections may be closed.  
      - a delayed suspend happens but the IPv4 addresses are lost when the VM is resumed.
    
    Even an apparent "stall" which exceeds 30 seconds is an indication of the problem.
    
    For more details, see [KB 74722](https://kb.vmware.com/s/article/74722).
    
    Workaround:
    
    Update the selinux-policy and selinux-policy-targeted packages to the latest version available from the Linux vendor.  If package updates are not available or if the issue persists, then consider the following workaround:
    
    Create an exemption for the vmtools/NetworkManager denied access by using the audit2allow command to generate a local loadable SELinux policy module as outlined in [KB 74722](https://kb.vmware.com/s/article/74722).
    
*   **Drag functionality fails to work in Ubuntu.**
    
    Drag functionality fails to work in Ubuntu 16.04.4 32-bit virtual machine installed using easy install. Also, failure of copy and paste functionality is observed in the same system.
    
    Note: This issue is applicable for open-vm-tools running on Workstation and Fusion.
    
    Workaround:
    
    *   Add the modprobe.blacklist=vmwgfx linux kernel boot option.
    *   To gain access to larger resolutions, remove svga.guestBackedPrimaryAware = "TRUE" option from the VMX file.
*   **Shared Folders mount is unavailable on Linux VM.**
    
    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, shared folders mount is not available on restart.
    
    Note: This issue is applicable for open-vm-tools running on Workstation and Fusion.
    
    Workaround:
    
    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface.  
    For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  
    For example, add the line:  
    vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow\_other    0    0
    

