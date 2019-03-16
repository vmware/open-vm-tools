open-vm-tools 10.3.10 Release Notes
=================================

**Updated on: 14 MAR 2019**

open-vm-tools | 14 MAR 2019 | Build 12406962

Check for additions and updates to these release notes.

What's in the Release Notes
---------------------------

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Before You Begin](#beforeyoubegin)
*   [Internationalization](#i18n)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Guest Operating System Customization Support](#guestop)
*   [Interoperability Matrix](#interop)
*   [Resolved Issues](#resolvedissues)
*   [Known Issues](#knownissues)

What's New
----------

*   **Resolved Issues: **There are some issues that are resolved in this release of open-vm-tools which are documented in the [Resolved Issues](#resolvedissues) section of this release notes.

Before You Begin
----------------

**Important note about upgrading to ESXi 5.5 Update 3b or later**

Resolution on incompatibility and general guidelines: While upgrading ESXi hosts to ESXi 5.5 Update 3b or ESXi 6.0 Update 1 or later, and using older versions of Horizon View Agent, refer to the knowledge base articles:

*   [Connecting to View desktops with Horizon View Agent 5.3.5 or earlier hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144438)
*   [Connecting to View desktops with Horizon View Agent 6.0.x or 6.1.x hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144518)
*   [Connecting to View desktops with Horizon View Agent 6.1.x hosted on ESXi 6.0 Update 1 or later fails with a black screen.](http://kb.vmware.com/kb/2144453)

Internationalization
--------------------

open-vm-tools 10.3.10 is available in the following languages:

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

*   Support for Common Agent Framework (CAF) will be removed in the next major release of open-vm-tools.
*   VMware Tools 10.3.5 freezes feature support for tar tools and OSPs   
    The tar tools (linux.iso) and OSPs shipped with open-vm-tools 10.3.5 release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes and no new feature support in these types of open-vm-tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information on different types of open-vm-tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

Guest Operating System Customization Support
--------------------------------------------

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

Interoperability Matrix
-----------------------

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. 

Resolved Issues
---------------

*   **In certain cases, quiesced snapshots on Linux guests do not include backup manifests.**

    On a Linux guest, if VMware Tools 10.3.5 gets an error when notifying the ESXi host of a quiesced snapshot's backup manifest file, VMware Tools logs an error and does not notify the ESXi host of the backup manifest file on subsequent quiesced snapshots. As a result, some quiesced snapshots do not include the backup manifest file, that would otherwise be available on the ESXi host. Such snapshots are not identified as quiesced by vSphere clients.

    This issue is fixed in this release.

Known Issues
------------

*   **Drag functionality fails to work in Ubuntu.**
    
    Drag functionality fails to work in Ubuntu 16.04.4 32-bit virtual machine installed using easy install. Also, failure of copy and paste functionality is observed in the same system.
    
    Workaround:
    
    *   Add the modprobe.blacklist=vmwgfx linux kernel boot option.
    *   To gain access to larger resolutions, remove svga.guestBackedPrimaryAware = "TRUE" option from the VMX file.

*   **Shared Folders mount is unavailable on Linux VM.**
    
    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, shared folders mount is not available on restart.
    
    Workaround:
    
    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface.
    
    For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.
    
    For example, add the line:
    
    vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow\_other    0    0
    
