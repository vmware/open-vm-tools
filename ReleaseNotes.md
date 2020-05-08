#open-vm-tools 11.1.0 Release Notes

**Updated on: 07 MAY 2020**

VMware Tools | 07 MAY 2020 | Build 16036546

Check for additions and updates to these release notes.

What's in the Release Notes
---------------------------

The release notes cover the following topics:

*   [What's New](#whatsnew)
*   [Product Support Notice](#productsupport)
*   [End of Feature Support Notice](#endoffeaturesupport)
*   [Compatibility Notes](#compatibility)
*   [Known Issues](#knownissues)

What's New
----------

*   open-vm-tools 11.1.0 includes a new 'Service Discovery' plugin, which connects with the vRealize Operations Manager product. Refer to the following links for more information on this feature:
    [https://marketplace.vmware.com/vsx/solutions/vrealize-operations-service-discovery-management-pack?ref=search](https://marketplace.vmware.com/vsx/solutions/vrealize-operations-service-discovery-management-pack?ref=search)
    [https://www.vmware.com/products/vrealize-operations.html](https://www.vmware.com/products/vrealize-operations.html)

    The 'Service Discovery' plugin is installed and enabled by default in a Windows VM.
    For information on open-vm-tools for Linux, refer to [https://github.com/vmware/open-vm-tools/blob/master/README.md](https://github.com/vmware/open-vm-tools/blob/master/README.md).
    For more details on configuring this plugin, refer to [Configuring Service Discovery](https://docs.vmware.com/en/VMware-Tools/11.0.0/com.vmware.vsphere.vmwaretools.doc/GUID-ADC00685-CB08-4BE6-B815-6E87D5D3A379.html).
*   In this release, a new tools.conf switch is added to enable and disable the guest customization in the guest virtual machine. By default, the guest customization is enabled. For more details, refer [KB 78903](https://kb.vmware.com/s/article/78903).

End of Feature Support Notice
-----------------------------

*   The tar tools (linux.iso) and OSPs shipped with VMware Tools 10.3.5 release will continue to be supported. However, releases after VMware Tools 10.3.5 will only include critical and security fixes and no new feature support in these types of VMware Tools (tar tools and OSP's). It is recommended that customers use open-vm-tools for those operating systems that support open-vm-tools. For more information on different types of VMware Tools, see [https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html](https://blogs.vmware.com/vsphere/2016/02/understanding-the-three-types-of-vm-tools.html)

Known Issues
------------

*   **Drag functionality fails to work in Ubuntu.**

    Drag functionality fails to work in Ubuntu 16.04.4 32-bit virtual machine installed using easy install. Also, failure of copy and paste functionality is observed in the same system.

    Note: This issue is applicable for open-vm-tools running on Workstation and Fusion.

    Workaround:

    *   Add the modprobe.blacklist=vmwgfx linux kernel boot option.
    *   To gain access to larger resolutions, remove svga.guestBackedPrimaryAware = "TRUE" option from the VMX file.

*   **Shared Folders mount is unavailable on Linux VM.**

    If the **Shared Folders** feature is enabled on a Linux VM while it is powered off, shared folders mount is not available on restart.

    Note: This issue is applicable for open-vm-tools running on Workstation and Fusion.

    Workaround:

    If the VM is powered on, disable and enable the **Shared Folders** feature from the interface.
    For resolving the issue permanently, edit **/etc/fstab** and add an entry to mount the Shared Folders automatically on boot.
    For example, add the line:
    vmhgfs-fuse   /mnt/hgfs    fuse    defaults,allow\_other    0    0
