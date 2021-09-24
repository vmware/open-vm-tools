#          Open-vm-tools 11.3.5 Release Notes

Updated on: 23 SEP 2021

Open-vm-tools | 23 SEP 2021 | Build 18557794

Check back for additions and updates to these release notes.

## What's in the Release Notes

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Internationalization](#internationalization)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Guest Operating System Customization Support](#guestop)
*   [Interoperability Matrix](#interop)
*   [Resolved Issues](#resolved-issues)
*   [Known Issues](#known-issues)

## <a id="whatsnew" name="whatsnew"></a>What's New

*   **For issues resolved in this release, see [Resolved Issues](#resolved-issues) section.**

*   **Added a configurable logging capability to the `network` script.**

    The `network` script has been updated to:
       - use `vmware-toolbox-cmd` to query any network logging configuration from the `tools.conf` file.
       - use `vmtoolsd --cmd "log ..."` to log a message to the vmx logfile when the logging handler is configured to "vmx" or when the logfile is full or is not writeable.

*   **The hgfsmounter (`mount.vmhgfs`) command has been removed from open-vm-tools.**

    The hgfsmounter (`mount.vmhgfs`) command is no longer used in Linux open-vm-tools.  It has been replaced by hgfs-fuse.  Therefore, removing all references to the hgfsmounter in Linux builds.

## <a id="internationalization" name="internationalization"></a>Internationalization

Open-vm-tools 11.3.5 is available in the following languages:

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

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.x release will continue to be supported.  However, releases after VMware Tools 10.3.5 will only include critical and security fixes.  No new feature support will be provided in these types of VMware Tools (tar tools and OSP's).  It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools.  For more information on different types of VMware Tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

## <a id="guestop" name="guestop"></a>Guest Operating System Customization Support

The [Guest OS Customization Support Matrix](http://partnerweb.vmware.com/programs/guestOS/guest-os-customization-matrix.pdf) provides details about the guest operating systems supported for customization.

## <a id="interop" name="interop"></a>Interoperability Matrix

The [VMware Product Interoperability Matrix](http://partnerweb.vmware.com/comp_guide2/sim/interop_matrix.php) provides details about the compatibility of current and earlier versions of VMware Products.

## <a id="resolved-issues" name="resolved-issues"></a>Resolved Issues

*   **Customization: Retry the Linux reboot if `telinit` is a soft link to systemctl
.**

    Issues have been reported on some newer versions of Linux where the VM failed to reboot at the end of a traditional customization.  The command '/sbin/telinit 6' exited abnormally due to SIGTERM sent by systemd and where `telinit` is a symlink to systemctl.

    This issue is fixed in the 11.3.5 open-vm-tools release.

*   **Open-vm-tools commands would hang if configured with "--enable-valgrind".**

    The "backdoor" touch test cannot be handled by Valgrind.

    This issue is fixed in the 11.3.5 open-vm-tools release.

## <a id="known-issues" name="known-issues"></a>Known Issues

**Open-vm-tools Issues in VMware Workstation or Fusion**

*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, the shared folders mount is not available on restart.

    Note: This issue is applicable to open-vm-tools running on Workstation and Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface. For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.

    For example, add the line:

    <tt>vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow_other    0    0</tt>

*   **Drag functionality fails to work in Ubuntu.**

    Drag functionality fails to work in Ubuntu 16.04.4 32-bit virtual machines installed using Easy Install. Also, failure of the copy and paste functionality is observed in the same systems.

    Note: This issue is applicable to open-vm-tools running on Workstation and Fusion.

    Workaround:

    *   Add the <tt>modprobe.blacklist=vmwgfx</tt> linux kernel boot option.
    *   To gain access to larger resolutions, remove <tt>svga.guestBackedPrimaryAware = "TRUE"</tt> option from the <tt>VMX</tt> file.

