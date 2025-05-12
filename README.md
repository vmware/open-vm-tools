#    CVE-2025-22247

A security issue in open-vm-tools has been announced by VMware in security advisory [VMSA-2025-0007](https://support.broadcom.com/web/ecx/support-content-notification/-/external/content/SecurityAdvisories/0/25683).

The issue has been fixed in the open-vm-tools version 12.5.2 released on May 12, 2025.

The following patch provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For releases 12.5.0, 12.4.5, 12.4.0, 12.3.5, 12.3.0


*   **[CVE-2025-22247-1230-1250-VGAuth-updates.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-22247.patch/CVE-2025-22247-1230-1250-VGAuth-updates.patch)**


## For releases 12.2.5, 12.2.0, 12.1.5, 12.1.0, 12.0.5, 12.0.0, 11.3.5, 11.3.0, 11.2.5, 11.2.0, 11.1.5. 11.1.0, 11.0.5, 11.0.0


*   **[CVE-2025-22247-1100-1225-VGAuth-updates.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-22247.patch/CVE-2025-22247-1100-1225-VGAuth-updates.patch)**


The patches have been tested against the above open-vm-tools releases.  Each applies cleanly with: 

    git am        for a git repository.
    patch -p2     in the top directory of an open-vm-tools source tree. 

 
