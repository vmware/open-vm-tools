**Updated on: 29 MAR 2018**

open-vm-tools | 29 MAR 2018 | Build 8068406

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

*   **Quiesced snapshots**: Ability to exclude specific file systems from quiesced snapshots on Linux guest operating systems. This configuration can be set in the tools configuration file. For more details, see the VMware Tools [Documentation](https://docs.vmware.com/en/VMware-Tools/index.html) page.
*   **Disable display mode setting**: A configuration option is introduced to disable normal display mode setting functionality using open-vm-tools. For more details, see [KB 53572](https://kb.vmware.com/s/article/53572).
*   **Resolved Issues: **This release of open-vm-tools resolves few issues which are documented in the [Resolved Issues](#resolvedissues) section of this release notes.

Before You Begin
----------------

Important note about upgrading to ESXi 5.5 Update 3b or later

Resolution on incompatibility and general guidelines: While upgrading ESXi hosts to ESXi 5.5 Update 3b or ESXi 6.0 Update 1 or later, and using older versions of Horizon View Agent, refer to the knowledge base articles:

*   [Connecting to View desktops with Horizon View Agent 5.3.5 or earlier hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144438)
*   [Connecting to View desktops with Horizon View Agent 6.0.x or 6.1.x hosted on ESXi 5.5 Update 3b or later fails with a black screen.](http://kb.vmware.com/kb/2144518)
*   [Connecting to View desktops with Horizon View Agent 6.1.x hosted on ESXi 6.0 Update 1 or later fails with a black screen.](http://kb.vmware.com/kb/2144453)

Internationalization
--------------------

open-vm-tools 10.2.5 is available in the following languages:

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

*   open-vm-tools 10.2.5 is compatible with supported versions of VMware vSphere ESXi 5.5 and later, VMware Workstation 14.0 and VMware Fusion 10.0. See [VMware Compatibility Guide](http://www.vmware.com/resources/compatibility/search.php) for more information.
*   Starting with open-vm-tools version 10.2.0, Perl script-based open-vm-tools installation for FreeBSD has been discontinued. FreeBSD systems are supported only through the open-vm-tools packages directly available from FreeBSD package repositories. FreeBSD packages for open-vm-tools 10.1.0 and later are available from FreeBSD package repositories.

### Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

Resolved Issues
---------------

*   **open-vm-tools 10.2.0 does not recognize UFS filesystem partitions**
    
    open-vm-tools 10.2.0 has dropped UFS from the list of known file system type. As a result, the default filesystem of Solaris and FreeBSD is not recognized. open-vm-tools Services in the GuestInfo for the virtual machine do not report these filesystems. You might not be able to monitor the disk usage of UFS filesystems with vRealize Operations or vCenter Managed Object Browser.
    
    This issue is resolved in this release.
    
*   **Information about non-existing device mounted to a file system was not reported**
    
    Few Linux guest operating systems might have a non-existing device mounted to a filesystem. For example /dev/root/. open-vm-tools does not report this information.
    
    This issue is resolved in this release.

