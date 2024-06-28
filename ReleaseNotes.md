#                      open-vm-tools 12.4.5 Release Notes

Updated on: 27 June 2024

open-vm-tools | 27 JUNE 2024 | Build 23787635

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

* [What's New](#whatsnew) 
* [Internationalization](#i18n) 
* [Product Support Notice](#suppnote)
* [End of Feature Support Notice](#endsupport)
* [Guest Operating System Customization Support](#guestop) 
* [Interoperability Matrix](#interop) 
* [Resolved Issues](#resolvedissues) 
* [Known Issues](#knownissues)

## <a id="whatsnew" name="whatsnew"></a>What's New


*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

*   A complete list of the granular changes in the open-vm-tools 12.4.5 release is available at:

    [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.4.5/open-vm-tools/ChangeLog)

## <a id="i18n" name="i18n"></a>Internationalization

open-vm-tools 12.4.5 is available in the following languages:

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

## <a id="endsupport" name="endsupport"></a>End of Feature Support Notice

*   **Discontinued: Using "xml-security-c" and "xerces-c" to build the VMware Guest Authentication Service (VGAuth)**

    Starting with open-vm-tools 12.4.0, and going forward, the VGAuth service build will require the "xmlsec1" and "libxml2" development and runtime packages.  If still using the "xml-security-c" and "xerces-c" open source projects to build open-vm-tools, you must make the change now.  The open-vm-tools 12.3.x series will be the last version that can use "xml-security-c" and "xerces-c".

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

## <a id="interop" name="interop"></a>Interoperability Matrix

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products. 

## <a id="resolvedissues" name ="resolvedissues"></a> Resolved Issues

*   **A number of issues flagged by Coverity and ShellCheck have been addressed.**

    The changes include code fixes and Coverity escapes for reported false positives.
    See the details in the [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.4.5/open-vm-tools/ChangeLog)  for specific fix or false positive escape.

*   **Nested logging from RPCChannel error may hang the vmtoolsd process.**

    This issue has been fixed in this release.

*   **vmtoolsd child processes invoke parent's atexit handler.****

    Fixed in this release by terminating child processes with _exit().

*   **Mutexes in lib/libvmtools/vmtoolsLog.c and glib could have been locked at fork time.  The vmtoolsLog.c Debug(), Warning() and Panic() functions are not safe for child processes.**

    Fixed in this release by directing child processes' logging to stdout.

*   **Permission on the vmware-network.log file incorrectly defaults to (0644).**

    Fixed in this release.  The correct default is set to (0600).

*   **The NetworkManager calls in the Linux "network" script have been updated.**

    Defaults to using the "Sleep" method over the "Enabled" method used to
    work around a bug in NetworkManager version 0.9.0.

    Resolves:
     * [Pull request #699](https://github.com/vmware/open-vm-tools/pull/699)
     * [Issue #426](https://github.com/vmware/open-vm-tools/issues/426)

*   **Unused header files have been dropped from the current open-vm-tools source.**

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
