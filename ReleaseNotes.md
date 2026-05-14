#                      open-vm-tools 13.1.0 Release Notes

Updated on: 12 May 2026

open-vm-tools | 12 MAY 2026 | Build 25218885

Check back for additions and updates to these release notes.

## What is in the Release Notes

The release notes cover the following topics:

* [What's New](#whatsnew) 
* [Internationalization](#i18n) 
* [Compatibility Notes](#compat)
* [Guest Operating System Customization Support](#guestop) 
* [Interoperability Matrix](#interop) 
* [Resolved Issues](#resolvedissues) 
* [Known Issues](#knownissues)

## <a id="whatsnew" name="whatsnew"></a>What's New

#### Support for GNOME Toolkit version 4 - GTK4.

The open-vm-tools release 13.1.0 introduces support for the GNOME Toolkit version 4.  Providers of open-vm-tools can choose to build the desktop plugins using GTK4 or continue to build using GTK3.  The choice of GTK version to be used is made with the one of the following options to the configure script

* __--with-gtk4__        # build only with GTK4; configure error if not available.
* __--with-gtk3__        # build only with GTK3; configure error if not available.

If neither option is provided, the configure script will attempt to determine if the required pieces of GTK4 or GTK3 are available on the system.   If successful, the open-vm-tools build will be configured to use the highest available version.

If no GTK development and runtime is found on the system, the configure script will terminate with a short message with details in the "config.log" file.

Support for GTK2 has been removed from the open-vm-tools source.

See the [Known Issues](#knownissues) section below for GTK4 related issues that are being tracked.

#### Additional Changes.

*   New release of the Salt-Minion integration script.

    Picking up the latest release of the svtminion.sh script - version v2026.01.09

*   Fallback to ignore systemd inhibitors during guest poweroff / reboot.

    If the system() shutdown command fails due to any system inhibitors, a fallback "systemctl shutdown/reboot" is executed while ignoring the systemd inhibitors.

*   Two issues reported on github have been fixed.  Please see the [Resolved Issues](#resolvedissues) section below.

*   Various fixes for critical issues.

*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

*   A complete list of the granular changes in the open-vm-tools 13.1.0 release is available at:

    [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-13.1.0/open-vm-tools/ChangeLog)

## <a id="i18n" name="i18n"></a>Internationalization

open-vm-tools 13.1.0 is available in the following languages:

* English
* French
* Japanese
* Spanish

## <a id="compat" name="compat"></a>Compatibility Notes

* **VMware Product and Hardware Version Compatibility**

This release of open-vm-tools is compatible with all supported versions of VMware products and with different hardware versions supported by VMware products.  For more information about the officially supported VMware product compatibility, see the [Broadcom Interoperability Matrix](https://interopmatrix.broadcom.com/Interoperability).

* **Guest Operating System Compatibility**

This release of open-vm-tools is compatible with a wide range of guest OS versions supported by VMware products. For more information about the officially supported Linux guest OS versions, see the Broadcom knowledge base article [313371](https://knowledge.broadcom.com/external/article/313371).

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](https://compatibilityguide.broadcom.com/search?program=software&persona=live&customization=Guest+Customization&column=osVendors&order=asc) provides details about the guest operating systems that are supported for customization.


## <a id="interop" name="interop"></a>Interoperability Matrix

The [Broadcom Interoperability Matrix](https://interopmatrix.broadcom.com/Interoperability) provides details about the compatibility of current and earlier versions of VMware Products. 

## <a id="resolvedissues" name ="resolvedissues"></a> Resolved Issues

*   **Missing VGAuthLib vmsg files.**

    [issue #707](https://github.com/vmware/open-vm-tools/issues/707) vmware-vgauth-smoketest: no VGAuthLib.vmsg file

    The vgauth/lib/Makefile.am has been corrected to install the expected VGAuthLib vmsg files.

    This issue is resolved in this release.

*   **Guest customization fails to recognize cloudinit "disable_vmware_customization" flag in the presence of a comment.**

    [issue #763](https://github.com/vmware/open-vm-tools/issues/763) Inline comment breaks "disable_vmware_customization" check

    The regex check for "disable_vmware_customization" was too strict and fails when there are inline comments after the flag.

    This issue is resolved in this release.

## <a id="knownissues" name="knownissues"></a>Known Issues

*   **General list of GTK4 related issues.**

    The following issues identified when testing with GTK4 are being tracked.
    * In general, UI features work better when using Xorg/X11 rather than Wayland.
    * CopyPaste is more stable than Drag and Drop (DnD). It is recommended to use CopyPaste where possible.
    * DnD from a guest to the host may block with the cursor at the guest VM's desktop edge.
    * DnD of RTF text from a guest to the host loses the RTF formatting information.
    * DnD is working with Windows as a host; Linux and MacOS hosts experience issues.

*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Note: This issue is applicable to open-vm-tools running on VMware Workstation and VMware Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>

    For more information on how to configure open-vm-tools Shared Folders, see [KB 60262](https://knowledge.broadcom.com/external/article?legacyId=60262).
