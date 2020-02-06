t's in the Release Notes

The release notes cover the following topics:

* [Before You Begin](#beforeyoubegin)
* [Internationalization](#i18n)
* [Compatibility Notes](#compatibility)
* [Guest Operating System Customization Support](#guestop)
* [Interoperability Matrix](#interop)
* [Installation and Upgrades for This Release](#installupgrade)
* [Resolved Issues](#resolvedissues)
* [Known Issues](#knownissues)

## Before You Begin

**Important note about upgrading to ESXi 6.0 or later**

Resolution on incompatibility and general guidelines: While upgrading ESXi hosts to ESXi 6.0 or later, and using older versions of Horizon View Agent, refer to the knowledge base articles:

* [Connecting to View desktops with Horizon View Agent 5.3.5 or earlier hosted on ESXi 6.0 or later fails with a black screen.](http://kb.vmware.com/kb/2144438)
* [Connecting to View desktops with Horizon View Agent 6.0.x or 6.1.x hosted on ESXi 6.0 or later fails with a black screen.](http://kb.vmware.com/kb/2144518)
* [Connecting to View desktops with Horizon View Agent 6.1.x hosted on ESXi 6.0 or later fails with a black screen.](http://kb.vmware.com/kb/2144453)

## Internationalization

VMware Tools 11.0.5 is available in the following languages:

* English
* French
* German
* Spanish
* Italian
* Japanese
* Korean
* Simplified Chinese
* Traditional Chinese

## Compatibility Notes

* Starting with VMware Tools version 10.2.0, Perl script-based VMware Tools installation for FreeBSD has been discontinued. FreeBSD systems are supported only through the open-vm-tools packages directly available from FreeBSD package repositories. FreeBSD packages for open-vm-tools 10.1.0 and later are available from FreeBSD package repositories.

## Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

## Interoperability Matrix

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) of VMware Tools 11.0.0 can be applied to VMware Tools 11.0.5.

## Installation and Upgrades for this release

VMware Tools can be downloaded from the [Product Download](https://my.vmware.com/web/vmware/details?downloadGroup=VMTOOLS1105&productId=742) page.

The steps to install VMware Tools vary depending on your VMware product and the guest operating system you have installed. For general steps to install VMware Tools in most VMware products, see [General VMware Tools installation instructions (1014294)](http://kb.vmware.com/selfservice/search.do?cmd=displayKC&docType=kc&docTypeID=DT_KB_1_1&externalId=1014294).To set up productLocker to point to the shared datastore, see [KB 2004018](https://kb.vmware.com/kb/2004018).<br>
<br>
For specific instructions to install, upgrade, and configure VMware Tools, see the VMware Tools [Documentation](https://www.vmware.com/support/pubs/vmware-tools-pubs.html) page.

## Resolved Issues

* **DNS server is reported incorrectly as '127.0.0.53' when using systemd-resolved.** DNS server is reported incorrectly in GuestInfo as '127.0.0.53' , when the OS uses systemd-resolved. This issue is fixed in this release.

## Known Issues

* **In Linux guests, when /tmp is mounted with "noexec" option, any solution that depends on running a program or script stored under /tmp inside the guest using VMware Tools fails.** In Linux guests, when is mounted with "noexec" option, any solution that depends on running a program or script stored under inside the guest using VMware Tools fails.
 Workaround: Override TMPDIR environment variable for service to a different path with executable permissions. However, this workaround is lost due to guest OS or VMware Tools upgrade. In this version of VMware Tools, can be used to override the environment variables for instances as following: Note: Environment settings are applied when starts up. So, vmtoolsd needs restart for new settings to take effect.
* **Drag functionality fails to work in Ubuntu.** Drag functionality fails to work in Ubuntu 16.04.4 32-bit virtual machine installed using easy install. Also, failure of copy and paste functionality is observed in the same system. Note: This issue is applicable for VMware Tools running on Workstation and Fusion. Workaround:
  - Add the linux kernel boot option.
  - To gain access to larger resolutions, remove option from the file.
* **Shared Folders mount is unavailable on Linux VM.** If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, shared folders mount is not available on restart. Note: This issue is applicable for VMware Tools running on Workstation and Fusion. Workaround: If the VM is powered on, disable and enable the **Shared Folders** feature from the interface.<br>
For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.<br>
For example, add the line:

  vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0

