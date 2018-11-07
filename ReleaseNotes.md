open-vm-tools 10.3.5 Release Notes
=================================

**Updated on: 01 Nov 2018**

open-vm-tools | 01 NOV 2018 | Build 10430147

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

open-vm-tools 10.3.5 is available in the following languages:

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
*   open-vm-tools 10.3.5 freezes feature support for tar tools and OSPs   
    The tar tools (linux.iso) and OSPs shipped with open-vm-tools 10.3.5 release will continue to be supported. However, releases after open-vm-tools 10.3.5 will only include critical and security fixes and no new feature support in these types of open-vm-tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information on different types of open-vm-tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

Guest Operating System Customization Support
--------------------------------------------

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

Interoperability Matrix
-----------------------

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. 

Resolved Issues
---------------

*   **While running a quiesced snapshot of a Linux guest on the vSphere hosts earlier than version 6.7, open-vm-tools logs warning messages.**
    
    open-vm-tools logs warning messages like the following:
    
    \[<date> <time>\] \[ warning\] \[vmbackup\] Failed to send vmbackup event: vmbackup.eventSet req.genericManifest 0 /etc/vmware-tools/quiesce\_manifest.xml, result: Error processing event.
    
    \[<date> <time>\] \[ warning\] \[vmbackup\] Error trying to send VMBACKUP\_EVENT\_GENERIC\_MANIFEST
    
    The warning messages are logged by default or if vmsvc.level is set to warning or higher in the tools.conf file. Note that this is purely a logging issue and that quiesced snapshot still works even when these messages appear.
    
    This issue is fixed in this release. The warning message has been reworded for clarity and is no longer logged on every quiesced snapshot, but instead only on the first quiesced snapshot after a power on or vMotion.
    
*   **CreateTemporaryFileInGuest/CreateTemporaryDirectoryInGuest returns a file path that does not exist.**
    
    When guest authentication user name is set to <domain>\\<user> and the user's profile directory does not exist in the guest file system, Win32 LoadUserProfile() creates a temporary user profile directory "C:\\Users\\TEMP" when the guest operation starts. After the guest operation completes, Win32 UnloadUserProfile() deletes this directory and the temporary file in the said directory.
    
    This issue is fixed in this release. CreateTemporaryFileInGuest/CreateTemporaryDirectoryInGuest now returns "C:\\Windows\\TEMP\\xxxxxx".
    
*   **Excessive spikes of the Memory/Workload percentage metric, in a vRealize Operations cluster.**
    
    Guest statistics "Guest | Page in rate per second" provided by open-vm-tools is not properly handling 32-bit unsigned integer overflow on 32-bit Linux guests.
    
    This issue is fixed in this release.
    
*   **open-vm-tools Service running as vmusr crashes on Linux systems which are not running on the VMware platform.**
    
    When a user logs in to the Linux desktop UI of a Linux OS that packages open-vm-tools and is not running on the VMware platform, "/usr/bin/vmtoolsd -n vmusr" process generates a coredump.
    
    This issue is fixed in this release.


FreeBSD Drivers
---------------

*   **vmxnet and vmxnet3 drivers have been removed from open-vm-tools for FreeBSD**

    The vmxnet (version 1) network driver is not supported by any currently
    supported VMware virtualization platform and has been removed for
    FreeBSD from the open-vm-tools source.

    FreeBSD has their own vmxnet3 network driver based on community source
    and has never made use of the vmxnet3 source code or drivers from
    VMware. The unneeded FreeBSD vmxnet3 source has been removed from open-vm-tools.


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
    
