#                      open-vm-tools 12.3.0 Release Notes

Updated on: 31 August 2023

open-vm-tools | 31 AUGUST 2023 | Build 22234872

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

- [open-vm-tools 12.3.0 Release Notes](#open-vm-tools-1230-release-notes)
	- [What's in the Release Notes](#whats-in-the-release-notes)
	- [What's New](#whats-new)
	- [End of Feature Support Notice](#end-of-feature-support-notice)
	- [Internationalization](#internationalization)
	- [Guest Operating System Customization Support](#guest-operating-system-customization-support)
	- [Interoperability Matrix](#interoperability-matrix)
	- [ Resolved Issues](#-resolved-issues)
	- [Known Issues](#known-issues)

## <a id="whatsnew" name="whatsnew"></a>What's New

This release resolves CVE-2023-20900. For more information on this vulnerability and its impact on VMware products, see https://www.vmware.com/security/advisories/VMSA-2023-0019.html.

*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

*   A complete list of the granular changes in the open-vm-tools 12.3.0 release is available at:

    [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.3.0/open-vm-tools/ChangeLog)

## <a id="endsupport" name="endsupport"></a>End of Feature Support Notice

*   Deprecated: Using "xml-security-c" and "xerces-c" to build the VMware Guest Authentication Service (VGAuth)

    Starting with open-vm-tools 12.4.0, and going forward, the VGAuth service build will require the "xmlsec1" and "libxml2" development and runtime packages.  If still using the "xml-security-c" and "xerces-c" open source projects to build open-vm-tools, now is the time to plan for the change.  The open-vm-tools 12.3.x series will be the last version that can use "xml-security-c" and "xerces-c".

## <a id="i18n" name="i18n"></a>Internationalization

open-vm-tools 12.3.0 is available in the following languages:

* English
* French
* German
* Spanish
* Italian
* Japanese
* Korean
* Simplified Chinese
* Traditional Chinese

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

## <a id="interop" name="interop"></a>Interoperability Matrix

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. 

## <a id="resolvedissues" name ="resolvedissues"></a> Resolved Issues

*   **This release resolves CVE-2023-20900.**

    For more information on this vulnerability and its impact on VMware products, see https://www.vmware.com/security/advisories/VMSA-2023-0019.html.

*   **Linux quiesced snapshot: "SyncDriver: failed to freeze '_filesystem_'"**

    The open-vm-tools 12.2.0 release had an update to the Linux quiesced snapshot operation that would avoid starting a quiesced snapshot if a filesystem had already been frozen by another process.  See the [Resolved Issues](https://github.com/vmware/open-vm-tools/blob/stable-12.2.0/ReleaseNotes.md#-resolved-issues) section in the open-vm-tools 12.2.0 Release Notes.   That fix may have been backported into earlier versions of open-vm-tools by Linux vendors.  

    It is possible that filesystems are being frozen in custom pre-freeze scripts to control the order in which those specific filesystems are to be frozen.  The vmtoolsd process **must be informed** of all such filesystems with the help of "excludedFileSystems" setting of tools.conf.

    ```
    [vmbackup]

    excludedFileSystems=/opt/data,/opt/app/project-*,...
    ```

    A temporary workaround is available (starting from open-vm-tools 12.3.0) for system administrators to quickly allow a quiescing operation to succeed until the "excludedFileSystems" list can be configured.  Note, if another process thaws the file system while a quiescing snapshot operation is ongoing, the snapshot may be compromised.  Once the "excludedFileSystems" list is configured this setting MUST be unset (or set to false).

    ```
    [vmbackup]

    ignoreFrozenFileSystems = true
    ```

    This workaround is provided in the source file changes in 

        https://github.com/vmware/open-vm-tools/commit/60c3a80ddc2b400366ed05169e16a6bed6501da2

    and at Linux vendors' discretion, may be backported to earlier versions of open-vm-tools.

*   **A number of Coverity reported issues have been addressed.**

*   **Component Manager / salt-minion: New InstallStatus "UNMANAGED".**

    Salt-minion added support for "ExternalInstall" (106) to indicate an older version of salt-minion is installed on the vm and cannot be managed by the svtminion.* scripts.  The Component Manager will track that as "UNMANAGED" and take no action.

*   **The following pull requests and issues have been addressed**

    * Add antrea and calico interface pattern to GUESTINFO_DEFAULT_IFACE_EXCLUDES

      [Issue #638](https://github.com/vmware/open-vm-tools/issues/638)  
      [Pull request #639](https://github.com/vmware/open-vm-tools/pull/639)

    * Invalid argument with "\\" in Linux username (Active Directory user)

      [Issue #641](https://github.com/vmware/open-vm-tools/issues/641)

    * Improve POSIX guest identification

      [Issue #647](https://github.com/vmware/open-vm-tools/issues/647)  
      [Issue #648](https://github.com/vmware/open-vm-tools/issues/648)

    * Remove appUtil library which depends on deprecated "gdk-pixbuf-xlib"

      [Issue #658](https://github.com/vmware/open-vm-tools/issues/658)

    * Fix build problems with grpc

      [Pull request #664](https://github.com/vmware/open-vm-tools/pull/664)  
      [Issue #676](https://github.com/vmware/open-vm-tools/issues/676)

## <a id="knownissues" name="knownissues"></a>Known Issues


*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Note: This issue is applicable to open-vm-tools running on VMware Workstation and VMware Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>