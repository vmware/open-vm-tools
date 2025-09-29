#    CVE-2025-41244

A security issue in open-vm-tools has been announced by VMware in security advisory [VMSA-2025-0015](https://support.broadcom.com/web/ecx/support-content-notification/-/external/content/SecurityAdvisories/0/36149).

The issue has been fixed in the open-vm-tools versions 13.0.5 and 12.5.4 released on Sept 29, 2025.

The following patch provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For releases 12.4.0, 12.4.5. 12.5.0, 13.0.0


*   **[CVE-2025-41244-1240-1300-SDMP.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-41244.patch/CVE-2025-41244-1240-1300-SDMP.patch)**


## For releases 12.3.0, 12.3.5


*   **[CVE-2025-41244-1230-1235-SDMP.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-41244.patch/CVE-2025-41244-1230-1235-SDMP.patch)**


## For releases 12.0.0, 12.0.5, 12.1.0, 12.1.5, 12.2.0, 12.2.5


*   **[CVE-2025-41244-1200-1225-SDMP.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-41244.patch/CVE-2025-41244-1200-1225-SDMP.patch)**


## For releases 11.2.0, 11.2.5, 11.3.0, 11.3.5


*   **[CVE-2025-41244-1120-1135-SDMP.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2025-41244.patch/CVE-2025-41244-1120-1135-SDMP.patch)**



The patches have been tested against the above open-vm-tools releases.  Each applies cleanly with: 

    git am        for a git repository.
    patch -p2     in the top directory of an open-vm-tools source tree. 

 
