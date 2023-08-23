#    CVE-2023-20900

A security issue in VMware Tools and open-vm-tools has been announced by VMware in security advisory [VMSA-2023-0019](https://www.vmware.com/security/advisories/VMSA-2023-0019.html).

The issue has been fixed in the open-vm-tools version 12.3.0 released on August 31, 2023.

The following patch provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For all open-vm-tools releases 10.3.0 through 12.2.5


*   **[CVE-2023-20900.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-20900.patch/CVE-2023-20900.patch)**

The patches have been tested against the above open-vm-tools releases.  Each applies cleanly with: 

    git am        for a git repository.
    patch -p2     in the top directory of an open-vm-tools source tree. 

