#                      open-vm-tools 12.5.0 Release Notes

Updated on: 8 October 2024

open-vm-tools | 8 OCTOBER 2024 | Build 24276846

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

* [What's New](#whatsnew) 
* [Internationalization](#i18n) 
* [Product Support Notice](#suppnote)
* [Guest Operating System Customization Support](#guestop) 
* [Interoperability Matrix](#interop) 
* [Resolved Issues](#resolvedissues) 
* [Known Issues](#knownissues)

## <a id="whatsnew" name="whatsnew"></a>What's New


*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

*   A complete list of the granular changes in the open-vm-tools 12.5.0 release is available at:

    [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.5.0/open-vm-tools/ChangeLog)

## <a id="i18n" name="i18n"></a>Internationalization

open-vm-tools 12.5.0 is available in the following languages:

* English
* French
* German
* Spanish
* Italian
* Japanese
* Korean
* Simplified Chinese
* Traditional Chinese

## <a id="suppnote" name="suppnote"></a>Product Support Notice

Beginning with the next major release, we will be reducing the number of supported localization languages.  The three supported languages will be:
  * Japanese
  * Spanish
  * French

The following languages will no longer be supported:
  * Italian
  * German
  * Brazilian Portuguese
  * Traditional Chinese
  * Korean
  * Simplified Chinese

Impact:
  * Users who have been using the deprecated languages will no longer receive updates or support in these languages.
  * All user interfaces, message catalogs, help documentation, and customer support will be available only in English or in the three supported languages mentioned above.

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.


## <a id="interop" name="interop"></a>Interoperability Matrix

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. 

## <a id="resolvedissues" name ="resolvedissues"></a> Resolved Issues

*   **The following github.com/vmware/open-vm-tools pull request has been addressed.**

    * Revise settings for vmware-user.desktop

      [Pull request #668](https://github.com/vmware/open-vm-tools/pull/668)

*   **Accomodate newer releases of libxml2 and xmlsec1.**

    The configure.ac and VGAuth code updated to avoid deprecated functions and build options based on OSS product version.

## <a id="knownissues" name="knownissues"></a>Known Issues

*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Note: This issue is applicable to open-vm-tools running on VMware Workstation and VMware Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>

    For more information on how to configure VMware Tools Shared Folders, see [KB 60262](https://kb.vmware.com/s/article/60262)
