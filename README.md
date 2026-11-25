#    CVE-2026-123917

A security issue in the VGAuthService in open-vm-tools has been announced by VMware in CVE-2026-123917.

The issue will be fixed in an upcoming open-vm-tools release.

The following patch provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For all open-vm-tools releases 11.0.0 through 13.1.0


*   **[CVE-2026-123917-VGAuth-updates.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2026-123917.patch/CVE-2026-123917-VGAuth-updates.patch)**


## Prerequisite for releases 11.0.0 through 12.5.0

For the releases listed below, the [CVE-2025-22247](https://github.com/vmware/open-vm-tools/tree/CVE-2025-22247.patch) security fix must be applied first, and then the patch above.

For releases 12.5.0, 12.4.5, 12.4.0, 12.3.5, 12.3.0, apply [CVE-2025-22247-1230-1250-VGAuth-updates.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-22247.patch/CVE-2025-22247-1230-1250-VGAuth-updates.patch)

For releases 12.2.5, 12.2.0, 12.1.5, 12.1.0, 12.0.5, 12.0.0, 11.3.5, 11.3.0, 11.2.5, 11.2.0, 11.1.5, 11.1.0, 11.0.5, 11.0.0, apply [CVE-2025-22247-1100-1225-VGAuth-updates.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-22247.patch/CVE-2025-22247-1100-1225-VGAuth-updates.patch)

Releases 12.5.2 and later (12.5.2, 12.5.4, 13.0.0, 13.0.5, 13.0.10, 13.1.0) already contain CVE-2025-22247 and need only the patch above.


The patches have been tested against the above open-vm-tools releases. Each applies cleanly with:

    git am        for a git repository.
    patch -p2     in the top directory of an open-vm-tools source tree.


