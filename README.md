#    CVE-2022-31676

A security issue in VMware Tools and open-vm-tools has been announced by VMware in a security advisory [VMSA-2023-0024](https://www.vmware.com/security/advisories/VMSA-2023-0024.html).

The issue has been fixed in the open-vm-tools release 12.3.5 made on October 26 2023.

The following patch provided to the open-vm-tools community can be used to apply the security fix to previous open-vm-tools releases.

## For all open-vm-tools releases 11.0.0 through 12.3.0


*   **[CVE-2023-34058.patch](https://github.com/vmware/open-vm-tools/blob/CVE-2023-34058.patch/CVE-2023-34058.patch)**

 

The patch has been tested against all open-vm-tools releases in the range shown.  Each applies cleanly with: 

    git am        for a git repository.
    patch -p1     in the top directory of an open-vm-tools source tree.

 
