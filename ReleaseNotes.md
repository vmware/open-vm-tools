#                      open-vm-tools 12.2.0 Release Notes

Updated on: 7 MAR 2023

open-vm-tools | 7 MAR 2023 | Build 21223074

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

* [What's New](#whatsnew) 
* [Internationalization](#i18n) 
* [End of Feature Support Notice](#endoffeaturesupport) 
* [Guest Operating System Customization Support](#guestop) 
* [Interoperability Matrix](#interop) 
* [Resolved Issues](#resolvedissues) 
* [Known Issues](#knownissues)

## <a id="whatsnew" name="whatsnew"></a>What's New

There are no new features in the open-vm-tools 12.2.0 release.  This is primarily a maintenance release that addresses a few critical problems.

*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

*   A complete list of the granular changes in the open-vm-tools 12.2.0 release is available at:

    [open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.2.0/open-vm-tools/ChangeLog)

## <a id="i18n" name="i18n"></a>Internationalization

open-vm-tools 12.2.0 is available in the following languages:

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

*   **A number of Coverity reported issues have been addressed.**

*   **The vmtoolsd task is blocked in the uninterruptible state while doing a quiesced snapshot.**

    As the ioctl FIFREEZE is done during a quiesced snapshot operation, an EBUSY could be seen because of an attempt to freeze the same superblock more than once depending on the OS configuration (e.g. usage of bind mounts).  An EBUSY could also mean another process has locked or frozen that filesystem.  That later could lead to the vmtoolsd process being blocked and ultimately other processes on the system could be blocked.

    The Linux quiesced snapshot procedure has been updated that when an EBUSY is received, the filesystem FSID is checked against the list of filesystems that have already been quiesced.  If not previously seen, a warning that the filesystem is controlled by another process is logged and the quiesced snapshot request will be rejected.

    This fix to lib/syncDriver/syncDriverLinux.c is directly applicable to previous releases of open-vm-tools and is available at:

        https://github.com/vmware/open-vm-tools/commit/9d458c53a7a656d4d1ba3a28d090cce82ac4af0e

*   **Updated the guestOps to handle some edge cases.**

    When File_GetSize() fails or returns a -1 indicating the user does not have access permissions:

    1. Skip the file in the output of the ListFiles() request.
    2. Fail an InitiateFileTransferFromGuest operation.

*   **The following pull requests and issues have been addressed.**

    * Detect the proto files for the containerd grpc client in alternate locations.

      [Pull request #626](https://github.com/vmware/open-vm-tools/pull/626)

    * FreeBSD: Support newer releases and code clean-up for earlier versions.

      [Pull request #584](https://github.com/vmware/open-vm-tools/pull/584)



## <a id="knownissues" name="knownissues"></a>Known Issues


*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Note: This issue is applicable to open-vm-tools running on VMware Workstation and VMware Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>

