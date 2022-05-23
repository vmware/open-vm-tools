#                Open-vm-tools 12.0.5 Release Notes

Updated on: 24th MAY 2022

Open-vm-tools | 24th MAY 2022 | Build 19716617

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Internationalization](#i18n)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Guest Operating System Customization Support](#guestop)
*   [Interoperability Matrix](#interop)
*   [Resolved Issues](#resolvedissues)
*   [Known Issues](#knownissues)

## <a id="whatsnew" name="whatsnew"></a>What's New

*   There are no new features or significant changes in the open-vm-tools 12.0.5 release.  This is primarily a maintenance release.

*   Please see the [Resolved Issues](#resolvedissues) and [Known Issues](#knownissues) sections below.

## <a id="i18n" name="i18n"></a>Internationalization

Open-vm-tools 12.0.5 is available in the following languages:

*   English
*   French
*   German
*   Spanish
*   Italian
*   Japanese
*   Korean
*   Simplified Chinese
*   Traditional Chinese

## <a id="endoffeaturesupport" name="endoffeaturesupport"></a>End of Feature Support Notice

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.x release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes. No new feature support will be provided in these types of VMware Tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information about open-vm-tools, see [https://kb.vmware.com/s/article/2073803](https://kb.vmware.com/s/article/2073803).

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

## <a id="interop" name="interop"></a>Interoperability Matrix

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products.

## <a id="resolvedissues" name ="resolvedissues"></a> Resolved Issues

*   **[FTBFS] glibc 2.35 and GCC 11 & 12 reporting possible array bounds overflow - bora/lib/asyncsocket/asyncksocket.c: AsyncTCPSocketPollWork()**

    Reported in [open-vm-tools issue #570](https://github.com/vmware/open-vm-tools/issues/570), the fix was not available at the time of the open-vm-tools 12.0.0 code freeze date.  The fix was made available in the open-vm-tools "devel" branch.

    This issue is fixed in this release.

*   **[FTBFS] ContainerInfo Plugin: compilation warnings with a newer version of GCC**

    Newer releases of the GNU GCC/C++ compiler report a warning about "comparison of integer expressions of different signedness" in containerInfo_grpc.cc

    This issue is fixed in this release.

*   **A complete list of the granular changes in the open-vm-tools 12.0.5 release is available at:**

    [Open-vm-tools ChangeLog](https://github.com/vmware/open-vm-tools/blob/stable-12.0.5/open-vm-tools/ChangeLog)


## <a id="knownissues" name="knownissues"></a>Known Issues

The known issues are grouped as follows.

*   [Open-vm-tools Issues](#open-vm-tools-issues-known)
*   [Open-vm-tools Issues in VMware Workstation or Fusion](#open-vm-tools-issues-in-vmware-workstation-or-fusion-known)

**<a id="open-vm-tools-issues-known" name="open-vm-tools-issues-known"></a>Open-vm-tools Issues**

*   **[FTBFS] Fix the build of the ContainerInfo plugin for a 32-bit Linux release**

    Reported in [open-vm-tools pull request #588](https://github.com/vmware/open-vm-tools/pull/588), the fix did not make the code freeze date.   The fix is available in the open-vm-tools "devel" branch at [commit 605c30a](https://github.com/vmware/open-vm-tools/commit/605c30a33f6dedc07ded8e24ce1491fba162ce82) and can be applied directly to the 12.0.0 and 12.0.5 open-vm-tools sources.

    The fix will be in the next open-vm-tools source release.

**<a id="open-vm-tools-issues-in-vmware-workstation-or-fusion-known" name="open-vm-tools-issues-in-vmware-workstation-or-fusion-known"></a>Open-vm-tools Issues in VMware Workstation or Fusion**

*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.  For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>


