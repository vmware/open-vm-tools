#                      open-vm-tools 12.3.5 Release Notes

Updated on: 26 October 2023

open-vm-tools | 26 OCTOBER 2023 | Build 22544099

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

- [open-vm-tools 12.3.5 Release Notes](#open-vm-tools-1235-release-notes)
	- [What's in the Release Notes](#whats-in-the-release-notes)
	- [What's New](#whats-new)
	- [End of Feature Support Notice](#end-of-feature-support-notice)
	- [Internationalization](#internationalization)
	- [Guest Operating System Customization Support](#guest-operating-system-customization-support)
	- [Interoperability Matrix](#interoperability-matrix)
	- [ Resolved Issues](#-resolved-issues)
	- [Known Issues](#known-issues)

## <a id="whatsnew" name="whatsnew"></a>What's New

*   This release resolves CVE-2023-34058. For more information on this vulnerability and its impact on VMware products, see https://www.vmware.com/security/advisories/VMSA-2023-0024.html.

*   This release resolves CVE-2023-34059 which only affects open-vm-tools.

*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

*   A complete list of the granular changes in the open-vm-tools 12.3.5 release is available at:

    [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.3.5/open-vm-tools/ChangeLog)

## <a id="endsupport" name="endsupport"></a>End of Feature Support Notice

*   Deprecated: Using "xml-security-c" and "xerces-c" to build the VMware Guest Authentication Service (VGAuth)

    Starting with open-vm-tools 12.4.0, and going forward, the VGAuth service build will require the "xmlsec1" and "libxml2" development and runtime packages.  If still using the "xml-security-c" and "xerces-c" open source projects to build open-vm-tools, now is the time to plan for the change.  The open-vm-tools 12.3.x series will be the last version that can use "xml-security-c" and "xerces-c".

## <a id="i18n" name="i18n"></a>Internationalization

open-vm-tools 12.3.5 is available in the following languages:

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

*   **This release resolves CVE-2023-34058.**

    For more information on this vulnerability and its impact on VMware products, see https://www.vmware.com/security/advisories/VMSA-2023-0024.html.

    open-vm-tools contains a SAML token signature bypass vulnerability. VMware has evaluated the severity of this issue to be in the Important severity range with a maximum CVSSv3 base score of 7.5 - CVSS:3.1/AV:A/AC:H/PR:N/UI:N/S:U/C:H/I:H/A:H

    A malicious actor that has been granted Guest Operation Privileges in a target virtual machine may be able to elevate their privileges if that target virtual machine has been assigned a more privileged Guest Alias.

    Note: While the description and known attack vectors are very similar to CVE-2023-20900, CVE-2023-34058 has a different root cause that must be addressed.

    A patch for earlier versions of open-vm-tools is available at [CVE-2023-34058.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-34058.patch).

*   **This release resolves CVE-2023-34059.**

    open-vm-tools contains a file descriptor hijack vulnerability in the vmware-user-suid-wrapper. VMware has evaluated the severity of this issue to be in the Important severity range with a maximum CVSSv3 base score of 7.4. - CVSS:3.1/AV:L/AC:H/PR:N/UI:N/S:U/C:H/I:H/A:H

    A malicious actor with non-root privileges may be able to hijack the /dev/uinput file descriptor allowing them to simulate user inputs.

    A patch for earlier versions of open-vm-tools is available at [CVE-2023-34059.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-34059.patch).

*   **The following github.com/vmware/open-vm-tools issue have been addressed**

    * Better cooperation between deployPkg plugin and cloud-init concerning location of 'disable_vmware_customization' flag.

      [Issue #310](https://github.com/vmware/open-vm-tools/issues/310)


## <a id="knownissues" name="knownissues"></a>Known Issues


*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Note: This issue is applicable to open-vm-tools running on VMware Workstation and VMware Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>