open-vm-tools 10.3.0 Release Notes
==================================

**Updated on: 12 JUL 2018**

open-vm-tools | 12 JUL 2018 | Build 8931395

Check for additions and updates to these release notes.

What's in the Release Notes
---------------------------

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Before You Begin](#beforeyoubegin)
*   [Internationalization](#i18n)
*   [Compatibility Notes](#compatibility)
*   [Resolved Issues](#resolvedissues)
*   [Known Issues](#knownissues)

What's New
----------

*   Starting with 10.3.0, open-vm-tools builds with xmlsec1 by default (instead of building with xml-security). To revert to the old behavior and build with xml-security, use the option
    '--enable-xmlsecurity' for the ./configure command.

Before You Begin
----------------

**Important note about upgrading to ESXi 5.5 Update 3b or later**

General guidelines: While upgrading ESXi hosts to ESXi 5.5 Update 3b or ESXi 6.0 Update 1 or later, and using older versions of Horizon View Agent, refer to the knowledge base articles:

*   [Connecting to View desktops with Horizon View Agent 5.3.5 or earlier hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144438)
*   [Connecting to View desktops with Horizon View Agent 6.0.x or 6.1.x hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144518)
*   [Connecting to View desktops with Horizon View Agent 6.1.x hosted on ESXi 6.0 Update 1 or later fails with a black screen.](http://kb.vmware.com/kb/2144453)

Internationalization
--------------------

open-vm-tools 10.3.0 is available in the following languages:

*   English
*   French
*   German
*   Spanish
*   Italian
*   Japanese
*   Korean
*   Simplified Chinese
*   Traditional Chinese

Compatibility Notes
-------------------

*   As of tools release 10.2.0, FreeBSD guests are supported only by open-vm-tools; support for the VM Tools binary package supplied directly by VMware has been discontinued for FreeBSD.  Binary packages for open-vm-tools 10.1.0 and later are available from FreeBSD package repositories.

### Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.


Resolved Issues
---------------

*   **The open-vm-tools process might take a long time and consume 100% CPU of a core in a Linux OS with many IPv6 routes**
    
    Prior to open-vm-tools 10.3.0, gathering network adapter information in a Linux guest OS with many IPv6 routes was a time-consuming process with 100% use of the CPU of a core. The exported data contained only a maximum of 100 routes. IPv4 routes took precedence over IPv6, leading to data loss in reporting IPv6 routes. If there were more than 100 IPv4 routes, IPv6 routes were not reported.
    
    This performance issue has been resolved in this release. The default routes gathering behavior can be overridden by configuring the values in the /etc/vmware-tools/tools.conf file:  
      
    \[guestinfo\]  
    max-ipv4-routes=0  
    max-ipv6-routes=0  
      
    Note: If they are not manually set, or an invalid value (over 100 or less than 0) is set, 'max-ipv4-routes' and 'max-ipv6-routes' are set to 100 by default. They can be set to 0 to disable the data collection.
    
    This issue is resolved in this release.
    
*   **Installation of the libvmtools package might fail the installation of VMware Tools**
    
    When the package "libvmtools0" is installed in SUSE Linux 12 and open-vm-tools is not installed, the VMware Tools installer fails. This is done to prevent an incomplete installation. Users have to uninstall both open-vm-tools and libvmtools0 packages to install VMware Tools.
    
Known Issues
------------

*   **Drag and Drop  functionality fails to work in Ubuntu**
    
    Drag and Drop functionality fails to work in Ubuntu 16.04.4 32-bit virtual machines installed using easy install. Also, failure of copy and paste functionality is observed in the same system.
    
    Workaround:
    
    *   Add the modprobe.blacklist=vmwgfx linux kernel boot option.
    *   To gain access to larger resolutions, remove svga.guestBackedPrimaryAware = "TRUE" option from the VMX file.

*   **Shared Folders mount is unavailable on Linux VM**
    
    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, shared folders mount is not available upon restart.
    
    Workaround: If the VM is powered on, disable and enable the **Shared Folders** feature from the interface.
    
    For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.
    
    For example, add the line:
    
    vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0
